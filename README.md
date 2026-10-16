# frame-mover

An [OVR Advanced Settings](https://github.com/OpenVR-Advanced-Settings/OpenVR-AdvancedSettings)-style **playspace mover that
runs on the Steam Frame itself** (native Linux ARM64, against the SteamVR that is installed on the headset).

* **Grab and move** the world with a controller button, **grab and turn** it around your head, **snap turn**, **raise / lower** yourself.
* **Key binds are SteamVR Input bindings**: rebind them in SteamVR's own binding editor (button in the dashboard tab).
* **Reset to original** puts back exactly the playspace / chaperone you had saved. The mover itself never commits anything to SteamVR's chaperone.

## How it works

The mover edits SteamVR's *working copy* of the standing and seated zero poses (`IVRChaperoneSetup`) and shows it with
`ShowWorkingSetPreview`, like OVR-AS. It never calls `CommitWorkingCopy`.

* The virtual world moves; the chaperone stays where your real room is (the play area shifts opposite to the move).
* **Huge chaperone while moved** (panel toggle, on by default, `largeChaperone` in `config.ini`): while the world is moved, the working copy's play area is replaced by a 200 x 200 m square with four wall quads, so SteamVR's chaperone does not show up at the edge of your (or the 1 x 1 m placeholder) play area. It lives in the same working copy as the moved playspace: never committed, and Reset puts the saved size and walls back. Switch it off and the chaperone you saved is used as is. Note that the chaperone is also the only warning before a real wall.
* **No chaperone** (panel toggle, off by default, `disableChaperone` in `config.ini`): while the world is moved, also sets SteamVR's `steamvr/disableChaperone` setting, for when the Frame still switches to passthrough outside the play area. That setting is written to `steamvr.vrsettings` immediately (SteamVR has no temporary settings), so the previous value is first saved to `~/.config/frame-mover/chaperone-setting.restore`; Reset, Quit and the next start put it back.
* **Reset** = drop the preview and `RevertWorkingCopy`, which reloads `~/.config/openvr/config/chaperone_info.vrchap`. The mover never commits to that file. SteamVR does rewrite it on its own, in the background (it adds and prunes placeholder universes while the headset is not tracking, re-saves the seated origin after recenters, and its `cv` driver re-reads it every minute while tracking); the backups in `~/.local/share/frame-mover/backup/` show this.
* Crash or kill: SteamVR notices the preview owner is gone and reloads the saved chaperone itself (`vrserver` log: "Live chaperone preview process ... isn't running anymore, reloading saved chaperone").
* Extra safety net: a copy of `chaperone_info.vrchap` is kept in `~/.local/share/frame-mover/backup/` at every start and install (one per distinct content, newest 10).
* Grabbing uses controller positions in *raw* tracking space, so the move does not feed back into the measurement. Both the standing and the seated universe move together.
* Tracking universe change, recenter, or finished room setup: the new saved playspace becomes the baseline (moves are dropped). SteamVR room setup in progress: the mover steps aside and puts everything back first.
* Quitting the mover (dashboard Close, SIGTERM, SteamVR shutdown) restores the original.
* Moves are not remembered across restarts of the mover.

## Default controls (Steam Frame controllers)

| Action | Default binding | Notes |
|---|---|---|
| Grab and move | Bumper (either hand) | double-press and hold, move your hand: the world follows it |
| Grab and turn | Thumbstick click (either hand) | double-press and hold, swing the hand around your head: the world rotates about you |
| Snap turn left / right | Left D-pad left / right | angle in the panel (default 45 deg) |
| Raise me / Lower me | Left D-pad up / down | hold; speed in the panel |
| Reset to original | Left View button | hold 1.5 s, buzz when done |

**Double-press + hold** (like OVR Advanced Settings): grab and turn only start when you tap the button, release, and press it again within 0.4 s and keep holding it. A single press, which a game may use the same button for, never grabs the world. The grab ends when you let go, and you feel a short buzz when it starts. Switch it off in the panel ("Double-press + hold to grab") to grab on a plain hold. A button bound as *toggle* in SteamVR's editor can not be double-pressed, so turn the option off if you bind one of them that way.

Change any of them: dashboard tab **Frame Mover** > **Edit bindings in SteamVR**. The panel shows what each action is
currently bound to and updates as soon as SteamVR reloads the bindings (it listens for SteamVR's binding events). SteamVR loads
bindings per controller type, so until a controller has connected in the current SteamVR session every action reads
"not bound" (the panel says so). By default your game still sees those buttons too; to withhold them from the game set
`overrideGameInput=1` in `~/.config/frame-mover/config.ini` and enable SteamVR's developer setting
"Experimental overlay input overrides".

The dashboard tab also has: **Reset to original playspace**, raise / lower / turn buttons, grab speed, snap angle, raise speed,
lock height, per-hand enable, haptics. Settings are in `~/.config/frame-mover/config.ini`.

## Quick-launch tile

`--install` registers the app as a dashboard overlay with `show_overlay_in_quicklaunch`, so it appears in SteamVR's quick-launch
bar (artwork: `header.png`, `capsule.png`, generated at install time). SteamVR orders that bar by most recently launched, so
the tile can sit in the second row until it has been used. Pressing the tile while the mover is already running starts a second
copy, which tells the running one to open its dashboard tab (the pid is kept in `$XDG_RUNTIME_DIR/frame-mover.<uid>.lock`)
and exits.

## Build, install

```
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure      # model unit tests, no SteamVR needed
./build/frame-mover --install         # copies to ~/.local, registers with SteamVR, autostarts with it
./build/frame-mover --uninstall
```

Needs `libopenvr_api.so` and headers from `/opt/steamvr` (override with `-DOPENVR_LIB=... -DOPENVR_HEADERS=...`) and cairo.

Other commands: `--selftest` (moves the live playspace for about a second through SteamVR, verifies, restores, verifies; with the
headset on this briefly shifts your world), `--screenshot FILE.png` (renders the panel, no SteamVR), `--show-dashboard`.

## Known risk: leaving while the dashboard panel is open

The panel is drawn with `SetOverlayRaw` (pixels through shared memory). Killing the mover while the dashboard tab was open once
made `vrcompositor` crash (SIGBUS in a `memcpy` on its render thread, seen in `journalctl`/`coredumpctl`), and SteamVR restarted
itself. To reduce this the panel is now only re-uploaded when its pixels change, and on a normal exit (SIGTERM, dashboard Close)
the overlay is hidden and destroyed before disconnecting. A `kill -9` or a crash of the mover can still do it. If it ever happens
again, switching the panel to Vulkan textures (`SetOverlayTexture`, as the PassthroughOverlay project does) is the next step.

## Playspace snapping back on its own

Works with QuestLHSync on. In rare cases SteamVR still replaces the live playspace with the saved one and the floor height returns to the original. What is known:

* `CheckLivePreview` compares the zero pose SteamVR is really using (recovered from the headset's pose in raw space and in the standing
  universe) with the one the mover applied. On a mismatch while idle it logs one line (`live playspace differs from ours ...` with the three
  origins and whether the working copy is still ours), then re-applies (`re-applied the playspace SteamVR had dropped`). It stops trying for 30 s
  if SteamVR replaces it three times in 20 s.
* What causes SteamVR to drop it is not established. Candidates seen in `vrserver.txt`: the `cv` driver's once-a-minute "Preserving worlds ..." followed by
  "Read chaperone JSON", relocalization (a new universe id appears in the saved file every few hours), recenter (system-button long press), and the
  Frame's own room-setup start/cancel (`ERoomSetupStep_RoomSetupFull`). A recenter or room-setup event is, by design, taken as a new original, which drops the move.
