// The Settings window: frontend-qt6's PageOptions for a local player (display,
// sound, language, controls), and the engine arguments they make
// (HWGame::getArguments). Kept in settings.ini with the Qt frontend's keys,
// the file the engine reads its key bindings from.
#pragma once

#include "Binds.h"

#include <functional>
#include <string>
#include <vector>

class BView;

struct Options {
	bool fullscreen = false, sound = true, music = true, showFps = false, altDamage = false,
		tooltips = true;
	int width = 1024, height = 768, quality = 5, volume = 100;	// quality 0..5, volume 0..100
	std::string language;	// a Data/Locale file's code ("de", "pt_BR"); "" follows the system
};

extern Options gOptions;
extern Binds gBinds;	// everyone's key bindings; "" is the default key
extern std::vector<std::string> gKeyNames;	// engine key names by SDL scancode

void load_options();	// after find_data()
void save_options();
std::vector<std::string> engine_args();	// for run_game, after --user-prefix
std::string language_code();	// the Data/Locale file in use, "en" if none fits
// a line of Locale/<kind>_<language>.txt, else of the English file, else the fallback
std::string locale_value(const char* kind, const std::string& key, const std::string& fallback);
void show_settings();

enum { kMsgLanguageChanged = 'lang' };	// sent to every window when the language changes

// a list of the actions and their keys; a key is set by pressing it.
// team: the bindings are one team's, and "" means the player's own key
BView* new_bind_editor(Binds* binds, bool team, std::function<void()> changed);
