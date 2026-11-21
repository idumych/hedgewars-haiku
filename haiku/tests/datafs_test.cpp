// Host test for launcher/DataFS.cpp: a data folder, a user folder with loose files,
// and a .hwp package (a zip written here, one file stored and one deflated).
#include "DataFS.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include <string>
#include <vector>

static void put(std::string& b, uint32_t v, int n)
{
	for (int i = 0; i < n; i++)
		b += (char)(v >> (8 * i));
}

static void write_file(const std::string& path, const std::string& data)
{
	FILE* f = fopen(path.c_str(), "wb");
	fwrite(data.data(), 1, data.size(), f);
	fclose(f);
}

// a zip as Info-ZIP writes one, with a comment after the end record
static std::string make_zip(const std::vector<std::pair<std::string, std::string>>& files)
{
	std::string out, dir;
	for (size_t i = 0; i < files.size(); i++) {
		const std::string& name = files[i].first;
		const std::string& data = files[i].second;
		bool packed = i % 2 == 1;
		std::string comp = data;
		if (packed) {
			comp.resize(compressBound(data.size()) + 16);
			z_stream z = {};
			deflateInit2(&z, 9, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
			z.next_in = (Bytef*)data.data();
			z.avail_in = data.size();
			z.next_out = (Bytef*)&comp[0];
			z.avail_out = comp.size();
			int r = deflate(&z, Z_FINISH);
			assert(r == Z_STREAM_END);
			comp.resize(z.total_out);
			deflateEnd(&z);
		}
		uint32_t crc = crc32(0, (const Bytef*)data.data(), data.size());
		uint32_t at = out.size();
		put(out, 0x04034b50, 4); put(out, 20, 2); put(out, 0, 2); put(out, packed ? 8 : 0, 2);
		put(out, 0, 4); put(out, crc, 4); put(out, comp.size(), 4); put(out, data.size(), 4);
		put(out, name.size(), 2); put(out, 3, 2);
		out += name + "xyz" + comp;
		put(dir, 0x02014b50, 4); put(dir, 20, 2); put(dir, 20, 2); put(dir, 0, 2);
		put(dir, packed ? 8 : 0, 2); put(dir, 0, 4); put(dir, crc, 4); put(dir, comp.size(), 4);
		put(dir, data.size(), 4); put(dir, name.size(), 2); put(dir, 0, 2); put(dir, 0, 2);
		put(dir, 0, 2); put(dir, 0, 2); put(dir, 0, 4); put(dir, at, 4);
		dir += name;
	}
	uint32_t dirAt = out.size();
	out += dir;
	put(out, 0x06054b50, 4); put(out, 0, 4); put(out, files.size(), 2); put(out, files.size(), 2);
	put(out, dir.size(), 4); put(out, dirAt, 4); put(out, 7, 2);
	return out + "comment";
}

int main()
{
	char tmpl[] = "/tmp/datafs_test.XXXXXX";
	std::string root = mkdtemp(tmpl);
	std::string data = root + "/data", user = root + "/user";
	for (const char* d : {"/data", "/data/Maps", "/data/Maps/Bath", "/data/Themes", "/data/Themes/Nature",
			"/user", "/user/Graphics", "/user/Graphics/Hats"})
		mkdir((root + d).c_str(), 0755);
	write_file(data + "/Maps/Bath/map.cfg", "Bath\n");
	write_file(data + "/Themes/Nature/theme.cfg", "sky = 1\n");
	write_file(user + "/Graphics/Hats/cap.png", "png");
	std::string big(5000, 'a');	// deflates to a little
	write_file(user + "/Addon.hwp", make_zip({
		{"Maps/Island/map.cfg", "Cheese\n4\n"},
		{"Maps/Island/map.png", big},
		{"Maps/Bath/map.cfg", "Golf\n"},	// a package wins over the game's data
		{"Themes/Cheese/theme.cfg", "hidden = false\n"},
		{"Graphics/Hats/crown.png", "png"},
	}));
	write_file(user + "/broken.hwp", "not a zip");	// skipped, with a message

	datafs_mount({data, user});
	auto maps = datafs_list("Maps", NULL, true);
	assert((maps == std::vector<std::string>{"Bath", "Island"}));
	assert((datafs_list("Themes", NULL, true) == std::vector<std::string>{"Cheese", "Nature"}));
	assert((datafs_list("Graphics/Hats", ".png", false) == std::vector<std::string>{"cap", "crown"}));
	assert(datafs_list("Maps/Island", NULL, false).size() == 2 && datafs_list("Nope", NULL, true).empty());
	assert(datafs_exists("Maps/Island/map.cfg") && datafs_exists("Maps/Island") && datafs_exists("Maps/Bath/map.cfg"));
	assert(!datafs_exists("Maps/Island/map.lua") && !datafs_exists("Maps/Isl"));
	std::string s;
	assert(datafs_read("Maps/Island/map.cfg", s) && s == "Cheese\n4\n");
	assert(datafs_read("Maps/Island/map.png", s) && s == big);
	assert(datafs_read("Maps/Bath/map.cfg", s) && s == "Golf\n");
	assert(datafs_read("Themes/Nature/theme.cfg", s) && s == "sky = 1\n");
	assert(datafs_read("Graphics/Hats/cap.png", s) && s == "png");
	assert(!datafs_read("Maps/None/map.cfg", s));
	system(("rm -rf " + root).c_str());
	printf("datafs_test: ok\n");
	return 0;
}
