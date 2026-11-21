// The hand-drawn map window: frontend-qt6's PageDrawMap. The drawing is kept
// in drawn.hwmap and sent to the engine as "edraw" messages.
#pragma once

#include <string>

void show_draw_map();
bool drawn_map_empty();
std::string drawn_map_config();
std::string drawn_map_file();	// the drawing as a .hwmap holds it: DRAWNMAP in a net game	// "edraw" messages for the current drawing (GameCFGWidget::getFullConfig)
