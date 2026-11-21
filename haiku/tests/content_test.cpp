// Host test for launcher/ContentData.cpp: reading hedgewars.org's add-on table.
#include "ContentData.h"

#include <assert.h>
#include <stdio.h>

int main()
{
	// shaped like hedgewars.org/content.html
	const char* page = "<html><a class=\"dlc_link\" href=\"/outside.hwp\">not in the list</a>\n"
		"<!-- BEGIN -->\n<table>\n<tr>\n<th>Name</th><th>Description</th>\n</tr>\n"
		"<tr><td colspan=6 class=\"dlc_type\">Themes</td></tr>\n"
		"<tr>\n<td><a class=\"dlc_link\" href=\"/download/packs/Theme_A_v1.hwp\">A v1</a></td>\n"
		"<td>Morning &amp; night,\n  <code>ready=1</code></td>\n<td>KIRA</td>\n<td>899 KiB</td>\n"
		"<td>2024-12-30</td>\n<td><a href=\"/node/9020\">CC BY-SA 4.0</a></td>\n</tr>\n"
		"<tr><td colspan=6 class=\"dlc_type\">Hats</td></tr>\n"
		"<tr>\n<td><a class=\"dlc_link\" href=\"/download/packs/Hats.zip\">Hats</a></td>\n"
		"<td>Some hats</td>\n<td></td>\n<td>20 KiB</td>\n<td>2010-01-01</td>\n<td>GPL</td>\n</tr>\n"
		"</table>\n<a href=\"https://hh.unit22.org/dlc\">elsewhere</a>\n<!-- END --></html>";
	std::vector<Package> p = parse_content(page);
	assert(p.size() == 2);
	assert(p[0].kind == "Themes" && p[0].name == "A v1" && p[0].url == "/download/packs/Theme_A_v1.hwp");
	assert(p[0].description == "Morning & night, ready=1" && p[0].credits == "KIRA");
	assert(p[0].size == "899 KiB" && p[0].date == "2024-12-30" && p[0].license == "CC BY-SA 4.0");
	assert(p[1].kind == "Hats" && p[1].credits.empty() && p[1].license == "GPL");
	assert(package_url(p[0]) == "https://www.hedgewars.org/download/packs/Theme_A_v1.hwp");
	assert(package_file(p[0]) == "Theme_A_v1.hwp" && package_file(p[1]) == "Hats.hwp");
	assert(parse_content("").empty());
	puts("content_test: all passed");
	return 0;
}
