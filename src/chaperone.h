#pragma once
#include "playspace.h"

#include <openvr.h>
#include <string>
#include <vector>

// Thin wrapper over IVRChaperoneSetup. The mover only ever edits SteamVR's *working copy* and shows it as a live
// preview; it never calls CommitWorkingCopy, so the chaperone/playspace the user saved (chaperone_info.vrchap) is
// never written. "Restore" is therefore exact: drop the preview and reload the saved copy.
class Chaperone
{
public:
	// Reloads the saved copy into the working copy and reads the zero poses the user had set.
	bool Capture( Mat34 &standing, Mat34 &seated );
	// Puts these zero poses on screen immediately (working copy + preview).
	// `largeChaperone` also replaces the working play area / wall bounds with a huge box (and puts the saved ones back
	// when it is false again). `disableChaperone` additionally sets SteamVR's `steamvr/disableChaperone` setting for as
	// long as the preview is shown; the previous value is kept on disk and put back by Restore() / RecoverSetting().
	bool Apply( const Mat34 &standing, const Mat34 &seated, bool largeChaperone, bool disableChaperone );
	// Puts the chaperone setting back if an earlier run died while holding the override. Call once after VR_Init.
	static void RecoverSetting();
	// Drops the preview and reloads the saved copy.
	void Restore();
	// True when the working copy no longer holds what Apply() last wrote, i.e. another program (SteamVR room
	// setup, another overlay) is editing it.
	bool ModifiedByOthers() const;
	bool PreviewShown() const { return m_applied; }

private:
	bool m_applied = false;
	Mat34 m_lastStanding, m_lastSeated;
	bool m_boundsOverridden = false;
	float m_savedAreaX = 0, m_savedAreaZ = 0;
	std::vector<vr::HmdQuad_t> m_savedQuads; // the working bounds as Capture() found them
	bool m_settingOverridden = false;
	std::string m_settingBefore; // "absent", "0" or "1": what steamvr/disableChaperone was before we set it

	void SetChaperoneDisabled( bool disabled );
};
