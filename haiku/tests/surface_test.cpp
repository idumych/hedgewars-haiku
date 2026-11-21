// Host test for hwhaiku/surface.cpp: blit clipping, alpha blending, colour key.
// Builds anywhere: make -f haiku/Makefile test
#include "hwhaiku.h"

#include <assert.h>
#include <stdio.h>

HW_API int SDL_SetError(const char*, ...) { return -1; }	// core.cpp's, which needs Haiku
HW_API SDL_Surface* SDL_CreateRGBSurface(Uint32, int, int, int, Uint32, Uint32, Uint32, Uint32);
HW_API void SDL_FreeSurface(SDL_Surface*);
HW_API int SDL_UpperBlit(SDL_Surface*, const SDL_Rect*, SDL_Surface*, SDL_Rect*);
HW_API int SDL_SetColorKey(SDL_Surface*, Uint32, Uint32);
HW_API SDL_Surface* SDL_ConvertSurface(SDL_Surface*, const SDL_PixelFormat*, Uint32);
HW_API SDL_PixelFormat* SDL_AllocFormat(Uint32);
HW_API void SDL_FreeFormat(SDL_PixelFormat*);

static Uint32& px(SDL_Surface* s, int x, int y) { return ((Uint32*)s->pixels)[y * s->w + x]; }

// a 4x4 source whose pixel (x, y) is opaque with red = 10x + y + 1
static SDL_Surface* numbered()
{
	SDL_Surface* s = hw_create_rgba(4, 4);
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 4; x++)
			px(s, x, y) = 0xFF000000u | (10 * x + y + 1);
	return s;
}

static void check_rect(const SDL_Rect& r, int x, int y, int w, int h)
{
	assert(r.x == x && r.y == y && r.w == w && r.h == h);
}

int main()
{
	SDL_Surface* src = numbered();

	{	// partly off the top left: dst (0..2) gets src (1..3)
		SDL_Surface* dst = hw_create_rgba(4, 4);
		SDL_Rect d = {-1, -1, 0, 0};
		SDL_UpperBlit(src, NULL, dst, &d);
		check_rect(d, 0, 0, 3, 3);
		assert(px(dst, 0, 0) == px(src, 1, 1));
		assert(px(dst, 2, 2) == px(src, 3, 3));
		assert(px(dst, 3, 3) == 0);
		SDL_FreeSurface(dst);
	}
	{	// partly off the bottom right: only the corner lands
		SDL_Surface* dst = hw_create_rgba(4, 4);
		SDL_Rect d = {3, 3, 0, 0};
		SDL_UpperBlit(src, NULL, dst, &d);
		check_rect(d, 3, 3, 1, 1);
		assert(px(dst, 3, 3) == px(src, 0, 0));
		assert(px(dst, 2, 2) == 0);
		SDL_FreeSurface(dst);
	}
	{	// a source rect hanging off the source moves the destination with it
		SDL_Surface* dst = hw_create_rgba(8, 8);
		SDL_Rect s = {-2, 0, 10, 10}, d = {0, 0, 0, 0};
		SDL_UpperBlit(src, &s, dst, &d);
		check_rect(d, 2, 0, 4, 4);
		assert(px(dst, 2, 0) == px(src, 0, 0));
		assert(px(dst, 1, 0) == 0);
		SDL_FreeSurface(dst);
	}
	{	// entirely outside: nothing drawn, empty rect back
		SDL_Surface* dst = hw_create_rgba(4, 4);
		SDL_Rect d = {10, 0, 0, 0};
		SDL_UpperBlit(src, NULL, dst, &d);
		assert(d.w == 0 && d.h == 0);
		for (int i = 0; i < 16; i++)
			assert(((Uint32*)dst->pixels)[i] == 0);
		SDL_FreeSurface(dst);
	}
	{	// half-transparent red over opaque blue
		SDL_Surface* a = hw_create_rgba(1, 1);
		SDL_Surface* dst = hw_create_rgba(1, 1);
		px(a, 0, 0) = 0x800000FFu;	// R=255, A=128
		px(dst, 0, 0) = 0xFFFF0000u;	// B=255, A=255
		SDL_UpperBlit(a, NULL, dst, NULL);
		Uint32 p = px(dst, 0, 0);
		assert((p & 0xFF) == 128 && ((p >> 16) & 0xFF) == 127 && (p >> 24) == 255);
		SDL_FreeSurface(a);
		SDL_FreeSurface(dst);
	}
	{	// colour-keyed pixels are skipped, and the key survives a conversion
		SDL_Surface* k = numbered();
		SDL_SetColorKey(k, 1, px(k, 0, 0));
		SDL_Surface* dst = hw_create_rgba(4, 4);
		SDL_UpperBlit(k, NULL, dst, NULL);
		assert(px(dst, 0, 0) == 0 && px(dst, 1, 0) == px(k, 1, 0));
		SDL_PixelFormat* argb = SDL_AllocFormat((1u << 28) | (6u << 24) | (3u << 20) | (6u << 16) | (32u << 8) | 4);
		SDL_Surface* c = SDL_ConvertSurface(k, argb, 0);
		assert(px(c, 1, 0) == (0xFF000000u | (px(k, 1, 0) << 16)));	// red moved to bits 16..23
		SDL_Surface* dst2 = hw_create_rgba(4, 4);
		SDL_UpperBlit(c, NULL, dst2, NULL);
		assert(px(dst2, 0, 0) == 0 && px(dst2, 1, 0) == px(k, 1, 0));
		SDL_FreeFormat(argb);
		SDL_FreeSurface(c);
		SDL_FreeSurface(k);
		SDL_FreeSurface(dst);
		SDL_FreeSurface(dst2);
	}

	SDL_FreeSurface(src);
	puts("surface_test: all passed");
	return 0;
}
