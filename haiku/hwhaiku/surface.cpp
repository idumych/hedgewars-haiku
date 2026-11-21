// Software surfaces. The engine only uses 32-bit RGBA ones, so that is all
// this supports; 24-bit input from images is expanded on load.
#include "hwhaiku.h"

#include <stdlib.h>
#include <string.h>

static const Uint32 kPixelFormatABGR8888 = (1u << 28) | (6u << 24) | (7u << 20) | (6u << 16) | (32u << 8) | 4;

static int lowest_bit(Uint32 m) { return m ? __builtin_ctz(m) : 0; }

static SDL_PixelFormat* make_format(Uint32 fmt, Uint32 r, Uint32 g, Uint32 b, Uint32 a)
{
	SDL_PixelFormat* f = (SDL_PixelFormat*)calloc(1, sizeof(SDL_PixelFormat));
	f->format = fmt;
	f->BitsPerPixel = 32;
	f->BytesPerPixel = 4;
	f->Rmask = r; f->Gmask = g; f->Bmask = b; f->Amask = a;
	f->Rshift = lowest_bit(r); f->Gshift = lowest_bit(g);
	f->Bshift = lowest_bit(b); f->Ashift = lowest_bit(a);
	f->Aloss = a ? 0 : 8;
	f->refcount = 1;
	return f;
}

// SDL_PIXELFORMAT_* packed 32-bit orders: 3 ARGB, 4 RGBA, 7 ABGR, 8 BGRA
static bool masks_for_order(int order, Uint32* r, Uint32* g, Uint32* b, Uint32* a)
{
	switch (order) {
		case 3: *a = 0xFF000000; *r = 0xFF0000; *g = 0xFF00; *b = 0xFF; return true;
		case 4: *r = 0xFF000000; *g = 0xFF0000; *b = 0xFF00; *a = 0xFF; return true;
		case 7: *a = 0xFF000000; *b = 0xFF0000; *g = 0xFF00; *r = 0xFF; return true;
		case 8: *b = 0xFF000000; *g = 0xFF0000; *r = 0xFF00; *a = 0xFF; return true;
	}
	return false;
}

HW_API SDL_PixelFormat* SDL_AllocFormat(Uint32 fmt)
{
	Uint32 r, g, b, a;
	if (((fmt >> 8) & 0xFF) != 32 || !masks_for_order((fmt >> 20) & 0xF, &r, &g, &b, &a)) {
		hw_set_error("unsupported pixel format %x", fmt);
		return NULL;
	}
	return make_format(fmt, r, g, b, a);
}

HW_API void SDL_FreeFormat(SDL_PixelFormat* f) { free(f); }

HW_API int SDL_PixelFormatEnumToMasks(Uint32 fmt, int* bpp, Uint32* r, Uint32* g, Uint32* b, Uint32* a)
{
	*bpp = 32;
	return masks_for_order((fmt >> 20) & 0xF, r, g, b, a);
}

static SDL_Surface* wrap(void* pixels, int w, int h, int pitch, Uint32 r, Uint32 g, Uint32 b, Uint32 a, Uint32 flags)
{
	SDL_Surface* s = (SDL_Surface*)calloc(1, sizeof(SDL_Surface));
	s->flags = flags;
	s->format = make_format(r == 0xFF && a == 0xFF000000 ? kPixelFormatABGR8888 : 0, r, g, b, a);
	s->w = w;
	s->h = h;
	s->pitch = pitch;
	s->pixels = pixels;
	s->clip_rect = {0, 0, w, h};
	s->refcount = 1;
	return s;
}

HW_API SDL_Surface* SDL_CreateRGBSurface(Uint32, int w, int h, int depth, Uint32 r, Uint32 g, Uint32 b, Uint32 a)
{
	if (depth != 32) {
		hw_set_error("only 32-bit surfaces are supported");
		return NULL;
	}
	if (!r && !g && !b) { r = 0xFF; g = 0xFF00; b = 0xFF0000; }
	void* px = calloc((size_t)w * h, 4);
	return px ? wrap(px, w, h, w * 4, r, g, b, a, 0) : NULL;
}

HW_API SDL_Surface* SDL_CreateRGBSurfaceFrom(void* px, int w, int h, int depth, int pitch, Uint32 r, Uint32 g, Uint32 b, Uint32 a)
{
	if (depth != 32) {
		hw_set_error("only 32-bit surfaces are supported");
		return NULL;
	}
	return wrap(px, w, h, pitch, r, g, b, a, 1);	// SDL_PREALLOC: pixels aren't ours
}

SDL_Surface* hw_create_rgba(int w, int h)
{
	return SDL_CreateRGBSurface(0, w, h, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000);
}

HW_API void SDL_FreeSurface(SDL_Surface* s)
{
	if (!s || --s->refcount > 0)
		return;
	if (!(s->flags & 1))
		free(s->pixels);
	free(s->format);
	free(s);
}

HW_API int SDL_LockSurface(SDL_Surface*) { return 0; }
HW_API void SDL_UnlockSurface(SDL_Surface*) {}

static inline Uint32* row(SDL_Surface* s, int y) { return (Uint32*)((Uint8*)s->pixels + (size_t)y * s->pitch); }
static inline bool has_key(SDL_Surface* s) { return ((uintptr_t)s->map >> 32) & 1; }
static inline Uint32 key_of(SDL_Surface* s) { return (Uint32)(uintptr_t)s->map; }

HW_API int SDL_SetColorKey(SDL_Surface* s, Uint32 flag, Uint32 key)
{
	s->map = flag ? (void*)(uintptr_t)(key | (1ull << 32)) : NULL;
	return 0;
}

// SDL1 leftover: the engine only calls it to turn per-surface alpha off
HW_API int SDL_SetAlpha(SDL_Surface*, Uint32, Uint32) { return 0; }

static inline void unpack(const SDL_PixelFormat* f, Uint32 p, Uint8* r, Uint8* g, Uint8* b, Uint8* a)
{
	*r = (p & f->Rmask) >> f->Rshift;
	*g = (p & f->Gmask) >> f->Gshift;
	*b = (p & f->Bmask) >> f->Bshift;
	*a = f->Amask ? (p & f->Amask) >> f->Ashift : 255;
}

static inline Uint32 pack(const SDL_PixelFormat* f, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	return ((Uint32)r << f->Rshift) | ((Uint32)g << f->Gshift) | ((Uint32)b << f->Bshift)
		| (f->Amask ? (Uint32)a << f->Ashift : 0);
}

HW_API Uint32 SDL_MapRGB(const SDL_PixelFormat* f, Uint8 r, Uint8 g, Uint8 b) { return pack(f, r, g, b, 255); }
HW_API Uint32 SDL_MapRGBA(const SDL_PixelFormat* f, Uint8 r, Uint8 g, Uint8 b, Uint8 a) { return pack(f, r, g, b, a); }
HW_API void SDL_GetRGB(Uint32 p, const SDL_PixelFormat* f, Uint8* r, Uint8* g, Uint8* b) { Uint8 a; unpack(f, p, r, g, b, &a); }
HW_API void SDL_GetRGBA(Uint32 p, const SDL_PixelFormat* f, Uint8* r, Uint8* g, Uint8* b, Uint8* a) { unpack(f, p, r, g, b, a); }

HW_API SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, const SDL_PixelFormat* fmt, Uint32)
{
	SDL_Surface* dst = SDL_CreateRGBSurface(0, src->w, src->h, 32, fmt->Rmask, fmt->Gmask, fmt->Bmask, fmt->Amask);
	if (!dst)
		return NULL;
	for (int y = 0; y < src->h; y++) {
		Uint32 *s = row(src, y), *d = row(dst, y);
		for (int x = 0; x < src->w; x++) {
			Uint8 r, g, b, a;
			unpack(src->format, s[x], &r, &g, &b, &a);
			d[x] = pack(dst->format, r, g, b, a);
		}
	}
	if (has_key(src)) {
		Uint8 r, g, b, a;
		unpack(src->format, key_of(src), &r, &g, &b, &a);
		SDL_SetColorKey(dst, 1, pack(dst->format, r, g, b, a));
	}
	return dst;
}

HW_API SDL_Surface* SDL_DisplayFormat(SDL_Surface* s)
{
	SDL_PixelFormat* f = SDL_AllocFormat(kPixelFormatABGR8888);
	SDL_Surface* d = SDL_ConvertSurface(s, f, 0);
	SDL_FreeFormat(f);
	return d;
}
HW_API SDL_Surface* SDL_DisplayFormatAlpha(SDL_Surface* s) { return SDL_DisplayFormat(s); }

// clip r against the surface's clip rect; false when nothing is left
static bool clip(SDL_Surface* s, SDL_Rect* r)
{
	int x0 = r->x > s->clip_rect.x ? r->x : s->clip_rect.x;
	int y0 = r->y > s->clip_rect.y ? r->y : s->clip_rect.y;
	int x1 = r->x + r->w, cx1 = s->clip_rect.x + s->clip_rect.w;
	int y1 = r->y + r->h, cy1 = s->clip_rect.y + s->clip_rect.h;
	if (x1 > cx1) x1 = cx1;
	if (y1 > cy1) y1 = cy1;
	*r = {x0, y0, x1 - x0, y1 - y0};
	return r->w > 0 && r->h > 0;
}

HW_API int SDL_FillRect(SDL_Surface* s, const SDL_Rect* rect, Uint32 color)
{
	SDL_Rect r = rect ? *rect : SDL_Rect{0, 0, s->w, s->h};
	if (!clip(s, &r))
		return 0;
	for (int y = r.y; y < r.y + r.h; y++) {
		Uint32* p = row(s, y) + r.x;
		for (int x = 0; x < r.w; x++)
			p[x] = color;
	}
	return 0;
}

// SDL2 semantics: a source with an alpha mask is alpha-blended onto the
// destination, otherwise copied; colour-keyed pixels are skipped.
HW_API int SDL_UpperBlit(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect)
{
	SDL_Rect sr = srcrect ? *srcrect : SDL_Rect{0, 0, src->w, src->h};
	SDL_Rect full = {0, 0, src->w, src->h};
	SDL_Rect sc = sr;
	{	// clip the source rect to the source surface
		int x0 = sc.x < 0 ? 0 : sc.x, y0 = sc.y < 0 ? 0 : sc.y;
		int x1 = sc.x + sc.w > full.w ? full.w : sc.x + sc.w;
		int y1 = sc.y + sc.h > full.h ? full.h : sc.y + sc.h;
		int dx = x0 - sr.x, dy = y0 - sr.y;
		sc = {x0, y0, x1 - x0, y1 - y0};
		sr = sc;
		if (dstrect) { dstrect->x += dx; dstrect->y += dy; }
	}
	SDL_Rect dr = {dstrect ? dstrect->x : 0, dstrect ? dstrect->y : 0, sr.w, sr.h};
	SDL_Rect dc = dr;
	if (sr.w <= 0 || sr.h <= 0 || !clip(dst, &dc)) {
		if (dstrect) { dstrect->w = 0; dstrect->h = 0; }
		return 0;
	}
	sr.x += dc.x - dr.x;
	sr.y += dc.y - dr.y;
	bool blend = src->format->Amask != 0;
	bool keyed = has_key(src);
	Uint32 key = key_of(src);
	for (int y = 0; y < dc.h; y++) {
		Uint32* s = row(src, sr.y + y) + sr.x;
		Uint32* d = row(dst, dc.y + y) + dc.x;
		for (int x = 0; x < dc.w; x++) {
			if (keyed && s[x] == key)
				continue;
			Uint8 r, g, b, a;
			unpack(src->format, s[x], &r, &g, &b, &a);
			if (blend && a != 255) {
				Uint8 dr_, dg, db, da;
				unpack(dst->format, d[x], &dr_, &dg, &db, &da);
				r = (r * a + dr_ * (255 - a)) / 255;
				g = (g * a + dg * (255 - a)) / 255;
				b = (b * a + db * (255 - a)) / 255;
				a = a + da * (255 - a) / 255;
			}
			d[x] = pack(dst->format, r, g, b, a);
		}
	}
	if (dstrect)
		*dstrect = dc;
	return 0;
}

HW_API void SDL_UpdateRect(SDL_Surface*, int, int, Uint32, Uint32) {}
HW_API int SDL_Flip(SDL_Surface*) { return 0; }

static void put32(Uint8* p, Uint32 v) { memcpy(p, &v, 4); }	// x86: little-endian

HW_API int SDL_SaveBMP_RW(SDL_Surface* s, SDL_RWops* dst, int freedst)
{
	Uint32 imageSize = (Uint32)s->w * s->h * 4;
	Uint8 hdr[54] = {'B', 'M'};
	put32(hdr + 2, 54 + imageSize);
	put32(hdr + 10, 54);
	put32(hdr + 14, 40);
	put32(hdr + 18, s->w);
	put32(hdr + 22, (Uint32)-s->h);	// negative height: rows top to bottom
	hdr[26] = 1;
	hdr[28] = 32;
	put32(hdr + 34, imageSize);
	bool ok = dst->write(dst, hdr, 54, 1) == 1;
	Uint32* line = (Uint32*)malloc(s->w * 4);
	for (int y = 0; ok && y < s->h; y++) {
		for (int x = 0; x < s->w; x++) {
			Uint8 r, g, b, a;
			unpack(s->format, row(s, y)[x], &r, &g, &b, &a);
			line[x] = b | (g << 8) | (r << 16) | ((Uint32)a << 24);
		}
		ok = dst->write(dst, line, s->w * 4, 1) == 1;
	}
	free(line);
	if (freedst)
		dst->close(dst);
	return ok ? 0 : -1;
}
