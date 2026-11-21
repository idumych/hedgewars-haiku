// The scheme and weapon set editors: frontend-qt6's PageScheme and
// PageSelectWeapon as native windows. Predefined ones are shown read-only.
#pragma once

#include <Messenger.h>

#include <string>

// sent to the launcher after any change; "select": a name it should now pick
const uint32 kMsgEditorsChanged = 'edch';

void show_scheme_editor(BMessenger launcher, const std::string& select);
void show_weapon_editor(BMessenger launcher, const std::string& select);
