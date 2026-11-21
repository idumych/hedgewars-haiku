// The generated maps' options, as frontend-qt6's HWMapContainer offers them:
// map size (template filter), maze style, feature size, and the seed.
#pragma once

#include <MenuField.h>
#include <Slider.h>
#include <TextControl.h>

#include <string>

struct MapOptions {
	BMenuField *size, *maze;
	BSlider* feature;
	BTextControl* seed;	// empty: a new random seed each game

	MapOptions();
	void Update(int mapgen);	// enables what this generator uses (-1: a fixed map)
	std::string Seed() const;
	// the "e$…" messages after the scheme's, as GameCFGWidget::getFullConfig sends them;
	// before is what goes before e$mapgen, after what follows e$scriptparam
	void Config(int mapgen, std::string& before, std::string& after) const;
};
