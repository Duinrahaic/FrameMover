#pragma once
#include "actions.h"
#include "chaperone.h"
#include "config.h"
#include "panel.h"
#include "playspace.h"
#include "ui.h"
#include "vk_texture.h"

#include <openvr.h>

#include <atomic>
#include <memory>
#include <string>

constexpr const char *kAppKey = "dev.framemover.app";
constexpr const char *kPanelKey = "dev.framemover.panel";

// Directory holding action_manifest.json: next to the executable (build tree), or the installed copy.
std::string FindDataDir();

class App
{
public:
	static std::atomic<bool> s_quit;
	static std::atomic<bool> s_showDashboard; // set by SIGUSR1: a second launch (quick-launch tile) asks us to show our tab

	App() = default;
	~App();
	bool Init( std::string &error );
	int Run();
	// Applies a known offset to the live playspace, verifies it through SteamVR, restores, verifies again.
	int SelfTest();
	void ShowDashboardPanel();

private:
	void Tick();
	void PollEvents();
	void HandleSystemEvent( const vr::VREvent_t &ev );
	void RefreshBindings( double now );
	void HandlePanelEvent( const vr::VREvent_t &ev );

	void Rebase();            // adopt SteamVR's current saved playspace as "original"
	void ApplyIfChanged();
	void CheckLivePreview( double now ); // is SteamVR still using the playspace we applied? re-apply if it silently dropped it
	void Pause( bool restore );
	void Pulse( bool bothHands );

	struct Tracked
	{
		bool valid = false;
		Vec3 pos;
	};
	Tracked ReadPose( vr::TrackedDeviceIndex_t idx, const vr::TrackedDevicePose_t *poses ) const;

	void RedrawPanel();
	void HandlePanelAction( PanelAction action, double value );
	bool MoveAllowed() const { return m_state == MoverState::Ready; }
	bool GestureActive() const { return m_drag[0].Active() || m_drag[1].Active() || m_turn[0].Active() || m_turn[1].Active(); }

	bool m_vrInit = false;
	Settings m_settings;
	Chaperone m_chaperone;
	Playspace m_playspace;
	Actions m_actions;
	bool m_inputOk = false;
	std::string m_inputError;

	DragGesture m_drag[2];
	TurnGesture m_turn[2];
	DoublePressGate m_dragGate[2], m_turnGate[2]; // per hand: tap, then press and hold
	bool m_dragWasOn[2] = {}, m_turnWasOn[2] = {};   // previous gated state, to buzz when a grab starts
	MoverState m_state = MoverState::WaitingForChaperone;

	// timing (seconds on a steady clock)
	double m_lastTick = 0;
	double m_rebaseAt = -1;       // pending debounced rebase, or -1
	double m_nextRetry = 0;       // next attempt to get a playspace while waiting
	double m_nextExternalCheck = 0;
	double m_resetHeldSince = -1;
	bool m_resetFired = false;

	Vec3 m_appliedOffset;
	double m_appliedYaw = 0;
	bool m_appliedAny = false;
	std::vector<double> m_healTimes; // recent self-repairs, for the back-off
	double m_healBackoffUntil = 0;
	double m_lastApply = -10; // when we last wrote a preview; used to tell our own echo events from outside changes

	// dashboard panel
	vr::VROverlayHandle_t m_panel = vr::k_ulOverlayHandleInvalid;
	vr::VROverlayHandle_t m_thumb = vr::k_ulOverlayHandleInvalid;
	ButtonPanel m_buttons;
	bool m_panelDirty = true;
	double m_panelRetryAt = 0;
	double m_lastBindingRefresh = -10;
	bool m_bindingsStale = true;
	int m_hover = -1;
	std::vector<std::pair<std::string, std::string>> m_bindings;
	std::vector<uint8_t> m_lastPanelPixels; // what the compositor currently has
	std::unique_ptr<VulkanContext> m_vk;    // declared before the texture that lives on it
	std::unique_ptr<OverlayTexture> m_panelTex;
};
