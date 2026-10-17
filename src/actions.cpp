#include "actions.h"
#include "bindings.h"
#include "files.h"

#include <openvr.h>

#include "app.h"

#include <algorithm>

using namespace vr;
using namespace bindings;

namespace
{
constexpr const char *kSet = "/actions/playspace";

bool Digital( IVRInput *input, VRActionHandle_t action, VRInputValueHandle_t restrictTo, bool rising = false )
{
	InputDigitalActionData_t d = {};
	return input->GetDigitalActionData( action, &d, sizeof d, restrictTo ) == VRInputError_None &&
		d.bActive && d.bState && ( !rising || d.bChanged );
}

// SteamVR keeps a user's saved binding configuration under "<controller type>_<SteamVR's app id>_..._steamvrinput" in the app's
// settings section; the section has none until the user has saved bindings in the editor.
bool UserHasCustomBindings()
{
	for ( const char *key : { "frame_controller_250820_CurrentURL_steamvrinput", "frame_controller_250820_AutosaveURL_steamvrinput" } )
	{
		char value[256] = {};
		EVRSettingsError e = VRSettingsError_None;
		VRSettings()->GetString( kAppKey, key, value, sizeof value, &e );
		if ( e == VRSettingsError_None && value[0] )
			return true;
	}
	return false;
}
} // namespace

bool Actions::Init( const std::string &manifestPath, bool overrideGameInput, std::string &error )
{
	IVRInput *in = VRInput();
	if ( !in )
	{
		error = "IVRInput unavailable";
		return false;
	}
	EVRInputError e = in->SetActionManifestPath( manifestPath.c_str() );
	if ( e != VRInputError_None )
	{
		error = "SetActionManifestPath(" + manifestPath + ") failed, error " + std::to_string( (int)e );
		return false;
	}
	for ( int i = 0; i <= Haptic; i++ )
	{
		e = in->GetActionHandle( kActions[i].path, &m_action[i] );
		if ( e != VRInputError_None )
		{
			error = std::string( "GetActionHandle(" ) + kActions[i].path + ") failed, error " + std::to_string( (int)e );
			return false;
		}
	}
	if ( in->GetActionSetHandle( kSet, &m_set ) != VRInputError_None )
	{
		error = "GetActionSetHandle failed";
		return false;
	}
	if ( in->GetInputSourceHandle( "/user/hand/left", &m_hand[0] ) != VRInputError_None ||
		in->GetInputSourceHandle( "/user/hand/right", &m_hand[1] ) != VRInputError_None )
	{
		error = "GetInputSourceHandle failed";
		return false;
	}
	m_priority = overrideGameInput ? k_nActionSetOverlayGlobalPriorityMin : 0;
	std::string defaults;
	if ( files::ReadFile( manifestPath.substr( 0, manifestPath.rfind( '/' ) + 1 ) + "bindings/frame_controller.json", defaults ) )
		LoadDefaults( defaults, m_defaults );
	return true;
}

bool Actions::Update( InputFrame &out )
{
	out = InputFrame();
	IVRInput *input = VRInput();
	VRActiveActionSet_t active = {};
	active.ulActionSet = m_set;
	active.ulRestrictedToDevice = k_ulInvalidInputValueHandle;
	active.nPriority = m_priority;
	if ( input->UpdateActionState( &active, sizeof active, 1 ) != VRInputError_None )
		return false;

	for ( int h = 0; h < 2; h++ )
	{
		out.drag[h] = Digital( input, m_action[Drag], m_hand[h] );
		out.turn[h] = Digital( input, m_action[Turn], m_hand[h] );
	}
	out.snapLeft = Digital( input, m_action[SnapLeft], k_ulInvalidInputValueHandle, true );
	out.snapRight = Digital( input, m_action[SnapRight], k_ulInvalidInputValueHandle, true );
	out.heightUp = Digital( input, m_action[Up], k_ulInvalidInputValueHandle );
	out.heightDown = Digital( input, m_action[Down], k_ulInvalidInputValueHandle );
	out.reset = Digital( input, m_action[Reset], k_ulInvalidInputValueHandle );
	return true;
}

void Actions::Pulse( Hand hand, float seconds, float amplitude )
{
	VRInput()->TriggerHapticVibrationAction( m_action[Haptic], 0.0f, seconds, 150.0f, amplitude, m_hand[(int)hand] );
}

std::vector<std::pair<std::string, std::string>> Actions::Bindings()
{
	std::vector<std::pair<std::string, std::string>> out;
	out.reserve( Haptic );
	IVRInput *input = VRInput();
	int custom = -1; // lazily: does the user have saved bindings of their own
	for ( int i = 0; i < Haptic; i++ )
	{
		VRInputValueHandle_t origins[16] = {};
		std::string names;
		const EVRInputError err = input->GetActionOrigins( m_set, m_action[i], origins, 16 );
		if ( err == VRInputError_None )
		{
			for ( VRInputValueHandle_t o : origins )
			{
				if ( o == k_ulInvalidInputValueHandle )
					continue;
				char buf[256] = {};
				if ( input->GetOriginLocalizedName( o, buf, sizeof buf, VRInputString_All ) == VRInputError_None && buf[0] )
				{
					std::string n = buf;
					for ( size_t p; ( p = n.find( " Frame Controller" ) ) != std::string::npos; ) // "Left Hand Frame Controller Bumper" -> "Left Hand Bumper"
						n.erase( p, 17 );
					AppendName( names, n );
				}
			}
			if ( names.empty() )
			{
				// SteamVR reports no active origins while the dashboard has input focus, even though the bindings
				// exist. The binding description does not depend on that, so read it instead.
				InputBindingInfo_t info[16] = {};
				uint32_t count = 0;
				if ( input->GetActionBindingInfo( m_action[i], info, sizeof( InputBindingInfo_t ), 16, &count ) == VRInputError_None )
				{
					std::vector<std::string> seen;
					for ( uint32_t k = 0; k < count && k < 16; k++ )
					{
						const std::string n = Pretty( info[k].rchDevicePathName ) + " " + Pretty( info[k].rchInputPathName );
						if ( std::find( seen.begin(), seen.end(), n ) != seen.end() )
							continue;
						seen.push_back( n );
						AppendName( names, n );
					}
				}
			}
			// Still nothing while the dashboard is up: keep what we last knew rather than claiming "not bound".
			if ( names.empty() && VROverlay()->IsDashboardVisible() )
				names = m_lastGood[i];
			if ( !names.empty() )
				m_lastGood[i] = names;
			// SteamVR has no resolved bindings yet (no controller connected this session): unless the user has saved their own
			// bindings, what the shipped default file maps the action to is what will apply, so show that instead of "not bound".
			if ( names.empty() && !m_defaults[i].empty() )
			{
				if ( custom < 0 )
					custom = UserHasCustomBindings() ? 1 : 0;
				if ( !custom )
					names = m_defaults[i];
			}
		}
		if ( err != VRInputError_None )
			out.emplace_back( kActions[i].label, "unavailable (SteamVR Input error " + std::to_string( (int)err ) + ")" );
		else
			out.emplace_back( kActions[i].label, names.empty() ? "not bound" : names );
	}
	return out;
}

bool Actions::OpenBindingEditor( const std::string &appKey )
{
	return VRInput()->OpenBindingUI( appKey.c_str(), m_set, k_ulInvalidInputValueHandle, false ) == VRInputError_None;
}
