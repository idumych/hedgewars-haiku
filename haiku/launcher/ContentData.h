// The add-on list on hedgewars.org/content.html (frontend-qt6/ui/page/pagedata.cpp
// shows that page as it is; this reads its table). No Haiku headers.
#pragma once

#include <string>
#include <vector>

struct Package {
	std::string kind, name, url, description, credits, size, date, license;
};

// the packages in the page's table, under their kinds ("Themes", "Maps"…)
std::vector<Package> parse_content(const std::string& html);
// where a package's link points ("/download/…" is on hedgewars.org)
std::string package_url(const Package& p);
// the name it's saved under in the user's Data folder: a .zip becomes a .hwp, as in the Qt frontend
std::string package_file(const Package& p);
