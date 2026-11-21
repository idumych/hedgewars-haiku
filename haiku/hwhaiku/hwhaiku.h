// libhwhaiku: the subset of the SDL2 C ABI that hedgewars' engine (SDLh.pas)
// and libphyslayer import, implemented directly on the Haiku API.
// Struct layouts must match hedgewars/SDLh.pas ({$PACKRECORDS C}).
#ifndef HWHAIKU_H
#define HWHAIKU_H

#include <stdint.h>
#include <stddef.h>

#define HW_API extern "C" __attribute__((visibility("default")))

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int32_t Sint32;
typedef int64_t Sint64;

struct SDL_Rect { int x, y, w, h; };
struct SDL_Color { Uint8 r, g, b, a; };

struct SDL_PixelFormat {
	Uint32 format;
	void* palette;
	Uint8 BitsPerPixel, BytesPerPixel, padding[2];
	Uint32 Rmask, Gmask, Bmask, Amask;
	Uint8 Rloss, Gloss, Bloss, Aloss;
	Uint8 Rshift, Gshift, Bshift, Ashift;
	int refcount;
	SDL_PixelFormat* next;
};

struct SDL_Surface {
	Uint32 flags;
	SDL_PixelFormat* format;
	int w, h, pitch;
	void* pixels;
	void* userdata;
	int locked;
	void* lock_data;
	SDL_Rect clip_rect;
	void* map;			// we keep the colour key here: (void*)(key | 1ull<<32) when set
	int refcount;
};

struct SDL_RWops {
	Sint64 (*size)(SDL_RWops*);
	Sint64 (*seek)(SDL_RWops*, Sint64, int);
	size_t (*read)(SDL_RWops*, void*, size_t, size_t);
	size_t (*write)(SDL_RWops*, const void*, size_t, size_t);
	int (*close)(SDL_RWops*);
	Uint32 type;
	union {
		struct { int autoclose; void* fp; } stdio;
		struct { Uint8 *base, *here, *stop; } mem;
		struct { void *data1, *data2; } unknown;
	} hidden;
};

enum { RW_SEEK_SET = 0, RW_SEEK_CUR = 1, RW_SEEK_END = 2 };

// Events: only the members the engine reads. The Pascal record is larger
// (it has a shortstring arm), so writing up to 56 bytes is safe.
enum {
	SDL_QUIT = 0x100, SDL_WINDOWEVENT = 0x200, SDL_KEYDOWN = 0x300, SDL_KEYUP,
	SDL_TEXTINPUT = 0x303, SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN,
	SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL, SDL_JOYAXISMOTION = 0x600,
	SDL_JOYHATMOTION = 0x602, SDL_JOYBUTTONDOWN, SDL_JOYBUTTONUP
};
enum {
	SDL_WINDOWEVENT_SHOWN = 1, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_EXPOSED,
	SDL_WINDOWEVENT_MOVED, SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED,
	SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_RESTORED,
	SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_LEAVE, SDL_WINDOWEVENT_FOCUS_GAINED,
	SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_CLOSE
};

struct SDL_Keysym { Sint32 scancode, sym; Uint16 mod; Uint32 unused; };

union SDL_Event {
	Uint32 type;
	struct { Uint32 type, timestamp, windowID; Uint8 event, p1, p2, p3; Sint32 data1, data2; } window;
	struct { Uint32 type, timestamp, windowID; Uint8 state, repeat, p2, p3; SDL_Keysym keysym; } key;
	struct { Uint32 type, timestamp, windowID; char text[32]; } text;
	struct { Uint32 type, timestamp, windowID, which, state; Sint32 x, y, xrel, yrel; } motion;
	struct { Uint32 type, timestamp, windowID, which; Uint8 button, state, p1, p2; Sint32 x, y; } button;
	struct { Uint32 type, timestamp, windowID, which; Sint32 x, y; } wheel;
	struct { Uint32 type, timestamp; Sint32 which; Uint8 axis, p1, p2, p3; int16_t value; Uint16 p4; } jaxis;
	struct { Uint32 type, timestamp; Sint32 which; Uint8 hat, value, p1, p2; } jhat;
	struct { Uint32 type, timestamp; Sint32 which; Uint8 button, state, p1, p2; } jbutton;
	Uint8 padding[56];
};

// internal helpers shared between the source files
HW_API int SDL_SetError(const char* fmt, ...);
void hw_push_event(const SDL_Event& e);
Uint32 hw_ticks();
// whole RWops contents in a malloc'd buffer; closes it when freesrc
void* hw_read_all(SDL_RWops* rw, size_t* size, bool freesrc);
SDL_Surface* hw_create_rgba(int w, int h);	// 32 bpp, R in the low byte
// BBitmap and BFont need an app_server connection, even without a window
bool hw_start_app();

extern Uint8 hw_keystate[512];

#endif
