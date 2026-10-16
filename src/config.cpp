#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>

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
	s.dragMultiplier = std::clamp( s.dragMultiplier, 0.25f, 4.0f );
	s.snapDegrees = std::clamp( s.snapDegrees, 5.0f, 180.0f );
	s.heightSpeed = std::clamp( s.heightSpeed, 0.1f, 2.0f );
	return s;
}

Settings LoadSettings()
{
	Settings s;
	std::ifstream in( ConfigDir() + "/config.ini" );
	std::string line;
	while ( std::getline( in, line ) )
	{
		const size_t eq = line.find( '=' );
		if ( eq == std::string::npos )
			continue;
		const std::string k = line.substr( 0, eq ), v = line.substr( eq + 1 );
		if ( k == "dragMultiplier" ) s.dragMultiplier = (float)atof( v.c_str() );
		else if ( k == "snapDegrees" ) s.snapDegrees = (float)atof( v.c_str() );
		else if ( k == "heightSpeed" ) s.heightSpeed = (float)atof( v.c_str() );
		else if ( k == "lockHeight" ) s.lockHeight = v == "1";
		else if ( k == "leftHand" ) s.leftHand = v == "1";
		else if ( k == "rightHand" ) s.rightHand = v == "1";
		else if ( k == "haptics" ) s.haptics = v == "1";
		else if ( k == "doublePress" ) s.doublePress = v == "1";
		else if ( k == "largeChaperone" ) s.largeChaperone = v == "1";
		else if ( k == "disableChaperone" ) s.disableChaperone = v == "1";
		else if ( k == "overrideGameInput" ) s.overrideGameInput = v == "1";
	}
	return Clamped( s );
}

void SaveSettings( const Settings &s )
{
	mkdir( ConfigDir().c_str(), 0755 );
	std::ofstream out( ConfigDir() + "/config.ini", std::ios::trunc );
	out << "dragMultiplier=" << s.dragMultiplier << "\n"
		<< "snapDegrees=" << s.snapDegrees << "\n"
		<< "heightSpeed=" << s.heightSpeed << "\n"
		<< "lockHeight=" << ( s.lockHeight ? 1 : 0 ) << "\n"
		<< "leftHand=" << ( s.leftHand ? 1 : 0 ) << "\n"
		<< "rightHand=" << ( s.rightHand ? 1 : 0 ) << "\n"
		<< "haptics=" << ( s.haptics ? 1 : 0 ) << "\n"
		<< "doublePress=" << ( s.doublePress ? 1 : 0 ) << "\n"
		<< "largeChaperone=" << ( s.largeChaperone ? 1 : 0 ) << "\n"
		<< "disableChaperone=" << ( s.disableChaperone ? 1 : 0 ) << "\n"
		<< "overrideGameInput=" << ( s.overrideGameInput ? 1 : 0 ) << "\n";
}
