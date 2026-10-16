#include "actions.h"

#include <openvr.h>

#include "app.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <regex>
#include <cstring>

using namespace vr;

namespace
{
constexpr const char *kSet = "/actions/playspace";

bool Digital( VRActionHandle_t action, VRInputValueHandle_t restrictTo, bool &state, bool &changed )
{
	InputDigitalActionData_t d = {};
	if ( VRInput()->GetDigitalActionData( action, &d, sizeof d, restrictTo ) != VRInputError_None || !d.bActive )
	{
		state = changed = false;
		return false;
	}
	state = d.bState;
	changed = d.bChanged;
	return true;
}

// "/user/hand/left" -> "Left Hand", "/input/dpad_left" -> "Dpad Left"
std::string Pretty( std::string s )
{
	for ( const char *prefix : { "/user/hand/", "/user/", "/input/" } )
		if ( s.rfind( prefix, 0 ) == 0 )
		{
			const bool hand = std::string( prefix ) == "/user/hand/";
			s = s.substr( strlen( prefix ) );
			for ( char &c : s )
				if ( c == '_' || c == '/' )
					c = ' ';
			bool start = true;
			for ( char &c : s )
			{
				if ( start && c >= 'a' && c <= 'z' )
					c = (char)( c - 'a' + 'A' );
				start = c == ' ';
			}
			return hand ? s + " Hand" : s;
		}
	return s;
}

// Reads the shipped default binding file and writes, per action (same order as Actions::Bindings), the text Bindings() would give
// for it, e.g. "Left Hand Bumper, Right Hand Bumper". Each source lists its action ("output") before its "path".
void LoadDefaultBindings( const std::string &file, std::string ( &out )[7] )
{
	static const char *const actions[7] = { "drag", "turn", "snapturnleft", "snapturnright", "heightup", "heightdown", "resetoriginal" };
	std::ifstream in( file );
	const std::string text( ( std::istreambuf_iterator<char>( in ) ), std::istreambuf_iterator<char>() );
	const std::regex pair( "\"(output|path)\"\\s*:\\s*\"([^\"]*)\"" );
	std::string pending;
	for ( std::sregex_iterator it( text.begin(), text.end(), pair ), end; it != end; ++it )
	{
		const std::string key = ( *it )[1], value = ( *it )[2];
		if ( key == "output" )
		{
			pending = value;
			continue;
		}
		const size_t split = value.find( "/input/" );
		for ( int i = 0; i < 7 && split != std::string::npos; i++ )
			if ( pending == std::string( kSet ) + "/in/" + actions[i] )
				out[i] += ( out[i].empty() ? "" : ", " ) + Pretty( value.substr( 0, split ) ) + " " + Pretty( value.substr( split ) );
		pending.clear();
	}
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
	struct Item
	{
		const char *path;
		uint64_t *handle;
	};
	const Item items[] = {
		{ "/actions/playspace/in/drag", &m_drag },
		{ "/actions/playspace/in/turn", &m_turn },
		{ "/actions/playspace/in/snapturnleft", &m_snapL },
		{ "/actions/playspace/in/snapturnright", &m_snapR },
		{ "/actions/playspace/in/heightup", &m_up },
		{ "/actions/playspace/in/heightdown", &m_down },
		{ "/actions/playspace/in/resetoriginal", &m_reset },
		{ "/actions/playspace/out/haptic", &m_haptic },
	};
	for ( const Item &it : items )
	{
		e = in->GetActionHandle( it.path, it.handle );
		if ( e != VRInputError_None )
		{
			error = std::string( "GetActionHandle(" ) + it.path + ") failed, error " + std::to_string( (int)e );
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
	LoadDefaultBindings( manifestPath.substr( 0, manifestPath.rfind( '/' ) + 1 ) + "bindings/frame_controller.json", m_defaults );
	return true;
}

bool Actions::Update( InputFrame &out )
{
	out = InputFrame();
	VRActiveActionSet_t active = {};
	active.ulActionSet = m_set;
	active.ulRestrictedToDevice = k_ulInvalidInputValueHandle;
	active.nPriority = m_priority;
	if ( VRInput()->UpdateActionState( &active, sizeof active, 1 ) != VRInputError_None )
		return false;

	bool changed;
	for ( int h = 0; h < 2; h++ )
	{
		Digital( m_drag, m_hand[h], out.drag[h], changed );
		Digital( m_turn, m_hand[h], out.turn[h], changed );
	}
	bool state;
	if ( Digital( m_snapL, k_ulInvalidInputValueHandle, state, changed ) )
		out.snapLeft = state && changed;
	if ( Digital( m_snapR, k_ulInvalidInputValueHandle, state, changed ) )
		out.snapRight = state && changed;
	Digital( m_up, k_ulInvalidInputValueHandle, out.heightUp, changed );
	Digital( m_down, k_ulInvalidInputValueHandle, out.heightDown, changed );
	Digital( m_reset, k_ulInvalidInputValueHandle, out.reset, changed );
	return true;
}

void Actions::Pulse( Hand hand, float seconds, float amplitude )
{
	VRInput()->TriggerHapticVibrationAction( m_haptic, 0.0f, seconds, 150.0f, amplitude, m_hand[(int)hand] );
}

std::vector<std::pair<std::string, std::string>> Actions::Bindings()
{
	struct Item
	{
		const char *label;
		uint64_t handle;
	};
	const Item items[] = {
		{ "Grab and move", m_drag },
		{ "Grab and turn", m_turn },
		{ "Snap turn left", m_snapL },
		{ "Snap turn right", m_snapR },
		{ "Raise me", m_up },
		{ "Lower me", m_down },
		{ "Reset to original", m_reset },
	};

	std::vector<std::pair<std::string, std::string>> out;
	int index = 0;
	int custom = -1; // lazily: does the user have saved bindings of their own
	for ( const Item &it : items )
	{
		const int i = index++;
		VRInputValueHandle_t origins[16] = {};
		std::string names;
		const EVRInputError err = VRInput()->GetActionOrigins( m_set, it.handle, origins, 16 );
		if ( err == VRInputError_None )
		{
			for ( VRInputValueHandle_t o : origins )
			{
				if ( o == k_ulInvalidInputValueHandle )
					continue;
				char buf[256] = {};
				if ( VRInput()->GetOriginLocalizedName( o, buf, sizeof buf, VRInputString_All ) == VRInputError_None && buf[0] )
				{
					std::string n = buf;
					for ( size_t p; ( p = n.find( " Frame Controller" ) ) != std::string::npos; ) // "Left Hand Frame Controller Bumper" -> "Left Hand Bumper"
						n.erase( p, 17 );
					names += ( names.empty() ? "" : ", " ) + n;
				}
			}
			if ( names.empty() )
			{
				// SteamVR reports no active origins while the dashboard has input focus, even though the bindings
				// exist. The binding description does not depend on that, so read it instead.
				InputBindingInfo_t info[16] = {};
				uint32_t count = 0;
				if ( VRInput()->GetActionBindingInfo( it.handle, info, sizeof( InputBindingInfo_t ), 16, &count ) == VRInputError_None )
				{
					std::vector<std::string> seen;
					for ( uint32_t k = 0; k < count && k < 16; k++ )
					{
						const std::string n = Pretty( info[k].rchDevicePathName ) + " " + Pretty( info[k].rchInputPathName );
						if ( std::find( seen.begin(), seen.end(), n ) != seen.end() )
							continue;
						seen.push_back( n );
						names += ( names.empty() ? "" : ", " ) + n;
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
			out.emplace_back( it.label, "unavailable (SteamVR Input error " + std::to_string( (int)err ) + ")" );
		else
			out.emplace_back( it.label, names.empty() ? "not bound" : names );
	}
	return out;
}

bool Actions::OpenBindingEditor( const std::string &appKey )
{
	return VRInput()->OpenBindingUI( appKey.c_str(), m_set, k_ulInvalidInputValueHandle, false ) == VRInputError_None;
}
