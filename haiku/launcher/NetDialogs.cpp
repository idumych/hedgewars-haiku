// The network game's smaller windows (see NetDialogs.h)
#include "NetDialogs.h"

#include <Alert.h>
#include <Button.h>
#include <ColumnListView.h>	// private/interface
#include <ColumnTypes.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <RadioButton.h>
#include <Spinner.h>	// private/interface
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <Window.h>

#include <stdlib.h>
#include <vector>

#include "NetProto.h"

static void send(BMessenger& net, const std::vector<std::string>& parts)
{
	BMessage m(kNetSend);
	for (const std::string& p : parts)
		m.AddString("part", p.c_str());
	net.SendMessage(&m);
}

static BTextControl* secret_field(const char* label)
{
	BTextControl* t = new BTextControl(label, "", NULL);
	t->TextView()->HideTyping(true);
	return t;
}

// a small window over another, with Cancel and an action button
class Dialog : public BWindow {
public:
	Dialog(const char* title, BRect over)
		: BWindow(BRect(0, 0, 320, 120), title, B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
			B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
		fOver(over) {}

	// call once the subclass made its fields
	void Build(BView* fields, const char* action)
	{
		BButton* ok = new BButton(action, new BMessage('ok  '));
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(fields)
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(new BButton("Cancel", new BMessage(B_QUIT_REQUESTED)))
				.Add(ok)
			.End();
		SetDefaultButton(ok);
		CenterIn(fOver);
	}

	void MessageReceived(BMessage* msg) override
	{
		if (msg->what == 'ok  ' && Accept())
			Quit();
		else
			BWindow::MessageReceived(msg);
	}

	virtual bool Accept() = 0;	// false keeps the window open

private:
	BRect fOver;
};

// ── create room (roomnameprompt.cpp) ──
class CreateRoomWindow : public Dialog {
public:
	CreateRoomWindow(BMessenger net, BRect over, const std::string& name)
		: Dialog("Create room", over), fNet(net)
	{
		fName = new BTextControl("Name:", name.c_str(), NULL);
		fPassword = secret_field("Password:");
		BGroupView* v = new BGroupView(B_VERTICAL);
		BLayoutBuilder::Group<>(v)
			.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.AddTextControl(fName, 0, 0)
				.AddTextControl(fPassword, 0, 1)
			.End()
			.Add(new BStringView("hint", "Without a password, anyone can join."));
		fName->SetExplicitMinSize(BSize(be_plain_font->StringWidth("A fairly long room name"), B_SIZE_UNSET));
		Build(v, "Create room");
		fName->MakeFocus();
	}

	bool Accept() override
	{
		if (!fName->Text()[0]) {
			fName->MakeFocus();
			return false;
		}
		BMessage m(kNetCreateRoom);
		m.AddString("name", fName->Text());
		m.AddString("password", fPassword->Text());
		fNet.SendMessage(&m);
		return true;
	}

private:
	BMessenger fNet;
	BTextControl *fName, *fPassword;
};

void open_create_room_window(BMessenger net, BRect over, const std::string& name)
{
	(new CreateRoomWindow(net, over, name))->Show();
}

// ── ban (bandialog.cpp) ──
static const struct { const char* label; int seconds; } kDurations[] = {
	{"10 minutes", 600}, {"30 minutes", 1800}, {"1 hour", 3600}, {"3 hours", 3 * 3600},
	{"5 hours", 5 * 3600}, {"12 hours", 12 * 3600}, {"1 day", 86400}, {"3 days", 3 * 86400},
	{"7 days", 7 * 86400}, {"14 days", 14 * 86400}, {"Permanent", 3650 * 86400},
};

class BanWindow : public Dialog {
public:
	BanWindow(BMessenger net, BRect over, const std::string& nick)
		: Dialog(nick.empty() ? "Add ban" : ("Ban " + nick).c_str(), over), fNet(net), fNick(nick)
	{
		fReason = new BTextControl("Reason:", "", NULL);
		BPopUpMenu* menu = new BPopUpMenu("");
		for (const auto& d : kDurations)
			menu->AddItem(new BMenuItem(d.label, NULL));
		menu->ItemAt(0)->SetMarked(true);
		fDuration = new BMenuField("Duration:", menu);
		BGroupView* v = new BGroupView(B_VERTICAL);
		BLayoutBuilder::Group<> b(v);
		if (nick.empty()) {
			fByIP = new BRadioButton("IP address", NULL);
			fByNick = new BRadioButton("Nickname", NULL);
			fByIP->SetValue(B_CONTROL_ON);
			fWho = new BTextControl("IP address or nickname:", "", NULL);
			b.AddGroup(B_HORIZONTAL).Add(fByIP).Add(fByNick).AddGlue().End();
			b.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.AddTextControl(fWho, 0, 0)
				.AddTextControl(fReason, 0, 1)
				.AddMenuField(fDuration, 0, 2)
			.End();
		} else {
			b.Add(new BStringView("what", ("Bans the address " + nick + " connects from, and disconnects them.").c_str()));
			b.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.AddTextControl(fReason, 0, 0)
				.AddMenuField(fDuration, 0, 1)
			.End();
		}
		Build(v, "Ban");
		(nick.empty() ? fWho : fReason)->MakeFocus();
	}

	bool Accept() override
	{
		std::string reason = fReason->Text()[0] ? fReason->Text() : "you know why";	// Qt's
		int i = fDuration->Menu()->IndexOf(fDuration->Menu()->FindMarked());
		std::string seconds = std::to_string(kDurations[i < 0 ? 0 : i].seconds);
		if (!fNick.empty()) {
			send(fNet, {"BAN", fNick, reason, seconds});
			return true;
		}
		bool ip = fByIP->Value() == B_CONTROL_ON;
		if (!fWho->Text()[0]) {
			(new BAlert("Hedgewars", ip ? "Type the IP address to ban." : "Type the nickname to ban.",
				"OK"))->Go(NULL);
			return false;
		}
		send(fNet, {ip ? "BANIP" : "BANNICK", fWho->Text(), reason, seconds});
		send(fNet, {"BANLIST"});	// for the administration window
		return true;
	}

private:
	BMessenger fNet;
	std::string fNick;
	BTextControl *fWho = NULL, *fReason;
	BRadioButton *fByIP = NULL, *fByNick = NULL;
	BMenuField* fDuration;
};

void open_ban_window(BMessenger net, BRect over, const std::string& nick)
{
	(new BanWindow(net, over, nick))->Show();
}

// ── administration (pageadmin.cpp) ──
enum { kFetch = 'afet', kApply = 'aapl', kClearCache = 'acac', kRefreshBans = 'aref',
	kAddBan = 'aadd', kRemoveBan = 'arem', kBanSel = 'asel' };

class AdminWindow : public BWindow {
public:
	AdminWindow(BMessenger net, BRect over)
		: BWindow(BRect(0, 0, 520, 320), "Server administration", B_TITLED_WINDOW,
			B_AUTO_UPDATE_SIZE_LIMITS | B_CLOSE_ON_ESCAPE),
		fNet(net)
	{
		fMotdNew = new BTextControl("Message for the latest version:", "", NULL);
		fMotdOld = new BTextControl("Message for older versions:", "", NULL);
		fProto = new BSpinner("proto", "Latest protocol number:", NULL);
		fProto->SetRange(0, 9999);
		BGroupView* general = new BGroupView("General", B_VERTICAL);
		BLayoutBuilder::Group<>(general)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.AddTextControl(fMotdNew, 0, 0)
				.AddTextControl(fMotdOld, 0, 1)
				.Add(fProto->CreateLabelLayoutItem(), 0, 2)
				.Add(fProto->CreateTextViewLayoutItem(), 1, 2)
			.End()
			.Add(new BStringView("motd hint", "Players see the message (HTML) when they join the lobby."))
			.AddGlue()
			.AddGroup(B_HORIZONTAL)
				.Add(new BButton("Clear accounts cache", new BMessage(kClearCache)))
				.AddGlue()
				.Add(new BButton("Reload", new BMessage(kFetch)))
				.Add(new BButton("Apply", new BMessage(kApply)))
			.End();

		fBans = new BColumnListView("bans", B_NAVIGABLE, B_FANCY_BORDER);
		const char* cols[] = {"IP address or nickname", "Time left", "Reason"};
		float widths[] = {170, 80, 200};
		for (int i = 0; i < 3; i++)
			fBans->AddColumn(new BStringColumn(cols[i], widths[i], 40, 500, B_TRUNCATE_END), i);
		fBans->SetSelectionMessage(new BMessage(kBanSel));
		fBans->SetExplicitMinSize(BSize(400, 150));
		fRemove = new BButton("Remove", new BMessage(kRemoveBan));
		fRemove->SetEnabled(false);
		BGroupView* bans = new BGroupView("Bans", B_VERTICAL);
		BLayoutBuilder::Group<>(bans)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(fBans)
			.AddGroup(B_HORIZONTAL)
				.Add(new BButton("Refresh", new BMessage(kRefreshBans)))
				.AddGlue()
				.Add(new BButton("Add…", new BMessage(kAddBan)))
				.Add(fRemove)
			.End();

		BTabView* tabs = new BTabView("tabs", B_WIDTH_FROM_LABEL);
		tabs->AddTab(general);
		tabs->AddTab(bans);
		tabs->SetBorder(B_NO_BORDER);
		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.SetInsets(0, B_USE_SMALL_SPACING, 0, 0)
			.Add(tabs);
		// shortcut: centred over the network window each time, not remembered; remember it if asked
		CenterIn(over);
		send(fNet, {"GET_SERVER_VAR"});
		send(fNet, {"BANLIST"});
	}

	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kNetReply: {
				std::vector<std::string> parts;
				const char* p;
				for (int32 i = 0; msg->FindString("part", i, &p) == B_OK; i++)
					parts.push_back(p);
				if (parts.empty())
					break;
				std::vector<std::string> f(parts.begin() + 1, parts.end());
				if (parts[0] == "SERVER_VARS") {
					auto v = net::server_vars(f);
					fMotdNew->SetText(v["MOTD_NEW"].c_str());
					fMotdOld->SetText(v["MOTD_OLD"].c_str());
					fProto->SetValue(atoi(v["LATEST_PROTO"].c_str()));
				} else if (parts[0] == "BANLIST") {
					fBans->Clear();
					for (const net::Ban& b : net::bans(f)) {
						BRow* row = new BRow();
						row->SetField(new BStringField(b.who.c_str()), 0);
						row->SetField(new BStringField(b.left.c_str()), 1);
						row->SetField(new BStringField(b.reason.c_str()), 2);
						fBans->AddRow(row);
					}
					fRemove->SetEnabled(false);
				}
				break;
			}
			case kFetch: send(fNet, {"GET_SERVER_VAR"}); break;
			case kApply:	// PageAdmin::smChanged
				fProto->SetValue(atoi(fProto->TextView()->Text()));	// typed, not yet entered
				send(fNet, {"SET_SERVER_VAR", "MOTD_NEW", fMotdNew->Text()});
				send(fNet, {"SET_SERVER_VAR", "MOTD_OLD", fMotdOld->Text()});
				send(fNet, {"SET_SERVER_VAR", "LATEST_PROTO", std::to_string(fProto->Value())});
				break;
			case kClearCache: send(fNet, {"CLEAR_ACCOUNTS_CACHE"}); break;
			case kRefreshBans: send(fNet, {"BANLIST"}); break;
			case kAddBan: open_ban_window(fNet, Frame(), ""); break;
			case kBanSel: fRemove->SetEnabled(fBans->CurrentSelection() != NULL); break;
			case kRemoveBan:
				if (BRow* row = fBans->CurrentSelection()) {
					send(fNet, {"UNBAN", ((BStringField*)row->GetField(0))->String()});
					send(fNet, {"BANLIST"});
				}
				break;
			default:
				BWindow::MessageReceived(msg);
		}
	}

private:
	BMessenger fNet;
	BTextControl *fMotdNew, *fMotdOld;
	BSpinner* fProto;
	BColumnListView* fBans;
	BButton* fRemove;
};

BMessenger open_admin_window(BMessenger net, BRect over)
{
	AdminWindow* w = new AdminWindow(net, over);
	w->Show();
	return BMessenger(w);
}
