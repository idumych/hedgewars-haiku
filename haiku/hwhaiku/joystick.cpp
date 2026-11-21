// Joysticks on BJoystick, polled from SDL_PumpEvents and turned into SDL
// joystick events.
#include "hwhaiku.h"

#include <Joystick.h>
#include <String.h>

#include <string.h>
#include <vector>

struct SDL_Joystick {
	BJoystick stick;
	int index;
	int axes, hats, buttons;
	std::vector<int16> axisValues;
	std::vector<uint8> hatValues;
	uint32 buttonValues;
};

static std::vector<SDL_Joystick*> sOpen;
static bool sEvents = true;

HW_API int SDL_NumJoysticks() { BJoystick j; return j.CountDevices(); }

HW_API const char* SDL_JoystickNameForIndex(int i)
{
	static char name[B_OS_NAME_LENGTH];
	BJoystick j;
	if (j.GetDeviceName(i, name, sizeof(name)) != B_OK)
		return NULL;
	return name;
}

HW_API SDL_Joystick* SDL_JoystickOpen(int i)
{
	char name[B_OS_NAME_LENGTH];
	SDL_Joystick* j = new SDL_Joystick;
	if (j->stick.GetDeviceName(i, name, sizeof(name)) != B_OK || j->stick.Open(name, true) < 0) {
		delete j;
		SDL_SetError("can't open joystick %d", i);
		return NULL;
	}
	j->index = i;
	j->axes = j->stick.CountAxes();
	j->hats = j->stick.CountHats();
	j->buttons = j->stick.CountButtons();
	j->axisValues.assign(j->axes, 0);
	j->hatValues.assign(j->hats, 0);
	j->buttonValues = 0;
	sOpen.push_back(j);
	return j;
}

HW_API void SDL_JoystickClose(SDL_Joystick* j)
{
	for (size_t i = 0; i < sOpen.size(); i++)
		if (sOpen[i] == j)
			sOpen.erase(sOpen.begin() + i);
	delete j;
}

HW_API int SDL_JoystickNumAxes(SDL_Joystick* j) { return j->axes; }
HW_API int SDL_JoystickNumHats(SDL_Joystick* j) { return j->hats; }
HW_API int SDL_JoystickNumButtons(SDL_Joystick* j) { return j->buttons; }
HW_API int SDL_JoystickEventState(int state) { if (state >= 0) sEvents = state; return sEvents; }

// Haiku hats count clockwise from 1 = up; SDL uses up/right/down/left bits
static Uint8 sdl_hat(uint8 v)
{
	static const Uint8 kMap[9] = {0, 1, 3, 2, 6, 4, 12, 8, 9};
	return v < 9 ? kMap[v] : 0;
}

void hw_joystick_poll()
{
	for (SDL_Joystick* j : sOpen) {
		if (j->stick.Update() != B_OK)
			continue;
		std::vector<int16> axes(j->axes);
		std::vector<uint8> hats(j->hats);
		if (j->axes)
			j->stick.GetAxisValues(axes.data());
		if (j->hats)
			j->stick.GetHatValues(hats.data());
		uint32 buttons = j->stick.ButtonValues();
		if (!sEvents) {
			j->axisValues = axes;
			j->hatValues = hats;
			j->buttonValues = buttons;
			continue;
		}
		for (int a = 0; a < j->axes; a++) {
			if (axes[a] == j->axisValues[a])
				continue;
			j->axisValues[a] = axes[a];
			SDL_Event e = {};
			e.jaxis.type = SDL_JOYAXISMOTION;
			e.jaxis.timestamp = hw_ticks();
			e.jaxis.which = j->index;
			e.jaxis.axis = a;
			e.jaxis.value = axes[a];
			hw_push_event(e);
		}
		for (int h = 0; h < j->hats; h++) {
			if (hats[h] == j->hatValues[h])
				continue;
			j->hatValues[h] = hats[h];
			SDL_Event e = {};
			e.jhat.type = SDL_JOYHATMOTION;
			e.jhat.timestamp = hw_ticks();
			e.jhat.which = j->index;
			e.jhat.hat = h;
			e.jhat.value = sdl_hat(hats[h]);
			hw_push_event(e);
		}
		for (int b = 0; b < j->buttons && b < 32; b++) {
			bool now = (buttons >> b) & 1;
			if (now == ((j->buttonValues >> b) & 1))
				continue;
			SDL_Event e = {};
			e.jbutton.type = now ? SDL_JOYBUTTONDOWN : SDL_JOYBUTTONUP;
			e.jbutton.timestamp = hw_ticks();
			e.jbutton.which = j->index;
			e.jbutton.button = b;
			e.jbutton.state = now;
			hw_push_event(e);
		}
		j->buttonValues = buttons;
	}
}
