#pragma once

// Copies the program and its data to ~/.local, writes the SteamVR manifest and registers it (autostart with SteamVR).
int InstallApp();
// Removes what InstallApp created. Settings and chaperone backups are kept.
int UninstallApp();
// Holds a per-user lock for the life of the process; false (after saying so) when another copy already runs.
bool BecomeOnlyInstance();
