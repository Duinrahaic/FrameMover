#include "config.h"
#include "files.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>

static int failures = 0;
#define CHECK(condition) do { if ( !(condition) ) { \
	std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition ); ++failures; } } while ( false )

struct Environment
{
	const char *key;
	std::string previous;
	bool present;
	static void Set( const char *key, const char *value )
	{
#ifdef _WIN32
		_putenv_s( key, value ? value : "" );
#else
		if ( value ) setenv( key, value, 1 );
		else unsetenv( key );
#endif
	}
	Environment( const char *name, const char *value ) : key( name ), present( getenv( key ) != nullptr )
	{
		if ( present ) previous = getenv( key );
		Set( key, value );
	}
	~Environment() { Set( key, present ? previous.c_str() : nullptr ); }
};

static bool Same( const Settings &a, const Settings &b )
{
	return a.dragMultiplier == b.dragMultiplier && a.snapDegrees == b.snapDegrees && a.heightSpeed == b.heightSpeed &&
		a.lockHeight == b.lockHeight && a.leftHand == b.leftHand && a.rightHand == b.rightHand && a.haptics == b.haptics &&
		a.doublePress == b.doublePress && a.disableChaperone == b.disableChaperone && a.largeChaperone == b.largeChaperone &&
		a.overrideGameInput == b.overrideGameInput;
}

int main()
{
	namespace fs = std::filesystem;
	const fs::path temp = fs::temp_directory_path();
	const fs::path root = temp / ( "frame-mover-config-test-" + std::to_string( std::chrono::steady_clock::now().time_since_epoch().count() ) );
	std::error_code ec;
	CHECK( root.is_absolute() && fs::equivalent( root.parent_path(), temp, ec ) && !ec );
	if ( failures || !fs::create_directory( root, ec ) )
		return 1; // never remove an existing directory
	{
		const std::string base = root.generic_string();
		const std::string home = base + "/home", xdg = base + "/xdg/nested";
		Environment homeEnv( "HOME", home.c_str() ), configEnv( "XDG_CONFIG_HOME", xdg.c_str() );
		CHECK( files::Home() == home );
		CHECK( ConfigDir() == xdg + "/frame-mover" );
		{
			Environment fallback( "XDG_CONFIG_HOME", nullptr );
			CHECK( ConfigDir() == home + "/.config/frame-mover" );
		}
		CHECK( Same( LoadSettings(), Settings() ) );

		// Exercise multiple read-buffer lengths, including embedded NUL and non-ASCII bytes.
		const std::string binaryFile = base + "/nested/files/data.bin";
		files::MakeDirs( base + "/nested/files" );
		std::string binary( 10003, '\0' ), read = "previous content";
		for ( size_t i = 0; i < binary.size(); ++i ) binary[i] = (char)( i * 37 );
		CHECK( files::WriteFile( binaryFile, binary ) );
		CHECK( fs::file_size( binaryFile, ec ) == binary.size() && !ec );
		CHECK( files::ReadFile( binaryFile, read ) && read == binary );
		CHECK( files::WriteFile( binaryFile, "short" ) );
		CHECK( files::ReadFile( binaryFile, read ) && read == "short" );
		CHECK( files::WriteFile( binaryFile, "" ) );
		CHECK( files::ReadFile( binaryFile, read ) && read.empty() );
		read = "preserved on open failure";
		CHECK( !files::ReadFile( base + "/missing", read ) && read == "preserved on open failure" );
		CHECK( !files::WriteFile( base + "/missing/parent/file", binary ) );
		CHECK( !files::WriteFile( base + "/nested", binary ) );
		CHECK( !files::ReadFile( base + "/nested", read ) );
#ifndef _WIN32
		if ( fs::exists( "/dev/full" ) ) CHECK( !files::WriteFile( "/dev/full", "delayed write failure" ) );
#endif

		const std::string unrelated = base + "/unrelated.bin";
		CHECK( files::WriteFile( unrelated, binary ) );
		Settings s;
		s.dragMultiplier = 1.23456f;
		s.snapDegrees = 123.456f;
		s.heightSpeed = 0.678901f;
		s.lockHeight = true;
		s.leftHand = s.rightHand = s.haptics = s.doublePress = s.largeChaperone = false;
		s.disableChaperone = s.overrideGameInput = true;
		SaveSettings( s ); // creates all missing config-parent directories
		CHECK( Same( LoadSettings(), s ) );
		const std::string configFile = ConfigDir() + "/config.ini";
		CHECK( files::ReadFile( configFile, read ) );
		CHECK( read == "dragMultiplier=1.23456\nsnapDegrees=123.456\nheightSpeed=0.678901\n"
			"lockHeight=1\nleftHand=0\nrightHand=0\nhaptics=0\ndoublePress=0\n"
			"largeChaperone=0\ndisableChaperone=1\noverrideGameInput=1\n" );
		s.dragMultiplier = 1.23456789f;
		SaveSettings( s );
		CHECK( LoadSettings().dragMultiplier == 1.23457f );
		CHECK( files::ReadFile( unrelated, read ) && read == binary );

		s.dragMultiplier = -100;
		s.snapDegrees = 1000;
		s.heightSpeed = -10;
		Settings clamped = Clamped( s );
		CHECK( clamped.dragMultiplier == 0.25f && clamped.snapDegrees == 180 && clamped.heightSpeed == 0.1f );
		CHECK( clamped.lockHeight && !clamped.rightHand && clamped.overrideGameInput );
		s.dragMultiplier = 100;
		s.snapDegrees = -100;
		s.heightSpeed = 100;
		clamped = Clamped( s );
		CHECK( clamped.dragMultiplier == 4 && clamped.snapDegrees == 5 && clamped.heightSpeed == 2 );
		for ( float bad : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() } )
		{
			Settings invalid;
			invalid.dragMultiplier = invalid.snapDegrees = invalid.heightSpeed = bad;
			CHECK( Same( Clamped( invalid ), Settings() ) );
		}

		const std::string custom = "unknown=unchanged\ninvalid line\ndragMultiplier=3\ndragMultiplier=2.5\nsnapDegrees=15\n"
			"heightSpeed=1.25\nlockHeight=0\nleftHand=0\nrightHand=1\nhaptics=0\ndoublePress=1\n"
			"largeChaperone=0\ndisableChaperone=1\noverrideGameInput=0";
		CHECK( files::WriteFile( configFile, custom ) );
		const Settings loaded = LoadSettings();
		CHECK( loaded.dragMultiplier == 2.5f && loaded.snapDegrees == 15 && loaded.heightSpeed == 1.25f );
		CHECK( !loaded.lockHeight && !loaded.leftHand && loaded.rightHand && !loaded.haptics && loaded.doublePress );
		CHECK( !loaded.largeChaperone && loaded.disableChaperone && !loaded.overrideGameInput );
		CHECK( files::ReadFile( configFile, read ) && read == custom ); // loading never rewrites unrelated/unknown data
		CHECK( files::WriteFile( configFile, "dragMultiplier=nan\nsnapDegrees=inf\nheightSpeed=-inf\n" ) );
		CHECK( Same( LoadSettings(), Settings() ) );
		CHECK( files::WriteFile( configFile, "dragMultiplier=\n4\nheightSpeed=1.5" ) );
		CHECK( LoadSettings().dragMultiplier == 0.25f && LoadSettings().heightSpeed == 1.5f );
	}
	fs::remove_all( root, ec );
	CHECK( !ec );
	std::printf( "%s: binary file I/O, config roundtrip, precision, ranges and invalid settings\n", failures ? "FAIL" : "PASS" );
	return failures ? 1 : 0;
}
