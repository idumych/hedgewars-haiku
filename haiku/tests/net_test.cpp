// Host test for launcher/NetProto.cpp: server message framing and a net game's engine config.
#include "NetProto.h"
#include "DrawnMap.h"

#include <assert.h>
#include <stdio.h>

using namespace net;

// the engine messages in a length-prefixed buffer
static std::vector<std::string> unpack(const std::string& buf)
{
	std::vector<std::string> out;
	for (size_t at = 0; at < buf.size(); at += 1 + (unsigned char)buf[at])
		out.push_back(buf.substr(at + 1, (unsigned char)buf[at]));
	return out;
}

static bool has(const std::vector<std::string>& v, const std::string& s)
{
	for (const std::string& x : v)
		if (x == s)
			return true;
	return false;
}

int main()
{
	std::vector<std::string> parts;
	std::string buf = "CONNECTED\nHedgewars server\n3\n\nPING\n\nROOMS\n";
	assert(next_message(buf, parts) && parts.size() == 3 && parts[0] == "CONNECTED" && parts[2] == "3");
	assert(next_message(buf, parts) && parts.size() == 1 && parts[0] == "PING");
	assert(!next_message(buf, parts) && buf == "ROOMS\n");
	buf += "\n";
	assert(next_message(buf, parts) && parts == std::vector<std::string>{"ROOMS"} && buf.empty());
	assert(message({"CHAT", "hi"}) == "CHAT\nhi\n\n");

	for (const char* s : {"", "a", "ab", "abc", "\x01\xff\x80 engine"})
		assert(base64_decode(base64_encode(s)) == s);
	assert(base64_encode("\x03" "edg") == "A2VkZw==");

	Team t;
	t.name = "Haiku Hogs";
	t.hogNames[0] = "Spiky";
	std::vector<std::string> f = add_team(t);
	assert(f.size() == 23 && f[0] == "Haiku Hogs" && f[7] == "Spiky" && f[8] == "NoHat" && f[9] == "Hog 2");
	// the server sends the owner where the colour was
	f[1] = "Simple";
	f.insert(f.begin() + 5, "alice");
	f.erase(f.begin() + 6);
	Team r;
	assert(parse_team(f, r) && r.owner == "alice" && r.hogNames[0] == "Spiky" && r.hats[1] == "NoHat");
	assert(!parse_team({"x"}, r));

	// the master's settings, as another client receives them
	Config c;
	for (const auto& m : master_config(predefined_schemes()[1], predefined_weapons()[2], "+maze+", 1, "Nature", "{seed}", 0)) {
		assert(m.size() >= 2);
		set_param(c, m[0], std::vector<std::string>(m.begin() + 1, m.end()));
	}
	assert(c["SCHEME"].size() == 45 && c["SCHEME"][0] == "Pro Mode" && c["SCHEME"][44] == "!");
	assert(param(c, "AMMO") == "Pro Mode" && param(c, "AMMO", 1).size() % 4 == 0);
	Team mine = t;
	mine.remote = false;
	mine.owner = "bob";
	mine.color = 1;
	mine.hogs = 2;
	r.color = 0;
	std::vector<std::string> cfg = unpack(engine_config(c, {r, mine}, "Nature"));
	assert(cfg[0] == "TN");
	assert(has(cfg, "etheme Nature") && has(cfg, "eseed {seed}") && has(cfg, "e$mapgen 1"));
	assert(has(cfg, "e$turntime 15000") && has(cfg, "e$maze_size 0") && has(cfg, "e$gmflags 73728"));
	assert(!has(cfg, "emap +maze+"));
	assert(has(cfg, "eaddteam 0 16712196 Haiku Hogs") && has(cfg, "eaddteam 0 4817089 Haiku Hogs"));
	int rdriven = 0, hogs = 0;
	for (const std::string& m : cfg) {
		rdriven += m == "erdriven";
		hogs += m.compare(0, 7, "eaddhh ") == 0;
	}
	assert(rdriven == 1 && hogs == 4 + 2);
	assert(has(cfg, "eaddhh 0 100 Spiky") && has(cfg, "eowner alice"));

	// a room that only names its scheme and weapons (the server's defaults) and a fixed map
	Config d;
	set_param(d, "FULLMAPCONFIG", {"12", "Bath", "0", "0", "seed", "0"});
	set_param(d, "SCHEME", {"Default"});
	set_param(d, "AMMO", {"Default"});
	set_param(d, "SCRIPT", {"Capture the Flag"});
	cfg = unpack(engine_config(d, {r}, "Bath"));
	assert(has(cfg, "emap Bath") && has(cfg, "e$turntime 45000") && has(cfg, "e$feature_size 12"));
	assert(has(cfg, "escript Scripts/Multiplayer/Capture_the_Flag.lua") && has(cfg, "eammstore"));

	// a hand-drawn map: DRAWNMAP holds the drawing as a .hwmap does, the engine gets it as edraw
	std::string points(450, 'x');
	Config h;
	for (const auto& m : master_config(predefined_schemes()[0], predefined_weapons()[0], "+drawn+", 3,
			"Nature", "s", 0, to_hwmap(points)))
		set_param(h, m[0], std::vector<std::string>(m.begin() + 1, m.end()));
	cfg = unpack(engine_config(h, {r}, "Nature"));
	assert(has(cfg, "e$mapgen 3") && !has(cfg, "emap +drawn+"));
	assert(has(cfg, "edraw " + points.substr(0, 200)) && has(cfg, "edraw " + points.substr(400)));
	// the host's game style
	Config e;
	for (const auto& m : master_config(predefined_schemes()[0], predefined_weapons()[0], "+rnd+", 0, "Nature", "s", 0, "",
			"Capture the Flag"))
		set_param(e, m[0], std::vector<std::string>(m.begin() + 1, m.end()));
	assert(param(e, "SCRIPT") == "Capture the Flag");

	// room flags, admin replies
	assert(room_flags_text("-") == "" && room_flags_text("-pj") == "password, joins restricted");
	auto vars = server_vars({"MOTD_NEW", "hi", "MOTD_OLD", "old", "LATEST_PROTO", "60"});
	assert(vars["MOTD_NEW"] == "hi" && vars["LATEST_PROTO"] == "60");
	auto b = bans({"I", "10.0.0.3", "spam", "50s", "N", "bob", "rude", "3600s", "x"});
	assert(b.size() == 2 && b[1].kind == "N" && b[1].who == "bob" && b[0].left == "50s");

	assert(chat_line("bob", "hi") == "\x01" "bob: hi");
	assert(chat_line("bob", "/me waves") == "\x02* bob waves");
	puts("net_test: all passed");
	return 0;
}
