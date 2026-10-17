#include "config.h"
#include "files.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <string_view>

namespace
{
template<typename T> struct Field
{
	const char *name;
	T Settings::*value;
};
constexpr Field<float> kFloats[] = {
	{ "dragMultiplier", &Settings::dragMultiplier },
	{ "snapDegrees", &Settings::snapDegrees },
	{ "heightSpeed", &Settings::heightSpeed },
};
constexpr Field<bool> kBools[] = {
	{ "lockHeight", &Settings::lockHeight },
	{ "leftHand", &Settings::leftHand },
	{ "rightHand", &Settings::rightHand },
	{ "haptics", &Settings::haptics },
	{ "doublePress", &Settings::doublePress },
	{ "largeChaperone", &Settings::largeChaperone },
	{ "disableChaperone", &Settings::disableChaperone },
	{ "overrideGameInput", &Settings::overrideGameInput },
};
} // namespace

std::string ConfigDir()
{
	const char *xdg = getenv( "XDG_CONFIG_HOME" );
	std::string base;
	if ( xdg && *xdg )
		base = xdg;
	else if ( const char *home = getenv( "HOME" ) )
		base = std::string( home ) + "/.config";
	else
		base = ".";
	return base + "/frame-mover";
}

Settings Clamped( Settings s )
{
	const Settings defaults;
	for ( const auto &f : kFloats )
		if ( !std::isfinite( s.*f.value ) )
			s.*f.value = defaults.*f.value;
	s.dragMultiplier = std::clamp( s.dragMultiplier, 0.25f, 4.0f );
	s.snapDegrees = std::clamp( s.snapDegrees, 5.0f, 180.0f );
	s.heightSpeed = std::clamp( s.heightSpeed, 0.1f, 2.0f );
	return s;
}

Settings LoadSettings()
{
	Settings s;
	std::string text;
	if ( !files::ReadFile( ConfigDir() + "/config.ini", text ) )
		return s;
	for ( size_t pos = 0; pos < text.size(); )
	{
		size_t end = text.find( '\n', pos );
		if ( end == std::string::npos )
			end = text.size();
		else
			text[end] = '\0'; // keep strtof bounded to this line without allocating a substring
		const std::string_view line( text.data() + pos, end - pos );
		pos = end + 1;
		const size_t eq = line.find( '=' );
		if ( eq == std::string::npos )
			continue;
		const std::string_view k( line.data(), eq ), v( line.data() + eq + 1, line.size() - eq - 1 );
		for ( const auto &f : kFloats )
			if ( k == f.name )
				s.*f.value = strtof( v.data(), nullptr );
		for ( const auto &f : kBools )
			if ( k == f.name )
				s.*f.value = v == "1";
	}
	return Clamped( s );
}

void SaveSettings( const Settings &s )
{
	const std::string dir = ConfigDir();
	files::MakeDirs( dir );
	FILE *out = std::fopen( ( dir + "/config.ini" ).c_str(), "wb" );
	if ( !out )
		return;
	for ( const auto &f : kFloats )
		std::fprintf( out, "%s=%.6g\n", f.name, s.*f.value );
	for ( const auto &f : kBools )
		std::fprintf( out, "%s=%d\n", f.name, s.*f.value );
	std::fclose( out );
}
