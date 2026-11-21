// The game's data as the engine sees it through PhysFS (uPhysFSLayer.pas): the
// game's Data folder, the user's Data folder, and the .hwp (zip) add-on packages
// in either. Plain C++ and POSIX, no Haiku headers (tests/datafs_test.cpp).
#pragma once

#include <string>
#include <vector>

// folders whose files and .hwp packages make up the data, in the engine's order
void datafs_mount(const std::vector<std::string>& dirs);
// names in a data folder ("Themes"), from every layer, sorted and without repeats;
// suffix (".png") must match and is cut off; dirs picks folders or files
std::vector<std::string> datafs_list(const std::string& sub, const char* suffix, bool dirs);
bool datafs_exists(const std::string& path);	// a file or folder, in any layer
// a file's contents, from the last layer that has it (as PhysFS: later mounts win)
bool datafs_read(const std::string& path, std::string& out);
