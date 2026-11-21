// The network game's smaller windows: creating a room, banning, and the server
// administration window (frontend-qt6 roomnameprompt.cpp, bandialog.cpp, pageadmin.cpp).
// They talk to the network window through messages.
#pragma once

#include <Messenger.h>
#include <Rect.h>

#include <string>

enum {
	kNetSend = 'nsnd',	// to the network window: "part" strings, one server message
	kNetReply = 'nrpl',	// to the administration window: "part" strings (SERVER_VARS, BANLIST)
	kNetCreateRoom = 'ncrr',	// to the network window: "name", "password"
};

// asks for a new room's name and password
void open_create_room_window(BMessenger net, BRect over, const std::string& name);
// nick: an online player (BAN: their address); empty: a nickname or address pattern (BANNICK, BANIP)
void open_ban_window(BMessenger net, BRect over, const std::string& nick);
// the server's messages, protocol number and bans; replies come as kNetReply
BMessenger open_admin_window(BMessenger net, BRect over);
