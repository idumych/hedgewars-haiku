// Runs hwengine with --internal --port and answers its IPC requests, as
// frontend-qt6/game.cpp and net/tcpBase.cpp do.
#include "Game.h"

#include <Application.h>
#include <Directory.h>
#include <Entry.h>
#include <File.h>
#include <FindDirectory.h>
#include <Autolock.h>
#include <Locker.h>
#include <Message.h>
#include <PathFinder.h>
#include <StringList.h>
#include <Roster.h>
#include <Screen.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char** environ;

BPath gAppDir, gDataDir, gUserDir;

static const char* kProto = "60";	// cProtoVer, in recordings' file names

bool find_data()
{
	app_info info;
	be_app->GetAppInfo(&info);
	BPath app(&info.ref);
	app.GetParent(&gAppDir);
	std::vector<BPath> candidates;
	if (const char* env = getenv("HEDGEWARS_DATA"))
		candidates.push_back(BPath(env));
	candidates.push_back(BPath(gAppDir.Path(), "Data"));
	// the package's data, wherever it's installed (system or home)
	BStringList found;
	BPathFinder::FindPaths(B_FIND_PATH_DATA_DIRECTORY, "hedgewars/Data", found);
	for (int32 i = 0; i < found.CountStrings(); i++)
		candidates.push_back(BPath(found.StringAt(i)));
#ifdef HW_DATADIR
	candidates.push_back(BPath(HW_DATADIR "/Data"));
#endif
	for (const BPath& p : candidates) {
		if (BEntry(BPath(p.Path(), "misc/keys.csv").Path()).Exists()) {
			gDataDir = p;
			break;
		}
	}
	find_directory(B_USER_SETTINGS_DIRECTORY, &gUserDir);
	gUserDir.Append("Hedgewars");
	create_directory(BPath(gUserDir.Path(), "Demos").Path(), 0755);
	create_directory(BPath(gUserDir.Path(), "Saves").Path(), 0755);
	return gDataDir.InitCheck() == B_OK;
}

std::vector<std::string> list_dir(const char* sub, const char* suffix, bool dirs, bool user)
{
	std::vector<std::string> out;
	BDirectory d(BPath(user ? gUserDir.Path() : gDataDir.Path(), sub).Path());
	BEntry e;
	while (d.GetNextEntry(&e) == B_OK) {
		char name[B_FILE_NAME_LENGTH];
		e.GetName(name);
		std::string n = name;
		if (dirs != e.IsDirectory())
			continue;
		if (suffix) {
			size_t l = strlen(suffix);
			if (n.size() <= l || n.compare(n.size() - l, l, suffix) != 0)
				continue;
			n.resize(n.size() - l);
		}
		if (n != "CMakeLists.txt")
			out.push_back(n);
	}
	std::sort(out.begin(), out.end());
	return out;
}

std::vector<std::string> read_lines(const char* dataPath)
{
	std::vector<std::string> out;
	FILE* f = fopen(BPath(gDataDir.Path(), dataPath).Path(), "r");
	char line[1024];
	while (f && fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = 0;
		if (line[0])
			out.push_back(line);
	}
	if (f)
		fclose(f);
	return out;
}

std::string ini_value(const char* dataPath, const std::string& key, const std::string& fallback)
{
	std::string section;
	for (const std::string& line : read_lines(dataPath)) {
		if (line[0] == '[') {
			section = line.substr(1, line.find(']') - 1) + "/";
			continue;
		}
		size_t eq = line.find('=');
		if (eq == std::string::npos || section + line.substr(0, eq) != key)
			continue;
		std::string v = line.substr(eq + 1);
		if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
			v = v.substr(1, v.size() - 2);
		return v;
	}
	return fallback;
}

void add(std::string& buf, const std::string& s)
{
	std::string m = s.substr(0, 250);	// the protocol's limit
	buf += (char)(uint8)m.size();
	buf += m;
}

std::string new_seed()
{
	char seed[64];
	snprintf(seed, sizeof(seed), "{%08x-%04x-%04x-%04x-%08x%04x}", (unsigned)rand(),
		rand() & 0xffff, rand() & 0xffff, rand() & 0xffff, (unsigned)rand(), rand() & 0xffff);
	return seed;
}

// HWTeam::teamGameConfig
std::string team_config(const TeamInfo& t, int hogs, int health, bool mission)
{
	std::string buf;
	char line[300];
	snprintf(line, sizeof(line), "%s 0 %u %s", mission ? "esetmissteam" : "eaddteam",
		(unsigned)t.color, t.name.c_str());
	add(buf, line);
	add(buf, "egrave " + t.grave);
	add(buf, "efort " + t.fort);
	add(buf, "evoicepack " + t.voice);
	add(buf, "eflag " + t.flag);
	for (int h = 0; h < hogs && h < 8; h++) {
		std::string name = t.hogs[h].empty() ? "Hog " + std::to_string(h + 1) : t.hogs[h];
		add(buf, std::string(mission ? "eaddmisshh " : "eaddhh ") + std::to_string(t.difficulty)
			+ " " + std::to_string(health) + " " + name);
		add(buf, "ehat " + (t.hats[h].empty() ? std::string("NoHat") : t.hats[h]));
	}
	return buf;
}

// ── progress: what mission and campaign scripts save (Qt keeps it in Teams/*.hwt) ──
static BLocker sProgressLock;

static BPath progress_path() { return BPath(gUserDir.Path(), "progress"); }

std::string get_progress(const std::string& key)
{
	BAutolock l(sProgressLock);
	BFile f(progress_path().Path(), B_READ_ONLY);
	BMessage m;
	const char* v;
	if (m.Unflatten(&f) == B_OK && m.FindString(key.c_str(), &v) == B_OK)
		return v;
	return "";
}

bool move_to_trash(const char* path)
{
	BPath trash;
	BEntry entry(path);
	if (find_directory(B_TRASH_DIRECTORY, &trash) != B_OK)
		return false;
	BNode(&entry).WriteAttr("_trk/original_path", B_STRING_TYPE, 0, path, strlen(path) + 1);
	BDirectory dir(trash.Path());
	return entry.MoveTo(&dir) == B_OK;
}

bool save_message(const BMessage& m, const BPath& path)
{
	BPath tmp(path);
	tmp.GetParent(&tmp);
	tmp.Append((std::string(path.Leaf()) + ".tmp").c_str());
	BFile f(tmp.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (f.InitCheck() != B_OK || m.Flatten(&f) != B_OK || f.Sync() != B_OK)
		return false;
	return BEntry(tmp.Path()).Rename(path.Leaf(), true) == B_OK;
}

void set_progress(const std::string& key, const std::string& value)
{
	BAutolock l(sProgressLock);
	BMessage m;
	BFile f(progress_path().Path(), B_READ_ONLY);
	// a file that's there but unreadable is kept as it is, not replaced by one key
	off_t size = 0;
	if (f.InitCheck() == B_OK && f.GetSize(&size) == B_OK && size > 0 && m.Unflatten(&f) != B_OK) {
		fprintf(stderr, "Hedgewars: can't read %s; not saving %s\n", progress_path().Path(), key.c_str());
		return;
	}
	m.RemoveName(key.c_str());
	m.AddString(key.c_str(), value.c_str());
	save_message(m, progress_path());
}

// ── one game ──
static void replace_first(std::string& s, const std::string& from, const std::string& to)
{
	size_t at = s.find(from);
	if (at != std::string::npos)
		s.replace(at, from.size(), to);
}

// HWForm::GetRecord: an interrupted game becomes a saved game, a finished one a demo
static std::string save_recording(std::string rec, bool finished)
{
	char stamp[32];
	time_t now = time(NULL);
	strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
	std::string path;
	if (finished) {
		replace_first(rec, "\x02TL", "\x02TD");
		replace_first(rec, "\x02TS", "\x02TD");
		replace_first(rec, "\x02TN", "\x02TD");
		path = BPath(gUserDir.Path(), (std::string("Demos/") + stamp + "." + kProto + ".hwd").c_str()).Path();
	} else {
		replace_first(rec, "\x02TL", "\x02TS");
		path = BPath(gUserDir.Path(), (std::string("Saves/") + stamp + "." + kProto + ".hws").c_str()).Path();
	}
	BFile f(path.c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (f.InitCheck() != B_OK || f.Write(rec.data(), rec.size()) != (ssize_t)rec.size())
		return "";
	return path;
}

Outcome run_game(const Game& g)
{
	Outcome out;
	int server = socket(AF_INET, SOCK_STREAM, 0);
	sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	socklen_t len = sizeof(sa);
	if (bind(server, (sockaddr*)&sa, sizeof(sa)) < 0 || listen(server, 1) < 0
		|| getsockname(server, (sockaddr*)&sa, &len) < 0) {
		close(server);
		out.error = "Could not open a local connection for the game. Try again, and restart the computer if it keeps happening.";
		return out;
	}
	std::string port = std::to_string(ntohs(sa.sin_port));
	std::string engine = BPath(gAppDir.Path(), "hwengine").Path();
	BRect screen = BScreen().Frame();
	std::string sw = std::to_string((int)screen.Width() + 1), sh = std::to_string((int)screen.Height() + 1);
	std::vector<std::string> args = {engine, "--internal", "--port", port,
		"--prefix", gDataDir.Path(), "--user-prefix", gUserDir.Path(), "--locale", "en.txt",
		"--fullscreen-width", sw, "--fullscreen-height", sh, "--width", "1024", "--height", "768"};
	if (!g.videoPrefix.empty()) {	// HWRecorder::getArguments
		args = {engine, "--internal", "--port", port, "--prefix", gDataDir.Path(),
			"--user-prefix", gUserDir.Path(), "--locale", "en.txt", "--width", "1280", "--height", "720",
			"--nosound", "--nomusic", "--volume", "0", "--recorder", std::to_string(g.videoFps), "1", g.videoPrefix,
			// shortcut: fixed 1280x720 and bit rate; settings for them when someone asks
			g.videoFormat, g.videoCodec, std::to_string(4 * 1048576), g.sound ? g.audioCodec : "no"};
	} else if (g.fullscreen)
		args.push_back("--fullscreen");
	if (!g.sound)
		args.push_back("--nosound");
	if (!g.music)
		args.push_back("--nomusic");
	args.insert(args.end(), g.args.begin(), g.args.end());
	std::vector<char*> argv;
	for (std::string& a : args)
		argv.push_back(&a[0]);
	argv.push_back(NULL);
	pid_t pid;
	if (posix_spawn(&pid, engine.c_str(), NULL, NULL, argv.data(), environ) != 0) {
		close(server);
		out.error = "Could not start the game engine. Reinstall the Hedgewars package.";
		return out;
	}

	pollfd pfd = {server, POLLIN, 0};
	int client = -1;
	for (int waited = 0; waited < 30000 && client < 0; waited += 200) {	// 30 s for it to connect
		if (poll(&pfd, 1, 200) > 0)
			client = accept(server, NULL, NULL);
		else if (waitpid(pid, NULL, WNOHANG) == pid) {
			close(server);
			out.error = "The game engine quit before it started. Start Hedgewars from Terminal to see why.";
			return out;
		}
	}
	close(server);
	if (client < 0) {
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
		out.error = "The game engine did not answer within 30 seconds. Try again.";
		return out;
	}

	// everything sent and every game message, as the Qt frontend records it
	std::string record;
	bool keepRecord = g.replay.find("\x02TD") == std::string::npos && g.videoPrefix.empty();	// not when watching
	char state = 0;	// 'q' finished, 'Q' interrupted, 'H' halted
	auto send_all = [&](const std::string& data) {
		record += data;
		for (size_t sent = 0; sent < data.size();) {
			ssize_t n = send(client, data.data() + sent, data.size() - sent, 0);
			if (n <= 0)
				break;
			sent += n;
		}
	};
	if (!g.replay.empty())
		send_all(g.replay);

	std::string in, toNet;
	char chunk[4096];
	ssize_t got;
	pollfd fds[2] = {{client, POLLIN, 0}, {g.netIn, POLLIN, 0}};
	while (poll(fds, g.netIn >= 0 ? 2 : 1, -1) > 0) {
		if (fds[1].revents) {	// from the other players
			if ((got = read(g.netIn, chunk, sizeof(chunk))) > 0)
				send_all(std::string(chunk, got));
			else
				fds[1].fd = -1;
			continue;
		}
		if ((got = recv(client, chunk, sizeof(chunk), 0)) <= 0)
			break;
		in.append(chunk, got);
		while (!in.empty() && in.size() >= (size_t)(uint8)in[0] + 1) {
			std::string raw = in.substr(0, (uint8)in[0] + 1);
			in.erase(0, raw.size());
			// SendIPC appends two bytes of game ticks; stats ('i') are sent raw, without them
			std::string msg = raw.size() > 1 && raw[1] == 'i' ? raw.substr(1)
				: raw.substr(1, raw.size() > 3 ? raw.size() - 3 : 0);
			std::string reply;
			switch (msg.empty() ? 0 : msg[0]) {
				case '?': add(reply, "!"); break;
				case 'C': reply = g.config; break;
				case 'E': out.engineError = msg.substr(1); break;
				case 'q': case 'Q': case 'H': state = msg[0]; break;
				case 'm': keepRecord = false; break;
				case 'i': out.stats.push_back(msg.substr(1)); break;
				case 'p':	// the recorder's progress; in a game, a target point
					if (g.videoPrefix.empty()) {
						record += raw;
						toNet += raw;
					} else if (g.progress && msg.size() >= 3)
						g.progress(((uint8)msg[1] * 256 + (uint8)msg[2]) / 10000.f);
					break;
				case 's':
					if (g.chat)
						g.chat(msg.substr(1));
					break;
				case 'b': case 'W': case '~': break;
				case 'V': case 'v': {
					if (msg == "v" && !g.videoPrefix.empty()) {	// the recorder finished
						out.encoded = true;
						break;
					}
					// campaign and mission variables: "?name" asks, "!name value" stores
					std::string body = msg.size() > 1 ? msg.substr(2) : "";
					body = body.substr(0, body.find('\0'));
					if (msg.size() > 1 && msg[1] == '?')
						add(reply, std::string(1, msg[0]) + "." + get_progress(g.varPrefix + body));
					else if (msg.size() > 1 && msg[1] == '!' && body.find(' ') != std::string::npos)
						set_progress(g.varPrefix + body.substr(0, body.find(' ')), body.substr(body.find(' ') + 1));
					break;
				}
				default:
					record += raw;
					toNet += raw;
			}
			if (!reply.empty())
				send_all(reply);
		}
		if (g.toNet && !toNet.empty())
			g.toNet(toNet);
		toNet.clear();
	}
	close(client);
	int status;
	waitpid(pid, &status, 0);
	out.finished = state == 'q';
	out.record = record;
	if (keepRecord && state)
		out.saved = save_recording(record, out.finished);
	return out;
}
