// Parsing the engine's statistics messages; no Haiku headers, so it's tested on any system.
#pragma once

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

struct StatsData {
	struct Rank { int position; uint32_t color; std::string team, points; };
	struct Award { std::string icon, text; };	// icon: a Stats*.png resource name
	std::string winner;
	std::string graphTitle = "Health graph";
	std::vector<Rank> ranks;
	std::vector<Award> awards;
	std::map<uint32_t, std::vector<int>> health;	// clan color -> one value per turn
};

// lines: the stat's type character followed by its text, as the engine sends them
StatsData parse_stats(const std::vector<std::string>& lines);
