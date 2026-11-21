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

`SDLh.pas` points its five library names at `libhwhaiku` on Haiku; nothing
else in the engine changes. A native launcher (`launcher/`) replaces the Qt
frontend for local games.

## Build

On Haiku R1/beta6 (x86_64), from the top of the tree:

    pkgman install fpc mesa_devel glu_devel rust_bin
    make -f haiku/Makefile                 # haiku/build/Hedgewars/
    sh haiku/package/build-hpkg.sh         # haiku/build/hedgewars-*.hpkg

`HWHAIKU_DEBUG=1` makes the engine print every input event it receives.

## Status

| | State |
|---|---|
| Engine, physics, AI, Lua, map generation | works; upstream Lua tests 6 of 7 pass, with and without video |
| Rendering (llvmpipe in a VM) | works |
| Keyboard and mouse | works; keys arrive as SDL scancodes |
| Sound effects, voices, music | works (checked from the VM's captured audio) |
| Local game: you against the computer, or two players on one computer | works, from the launcher |
| `.hpkg` | installs and plays: app in `apps/Hedgewars`, data in `data/hedgewars`, Deskbar entry |
| Joysticks | written, not tested (no joystick in the VM) |
| Schemes, weapon sets, team editor, missions, campaigns, training, saved games, network play | not in the launcher yet (the Qt frontend's other pages) |
| App icon | none yet |
| arm64 | not built |

Known issue: `tests/lua/luaAPI/gravity_get_set.lua` misses its tolerance
by 0.5 px, the same with and without video, so it's in engine physics,
not in this port.
