// Haiku-only test for Game.cpp's progress file: a damaged file is kept, never
// replaced by a single key. Run by make -f haiku/Makefile test on Haiku.
#include "Game.h"
#include <Application.h>
#include <File.h>
#include <Directory.h>
#include <Entry.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main() {
	BApplication app("application/x-vnd.progress-test");
	gUserDir.SetTo("/tmp/hw-progress-test");
	create_directory(gUserDir.Path(), 0755);
	BPath p(gUserDir.Path(), "progress");
	unlink(p.Path());
	set_progress("a", "1"); set_progress("b", "2");	// new file, then an update
	assert(get_progress("a") == "1" && get_progress("b") == "2");
	{ BFile f(p.Path(), B_WRITE_ONLY | B_ERASE_FILE); }	// empty: start over
	set_progress("c", "3");
	assert(get_progress("c") == "3");
	set_progress("d", "4");
	{ BFile f(p.Path(), B_READ_WRITE); f.SetSize(10); }	// half written: keep it
	set_progress("e", "5");
	off_t size; BFile(p.Path(), B_READ_ONLY).GetSize(&size);
	assert(size == 10 && get_progress("e") == "");
	assert(!BEntry(BPath(gUserDir.Path(), "progress.tmp").Path()).Exists());
	puts("progress: all passed");
	return 0;
}
