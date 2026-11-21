// Network play: a window that connects to a Hedgewars server, lists its rooms,
// and plays net games in them (frontend-qt6/net/newnetclient.cpp and its pages).
#pragma once

#include "Game.h"

// team: the player's team; prefs: a template for the games (engine settings come from engine_args())
void open_net_window(const TeamInfo& team, const Game& prefs);
