#pragma once
#include <string>

// User-adjustable settings, persisted to ~/.config/frame-mover/config.ini.
// Button assignments are NOT here: they are SteamVR Input bindings (see data/bindings and the "Edit bindings" button).
struct Settings
{
	float dragMultiplier = 1.0f; // world moves this many times the hand movement
	float snapDegrees = 45.0f;   // size of one snap turn
	float heightSpeed = 0.5f;    // meters per second while "raise me / lower me" is held
	bool lockHeight = false;     // grab-and-move ignores vertical hand movement
	bool leftHand = true;        // left controller may grab
	bool rightHand = true;       // right controller may grab
	bool haptics = true;         // short buzz on snap turn, reset and when a grab starts
	bool doublePress = true;     // grab / turn need tap, release, press again and hold (like OVR Advanced Settings)
	bool disableChaperone = false; // while the world is moved, also switch SteamVR's chaperone (and its outside-play-area passthrough) off
	bool largeChaperone = true;  // while the world is moved, show a huge chaperone instead of the saved (or placeholder) one
	bool overrideGameInput = false; // withhold our bound inputs from the running game (needs a SteamVR developer setting)
};

std::string ConfigDir();
Settings LoadSettings();
void SaveSettings( const Settings &s );
Settings Clamped( Settings s );
