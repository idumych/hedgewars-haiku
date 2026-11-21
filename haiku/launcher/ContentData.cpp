#include "ContentData.h"

#include <string.h>

// the text of an HTML fragment: tags dropped, the common entities decoded, spaces collapsed
static std::string text_of(const std::string& html)
{
	std::string out;
	bool tag = false;
	for (size_t i = 0; i < html.size(); i++) {
		char c = html[i];
		if (c == '<')
			tag = true;
		else if (c == '>')
			tag = false;
		else if (!tag) {
			static const struct { const char* entity; const char* text; } kEntities[] = {
				{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&nbsp;", " "}};
			bool decoded = false;
			for (const auto& e : kEntities)
				if (html.compare(i, strlen(e.entity), e.entity) == 0) {
					out += e.text;
					i += strlen(e.entity) - 1;
					decoded = true;
					break;
				}
			if (decoded)
				continue;
			if (c == '\n' || c == '\r' || c == '\t')
				c = ' ';
			if (c != ' ' || (!out.empty() && out.back() != ' '))
				out += c;
		}
	}
	while (!out.empty() && out.back() == ' ')
		out.pop_back();
	return out;
}

// the contents of the next <td …>…</td> from at; at moves past it
static bool next_cell(const std::string& html, size_t& at, size_t end, std::string& cell)
{
	size_t open = html.find("<td", at);
	if (open == std::string::npos || open >= end)
		return false;
	size_t start = html.find('>', open);
	size_t close = html.find("</td>", start);
	if (start == std::string::npos || close == std::string::npos)
		return false;
	cell = html.substr(start + 1, close - start - 1);
	at = close + 5;
	return true;
}

std::vector<Package> parse_content(const std::string& page)
{
	std::vector<Package> out;
	size_t begin = page.find("<!-- BEGIN -->"), stop = page.find("<!-- END -->");
	std::string html = begin != std::string::npos && stop != std::string::npos && begin < stop
		? page.substr(begin, stop - begin) : page;
	std::string kind;
	for (size_t row = html.find("<tr"); row != std::string::npos; row = html.find("<tr", row + 3)) {
		size_t end = html.find("</tr>", row);
		if (end == std::string::npos)
			break;
		std::string first;
		size_t at = row;
		if (!next_cell(html, at, end, first))
			continue;
		size_t open = html.rfind("<td", at);
		std::string tag = html.substr(open, html.find('>', open) - open);
		if (tag.find("dlc_type") != std::string::npos) {
			kind = text_of(first);
			continue;
		}
		size_t link = first.find("dlc_link");
		size_t href = first.find("href=\"", link == std::string::npos ? 0 : link);
		if (link == std::string::npos || href == std::string::npos)
			continue;
		Package p;
		p.kind = kind;
		p.url = first.substr(href + 6, first.find('"', href + 6) - href - 6);
		p.name = text_of(first);
		std::string* fields[] = {&p.description, &p.credits, &p.size, &p.date, &p.license};
		std::string cell;
		for (std::string* f : fields)
			if (next_cell(html, at, end, cell))
				*f = text_of(cell);
		out.push_back(p);
	}
	return out;
}

std::string package_url(const Package& p)
{
	return p.url.compare(0, 1, "/") == 0 ? "https://www.hedgewars.org" + p.url : p.url;
}

std::string package_file(const Package& p)
{
	std::string name = p.url.substr(p.url.rfind('/') + 1);
	if (name.size() > 4 && name.compare(name.size() - 4, 4, ".zip") == 0)
		name.replace(name.size() - 4, 4, ".hwp");
	return name;
}
