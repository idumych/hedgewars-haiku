// Init, time, errors, threads, locks, RWops, clipboard.
#include "hwhaiku.h"

#include <Clipboard.h>
#include <OS.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char sError[512];
static bigtime_t sStart = system_time();

HW_API int SDL_SetError(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(sError, sizeof(sError), fmt, ap);
	va_end(ap);
	return -1;
}

Uint32 hw_ticks() { return (Uint32)((system_time() - sStart) / 1000); }

HW_API const char* SDL_GetError() { return sError; }
HW_API Uint32 SDL_GetTicks() { return hw_ticks(); }
HW_API void SDL_Delay(Uint32 ms) { snooze((bigtime_t)ms * 1000); }
HW_API void SDL_free(void* p) { free(p); }

void hw_stop_app();	// video.cpp

HW_API int SDL_InitSubSystem(Uint32 flags)
{
	if ((flags & 0x20) && !hw_start_app())	// SDL_INIT_VIDEO
		return -1;
	return 0;
}

HW_API int SDL_Init(Uint32 flags) { return SDL_InitSubSystem(flags); }
HW_API void SDL_Quit() { hw_stop_app(); }

// ── threads ──
struct HWThread { thread_id id; int (*fn)(void*); void* data; int status; };

static int32 hw_thread_main(void* p)
{
	HWThread* t = (HWThread*)p;
	t->status = t->fn(t->data);
	return t->status;
}

HW_API HWThread* SDL_CreateThread(int (*fn)(void*), const char* name, void* data)
{
	HWThread* t = new HWThread{-1, fn, data, 0};
	t->id = spawn_thread(hw_thread_main, name ? name : "hw thread", B_NORMAL_PRIORITY, t);
	if (t->id < 0 || resume_thread(t->id) != B_OK) {
		delete t;
		SDL_SetError("spawn_thread failed");
		return NULL;
	}
	return t;
}

// shortcut: a detached thread's record leaks (one small struct per screenshot)
HW_API void SDL_DetachThread(HWThread*) {}

HW_API void* SDL_CreateSemaphore(Uint32 initial)
{
	sem_id s = create_sem(initial, "hw sem");
	return s < 0 ? NULL : (void*)(intptr_t)(s + 1);	// +1 so a valid sem is never NULL
}
HW_API void SDL_DestroySemaphore(void* s) { if (s) delete_sem((sem_id)(intptr_t)s - 1); }
HW_API int SDL_SemWait(void* s) { return acquire_sem((sem_id)(intptr_t)s - 1) == B_OK ? 0 : -1; }
HW_API int SDL_SemPost(void* s) { return release_sem((sem_id)(intptr_t)s - 1) == B_OK ? 0 : -1; }

// ── RWops ──
HW_API SDL_RWops* SDL_AllocRW() { return (SDL_RWops*)calloc(1, sizeof(SDL_RWops)); }
HW_API void SDL_FreeRW(SDL_RWops* rw) { free(rw); }

void* hw_read_all(SDL_RWops* rw, size_t* size, bool freesrc)
{
	size_t cap = 65536, len = 0;
	char* buf = (char*)malloc(cap);
	for (;;) {
		if (len == cap)
			buf = (char*)realloc(buf, cap *= 2);
		size_t got = rw->read(rw, buf + len, 1, cap - len);
		if (got == 0)
			break;
		len += got;
	}
	if (freesrc)
		rw->close(rw);
	*size = len;
	return buf;
}

// ── clipboard ──
HW_API int SDL_SetClipboardText(const char* text)
{
	if (!be_clipboard->Lock())
		return -1;
	be_clipboard->Clear();
	be_clipboard->Data()->AddData("text/plain", B_MIME_TYPE, text, strlen(text));
	be_clipboard->Commit();
	be_clipboard->Unlock();
	return 0;
}

HW_API char* SDL_GetClipboardText()
{
	char* out = NULL;
	if (be_clipboard->Lock()) {
		const char* data;
		ssize_t len;
		if (be_clipboard->Data()->FindData("text/plain", B_MIME_TYPE,
				(const void**)&data, &len) == B_OK) {
			out = (char*)malloc(len + 1);
			memcpy(out, data, len);
			out[len] = 0;
		}
		be_clipboard->Unlock();
	}
	return out ? out : strdup("");
}

HW_API int SDL_HasClipboardText()
{
	char* t = SDL_GetClipboardText();
	int has = t[0] != 0;
	free(t);
	return has;
}
