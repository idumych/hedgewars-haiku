// Hedgewars launcher for Haiku: a native stand-in for the Qt frontend's
// local pages (game setup, training and missions, campaigns, demos and
// saved games, the team). Game.cpp runs the engine.
#include <AboutWindow.h>
#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Button.h>
#include <DateTimeFormat.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <MenuBar.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Node.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <SeparatorItem.h>
#include <Spinner.h>	// in private/interface on R1/beta6
#include <StringView.h>
#include <TabView.h>
#include <TextControl.h>
#include <TextView.h>
#include <TranslationUtils.h>
#include <Window.h>

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "Content.h"
#include "DrawMap.h"
#include "Editors.h"
#include "Game.h"
#include "MapOptions.h"
#include "Net.h"
#include "SchemeData.h"
#include "Settings.h"
#include "Stats.h"
#include "Team.h"
#include "Videos.h"

static const char* kSignature = "application/x-vnd.hedgewars-launcher";

static const char* const kOpponents[] = {
	"Computer (very hard)", "Computer (hard)", "Computer (medium)",
	"Computer (easy)", "Computer (very easy)", "Another player on this computer",
};
static const char* const kGenerated[] = {"Random island", "Maze", "Cave", "Forts", "WFC", "Hand-drawn"};
static const int kGeneratedGen[] = {0, 1, 2, 4, 5, 3};	// e$mapgen
static const int kGenCount = 6, kHandDrawn = 5;	// then the fixed maps
static const char* const kMissionKinds[] = {"Training", "Challenge", "Scenario"};
static const char* const kMissionKindLabels[] = {"Training", "Challenges", "Scenarios"};

enum {
	kMsgStartMatch = 'stma', kMsgStartMission = 'stmi', kMsgStartCampaign = 'stca',
	kMsgPlayRecording = 'plrc', kMsgTrashRecording = 'trrc', kMsgGameOver = 'gmov',
	kMsgScheme = 'schm', kMsgStyle = 'styl', kMsgMap = 'map ', kMsgMissionKind = 'mkin',
	kMsgMissionSel = 'msel', kMsgCampaign = 'camp', kMsgCampaignSel = 'csel',
	kMsgRecordingSel = 'rsel', kMsgSettings = 'sett', kMsgTab = 'tab ', kMsgEditDrawing = 'edrw',
	kMsgNetGame = 'netg', kMsgContent = 'cont', kMsgEditSchemes = 'edsc', kMsgEditWeapons = 'edwp',
};

static BMessenger sWindow;

static std::string pick(const std::vector<std::string>& v, const std::string& fallback)
{
	return v.empty() ? fallback : v[rand() % v.size()];
}

static std::string spaced(std::string s)
{
	for (char& c : s)
		if (c == '_')
			c = ' ';
	return s;
}

// A picture that keeps its aspect ratio
class Thumbnail : public BView {
public:
	Thumbnail() : BView("thumbnail", B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
	{
		SetExplicitSize(BSize(156, 129));	// half the missions' 314x260 @2x pictures
		SetViewUIColor(B_PANEL_BACKGROUND_COLOR);
	}
	~Thumbnail() { delete fBitmap; }

	void SetPicture(const std::string& dataPath)
	{
		delete fBitmap;
		fBitmap = BTranslationUtils::GetBitmap(BPath(gDataDir.Path(), dataPath.c_str()).Path());
		Invalidate();
	}

	void Draw(BRect) override
	{
		if (!fBitmap)
			return;
		BRect b = fBitmap->Bounds(), r = Bounds();
		float scale = std::min(r.Width() / b.Width(), r.Height() / b.Height());
		BRect dst(0, 0, b.Width() * scale, b.Height() * scale);
		dst.OffsetBy((r.Width() - dst.Width()) / 2, (r.Height() - dst.Height()) / 2);
		SetDrawingMode(B_OP_ALPHA);
		DrawBitmap(fBitmap, b, dst, B_FILTER_BITMAP_BILINEAR);
	}

private:
	BBitmap* fBitmap = NULL;
};

static BTextView* description_view(BScrollView** scroll)
{
	BTextView* t = new BTextView("description");
	t->MakeEditable(false);
	t->SetWordWrap(true);
	t->SetExplicitMinSize(BSize(220, 129));
	*scroll = new BScrollView("description scroll", t, 0, false, true);
	return t;
}

static BListView* list_view(BScrollView** scroll, uint32 selectWhat, uint32 invokeWhat)
{
	BListView* l = new BListView("list");
	l->SetSelectionMessage(new BMessage(selectWhat));
	l->SetInvocationMessage(new BMessage(invokeWhat));
	l->SetExplicitMinSize(BSize(380, 170));
	*scroll = new BScrollView("list scroll", l, 0, false, true);
	return l;
}

static void clear_list(BListView* l)
{
	while (BListItem* item = l->RemoveItem((int32)0))
		delete item;
}

// tells the window which tab is showing, so Enter starts that tab's game
class Tabs : public BTabView {
public:
	Tabs() : BTabView("tabs", B_WIDTH_FROM_LABEL) {}
	void Select(int32 index) override
	{
		BTabView::Select(index);
		if (Window())
			Window()->PostMessage(kMsgTab);
	}
};

// ── window ──
class LauncherWindow : public BWindow {
public:
	LauncherWindow()
		: BWindow(BRect(100, 100, 700, 500), "Hedgewars", B_TITLED_WINDOW,
			B_AUTO_UPDATE_SIZE_LIMITS | B_QUIT_ON_WINDOW_CLOSE)
	{
		fHats = list_dir("Graphics/Hats", ".png", false);
		fNames = read_lines("Names/generic.txt");

		BMenuBar* bar = new BMenuBar("menu");
		BMenu* app = new BMenu("Hedgewars");
		app->AddItem(new BMenuItem("About Hedgewars", new BMessage(B_ABOUT_REQUESTED)));
		app->AddItem(new BMenuItem("Settings…", new BMessage(kMsgSettings), ','));
		app->AddSeparatorItem();
		BMenuItem* net = new BMenuItem("Network game…", new BMessage(kMsgNetGame));
		BMenuItem* content = new BMenuItem("Downloadable content", new BMessage(kMsgContent));
		app->AddItem(net);
		app->AddItem(content);
		app->AddSeparatorItem();
		app->AddItem(new BMenuItem("Quit", new BMessage(B_QUIT_REQUESTED), 'Q'));
		app->SetTargetForItems(be_app);
		app->ItemAt(1)->SetTarget(this);
		net->SetTarget(this);
		content->SetTarget(this);
		bar->AddItem(app);

		fTabs = new Tabs;
		fTabs->AddTab(MatchTab());
		fTabs->AddTab(MissionsTab());
		fTabs->AddTab(CampaignsTab());
		fTabs->AddTab(RecordingsTab());
		fTabs->AddTab(fVideos = new VideosView);
		fDefault[4] = fVideos->PlayButton();
		fTabs->AddTab(fTeamView = new TeamView);
		fTabs->SetBorder(B_NO_BORDER);
		fStatus = new BStringView("status", "");
		fStatus->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));	// else it caps the window's width

		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.Add(bar)
			.AddGroup(B_VERTICAL)
				.SetInsets(B_USE_WINDOW_SPACING)
				.Add(fTabs)
				.Add(fStatus)
			.End();
		sWindow = BMessenger(this);
		CenterOnScreen();
		LoadSettings();
		FillMissions();
		FillCampaign();
		FillRecordings();
		PostMessage(kMsgTab);
	}

	// ── match ──
	BView* MatchTab()
	{
		BGroupView* v = new BGroupView("Match", B_VERTICAL);
		fOpponent = Popup("Opponent:", kOpponents, 6, 4, 0);
		fHogs = new BSpinner("hogs", "Hedgehogs per team:", NULL);
		fHogs->SetRange(1, 8);
		fHogs->SetValue(4);
		std::vector<std::string> names;
		fScheme = Popup("Scheme:", names, 0, kMsgScheme);
		fWeapons = Popup("Weapons:", names, 0, 0);
		FillSchemeMenus("", "");
		fStyles = list_dir("Scripts/Multiplayer", ".lua", false);
		names = {"Normal"};
		for (const std::string& s : fStyles)
			names.push_back(spaced(s));
		fStyle = Popup("Style:", names, 0, kMsgStyle);
		fStyle->Menu()->AddItem(new BSeparatorItem(), 1);

		names.assign(kGenerated, kGenerated + kGenCount);
		for (const std::string& m : list_dir("Maps", NULL, true)) {
			BPath dir(gDataDir.Path(), ("Maps/" + m).c_str());
			// missions (with a map.lua) and the test map aren't for a match
			if (m != "test" && BEntry(BPath(dir.Path(), "map.cfg").Path()).Exists()
				&& !BEntry(BPath(dir.Path(), "map.lua").Path()).Exists())
				fMaps.push_back(m);
		}
		for (const std::string& m : fMaps)
			names.push_back(spaced(m));
		fMap = Popup("Map:", names, 0, kMsgMap);
		fMap->Menu()->AddItem(new BSeparatorItem(), kGenCount);
		fEditDrawing = new BButton("Edit drawing…", new BMessage(kMsgEditDrawing));

		names.clear();
		int nature = 0;
		for (const std::string& t : list_dir("Themes", NULL, true)) {
			std::string cfg = ini_value(("Themes/" + t + "/theme.cfg").c_str(), "hidden ");
			if (cfg.find("true") != std::string::npos)
				continue;
			if (t == "Nature")
				nature = names.size();
			names.push_back(t);
		}
		fTheme = Popup("Theme:", names, nature, 0);
		BButton* start = fDefault[0] = new BButton("Start game", new BMessage(kMsgStartMatch));
		BLayoutItem* hogsField = fHogs->CreateTextViewLayoutItem();
		hogsField->SetExplicitMaxSize(BSize(90, B_SIZE_UNSET));
		hogsField->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));

		BLayoutBuilder::Group<>(v)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.AddMenuField(fOpponent, 0, 0)
				.Add(fHogs->CreateLabelLayoutItem(), 0, 1)
				.Add(hogsField, 1, 1)
				.AddMenuField(fScheme, 0, 2)
				.Add(new BButton("Edit…", new BMessage(kMsgEditSchemes)), 2, 2)
				.AddMenuField(fWeapons, 0, 3)
				.Add(new BButton("Edit…", new BMessage(kMsgEditWeapons)), 2, 3)
				.AddMenuField(fStyle, 0, 4)
				.AddMenuField(fMap, 0, 5)
				.Add(fEditDrawing, 2, 5)
				.AddMenuField(fTheme, 0, 6)
				.AddMenuField(fMapOptions.size, 0, 7)
				.AddMenuField(fMapOptions.maze, 0, 8)
				.Add(fMapOptions.feature, 0, 9, 2)
				.AddTextControl(fMapOptions.seed, 0, 10)
				.AddGlue(3, 0)
				.SetColumnWeight(3, 10)
			.End()
			.AddGlue()
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(start)
			.End();
		return v;
	}

	BMenuField* Popup(const char* label, const std::vector<std::string>& items, int marked, uint32 what)
	{
		BPopUpMenu* menu = new BPopUpMenu("");
		for (const std::string& s : items)
			menu->AddItem(new BMenuItem(s.c_str(), what ? new BMessage(what) : NULL));
		if (BMenuItem* item = menu->ItemAt(marked))
			item->SetMarked(true);
		return new BMenuField(label, menu);
	}

	BMenuField* Popup(const char* label, const char* const* items, int count, int marked, uint32 what)
	{
		return Popup(label, std::vector<std::string>(items, items + count), marked, what);
	}

	static int Marked(BMenuField* f)
	{
		int i = f->Menu()->IndexOf(f->Menu()->FindMarked());
		// separators count as items; report the index among the real items
		for (int j = 0; j < i; j++)
			if (dynamic_cast<BSeparatorItem*>(f->Menu()->ItemAt(j)))
				i--;
		return i;
	}

	static std::string MarkedLabel(BMenuField* f)
	{
		BMenuItem* item = f->Menu()->FindMarked();
		return item ? item->Label() : "";
	}

	static void Mark(BMenuField* f, const char* label)
	{
		if (BMenuItem* item = f->Menu()->FindItem(label))
			item->SetMarked(true);
	}

	std::string MatchConfig()
	{
		GameScheme s = fSchemeList[0];
		for (const GameScheme& g : fSchemeList)
			if (g.name == MarkedLabel(fScheme))
				s = g;
		std::string ammo = fWeaponList[0].ammo;
		for (const WeaponSet& w : fWeaponList)
			if (w.name == MarkedLabel(fWeapons))
				ammo = w.ammo;
		std::string buf;
		add(buf, "TL");
		int style = Marked(fStyle);
		if (style > 0)
			add(buf, "escript Scripts/Multiplayer/" + fStyles[style - 1] + ".lua");
		int map = Marked(fMap);
		std::string theme = MarkedLabel(fTheme);
		if (map >= kGenCount) {
			std::string name = fMaps[map - kGenCount];
			add(buf, "emap " + name);
			std::vector<std::string> cfg = read_lines(("Maps/" + name + "/map.cfg").c_str());
			if (!cfg.empty())
				theme = cfg[0];
		}
		add(buf, "etheme " + theme);
		add(buf, "eseed " + fMapOptions.Seed());
		for (const std::string& c : scheme_commands(s))
			add(buf, c);
		int gen = map >= kGenCount ? 0 : kGeneratedGen[map];
		std::string after;
		fMapOptions.Config(gen, buf, after);
		add(buf, "e$mapgen " + std::to_string(gen));
		add(buf, "e$scriptparam " + s.scriptparam);
		buf += after;
		if (gen == 3)
			buf += drawn_map_config();

		std::vector<TeamInfo> teams = {fTeamView->Team()};
		int opp = Marked(fOpponent);
		TeamInfo other;
		other.name = opp < 5 ? "Computer" : "Second Player";
		other.difficulty = opp < 5 ? opp + 1 : 0;
		other.color = 0x4980c1;
		other.flag = "cm_binary";
		for (int h = 0; h < 8; h++) {
			other.hogs[h] = pick(fNames, "Hog " + std::to_string(h + 1));
			other.hats[h] = pick(fHats, "NoHat");
		}
		teams.push_back(other);
		size_t n = ammo.size() / 4;
		for (const TeamInfo& t : teams) {
			add(buf, "eammloadt " + ammo.substr(0, n));
			add(buf, "eammprob " + ammo.substr(n, n));
			add(buf, "eammdelay " + ammo.substr(2 * n, n));
			add(buf, "eammreinf " + ammo.substr(3 * n, n));
			if (s.v[15] || !s.v[21])	// shared ammo, or not per-hog ammo
				add(buf, "eammstore");
			buf += team_config(t, fHogs->Value(), s.v[28], false);
		}
		if (getenv("HEDGEWARS_LOG_CONFIG"))	// every message, one per line, for checking what the engine gets
			for (size_t i = 0; i < buf.size(); i += (uint8)buf[i] + 1)
				fprintf(stderr, "config: %s\n", buf.substr(i + 1, (uint8)buf[i]).c_str());
		return buf;
	}

	// ── missions ──
	BView* MissionsTab()
	{
		BGroupView* v = new BGroupView("Missions", B_VERTICAL);
		fMissionKind = Popup("Kind:", kMissionKindLabels, 3, 0, kMsgMissionKind);
		BScrollView *ls, *ds;
		fMissionList = list_view(&ls, kMsgMissionSel, kMsgStartMission);
		fMissionText = description_view(&ds);
		fMissionThumb = new Thumbnail;
		BLayoutBuilder::Group<>(v)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.AddGroup(B_HORIZONTAL)
				.Add(fMissionKind)
				.AddGlue()
			.End()
			.Add(ls)
			.AddGroup(B_HORIZONTAL)
				.Add(fMissionThumb)
				.Add(ds)
			.End()
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(fDefault[1] = new BButton("Start mission", new BMessage(kMsgStartMission)))
			.End();
		return v;
	}

	std::string MissionKind() { return kMissionKinds[std::max(Marked(fMissionKind), 0)]; }

	// PageTraining: order.cfg first, then the rest
	void FillMissions()
	{
		std::string kind = MissionKind();
		fMissionIds = read_lines(("Missions/" + kind + "/order.cfg").c_str());
		for (const std::string& m : list_dir(("Missions/" + kind).c_str(), ".lua", false))
			if (std::find(fMissionIds.begin(), fMissionIds.end(), m) == fMissionIds.end())
				fMissionIds.push_back(m);
		clear_list(fMissionList);
		for (const std::string& id : fMissionIds) {
			std::string name = locale_value("missions", id + ".name", spaced(id));
			if (get_progress("Mission " + id + "/Won") == "true")
				name += "  ✓";
			fMissionList->AddItem(new BStringItem(name.c_str()));
		}
		fMissionList->Select(0);
	}

	void ShowMission()
	{
		int i = fMissionList->CurrentSelection();
		if (i < 0)
			return;
		const std::string& id = fMissionIds[i];
		fMissionText->SetText(locale_value("missions", id + ".desc", "No description available.").c_str());
		std::string pic = "Graphics/Missions/" + MissionKind() + "/" + id + "@2x.png";
		fMissionThumb->SetPicture(pic);
	}

	// ── campaigns ──
	BView* CampaignsTab()
	{
		BGroupView* v = new BGroupView("Campaigns", B_VERTICAL);
		fCampaigns = list_dir("Missions/Campaign", NULL, true);
		std::vector<std::string> names;
		for (const std::string& c : fCampaigns)
			names.push_back(locale_value("campaigns", c + ".name", spaced(c)));
		fCampaign = Popup("Campaign:", names, 0, kMsgCampaign);
		BScrollView *ls, *ds;
		fCampaignList = list_view(&ls, kMsgCampaignSel, kMsgStartCampaign);
		fCampaignText = description_view(&ds);
		fCampaignThumb = new Thumbnail;
		BLayoutBuilder::Group<>(v)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.AddGroup(B_HORIZONTAL)
				.Add(fCampaign)
				.AddGlue()
			.End()
			.Add(ls)
			.AddGroup(B_HORIZONTAL)
				.Add(fCampaignThumb)
				.Add(ds)
			.End()
			.AddGroup(B_HORIZONTAL)
				.AddGlue()
				.Add(fDefault[2] = new BButton("Start mission", new BMessage(kMsgStartCampaign)))
			.End();
		return v;
	}

	std::string Campaign()
	{
		int i = Marked(fCampaign);
		return i >= 0 && i < (int)fCampaigns.size() ? fCampaigns[i] : "";
	}

	// campaign.cpp getCampMissionList: the missions the saved progress unlocks, newest first
	void FillCampaign()
	{
		std::string camp = Campaign();
		std::string ini = "Missions/Campaign/" + camp + "/campaign.ini";
		std::string key = "Campaign " + camp + "/";
		int progress = atoi(get_progress(key + "Progress").c_str());
		int unlocked = atoi(get_progress(key + "UnlockedMissions").c_str());
		std::vector<int> missions;
		if (unlocked == 0)
			for (int i = progress + 1; i > 0; i--)
				missions.push_back(i);
		else
			for (int i = 1; i <= unlocked; i++)
				missions.push_back(atoi(get_progress(key + "Mission" + std::to_string(i)).c_str()));
		fCampaignScripts.clear();
		clear_list(fCampaignList);
		for (int m : missions) {
			std::string script = ini_value(ini.c_str(), "Mission " + std::to_string(m) + "/Script");
			if (script.empty())
				continue;
			std::string base = script.substr(0, script.rfind(".lua"));
			std::string name = locale_value("campaigns", camp + "-" + base + ".name",
				ini_value(ini.c_str(), "Mission " + std::to_string(m) + "/Name"));
			fCampaignScripts.push_back(script);
			fCampaignList->AddItem(new BStringItem(name.c_str()));
		}
		fCampaignList->Select(0);
	}

	void ShowCampaignMission()
	{
		int i = fCampaignList->CurrentSelection();
		if (i < 0)
			return;
		std::string camp = Campaign();
		std::string base = fCampaignScripts[i].substr(0, fCampaignScripts[i].rfind(".lua"));
		fCampaignText->SetText(locale_value("campaigns", camp + "-" + base + ".desc",
			"No description available.").c_str());
		fCampaignThumb->SetPicture("Graphics/Missions/Campaign/" + camp + "/" + base + "@2x.png");
	}

	// ── recordings ──
	BView* RecordingsTab()
	{
		BGroupView* v = new BGroupView("Recordings", B_VERTICAL);
		BScrollView* ls;
		fRecordingList = list_view(&ls, kMsgRecordingSel, kMsgPlayRecording);
		BStringView* hint = new BStringView("hint", "Interrupted games are kept as saved games, finished ones as demos.");
		hint->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
		fPlay = fDefault[3] = new BButton("Play", new BMessage(kMsgPlayRecording));
		fTrash = new BButton("Move to Trash", new BMessage(kMsgTrashRecording));
		BLayoutBuilder::Group<>(v)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(hint)
			.Add(ls)
			.AddGroup(B_HORIZONTAL)
				.Add(fTrash)
				.AddGlue()
				.Add(fPlay)
			.End();
		return v;
	}

	void FillRecordings()
	{
		// a refresh keeps the selection: it runs each time the window is activated
		int sel = fRecordingList->CurrentSelection();
		std::string selected = sel >= 0 && sel < (int)fRecordings.size() ? fRecordings[sel] : "";
		fRecordings.clear();
		clear_list(fRecordingList);
		for (const char* kind : {"Saves", "Demos"}) {
			std::vector<std::string> files = list_dir(kind, NULL, false, true);
			for (auto f = files.rbegin(); f != files.rend(); ++f) {	// newest first
				std::string rel = std::string(kind) + "/" + *f;
				std::string label = kind[0] == 'S' ? "Saved game, " : "Demo, ";
				// the date in the system's format, from when the file was written
				time_t when;
				BString date;
				if (BEntry(BPath(gUserDir.Path(), rel.c_str()).Path()).GetModificationTime(&when) == B_OK
					&& BDateTimeFormat().Format(date, when, B_SHORT_DATE_FORMAT, B_SHORT_TIME_FORMAT) == B_OK)
					label += date.String();
				else
					label += f->substr(0, f->find('.'));
				fRecordings.push_back(rel);
				fRecordingList->AddItem(new BStringItem(label.c_str()));
			}
		}
		auto at = std::find(fRecordings.begin(), fRecordings.end(), selected);
		fRecordingList->Select(at != fRecordings.end() ? at - fRecordings.begin() : 0);
		bool any = !fRecordings.empty();
		fPlay->SetEnabled(any);
		fTrash->SetEnabled(any);
	}

		void TrashRecording()
	{
		int i = fRecordingList->CurrentSelection();
		if (i < 0)
			return;
		if (!move_to_trash(BPath(gUserDir.Path(), fRecordings[i].c_str()).Path()))
			(new BAlert("Hedgewars", "Could not move the recording to the Trash.", "OK"))->Go(NULL);
		FillRecordings();
	}

	// ── settings ──
	// the menus whose choice is remembered, by settings key
	std::vector<std::pair<const char*, BMenuField*>> SavedPopups()
	{
		return {{"opponent", fOpponent}, {"scheme", fScheme}, {"weapons", fWeapons},
			{"style", fStyle}, {"map", fMap}, {"theme", fTheme}, {"map size", fMapOptions.size},
			{"maze style", fMapOptions.maze},
			{"mission kind", fMissionKind}, {"campaign", fCampaign}};
	}

	BPath SettingsPath() { return BPath(gUserDir.Path(), "launcher_settings"); }

	void LoadSettings()
	{
		BFile file(SettingsPath().Path(), B_READ_ONLY);
		BMessage s;
		bool loaded = s.Unflatten(&file) == B_OK;
		const char* str;
		if (loaded) {
			BRect frame;
			if (s.FindRect("frame", &frame) == B_OK) {
				MoveTo(frame.LeftTop());
				ResizeTo(frame.Width(), frame.Height());	// the size limits keep it big enough
				MoveOnScreen();
			}
			for (const auto& p : SavedPopups())
				if (s.FindString(p.first, &str) == B_OK)
					Mark(p.second, str);
			int32 v;
			if (s.FindInt32("hogs", &v) == B_OK) fHogs->SetValue(v);
			fMapOptions.feature->SetValue(s.GetInt32("feature size", 12));
			if (s.FindInt32("tab", &v) == B_OK) fTabs->Select(v);
		}
		fTeamView->Load(s);
		UpdateThemeField();
	}

	void SaveSettings()
	{
		BMessage s;
		s.AddRect("frame", Frame());
		for (const auto& p : SavedPopups())
			s.AddString(p.first, MarkedLabel(p.second).c_str());
		s.AddInt32("hogs", fHogs->Value());
		s.AddInt32("feature size", fMapOptions.feature->Value());
		s.AddInt32("tab", fTabs->Selection());
		fTeamView->Save(s);
		save_message(s, SettingsPath());
	}

	// a fixed map brings its own theme
	void UpdateThemeField()
	{
		int map = Marked(fMap);
		fTheme->SetEnabled(map < kGenCount);
		fEditDrawing->SetEnabled(map == kHandDrawn);
		fMapOptions.Update(map < kGenCount ? kGeneratedGen[std::max(map, 0)] : -1);
	}

	// predefined schemes and weapon sets, then the user's; keeps the choice when it's still there
	void FillSchemeMenus(std::string scheme, std::string weapons)
	{
		if (scheme.empty())
			scheme = MarkedLabel(fScheme);
		if (weapons.empty())
			weapons = MarkedLabel(fWeapons);
		fSchemeList = load_schemes(BPath(gUserDir.Path(), "Schemes/Game").Path());
		fWeaponList = load_weapons(BPath(gUserDir.Path(), "Schemes/Ammo").Path());
		auto fill = [](BMenuField* f, const auto& list, const std::string& select, uint32 what) {
			BMenu* m = f->Menu();
			m->RemoveItems(0, m->CountItems(), true);
			for (size_t i = 0; i < list.size(); i++) {
				const auto& item = list[i];
				if (i > 0 && !item.predefined && list[i - 1].predefined)
					m->AddItem(new BSeparatorItem());
				m->AddItem(new BMenuItem(item.name.c_str(), what ? new BMessage(what) : NULL));
			}
			BMenuItem* marked = m->FindItem(select.c_str());
			(marked ? marked : m->ItemAt(0))->SetMarked(true);
		};
		fill(fScheme, fSchemeList, scheme, kMsgScheme);
		fill(fWeapons, fWeaponList, weapons, 0);
	}

	// recordings can come and go outside the launcher
	void WindowActivated(bool active) override
	{
		if (active) {
			FillRecordings();
			fVideos->Fill();
		}
	}

	bool QuitRequested() override
	{
		SaveSettings();
		return true;
	}

	// ── running a game ──
	void Start(Game* g)
	{
		SaveSettings();
		fStatus->SetText("Game running…");
		Hide();	// out of the game's way; back when it ends
		resume_thread(spawn_thread(game_thread, "game", B_NORMAL_PRIORITY, g));
	}

	std::string MissionConfig(const std::string& script)
	{
		std::string buf;
		add(buf, "TL");
		buf += team_config(fTeamView->Team(), 8, 100, true);
		add(buf, "eseed " + fMapOptions.Seed());
		add(buf, "escript " + script);
		return buf;
	}

	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kMsgStartMatch: {
				Game* g = new Game;
				g->config = MatchConfig();
				Start(g);
				break;
			}
			case kMsgStartMission: {
				int i = fMissionList->CurrentSelection();
				if (i < 0)
					break;
				Game* g = new Game;
				g->config = MissionConfig("Missions/" + MissionKind() + "/" + fMissionIds[i] + ".lua");
				g->varPrefix = "Mission " + fMissionIds[i] + "/";
				Start(g);
				break;
			}
			case kMsgStartCampaign: {
				int i = fCampaignList->CurrentSelection();
				if (i < 0)
					break;
				Game* g = new Game;
				g->config = MissionConfig("Missions/Campaign/" + Campaign() + "/" + fCampaignScripts[i]);
				g->varPrefix = "Campaign " + Campaign() + "/";
				Start(g);
				break;
			}
			case kMsgPlayRecording: {
				int i = fRecordingList->CurrentSelection();
				if (i < 0)
					break;
				BFile f(BPath(gUserDir.Path(), fRecordings[i].c_str()).Path(), B_READ_ONLY);
				off_t size = 0;
				f.GetSize(&size);
				Game* g = new Game;
				g->replay.resize(size);
				if (size <= 0 || f.Read(&g->replay[0], size) != size) {
					delete g;
					(new BAlert("Hedgewars", "Could not read the recording. It may be damaged.", "OK"))->Go(NULL);
					break;
				}
				Start(g);
				break;
			}
			case kMsgTrashRecording: TrashRecording(); break;
			case kMsgNetGame: open_net_window(fTeamView->Team(), Game()); break;	// settings: engine_args()
			case kMsgContent: open_content_window(); break;
			case kMsgScheme: {
				// as in the Qt frontend, a scheme picks the weapon set of the same name
				Mark(fWeapons, MarkedLabel(fScheme).c_str());
				break;
			}
			case kMsgStyle: {
				// a style's .cfg names its scheme and weapons ("*": any)
				int i = Marked(fStyle);
				if (i > 0) {
					std::vector<std::string> cfg = read_lines(("Scripts/Multiplayer/" + fStyles[i - 1] + ".cfg").c_str());
					if (cfg.size() > 0 && cfg[0] != "*") Mark(fScheme, spaced(cfg[0]).c_str());
					if (cfg.size() > 1 && cfg[1] != "*") Mark(fWeapons, spaced(cfg[1]).c_str());
				}
				break;
			}
			case kMsgMap:
				UpdateThemeField();
				if (Marked(fMap) == kHandDrawn && drawn_map_empty())	// nothing drawn yet
					show_draw_map();
				break;
			case kMsgEditDrawing: show_draw_map(); break;
			case kMsgSettings: show_settings(); break;
			case kMsgLanguageChanged:
				FillMissions();
				FillCampaign();
				break;
			case kMsgEditSchemes: show_scheme_editor(BMessenger(this), MarkedLabel(fScheme)); break;
			case kMsgEditWeapons: show_weapon_editor(BMessenger(this), MarkedLabel(fWeapons)); break;
			case kMsgEditorsChanged: {
				std::string select = msg->GetString("select", "");
				bool schemes = strcmp(msg->GetString("kind", ""), "schemes") == 0;
				FillSchemeMenus(schemes ? select : "", schemes ? "" : select);
				break;
			}
			case kMsgTab: {
				int i = fTabs->Selection();
				SetDefaultButton(i >= 0 && i < 5 ? fDefault[i] : NULL);	// the Team tab has none
				break;
			}
			case kMsgMissionKind: FillMissions(); break;
			case kMsgMissionSel: ShowMission(); break;
			case kMsgCampaign: FillCampaign(); break;
			case kMsgCampaignSel: ShowCampaignMission(); break;
			case kMsgGameOver: {
				if (IsHidden())
					Show();
				Activate();
				const char *error, *engineError, *saved;
				msg->FindString("error", &error);
				msg->FindString("engine error", &engineError);
				msg->FindString("saved", &saved);
				std::string status;
				if (saved[0])
					status = strstr(saved, "/Saves/") ? "Kept as a saved game in Recordings."
						: "Kept as a demo in Recordings.";
				fStatus->SetText(status.c_str());
				FillMissions();
				FillCampaign();
				FillRecordings();
				std::vector<std::string> stats;
				const char* stat;
				for (int32 i = 0; msg->FindString("stat", i, &stat) == B_OK; i++)
					stats.push_back(stat);
				if (msg->GetBool("finished"))
					show_stats(stats, Frame());
				const void* record;
				ssize_t recordSize;
				if (msg->FindData("record", B_RAW_TYPE, &record, &recordSize) == B_OK
					&& fVideos->StartEncoding(std::string((const char*)record, recordSize)) > 0)
					fStatus->SetText((status + (status.empty() ? "" : " ")
						+ "The video is being made in Videos.").c_str());
				std::string text = error;
				if (text.empty() && engineError[0])
					text = std::string("The game engine stopped with an error:\n\n") + engineError;
				if (!text.empty())
					(new BAlert("Hedgewars", text.c_str(), "OK", NULL, NULL,
						B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go(NULL);
				break;
			}
			default:
				BWindow::MessageReceived(msg);
		}
	}

	static int32 game_thread(void* data)
	{
		Game* g = (Game*)data;
		Outcome o = run_game(*g);
		delete g;
		BMessage done(kMsgGameOver);
		done.AddString("error", o.error.c_str());
		done.AddString("engine error", o.engineError.c_str());
		done.AddString("saved", o.saved.c_str());
		done.AddBool("finished", o.finished);
		done.AddData("record", B_RAW_TYPE, o.record.data(), o.record.size());
		for (const std::string& s : o.stats)
			done.AddString("stat", s.c_str());
		sWindow.SendMessage(&done);
		return 0;
	}

private:
	TeamView* fTeamView;
	std::vector<std::string> fHats, fNames, fStyles, fMaps, fMissionIds, fCampaigns,
		fCampaignScripts, fRecordings;
	BTabView* fTabs;
	VideosView* fVideos;
	BStringView* fStatus;
	BMenuField *fOpponent, *fScheme, *fWeapons, *fStyle, *fMap, *fTheme;
	MapOptions fMapOptions;
	std::vector<GameScheme> fSchemeList;
	std::vector<WeaponSet> fWeaponList;
	BSpinner* fHogs;
	BMenuField* fMissionKind;
	BListView* fMissionList;
	BTextView* fMissionText;
	Thumbnail* fMissionThumb;
	BMenuField* fCampaign;
	BListView* fCampaignList;
	BTextView* fCampaignText;
	Thumbnail* fCampaignThumb;
	BListView* fRecordingList;
	BButton *fPlay, *fTrash;
	BButton* fDefault[5] = {};
	BButton* fEditDrawing;
};

class LauncherApp : public BApplication {
public:
	LauncherApp() : BApplication(kSignature) {}

	void ReadyToRun() override
	{
		srand(time(NULL));
		if (!find_data()) {
			(new BAlert("Hedgewars", "The Hedgewars game data was not found. Reinstall the Hedgewars package.", "Quit",
				NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go();
			Quit();
			return;
		}
		load_options();
		(new LauncherWindow)->Show();
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
