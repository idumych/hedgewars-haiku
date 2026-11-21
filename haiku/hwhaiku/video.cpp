// Window, OpenGL context, input events and the cursor, on BApplication /
// BWindow / BGLView. BApplication runs in its own thread; the engine's main
// thread holds the GL context and drains the event queue.
#include "hwhaiku.h"

#include <Application.h>
#include <GLView.h>
#include <InterfaceDefs.h>
#include <Screen.h>
#include <Window.h>

#include <ctype.h>
#include <deque>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kSignature = "application/x-vnd.hedgewars-engine";
static const Uint32 kScancodeMask = 1u << 30;

Uint8 hw_keystate[512];

// ── event queue ──
static std::deque<SDL_Event> sQueue;
static pthread_mutex_t sQueueLock = PTHREAD_MUTEX_INITIALIZER;

void hw_push_event(const SDL_Event& e)
{
	static const bool debug = getenv("HWHAIKU_DEBUG") != NULL;
	if (debug)
		fprintf(stderr, "hwhaiku event %#x key %d text '%.8s'\n", e.type,
			e.type == SDL_KEYDOWN || e.type == SDL_KEYUP ? e.key.keysym.scancode : 0,
			e.type == SDL_TEXTINPUT ? e.text.text : "");
	pthread_mutex_lock(&sQueueLock);
	if (sQueue.size() < 4096)
		sQueue.push_back(e);
	pthread_mutex_unlock(&sQueueLock);
}

// ── keyboard: Haiku raw key codes to SDL (USB HID) scancodes ──
#include "rawkeys.h"

static const char* scancode_name(int sc)
{
	// runs of consecutive scancodes: first scancode, then its names
	static const struct { int first; const char* names[30]; } kRuns[] = {
		{4, {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
			"O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"}},
		{30, {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "Return", "Escape",
			"Backspace", "Tab", "Space", "-", "=", "[", "]", "\\", "#", ";", "'",
			"`", ",", ".", "/", "CapsLock"}},
		{58, {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11",
			"F12", "PrintScreen", "ScrollLock", "Pause", "Insert", "Home", "PageUp",
			"Delete", "End", "PageDown", "Right", "Left", "Down", "Up", "Numlock",
			"Keypad /", "Keypad *", "Keypad -", "Keypad +"}},
		{88, {"Keypad Enter", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4",
			"Keypad 5", "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9", "Keypad 0",
			"Keypad .", "", "Application"}},
		{224, {"Left Ctrl", "Left Shift", "Left Alt", "Left GUI", "Right Ctrl",
			"Right Shift", "Right Alt", "Right GUI"}},
	};
	for (const auto& run : kRuns)
		if (sc >= run.first && sc < run.first + 30 && run.names[sc - run.first])
			return run.names[sc - run.first];
	return "";
}

// keycodes as SDL2 makes them for a US layout
static Sint32 keycode_for(int sc)
{
	static const char kChars[] = "\0\0\0\0abcdefghijklmnopqrstuvwxyz1234567890\r\x1b\b\t -=[]\\#;'`,./";
	if (sc > 0 && sc < (int)sizeof(kChars) - 1 && kChars[sc])
		return (Uint8)kChars[sc];
	if (sc == 76)
		return 127;	// Delete
	return sc | kScancodeMask;
}

HW_API Sint32 SDL_GetKeyFromScancode(int sc) { return keycode_for(sc); }

HW_API const char* SDL_GetKeyName(Sint32 key)
{
	static char buf[2];
	if (key & kScancodeMask)
		return scancode_name(key & ~kScancodeMask);
	switch (key) {
		case '\r': return "Return";
		case 27: return "Escape";
		case '\b': return "Backspace";
		case '\t': return "Tab";
		case ' ': return "Space";
		case 127: return "Delete";
	}
	buf[0] = toupper(key);
	return buf;
}

HW_API Uint8* SDL_GetKeyboardState(int* numkeys)
{
	if (numkeys)
		*numkeys = 512;
	return hw_keystate;
}

static Uint16 sdl_mod(uint32 m)
{
	return ((m & B_LEFT_SHIFT_KEY) ? 0x1 : 0) | ((m & B_RIGHT_SHIFT_KEY) ? 0x2 : 0)
		| ((m & B_LEFT_CONTROL_KEY) ? 0x40 : 0) | ((m & B_RIGHT_CONTROL_KEY) ? 0x80 : 0)
		| ((m & B_LEFT_COMMAND_KEY) ? 0x100 : 0) | ((m & B_RIGHT_COMMAND_KEY) ? 0x200 : 0)
		| ((m & B_LEFT_OPTION_KEY) ? 0x400 : 0) | ((m & B_RIGHT_OPTION_KEY) ? 0x800 : 0)
		| ((m & B_NUM_LOCK) ? 0x1000 : 0) | ((m & B_CAPS_LOCK) ? 0x2000 : 0);
}

static void key_event(int sc, bool down, bool repeat, uint32 mods)
{
	if (sc <= 0)
		return;
	hw_keystate[sc] = down;
	SDL_Event e = {};
	e.key.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	e.key.timestamp = hw_ticks();
	e.key.windowID = 1;
	e.key.state = down;
	e.key.repeat = repeat;
	e.key.keysym.scancode = sc;
	e.key.keysym.sym = keycode_for(sc);
	e.key.keysym.mod = sdl_mod(mods);
	hw_push_event(e);
}

// ── window ──
class HWView;
class HWWindow;
static HWWindow* sWindow;
static HWView* sView;
static bool sRelative, sCursorVisible = true, sCursorHidden;
static BPoint sMouse;
static Uint32 sButtons;

static void window_event(int ev, int d1 = 0, int d2 = 0)
{
	SDL_Event e = {};
	e.window.type = SDL_WINDOWEVENT;
	e.window.timestamp = hw_ticks();
	e.window.windowID = 1;
	e.window.event = ev;
	e.window.data1 = d1;
	e.window.data2 = d2;
	hw_push_event(e);
}

static void update_cursor()
{
	bool hide = sRelative || !sCursorVisible;
	if (hide != sCursorHidden && be_app) {
		hide ? be_app->HideCursor() : be_app->ShowCursor();
		sCursorHidden = hide;
	}
}

class HWView : public BGLView {
public:
	HWView(BRect r)
		: BGLView(r, "hedgewars", B_FOLLOW_ALL_SIDES, B_WILL_DRAW | B_FRAME_EVENTS,
			BGL_RGB | BGL_DOUBLE) {}

	BPoint Center() const { return BPoint(floorf(Bounds().Width() / 2), floorf(Bounds().Height() / 2)); }

	void Warp(BPoint p)
	{
		ConvertToScreen(&p);
		set_mouse_position((int32)p.x, (int32)p.y);
	}

	void AttachedToWindow() override
	{
		BGLView::AttachedToWindow();
		MakeFocus();
		SetEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
	}

	void MouseMoved(BPoint where, uint32, const BMessage*) override
	{
		SDL_Event e = {};
		e.motion.type = SDL_MOUSEMOTION;
		e.motion.timestamp = hw_ticks();
		e.motion.windowID = 1;
		e.motion.state = sButtons;
		if (sRelative) {
			BPoint c = Center();
			int dx = (int)(where.x - c.x), dy = (int)(where.y - c.y);
			if (dx == 0 && dy == 0)
				return;	// the warp below coming back
			e.motion.x = (int)c.x;
			e.motion.y = (int)c.y;
			e.motion.xrel = dx;
			e.motion.yrel = dy;
			if (Window()->IsActive())
				Warp(c);
		} else {
			e.motion.x = (int)where.x;
			e.motion.y = (int)where.y;
			e.motion.xrel = (int)(where.x - sMouse.x);
			e.motion.yrel = (int)(where.y - sMouse.y);
			sMouse = where;
		}
		hw_push_event(e);
	}

	void Button(BPoint where, bool down)
	{
		int32 buttons = 0;
		Window()->CurrentMessage()->FindInt32("buttons", &buttons);
		static const struct { int32 be; Uint8 sdl; } kMap[] = {
			{B_PRIMARY_MOUSE_BUTTON, 1}, {B_TERTIARY_MOUSE_BUTTON, 2}, {B_SECONDARY_MOUSE_BUTTON, 3}};
		for (auto m : kMap) {
			bool now = buttons & m.be, was = sButtons & (1u << (m.sdl - 1));
			if (now == was)
				continue;
			sButtons ^= 1u << (m.sdl - 1);
			SDL_Event e = {};
			e.button.type = now ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
			e.button.timestamp = hw_ticks();
			e.button.windowID = 1;
			e.button.button = m.sdl;
			e.button.state = now;
			e.button.x = (int)where.x;
			e.button.y = (int)where.y;
			hw_push_event(e);
		}
		(void)down;
	}

	void MouseDown(BPoint where) override { MakeFocus(); Button(where, true); }
	void MouseUp(BPoint where) override { Button(where, false); }

	void FrameResized(float w, float h) override
	{
		BGLView::FrameResized(w, h);
		window_event(SDL_WINDOWEVENT_RESIZED, (int)w + 1, (int)h + 1);
		window_event(SDL_WINDOWEVENT_SIZE_CHANGED, (int)w + 1, (int)h + 1);
	}
};

class HWWindow : public BWindow {
public:
	HWWindow(BRect frame, const char* title, uint32 flags)
		: BWindow(frame, title, B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE
			| ((flags & 0x20) ? 0 : (B_NOT_RESIZABLE | B_NOT_ZOOMABLE)))
	{
		sView = new HWView(Bounds());
		AddChild(sView);
	}

	void DispatchMessage(BMessage* msg, BHandler* target) override
	{
		int32 raw = 0, repeat = 0;
		uint32 mods = modifiers();
		msg->FindInt32("modifiers", (int32*)&mods);
		switch (msg->what) {
			case B_KEY_DOWN:
			case B_UNMAPPED_KEY_DOWN:
				msg->FindInt32("key", &raw);
				msg->FindInt32("be:key_repeat", &repeat);
				if (raw > 0 && raw < 128)
					key_event(kRawToScancode[raw], true, repeat > 0, mods);
				if (msg->what == B_KEY_DOWN && !(mods & (B_COMMAND_KEY | B_CONTROL_KEY))) {
					const char* bytes;
					if (msg->FindString("bytes", &bytes) == B_OK
						&& (Uint8)bytes[0] >= 0x20 && bytes[0] != 0x7f) {
						SDL_Event e = {};
						e.text.type = SDL_TEXTINPUT;
						e.text.timestamp = hw_ticks();
						e.text.windowID = 1;
						strlcpy(e.text.text, bytes, sizeof(e.text.text));
						hw_push_event(e);
					}
				}
				return;	// keys are the game's, including Alt shortcuts
			case B_KEY_UP:
			case B_UNMAPPED_KEY_UP:
				msg->FindInt32("key", &raw);
				if (raw > 0 && raw < 128)
					key_event(kRawToScancode[raw], false, false, mods);
				return;
			case B_MODIFIERS_CHANGED: {
				int32 old = 0;
				msg->FindInt32("be:old_modifiers", &old);
				static const struct { uint32 bit; int sc; } kMods[] = {
					{B_LEFT_SHIFT_KEY, 225}, {B_RIGHT_SHIFT_KEY, 229},
					{B_LEFT_CONTROL_KEY, 224}, {B_RIGHT_CONTROL_KEY, 228},
					{B_LEFT_COMMAND_KEY, 226}, {B_RIGHT_COMMAND_KEY, 230},
					{B_LEFT_OPTION_KEY, 227}, {B_RIGHT_OPTION_KEY, 231}};
				for (auto m : kMods)
					if ((mods ^ (uint32)old) & m.bit)
						key_event(m.sc, mods & m.bit, false, mods);
				return;
			}
			case B_MOUSE_WHEEL_CHANGED: {
				float dy = 0, dx = 0;
				msg->FindFloat("be:wheel_delta_y", &dy);
				msg->FindFloat("be:wheel_delta_x", &dx);
				SDL_Event e = {};
				e.wheel.type = SDL_MOUSEWHEEL;
				e.wheel.timestamp = hw_ticks();
				e.wheel.windowID = 1;
				e.wheel.x = (int)dx;
				e.wheel.y = (int)-dy;	// Haiku: positive is down; SDL: positive is up
				hw_push_event(e);
				return;
			}
		}
		BWindow::DispatchMessage(msg, target);
	}

	void WindowActivated(bool active) override
	{
		window_event(active ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST);
		if (!active) {	// keys released while we weren't looking
			for (int sc = 0; sc < 512; sc++)
				if (hw_keystate[sc])
					key_event(sc, false, false, 0);
		}
	}

	void Minimize(bool minimize) override
	{
		BWindow::Minimize(minimize);
		window_event(minimize ? SDL_WINDOWEVENT_MINIMIZED : SDL_WINDOWEVENT_RESTORED);
	}

	bool QuitRequested() override
	{
		SDL_Event e = {};
		e.type = SDL_QUIT;
		hw_push_event(e);
		return false;	// the engine shuts down and destroys us
	}

	BRect fWindowedFrame;
};

// ── application thread ──
static bool sQuitting;	// set by hw_stop_app: only then may the app really quit

class HWApp : public BApplication {
public:
	HWApp(sem_id ready) : BApplication(kSignature), fReady(ready) {}
	void ReadyToRun() override { release_sem(fReady); }
	bool QuitRequested() override
	{
		if (sQuitting)
			return true;
		SDL_Event e = {};
		e.type = SDL_QUIT;
		hw_push_event(e);
		return false;	// the engine decides; it ends up in hw_stop_app()
	}
	sem_id fReady;
};

static thread_id sAppThread = -1;

static int32 app_thread(void* ready)
{
	HWApp* app = new HWApp((sem_id)(intptr_t)ready);
	app->Run();
	delete app;
	return 0;
}

void hw_stop_app();

bool hw_start_app()
{
	if (sAppThread >= 0 || be_app)
		return true;
	sem_id ready = create_sem(0, "hw app ready");
	sAppThread = spawn_thread(app_thread, "hw app", B_NORMAL_PRIORITY, (void*)(intptr_t)ready);
	if (sAppThread < 0 || resume_thread(sAppThread) != B_OK) {
		SDL_SetError("can't start the application thread");
		return false;
	}
	acquire_sem(ready);
	delete_sem(ready);
	atexit(hw_stop_app);	// halt() skips SDL_Quit; libbe can't tear down a running app
	return true;
}

HW_API int SDL_DestroyWindow(void* win);

void hw_stop_app()
{
	if (sAppThread < 0)
		return;
	if (sWindow)
		SDL_DestroyWindow(sWindow);
	sQuitting = true;
	be_app->PostMessage(B_QUIT_REQUESTED);	// Quit() from another thread would keep the lock
	status_t r;
	wait_for_thread(sAppThread, &r);
	sAppThread = -1;
}

// ── SDL window / GL API ──
static bool sGLLocked;

static int place(int v, int size, int screen)
{
	return ((v & 0xFFFF0000) == 0x2FFF0000 || (v & 0xFFFF0000) == 0x1FFF0000)
		? (screen - size) / 2 : v;
}

HW_API void* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags)
{
	if (sWindow) {
		SDL_SetError("only one window is supported");
		return NULL;
	}
	BRect screen = BScreen().Frame();
	x = place(x, w, (int)screen.Width() + 1);
	y = place(y, h, (int)screen.Height() + 1);
	sWindow = new HWWindow(BRect(x, y, x + w - 1, y + h - 1), title, flags);
	sWindow->fWindowedFrame = sWindow->Frame();
	if (flags & 0x1) {	// SDL_WINDOW_FULLSCREEN
		sWindow->SetLook(B_NO_BORDER_WINDOW_LOOK);
		sWindow->MoveTo(0, 0);
		sWindow->ResizeTo(screen.Width(), screen.Height());
	}
	if (!(flags & 0x8)) {	// not SDL_WINDOW_HIDDEN
		sWindow->Show();
		sWindow->Activate();	// started from a launcher: take the keyboard
	}
	else {
		sWindow->Hide();
		sWindow->Show();
	}
	return sWindow;
}

HW_API int SDL_DestroyWindow(void* win)
{
	if (!win || win != sWindow)
		return -1;
	if (sGLLocked) {
		sView->UnlockGL();
		sGLLocked = false;
	}
	if (sCursorHidden) {
		be_app->ShowCursor();
		sCursorHidden = false;
	}
	sWindow->Lock();
	sWindow->Quit();
	sWindow = NULL;
	sView = NULL;
	return 0;
}

HW_API void SDL_SetWindowIcon(void*, SDL_Surface*) {}	// Haiku shows the app's resource icon

HW_API void SDL_SetWindowSize(void*, int w, int h)
{
	if (sWindow && sWindow->Lock()) {
		sWindow->ResizeTo(w - 1, h - 1);
		sWindow->Unlock();
	}
}

HW_API void SDL_SetWindowPosition(void*, int x, int y)
{
	if (sWindow && sWindow->Lock()) {
		BRect screen = BScreen(sWindow).Frame();
		sWindow->MoveTo(place(x, sWindow->Frame().IntegerWidth() + 1, (int)screen.Width() + 1),
			place(y, sWindow->Frame().IntegerHeight() + 1, (int)screen.Height() + 1));
		sWindow->Unlock();
	}
}

HW_API void SDL_SetWindowFullscreen(void*, Uint32 flags)
{
	if (!sWindow || !sWindow->Lock())
		return;
	if (flags & 0x1) {
		if (sWindow->Look() != B_NO_BORDER_WINDOW_LOOK)
			sWindow->fWindowedFrame = sWindow->Frame();
		BRect screen = BScreen(sWindow).Frame();
		sWindow->SetLook(B_NO_BORDER_WINDOW_LOOK);
		sWindow->MoveTo(0, 0);
		sWindow->ResizeTo(screen.Width(), screen.Height());
	} else if (sWindow->Look() == B_NO_BORDER_WINDOW_LOOK) {
		sWindow->SetLook(B_TITLED_WINDOW_LOOK);
		sWindow->MoveTo(sWindow->fWindowedFrame.LeftTop());
		sWindow->ResizeTo(sWindow->fWindowedFrame.Width(), sWindow->fWindowedFrame.Height());
	}
	sWindow->Unlock();
}

HW_API const char* SDL_GetCurrentVideoDriver() { return "haiku"; }

// shortcut: the attributes are ignored; the view is always RGB, double-buffered
HW_API int SDL_GL_SetAttribute(int, int) { return 0; }
HW_API int SDL_GL_SetSwapInterval(int) { return 0; }

HW_API void* SDL_GL_CreateContext(void* win)
{
	if (!win || win != sWindow) {
		SDL_SetError("no window");
		return NULL;
	}
	if (!sGLLocked) {
		sView->LockGL();
		sGLLocked = true;
	}
	return sView;
}

HW_API void SDL_GL_DeleteContext(void*)
{
	if (sGLLocked && sView) {
		sView->UnlockGL();
		sGLLocked = false;
	}
}

HW_API void SDL_GL_SwapWindow(void*)
{
	if (!sView)
		return;
	sView->SwapBuffers();
	// let the window thread resize the view between frames
	sView->UnlockGL();
	sView->LockGL();
}

// ── mouse ──
HW_API int SDL_ShowCursor(int toggle)
{
	if (toggle >= 0) {
		sCursorVisible = toggle;
		update_cursor();
	}
	return sCursorVisible;
}

HW_API int SDL_SetRelativeMouseMode(int enabled)
{
	sRelative = enabled;
	update_cursor();
	if (enabled && sWindow && sWindow->Lock()) {
		sView->Warp(sView->Center());
		sWindow->Unlock();
	}
	return 0;
}

HW_API void SDL_WarpMouseInWindow(void*, int x, int y)
{
	if (sWindow && sWindow->Lock()) {
		sMouse = BPoint(x, y);
		if (!sRelative)
			sView->Warp(sMouse);
		sWindow->Unlock();
	}
}

// ── event API ──
void hw_joystick_poll();	// joystick.cpp

HW_API void SDL_PumpEvents() { hw_joystick_poll(); }
HW_API void SDL_StartTextInput() {}
HW_API void SDL_StopTextInput() {}

// action: 0 add, 1 peek, 2 get
HW_API int SDL_PeepEvents(SDL_Event* events, int n, int action, Uint32 minType, Uint32 maxType)
{
	if (action == 0) {
		for (int i = 0; i < n; i++)
			hw_push_event(events[i]);
		return n;
	}
	int count = 0;
	pthread_mutex_lock(&sQueueLock);
	for (auto it = sQueue.begin(); it != sQueue.end() && count < n;) {
		if (it->type >= minType && it->type <= maxType) {
			if (events)
				memcpy(&events[count], &*it, sizeof(SDL_Event));
			count++;
			if (action == 2) {
				it = sQueue.erase(it);
				continue;
			}
		}
		++it;
	}
	pthread_mutex_unlock(&sQueueLock);
	return count;
}

HW_API void SDL_FlushEvent(Uint32 type)
{
	pthread_mutex_lock(&sQueueLock);
	for (auto it = sQueue.begin(); it != sQueue.end();)
		it = it->type == type ? sQueue.erase(it) : it + 1;
	pthread_mutex_unlock(&sQueueLock);
}
