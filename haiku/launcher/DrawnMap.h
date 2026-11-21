// Hand-drawn maps: frontend-qt6/drawmapscene.cpp's point format, which the
// engine reads from "edraw" messages, and the .hwmap files the Qt frontend
// saves (base64 of qCompress). Portable: no Haiku headers, tested in
// tests/ini_test.cpp.
#pragma once

#include <string>
#include <utility>
#include <vector>

// the map is 4096 x 2048; pen widths 16..516 in steps of 10
enum { kMapWidth = 4096, kMapHeight = 2048, kPenMin = 16, kPenMax = 516, kPenStart = 76 };

struct Stroke {
	int width = kPenStart;
	bool erasing = false;
	std::vector<std::pair<int, int>> points;
};

struct Drawing {
	std::string special;	// points that came before the first stroke (mission maps), kept as they were
	std::vector<Stroke> strokes;	// oldest first
};

std::string encode_drawing(const Drawing& d);
Drawing decode_drawing(const std::string& data);
// drops points closer than ~32 px to the previous one, as DrawMapScene::simplifyLast
void simplify(Stroke& s);
std::vector<std::pair<int, int>> rectangle_points(int x1, int y1, int x2, int y2);
std::vector<std::pair<int, int>> ellipse_points(int cx, int cy, int x2, int y2);

std::string to_hwmap(const std::string& data);
bool from_hwmap(const std::string& file, std::string& data);
