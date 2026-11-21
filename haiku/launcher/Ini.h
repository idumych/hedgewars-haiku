// INI files the way Qt's QSettings writes them (the Qt frontend's Teams/*.hwt
// and settings.ini), which the engine also reads: "[Binds]" and "[colors]".
// Portable: no Haiku headers, tested in tests/ini_test.cpp.
#pragma once

#include <string>
#include <utility>
#include <vector>

struct Ini {
	typedef std::vector<std::pair<std::string, std::string>> Keys;
	std::vector<std::pair<std::string, Keys>> sections;	// in file order; names and keys decoded

	void parse(const std::string& text);
	std::string text() const;
	bool load(const std::string& path);
	bool save(const std::string& path) const;	// beside the file, then renamed over it

	std::string get(const std::string& section, const std::string& key,
		const std::string& fallback = "") const;
	void set(const std::string& section, const std::string& key, const std::string& value);
	Keys* find(const std::string& section);
};

// "Team name/x" -> a safe file name, as DataManager::safeFileName and the engine do
std::string safe_file_name(std::string name);
