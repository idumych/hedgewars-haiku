// The Team tab (see Team.h)
#include "Team.h"
#include "Ini.h"
#include "Settings.h"

#include <Alert.h>
#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <LayoutBuilder.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Node.h>
#include <PopUpMenu.h>
#include <StringView.h>
#include <TextControl.h>
#include <Window.h>

#include <algorithm>
#include <stdlib.h>
#include <string.h>

enum {
	kMsgTeamSel = 'tsel', kMsgNewTeam = 'tnew', kMsgTrashTeam = 'ttrs', kMsgRenamed = 'tren',
	kMsgControls = 'tctl', kMsgTeamBinds = 'tbnd', kMsgRandomNames = 'trnm', kMsgRandomHats = 'trht',
};
static const char* kNoHat = "No hat";
static const char* const kPlayers[] = {"Human", "Computer (level 5)", "Computer (level 4)",
	"Computer (level 3)", "Computer (level 2)", "Computer (level 1)"};

static BMenuField* popup(const char* label, const std::vector<std::string>& items, uint32 what = 0)
{
	BPopUpMenu* menu = new BPopUpMenu("");
	for (const std::string& s : items)
		menu->AddItem(new BMenuItem(s.c_str(), what ? new BMessage(what) : NULL));
	if (BMenuItem* item = menu->ItemAt(0))
		item->SetMarked(true);
	return new BMenuField(label, menu);
}

static void mark(BMenuField* f, const std::string& label)
{
	if (BMenuItem* item = f->Menu()->FindItem(label.c_str()))
		item->SetMarked(true);
}

static std::string marked(BMenuField* f)
{
	BMenuItem* item = f->Menu()->FindMarked();
	return item ? item->Label() : "";
}

static std::string pick(const std::vector<std::string>& v, const std::string& fallback)
{
	return v.empty() ? fallback : v[rand() % v.size()];
}

static std::string team_path(const std::string& name)
{
	return BPath(gUserDir.Path(), ("Teams/" + safe_file_name(name) + ".hwt").c_str()).Path();
}

// HWTeam::loadFromFile
static void load_team(const std::string& name, TeamInfo& t, Binds& b)
{
	Ini ini;
	ini.load(team_path(name));
	t.name = ini.get("Team", "Name", name);
	t.grave = ini.get("Team", "Grave", "Statue");
	t.fort = ini.get("Team", "Fort", "Plane");
	t.voice = ini.get("Team", "Voicepack", "Default");
	t.flag = ini.get("Team", "Flag", "hedgewars");
	t.difficulty = std::min(std::max(atoi(ini.get("Team", "Difficulty", "0").c_str()), 0), 5);
	for (int h = 0; h < 8; h++) {
		std::string hh = "Hedgehog" + std::to_string(h);
		t.hogs[h] = ini.get(hh, "Name", "Hedgehog " + std::to_string(h + 1));
		t.hats[h] = ini.get(hh, "Hat", "NoHat");
	}
	b = read_binds(ini);
}

// a window for one team's own keys; changes go back to the Team tab
class TeamControlsWindow : public BWindow {
public:
	TeamControlsWindow(const std::string& team, const Binds& binds, BMessenger target)
		: BWindow(BRect(0, 0, 300, 300), ("Controls for " + team).c_str(), B_TITLED_WINDOW,
			B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
		  fBinds(binds)
	{
		BView* editor = new_bind_editor(&fBinds, true, [this, target] {
			BMessage m(kMsgTeamBinds);
			for (const auto& b : fBinds) {
				m.AddString("action", b.first.c_str());
				m.AddString("key", b.second.c_str());
			}
			target.SendMessage(&m);
		});
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(new BStringView("hint", "Keys this team uses instead of the ones in Settings."))
			.Add(editor);
		CenterOnScreen();
	}

private:
	Binds fBinds;
};

TeamView::TeamView() : BGroupView("Team", B_VERTICAL)
{
	fHats = list_dir("Graphics/Hats", ".png", false);
	fNames = read_lines("Names/generic.txt");
	fTeamMenu = popup("Team:", {}, kMsgTeamSel);
	fNew = new BButton("New team", new BMessage(kMsgNewTeam));
	fTrash = new BButton("Move to Trash", new BMessage(kMsgTrashTeam));
	fTeamName = new BTextControl("Name:", "", new BMessage(kMsgRenamed));
	fPlayer = popup("Player:", std::vector<std::string>(kPlayers, kPlayers + 6));
	fFlag = popup("Flag:", list_dir("Graphics/Flags", ".png", false));
	fGrave = popup("Grave:", list_dir("Graphics/Graves", ".png", false));
	fFort = popup("Fort:", list_dir("Forts", "L.png", false));
	fVoice = popup("Voice:", list_dir("Sounds/voices", NULL, true));
	fControlsButton = new BButton("Controls…", new BMessage(kMsgControls));
	fRandomNames = new BButton("Random names", new BMessage(kMsgRandomNames));
	fRandomHats = new BButton("Random hats", new BMessage(kMsgRandomHats));
	std::vector<std::string> hats = {kNoHat};
	for (const std::string& h : fHats)
		if (h != "NoHat")
			hats.push_back(h);

	// room for a typical name in the current font
	BSize nameSize(be_plain_font->StringWidth("Sir Hedgehog the Great"), B_SIZE_UNSET);
	BLayoutItem* teamField = fTeamName->CreateTextViewLayoutItem();
	teamField->SetExplicitMinSize(nameSize);
	BGridLayout* grid;
	BLayoutBuilder::Group<>(this)
		.SetInsets(B_USE_DEFAULT_SPACING)
		.AddGroup(B_HORIZONTAL)
			.Add(fTeamMenu)
			.Add(fNew)
			.Add(fTrash)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_BIG_SPACING)
			.AddGroup(B_VERTICAL)
				.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
					.Add(fTeamName->CreateLabelLayoutItem(), 0, 0)
					.Add(teamField, 1, 0)
					.AddMenuField(fPlayer, 0, 1)
					.AddMenuField(fFlag, 0, 2)
					.AddMenuField(fGrave, 0, 3)
					.AddMenuField(fFort, 0, 4)
					.AddMenuField(fVoice, 0, 5)
				.End()
				.AddGroup(B_HORIZONTAL)
					.Add(fControlsButton)
					.AddGlue()
				.End()
				.AddGlue()
			.End()
			.AddGroup(B_VERTICAL)
				.AddGrid(B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
					.GetLayout(&grid)
				.End()
				.AddGroup(B_HORIZONTAL)
					.AddGlue()
					.Add(fRandomNames)
					.Add(fRandomHats)
				.End()
			.End()
		.End()
		.AddGlue();
	for (int h = 0; h < 8; h++) {
		fHogName[h] = new BTextControl(("Hedgehog " + std::to_string(h + 1) + ":").c_str(), "", NULL);
		fHat[h] = popup("", hats);
		fHat[h]->SetDivider(0);
		grid->AddItem(fHogName[h]->CreateLabelLayoutItem(), 0, h);
		BLayoutItem* field = fHogName[h]->CreateTextViewLayoutItem();
		field->SetExplicitMinSize(nameSize);
		grid->AddItem(field, 1, h);
		grid->AddView(fHat[h], 2, h);
	}
}

void TeamView::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	fTeamMenu->Menu()->SetTargetForItems(this);
	for (BControl* c : std::vector<BControl*>{fNew, fTrash, fTeamName, fControlsButton, fRandomNames, fRandomHats})
		c->SetTarget(this);
}

void TeamView::FillTeams()
{
	fTeams.clear();
	BMenu* menu = fTeamMenu->Menu();
	while (BMenuItem* item = menu->RemoveItem((int32)0))
		delete item;
	for (const std::string& file : list_dir("Teams", ".hwt", false, true)) {
		Ini ini;
		ini.load(BPath(gUserDir.Path(), ("Teams/" + file + ".hwt").c_str()).Path());
		std::string name = ini.get("Team", "Name", file);
		fTeams.push_back(name);
		BMenuItem* item = new BMenuItem(name.c_str(), new BMessage(kMsgTeamSel));
		item->SetTarget(this);
		item->SetMarked(name == fTeam.name);
		menu->AddItem(item);
	}
	fTrash->SetEnabled(fTeams.size() > 1);	// as in the Qt frontend: never no team at all
}

void TeamView::ShowTeam(const std::string& name)
{
	if (fControls.IsValid())
		fControls.SendMessage(B_QUIT_REQUESTED);
	load_team(name, fTeam, fBinds);
	fFileName = fTeam.name;
	ToFields();
	FillTeams();
}

void TeamView::Load(const BMessage& s)
{
	create_directory(BPath(gUserDir.Path(), "Teams").Path(), 0755);
	const char* str;
	std::string current = s.FindString("current team", &str) == B_OK ? str : "";
	if (list_dir("Teams", ".hwt", false, true).empty()) {
		// the first team: the one the launcher kept before teams had files, or a new one
		TeamInfo t;
		t.grave = "Statue";
		for (int h = 0; h < 8; h++)
			t.hogs[h] = pick(fNames, "Hedgehog " + std::to_string(h + 1));
		BMessage old;
		if (s.FindMessage("team", &old) == B_OK) {
			if (old.FindString("name", &str) == B_OK) t.name = str;
			if (old.FindString("flag", &str) == B_OK) t.flag = str;
			if (old.FindString("grave", &str) == B_OK) t.grave = str;
			if (old.FindString("fort", &str) == B_OK) t.fort = str;
			if (old.FindString("voice", &str) == B_OK) t.voice = str;
			for (int h = 0; h < 8; h++) {
				if (old.FindString("hog", h, &str) == B_OK) t.hogs[h] = str;
				if (old.FindString("hat", h, &str) == B_OK) t.hats[h] = str;
			}
		}
		fTeam = t;
		fBinds.clear();
		fFileName = t.name;
		Write();
		current = t.name;
	}
	FillTeams();
	if (std::find(fTeams.begin(), fTeams.end(), current) == fTeams.end())
		current = fTeams.empty() ? "" : fTeams[0];
	ShowTeam(current);
}

void TeamView::Save(BMessage& s)
{
	FromFields();
	Write();
	s.AddString("current team", fTeam.name.c_str());
}

TeamInfo TeamView::Team()
{
	FromFields();
	Write();
	return fTeam;
}

std::vector<std::string> TeamView::OtherTeams() const
{
	std::vector<std::string> out;
	for (const std::string& t : fTeams)
		if (t != fTeam.name)
			out.push_back(t);
	return out;
}

TeamInfo TeamView::SavedTeam(const std::string& name)
{
	TeamInfo t;
	Binds b;	// the engine reads them from the file itself
	load_team(name, t, b);
	return t;
}

void TeamView::ToFields()
{
	fTeamName->SetText(fTeam.name.c_str());
	if (BMenuItem* item = fPlayer->Menu()->ItemAt(fTeam.difficulty))
		item->SetMarked(true);
	mark(fFlag, fTeam.flag);
	mark(fGrave, fTeam.grave);
	mark(fFort, fTeam.fort);
	mark(fVoice, fTeam.voice);
	for (int h = 0; h < 8; h++) {
		fHogName[h]->SetText(fTeam.hogs[h].c_str());
		mark(fHat[h], fTeam.hats[h].empty() || fTeam.hats[h] == "NoHat" ? kNoHat : fTeam.hats[h]);
	}
}

void TeamView::FromFields()
{
	std::string name = fTeamName->Text();
	fTeam.name = name.empty() ? fFileName : name;
	fTeam.difficulty = std::max(0, (int)fPlayer->Menu()->IndexOf(fPlayer->Menu()->FindMarked()));
	fTeam.flag = marked(fFlag);
	fTeam.grave = marked(fGrave);
	fTeam.fort = marked(fFort);
	fTeam.voice = marked(fVoice);
	for (int h = 0; h < 8; h++) {
		fTeam.hogs[h] = fHogName[h]->Text();
		if (fTeam.hogs[h].empty())
			fTeam.hogs[h] = "Hedgehog " + std::to_string(h + 1);
		fTeam.hats[h] = marked(fHat[h]);
		if (fTeam.hats[h] == kNoHat)
			fTeam.hats[h] = "NoHat";
	}
}

// HWTeam::saveToFile: a renamed team's file is renamed too
bool TeamView::Write()
{
	if (fTeam.name != fFileName && BEntry(team_path(fTeam.name).c_str()).Exists()
		&& safe_file_name(fTeam.name) != safe_file_name(fFileName)) {
		std::string text = "There is already a team named \"" + fTeam.name + "\". Choose another name.";
		fTeam.name = fFileName;
		fTeamName->SetText(fFileName.c_str());
		(new BAlert("Hedgewars", text.c_str(), "OK", NULL, NULL, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go(NULL);
	}
	Ini ini;
	ini.load(team_path(fFileName));	// keeps what else is there (the Qt frontend's achievements)
	ini.set("Team", "Name", fTeam.name);
	ini.set("Team", "Grave", fTeam.grave);
	ini.set("Team", "Fort", fTeam.fort);
	ini.set("Team", "Voicepack", fTeam.voice);
	ini.set("Team", "Flag", fTeam.flag);
	ini.set("Team", "Difficulty", std::to_string(fTeam.difficulty));
	for (int h = 0; h < 8; h++) {
		std::string hh = "Hedgehog" + std::to_string(h);
		ini.set(hh, "Name", fTeam.hogs[h]);
		ini.set(hh, "Hat", fTeam.hats[h]);
	}
	write_binds(ini, fBinds, true);
	if (!ini.save(team_path(fTeam.name)))
		return false;
	if (safe_file_name(fTeam.name) != safe_file_name(fFileName))
		BEntry(team_path(fFileName).c_str()).Remove();
	bool renamed = fTeam.name != fFileName;
	fFileName = fTeam.name;
	if (renamed)
		FillTeams();
	return true;
}

void TeamView::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case kMsgTeamSel: {
			int32 i = msg->GetInt32("index", -1);
			FromFields();
			Write();
			if (i >= 0 && i < (int32)fTeams.size())
				ShowTeam(fTeams[i]);
			break;
		}
		case kMsgNewTeam: {
			FromFields();
			Write();
			std::string name = "New team";
			for (int n = 2; BEntry(team_path(name).c_str()).Exists(); n++)
				name = "New team " + std::to_string(n);
			TeamInfo t;
			t.name = name;
			t.grave = "Statue";
			for (int h = 0; h < 8; h++)
				t.hogs[h] = pick(fNames, "Hedgehog " + std::to_string(h + 1));
			fTeam = t;
			fBinds.clear();
			fFileName = name;
			Write();
			ShowTeam(name);
			fTeamName->MakeFocus();
			break;
		}
		case kMsgTrashTeam: {
			if (fTeams.size() < 2)
				break;
			// like Tracker: into the Trash with the attribute it restores from
			BPath path(team_path(fFileName).c_str()), trash;
			BEntry entry(path.Path());
			if (find_directory(B_TRASH_DIRECTORY, &trash) == B_OK) {
				BNode(&entry).WriteAttr("_trk/original_path", B_STRING_TYPE, 0, path.Path(), strlen(path.Path()) + 1);
				BDirectory dir(trash.Path());
				if (entry.MoveTo(&dir) != B_OK) {
					(new BAlert("Hedgewars", "Could not move the team to the Trash.", "OK"))->Go(NULL);
					break;
				}
			}
			std::string next = fTeams[0] == fFileName ? fTeams[1] : fTeams[0];
			ShowTeam(next);
			break;
		}
		case kMsgRenamed:
			FromFields();
			Write();
			break;
		case kMsgControls: {
			if (fControls.IsValid()) {
				fControls.SendMessage(B_QUIT_REQUESTED);
			}
			FromFields();
			Write();
			BWindow* w = new TeamControlsWindow(fTeam.name, fBinds, BMessenger(this));
			fControls = BMessenger(w);
			w->Show();
			break;
		}
		case kMsgTeamBinds: {
			fBinds.clear();
			const char *a, *k;
			for (int32 i = 0; msg->FindString("action", i, &a) == B_OK && msg->FindString("key", i, &k) == B_OK; i++)
				fBinds[a] = k;
			FromFields();
			Write();
			break;
		}
		case kMsgRandomNames:
			for (int h = 0; h < 8; h++)
				fHogName[h]->SetText(pick(fNames, fHogName[h]->Text()).c_str());
			break;
		case kMsgRandomHats:
			for (int h = 0; h < 8; h++) {
				std::string hat = pick(fHats, "NoHat");
				mark(fHat[h], hat == "NoHat" ? kNoHat : hat);
			}
			break;
		default:
			BGroupView::MessageReceived(msg);
	}
}
