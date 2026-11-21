/* The part of SDL.h that libphyslayer uses, served by libhwhaiku. */
#ifndef HW_SDL_H
#define HW_SDL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t Sint64;
typedef uint32_t Uint32;

#define SDLCALL
#define SDL_VERSION_ATLEAST(x, y, z) ((x) <= 2)
#define RW_SEEK_SET 0
#define RW_SEEK_CUR 1
#define RW_SEEK_END 2

typedef struct SDL_RWops {
	Sint64 (*size)(struct SDL_RWops*);
	Sint64 (*seek)(struct SDL_RWops*, Sint64, int);
	size_t (*read)(struct SDL_RWops*, void*, size_t, size_t);
	size_t (*write)(struct SDL_RWops*, const void*, size_t, size_t);
	int (*close)(struct SDL_RWops*);
	Uint32 type;
	union {
		struct { void *data1, *data2; } unknown;
		struct { void *base, *here, *stop; } mem;
	} hidden;
} SDL_RWops;

SDL_RWops* SDL_AllocRW(void);
void SDL_FreeRW(SDL_RWops*);
int SDL_SetError(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
