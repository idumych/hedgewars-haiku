// Hedgewars launcher for Haiku: a native stand-in for the Qt frontend's
// local game. It sets up a match, starts hwengine with --internal --port,
// and answers the engine's IPC requests (see frontend-qt6/game.cpp).
#include <Alert.h>
#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Path.h>
#include <PopUpMenu.h>
#include <Roster.h>
#include <Screen.h>
#include <Spinner.h>	// in private/interface on R1/beta6
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>
#include <AboutWindow.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "weapons.h"	// frontend-qt6: the default ammo scheme

extern char** environ;

static const char* kSignature = "application/x-vnd.hedgewars-launcher";
static const char* kAmmo = AMMOLINE_DEFAULT_QT AMMOLINE_DEFAULT_PROB AMMOLINE_DEFAULT_DELAY AMMOLINE_DEFAULT_CRATE;

enum { kMsgStart = 'strt', kMsgGameOver = 'gmov' };

static const char* const kOpponents[] = {
	"Computer (very hard)", "Computer (hard)", "Computer (medium)",
	"Computer (easy)", "Computer (very easy)", "Another player on this computer",
};
static const char* const kMaps[] = {"Random island", "Maze", "Cave (Perlin)"};

// ── paths ──
static BPath sAppDir, sDataDir, sUserDir;

static bool find_data()
{
	app_info info;
	be_app->GetAppInfo(&info);
	BPath app(&info.ref);
	app.GetParent(&sAppDir);
	std::vector<BPath> candidates;
	if (const char* env = getenv("HEDGEWARS_DATA"))
		candidates.push_back(BPath(env));
	candidates.push_back(BPath(sAppDir.Path(), "Data"));
	BPath sys;
	if (find_directory(B_SYSTEM_DATA_DIRECTORY, &sys) == B_OK)
		candidates.push_back(BPath(sys.Path(), "hedgewars/Data"));
#ifdef HW_DATADIR
	candidates.push_back(BPath(HW_DATADIR "/Data"));
#endif
	for (const BPath& p : candidates) {
		BEntry e(BPath(p.Path(), "misc/keys.csv").Path());
		if (e.Exists()) {
			sDataDir = p;
			break;
		}
	}
	find_directory(B_USER_SETTINGS_DIRECTORY, &sUserDir);
	sUserDir.Append("Hedgewars");
	create_directory(sUserDir.Path(), 0755);
	return sDataDir.InitCheck() == B_OK;
}

static std::vector<std::string> list_dir(const char* sub, const char* suffix, bool dirs)
{
	std::vector<std::string> out;
	BDirectory d(BPath(sDataDir.Path(), sub).Path());
	BEntry e;
	while (d.GetNextEntry(&e) == B_OK) {
		char name[B_FILE_NAME_LENGTH];
		e.GetName(name);
		std::string n = name;
		if (dirs != e.IsDirectory())
			continue;
		if (suffix) {
			size_t l = strlen(suffix);
			if (n.size() <= l || n.compare(n.size() - l, l, suffix) != 0)
				continue;
			n.resize(n.size() - l);
		}
		out.push_back(n);
	}
	std::sort(out.begin(), out.end());
	return out;
}

static bool theme_hidden(const std::string& theme)
{
	BFile f(BPath(sDataDir.Path(), ("Themes/" + theme + "/theme.cfg").c_str()).Path(), B_READ_ONLY);
	char buf[256] = {};
	f.Read(buf, sizeof(buf) - 1);
	return strstr(buf, "hidden = true") != NULL;
}

static std::vector<std::string> read_lines(const char* sub)
{
	std::vector<std::string> out;
	FILE* f = fopen(BPath(sDataDir.Path(), sub).Path(), "r");
	char line[256];
	while (f && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (line[0])
			out.push_back(line);
	}
	if (f)
		fclose(f);
	return out;
}

// ── the match ──
struct Team {
	std::string name;
	int difficulty;	// 0 human, 1 (very hard) .. 5 (very easy)
	uint32 color;
};

struct Match {
	std::vector<Team> teams;
	int hogs;
	int map;
	std::string theme;
	bool fullscreen;
};

static std::string pick(const std::vector<std::string>& v, const char* fallback)
{
	return v.empty() ? fallback : v[rand() % v.size()];
}

static void add(std::string& buf, const std::string& s)
{
	std::string m = s.substr(0, 250);	// the protocol's limit
	buf += (char)(uint8)m.size();
	buf += m;
}

static std::string match_config(const Match& m)
{
	std::string buf;
	add(buf, "TL");
	char seed[64];
	snprintf(seed, sizeof(seed), "eseed {%08x-%04x-%04x}", (unsigned)rand(), rand() & 0xffff, rand() & 0xffff);
	add(buf, seed);
	switch (m.map) {
		case 0:
			add(buf, "e$mapgen 0");
			add(buf, "e$template_filter " + std::to_string(1 << (rand() % 3)));
			add(buf, "e$feature_size " + std::to_string(rand() % 18 + 4));
			break;
		case 1:
			add(buf, "e$mapgen 1");
			add(buf, "e$template_filter " + std::to_string(rand() % 6));
			add(buf, "e$feature_size " + std::to_string(rand() % 16 + 6));
			break;
		default:
			add(buf, "e$mapgen 2");
			add(buf, "e$template_filter " + std::to_string(rand() % 6));
			add(buf, "e$feature_size " + std::to_string(rand() % 18 + 4));
			break;
	}
	add(buf, "etheme " + m.theme);

	std::vector<std::string> hats = list_dir("Graphics/Hats", ".png", false);
	std::vector<std::string> names = read_lines("Names/generic.txt");
	for (const Team& t : m.teams) {
		char line[300];
		snprintf(line, sizeof(line), "eaddteam 0 %u %s", (unsigned)t.color, t.name.c_str());
		add(buf, line);
		add(buf, "egrave Simple");
		add(buf, "efort Island");
		add(buf, "evoicepack Default");
		add(buf, "eflag hedgewars");
		for (int h = 0; h < m.hogs; h++) {
			add(buf, "eaddhh " + std::to_string(t.difficulty) + " 100 "
				+ pick(names, ("Hog " + std::to_string(h + 1)).c_str()));
			add(buf, "ehat " + pick(hats, "NoHat"));
		}
	}
	size_t n = strlen(kAmmo) / 4;
	std::string ammo = kAmmo;
	add(buf, "eammloadt " + ammo.substr(0, n));
	add(buf, "eammprob " + ammo.substr(n, n));
	add(buf, "eammdelay " + ammo.substr(2 * n, n));
	add(buf, "eammreinf " + ammo.substr(3 * n, n));
	for (size_t i = 0; i < m.teams.size(); i++)
		add(buf, "eammstore");
	return buf;
}

// Runs one game: serves the engine's requests until it disconnects.
// Returns an error message, or "" when the game ran.
static std::string run_game(const Match& m)
{
	int server = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	socklen_t len = sizeof(sa);
	if (bind(server, (sockaddr*)&sa, sizeof(sa)) < 0 || listen(server, 1) < 0
		|| getsockname(server, (sockaddr*)&sa, &len) < 0) {
		close(server);
		return "Couldn't open a local connection for the game.";
	}
	std::string port = std::to_string(ntohs(sa.sin_port));
	std::string engine = BPath(sAppDir.Path(), "hwengine").Path();
	BRect screen = BScreen().Frame();
	std::string sw = std::to_string((int)screen.Width() + 1), sh = std::to_string((int)screen.Height() + 1);
	std::vector<std::string> args = {engine, "--internal", "--port", port,
		"--prefix", sDataDir.Path(), "--user-prefix", sUserDir.Path(), "--locale", "en.txt",
		"--fullscreen-width", sw, "--fullscreen-height", sh, "--width", "1024", "--height", "768"};
	if (m.fullscreen)
		args.push_back("--fullscreen");
	std::vector<char*> argv;
	for (std::string& a : args)
		argv.push_back(&a[0]);
	argv.push_back(NULL);
	pid_t pid;
	if (posix_spawn(&pid, engine.c_str(), NULL, NULL, argv.data(), environ) != 0) {
		close(server);
		return "Couldn't start the game engine (hwengine).";
	}

	pollfd pfd = {server, POLLIN, 0};
	int client = -1;
	for (int waited = 0; waited < 30000 && client < 0; waited += 200) {	// 30 s for it to connect
		if (poll(&pfd, 1, 200) > 0)
			client = accept(server, NULL, NULL);
		else if (waitpid(pid, NULL, WNOHANG) == pid) {
			close(server);
			return "The game engine quit before it started.";
		}
	}
	close(server);
	if (client < 0) {
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
		return "The game engine didn't connect.";
	}

	std::string in;
	char chunk[4096];
	ssize_t got;
	while ((got = recv(client, chunk, sizeof(chunk), 0)) > 0) {
		in.append(chunk, got);
		while (!in.empty() && in.size() >= (size_t)(uint8)in[0] + 1) {
			std::string msg = in.substr(1, (uint8)in[0]);
			in.erase(0, msg.size() + 1);
			// the engine appends two bytes of game ticks to each message
			std::string reply;
			char type = msg.empty() ? 0 : msg[0];
			if (type == '?')
				add(reply, "!");
			else if (type == 'C')
				reply = match_config(m);
			else if (type == 'E' && msg.size() > 3)
				fprintf(stderr, "hwengine: %s\n", msg.substr(1, msg.size() - 3).c_str());
			if (!reply.empty())
				send(client, reply.data(), reply.size(), 0);
		}
	}
	close(client);
	int status;
	waitpid(pid, &status, 0);
	return "";
}

// ── window ──
class LauncherWindow : public BWindow {
public:
	LauncherWindow(BRect frame)
		: BWindow(frame, "Hedgewars", B_TITLED_WINDOW,
			B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE)
	{
		BMenuBar* bar = new BMenuBar("menu");
		BMenu* app = new BMenu("Hedgewars");
		app->AddItem(new BMenuItem("About Hedgewars", new BMessage(B_ABOUT_REQUESTED)));
		app->AddSeparatorItem();
		app->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
		app->SetTargetForItems(be_app);
		bar->AddItem(app);

		fTeam = new BTextControl("Your team:", "Haiku Hogs", NULL);
		fOpponent = popup("Opponent:", kOpponents, 6, 4);
		fHogs = new BSpinner("hogs", "Hedgehogs per team:", NULL);
		fHogs->SetRange(1, 8);
		fHogs->SetValue(4);
		fMap = popup("Map:", kMaps, 3, 0);
		std::vector<std::string> themes;
		for (const std::string& t : list_dir("Themes", NULL, true))
			if (!theme_hidden(t))
				themes.push_back(t);
		std::vector<const char*> tn;
		for (const std::string& t : themes)
			tn.push_back(t.c_str());
		int nature = 0;
		for (size_t i = 0; i < themes.size(); i++)
			if (themes[i] == "Nature")
				nature = i;
		fTheme = popup("Theme:", tn.data(), tn.size(), nature);
		fFullscreen = new BCheckBox("Full screen", NULL);
		fStart = new BButton("Start game", new BMessage(kMsgStart));
		fStatus = new BStringView("status", "");

		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.Add(bar)
			.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.SetInsets(B_USE_WINDOW_SPACING)
				.AddTextControl(fTeam, 0, 0)
				.AddMenuField(fOpponent, 0, 1)
				.Add(fHogs->CreateLabelLayoutItem(), 0, 2)
				.Add(fHogs->CreateTextViewLayoutItem(), 1, 2)
				.AddMenuField(fMap, 0, 3)
				.AddMenuField(fTheme, 0, 4)
				.Add(fFullscreen, 1, 5)
			.End()
			.AddGroup(B_HORIZONTAL)
				.SetInsets(B_USE_WINDOW_SPACING, 0, B_USE_WINDOW_SPACING, B_USE_WINDOW_SPACING)
				.Add(fStatus)
				.AddGlue()
				.Add(fStart)
			.End();
		SetDefaultButton(fStart);
		LoadSettings();
	}

	BMenuField* popup(const char* label, const char* const* items, int count, int marked)
	{
		BPopUpMenu* menu = new BPopUpMenu("");
		for (int i = 0; i < count; i++)
			menu->AddItem(new BMenuItem(items[i], NULL));
		if (count)
			menu->ItemAt(marked < count ? marked : 0)->SetMarked(true);
		return new BMenuField(label, menu);
	}

	static int marked(BMenuField* f) { return f->Menu()->IndexOf(f->Menu()->FindMarked()); }

	static void mark(BMenuField* f, const char* label)
	{
		if (BMenuItem* item = f->Menu()->FindItem(label))
			item->SetMarked(true);
	}

	BPath SettingsPath() { return BPath(sUserDir.Path(), "launcher_settings"); }

	void LoadSettings()
	{
		BFile file(SettingsPath().Path(), B_READ_ONLY);
		BMessage s;
		if (s.Unflatten(&file) != B_OK)
			return;
		BRect frame;
		if (s.FindRect("frame", &frame) == B_OK) {
			MoveTo(frame.LeftTop());
			MoveOnScreen();
		}
		const char* str;
		if (s.FindString("team", &str) == B_OK) fTeam->SetText(str);
		if (s.FindString("opponent", &str) == B_OK) mark(fOpponent, str);
		if (s.FindString("map", &str) == B_OK) mark(fMap, str);
		if (s.FindString("theme", &str) == B_OK) mark(fTheme, str);
		int32 v;
		if (s.FindInt32("hogs", &v) == B_OK) fHogs->SetValue(v);
		bool b;
		if (s.FindBool("fullscreen", &b) == B_OK) fFullscreen->SetValue(b);
	}

	void SaveSettings()
	{
		BMessage s;
		s.AddRect("frame", Frame());
		s.AddString("team", fTeam->Text());
		s.AddString("opponent", fOpponent->Menu()->FindMarked()->Label());
		s.AddString("map", fMap->Menu()->FindMarked()->Label());
		if (BMenuItem* t = fTheme->Menu()->FindMarked())
			s.AddString("theme", t->Label());
		s.AddInt32("hogs", fHogs->Value());
		s.AddBool("fullscreen", fFullscreen->Value());
		BFile file(SettingsPath().Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
		s.Flatten(&file);
	}

	bool QuitRequested() override
	{
		SaveSettings();
		return true;
	}

	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kMsgStart: {
				Match* m = new Match;
				std::string team = fTeam->Text()[0] ? fTeam->Text() : "Haiku Hogs";
				m->teams.push_back({team, 0, 0xff0204});
				int opp = marked(fOpponent);
				if (opp < 5)
					m->teams.push_back({"Computer", opp + 1, 0x4980c1});
				else
					m->teams.push_back({"Second Player", 0, 0x4980c1});
				m->hogs = fHogs->Value();
				m->map = marked(fMap);
				m->theme = fTheme->Menu()->FindMarked() ? fTheme->Menu()->FindMarked()->Label() : "Nature";
				m->fullscreen = fFullscreen->Value();
				SaveSettings();
				fStart->SetEnabled(false);
				fStatus->SetText("Game running…");
				Hide();	// out of the game's way; back when it ends
				thread_id t = spawn_thread(game_thread, "game", B_NORMAL_PRIORITY, m);
				resume_thread(t);
				break;
			}
			case kMsgGameOver: {
				if (IsHidden())
					Show();
				Activate();
				fStart->SetEnabled(true);
				fStatus->SetText("");
				const char* error;
				if (msg->FindString("error", &error) == B_OK && error[0]) {
					BAlert* a = new BAlert("Hedgewars", error, "OK", NULL, NULL,
						B_WIDTH_AS_USUAL, B_STOP_ALERT);
					a->Go(NULL);
				}
				break;
			}
			default:
				BWindow::MessageReceived(msg);
		}
	}

	static int32 game_thread(void* data)
	{
		Match* m = (Match*)data;
		BMessage done(kMsgGameOver);
		done.AddString("error", run_game(*m).c_str());
		delete m;
		be_app->WindowAt(0)->PostMessage(&done);
		return 0;
	}

private:
	BTextControl* fTeam;
	BMenuField *fOpponent, *fMap, *fTheme;
	BSpinner* fHogs;
	BCheckBox* fFullscreen;
	BButton* fStart;
	BStringView* fStatus;
};

class LauncherApp : public BApplication {
public:
	LauncherApp() : BApplication(kSignature) {}

	void ReadyToRun() override
	{
		srand(time(NULL));
		if (!find_data()) {
			(new BAlert("Hedgewars", "The Hedgewars game data wasn't found.", "Quit",
				NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
			Quit();
			return;
		}
		LauncherWindow* w = new LauncherWindow(BRect(100, 100, 400, 300));
		w->CenterOnScreen();
		w->Show();
	}

	void AboutRequested() override
	{
		BAboutWindow* about = new BAboutWindow("Hedgewars", kSignature);
		about->AddDescription("A turn-based strategy, artillery, action and comedy "
			"game. This is a native Haiku port of the engine, with this launcher "
			"in place of the Qt frontend.");
		about->AddCopyright(2004, "Andrey Korotaev and the Hedgewars contributors");
		about->AddText("Distributed under the GNU General Public License, version 2.");
		about->Show();
	}
};

int main()
{
	LauncherApp app;
	app.Run();
	return 0;
}
