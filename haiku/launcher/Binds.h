// Key bindings: frontend-qt6/binds.cpp's actions and defaults, and the key
// names the engine uses (Data/misc/keys.csv). The engine reads the bindings
// itself, from "[Binds]" in settings.ini (everyone's) and in Teams/<team>.hwt
// (one team's), so the launcher only writes those files.
// Portable: no Haiku headers, tested in tests/ini_test.cpp.
#pragma once

#include <map>
#include <string>
#include <vector>

struct BindAction {
	const char* action;		// the engine's command, the INI key
	const char* key;		// default key, an engine key name
	const char* label;
	const char* category;	// set on the first action of a group
};

extern const BindAction kBindActions[];
extern const int kBindActionCount;

typedef std::map<std::string, std::string> Binds;	// action -> key name; "" = default, "none" = no key

// keys.csv lines ("44,\"space\"") -> engine key name by SDL scancode
std::vector<std::string> parse_key_names(const std::vector<std::string>& csvLines);
// a Haiku raw key code (B_KEY_DOWN's "key") -> SDL scancode, as libhwhaiku maps it
int raw_key_to_scancode(int raw);
// "left_shift" -> "Left shift", "mousel" -> "Left mouse button"
std::string key_label(const std::string& key);

struct Ini;
Binds read_binds(const Ini& ini);
void write_binds(Ini& ini, const Binds& b, bool team);	// team: "" stays "" (the player's own key)
