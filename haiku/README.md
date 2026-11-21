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
  hedgehogs per team, the 16 predefined schemes, the weapon sets, game
  styles, generated or fixed maps, theme
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
  `Videos/` (mp4: MPEG-4 video, AAC sound, 1280×720, 30 frames/s). The list
  shows each video's picture and map; Play opens it in MediaPlayer
- **Team**: name, flag, grave, fort, voice, hedgehog names and hats

Settings, the team, recordings and mission progress live in
`~/config/settings/Hedgewars`.

## Build

On Haiku R1/beta6 (x86_64), from the top of the tree (the package also runs on Renku OS):

    pkgman install llvm19_clang mesa_devel glu_devel ffmpeg6_devel rust_bin
    make -f haiku/Makefile                 # haiku/build/Hedgewars/
    make -f haiku/Makefile compare         # C engine against Pascal (needs fpc)
    sh haiku/package/build-hpkg.sh         # haiku/build/hedgewars-*.hpkg

`launcher/schemes.h` is generated from the Qt frontend's scheme list:
`python3 haiku/tools/gen-schemes.py > haiku/launcher/schemes.h`.

`make -f haiku/Makefile test` runs the tests: the surface (blit and clipping)
and statistics code on any system, and on Haiku also the launcher's progress file.

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
| Video recording | works: a clip recorded with R in a match became an mp4 that MediaPlayer plays with sound; the format, size and frame rate are fixed |
| Scheme and weapon set editors, network play | not in the launcher (the predefined schemes and weapon sets are) |
| App icon | HVIF, from `tools/make-icon.py` |
| arm64 | not built: Renku OS arm64 (hrev60072_940) has no OpenGL (no Mesa in its ports repository, no OpenGL kit in the system), which the engine draws with; Rust has no aarch64 Haiku target either (it would need a custom target and a nightly toolchain). Free Pascal is no longer needed |

## Interface guidelines

The launcher follows the Haiku HIG: sentence case, action-labelled alerts that
say how to fix the problem, the window's size and place remembered, a default
button on each tab, dates in the system's format, every control reachable
with Tab, and no clipping with a 15 pt system font. Where it deviates:

- **Settings** is a menu of three on/off items rather than a "Settings…"
  window, since that is all there is to set.
- Scheme, weapon set and map names keep the game's capitals ("Pro Mode"):
  they are names, and other Hedgewars players know them that way.
- Flags, graves and forts are listed by their file names, without pictures.
- Mission and campaign lists follow the game's order, not the alphabet.
- No Help menu: Hedgewars has no offline manual.

Known issue: `tests/lua/luaAPI/gravity_get_set.lua` misses its tolerance
by 0.5 px, the same with and without video, so it's in engine physics,
not in this port.
