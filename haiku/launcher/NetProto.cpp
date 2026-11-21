#include "NetProto.h"

#include <stdlib.h>
#include <string.h>

#include "schemes.h"	// generated from frontend-qt6/model/gameSchemeModel.cpp
#include "weapons.h"	// frontend-qt6

namespace net {

// shortcut: these three tables repeat Launcher.cpp's; share them once the launcher has a config header
#define AMMO(n) AMMOLINE_##n##_QT AMMOLINE_##n##_PROB AMMOLINE_##n##_DELAY AMMOLINE_##n##_CRATE
static const struct { const char* name; const char* ammo; } kWeapons[] = {	// hwconsts.cpp
	{"Default", AMMO(DEFAULT)}, {"Crazy", AMMO(CRAZY)}, {"Pro Mode", AMMO(PROMODE)},
	{"Shoppa", AMMO(SHOPPA)}, {"Clean Slate", AMMO(CLEAN)}, {"Minefield", AMMO(MINES)},
	{"Thinking with Portals", AMMO(PORTALS)}, {"One of Everything", AMMO(ONEEVERY)},
	{"Highlander", AMMO(HIGHLANDER)}, {"Balanced Random Weapon", AMMO(BRW)},
	{"Construction Mode", AMMO(CONSTRUCTION)}, {"Shoppa Pro", AMMO(SHOPPAPRO)},
	{"HedgeEditor", AMMO(HEDGEEDITOR)},
};
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
// hwconsts.h HW_TEAMCOLOR_ARRAY
static const unsigned kColors[] = {0xff0204, 0x4980c1, 0x1de6ba, 0xb541ef, 0xe55bb0,
	0x20bf00, 0xfe8b0e, 0x8f5902, 0xffff01};

std::string message(const std::vector<std::string>& parts)
{
	std::string m;
	for (const std::string& p : parts)
		m += p + "\n";
	return m + "\n";
}

bool next_message(std::string& buf, std::vector<std::string>& parts)
{
	size_t end = buf.find("\n\n");
	if (end == std::string::npos)
		return false;
	parts.clear();
	size_t at = 0;
	while (at < end) {
		size_t nl = buf.find('\n', at);
		parts.push_back(buf.substr(at, nl - at));
		at = nl + 1;
	}
	buf.erase(0, end + 2);
	return true;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64_encode(const std::string& s)
{
	std::string out;
	for (size_t i = 0; i < s.size(); i += 3) {
		unsigned v = (unsigned char)s[i] << 16;
		if (i + 1 < s.size()) v |= (unsigned char)s[i + 1] << 8;
		if (i + 2 < s.size()) v |= (unsigned char)s[i + 2];
		out += kB64[v >> 18];
		out += kB64[(v >> 12) & 63];
		out += i + 1 < s.size() ? kB64[(v >> 6) & 63] : '=';
		out += i + 2 < s.size() ? kB64[v & 63] : '=';
	}
	return out;
}

std::string base64_decode(const std::string& s)
{
	std::string out;
	unsigned v = 0;
	int bits = 0;
	for (char c : s) {
		const char* p = c ? strchr(kB64, c) : NULL;
		if (!p)
			continue;	// '=' and anything else
		v = (v << 6) | (p - kB64);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out += (char)((v >> bits) & 0xff);
		}
	}
	return out;
}

std::vector<std::string> add_team(const Team& t)
{
	std::vector<std::string> f = {t.name, std::to_string(t.color), t.grave, t.fort, t.voice,
		t.flag, std::to_string(t.difficulty)};
	for (int h = 0; h < 8; h++) {
		f.push_back(t.hogNames[h].empty() ? "Hog " + std::to_string(h + 1) : t.hogNames[h]);
		f.push_back(t.hats[h].empty() ? "NoHat" : t.hats[h]);
	}
	return f;
}

// HWTeam(const QStringList&): name grave fort voice flag owner difficulty, then 8 name/hat pairs
bool parse_team(const std::vector<std::string>& f, Team& t)
{
	if (f.size() != 23)
		return false;
	t = Team();
	t.name = f[0];
	t.grave = f[1];
	t.fort = f[2];
	t.voice = f[3];
	t.flag = f[4];
	t.owner = f[5];
	t.difficulty = atoi(f[6].c_str());
	for (int h = 0; h < 8; h++) {
		t.hogNames[h] = f[7 + h * 2];
		t.hats[h] = f[8 + h * 2].empty() ? "NoHat" : f[8 + h * 2];
	}
	return true;
}

void set_param(Config& c, const std::string& name, const std::vector<std::string>& v)
{
	if (name == "FULLMAPCONFIG" && v.size() == 6) {
		const char* keys[] = {"FEATURE_SIZE", "MAP", "MAPGEN", "MAZE_SIZE", "SEED", "TEMPLATE"};
		for (int i = 0; i < 6; i++)
			c[keys[i]] = {v[i]};
	} else
		c[name] = v;
}

std::string param(const Config& c, const std::string& name, size_t i)
{
	auto it = c.find(name);
	return it != c.end() && i < it->second.size() ? it->second[i] : "";
}

std::vector<std::vector<std::string>> master_config(int scheme, int weapons, const std::string& map,
	int mapgen, const std::string& theme, const std::string& seed, int templateFilter)
{
	const Scheme& s = kSchemes[scheme];
	std::vector<std::string> sv = {"SCHEME", s.name};
	for (int i = 1; i < 44; i++)
		sv.push_back(i <= 25 ? (s.v[i] ? "true" : "false") : std::to_string(s.v[i]));
	sv.push_back(std::string("!") + (s.scriptparam ? s.scriptparam : ""));
	return {
		{"AMMO", kWeapons[weapons].name, kWeapons[weapons].ammo},
		{"SEED", seed},
		{"TEMPLATE", std::to_string(templateFilter)},
		{"THEME", theme},
		sv,
		{"SCRIPT", "Normal"},	// shortcut: no game styles in net games yet
		{"MAPGEN", std::to_string(mapgen)},
		{"MAZE_SIZE", "0"},
		{"FEATURE_SIZE", "12"},
		{"MAP", map},
	};
}

static void add(std::string& buf, const std::string& s)
{
	std::string m = s.substr(0, 250);	// the protocol's limit
	buf += (char)(unsigned char)m.size();
	buf += m;
}

// the room's scheme as schemes.h values, or the built-in scheme of its name
static void room_scheme(const Config& c, int v[44], std::string& scriptparam)
{
	auto it = c.find("SCHEME");
	if (it != c.end() && it->second.size() == 45) {
		const std::vector<std::string>& sv = it->second;
		for (int i = 1; i < 44; i++)
			v[i] = sv[i] == "true" ? 1 : sv[i] == "false" ? 0 : atoi(sv[i].c_str());
		scriptparam = sv[44].substr(sv[44].empty() ? 0 : 1);	// after its '!'
		return;
	}
	const Scheme* s = &kSchemes[0];
	for (const Scheme& k : kSchemes)
		if (param(c, "SCHEME") == k.name)
			s = &k;
	memcpy(v, s->v, sizeof(s->v));
	scriptparam = s->scriptparam ? s->scriptparam : "";
}

std::string engine_config(const Config& c, const std::vector<Team>& teams, const std::string& theme)
{
	std::string buf;
	add(buf, "TN");
	std::string script = param(c, "SCRIPT");
	if (!script.empty() && script != "Normal") {
		for (char& ch : script)
			if (ch == ' ')
				ch = '_';
		add(buf, "escript Scripts/Multiplayer/" + script + ".lua");
	}
	std::string map = param(c, "MAP");
	if (!map.empty() && map[0] != '+')
		add(buf, "emap " + map);
	add(buf, "etheme " + theme);
	add(buf, "eseed " + param(c, "SEED"));
	int v[44] = {};
	std::string scriptparam;
	room_scheme(c, v, scriptparam);
	unsigned flags = 0;
	for (int i = 1; i <= 25; i++)
		if (v[i])
			flags |= kFlagBits[i];
	add(buf, "e$gmflags " + std::to_string(flags));
	for (int i = 0; i < 18; i++)
		add(buf, std::string("e$") + kNumbers[i].cmd + " " + std::to_string(v[26 + i] * kNumbers[i].scale));
	int mapgen = atoi(param(c, "MAPGEN").c_str());
	add(buf, "e$template_filter " + std::to_string(atoi(param(c, "TEMPLATE").c_str())));
	add(buf, "e$feature_size " + std::to_string(atoi(param(c, "FEATURE_SIZE").c_str())));
	add(buf, "e$mapgen " + std::to_string(mapgen));
	add(buf, "e$scriptparam " + scriptparam);
	if (mapgen == 1 || mapgen == 2)	// maze, perlin
		add(buf, "e$maze_size " + std::to_string(atoi(param(c, "MAZE_SIZE").c_str())));
	// shortcut: hand-drawn maps (mapgen 3, DRAWNMAP: zlib) aren't sent to the engine

	std::string ammo = param(c, "AMMO", 1);
	if (ammo.empty()) {
		ammo = kWeapons[0].ammo;
		for (const auto& w : kWeapons)
			if (param(c, "AMMO") == w.name)
				ammo = w.ammo;
	}
	size_t n = ammo.size() / 4;
	for (const Team& t : teams) {
		add(buf, "eammloadt " + ammo.substr(0, n));
		add(buf, "eammprob " + ammo.substr(n, n));
		add(buf, "eammdelay " + ammo.substr(2 * n, n));
		add(buf, "eammreinf " + ammo.substr(3 * n, n));
		if (v[15] || !v[21])	// shared ammo, or not per-hog ammo
			add(buf, "eammstore");
		// HWTeam::teamGameConfig. The player hash only matters for reserved hats.
		unsigned color = kColors[t.color >= 0 && t.color < 9 ? t.color : 0];
		add(buf, "eaddteam 0 " + std::to_string(color) + " " + t.name);
		if (t.remote)
			add(buf, "erdriven");
		add(buf, "egrave " + t.grave);
		add(buf, "efort " + t.fort);
		add(buf, "evoicepack " + t.voice);
		add(buf, "eflag " + t.flag);
		if (!t.owner.empty())
			add(buf, "eowner " + t.owner);
		for (int h = 0; h < t.hogs && h < 8; h++) {
			add(buf, "eaddhh " + std::to_string(t.difficulty) + " " + std::to_string(v[28]) + " " + t.hogNames[h]);
			add(buf, "ehat " + t.hats[h]);
		}
	}
	return buf;
}

std::vector<std::string> scheme_names()
{
	std::vector<std::string> out;
	for (const Scheme& s : kSchemes)
		out.push_back(s.name);
	return out;
}

std::vector<std::string> weapon_names()
{
	std::vector<std::string> out;
	for (const auto& w : kWeapons)
		out.push_back(w.name);
	return out;
}

std::string chat_line(const std::string& nick, const std::string& text)
{
	if (text.compare(0, 4, "/me ") == 0 && !nick.empty() && nick[0] != '[' && nick[0] != '(')
		return "\x02* " + nick + " " + text.substr(4);
	return "\x01" + nick + ": " + text;
}

}
