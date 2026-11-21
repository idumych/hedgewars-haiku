// The Settings window and the options it keeps (see Settings.h).
#include "Settings.h"
#include "Game.h"
#include "Ini.h"

#include <Application.h>
#include <Button.h>
#include <CheckBox.h>
#include <GroupView.h>
#include <Language.h>
#include <LayoutBuilder.h>
#include <LocaleRoster.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Screen.h>
#include <SeparatorItem.h>
#include <Slider.h>
#include <TabView.h>
#include <Window.h>

#include <algorithm>
#include <stdio.h>

Options gOptions;
Binds gBinds;
std::vector<std::string> gKeyNames;

static std::string ini_path() { return BPath(gUserDir.Path(), "settings.ini").Path(); }

static bool to_bool(const std::string& s, bool fallback)
{
	return s.empty() ? fallback : s == "true";
}

static int to_int(const std::string& s, int fallback)
{
	return s.empty() ? fallback : atoi(s.c_str());
}

void load_options()
{
	gKeyNames = parse_key_names(read_lines("misc/keys.csv"));
	Ini ini;
	ini.load(ini_path());
	Options d;
	gOptions.fullscreen = to_bool(ini.get("video", "fullscreen"), d.fullscreen);
	gOptions.width = to_int(ini.get("video", "windowedWidth"), d.width);
	gOptions.height = to_int(ini.get("video", "windowedHeight"), d.height);
	gOptions.quality = std::min(std::max(to_int(ini.get("video", "quality"), d.quality), 0), 5);
	gOptions.tooltips = to_bool(ini.get("misc", "weaponTooltips"), d.tooltips);
	gOptions.showFps = to_bool(ini.get("fps", "show"), d.showFps);
	gOptions.altDamage = to_bool(ini.get("misc", "altdamage"), d.altDamage);
	gOptions.sound = to_bool(ini.get("audio", "sound"), d.sound);
	gOptions.music = to_bool(ini.get("audio", "music"), d.music);
	gOptions.volume = std::min(std::max(to_int(ini.get("audio", "volume"), d.volume), 0), 100);
	gOptions.language = ini.get("misc", "locale");
	gBinds = read_binds(ini);
	// the Qt frontend writes every key; the defaults count as "default key"
	for (int i = 0; i < kBindActionCount; i++)
		if (gBinds[kBindActions[i].action] == kBindActions[i].key)
			gBinds[kBindActions[i].action] = "";
}

void save_options()
{
	Ini ini;
	ini.load(ini_path());	// keeps what the Qt frontend wrote there
	auto b = [](bool v) { return std::string(v ? "true" : "false"); };
	ini.set("video", "fullscreen", b(gOptions.fullscreen));
	ini.set("video", "windowedWidth", std::to_string(gOptions.width));
	ini.set("video", "windowedHeight", std::to_string(gOptions.height));
	ini.set("video", "quality", std::to_string(gOptions.quality));
	ini.set("misc", "weaponTooltips", b(gOptions.tooltips));
	ini.set("fps", "show", b(gOptions.showFps));
	ini.set("misc", "altdamage", b(gOptions.altDamage));
	ini.set("audio", "sound", b(gOptions.sound));
	ini.set("audio", "music", b(gOptions.music));
	ini.set("audio", "volume", std::to_string(gOptions.volume));
	ini.set("misc", "locale", gOptions.language);
	write_binds(ini, gBinds, false);
	if (!ini.save(ini_path()))
		fprintf(stderr, "Hedgewars: can't write %s\n", ini_path().c_str());
}

// the engine's translations: Locale/<code>.txt
static std::vector<std::string> languages()
{
	std::vector<std::string> out;
	for (const std::string& f : list_dir("Locale", ".txt", false))
		if (f.find('_') == std::string::npos || (f.size() == 5 && f[2] == '_'))
			if (f.find("missions") == std::string::npos && f.find("campaigns") == std::string::npos)
				out.push_back(f);
	return out;
}

static std::string matching_language(const std::string& code)
{
	std::vector<std::string> all = languages();
	for (const std::string& c : {code, code.substr(0, 2)})
		if (std::find(all.begin(), all.end(), c) != all.end())
			return c;
	return "";
}

std::string language_code()
{
	std::string code = matching_language(gOptions.language);
	if (code.empty() && gOptions.language.empty()) {
		BMessage langs;
		const char* l;
		BLocaleRoster::Default()->GetPreferredLanguages(&langs);
		for (int32 i = 0; code.empty() && langs.FindString("language", i, &l) == B_OK; i++) {
			std::string s = l;
			std::replace(s.begin(), s.end(), '-', '_');
			code = matching_language(s);
		}
	}
	return code.empty() ? "en" : code;
}

std::string locale_value(const char* kind, const std::string& key, const std::string& fallback)
{
	std::string en = std::string("Locale/") + kind + "_en.txt";
	std::string code = language_code();
	for (const std::string& c : {code, code.substr(0, 2)}) {
		std::string path = std::string("Locale/") + kind + "_" + c + ".txt";
		if (c != "en" && BEntry(BPath(gDataDir.Path(), path.c_str()).Path()).Exists())
			return ini_value(path.c_str(), key, ini_value(en.c_str(), key, fallback));
	}
	return ini_value(en.c_str(), key, fallback);
}

std::vector<std::string> engine_args()
{
	// GameUIConfig::translateQuality: each step down turns more effects off
	static const uint32 kQuality[6] = {0x002 | 0x040 | 0x100 | 0x010 | 0x020 | 0x080 | 0x008 | 0x800 | 0x004 | 0x200,
		0x002 | 0x040 | 0x100 | 0x010 | 0x020 | 0x080 | 0x008 | 0x800,
		0x002 | 0x040 | 0x100 | 0x010 | 0x020 | 0x080, 0x002 | 0x040 | 0x100, 0x002, 0};
	uint32 quality = kQuality[gOptions.quality] | (gOptions.tooltips ? 0 : 0x400);
	BRect screen = BScreen().Frame();
	std::vector<std::string> args = {"--locale", language_code() + ".txt",
		"--fullscreen-width", std::to_string((int)screen.Width() + 1),
		"--fullscreen-height", std::to_string((int)screen.Height() + 1),
		"--width", std::to_string(gOptions.width), "--height", std::to_string(gOptions.height),
		"--volume", std::to_string(gOptions.volume), "--raw-quality", std::to_string(quality)};
	if (gOptions.fullscreen) args.push_back("--fullscreen");
	if (gOptions.showFps) args.push_back("--showfps");
	if (gOptions.altDamage) args.push_back("--altdmg");
	if (!gOptions.sound) args.push_back("--nosound");
	if (!gOptions.music) args.push_back("--nomusic");
	return args;
}

// ── window ──
enum { kMsgChanged = 'chgd', kMsgDefaults = 'dflt', kMsgRevert = 'rvrt' };

static const int kSizes[][2] = {{640, 480}, {800, 600}, {1024, 768}, {1280, 720}, {1280, 800},
	{1280, 1024}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}};

static BWindow* sSettings;

class SettingsWindow : public BWindow {
public:
	SettingsWindow()
		: BWindow(BRect(0, 0, 400, 300), "Settings", B_TITLED_WINDOW,
			B_NOT_ZOOMABLE | B_AUTO_UPDATE_SIZE_LIMITS),
		  fInitial(gOptions), fInitialBinds(gBinds)
	{
		BPopUpMenu* menu = new BPopUpMenu("");
		menu->AddItem(new BMenuItem("System default", new BMessage(kMsgChanged)));
		menu->AddItem(new BSeparatorItem);
		for (const std::string& code : languages()) {
			BString name;
			BLanguage lang(code.c_str());
			if (lang.GetNativeName(name) != B_OK || name.IsEmpty())
				name = code.c_str();
			name.SetByteAt(0, toupper(name.ByteAt(0)));	// "português" -> "Português"
			BMessage* m = new BMessage(kMsgChanged);
			m->AddString("code", code.c_str());
			menu->AddItem(new BMenuItem(name, m));
			fCodes.push_back(code);
		}
		fLanguage = new BMenuField("Language:", menu);

		fFullscreen = new BCheckBox("Full screen", new BMessage(kMsgChanged));
		menu = new BPopUpMenu("");
		BRect screen = BScreen().Frame();
		for (const auto& s : kSizes)
			if (s[0] <= screen.Width() + 1 && s[1] <= screen.Height() + 1)
				menu->AddItem(new BMenuItem((std::to_string(s[0]) + " × " + std::to_string(s[1])).c_str(),
					new BMessage(kMsgChanged)));
		fSize = new BMenuField("Window size:", menu);
		fQuality = new BSlider("quality", "Quality:", new BMessage(kMsgChanged), 0, 5, B_HORIZONTAL);
		fQuality->SetHashMarks(B_HASH_MARKS_BOTTOM);
		fQuality->SetHashMarkCount(6);
		fQuality->SetLimitLabels("Fast", "Best");
		fTooltips = new BCheckBox("Weapon descriptions in the weapon menu", new BMessage(kMsgChanged));
		fFps = new BCheckBox("Show the frame rate", new BMessage(kMsgChanged));
		fAltDamage = new BCheckBox("Alternative damage display", new BMessage(kMsgChanged));

		fSound = new BCheckBox("Sound effects", new BMessage(kMsgChanged));
		fMusic = new BCheckBox("Music", new BMessage(kMsgChanged));
		fVolume = new BSlider("volume", "Volume:", new BMessage(kMsgChanged), 0, 100, B_HORIZONTAL);
		fVolume->SetLimitLabels("Silent", "Loud");
		fVolume->SetModificationMessage(new BMessage(kMsgChanged));

		BGroupView* game = new BGroupView("Game", B_VERTICAL);
		BLayoutBuilder::Group<>(game)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.AddGrid(B_USE_DEFAULT_SPACING, B_USE_SMALL_SPACING)
				.AddMenuField(fLanguage, 0, 0)
				.AddMenuField(fSize, 0, 1)
				.Add(fFullscreen, 1, 2)
			.End()
			.Add(fQuality)
			.Add(fTooltips)
			.Add(fFps)
			.Add(fAltDamage)
			.AddGlue();
		BGroupView* sound = new BGroupView("Sound", B_VERTICAL);
		BLayoutBuilder::Group<>(sound)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(fSound)
			.Add(fMusic)
			.Add(fVolume)
			.AddGlue();
		fControls = new_bind_editor(&gBinds, false, [] { save_options(); });
		BGroupView* controls = new BGroupView("Controls", B_VERTICAL);
		BLayoutBuilder::Group<>(controls)
			.SetInsets(B_USE_DEFAULT_SPACING)
			.Add(fControls);

		BTabView* tabs = new BTabView("tabs", B_WIDTH_FROM_LABEL);
		tabs->AddTab(game);
		tabs->AddTab(sound);
		tabs->AddTab(controls);
		tabs->SetBorder(B_NO_BORDER);
		BLayoutBuilder::Group<>(this, B_VERTICAL)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(tabs)
			.AddGroup(B_HORIZONTAL)
				.Add(new BButton("Defaults", new BMessage(kMsgDefaults)))
				.Add(new BButton("Revert", new BMessage(kMsgRevert)))
				.AddGlue()
			.End();
		ToControls();

		Ini ini;
		ini.load(ini_path());
		BRect frame;
		if (sscanf(ini.get("haiku", "settingsWindow").c_str(), "%f %f %f %f",
				&frame.left, &frame.top, &frame.right, &frame.bottom) == 4) {
			MoveTo(frame.LeftTop());
			ResizeTo(frame.Width(), frame.Height());
			MoveOnScreen();
		} else
			CenterOnScreen();
	}

	void ToControls()
	{
		auto it = std::find(fCodes.begin(), fCodes.end(), gOptions.language);
		if (BMenuItem* item = fLanguage->Menu()->ItemAt(it == fCodes.end() ? 0 : it - fCodes.begin() + 2))
			item->SetMarked(true);
		std::string size = std::to_string(gOptions.width) + " × " + std::to_string(gOptions.height);
		if (!fSize->Menu()->FindItem(size.c_str()))
			fSize->Menu()->AddItem(new BMenuItem(size.c_str(), new BMessage(kMsgChanged)));
		fSize->Menu()->FindItem(size.c_str())->SetMarked(true);
		fFullscreen->SetValue(gOptions.fullscreen);
		fQuality->SetValue(gOptions.quality);
		fTooltips->SetValue(gOptions.tooltips);
		fFps->SetValue(gOptions.showFps);
		fAltDamage->SetValue(gOptions.altDamage);
		fSound->SetValue(gOptions.sound);
		fMusic->SetValue(gOptions.music);
		fVolume->SetValue(gOptions.volume);
		fVolume->SetEnabled(gOptions.sound || gOptions.music);
		fControls->FindView("bindings")->Invalidate();
	}

	void FromControls()
	{
		std::string old = language_code();
		BMenuItem* lang = fLanguage->Menu()->FindMarked();
		const char* code = NULL;
		gOptions.language = lang && lang->Message() && lang->Message()->FindString("code", &code) == B_OK ? code : "";
		if (BMenuItem* size = fSize->Menu()->FindMarked())
			sscanf(size->Label(), "%d × %d", &gOptions.width, &gOptions.height);
		gOptions.fullscreen = fFullscreen->Value();
		gOptions.quality = fQuality->Value();
		gOptions.tooltips = fTooltips->Value();
		gOptions.showFps = fFps->Value();
		gOptions.altDamage = fAltDamage->Value();
		gOptions.sound = fSound->Value();
		gOptions.music = fMusic->Value();
		gOptions.volume = fVolume->Value();
		fVolume->SetEnabled(gOptions.sound || gOptions.music);
		if (language_code() != old)
			for (int32 i = 0; BWindow* w = be_app->WindowAt(i); i++)
				w->PostMessage(kMsgLanguageChanged);
	}

	void Changed(const Options& o, const Binds& b)
	{
		std::string old = language_code();
		gOptions = o;
		gBinds = b;
		ToControls();
		save_options();
		if (language_code() != old)
			for (int32 i = 0; BWindow* w = be_app->WindowAt(i); i++)
				w->PostMessage(kMsgLanguageChanged);
	}

	void MessageReceived(BMessage* msg) override
	{
		switch (msg->what) {
			case kMsgChanged:
				FromControls();
				save_options();
				break;
			case kMsgDefaults: Changed(Options(), Binds()); break;
			case kMsgRevert: Changed(fInitial, fInitialBinds); break;
			default: BWindow::MessageReceived(msg);
		}
	}

	bool QuitRequested() override
	{
		Ini ini;
		ini.load(ini_path());
		BRect f = Frame();
		char buf[64];
		snprintf(buf, sizeof(buf), "%g %g %g %g", f.left, f.top, f.right, f.bottom);
		ini.set("haiku", "settingsWindow", buf);
		ini.save(ini_path());
		sSettings = NULL;
		return true;
	}

private:
	Options fInitial;
	Binds fInitialBinds;
	std::vector<std::string> fCodes;
	BMenuField *fLanguage, *fSize;
	BCheckBox *fFullscreen, *fTooltips, *fFps, *fAltDamage, *fSound, *fMusic;
	BSlider *fQuality, *fVolume;
	BView* fControls;
};

void show_settings()
{
	if (!sSettings)
		sSettings = new SettingsWindow;
	if (sSettings->Lock()) {
		if (sSettings->IsHidden())
			sSettings->Show();
		sSettings->Activate();
		sSettings->Unlock();
	}
}
