#include "Ini.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// QSettings' iniEscapedKey: letters, digits and "_-." stay, "/" becomes "\", the rest %XX
static std::string escape_key(const std::string& k)
{
	std::string out;
	for (unsigned char c : k) {
		if (c == '/')
			out += '\\';
		else if (isalnum(c) || c == '_' || c == '-' || c == '.')
			out += c;
		else {
			char hex[4];
			snprintf(hex, sizeof(hex), "%%%02X", c);
			out += hex;
		}
	}
	return out;
}

static std::string unescape_key(const std::string& k)
{
	std::string out;
	for (size_t i = 0; i < k.size(); i++) {
		if (k[i] == '\\')
			out += '/';
		else if (k[i] == '%' && i + 2 < k.size() && isxdigit((unsigned char)k[i + 1])
			&& isxdigit((unsigned char)k[i + 2])) {
			out += (char)strtol(k.substr(i + 1, 2).c_str(), NULL, 16);
			i += 2;
		} else
			out += k[i];
	}
	return out;
}

// QSettings' iniEscapedString: quotes around values with ";,=" or edge spaces
static std::string escape_value(const std::string& v)
{
	bool quote = !v.empty() && (v.front() == ' ' || v.back() == ' ');
	std::string body;
	for (char c : v) {
		if (c == ';' || c == ',' || c == '=')
			quote = true;
		if (c == '\\' || c == '"')
			body += '\\';
		if (c == '\n')
			body += "\\n";
		else
			body += c;
	}
	return quote ? "\"" + body + "\"" : body;
}

static std::string unescape_value(const std::string& v)
{
	std::string out;
	for (size_t i = 0; i < v.size(); i++) {
		if (v[i] == '"')
			continue;
		if (v[i] == '\\' && i + 1 < v.size()) {
			i++;
			out += v[i] == 'n' ? '\n' : v[i] == 't' ? '\t' : v[i];
		} else
			out += v[i];
	}
	return out;
}

static std::string trim(const std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

void Ini::parse(const std::string& text)
{
	sections.clear();
	Keys* cur = NULL;
	size_t start = 0;
	while (start < text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		std::string line = trim(text.substr(start, end - start));
		start = end + 1;
		if (line.empty() || line[0] == ';')
			continue;
		if (line[0] == '[') {
			std::string name = unescape_key(line.substr(1, line.find(']') - 1));
			cur = find(name);
			if (!cur) {
				sections.push_back({name, Keys()});
				cur = &sections.back().second;
			}
			continue;
		}
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		if (!cur) {
			sections.push_back({"General", Keys()});
			cur = &sections.back().second;
		}
		cur->push_back({unescape_key(trim(line.substr(0, eq))), unescape_value(trim(line.substr(eq + 1)))});
	}
}

std::string Ini::text() const
{
	std::string out;
	for (const auto& s : sections) {
		if (!out.empty())
			out += "\n";	// the engine reads a section up to its blank line
		out += "[" + escape_key(s.first) + "]\n";
		for (const auto& k : s.second)
			out += escape_key(k.first) + "=" + escape_value(k.second) + "\n";
	}
	return out;
}

bool Ini::load(const std::string& path)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	std::string text;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		text.append(buf, n);
	fclose(f);
	parse(text);
	return true;
}

bool Ini::save(const std::string& path) const
{
	std::string tmp = path + ".tmp", t = text();
	FILE* f = fopen(tmp.c_str(), "wb");
	if (!f)
		return false;
	bool ok = fwrite(t.data(), 1, t.size(), f) == t.size();
	ok = fclose(f) == 0 && ok;
	return ok && rename(tmp.c_str(), path.c_str()) == 0;
}

Ini::Keys* Ini::find(const std::string& section)
{
	for (auto& s : sections)
		if (s.first == section)
			return &s.second;
	return NULL;
}

std::string Ini::get(const std::string& section, const std::string& key, const std::string& fallback) const
{
	for (const auto& s : sections)
		if (s.first == section)
			for (const auto& k : s.second)
				if (k.first == key)
					return k.second;
	return fallback;
}

void Ini::set(const std::string& section, const std::string& key, const std::string& value)
{
	Keys* keys = find(section);
	if (!keys) {
		sections.push_back({section, Keys()});
		keys = &sections.back().second;
	}
	for (auto& k : *keys)
		if (k.first == key) {
			k.second = value;
			return;
		}
	keys->push_back({key, value});
}

std::string safe_file_name(std::string name)
{
	for (char& c : name)
		if (c == '\\' || c == '/' || c == ':')
			c = '_';
	return name;
}
