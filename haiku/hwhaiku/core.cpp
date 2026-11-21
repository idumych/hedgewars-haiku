// Init, time, errors, threads, locks, RWops, clipboard.
#include "hwhaiku.h"

#include <Clipboard.h>
#include <OS.h>

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char sError[512];
static bigtime_t sStart = system_time();

void hw_set_error(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(sError, sizeof(sError), fmt, ap);
	va_end(ap);
}

Uint32 hw_ticks() { return (Uint32)((system_time() - sStart) / 1000); }

HW_API int SDL_SetError(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(sError, sizeof(sError), fmt, ap);
	va_end(ap);
	return -1;
}

HW_API const char* SDL_GetError() { return sError; }
HW_API Uint32 SDL_GetTicks() { return hw_ticks(); }
HW_API void SDL_Delay(Uint32 ms) { snooze((bigtime_t)ms * 1000); }
HW_API void SDL_free(void* p) { free(p); }
HW_API int SDL_SetHint(const char*, const char*) { return 1; }

// video.cpp
bool hw_start_app();
void hw_stop_app();

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

HW_API void SDL_WaitThread(HWThread* t, int* status)
{
	if (!t)
		return;
	status_t ret;
	wait_for_thread(t->id, &ret);
	if (status)
		*status = t->status;
	delete t;
}

// shortcut: a detached thread's record leaks (one small struct per screenshot)
HW_API void SDL_DetachThread(HWThread*) {}
HW_API void SDL_KillThread(HWThread* t) { if (t) kill_thread(t->id); }

HW_API pthread_mutex_t* SDL_CreateMutex()
{
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);	// SDL mutexes are recursive
	pthread_mutex_t* m = new pthread_mutex_t;
	pthread_mutex_init(m, &a);
	pthread_mutexattr_destroy(&a);
	return m;
}
HW_API void SDL_DestroyMutex(pthread_mutex_t* m) { if (m) { pthread_mutex_destroy(m); delete m; } }
HW_API int SDL_LockMutex(pthread_mutex_t* m) { return pthread_mutex_lock(m); }
HW_API int SDL_UnlockMutex(pthread_mutex_t* m) { return pthread_mutex_unlock(m); }

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

static Sint64 file_size(SDL_RWops* rw)
{
	FILE* f = (FILE*)rw->hidden.stdio.fp;
	long pos = ftell(f);
	fseek(f, 0, SEEK_END);
	long end = ftell(f);
	fseek(f, pos, SEEK_SET);
	return end;
}
static Sint64 file_seek(SDL_RWops* rw, Sint64 off, int whence)
{
	FILE* f = (FILE*)rw->hidden.stdio.fp;
	if (fseeko(f, off, whence) != 0)
		return SDL_SetError("seek failed");
	return ftello(f);
}
static size_t file_read(SDL_RWops* rw, void* p, size_t size, size_t n)
{
	return fread(p, size, n, (FILE*)rw->hidden.stdio.fp);
}
static size_t file_write(SDL_RWops* rw, const void* p, size_t size, size_t n)
{
	return fwrite(p, size, n, (FILE*)rw->hidden.stdio.fp);
}
static int file_close(SDL_RWops* rw)
{
	int r = fclose((FILE*)rw->hidden.stdio.fp);
	SDL_FreeRW(rw);
	return r;
}

HW_API SDL_RWops* SDL_RWFromFile(const char* name, const char* mode)
{
	FILE* f = fopen(name, mode);
	if (!f) {
		SDL_SetError("can't open %s", name);
		return NULL;
	}
	SDL_RWops* rw = SDL_AllocRW();
	rw->size = file_size;
	rw->seek = file_seek;
	rw->read = file_read;
	rw->write = file_write;
	rw->close = file_close;
	rw->type = 2;
	rw->hidden.stdio.autoclose = 1;
	rw->hidden.stdio.fp = f;
	return rw;
}

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
