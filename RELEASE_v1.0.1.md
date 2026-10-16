## FrameMover 1.0.1

### Changes
- **FrameMover shows up in the Frame's launcher.** The installer now writes `~/.local/share/applications/frame-mover.desktop` and a 128x128 RGBA icon in the user's `hicolor` icon theme (`Icon=frame-mover`, the way the other Frame apps do it). Uninstall removes both.
- **Install and uninstall work from Desktop Mode and Flatpak launchers.** Those can start programs with `XDG_CONFIG_HOME` redirected (OpenVR then cannot find `openvrpaths.vrpath`) or with the nested Plasma session's `XDG_RUNTIME_DIR`. The installer reported "SteamVR is not running" and never registered the app. FrameMover now puts `HOME`, `XDG_CONFIG_HOME` and `XDG_RUNTIME_DIR` back to what SteamVR itself sees, and the message when it cannot reach SteamVR says what went wrong instead of claiming SteamVR is off.
- **The panel shows the default bindings instead of "not bound"** while SteamVR has not resolved any yet (no controller connected this session), as long as you have not saved bindings of your own. Once a controller connects, SteamVR's own labels are shown.
- **Renamed to "FrameMover"** (no space) in SteamVR, the dashboard tab, the launcher entry and the tile artwork.

### Upgrading
Run the new `FrameMover-installer-aarch64.AppImage` and pick **Install**; it replaces the installed copy, the launcher entry and the artwork. If SteamVR is running when FrameMover is replaced, restart FrameMover (or SteamVR) to load the new version.
