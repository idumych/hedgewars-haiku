// SDL_image on the Translation Kit.
#include "hwhaiku.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <File.h>
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>

#include <stdlib.h>
#include <string.h>

HW_API int IMG_Init(int flags) { return flags; }
HW_API void IMG_Quit() {}

static SDL_Surface* from_bitmap(BBitmap* bm)
{
	if (bm->ColorSpace() != B_RGBA32 && bm->ColorSpace() != B_RGB32) {
		BBitmap* conv = new BBitmap(bm->Bounds(), B_RGBA32);
		if (conv->ImportBits(bm) != B_OK) {
			delete conv;
			hw_set_error("unsupported image colour space");
			return NULL;
		}
		delete bm;
		bm = conv;
	}
	bool alpha = bm->ColorSpace() == B_RGBA32;
	int w = bm->Bounds().IntegerWidth() + 1, h = bm->Bounds().IntegerHeight() + 1;
	SDL_Surface* s = hw_create_rgba(w, h);
	for (int y = 0; s && y < h; y++) {
		const Uint8* in = (const Uint8*)bm->Bits() + (size_t)y * bm->BytesPerRow();
		Uint8* out = (Uint8*)s->pixels + (size_t)y * s->pitch;
		for (int x = 0; x < w; x++, in += 4, out += 4) {	// B G R A -> R G B A
			out[0] = in[2];
			out[1] = in[1];
			out[2] = in[0];
			out[3] = alpha ? in[3] : 255;
		}
	}
	delete bm;
	return s;
}

HW_API SDL_Surface* IMG_Load_RW(SDL_RWops* rw, int freesrc)
{
	if (!rw)
		return NULL;
	if (!hw_start_app()) {
		if (freesrc)
			rw->close(rw);
		return NULL;
	}
	size_t size;
	void* data = hw_read_all(rw, &size, freesrc);
	BMemoryIO io(data, size);
	BBitmap* bm = BTranslationUtils::GetBitmap(&io);
	free(data);
	if (!bm) {
		hw_set_error("can't decode image");
		return NULL;
	}
	return from_bitmap(bm);
}

HW_API SDL_Surface* IMG_LoadTyped_RW(SDL_RWops* rw, int freesrc, const char*) { return IMG_Load_RW(rw, freesrc); }
HW_API SDL_Surface* IMG_LoadPNG_RW(SDL_RWops* rw) { return IMG_Load_RW(rw, 0); }

HW_API SDL_Surface* IMG_Load(const char* file)
{
	if (!hw_start_app())
		return NULL;
	BBitmap* bm = BTranslationUtils::GetBitmapFile(file);
	if (!bm) {
		hw_set_error("can't load %s", file);
		return NULL;
	}
	return from_bitmap(bm);
}

HW_API int IMG_SavePNG(SDL_Surface* s, const char* file)
{
	if (!hw_start_app())
		return -1;
	BBitmap* bm = new BBitmap(BRect(0, 0, s->w - 1, s->h - 1), B_RGBA32);
	for (int y = 0; y < s->h; y++) {
		const Uint32* in = (const Uint32*)((Uint8*)s->pixels + (size_t)y * s->pitch);
		Uint8* out = (Uint8*)bm->Bits() + (size_t)y * bm->BytesPerRow();
		for (int x = 0; x < s->w; x++, out += 4) {
			const SDL_PixelFormat* f = s->format;
			out[2] = (in[x] & f->Rmask) >> f->Rshift;
			out[1] = (in[x] & f->Gmask) >> f->Gshift;
			out[0] = (in[x] & f->Bmask) >> f->Bshift;
			out[3] = f->Amask ? (in[x] & f->Amask) >> f->Ashift : 255;
		}
	}
	BBitmapStream stream(bm);	// owns bm now
	BFile out(file, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	status_t err = out.InitCheck();
	if (err == B_OK)
		err = BTranslatorRoster::Default()->Translate(&stream, NULL, NULL, &out, B_PNG_FORMAT);
	if (err != B_OK)
		hw_set_error("can't save %s", file);
	return err == B_OK ? 0 : -1;
}
