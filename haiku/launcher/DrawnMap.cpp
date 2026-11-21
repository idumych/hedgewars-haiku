#include "DrawnMap.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int pen_code(int width) { return (width - 6) / 10; }	// serializePenWidth
static int pen_width(int code) { return code * 10 + 6; }

std::string encode_drawing(const Drawing& d)
{
	std::string b = d.special;
	for (const Stroke& s : d.strokes) {
		bool first = true;
		for (const auto& p : s.points) {
			b += (char)((p.first >> 8) & 255);
			b += (char)(p.first & 255);
			b += (char)((p.second >> 8) & 255);
			b += (char)(p.second & 255);
			b += (char)(first ? 0x80 | pen_code(s.width) | (s.erasing ? 0x40 : 0) : 0);
			first = false;
		}
	}
	return b;
}

Drawing decode_drawing(const std::string& data)
{
	Drawing d;
	for (size_t i = 0; i + 5 <= data.size(); i += 5) {
		const unsigned char* r = (const unsigned char*)data.data() + i;
		int x = (int16_t)(r[0] << 8 | r[1]), y = (int16_t)(r[2] << 8 | r[3]);
		if (r[4] & 0x80) {
			d.strokes.push_back(Stroke());
			d.strokes.back().width = pen_width(r[4] & 0x3f);
			d.strokes.back().erasing = r[4] & 0x40;
		} else if (d.strokes.empty()) {
			d.special.append(data, i, 5);
			continue;
		}
		d.strokes.back().points.push_back({x, y});
	}
	return d;
}

void simplify(Stroke& s)
{
	auto& pts = s.points;
	if (pts.empty())
		return;
	auto prev = pts[0];
	size_t i = 1;
	while (i < pts.size()) {
		int dx = prev.first - pts[i].first, dy = prev.second - pts[i].second;
		if ((i != pts.size() - 1 || prev == pts[i]) && dx * dx + dy * dy < 1000)
			pts.erase(pts.begin() + i);
		else
			prev = pts[i++];
	}
}

std::vector<std::pair<int, int>> rectangle_points(int x1, int y1, int x2, int y2)
{
	return {{x1, y1}, {x1, y2}, {x2, y2}, {x2, y1}, {x1, y1}};
}

std::vector<std::pair<int, int>> ellipse_points(int cx, int cy, int x2, int y2)
{
	std::vector<std::pair<int, int>> l;
	double rx = fabs((double)cx - x2), ry = fabs((double)cy - y2), r = fmax(rx, ry);
	if (r < 4)
		return {{cx, cy}};
	double step = fmax(0.1, fmin(0.7, 120 / r));
	for (double a = 0; a < 2 * M_PI; a += step)
		l.push_back({(int)lround(cx + rx * cos(a)), (int)lround(cy + ry * sin(a))});
	l.push_back(l.front());
	return l;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// qCompress: the uncompressed size (big-endian), then a zlib stream
std::string to_hwmap(const std::string& data)
{
	uLongf len = compressBound(data.size());
	std::string z(4 + len, '\0');
	z[0] = data.size() >> 24; z[1] = data.size() >> 16; z[2] = data.size() >> 8; z[3] = data.size();
	compress2((Bytef*)&z[4], &len, (const Bytef*)data.data(), data.size(), Z_DEFAULT_COMPRESSION);
	z.resize(4 + len);
	std::string out;
	for (size_t i = 0; i < z.size(); i += 3) {
		uint32_t v = (unsigned char)z[i] << 16;
		if (i + 1 < z.size()) v |= (unsigned char)z[i + 1] << 8;
		if (i + 2 < z.size()) v |= (unsigned char)z[i + 2];
		out += kB64[v >> 18];
		out += kB64[(v >> 12) & 63];
		out += i + 1 < z.size() ? kB64[(v >> 6) & 63] : '=';
		out += i + 2 < z.size() ? kB64[v & 63] : '=';
	}
	return out;
}

bool from_hwmap(const std::string& file, std::string& data)
{
	std::string z;
	uint32_t v = 0;
	int bits = 0;
	for (char c : file) {
		const char* p = c ? strchr(kB64, c) : NULL;
		if (!p)
			continue;	// padding, line breaks
		v = v << 6 | (p - kB64);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			z += (char)((v >> bits) & 255);
		}
	}
	if (z.size() < 4)
		return false;
	uLongf len = (unsigned char)z[0] << 24 | (unsigned char)z[1] << 16 | (unsigned char)z[2] << 8 | (unsigned char)z[3];
	if (len > (64u << 20))
		return false;
	data.assign(len, '\0');
	return uncompress((Bytef*)&data[0], &len, (const Bytef*)z.data() + 4, z.size() - 4) == Z_OK
		&& (data.resize(len), true);
}
