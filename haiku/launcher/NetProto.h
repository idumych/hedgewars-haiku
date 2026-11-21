// The Hedgewars server protocol (frontend-qt6/net/newnetclient.cpp) without the UI:
// message framing, base64 for the engine messages it carries, and the engine
// config a net game starts with (game.cpp commonConfig, gamecfgwidget.cpp
// getFullConfig). No Haiku headers, so it's tested on any system.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "SchemeData.h"

namespace net {

// "PART\nPART\n\n"
std::string message(const std::vector<std::string>& parts);
// takes the next complete message off the front of buf; false when there's none yet
bool next_message(std::string& buf, std::vector<std::string>& parts);

std::string base64_encode(const std::string& s);
std::string base64_decode(const std::string& s);

struct Team {
	std::string name, grave = "Simple", fort = "Plane", voice = "Default", flag = "hedgewars", owner;
	int difficulty = 0, color = 0, hogs = 4;	// color: an index into the clan colours
	std::string hogNames[8], hats[8];
	bool remote = true;	// played by another computer
};

// ADD_TEAM's fields after the name
std::vector<std::string> add_team(const Team& t);
// from ADD_TEAM's fields (without the command); false if malformed
bool parse_team(const std::vector<std::string>& f, Team& t);

// the room's settings, by CFG name ("MAP", "SCHEME", …)
typedef std::map<std::string, std::vector<std::string>> Config;
// stores a CFG message's values (FULLMAPCONFIG becomes its six parts)
void set_param(Config& c, const std::string& name, const std::vector<std::string>& values);
std::string param(const Config& c, const std::string& name, size_t i = 0);

// the CFG messages a room master sends for these choices, in the Qt order (map last);
// drawnMap: for a hand-drawn map (mapgen 3), the drawing as a .hwmap holds it;
// script: the game style ("Normal", or a Scripts/Multiplayer file's name with spaces for '_')
std::vector<std::vector<std::string>> master_config(const GameScheme& s, const WeaponSet& w,
	const std::string& map, int mapgen, const std::string& theme, const std::string& seed,
	int templateFilter, const std::string& drawnMap = "", const std::string& script = "Normal");

// everything the engine is sent when it asks for the config. theme: the one to
// use (a fixed map's own); the scheme's and weapons' values come from the room,
// or the predefined ones of that name
std::string engine_config(const Config& c, const std::vector<Team>& teams, const std::string& theme);

// a room's flags ("-gpjr") as words: "playing, password, joins restricted, registered only"
std::string room_flags_text(const std::string& flags);

// SERVER_VARS' name/value pairs (after the command)
std::map<std::string, std::string> server_vars(const std::vector<std::string>& f);

// BANLIST's entries (after the command): kind ("I" IP, "N" nickname), who, reason, time left
struct Ban { std::string kind, who, reason, left; };
std::vector<Ban> bans(const std::vector<std::string>& f);

// a chat line as the engine shows it (proto.cpp formatChatMsg)
std::string chat_line(const std::string& nick, const std::string& text);

}
