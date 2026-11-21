// Host test for launcher/SchemeData.cpp: the Qt frontend's scheme and weapon set files.
#include "SchemeData.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main()
{
	std::vector<GameScheme> pre = predefined_schemes();
	assert(pre.size() == 16 && pre[0].name == "Default" && pre[0].v[27] == 45 && pre[0].predefined);
	assert(predefined_weapons().size() == 13 && predefined_weapons()[0].ammo.size() == 4 * kAmmoCount);
	assert(strlen(kEmptyAmmo) == 4 * kAmmoCount);
	assert(std::string(kAmmoNames[kAmmoMenu[0] - 1]) == "Bazooka");

	// a file lacking keys keeps Default's values; Qt writes booleans as true/false
	GameScheme s = parse_scheme("Mine", "turntime=20\r\nsharedammo=true\nscriptparam=a=b, c\nbogus\n");
	assert(s.name == "Mine" && !s.predefined);
	assert(s.v[27] == 20 && s.v[15] == 1 && s.v[12] == 1 && s.v[28] == 100);
	assert(s.scriptparam == "a=b, c");
	std::string text = format_scheme(s);
	assert(text.find("switchhog=false\n") == 0);
	assert(text.find("sharedammo=true\n") != std::string::npos);
	assert(text.find("turntime=20\n") != std::string::npos);
	assert(text.find("\nscriptparam=a=b, c\n") == text.size() - 20);
	GameScheme back = parse_scheme("Mine", text);
	for (int k = 1; k < 44; k++)
		assert(back.v[k] == s.v[k]);

	// an older, shorter weapon set: each quarter padded with zeros
	assert(fix_weapon_set("12345678") == std::string("12") + std::string(58, '0') + "34" + std::string(58, '0')
		+ "56" + std::string(58, '0') + "78" + std::string(58, '0'));
	assert(fix_weapon_set(kEmptyAmmo) == kEmptyAmmo);

	assert(unique_name("New", {"Default"}) == "New");
	assert(unique_name("New", {"new", "New (2)"}) == "New (3)");
	assert(unique_name("Copy of Default", {"Copy of Default"}) == "Copy of Default (2)");
	assert(valid_name("My scheme") && !valid_name("") && !valid_name("a/b") && !valid_name("c:d"));

	// round trip through a folder, which save creates; predefined names are skipped
	char dir[] = "/tmp/scheme_testXXXXXX";
	assert(mkdtemp(dir));
	std::string game = std::string(dir) + "/Schemes/Game", ammo = std::string(dir) + "/Schemes/Ammo";
	assert(save_scheme(game, s));
	GameScheme clash = s;
	clash.name = "default";
	assert(save_scheme(game, clash));
	std::vector<GameScheme> all = load_schemes(game);
	assert(all.size() == 17 && all[16].name == "Mine" && all[16].v[27] == 20 && all[16].scriptparam == "a=b, c");
	WeaponSet w = {"Mine", kEmptyAmmo, false};
	w.ammo[2] = '5';
	assert(save_weapons(ammo, w));
	std::vector<WeaponSet> ws = load_weapons(ammo);
	assert(ws.size() == 14 && ws[13].name == "Mine" && ws[13].ammo == w.ammo && !ws[13].predefined);
	std::string cmd = std::string("rm -rf ") + dir;
	assert(system(cmd.c_str()) == 0);

	puts("scheme_test: all passed");
	return 0;
}
