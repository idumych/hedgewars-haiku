// Game schemes and weapon sets: the predefined ones and the user's, in the Qt
// frontend's files (Schemes/Game/<name>.hwg, Schemes/Ammo/<name>.hwa).
// No Haiku headers, so it's tested on any system.
#pragma once

#include <string>
#include <vector>

// gameSchemeModel.cpp: v[1..25] flags, v[26..43] numbers (v[0] unused)
struct GameScheme {
	std::string name;
	int v[44] = {};
	std::string scriptparam;
	bool predefined = false;
};

struct WeaponSet {
	std::string name;
	std::string ammo;	// counts, probabilities, delays, crate amounts: 4 * kAmmoCount digits
	bool predefined = false;
};

const int kAmmoCount = 60;
extern const char* const kSchemeKeys[45];	// spNames: the .hwg keys
extern const char* const kAmmoNames[kAmmoCount];	// by ammo type - 1
extern const int kAmmoMenu[kAmmoCount];	// ammo types in the ammo menu's order
extern const char* const kEmptyAmmo;

std::vector<GameScheme> predefined_schemes();
std::vector<WeaponSet> predefined_weapons();

GameScheme parse_scheme(const std::string& name, const std::string& text);
std::string format_scheme(const GameScheme& s);
std::string fix_weapon_set(const std::string& s);	// SelWeaponWidget::fixWeaponSet
// the engine commands for a scheme's flags and numbers ("e$gmflags …", "e$turntime …")
std::vector<std::string> scheme_commands(const GameScheme& s);

// predefined first, then the user's files in dir (Schemes/Game or Schemes/Ammo),
// skipping files named like a predefined one
std::vector<GameScheme> load_schemes(const std::string& dir);
std::vector<WeaponSet> load_weapons(const std::string& dir);
bool save_scheme(const std::string& dir, const GameScheme& s);
bool save_weapons(const std::string& dir, const WeaponSet& w);

bool same_name(const std::string& a, const std::string& b);	// case-insensitive, as Qt compares
// "base", else "base (2)", "base (3)"… that no name in taken uses
std::string unique_name(const std::string& base, const std::vector<std::string>& taken);
bool valid_name(const std::string& name);	// a file name: not empty, no ':' '/' '\'
