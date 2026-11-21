// Network play, after frontend-qt6/net/newnetclient.cpp (the protocol) and the
// pages pagenet, pageconnecting, pageroomslist, pagenetgame and chatwidget.
// One window with three cards: the server to connect to, the lobby (rooms,
// players, chat) and a room (players, teams, settings, chat). A net game runs
// the engine like a local one (run_game), relaying its messages to the server.
#include "Net.h"

#include <Alert.h>
#include <Autolock.h>
#include <Button.h>
#include <CardLayout.h>
#include <CheckBox.h>
#include <ColumnListView.h>	// private/interface
#include <ColumnTypes.h>
#include <Entry.h>
#include <File.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Locker.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SeparatorItem.h>
#include <Spinner.h>	// private/interface
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>

#include <netdb.h>
#include <openssl/evp.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <stdlib.h>
#include <string.h>

#include "DrawMap.h"
#include "NetDialogs.h"
#include "NetProto.h"
#include "Stats.h"

static const char* kDefaultServer = "netserver.hedgewars.org";	// NETGAME_DEFAULT_SERVER
static const char* kProtoVer = "60";

enum {
	kMsgConnect = 'ncon', kMsgServer = 'nsrv', kMsgClosed = 'ncls', kMsgDisconnect = 'ndis',
	kMsgLobbyChat = 'nlch', kMsgRoomChat = 'nrch', kMsgCreate = 'ncre', kMsgJoin = 'njoi',
	kMsgLeave = 'nlea', kMsgTeam = 'ntea', kMsgReady = 'nrdy', kMsgStart = 'nsta',
	kMsgSetting = 'nset', kMsgPrompt = 'nprm', kMsgNetGameOver = 'ngov', kMsgRoomSel = 'nrsl',
	kMsgHogs = 'nhog', kMsgEditDrawing = 'ndrw', kMsgFollow = 'nfol', kMsgTeamSel = 'ntsl', kMsgTeamColor = 'ntcl', kMsgStyle = 'nsty',
	kMsgPlayerMenu = 'npmn', kMsgKick = 'nkck', kMsgBan = 'nban', kMsgDelegate = 'ndlg',
	kMsgRename = 'nren', kMsgToggle = 'ntgl', kMsgAdmin = 'nadm',
};

static const char* const kColorNames[] = {"Red", "Blue", "Teal", "Purple", "Pink", "Green",
	"Orange", "Brown", "Yellow"};	// HW_TEAMCOLOR_ARRAY

// a player list whose right-click asks the window for the player's menu (chatwidget.cpp)
class PlayerList : public BListView {
public:
	PlayerList(const char* name) : BListView(name) {}

	void MouseDown(BPoint where) override
	{
		int32 buttons = Window()->CurrentMessage()->GetInt32("buttons", B_PRIMARY_MOUSE_BUTTON);
		BListView::MouseDown(where);
		int32 i = IndexOf(where);
		if (!(buttons & B_SECONDARY_MOUSE_BUTTON) || i < 0)
			return;
		Select(i);
		BMessage m(kMsgPlayerMenu);
		m.AddPointer("list", this);
		m.AddPoint("where", ConvertToScreen(where));
		Window()->PostMessage(&m);
	}
};

// the generated maps: label, MAP, MAPGEN (mapContainer.cpp, MapModel.cpp)
static const struct { const char* label; const char* map; int gen; } kGenerated[] = {
	{"Random island", "+rnd+", 0}, {"Maze", "+maze+", 1}, {"Cave", "+perlin+", 2}, {"Forts", "+forts+", 4},
	{"Hand-drawn", "+drawn+", 3},
};
static const int kGenCount = sizeof(kGenerated) / sizeof(kGenerated[0]), kHandDrawn = 4;	// then a separator, the fixed maps

static std::string hex_digest(const std::string& s, const EVP_MD* md)
{
	unsigned char out[EVP_MAX_MD_SIZE];
	unsigned len = 0;
	EVP_Digest(s.data(), s.size(), out, &len, md, NULL);
	std::string hex;
	char b[3];
	for (unsigned i = 0; i < len; i++) {
		snprintf(b, sizeof(b), "%02x", out[i]);
		hex += b;
	}
	return hex;
}

static std::string spaced(std::string s)
{
	std::replace(s.begin(), s.end(), '_', ' ');
	return s;
}

static BMenuField* popup(const char* label, const std::vector<std::string>& items)
{
	BPopUpMenu* menu = new BPopUpMenu("");
	for (const std::string& s : items)
		menu->AddItem(new BMenuItem(s.c_str(), new BMessage(kMsgSetting)));
	if (menu->ItemAt(0))
		menu->ItemAt(0)->SetMarked(true);
	return new BMenuField(label, menu);
}

// marks the item with this label, adding it if the room uses one we don't have
static void show_value(BMenuField* f, const std::string& label)
{
	BMenuItem* item = f->Menu()->FindItem(label.c_str());
	if (!item)
		f->Menu()->AddItem(item = new BMenuItem(label.c_str(), new BMessage(kMsgSetting)));
	item->SetMarked(true);
}

static std::string marked(BMenuField* f)
{
	BMenuItem* item = f->Menu()->FindMarked();
	return item ? item->Label() : "";
}

static BTextView* chat_view(BScrollView** scroll)
{
	BTextView* t = new BTextView("chat");
	t->MakeEditable(false);
	t->SetWordWrap(true);
	t->SetExplicitMinSize(BSize(300, 120));
	*scroll = new BScrollView("chat scroll", t, 0, false, true);
	return t;
}

static void append(BTextView* t, const std::string& line)
{
	if (t->TextLength() > 0)
		t->Insert(t->TextLength(), "\n", 1);
	t->Insert(t->TextLength(), line.c_str(), line.size());
	t->ScrollToOffset(t->TextLength());
}

// Asks for one line of text; posts kMsgPrompt with "kind", "text" and "cancel" to the target
class PromptWindow : public BWindow {
public:
	PromptWindow(BWindow* over, const char* question, const char* kind, bool secret, const char* action)
		: BWindow(BRect(0, 0, 300, 100), "Hedgewars", B_MODAL_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
			B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
		fTarget(over), fKind(kind)
	{
		fText = new BTextControl("", "", NULL);
		fText->TextView()->HideTyping(secret);
		BButton* ok = new BButton(action, new BMessage('ok  '));
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(new BStringView("question", question))
			.Add(fText)
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(new BButton("Cancel", new BMessage('cncl')))
				.Add(ok)
			.End();
		SetDefaultButton(ok);
		fText->MakeFocus();
		CenterIn(over->Frame());
	}

	void MessageReceived(BMessage* msg) override
	{
		if (msg->what != 'ok  ' && msg->what != 'cncl') {
			BWindow::MessageReceived(msg);
			return;
		}
		Answer(msg->what == 'cncl');
		Quit();
	}

	bool QuitRequested() override	// closed, or Escape
	{
		if (!fAnswered)
			Answer(true);
		return true;
	}

	void Answer(bool cancel)
	{
		BMessage m(kMsgPrompt);
		m.AddString("kind", fKind.c_str());
		m.AddString("text", fText->Text());
		m.AddBool("cancel", cancel);
		fTarget.SendMessage(&m);
		fAnswered = true;
	}

private:
	BMessenger fTarget;
	std::string fKind;
	BTextControl* fText;
	bool fAnswered = false;
};

class NetWindow : public BWindow {
public:
	NetWindow(const TeamInfo& team, const Game& prefs)
		: BWindow(BRect(100, 80, 900, 680), "Network game", B_TITLED_WINDOW,
			B_AUTO_UPDATE_SIZE_LIMITS),
		fTeam(team), fPrefs(prefs)
	{
		signal(SIGPIPE, SIG_IGN);	// a closed connection or engine is an error, not the end
		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.Add(MenuBar())
			.AddCards()
				.GetLayout(&fCards)
				.Add(ConnectPage())
				.Add(LobbyPage())
				.Add(RoomPage())
			.End();
		LoadSettings();
		ShowCard(0);
	}

	~NetWindow() { Disconnect(); }

	// ── the menus: the room host's and the server admin's commands ──
	BMenuBar* MenuBar()
	{
		BMenuBar* bar = new BMenuBar("menu");
		fRoomMenu = new BMenu("Room");
		fRoomMenu->AddItem(new BMenuItem("Rename…", new BMessage(kMsgRename)));
		fRoomMenu->AddSeparatorItem();
		const char* toggles[][2] = {{"Restrict joins", "TOGGLE_RESTRICT_JOINS"},
			{"Restrict team additions", "TOGGLE_RESTRICT_TEAMS"},
			{"Registered players only", "TOGGLE_REGISTERED_ONLY"}};
		for (const auto& t : toggles) {
			BMessage* m = new BMessage(kMsgToggle);
			m->AddString("cmd", t[1]);
			fRoomMenu->AddItem(new BMenuItem(t[0], m));
		}
		fRoomMenu->AddSeparatorItem();
		fRoomMenu->AddItem(new BMenuItem("Kick player", new BMessage(kMsgKick)));
		fRoomMenu->AddItem(new BMenuItem("Make player host", new BMessage(kMsgDelegate)));
		fServerMenu = new BMenu("Server");
		fServerMenu->AddItem(new BMenuItem("Administration…", new BMessage(kMsgAdmin)));
		fServerMenu->AddSeparatorItem();
		fServerMenu->AddItem(new BMenuItem("Kick player", new BMessage(kMsgKick)));
		fServerMenu->AddItem(new BMenuItem("Ban player…", new BMessage(kMsgBan)));
		bar->AddItem(fRoomMenu);
		bar->AddItem(fServerMenu);
		return bar;
	}

	// the player picked in the list of the page shown (or in list)
	std::string SelectedNick(BListView* list = NULL)
	{
		if (!list)
			list = fState == kInRoom ? fRoomPlayers : fLobbyPlayers;
		int32 i = list->CurrentSelection();
		std::vector<std::string> nicks = list == fRoomPlayers ? fRoomNicks : SortedLobby();
		return i >= 0 && i < (int32)nicks.size() ? nicks[i] : "";
	}

	// marks and enables what the player may do now (the server checks again)
	void MenusBeginning() override
	{
		bool inRoom = fState == kInRoom;
		std::string flags = RoomFlags(), nick = SelectedNick();
		bool other = !nick.empty() && nick != fMyNick;
		// the menus themselves stay enabled: a disabled one doesn't open, so this wouldn't run again
		fRoomMenu->FindItem(kMsgRename)->SetEnabled(fChief);
		for (int32 i = 0; BMenuItem* item = fRoomMenu->ItemAt(i); i++) {
			if (item->Message() && item->Message()->what == kMsgToggle) {
				std::string cmd = item->Message()->GetString("cmd", "");
				item->SetEnabled(fChief);
				item->SetMarked(cmd == "TOGGLE_RESTRICT_TEAMS" ? fRestrictTeams
					: flags.find(cmd == "TOGGLE_RESTRICT_JOINS" ? 'j' : 'r') != std::string::npos);
			}
		}
		fRoomMenu->FindItem(kMsgKick)->SetEnabled(fChief && other);
		fRoomMenu->FindItem(kMsgDelegate)->SetEnabled(inRoom && (fChief || fAdmin) && other);
		fServerMenu->FindItem(kMsgAdmin)->SetEnabled(fAdmin);
		fServerMenu->FindItem(kMsgKick)->SetEnabled(fAdmin && !inRoom && other);
		fServerMenu->FindItem(kMsgBan)->SetEnabled(fAdmin && !inRoom && other);
	}

	// right-click on a player: what the host or admin may do to them (HWChatWidget::showNicksMenu)
	void PlayerMenu(BListView* list, BPoint where)
	{
		std::string nick = SelectedNick(list);
		if (nick.empty() || nick == fMyNick)
			return;
		BPopUpMenu* menu = new BPopUpMenu("player", false, false);
		auto add = [&](const char* label, uint32 what) {
			BMessage* m = new BMessage(what);
			m->AddString("nick", nick.c_str());
			menu->AddItem(new BMenuItem(label, m));
		};
		if (list == fRoomPlayers) {
			if (fChief)
				add("Kick from room", kMsgKick);
			if (fChief || fAdmin)
				add("Make host", kMsgDelegate);
		} else {
			if (fFlags[nick].find('i') != std::string::npos)	// in a room
				add("Follow", kMsgFollow);
			if (fAdmin) {
				add("Kick from server", kMsgKick);
				add("Ban…", kMsgBan);
			}
		}
		if (menu->CountItems() == 0) {
			delete menu;
			return;
		}
		menu->SetTargetForItems(this);
		menu->SetAsyncAutoDestruct(true);
		menu->Go(where, true, false, true);
	}

	// ── pages ──
	BView* ConnectPage()
	{
		BGroupView* v = new BGroupView("connect", B_VERTICAL);
		fHost = new BTextControl("Server:", kDefaultServer, NULL);
		fPort = new BTextControl("Port:", "46631", NULL);
		fNick = new BTextControl("Nickname:", "", NULL);
		fConnectStatus = new BStringView("status", "");
		fConnectStatus->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
		fConnect = new BButton("Connect", new BMessage(kMsgConnect));
		BLayoutBuilder::Group<>(v)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddGlue()
			.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.AddTextControl(fHost, 0, 0)
				.AddTextControl(fPort, 0, 1)
				.AddTextControl(fNick, 0, 2)
			.End()
			.Add(fConnectStatus)
			.AddGlue()
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(fConnect)
			.End();
		return v;
	}

	BView* LobbyPage()
	{
		BGroupView* v = new BGroupView("lobby", B_VERTICAL);
		fRooms = new BColumnListView("rooms", B_NAVIGABLE, B_FANCY_BORDER);
		const char* cols[] = {"Room", "State", "Players", "Teams", "Host", "Map", "Scheme", "Weapons"};
		float widths[] = {140, 150, 55, 50, 90, 90, 90, 90};
		for (int i = 0; i < 8; i++)
			fRooms->AddColumn(new BStringColumn(cols[i], widths[i], 30, 400, B_TRUNCATE_END), i);
		fRooms->SetSelectionMessage(new BMessage(kMsgRoomSel));
		fRooms->SetInvocationMessage(new BMessage(kMsgJoin));
		fRooms->SetExplicitMinSize(BSize(450, 150));
		fLobbyPlayers = new PlayerList("players");
		BScrollView* ps = new BScrollView("players scroll", fLobbyPlayers, 0, false, true);
		ps->SetExplicitPreferredSize(BSize(130, B_SIZE_UNSET));
		BScrollView* cs;
		fLobbyChat = chat_view(&cs);
		fLobbyEntry = new BTextControl(NULL, "", new BMessage(kMsgLobbyChat));
		fJoin = new BButton("Join room", new BMessage(kMsgJoin));
		BLayoutBuilder::Group<>(v)
			.SetInsets(B_USE_WINDOW_SPACING)
			.AddSplit(B_VERTICAL)
				.Add(fRooms, 3)
				.AddSplit(B_HORIZONTAL, B_USE_DEFAULT_SPACING, 2)
					.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 3)
						.Add(cs)
						.Add(fLobbyEntry)
					.End()
					.Add(ps, 1)
				.End()
			.End()
			.AddGroup(B_HORIZONTAL)
				.Add(new BButton("Disconnect", new BMessage(kMsgDisconnect)))
				.AddGlue()
				.Add(new BButton("Create room…", new BMessage(kMsgCreate)))
				.Add(fJoin)
			.End();
		return v;
	}

	BView* RoomPage()
	{
		BGroupView* v = new BGroupView("room", B_VERTICAL);
		fRoomPlayers = new PlayerList("room players");
		BScrollView* ps = new BScrollView("room players scroll", fRoomPlayers, 0, false, true);
		fTeams = new BListView("teams");
		fTeams->SetSelectionMessage(new BMessage(kMsgTeamSel));
		BScrollView* ts = new BScrollView("teams scroll", fTeams, 0, false, true);
		ts->SetExplicitMinSize(BSize(240, 90));
		fScheme = popup("Scheme:", {});
		fWeapons = popup("Weapons:", {});
		FillSchemeMenus();
		std::vector<std::string> maps;
		for (const auto& g : kGenerated)
			maps.push_back(g.label);
		for (const std::string& m : list_dir("Maps", NULL, true)) {
			BPath dir(gDataDir.Path(), ("Maps/" + m).c_str());
			// as on the Match tab: no missions, no test map
			if (m != "test" && BEntry(BPath(dir.Path(), "map.cfg").Path()).Exists()
				&& !BEntry(BPath(dir.Path(), "map.lua").Path()).Exists())
				fMaps.push_back(m);
		}
		for (const std::string& m : fMaps)
			maps.push_back(spaced(m));
		fMap = popup("Map:", maps);
		fMap->Menu()->AddItem(new BSeparatorItem(), kGenCount);
		fEditDrawing = new BButton("Edit drawing…", new BMessage(kMsgEditDrawing));
		std::vector<std::string> themes;
		for (const std::string& t : list_dir("Themes", NULL, true))
			if (ini_value(("Themes/" + t + "/theme.cfg").c_str(), "hidden ").find("true") == std::string::npos)
				themes.push_back(t);
		fTheme = popup("Theme:", themes);
		fStyles = list_dir("Scripts/Multiplayer", ".lua", false);
		std::vector<std::string> styles = {"Normal"};
		for (const std::string& st : fStyles)
			styles.push_back(spaced(st));
		fStyle = popup("Style:", styles);
		for (int32 i = 0; BMenuItem* item = fStyle->Menu()->ItemAt(i); i++)
			item->SetMessage(new BMessage(kMsgStyle));
		fStyle->Menu()->AddItem(new BSeparatorItem(), 1);
		// the selected team's hedgehogs and colour (frameTeam.cpp, hedgehogerWidget.cpp, colorwidget.cpp)
		fHogs = new BSpinner("hogs", "Hedgehogs:", new BMessage(kMsgHogs));
		fHogs->SetRange(1, 8);
		fHogs->SetValue(4);
		std::vector<std::string> colors(kColorNames, kColorNames + 9);
		fTeamColor = popup("Color:", colors);
		for (int32 i = 0; BMenuItem* item = fTeamColor->Menu()->ItemAt(i); i++)
			item->SetMessage(new BMessage(kMsgTeamColor));
		show_value(fTheme, "Nature");
		BScrollView* cs;
		fRoomChat = chat_view(&cs);
		fRoomEntry = new BTextControl(NULL, "", new BMessage(kMsgRoomChat));
		fRoomTitle = new BStringView("title", "");
		fRoomTitle->SetFont(be_bold_font);
		fTeamButton = new BButton("Add my team", new BMessage(kMsgTeam));
		fReady = new BCheckBox("Ready", new BMessage(kMsgReady));
		fStart = new BButton("Start game", new BMessage(kMsgStart));
		BLayoutBuilder::Group<>(v)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(fRoomTitle)
			.AddGroup(B_HORIZONTAL)
				.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 1)
					.Add(new BStringView("teams label", "Playing teams:"))
					.Add(ts)
					.AddGroup(B_HORIZONTAL)
						.Add(fHogs)
						.Add(fTeamColor)
					.End()
					.Add(new BStringView("players label", "Players:"))
					.Add(ps)
				.End()
				.AddGroup(B_VERTICAL, B_USE_SMALL_SPACING, 1)
					.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
						.AddMenuField(fStyle, 0, 0)
						.AddMenuField(fScheme, 0, 1)
						.AddMenuField(fWeapons, 0, 2)
						.AddMenuField(fMap, 0, 3)
						.Add(fEditDrawing, 2, 3)
						.AddMenuField(fTheme, 0, 4)
					.End()
					.AddGlue()
				.End()
			.End()
			.Add(cs)
			.Add(fRoomEntry)
			.AddGroup(B_HORIZONTAL)
				.Add(new BButton("Leave room", new BMessage(kMsgLeave)))
				.AddGlue()
				.Add(fTeamButton)
				.Add(fReady)
				.Add(fStart)
			.End();
		return v;
	}

	void ShowCard(int i)
	{
		fCards->SetVisibleItem(i);
		// only the first card: Enter in the chat fields sends the line
		SetDefaultButton(i == 0 ? fConnect : NULL);
		if (i == 1)
			fLobbyEntry->MakeFocus();
		else if (i == 2)
			fRoomEntry->MakeFocus();
	}

	// ── settings: the server, the window ──
	BPath SettingsPath() { return BPath(gUserDir.Path(), "net_settings"); }

	void LoadSettings()
	{
		BFile f(SettingsPath().Path(), B_READ_ONLY);
		BMessage s;
		const char* str;
		BRect frame;
		if (s.Unflatten(&f) == B_OK) {
			if (s.FindString("host", &str) == B_OK) fHost->SetText(str);
			if (s.FindString("port", &str) == B_OK) fPort->SetText(str);
			if (s.FindString("nick", &str) == B_OK) fNick->SetText(str);
			if (s.FindString("room name", &str) == B_OK) fLastRoomName = str;
			if (s.FindRect("frame", &frame) == B_OK) {
				MoveTo(frame.LeftTop());
				ResizeTo(frame.Width(), frame.Height());
				MoveOnScreen();
				return;
			}
		}
		CenterOnScreen();
	}

	void SaveSettings()
	{
		BMessage s;
		s.AddString("host", fHost->Text());
		s.AddString("port", fPort->Text());
		s.AddString("nick", fNick->Text());
		s.AddString("room name", fLastRoomName.c_str());
		s.AddRect("frame", Frame());
		save_message(s, SettingsPath());
	}

	bool QuitRequested() override
	{
		SaveSettings();
		if (fGameRunning) {
			(new BAlert("Hedgewars", "A game is running. Quit the game first, then close this window.",
				"OK"))->Go(NULL);
			return false;
		}
		Send({"QUIT", "Closed"});
		return true;
	}

	// ── the connection ──
	struct ConnectArgs { std::string host, port; BMessenger target; int* fd; BLocker* lock; };

	// connects, then reads the server's messages until the connection closes
	static int32 reader_thread(void* data)
	{
		ConnectArgs* a = (ConnectArgs*)data;
		addrinfo hints = {}, *res = NULL;
		hints.ai_socktype = SOCK_STREAM;
		std::string error;
		int fd = -1;
		if (getaddrinfo(a->host.c_str(), a->port.c_str(), &hints, &res) != 0 || !res)
			error = "The server " + a->host + " was not found. Check its name.";
		else {
			for (addrinfo* r = res; r && fd < 0; r = r->ai_next) {
				fd = socket(r->ai_family, r->ai_socktype, r->ai_protocol);
				if (fd >= 0 && connect(fd, r->ai_addr, r->ai_addrlen) != 0) {
					close(fd);
					fd = -1;
				}
			}
			freeaddrinfo(res);
			if (fd < 0)
				error = "The server " + a->host + " refused the connection. Check the port, or try again later.";
		}
		if (fd >= 0) {
			BAutolock l(a->lock);
			*a->fd = fd;
		}
		std::string buf;
		char chunk[8192];
		ssize_t got;
		while (fd >= 0 && (got = recv(fd, chunk, sizeof(chunk), 0)) > 0) {
			buf.append(chunk, got);
			std::vector<std::string> parts;
			while (net::next_message(buf, parts)) {
				BMessage m(kMsgServer);
				for (const std::string& p : parts)
					m.AddString("part", p.c_str());
				a->target.SendMessage(&m);
			}
		}
		BMessage closed(kMsgClosed);
		closed.AddString("error", error.c_str());
		a->target.SendMessage(&closed);
		delete a;
		return 0;
	}

	void Connect()
	{
		if (fReader >= 0)
			return;
		std::string nick = fNick->Text();
		if (nick.empty()) {
			fConnectStatus->SetText("Type a nickname first.");
			return;
		}
		fMyNick = nick;
		fConnectStatus->SetText("Connecting…");
		fConnect->SetEnabled(false);
		fReader = spawn_thread(reader_thread, "net reader", B_NORMAL_PRIORITY,
			new ConnectArgs{fHost->Text(), fPort->Text(), BMessenger(this), &fFd, &fLock});
		resume_thread(fReader);
	}

	void Disconnect()
	{
		{
			BAutolock l(fLock);
			if (fFd >= 0)
				shutdown(fFd, SHUT_RDWR);	// wakes the reader
		}
		if (fReader >= 0) {
			status_t s;
			wait_for_thread(fReader, &s);
			fReader = -1;
		}
		BAutolock l(fLock);
		if (fFd >= 0)
			close(fFd);
		fFd = -1;
	}

	// any thread: the game thread relays the engine's messages through here
	void Send(const std::vector<std::string>& parts)
	{
		std::string m = net::message(parts);
		BAutolock l(fLock);
		for (size_t sent = 0; fFd >= 0 && sent < m.size();) {
			ssize_t n = send(fFd, m.data() + sent, m.size() - sent, 0);
			if (n <= 0)
				break;
			sent += n;
		}
	}

	void ToEngine(const std::string& data)
	{
		if (fEngineIn >= 0)
			write(fEngineIn, data.data(), data.size());
	}

	// ── what the server says (HWNewNet::ParseCmd) ──
	void Chat(const std::string& line)
	{
		append(fState == kInLobby ? fLobbyChat : fRoomChat, line);
	}

	void ServerMessage(const std::vector<std::string>& m)
	{
		const std::string& cmd = m[0];
		size_t n = m.size();
		if (cmd == "CONNECTED") {
			Send({"NICK", fMyNick});
			Send({"PROTO", kProtoVer});
		} else if (cmd == "PING") {
			Send(n > 1 ? std::vector<std::string>{"PONG", m[1]} : std::vector<std::string>{"PONG"});
		} else if (cmd == "NICK" && n > 1) {
			fMyNick = m[1];
		} else if (cmd == "ERROR" || cmd == "WARNING") {
			std::string text = n > 1 ? m[1] : "Unknown " + cmd;
			if (fState == kConnecting)
				fConnectStatus->SetText(text.c_str());
			else
				Chat("*** " + text);
		} else if (cmd == "BYE") {
			fByeReason = n > 1 ? m[1] : "";
		} else if (cmd == "NOTICE" && n > 1) {
			if (m[1] == "0")
				(new PromptWindow(this, "That nickname is taken. Choose another:", "nick", false, "Use nickname"))->Show();
			else if (m[1] == "2")
				(new PromptWindow(this, ("The room " + fRoom + " has a password:").c_str(), "room password", true, "Join room"))->Show();
		} else if (cmd == "ASKPASSWORD" && n > 1) {
			fServerSalt = m[1];
			(new PromptWindow(this, ("The nickname " + fMyNick + " is registered. Its password:").c_str(),
				"password", true, "Log in"))->Show();
		} else if (cmd == "SERVER_AUTH" && n > 1) {
			if (m[1] != fServerHash) {
				fByeReason = "The server could not prove it knows your password.";
				Disconnect();
			}
		} else if (cmd == "LOBBY:JOINED") {
			for (size_t i = 1; i < n; i++) {
				if (m[i] == fMyNick && fState == kConnecting) {
					fState = kInLobby;
					fLobbyChat->SetText("");
					SetTitle(("Network game: " + std::string(fHost->Text())).c_str());
					ShowCard(1);
				}
				if (std::find(fLobby.begin(), fLobby.end(), m[i]) == fLobby.end())
					fLobby.push_back(m[i]);
			}
			FillLobbyPlayers();
		} else if (cmd == "LOBBY:LEFT" && n > 1) {
			fLobby.erase(std::remove(fLobby.begin(), fLobby.end(), m[1]), fLobby.end());
			FillLobbyPlayers();
		} else if (cmd == "SERVER_MESSAGE" && n > 1) {
			append(fLobbyChat, m[1]);	// shortcut: shown as it comes, HTML and all
		} else if (cmd == "CHAT" && n > 2) {
			std::string text = m[2].compare(0, 4, "/me ") == 0 ? "* " + m[1] + " " + m[2].substr(4)
				: m[1] + ": " + m[2];
			Chat(text);
			if (fGameRunning)
				ToEngineChat(net::chat_line(m[1], m[2]));
		} else if (cmd == "INFO" && n > 4) {
			Chat(m[1] + " " + m[2] + " " + m[3] + " " + m[4]);
		} else if (cmd == "ROOMS") {
			fRoomList.clear();
			for (size_t i = 1; i + 10 <= n; i += 10)
				fRoomList.push_back(std::vector<std::string>(m.begin() + i, m.begin() + i + 10));
			FillRooms();
		} else if (cmd == "ROOM" && n > 2) {
			if (m[1] == "ADD" && n == 12)
				fRoomList.push_back(std::vector<std::string>(m.begin() + 2, m.end()));
			else if (m[1] == "UPD" && n == 13) {
				for (auto& r : fRoomList)
					if (r[1] == m[2])
						r.assign(m.begin() + 3, m.end());
				if (fRoom == m[2])
					SetRoomName(m[4]);
			} else if (m[1] == "DEL")
				fRoomList.erase(std::remove_if(fRoomList.begin(), fRoomList.end(),
					[&](const std::vector<std::string>& r) { return r[1] == m[2]; }), fRoomList.end());
			FillRooms();
		} else if (cmd == "CLIENT_FLAGS" || cmd == "CF") {
			if (n < 3 || m[1].size() < 2)
				return;
			bool set = m[1][0] == '+';
			for (size_t f = 1; f < m[1].size(); f++) {
				char c = m[1][f];
				for (size_t i = 2; i < n; i++) {
					std::string& flags = fFlags[m[i]];
					if (set && flags.find(c) == std::string::npos)
						flags += c;
					else if (!set)
						flags.erase(std::remove(flags.begin(), flags.end(), c), flags.end());
					if (m[i] == fMyNick && c == 'h' && fState == kInRoom)
						SetChief(set);
					if (m[i] == fMyNick && c == 'r' && fState == kInRoom)
						fReady->SetValue(set);
					if (m[i] == fMyNick && c == 'a')
						fAdmin = set;
				}
			}
			FillRoomPlayers();
		} else if (cmd == "SERVER_VARS" || cmd == "BANLIST") {
			BMessage r(kNetReply);
			for (const std::string& p : m)
				r.AddString("part", p.c_str());
			fAdminWindow.SendMessage(&r);
		} else if (cmd == "JOINING" && n > 1) {
			fRoom = m[1];
		} else if (cmd == "JOINED") {
			for (size_t i = 1; i < n; i++) {
				if (m[i] == fMyNick && fState == kInLobby)
					EnterRoom();
				if (std::find(fRoomNicks.begin(), fRoomNicks.end(), m[i]) == fRoomNicks.end())
					fRoomNicks.push_back(m[i]);
				append(fRoomChat, "*** " + m[i] + " has joined the room");
			}
			FillRoomPlayers();
		} else if (cmd == "LEFT" && n > 1 && fState == kInRoom) {
			fRoomNicks.erase(std::remove(fRoomNicks.begin(), fRoomNicks.end(), m[1]), fRoomNicks.end());
			append(fRoomChat, "*** " + m[1] + " has left" + (n > 2 ? " (" + m[2] + ")" : ""));
			FillRoomPlayers();
		} else if ((cmd == "KICKED" || cmd == "ROOMABANDONED") && fState == kInRoom) {
			LeaveRoom(cmd == "KICKED" ? "You were kicked from the room." : "The room was closed by its host.");
		} else if (fState == kInRoom) {
			RoomMessage(m);
		}
	}

	void RoomMessage(const std::vector<std::string>& m)
	{
		const std::string& cmd = m[0];
		size_t n = m.size();
		if (cmd == "EM") {
			for (size_t i = 1; i < n; i++)
				ToEngine(net::base64_decode(m[i]));
		} else if (cmd == "ROUND_FINISHED") {
			ToEngine(std::string("\x01o", 2));
		} else if (cmd == "ADD_TEAM") {
			net::Team t;
			if (net::parse_team(std::vector<std::string>(m.begin() + 1, m.end()), t)) {
				fTeamList.push_back(t);
				FillTeams();
			}
		} else if (cmd == "TEAM_ACCEPTED" && n == 2) {
			net::Team t = MyTeam();
			t.remote = false;
			t.owner = fMyNick;
			fTeamList.push_back(t);
			FillTeams();
		} else if (cmd == "REMOVE_TEAM" && n == 2) {
			fTeamList.erase(std::remove_if(fTeamList.begin(), fTeamList.end(),
				[&](const net::Team& t) { return t.name == m[1]; }), fTeamList.end());
			FillTeams();
		} else if ((cmd == "TEAM_COLOR" || cmd == "HH_NUM") && n == 3) {
			for (net::Team& t : fTeamList)
				if (t.name == m[1])
					(cmd == "HH_NUM" ? t.hogs : t.color) = atoi(m[2].c_str());
			FillTeams();
		} else if (cmd == "CFG" && n > 2) {
			net::set_param(fCfg, m[1], std::vector<std::string>(m.begin() + 2, m.end()));
			ShowConfig();
		} else if (cmd == "RUN_GAME") {
			StartGame();
		}
	}

	// ── lobby ──
	void FillLobbyPlayers()
	{
		while (BListItem* item = fLobbyPlayers->RemoveItem((int32)0))
			delete item;
		for (const std::string& nick : SortedLobby())
			fLobbyPlayers->AddItem(new BStringItem(nick.c_str()));
	}

	std::vector<std::string> SortedLobby()
	{
		std::vector<std::string> sorted = fLobby;
		std::sort(sorted.begin(), sorted.end());
		return sorted;
	}

	// the flags ("-gpjr") of the room we're in
	std::string RoomFlags()
	{
		for (const auto& r : fRoomList)
			if (r[1] == fRoom)
				return r[0];
		return "";
	}

	static std::string MapLabel(const std::string& map)
	{
		for (const auto& g : kGenerated)
			if (map == g.map)
				return g.label;
		return map == "+drawn+" ? "Hand-drawn" : spaced(map);
	}

	void FillRooms()
	{
		std::string selected;
		if (BRow* row = fRooms->CurrentSelection())
			selected = ((BStringField*)row->GetField(0))->String();
		fRooms->Clear();
		for (const auto& r : fRoomList) {
			// flags, name, players, teams, host, map, style, scheme, weapons, protocol
			std::string state = net::room_flags_text(r[0]);
			if (r[0].find('g') == std::string::npos)
				state = state.empty() ? "waiting" : "waiting, " + state;
			state[0] = toupper(state[0]);
			const std::string fields[8] = {r[1], state, r[2], r[3], r[4], MapLabel(r[5]), r[7], r[8]};
			BRow* row = new BRow();
			for (int i = 0; i < 8; i++)
				row->SetField(new BStringField(fields[i].c_str()), i);
			fRooms->AddRow(row);
			if (r[1] == selected)
				fRooms->AddToSelection(row);
		}
		fJoin->SetEnabled(fRooms->CurrentSelection() != NULL);
		if (fState == kInRoom)
			SetRoomName(fRoom);	// its flags may have changed
	}

	void JoinSelected(const std::string& password = "")
	{
		BRow* row = fRooms->CurrentSelection();
		if (!row && password.empty())
			return;
		if (row)
			fRoom = ((BStringField*)row->GetField(0))->String();
		fChief = false;
		if (password.empty())
			Send({"JOIN_ROOM", fRoom});
		else
			Send({"JOIN_ROOM", fRoom, password});
	}

	// ── room ──
	net::Team MyTeam()
	{
		net::Team t;
		t.name = fTeam.name;
		t.grave = fTeam.grave;
		t.fort = fTeam.fort;
		t.voice = fTeam.voice;
		t.flag = fTeam.flag;
		for (int h = 0; h < 8; h++) {
			t.hogNames[h] = fTeam.hogs[h].empty() ? "Hog " + std::to_string(h + 1) : fTeam.hogs[h];
			t.hats[h] = fTeam.hats[h].empty() ? "NoHat" : fTeam.hats[h];
		}
		return t;
	}

	bool MyTeamPlays()
	{
		for (const net::Team& t : fTeamList)
			if (!t.remote)
				return true;
		return false;
	}

	void SetRoomName(const std::string& name)
	{
		fRoom = name;
		std::string flags = net::room_flags_text(RoomFlags());
		fRoomTitle->SetText(("Room: " + name + (flags.empty() ? "" : " (" + flags + ")")).c_str());
	}

	void EnterRoom()
	{
		fState = kInRoom;
		fRoomNicks.clear();
		fTeamList.clear();
		fCfg.clear();
		fRestrictTeams = false;
		fRoomChat->SetText("");
		SetRoomName(fRoom);
		SetChief(fChief);
		FillTeams();
		ShowCard(2);
		if (fChief)
			SendConfig(true);
	}

	void LeaveRoom(const std::string& why)
	{
		fState = kInLobby;
		fChief = false;
		fReady->SetValue(0);
		for (auto& f : fFlags)
			f.second.erase(std::remove_if(f.second.begin(), f.second.end(),
				[](char c) { return c == 'r' || c == 'h' || c == 'g'; }), f.second.end());
		ShowCard(1);
		if (!why.empty())
			append(fLobbyChat, "*** " + why);
	}

	void SetChief(bool chief)
	{
		fChief = chief;
		for (BMenuField* f : {fStyle, fScheme, fWeapons, fMap, fTheme})
			f->SetEnabled(chief);
		fStart->SetEnabled(chief);
		fEditDrawing->SetEnabled(chief && MapIndex() == kHandDrawn);
		ShowTeam();
	}

	int MapIndex() { return std::max(fMap->Menu()->IndexOf(fMap->Menu()->FindMarked()), 0); }

	// predefined schemes and weapon sets, then the user's (Schemes/Game, Schemes/Ammo)
	void FillSchemeMenus()
	{
		std::string scheme = marked(fScheme), weapons = marked(fWeapons);
		fSchemeList = load_schemes(BPath(gUserDir.Path(), "Schemes/Game").Path());
		fWeaponList = load_weapons(BPath(gUserDir.Path(), "Schemes/Ammo").Path());
		auto fill = [](BMenuField* f, const auto& list, const std::string& select) {
			BMenu* m = f->Menu();
			m->RemoveItems(0, m->CountItems(), true);
			for (size_t i = 0; i < list.size(); i++) {
				if (i > 0 && !list[i].predefined && list[i - 1].predefined)
					m->AddItem(new BSeparatorItem());
				m->AddItem(new BMenuItem(list[i].name.c_str(), new BMessage(kMsgSetting)));
			}
			BMenuItem* item = m->FindItem(select.c_str());
			(item ? item : m->ItemAt(0))->SetMarked(true);
		};
		fill(fScheme, fSchemeList, scheme);
		fill(fWeapons, fWeaponList, weapons);
	}

	// schemes and drawings can change in their editors while the room is open
	void WindowActivated(bool active) override
	{
		if (!active || fGameRunning)
			return;
		FillSchemeMenus();
		if (fState == kInRoom && fChief)
			SendConfig(false);
		else if (fState == kInRoom)
			ShowConfig();
	}

	// the room master's choices to the others (GameCFGWidget::fullNetConfig)
	void SendConfig(bool newSeed)
	{
		if (!fChief)
			return;
		if (newSeed || fSeed.empty())
			fSeed = new_seed();
		int map = MapIndex();
		bool fixed = map > kGenCount;	// after the separator
		std::string mapName = fixed ? fMaps[map - kGenCount - 1] : kGenerated[map].map;
		int gen = fixed ? 0 : kGenerated[map].gen;
		std::string theme = marked(fTheme);
		if (fixed) {
			std::vector<std::string> cfg = read_lines(("Maps/" + mapName + "/map.cfg").c_str());
			if (!cfg.empty())
				theme = cfg[0];
		}
		GameScheme scheme = fSchemeList[0];
		for (const GameScheme& g : fSchemeList)
			if (g.name == marked(fScheme))
				scheme = g;
		WeaponSet weapons = fWeaponList[0];
		for (const WeaponSet& w : fWeaponList)
			if (w.name == marked(fWeapons))
				weapons = w;
		fEditDrawing->SetEnabled(map == kHandDrawn);
		std::string drawn = map == kHandDrawn ? drawn_map_file() : "";
		// only what changed since the last time (the editors can change a scheme or the drawing)
		for (const auto& m : net::master_config(scheme, weapons, mapName, gen, theme, fSeed, 0, drawn,
				marked(fStyle))) {
			std::vector<std::string> values(m.begin() + 1, m.end());
			auto sent = fCfg.find(m[0]);
			if (sent != fCfg.end() && sent->second == values)
				continue;
			std::vector<std::string> msg = {"CFG"};
			msg.insert(msg.end(), m.begin(), m.end());
			Send(msg);
			net::set_param(fCfg, m[0], values);
		}
		ShowConfig();
	}

	// the selected team, or NULL
	net::Team* SelectedTeam()
	{
		int32 i = fTeams->CurrentSelection();
		return i >= 0 && i < (int32)fTeamList.size() ? &fTeamList[i] : NULL;
	}

	// the selected team's hedgehogs and colour; the host changes them
	void ShowTeam()
	{
		net::Team* t = SelectedTeam();
		fHogs->SetEnabled(fChief && t);
		fTeamColor->SetEnabled(fChief && t);
		if (!t)
			return;
		fHogs->SetValue(t->hogs);
		if (BMenuItem* item = fTeamColor->Menu()->ItemAt(t->color >= 0 && t->color < 9 ? t->color : 0))
			item->SetMarked(true);
	}

	void ShowConfig()
	{
		if (fChief)
			return;	// the menus are the master's own choices
		if (!net::param(fCfg, "SCRIPT").empty())
			show_value(fStyle, net::param(fCfg, "SCRIPT"));
		if (!net::param(fCfg, "SCHEME").empty())
			show_value(fScheme, net::param(fCfg, "SCHEME"));
		if (!net::param(fCfg, "AMMO").empty())
			show_value(fWeapons, net::param(fCfg, "AMMO"));
		if (!net::param(fCfg, "MAP").empty())
			show_value(fMap, MapLabel(net::param(fCfg, "MAP")));
		if (!net::param(fCfg, "THEME").empty())
			show_value(fTheme, RoomTheme());
	}

	void FillRoomPlayers()
	{
		while (BListItem* item = fRoomPlayers->RemoveItem((int32)0))
			delete item;
		for (const std::string& nick : fRoomNicks) {
			const std::string& f = fFlags[nick];
			std::string label = nick;
			if (f.find('h') != std::string::npos) label += " (host)";
			if (f.find('r') != std::string::npos) label += " ✓ ready";
			if (f.find('g') != std::string::npos) label += " (playing)";
			fRoomPlayers->AddItem(new BStringItem(label.c_str()));
		}
	}

	void FillTeams()
	{
		int32 selected = fTeams->CurrentSelection();
		while (BListItem* item = fTeams->RemoveItem((int32)0))
			delete item;
		for (const net::Team& t : fTeamList) {
			std::string color = kColorNames[t.color >= 0 && t.color < 9 ? t.color : 0];
			color[0] = tolower(color[0]);
			std::string label = t.name + " (" + (t.remote ? t.owner : std::string("you")) + "), "
				+ std::to_string(t.hogs) + (t.hogs == 1 ? " hedgehog, " : " hedgehogs, ") + color;
			fTeams->AddItem(new BStringItem(label.c_str()));
		}
		if (selected >= 0 && selected < fTeams->CountItems())
			fTeams->Select(selected);	// posts kMsgTeamSel
		ShowTeam();
		fTeamButton->SetLabel(MyTeamPlays() ? "Remove my team" : "Add my team");
	}

	// ── a game ──
	void ToEngineChat(const std::string& line)
	{
		std::string buf;
		add(buf, "s" + line + "  ");	// HWGame::FromNetChat
		ToEngine(buf);
	}

	// a fixed map's own theme, or the room's
	std::string RoomTheme()
	{
		std::string map = net::param(fCfg, "MAP");
		std::string theme = net::param(fCfg, "THEME");
		if (!map.empty() && map[0] != '+') {
			std::vector<std::string> cfg = read_lines(("Maps/" + map + "/map.cfg").c_str());
			if (!cfg.empty())
				theme = cfg[0];
		}
		// the server's own default is a hedgehog emoji, until the host sends a theme
		return theme.empty() || theme == "\xf0\x9f\xa6\x94" ? "Nature" : theme;
	}

	void StartGame()
	{
		if (fGameRunning)
			return;
		std::string theme = RoomTheme();
		int p[2];
		if (pipe(p) != 0)
			return;
		Game* g = new Game(fPrefs);
		g->config = net::engine_config(fCfg, fTeamList, theme);
		g->args = {"--nick", net::base64_encode(fMyNick)};
		g->netIn = p[0];
		fEngineIn = p[1];
		NetWindow* self = this;
		g->toNet = [self](const std::string& data) { self->Send({"EM", net::base64_encode(data)}); };
		g->chat = [self](const std::string& text) {
			self->Send({"CHAT", text});
			BMessage m(kMsgRoomChat);
			m.AddString("echo", text.c_str());
			self->PostMessage(&m);
		};
		g->teamChat = [self](const std::string& text) { self->Send({"TEAMCHAT", text}); };
		fGameRunning = true;
		fTeamButton->SetEnabled(false);
		Hide();
		resume_thread(spawn_thread(game_thread, "net game", B_NORMAL_PRIORITY, g));
	}

	static int32 game_thread(void* data)
	{
		Game* g = (Game*)data;
		Outcome o = run_game(*g);
		close(g->netIn);
		BMessage done(kMsgNetGameOver);
		done.AddString("error", o.error.c_str());
		done.AddString("engine error", o.engineError.c_str());
		done.AddBool("finished", o.finished);
		for (const std::string& s : o.stats)
			done.AddString("stat", s.c_str());
		sNetWindow.SendMessage(&done);
		delete g;
		return 0;
	}

	void GameOver(BMessage* msg)
	{
		fGameRunning = false;
		close(fEngineIn);
		fEngineIn = -1;
		bool finished = msg->GetBool("finished");
		Send({"ROUNDFINISHED", finished ? "1" : "0"});
		fTeamButton->SetEnabled(true);
		if (IsHidden())
			Show();
		Activate();
		std::vector<std::string> stats;
		const char* s;
		for (int32 i = 0; msg->FindString("stat", i, &s) == B_OK; i++)
			stats.push_back(s);
		if (finished)
			show_stats(stats, Frame());
		std::string text = msg->GetString("error", "");
		if (text.empty() && msg->GetString("engine error", "")[0])
			text = std::string("The game engine stopped with an error:\n\n") + msg->GetString("engine error", "");
		if (!text.empty())
			(new BAlert("Hedgewars", text.c_str(), "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go(NULL);
	}

	// ── messages ──
	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kMsgConnect: Connect(); break;
			case kMsgServer: {
				std::vector<std::string> parts;
				const char* p;
				for (int32 i = 0; msg->FindString("part", i, &p) == B_OK; i++)
					parts.push_back(p);
				if (!parts.empty())
					ServerMessage(parts);
				break;
			}
			case kMsgClosed: {
				Disconnect();
				std::string why = msg->GetString("error", "");
				if (why.empty())
					why = !fByeReason.empty() ? "The server closed the connection: " + fByeReason
						: fState == kConnecting ? "The server closed the connection." : "";
				fByeReason.clear();
				fState = kConnecting;
				fLobby.clear();
				fRoomList.clear();
				fFlags.clear();
				fAdmin = false;
				fAdminWindow.SendMessage(B_QUIT_REQUESTED);
				fConnect->SetEnabled(true);
				fConnectStatus->SetText(why.c_str());
				SetTitle("Network game");
				ShowCard(0);
				break;
			}
			case kMsgDisconnect:
				Send({"QUIT", "Disconnected"});
				Disconnect();
				break;
			case kMsgLobbyChat:
			case kMsgRoomChat: {
				const char* echo;
				if (msg->FindString("echo", &echo) == B_OK) {	// typed in the game
					append(fRoomChat, fMyNick + ": " + echo);
					break;
				}
				BTextControl* entry = msg->what == kMsgLobbyChat ? fLobbyEntry : fRoomEntry;
				std::string text = entry->Text();
				if (text.empty())
					break;
				Send({"CHAT", text});
				Chat(text.compare(0, 4, "/me ") == 0 ? "* " + fMyNick + " " + text.substr(4) : fMyNick + ": " + text);
				entry->SetText("");
				break;
			}
			case kMsgRoomSel: fJoin->SetEnabled(fRooms->CurrentSelection() != NULL); break;
			case kMsgJoin: JoinSelected(); break;
			case kMsgCreate: open_create_room_window(BMessenger(this), Frame(), fLastRoomName); break;
			case kNetCreateRoom: {	// HWNewNet::CreateRoom
				std::string name = msg->GetString("name", ""), password = msg->GetString("password", "");
				fRoom = fLastRoomName = name;
				fChief = true;
				if (password.empty())
					Send({"CREATE_ROOM", name});
				else
					Send({"CREATE_ROOM", name, password});
				break;
			}
			case kNetSend: {
				std::vector<std::string> parts;
				const char* p;
				for (int32 i = 0; msg->FindString("part", i, &p) == B_OK; i++)
					parts.push_back(p);
				if (!parts.empty())
					Send(parts);
				break;
			}
			case kMsgPlayerMenu: {
				BListView* list;
				if (msg->FindPointer("list", (void**)&list) == B_OK)
					PlayerMenu(list, msg->GetPoint("where", BPoint()));
				break;
			}
			case kMsgKick:	// in a room: from the room (its host); in the lobby: from the server (an admin)
			case kMsgDelegate:
			case kMsgBan: {
				std::string nick = msg->GetString("nick", SelectedNick().c_str());
				if (nick.empty() || nick == fMyNick)
					break;
				if (msg->what == kMsgBan)
					open_ban_window(BMessenger(this), Frame(), nick);
				else
					Send({msg->what == kMsgKick ? "KICK" : "DELEGATE", nick});
				break;
			}
			case kMsgRename:
				(new PromptWindow(this, "A new name for the room:", "rename", false, "Rename"))->Show();
				break;
			case kMsgToggle: {
				std::string cmd = msg->GetString("cmd", "");
				if (cmd == "TOGGLE_RESTRICT_TEAMS")
					fRestrictTeams = !fRestrictTeams;	// the server doesn't tell
				Send({cmd});
				break;
			}
			case kMsgAdmin: {
				BLooper* looper = NULL;
				fAdminWindow.Target(&looper);
				if (BWindow* w = dynamic_cast<BWindow*>(looper)) {
					if (w->Lock()) {
						w->Activate();
						w->Unlock();
					}
				} else
					fAdminWindow = open_admin_window(BMessenger(this), Frame());
				break;
			}
			case kMsgTeamSel: ShowTeam(); break;
			case kMsgTeamColor:	// HWNewNet::onTeamColorChanged
				if (net::Team* t = SelectedTeam(); t && fChief) {
					t->color = std::max(fTeamColor->Menu()->IndexOf(fTeamColor->Menu()->FindMarked()), 0);
					Send({"TEAM_COLOR", t->name, std::to_string(t->color)});
					FillTeams();
				}
				break;
			case kMsgStyle: {
				// a style's .cfg names its scheme and weapons ("*": any), as on the Match tab
				int i = fStyle->Menu()->IndexOf(fStyle->Menu()->FindMarked());
				if (i > 1 && i - 2 < (int)fStyles.size()) {	// after Normal and the separator
					std::vector<std::string> cfg = read_lines(("Scripts/Multiplayer/" + fStyles[i - 2] + ".cfg").c_str());
					if (cfg.size() > 0 && cfg[0] != "*" && fScheme->Menu()->FindItem(spaced(cfg[0]).c_str()))
						show_value(fScheme, spaced(cfg[0]));
					if (cfg.size() > 1 && cfg[1] != "*" && fWeapons->Menu()->FindItem(spaced(cfg[1]).c_str()))
						show_value(fWeapons, spaced(cfg[1]));
				}
				SendConfig(false);
				break;
			}
			case kMsgLeave:
				Send({"PART"});
				LeaveRoom("");
				break;
			case kMsgTeam:
				if (MyTeamPlays())
					Send({"REMOVE_TEAM", fTeam.name});
				else {
					std::vector<std::string> m = {"ADD_TEAM"};
					std::vector<std::string> f = net::add_team(MyTeam());
					m.insert(m.end(), f.begin(), f.end());
					Send(m);
				}
				break;
			case kMsgReady: Send({"TOGGLE_READY"}); break;
			case kMsgHogs:	// HWNewNet::onHedgehogsNumChanged
				if (net::Team* t = SelectedTeam(); t && fChief && t->hogs != fHogs->Value()) {
					t->hogs = fHogs->Value();
					Send({"HH_NUM", t->name, std::to_string(t->hogs)});
					FillTeams();
				}
				break;
			case kMsgStart:
				SendConfig(false);	// a drawing changed since
				Send({"START_GAME"});
				break;
			case kMsgEditDrawing: show_draw_map(); break;
			case kMsgFollow:
				if (fState == kInLobby) {
					fChief = false;
					Send({"FOLLOW", msg->GetString("nick", "")});
				}
				break;
			case kMsgSetting: {
				BMenuItem* item;
				bool mapChanged = msg->FindPointer("source", (void**)&item) == B_OK
					&& item->Menu() == fMap->Menu();
				SendConfig(mapChanged);
				if (mapChanged && MapIndex() == kHandDrawn && drawn_map_empty())	// nothing drawn yet
					show_draw_map();
				break;
			}
			case kMsgPrompt: {
				std::string kind = msg->GetString("kind", ""), text = msg->GetString("text", "");
				if (msg->GetBool("cancel")) {
					if (kind != "room password") {	// no way into the lobby without a nickname
						Send({"QUIT", "Cancelled"});
						Disconnect();
					}
				} else if (kind == "nick" && !text.empty()) {
					fMyNick = text;
					fNick->SetText(text.c_str());
					Send({"NICK", text});
				} else if (kind == "room password")
					JoinSelected(text);
				else if (kind == "rename" && !text.empty())
					Send({"ROOM_NAME", text});	// the server's ROOM UPD renames it here too
				else if (kind == "password")
					SendPassword(text);
				break;
			}
			case kMsgNetGameOver: GameOver(msg); break;
			default:
				BWindow::MessageReceived(msg);
		}
	}

	// HWNewNet::maybeSendPassword: both sides prove they know the password's hash
	void SendPassword(const std::string& password)
	{
		std::string hash = hex_digest(password, EVP_md5());
		char salt[40];
		snprintf(salt, sizeof(salt), "{%08x-%04x-%04x-%04x-%08x%04x}", (unsigned)rand(),
			rand() & 0xffff, rand() & 0xffff, rand() & 0xffff, (unsigned)rand(), rand() & 0xffff);
		std::string clientSalt = salt;
		std::string tail = hash + kProtoVer + "!hedgewars";
		fServerHash = hex_digest(fServerSalt + clientSalt + tail, EVP_sha1());
		Send({"PASSWORD", hex_digest(clientSalt + fServerSalt + tail, EVP_sha1()), clientSalt});
	}

	static BMessenger sNetWindow;

private:
	enum { kConnecting, kInLobby, kInRoom } fState = kConnecting;
	TeamInfo fTeam;
	Game fPrefs;
	BLocker fLock;	// fFd, shared with the reader and game threads
	int fFd = -1, fEngineIn = -1;
	thread_id fReader = -1;
	bool fChief = false, fGameRunning = false, fAdmin = false, fRestrictTeams = false;
	std::string fMyNick, fRoom, fSeed, fByeReason, fServerSalt, fServerHash, fLastRoomName;
	std::vector<std::string> fLobby, fRoomNicks, fMaps, fStyles;
	std::vector<std::vector<std::string>> fRoomList;
	std::map<std::string, std::string> fFlags;	// CLIENT_FLAGS letters by nickname
	std::vector<net::Team> fTeamList;	// in the room, in the order the server gave them
	net::Config fCfg;
	BCardLayout* fCards;
	BTextControl *fHost, *fPort, *fNick, *fLobbyEntry, *fRoomEntry;
	BStringView *fConnectStatus, *fRoomTitle;
	BButton *fConnect, *fJoin, *fTeamButton, *fStart, *fEditDrawing;
	std::vector<GameScheme> fSchemeList;
	std::vector<WeaponSet> fWeaponList;
	BColumnListView* fRooms;
	BListView *fLobbyPlayers, *fRoomPlayers, *fTeams;
	BTextView *fLobbyChat, *fRoomChat;
	BMenuField *fStyle, *fScheme, *fWeapons, *fMap, *fTheme, *fTeamColor;
	BMenu *fRoomMenu, *fServerMenu;
	BMessenger fAdminWindow;
	BCheckBox* fReady;
	BSpinner* fHogs;
};

BMessenger NetWindow::sNetWindow;

void open_net_window(const TeamInfo& team, const Game& prefs)
{
	BLooper* looper = NULL;
	NetWindow::sNetWindow.Target(&looper);
	if (BWindow* w = dynamic_cast<BWindow*>(looper)) {	// one at a time: one nickname, one connection
		if (w->Lock()) {
			w->Activate();
			w->Unlock();
		}
		return;
	}
	NetWindow* w = new NetWindow(team, prefs);
	NetWindow::sNetWindow = BMessenger(w);
	w->Show();
}
