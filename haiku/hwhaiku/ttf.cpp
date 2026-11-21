// SDL_ttf on BFont: fonts are loaded from memory (they come from PhysFS) and
// strings are drawn into an offscreen BBitmap.
#include "hwhaiku.h"

#include <Bitmap.h>
#include <Font.h>
#include <OS.h>
#include <View.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

struct TTF_Font {
	BFont font;
	float ascent, height;
};

// The app_server loads a given font file only once; a second LoadFont of
// the same data fails. So each distinct file is loaded once, kept for the
// life of the process, and every TTF_Font gets a sized copy of it.
struct LoadedFont {
	size_t size;
	uint32 hash;
	area_id area;
	BFont font;
};
static std::vector<LoadedFont*> sLoaded;

static uint32 fnv1a(const Uint8* p, size_t n)
{
	uint32 h = 2166136261u;
	for (size_t i = 0; i < n; i++)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

static const BFont* load_font(void* data, size_t size)
{
	uint32 hash = fnv1a((const Uint8*)data, size);
	for (LoadedFont* l : sLoaded)
		if (l->size == size && l->hash == hash)
			return &l->font;
	void* addr;
	size_t areaSize = (size + B_PAGE_SIZE - 1) & ~(size_t)(B_PAGE_SIZE - 1);
	area_id area = create_area("hw font", &addr, B_ANY_ADDRESS, areaSize, B_NO_LOCK,
		B_READ_AREA | B_WRITE_AREA | B_CLONEABLE_AREA);
	if (area < 0)
		return NULL;
	memcpy(addr, data, size);
	LoadedFont* l = new LoadedFont{size, hash, area, BFont()};
	if (l->font.LoadFont(area, size, 0) != B_OK) {
		delete_area(area);
		delete l;
		return NULL;
	}
	sLoaded.push_back(l);
	return &l->font;
}

HW_API int TTF_Init() { return 0; }
HW_API void TTF_Quit() {}

HW_API TTF_Font* TTF_OpenFontRW(SDL_RWops* rw, int freesrc, int ptsize)
{
	if (!rw || !hw_start_app()) {
		if (rw && freesrc)
			rw->close(rw);
		return NULL;
	}
	size_t size;
	void* data = hw_read_all(rw, &size, freesrc);
	const BFont* loaded = load_font(data, size);
	free(data);
	if (!loaded) {
		hw_set_error("can't load font");
		return NULL;
	}
	TTF_Font* f = new TTF_Font;
	f->font = *loaded;
	f->font.SetSize(ptsize);	// SDL_ttf points are pixels at 72 dpi, like BFont's
	f->font.SetFlags(B_FORCE_ANTIALIASING);	// grayscale coverage, no subpixel fringes
	font_height fh;
	f->font.GetHeight(&fh);
	f->ascent = ceilf(fh.ascent);
	f->height = f->ascent + ceilf(fh.descent);
	return f;
}

HW_API void TTF_CloseFont(TTF_Font* f) { delete f; }

// shortcut: styles are ignored; hedgewars ships its bold faces as separate files
HW_API void TTF_SetFontStyle(TTF_Font*, int) {}

HW_API int TTF_SizeUTF8(TTF_Font* f, const char* text, int* w, int* h)
{
	if (!f || !text) {	// the engine measures with no fonts in --stats-only
		if (w) *w = 0;
		if (h) *h = 0;
		hw_set_error("no font");
		return -1;
	}
	if (w)
		*w = (int)ceilf(f->font.StringWidth(text));
	if (h)
		*h = (int)f->height;
	return 0;
}

// coverage (0..255) of the text, one byte per pixel; caller frees
static Uint8* coverage(TTF_Font* f, const char* text, int* w, int* h)
{
	TTF_SizeUTF8(f, text, w, h);
	if (*w < 1)
		*w = 1;
	BBitmap bm(BRect(0, 0, *w - 1, *h - 1), B_BITMAP_ACCEPTS_VIEWS, B_RGB32);
	BView* v = new BView(bm.Bounds(), "text", B_FOLLOW_NONE, 0);
	bm.AddChild(v);
	bm.Lock();
	v->SetHighColor(0, 0, 0);
	v->FillRect(v->Bounds());
	v->SetFont(&f->font);
	v->SetHighColor(255, 255, 255);
	v->SetDrawingMode(B_OP_OVER);
	v->DrawString(text, BPoint(0, f->ascent));
	v->Sync();
	bm.Unlock();

	Uint8* cov = (Uint8*)malloc((size_t)*w * *h);
	for (int y = 0; y < *h; y++) {
		const Uint8* p = (const Uint8*)bm.Bits() + (size_t)y * bm.BytesPerRow();
		for (int x = 0; x < *w; x++)
			cov[y * *w + x] = p[x * 4 + 1];	// green channel of white-on-black
	}
	return cov;
}

enum { kBlended, kSolid, kShaded };

static SDL_Surface* render(TTF_Font* f, const char* text, SDL_Color fg, SDL_Color bg, int mode)
{
	if (!f || !text)
		return NULL;
	int w, h;
	Uint8* cov = coverage(f, text, &w, &h);
	SDL_Surface* s = hw_create_rgba(w, h);
	for (int i = 0; s && i < w * h; i++) {
		Uint8* p = (Uint8*)s->pixels + (size_t)(i / w) * s->pitch + (i % w) * 4;
		int c = cov[i];
		if (mode == kSolid)
			c = c >= 128 ? 255 : 0;
		if (mode == kShaded) {
			p[0] = (fg.r * c + bg.r * (255 - c)) / 255;
			p[1] = (fg.g * c + bg.g * (255 - c)) / 255;
			p[2] = (fg.b * c + bg.b * (255 - c)) / 255;
			p[3] = 255;
		} else {
			p[0] = fg.r;
			p[1] = fg.g;
			p[2] = fg.b;
			p[3] = c * (fg.a ? fg.a : 255) / 255;
		}
	}
	free(cov);
	return s;
}

HW_API SDL_Surface* TTF_RenderUTF8_Blended(TTF_Font* f, const char* t, SDL_Color fg) { return render(f, t, fg, fg, kBlended); }
HW_API SDL_Surface* TTF_RenderUTF8_Solid(TTF_Font* f, const char* t, SDL_Color fg) { return render(f, t, fg, fg, kSolid); }
HW_API SDL_Surface* TTF_RenderUTF8_Shaded(TTF_Font* f, const char* t, SDL_Color fg, SDL_Color bg) { return render(f, t, fg, bg, kShaded); }
