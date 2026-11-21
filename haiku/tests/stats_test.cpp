// Host test for launcher/StatsData.cpp: the engine's stats messages.
#include "StatsData.h"

#include <assert.h>
#include <stdio.h>

int main()
{
	StatsData d = parse_stats({
		"rHaiku Hogs wins!",
		"H13707586 400", "H4816065 400", "H13707586 380", "H4816065 150",
		"P13707586 3 Haiku Hogs", "P13707586 1 Haiku Hogs 2", "P4816065 0 Computer",
		"D57 Milky Way", "K1", "B2 Benjamin",
		"p!TIME", "P255 4500 Timed team",
		"R7", "p!EMPTY", "P65280 1 Scripted",
	});
	assert(d.winner == "Haiku Hogs wins!");
	assert(d.health.size() == 2 && d.health[13707586].size() == 2 && d.health[4816065][1] == 150);
	assert(d.ranks.size() == 5);
	assert(d.ranks[0].position == 1 && d.ranks[0].team == "Haiku Hogs" && d.ranks[0].points == "(3 kills)");
	assert(d.ranks[1].position == 1 && d.ranks[1].points == "(1 kill)");	// same clan, same place
	assert(d.ranks[2].position == 2 && d.ranks[2].team == "Computer");
	assert(d.ranks[3].points == "(4.500 seconds)");
	assert(d.ranks[4].position == 7 && d.ranks[4].points.empty());	// a script's place, no points
	assert(d.awards.size() == 3);
	assert(d.awards[0].icon == "StatsBestShot"
		&& d.awards[0].text == "The best shot award was won by Milky Way with 57 points.");
	assert(d.awards[1].text == "A total of 1 hedgehog was killed during this round.");
	assert(d.awards[2].text == "Benjamin was scared and skipped 2 turns.");
	assert(parse_stats({}).ranks.empty());
	puts("stats_test: all passed");
	return 0;
}
