#pragma once
#include <string>

// Safety net on top of "never commit": keeps copies of SteamVR's saved chaperone file
// (~/.config/openvr/config/chaperone_info.vrchap) in ~/.local/share/frame-mover/backup, one per distinct
// content, newest 10 kept. Returns the backup directory, or "" when there is no chaperone file to save.
std::string BackupChaperoneFile();
