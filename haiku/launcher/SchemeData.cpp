// Game schemes and weapon sets, stored as the Qt frontend stores them
// (model/gameSchemeModel.cpp, ui/widget/selectWeapon.cpp).
#include "SchemeData.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <algorithm>

#include "schemes.h"	// generated from frontend-qt6/model/gameSchemeModel.cpp
#include "weapons.h"	// frontend-qt6

const char* const kSchemeKeys[45] = {"name", "switchhog", "divteams", "solidland", "border",
	"lowgrav", "laser", "invulnerability", "resethealth", "vampiric", "karma", "artillery",
	"randomorder", "king", "placehog", "sharedammo", "disablegirders", "disablelandobjects",
	"aisurvival", "infattack", "resetweps", "perhogammo", "disablewind", "morewind", "tagteam",
	"bottomborder", "damagefactor", "turntime", "health", "suddendeath", "caseprobability",
	"minestime", "minesnum", "minedudpct", "explosives", "airmines", "sentries",
	"healthprobability", "healthcaseamount", "waterrise", "healthdecrease", "ropepct",
	"getawaytime", "worldedge", "scriptparam"};

// the engine's TAmmoType order, with Locale/en.txt's names
const char* const kAmmoNames[kAmmoCount] = {"Grenade", "Cluster Bomb", "Bazooka", "Homing Bee",
	"Shotgun", "Pick Hammer", "Skip", "Rope", "Mine", "Desert Eagle", "Dynamite", "Shoryuken",
	"Whip", "Baseball Bat", "Parachute", "Air Attack", "Mine Strike", "Blow Torch",
	"Construction", "Teleportation", "Switch Hedgehog", "Mortar", "Kamikaze", "Cake",
	"Seduction", "Watermelon Bomb", "Hellish hand-grenade", "Napalm", "Drill Rocket", "Ballgun",
	"RC Plane", "Low Gravity", "Extra Damage", "Invulnerable", "Extra Time", "Laser Sight",
	"Vampirism", "Sniper Rifle", "Flying Saucer", "Molotov Cocktail", "Birdy",
	"Portable Portal Device", "Piano Strike", "Old Limburger", "Sine Gun", "Flamethrower",
	"Sticky Mine", "Hammer", "Resurrector", "Drill Strike", "Mudball", "Time Box", "Land Spray",
	"Freezer", "Cleaver", "Rubber", "Air Mine", "Creeper", "Minigun", "Sentry Bot"};

const int kAmmoMenu[kAmmoCount] = {3, 4, 22, 29, 51, 55, 1, 2, 26, 27, 40, 44, 5, 10, 38,
	45, 54, 59, 12, 13, 14, 23, 25, 48, 9, 11, 24, 30, 31, 47, 16, 17, 28, 43, 50, 57, 6, 18,
	19, 46, 53, 56, 8, 15, 20, 39, 41, 42, 34, 36, 37, 49, 52, 58, 7, 21, 32, 33, 35, 60};	// HW_AMMOMENU_ARRAY

const char* const kEmptyAmmo = AMMOLINE_EMPTY_QT AMMOLINE_EMPTY_PROB AMMOLINE_EMPTY_DELAY AMMOLINE_EMPTY_CRATE;

#define AMMO(n) AMMOLINE_##n##_QT AMMOLINE_##n##_PROB AMMOLINE_##n##_DELAY AMMOLINE_##n##_CRATE
static const struct { const char* name; const char* ammo; } kWeapons[] = {	// hwconsts.cpp
	{"Default", AMMO(DEFAULT)}, {"Crazy", AMMO(CRAZY)}, {"Pro Mode", AMMO(PROMODE)},
	{"Shoppa", AMMO(SHOPPA)}, {"Clean Slate", AMMO(CLEAN)}, {"Minefield", AMMO(MINES)},
	{"Thinking with Portals", AMMO(PORTALS)}, {"One of Everything", AMMO(ONEEVERY)},
	{"Highlander", AMMO(HIGHLANDER)}, {"Balanced Random Weapon", AMMO(BRW)},
	{"Construction Mode", AMMO(CONSTRUCTION)}, {"Shoppa Pro", AMMO(SHOPPAPRO)},
	{"HedgeEditor", AMMO(HEDGEEDITOR)},
};

std::vector<GameScheme> predefined_schemes()
{
	std::vector<GameScheme> out;
	for (const Scheme& s : kSchemes) {
		GameScheme g;
		g.name = s.name;
		std::copy(s.v, s.v + 44, g.v);
		g.scriptparam = s.scriptparam ? s.scriptparam : "";
		g.predefined = true;
		out.push_back(g);
	}
	return out;
}

std::vector<WeaponSet> predefined_weapons()
{
	std::vector<WeaponSet> out;
	for (const auto& w : kWeapons)
		out.push_back({w.name, w.ammo, true});
	return out;
}

// GameCFGWidget::getGameFlags: scheme fields 1..25
static const unsigned kFlagBits[26] = {0, 0x1000, 0x10, 0x4, 0x8, 0x20, 0x40, 0x80, 0x100,
	0x200, 0x400, 0x800, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000, 0x40000, 0x80000,
	0x100000, 0x200000, 0x400000, 0x800000, 0x1000000, 0x2000000, 0x4000000};
// GameCFGWidget::getFullConfig: scheme fields 26..43
static const struct { const char* cmd; int scale; } kNumbers[18] = {
	{"damagepct", 1}, {"turntime", 1000}, {"inithealth", 1}, {"sd_turns", 1},
	{"casefreq", 1}, {"minestime", 1000}, {"minesnum", 1}, {"minedudpct", 1},
	{"explosives", 1}, {"airmines", 1}, {"sentries", 1}, {"healthprob", 1},
	{"hcaseamount", 1}, {"waterrise", 1}, {"healthdec", 1}, {"ropepct", 1},
	{"getawaytime", 1}, {"worldedge", 1},
};

std::vector<std::string> scheme_commands(const GameScheme& s)
{
	unsigned flags = 0;
	for (int i = 1; i <= 25; i++)
		if (s.v[i])
			flags |= kFlagBits[i];
	std::vector<std::string> out = {"e$gmflags " + std::to_string(flags)};
	for (int i = 0; i < 18; i++)
		out.push_back(std::string("e$") + kNumbers[i].cmd + " " + std::to_string(s.v[26 + i] * kNumbers[i].scale));
	return out;
}

// QVariant(QString).toBool() / toInt()
static int to_int(const std::string& v)
{
	if (v == "true")
		return 1;
	return atoi(v.c_str());
}

GameScheme parse_scheme(const std::string& name, const std::string& text)
{
	GameScheme s = predefined_schemes()[0];	// keys a file lacks keep Default's value
	s.name = name;
	s.predefined = false;
	size_t at = 0;
	while (at < text.size()) {
		size_t end = text.find('\n', at);
		if (end == std::string::npos)
			end = text.size();
		std::string line = text.substr(at, end - at);
		at = end + 1;
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		std::string key = line.substr(0, eq), value = line.substr(eq + 1);
		if (key == "scriptparam")
			s.scriptparam = value;
		for (int k = 1; k < 44; k++)
			if (key == kSchemeKeys[k])
				s.v[k] = to_int(value);
	}
	return s;
}

// GameSchemeModel::Save: every key but the name, booleans as "true"/"false"
std::string format_scheme(const GameScheme& s)
{
	std::string out;
	for (int k = 1; k < 44; k++)
		out += std::string(kSchemeKeys[k]) + "="
			+ (k <= 25 ? (s.v[k] ? "true" : "false") : std::to_string(s.v[k])) + "\n";
	out += "scriptparam=" + s.scriptparam + "\n";
	return out;
}

std::string fix_weapon_set(const std::string& s)
{
	size_t have = s.size() / 4;
	std::string r;
	for (int part = 0; part < 4; part++) {
		std::string p = part < 3 ? s.substr(std::min(s.size(), have * part), have)
			: s.substr(s.size() - std::min(s.size(), have));	// Qt takes the last quarter with right()
		p.resize(kAmmoCount, '0');
		r += p;
	}
	return r;
}

static std::string read_file(const std::string& path)
{
	std::string out;
	FILE* f = fopen(path.c_str(), "r");
	char buf[4096];
	size_t n;
	while (f && (n = fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	if (f)
		fclose(f);
	return out;
}

static bool write_file(const std::string& path, const std::string& text)
{
	// the folder may not be there yet: Schemes/ and Schemes/Game/
	std::string dir = path.substr(0, path.rfind('/'));
	mkdir(dir.substr(0, dir.rfind('/')).c_str(), 0755);
	mkdir(dir.c_str(), 0755);
	std::string tmp = path + ".tmp";
	FILE* f = fopen(tmp.c_str(), "w");
	if (!f)
		return false;
	bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
	ok = fclose(f) == 0 && ok;
	return ok && rename(tmp.c_str(), path.c_str()) == 0;
}

// file names without the suffix, sorted
static std::vector<std::string> files(const std::string& dir, const std::string& suffix)
{
	std::vector<std::string> out;
	if (DIR* d = opendir(dir.c_str())) {
		while (dirent* e = readdir(d)) {
			std::string n = e->d_name;
			if (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0)
				out.push_back(n.substr(0, n.size() - suffix.size()));
		}
		closedir(d);
	}
	std::sort(out.begin(), out.end());
	return out;
}

bool same_name(const std::string& a, const std::string& b)
{
	return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
		[](char x, char y) { return tolower((unsigned char)x) == tolower((unsigned char)y); });
}

template<class T> static bool taken(const std::vector<T>& v, const std::string& name)
{
	return std::any_of(v.begin(), v.end(), [&](const T& t) { return same_name(t.name, name); });
}

std::vector<GameScheme> load_schemes(const std::string& dir)
{
	std::vector<GameScheme> out = predefined_schemes();
	for (const std::string& n : files(dir, ".hwg"))
		if (!taken(out, n))
			out.push_back(parse_scheme(n, read_file(dir + "/" + n + ".hwg")));
	return out;
}

std::vector<WeaponSet> load_weapons(const std::string& dir)
{
	std::vector<WeaponSet> out = predefined_weapons();
	for (const std::string& n : files(dir, ".hwa")) {
		if (taken(out, n))
			continue;
		std::string ammo = read_file(dir + "/" + n + ".hwa");
		ammo = ammo.substr(0, ammo.find_first_of(" \r\n"));	// QTextStream >> reads one word
		out.push_back({n, fix_weapon_set(ammo), false});
	}
	return out;
}

bool save_scheme(const std::string& dir, const GameScheme& s)
{
	return write_file(dir + "/" + s.name + ".hwg", format_scheme(s));
}

bool save_weapons(const std::string& dir, const WeaponSet& w)
{
	return write_file(dir + "/" + w.name + ".hwa", w.ammo + "\n");
}

std::string unique_name(const std::string& base, const std::vector<std::string>& names)
{
	auto used = [&](const std::string& n) {
		return std::any_of(names.begin(), names.end(), [&](const std::string& t) { return same_name(t, n); });
	};
	std::string n = base;
	for (int i = 2; used(n); i++)
		n = base + " (" + std::to_string(i) + ")";
	return n;
}

bool valid_name(const std::string& name)
{
	return !name.empty() && name.find_first_of(":/\\") == std::string::npos;
}
