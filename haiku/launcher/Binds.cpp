#include "Binds.h"
#include "Ini.h"
#include "../hwhaiku/rawkeys.h"

#include <ctype.h>
#include <stdlib.h>

// frontend-qt6/binds.cpp, without its "!MULTI" combination notes
const BindAction kBindActions[] = {
	{"+up", "up", "Aim up", "Movement"},
	{"+left", "left", "Walk left", NULL},
	{"+right", "right", "Walk right", NULL},
	{"+down", "down", "Aim down", NULL},
	{"+precise", "left_shift", "Precise aim", NULL},
	{"ljump", "return", "Long jump", NULL},
	{"hjump", "backspace", "High jump", NULL},
	{"switch", "tab", "Switch hedgehog", NULL},
	{"ammomenu", "mouser", "Weapon menu", "Weapons"},
	{"slot 1", "f1", "Weapon slot 1", NULL},
	{"slot 2", "f2", "Weapon slot 2", NULL},
	{"slot 3", "f3", "Weapon slot 3", NULL},
	{"slot 4", "f4", "Weapon slot 4", NULL},
	{"slot 5", "f5", "Weapon slot 5", NULL},
	{"slot 6", "f6", "Weapon slot 6", NULL},
	{"slot 7", "f7", "Weapon slot 7", NULL},
	{"slot 8", "f8", "Weapon slot 8", NULL},
	{"slot 9", "f9", "Weapon slot 9", NULL},
	{"slot :", "f10", "Weapon slot 10", NULL},
	{"setweap ~", "none", "Put the weapon away", NULL},
	{"timer 1", "1", "Timer 1 second", NULL},
	{"timer 2", "2", "Timer 2 seconds", NULL},
	{"timer 3", "3", "Timer 3 seconds", NULL},
	{"timer 4", "4", "Timer 4 seconds", NULL},
	{"timer 5", "5", "Timer 5 seconds", NULL},
	{"timer_u", "n", "Change the timer", NULL},
	{"+attack", "space", "Attack", NULL},
	{"put", "mousel", "Pick a weapon or target", NULL},
	{"findhh", "h", "Find the hedgehog", "Camera"},
	{"+cur_u", "keypad_8", "Cursor up", NULL},
	{"+cur_l", "keypad_4", "Cursor left", NULL},
	{"+cur_r", "keypad_6", "Cursor right", NULL},
	{"+cur_d", "keypad_2", "Cursor down", NULL},
	{"zoomin", "wheelup", "Zoom in", NULL},
	{"zoomout", "wheeldown", "Zoom out", NULL},
	{"zoomreset", "mousem", "Reset zoom", NULL},
	{"chat", "t", "Chat", "Miscellaneous"},
	{"chat team", "u", "Clan chat", NULL},
	{"history", "`", "Chat history", NULL},
	{"pause", "p", "Pause", NULL},
	{"quit", "escape", "Quit", NULL},
	{"confirm", "y", "Confirm", NULL},
	{"+voldown", "9", "Volume down", NULL},
	{"+volup", "0", "Volume up", NULL},
	{"mute", "8", "Mute", NULL},
	{"fullscr", "f12", "Full screen", NULL},
	{"capture", "c", "Screenshot", NULL},
	{"+speedup", "f", "Speed up a replay", NULL},
	{"+mission", "m", "Mission information", NULL},
	{"gearinfo", "o", "Object information", NULL},
	{"rotmask", "delete", "Team bars", NULL},
	{"rottags", "home", "Hedgehog tags", NULL},
};
const int kBindActionCount = sizeof(kBindActions) / sizeof(kBindActions[0]);

std::vector<std::string> parse_key_names(const std::vector<std::string>& lines)
{
	std::vector<std::string> names(512);
	for (const std::string& l : lines) {
		size_t comma = l.find(',');
		if (comma == std::string::npos)
			continue;
		int sc = atoi(l.c_str());
		std::string name = l.substr(comma + 1);
		if (name.size() >= 2 && name.front() == '"' && name.back() == '"')
			name = name.substr(1, name.size() - 2);
		if (sc > 0 && sc < (int)names.size() && names[sc].empty())
			names[sc] = name;
	}
	return names;
}

int raw_key_to_scancode(int raw)
{
	return raw >= 0 && raw < 128 ? kRawToScancode[raw] : 0;
}

std::string key_label(const std::string& key)
{
	static const std::map<std::string, std::string> kNames = {
		{"none", "No key"}, {"mousel", "Left mouse button"}, {"mouser", "Right mouse button"},
		{"mousem", "Middle mouse button"}, {"wheelup", "Wheel up"}, {"wheeldown", "Wheel down"},
		{"pageup", "Page up"}, {"pagedown", "Page down"}, {"capslock", "Caps lock"},
		{"left_gui", "Left Windows key"}, {"right_gui", "Right Windows key"},
	};
	auto it = kNames.find(key);
	if (it != kNames.end())
		return it->second;
	std::string s = key;
	for (char& c : s)
		if (c == '_')
			c = ' ';
	if (!s.empty())
		s[0] = toupper((unsigned char)s[0]);
	return s;
}

Binds read_binds(const Ini& ini)
{
	Binds b;
	for (int i = 0; i < kBindActionCount; i++) {
		std::string v = ini.get("Binds", kBindActions[i].action);
		if (v != "default")
			b[kBindActions[i].action] = v;
	}
	return b;
}

void write_binds(Ini& ini, const Binds& b, bool team)
{
	for (int i = 0; i < kBindActionCount; i++) {
		auto it = b.find(kBindActions[i].action);
		std::string v = it != b.end() ? it->second : "";
		if (v.empty() && !team)
			v = kBindActions[i].key;
		ini.set("Binds", kBindActions[i].action, v);
	}
}
