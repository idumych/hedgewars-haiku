// SDL_image on the Translation Kit.
#include "hwhaiku.h"

#include <Bitmap.h>
#include <DataIO.h>
#include <TranslationUtils.h>

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
			SDL_SetError("unsupported image colour space");
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
		SDL_SetError("can't decode image");
		return NULL;
	}
	return from_bitmap(bm);
}
