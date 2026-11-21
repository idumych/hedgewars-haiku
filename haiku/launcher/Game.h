// The launcher's side of the engine: game data lookup, the settings it sends,
// running one game over IPC, recordings and mission/campaign progress.
#pragma once

#include <Message.h>
#include <Path.h>
#include <SupportDefs.h>

#include <string>
#include <vector>

extern BPath gAppDir, gDataDir, gUserDir;

bool find_data();
// names in a data folder ("Themes"), or in the user's folder with user=true
std::vector<std::string> list_dir(const char* sub, const char* suffix, bool dirs, bool user = false);
std::vector<std::string> read_lines(const char* dataPath);
// "key=value" files (Locale/missions_en.txt) and INI files with "Section/key"
std::string ini_value(const char* dataPath, const std::string& key, const std::string& fallback = "");

struct TeamInfo {
	std::string name = "Haiku Hogs", flag = "hedgewars", grave = "Simple",
		fort = "Plane", voice = "Default";
	std::string hogs[8], hats[8];
	int difficulty = 0;	// 0 human, 1 (very hard) .. 5 (very easy)
	uint32 color = 0xd12b42;
};

void add(std::string& buf, const std::string& s);	// one length-prefixed message
std::string new_seed();
std::string team_config(const TeamInfo& t, int hogs, int health, bool mission);

struct Game {
	std::string config;		// sent when the engine asks for it
	std::string replay;		// a recording, sent as soon as the engine connects
	std::string varPrefix;	// "Mission X/" or "Campaign Y/": where script variables live
	bool fullscreen = false, sound = true, music = true;
};

struct Outcome {
	std::string error;		// "" when the game ran
	std::string engineError;	// the engine's last fatal message
	std::string saved;		// the demo or saved game written, if any
};

Outcome run_game(const Game& g);

// writes a copy beside the file and renames it over, so a crash never leaves half a file
bool save_message(const BMessage& m, const BPath& path);

std::string get_progress(const std::string& key);
void set_progress(const std::string& key, const std::string& value);
