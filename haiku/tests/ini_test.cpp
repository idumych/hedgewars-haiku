// Host test for launcher/Ini.cpp, Binds.cpp and DrawnMap.cpp: the Qt frontend's
// INI files (as the engine reads them) and hand-drawn maps.
// Run from the top of the tree (reads misc/OfficialChallenges/racer_#1.hwmap).
#include "Binds.h"
#include "DrawnMap.h"
#include "Ini.h"

#include <assert.h>
#include <stdio.h>

int main()
{
	// a team file as QSettings writes it
	Ini t;
	t.parse("[Team]\nName=Hog Squad\nGrave=Statue\n\n[Hedgehog0]\nName=\"Sir, the First\"\nHat=NoHat\n\n"
		"[Binds]\n%2Battack=space\nslot%201=\nhistory=`\n\n[Achievements]\nx=3\n");
	assert(t.get("Team", "Name") == "Hog Squad");
	assert(t.get("Hedgehog0", "Name") == "Sir, the First");
	assert(t.get("Binds", "+attack") == "space" && t.get("Binds", "slot 1") == "");
	assert(t.get("Nope", "x", "fb") == "fb");
	Binds b = read_binds(t);
	b["+attack"] = "a";
	b["+left"] = "\\";
	write_binds(t, b, true);
	t.set("Team", "Name", "New name");
	std::string text = t.text();
	// keys escaped as QSettings does, the engine's "[Binds]" block ends at a blank line,
	// sections it doesn't know survive
	assert(text.find("%2Battack=a\n") != std::string::npos);
	assert(text.find("slot%201=\n") != std::string::npos);
	assert(text.find("%2Bleft=\\\\\n") != std::string::npos);
	assert(text.find("Name=\"Sir, the First\"") != std::string::npos);
	assert(text.find("\n\n[Achievements]\nx=3\n") != std::string::npos);
	Ini again;
	again.parse(text);
	assert(again.get("Binds", "+left") == "\\" && again.get("Team", "Name") == "New name");

	// settings.ini: every action gets a key, defaults included
	Ini s;
	write_binds(s, Binds{{"+up", "w"}}, false);
	assert(s.get("Binds", "+up") == "w" && s.get("Binds", "+down") == "down");
	assert(s.get("Binds", "setweap ~") == "none");

	assert(safe_file_name("a/b:c\\d") == "a_b_c_d");
	std::vector<std::string> names = parse_key_names({"4,\"a\"", "44,\"space\"", "49,\"\\\""});
	assert(names[4] == "a" && names[44] == "space" && names[49] == "\\");
	assert(raw_key_to_scancode(0x3c) == 4 && raw_key_to_scancode(0x5e) == 44);	// A, space
	assert(key_label("left_shift") == "Left shift" && key_label("mousel") == "Left mouse button");

	// drawn maps: a stroke's first point carries the pen, as DrawMapScene::encode
	Drawing d;
	Stroke st;
	st.width = 76;
	st.erasing = true;
	st.points = {{10, 20}, {-5, 2000}};
	d.strokes.push_back(st);
	std::string enc = encode_drawing(d);
	assert(enc.size() == 10 && (unsigned char)enc[4] == (0x80 | 0x40 | 7) && enc[9] == 0);
	Drawing back = decode_drawing(enc);
	assert(back.strokes.size() == 1 && back.strokes[0].width == 76 && back.strokes[0].erasing);
	assert(back.strokes[0].points[1] == std::make_pair(-5, 2000));
	std::string data;
	assert(from_hwmap(to_hwmap(enc), data) && data == enc);
	Stroke line;
	line.points = {{0, 0}, {1, 1}, {2, 2}, {100, 0}, {101, 0}};
	simplify(line);
	assert(line.points.size() == 3);	// the start, the far point, the end
	assert(ellipse_points(100, 100, 200, 150).front() == ellipse_points(100, 100, 200, 150).back());

	// a map saved by the Qt frontend decodes and encodes back to the same bytes
	FILE* f = fopen("misc/OfficialChallenges/racer_#1.hwmap", "rb");
	assert(f);
	std::string file;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		file.append(buf, n);
	fclose(f);
	assert(from_hwmap(file, data) && data.size() % 5 == 0 && !data.empty());
	Drawing racer = decode_drawing(data);
	assert(!racer.strokes.empty() && encode_drawing(racer) == data);

	printf("ini test: ok\n");
	return 0;
}
