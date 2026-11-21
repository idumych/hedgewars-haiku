// frontend-qt6/ui/page/pagegamestats.cpp, GameStats(), without the HTML
#include "StatsData.h"

#include <stdio.h>
#include <stdlib.h>

static std::string plural(long n, const char* one, const char* many)
{
	return std::to_string(n) + " " + (n == 1 ? one : many);
}

// "123 Some name": the number and the rest
static void split(const std::string& info, long* num, std::string* rest)
{
	size_t sp = info.find(' ');
	*num = atol(info.substr(0, sp).c_str());
	*rest = sp == std::string::npos ? "" : info.substr(sp + 1);
}

static std::string seconds(long ms, int decimals)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%.*f", decimals, ms / 1000.0);
	return std::string(buf) + (ms == 1000 ? " second" : " seconds");
}

// what follows a team's name in the ranking; kind comes from the 'p' message
static std::string points_text(const std::string& kind, long n)
{
	if (kind.empty())
		return "(" + plural(n, "kill", "kills") + ")";
	if (kind == "!POINTS")
		return "(" + plural(n, "point", "points") + ")";
	if (kind == "!TIME")
		return "(" + seconds(n, 3) + ")";
	if (kind.size() == 6 && kind.compare(0, 5, "!TIME") == 0 && kind[5] >= '0' && kind[5] <= '9')
		return "(" + seconds(n, kind[5] - '0') + ")";
	if (kind == "!CRATES")
		return "(" + plural(n, "crate", "crates") + ")";
	if (kind == "!EMPTY")
		return "";
	return "(" + std::to_string(n) + " " + kind + ")";
}

StatsData parse_stats(const std::vector<std::string>& lines)
{
	StatsData d;
	std::string kind;		// kind of points for the next ranking line
	int position = 0, scriptPosition = 0;
	uint32_t lastColor = 0xffffffff;
	for (const std::string& line : lines) {
		if (line.empty())
			continue;
		std::string info = line.substr(1), rest;
		long n;
		switch (line[0]) {
			case 'r': d.winner = info; break;
			case 'g': d.graphTitle = info; break;
			case 'p': kind = info; break;
			case 'R': scriptPosition = atoi(info.c_str()); break;
			case 'H':
				split(info, &n, &rest);
				d.health[(uint32_t)n].push_back(atoi(rest.c_str()));
				break;
			case 'P': {
				split(info, &n, &rest);
				uint32_t color = (uint32_t)n;
				long kills;
				std::string team;
				split(rest, &kills, &team);
				position++;
				if (color == lastColor)	// teams of one clan share a place
					position--;
				lastColor = color;
				d.ranks.push_back({scriptPosition ? scriptPosition : position, color, team,
					points_text(kind, kills)});
				kind.clear();
				scriptPosition = 0;
				break;
			}
			case 'D':
				split(info, &n, &rest);
				d.awards.push_back({"StatsBestShot", "The best shot award was won by " + rest
					+ " with " + plural(n, "point", "points") + "."});
				break;
			case 'k':
				split(info, &n, &rest);
				d.awards.push_back({"StatsBestKiller", "The best killer is " + rest + " with "
					+ plural(n, "kill", "kills") + " in a turn."});
				break;
			case 'K':
				n = atol(info.c_str());
				d.awards.push_back({"StatsHedgehogsKilled", "A total of "
					+ plural(n, "hedgehog was", "hedgehogs were") + " killed during this round."});
				break;
			case 's':
				split(info, &n, &rest);
				d.awards.push_back({"StatsMostSelfDamage", rest + " thought it was good to shoot "
					"their own hedgehogs for " + plural(n, "point", "points") + "."});
				break;
			case 'S':
				split(info, &n, &rest);
				d.awards.push_back({"StatsSelfKilled", rest + " killed "
					+ std::to_string(n) + " of their own hedgehogs."});
				break;
			case 'B':
				split(info, &n, &rest);
				d.awards.push_back({"StatsSkipped", rest + " was scared and skipped "
					+ plural(n, "turn", "turns") + "."});
				break;
			case 'c': d.awards.push_back({"StatsCustomAchievement", info}); break;
			case 'h':
				d.awards.push_back({"StatsEverAfter", "With everyone having the same clan color, "
					"there was no reason to fight. And so the hedgehogs happily lived in peace ever after."});
				break;
		}
	}
	return d;
}
