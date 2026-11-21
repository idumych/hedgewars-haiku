# Hedgewars for Haiku, without SDL

The engine runs on the Haiku API directly. `libhwhaiku` implements the
SDL2, SDL_ttf, SDL_mixer, SDL_image and SDL_net entry points that
`hedgewars/SDLh.pas` and `misc/libphyslayer` import:

| Part | Haiku API |
|---|---|
| Window, OpenGL, input | `BApplication`, `BWindow`, `BGLView` (Mesa) |
| Text | `BFont`, loaded from the game's font files, drawn into a `BBitmap` |
| Images | Translation Kit |
| Sound and music | `BMediaFile` decoding, mixed in a `BSoundPlayer` |
| Joysticks | `BJoystick` |
| Link to the launcher | POSIX sockets |
| Video encoding | upstream's `avwrapper` on the system's ffmpeg 6 (the libraries Haiku's media plugin uses) |

The engine is compiled from C: upstream's pas2c converts `hedgewars/*.pas`
to `engine-c/` (with `tools/gen-engine-c.sh`, on a system with GHC), and
clang compiles it with upstream's small stand-in for Free Pascal's runtime
(`project_files/hwc/rtl`). The Pascal stays the source; the C is regenerated
from it and committed, so building on Haiku needs neither GHC nor Free Pascal.
The C engine plays exactly like the Pascal one: `make compare` runs upstream's
Lua tests on both and fails on any difference in their output. Four small
changes let pas2c handle this engine version: `uConsts`' initialization
section becomes a procedure under `PAS2C`, `Int8`/`PtrUInt` are added to
pas2c's types, and pas2c imports `Control.Monad` (needed with newer GHC).
For video recording, two fixes to pas2c's runtime (`fileio.c`): `fileSize`
seeked to the end and stayed there, and `eof` was true only after a failed
read; and `avwrapper.c` converts the game's sound to the sample format and
rate the encoder takes (AAC, Opus), and uses a named channel layout.
Haiku's Media Kit was tried first for encoding: on R1/beta6 its writer
fails on every audio track (AAC and AC-3 don't open, FLAC and PCM writes
fail), so the engine uses ffmpeg directly.

A native launcher (`launcher/`) replaces the Qt
frontend's local pages, in tabs:

- **Match**: opponent (the computer at five levels, or a second player),
  hedgehogs per team, scheme and weapon set (the predefined ones and your
  own, each with an editor window), game styles, generated, hand-drawn or
  fixed maps, theme, and for generated maps the map size, maze style,
  feature size and seed
- **Missions**: training, challenges and scenarios, with their pictures and
  descriptions; won missions are marked
- **Campaigns**: the missions your progress has unlocked
- **Statistics** after a finished game or demo: winner, ranking, awards,
  health graph
- **Recordings**: interrupted games are kept as saved games and finished
  ones as demos, the way the Qt frontend records them; play them again or
  move them to the Trash
- **Videos**: press R in a game to start and stop recording; after the game a
  second engine replays the recorded stretch off screen and encodes it to
  `Videos/` at 1280×720, as MP4 (MPEG-4, AAC), WebM (VP8, Vorbis) or Ogg
  (Theora, Vorbis), at 24, 30 or 60 frames/s. The list shows each video's
  picture and map; Play opens it in MediaPlayer
- **Team**: several teams, each with name, player (human or computer
  level), flag, grave, fort, voice, hedgehog names and hats, and its own
  keys; kept as `Teams/<name>.hwt`, the Qt frontend's files

and, from the Hedgewars menu, two windows:

- **Network game**: connect to a Hedgewars server (nickname, and the password
  of a registered nickname or a locked room when asked), the lobby's rooms,
  players and chat, then a room: add or remove your team, chat, ready, and
  for the room's host the game style, scheme, weapons, map, theme, each
  team's hedgehogs and colour, and Start game. Rooms can have a password;
  the **Room** menu renames the room, restricts joins, team additions or
  unregistered players, kicks a player or makes them host (also on a right
  click in the player list). For server admins, the **Server** menu kicks
  or bans a lobby player and opens **Administration…**: the server's
  messages for new and old versions, the latest protocol number, the
  accounts cache and the ban list (add by address or nickname, remove). The engine plays the room's game and relays its moves
  through the server; the statistics follow, as after a local game.
  Implemented of the protocol (`launcher/NetProto.cpp`, `Net.cpp`):
  NICK, PROTO, CONNECTED, PING/PONG, ERROR, WARNING, BYE, NOTICE (nickname
  taken, room password), ASKPASSWORD/PASSWORD/SERVER_AUTH, LOBBY:JOINED/LEFT,
  ROOMS, ROOM ADD/UPD/DEL, SERVER_MESSAGE, CHAT, INFO, CLIENT_FLAGS (ready,
  host, playing), CREATE_ROOM, JOIN_ROOM, JOINING, JOINED, LEFT, PART,
  KICKED, ROOMABANDONED, ADD_TEAM, TEAM_ACCEPTED, REMOVE_TEAM, TEAM_COLOR,
  HH_NUM, ROOM_NAME, TOGGLE_RESTRICT_JOINS, TOGGLE_RESTRICT_TEAMS,
  TOGGLE_REGISTERED_ONLY, KICK, DELEGATE, BAN, BANIP, BANNICK, BANLIST, UNBAN,
  GET_SERVER_VAR/SERVER_VARS, SET_SERVER_VAR, CLEAR_ACCOUNTS_CACHE, CFG (all of FULLMAPCONFIG, MAP, MAPGEN, MAZE_SIZE, SEED, TEMPLATE,
  FEATURE_SIZE, THEME, SCRIPT, SCHEME, AMMO), TOGGLE_READY, START_GAME,
  RUN_GAME, EM, ROUND_FINISHED, ROUNDFINISHED, QUIT. Not implemented:
  FOLLOW, INFO requests, watching games in progress (REPLAY_START),
  REDIRECT, TLS, team chat, hand-drawn maps (DRAWNMAP), the MOTD preview
  and filters of the room list.
- **Downloadable content**: the add-ons listed on hedgewars.org (with
  libcurl), downloaded into `~/config/settings/Hedgewars/Data`, where the
  engine mounts them.

**Settings…** (Alt+,) sets the language, window size, full screen,
quality, weapon descriptions, frame rate, damage display, sound, music,
volume and the keys (press a key or mouse button to bind it). They're kept
in `settings.ini` with the Qt frontend's keys; the engine reads the key
bindings from there and from the team files itself. **Edit drawing…**
draws a hand-drawn map (pencil, rectangle, ellipse, eraser, line width,
undo, clear; opens and saves the Qt frontend's `.hwmap` files).

Settings, teams, recordings and mission progress live in
`~/config/settings/Hedgewars`; your schemes and weapon sets in its
`Schemes/Game/*.hwg` and `Schemes/Ammo/*.hwa`, the Qt frontend's files.

## Build

On Haiku R1/beta6 (x86_64), from the top of the tree (the package also runs on Renku OS):

    pkgman install llvm19_clang mesa_devel glu_devel ffmpeg6_devel rust_bin curl_devel openssl3_devel
    make -f haiku/Makefile                 # haiku/build/Hedgewars/
    make -f haiku/Makefile compare         # C engine against Pascal (needs fpc)
    sh haiku/package/build-hpkg.sh         # haiku/build/hedgewars-*.hpkg

`launcher/schemes.h` is generated from the Qt frontend's scheme list:
`python3 haiku/tools/gen-schemes.py > haiku/launcher/schemes.h`.

`make -f haiku/Makefile test` runs the tests: the surface (blit and clipping),
statistics, server protocol, add-on list, INI file, drawn map and scheme file code on any system,
and on Haiku also the launcher's progress file.

`HWHAIKU_DEBUG=1` makes the engine print every input event it receives.

## Status

| | State |
|---|---|
| Engine, physics, AI, Lua, map generation | works, built from C (pas2c); upstream Lua tests 6 of 7 pass, the same as the Pascal engine, with identical output |
| Rendering (llvmpipe in a VM) | works |
| Keyboard and mouse | works; keys arrive as SDL scancodes |
| Sound effects, voices, music | works (checked from the VM's captured audio) |
| Local game: you against the computer, or two players on one computer | works, from the launcher; checked with Default and Pro Mode, random and fixed (Bath) maps |
| Training missions | works (Basic Movement: the hedgehog walks, the mission advances) |
| Saved games and demos | works: quitting a game keeps it and playing it resumes where it stopped; a finished game is kept as a demo and plays back to its end |
| Statistics after a game | works (winner, ranking, awards, health graph; pictures from the Qt frontend) |
| Campaigns, challenges, scenarios, styles | listed and set up like the Qt frontend; not each one played through |
| `.hpkg` | installs and plays: app in `apps/Hedgewars`, data in `data/hedgewars`, Deskbar entry |
| Renku OS (x86_64, build00073, installed to disk in QEMU) | the same `.hpkg`, installed in the home folder: launcher, a match, sound |
| Joysticks | written, not tested (no joystick in the VM) |
| Settings window | works: window size 800 × 600 and the frame rate showed in the game; German mission names in the launcher and `--locale de.txt` to the engine; quality, volume, damage display and tooltips are passed as the Qt frontend passes them, not each checked by eye |
| Key bindings | works: Walk left set to A in Settings walks with A and not with Left; a team's own key (J) works on its turn while the other team keeps A |
| Teams | works: several teams in `Teams/*.hwt`, the old launcher's team carried over; the Qt frontend's achievements in a team file are kept, not used |
| Hand-drawn maps | works: drawn with pencil, rectangle and eraser, saved as `.hwmap`, played as a match on that land; opening a `.hwmap` from the Qt frontend is checked by the host test (decodes and encodes back to the same bytes) |
| Video recording | works: a clip recorded with R in a match became an mp4 that MediaPlayer plays with sound; WebM at 24 frames/s checked the same way; size and bit rate are fixed |
| Network play | works: a net game between Haiku R1/beta6 and Renku (two QEMU machines) through the repo's `hedgewars-server` on the host: lobby, room, teams, hedgehog count, chat in the lobby, the room and the game, the game in sync to its end (the same statistics on both) and a demo kept on each; the room password and taken-nickname prompts checked; the registered-nickname password exchange is written, not tested (the local server has no accounts) |
| Network room and admin commands | works, on Renku against the local server with a Python client as the second player: creating a password room (joining without it is refused), renaming, the three restriction toggles (the server refused a join, a team and an unregistered player), game style, a team's hedgehogs and colour, kick from the room, make host; as admin (the server makes loopback clients admins, and QEMU's network connects from loopback): server messages and protocol number read and set, a nickname and an address ban added and removed, a player banned and kicked from the server. Clear accounts cache is sent but not checked (the local server has no accounts) |
| Downloadable content | works: the list loads from hedgewars.org and a package (Ready Timer) lands in the Data folder, on Renku; the launcher's own menus don't list what add-ons bring (the engine uses them) |
| Scheme and weapon set editors | works: a new scheme (turn time 20 s) and a copied weapon set (2 bazookas) were made in the editors, saved as `.hwg`/`.hwa`, and the engine got them (turn timer counted from 20; `HEDGEWARS_LOG_CONFIG=1` prints the config sent); copy and delete checked |
| Map size, maze style, feature size, seed | works for a maze ("Large islands" and a typed seed reached the engine: it logged `large_islands`); WFC is offered but not played |
| App icon | HVIF, from `tools/make-icon.py` |
| arm64 | not built: Renku OS arm64 (hrev60072_940) has no OpenGL (no Mesa in its ports repository, no OpenGL kit in the system), which the engine draws with; Rust has no aarch64 Haiku target either (it would need a custom target and a nightly toolchain). Free Pascal is no longer needed |

## Interface guidelines

The launcher follows the Haiku HIG: sentence case, action-labelled alerts that
say how to fix the problem, the window's size and place remembered, a default
button on each tab, dates in the system's format, every control reachable
with Tab, and no clipping with a 15 pt system font. Where it deviates:

- The Settings window has no proxy or network settings (no network play),
  no video recording, stereoscopy or zoom, and keeps the Qt frontend's
  `settings.ini` rather than a flattened `BMessage`, because the engine reads
  its key bindings from that file.
- Scheme, weapon set and map names keep the game's capitals ("Pro Mode"):
  they are names, and other Hedgewars players know them that way.
- Flags, graves and forts are listed by their file names, without pictures.
- Mission and campaign lists follow the game's order, not the alphabet.
- No Help menu: Hedgewars has no offline manual.
- The network game's lobby and room have no default button: Enter sends
  the line typed in their chat fields.

Known issue: `tests/lua/luaAPI/gravity_get_set.lua` misses its tolerance
by 0.5 px, the same with and without video, so it's in engine physics,
not in this port.
