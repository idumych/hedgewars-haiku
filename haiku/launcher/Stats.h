// The statistics window after a finished game.
#pragma once

#include <Rect.h>

#include <string>
#include <vector>

// lines: the engine's stats messages, each its type character and text
void show_stats(const std::vector<std::string>& lines, BRect over);
