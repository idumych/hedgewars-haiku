// The data layers (see DataFS.h). A .hwp is a zip: its central directory is
// read once at mount time, and a file is inflated with zlib when it's read.
#include "DataFS.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#include <algorithm>
#include <map>
#include <set>

namespace {

struct ZipEntry {
	uint16_t method;
	uint32_t compSize, size, offset;	// offset of the local header
};

struct Layer {
	std::string dir;	// a folder, or
	std::string zip;	// a package, with its files by path ("Maps/X/map.cfg")
	std::map<std::string, ZipEntry> files;
};

std::vector<Layer> sLayers;

uint32_t le(const unsigned char* p, int n)
{
	uint32_t v = 0;
	for (int i = n - 1; i >= 0; i--)
		v = v << 8 | p[i];
	return v;
}

bool read_at(FILE* f, long at, void* buf, size_t n)
{
	return fseek(f, at, SEEK_SET) == 0 && fread(buf, 1, n, f) == n;
}

// shortcut: no zip64 and no multi-disk archives; add-on packages are small single zips
bool read_zip(const std::string& path, Layer& layer)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f)
		return false;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	// the end record is in the last 22 bytes plus a comment of up to 64 KiB
	long tail = std::min(size, 22L + 65535);
	std::vector<unsigned char> buf(tail);
	bool ok = read_at(f, size - tail, buf.data(), tail);
	long end = -1;
	for (long i = tail - 22; ok && i >= 0 && end < 0; i--)
		if (le(&buf[i], 4) == 0x06054b50)
			end = i;
	if (end < 0) {
		fclose(f);
		return false;
	}
	uint32_t count = le(&buf[end + 10], 2), dirSize = le(&buf[end + 12], 4), dirAt = le(&buf[end + 16], 4);
	std::vector<unsigned char> dir(dirSize);
	ok = read_at(f, dirAt, dir.data(), dirSize);
	fclose(f);
	for (size_t p = 0, n = 0; ok && n < count && p + 46 <= dir.size(); n++) {
		const unsigned char* h = &dir[p];
		if (le(h, 4) != 0x02014b50)
			break;
		uint32_t nameLen = le(h + 28, 2), extra = le(h + 30, 2), comment = le(h + 32, 2);
		if (p + 46 + nameLen > dir.size())
			break;
		std::string name((const char*)h + 46, nameLen);
		if (!name.empty() && name.back() != '/')
			layer.files[name] = {(uint16_t)le(h + 10, 2), le(h + 20, 4), le(h + 24, 4), le(h + 42, 4)};
		p += 46 + nameLen + extra + comment;
	}
	return true;
}

bool unzip(const Layer& layer, const ZipEntry& e, std::string& out)
{
	FILE* f = fopen(layer.zip.c_str(), "rb");
	if (!f)
		return false;
	unsigned char h[30];
	std::string comp(e.compSize, '\0');
	bool ok = read_at(f, e.offset, h, 30) && le(h, 4) == 0x04034b50
		&& read_at(f, e.offset + 30 + le(h + 26, 2) + le(h + 28, 2), &comp[0], e.compSize);
	fclose(f);
	if (!ok)
		return false;
	if (e.method == 0) {
		out = comp;
		return true;
	}
	if (e.method != 8)
		return false;
	out.assign(e.size, '\0');
	z_stream z = {};
	if (inflateInit2(&z, -MAX_WBITS) != Z_OK)	// raw deflate, no zlib header
		return false;
	z.next_in = (Bytef*)comp.data();
	z.avail_in = comp.size();
	z.next_out = (Bytef*)&out[0];
	z.avail_out = out.size();
	int r = inflate(&z, Z_FINISH);
	inflateEnd(&z);
	return r == Z_STREAM_END && z.total_out == e.size;
}

bool is_dir(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool ends_with(const std::string& s, const char* suffix)
{
	size_t l = strlen(suffix);
	return s.size() >= l && s.compare(s.size() - l, l, suffix) == 0;
}

}	// namespace

void datafs_mount(const std::vector<std::string>& dirs)
{
	sLayers.clear();
	std::vector<std::string> packages;
	for (const std::string& d : dirs) {
		sLayers.push_back({d, "", {}});
		if (DIR* dir = opendir(d.c_str())) {
			while (dirent* e = readdir(dir))
				if (ends_with(e->d_name, ".hwp"))
					packages.push_back(d + "/" + e->d_name);
			closedir(dir);
		}
	}
	std::sort(packages.begin(), packages.end());
	for (const std::string& p : packages) {
		Layer l = {"", p, {}};
		if (read_zip(p, l))
			sLayers.push_back(l);
		else
			fprintf(stderr, "Hedgewars: can't read the add-on package %s\n", p.c_str());
	}
}

std::vector<std::string> datafs_list(const std::string& sub, const char* suffix, bool dirs)
{
	std::set<std::string> names;
	auto add = [&](std::string n, bool isDir) {
		if (isDir != dirs || n == "CMakeLists.txt" || n[0] == '.')
			return;
		if (suffix) {
			if (n.size() <= strlen(suffix) || !ends_with(n, suffix))
				return;
			n.resize(n.size() - strlen(suffix));
		}
		names.insert(n);
	};
	std::string prefix = sub + "/";
	for (const Layer& l : sLayers) {
		if (!l.dir.empty()) {
			std::string path = l.dir + "/" + sub;
			if (DIR* dir = opendir(path.c_str())) {
				while (dirent* e = readdir(dir))
					add(e->d_name, is_dir(path + "/" + e->d_name));
				closedir(dir);
			}
			continue;
		}
		for (auto it = l.files.lower_bound(prefix); it != l.files.end()
				&& it->first.compare(0, prefix.size(), prefix) == 0; ++it) {
			std::string rest = it->first.substr(prefix.size());
			size_t slash = rest.find('/');
			add(rest.substr(0, slash), slash != std::string::npos);
		}
	}
	return std::vector<std::string>(names.begin(), names.end());
}

bool datafs_exists(const std::string& path)
{
	struct stat st;
	std::string dir = path + "/";
	for (const Layer& l : sLayers) {
		if (!l.dir.empty()) {
			if (stat((l.dir + "/" + path).c_str(), &st) == 0)
				return true;
			continue;
		}
		auto it = l.files.lower_bound(dir);	// a folder: some file under it
		if (l.files.count(path) || (it != l.files.end() && it->first.compare(0, dir.size(), dir) == 0))
			return true;
	}
	return false;
}

bool datafs_read(const std::string& path, std::string& out)
{
	for (auto l = sLayers.rbegin(); l != sLayers.rend(); ++l) {
		if (l->dir.empty()) {
			auto it = l->files.find(path);
			if (it != l->files.end())
				return unzip(*l, it->second, out);
			continue;
		}
		FILE* f = fopen((l->dir + "/" + path).c_str(), "rb");
		if (!f)
			continue;
		out.clear();
		char buf[8192];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
			out.append(buf, n);
		fclose(f);
		return true;
	}
	return false;
}
