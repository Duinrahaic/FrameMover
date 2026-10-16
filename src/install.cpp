#include "install.h"

#include "app.h"
#include "backup.h"
#include "files.h"
#include "panel.h"
#include "ui.h"

#include <openvr.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace vr;
using namespace files;

namespace
{
struct Paths
{
	std::string bin = Home() + "/.local/bin/frame-mover";
	std::string dataDir = Home() + "/.local/share/frame-mover";
	std::string actionManifest = dataDir + "/action_manifest.json";
	std::string binding = dataDir + "/bindings/frame_controller.json";
	std::string manifest = dataDir + "/frame-mover.vrmanifest";
	std::string iconDir = Home() + "/.local/share/icons/hicolor/128x128/apps";
	std::string icon = iconDir + "/frame-mover.png";
	std::string header = dataDir + "/header.png";
	std::string capsule = dataDir + "/capsule.png";
	std::string desktop = Home() + "/.local/share/applications/frame-mover.desktop";
};

bool CopyFile( const std::string &from, const std::string &to )
{
	std::string data;
	return ReadFile( from, data ) && WriteFile( to, data );
}

// Copies through a temporary file and a rename, so a running copy is never overwritten in place.
bool CopyExecutable( const std::string &from, const std::string &to )
{
	const std::string tmp = to + ".new";
	if ( CopyFile( from, tmp ) && chmod( tmp.c_str(), 0755 ) == 0 && rename( tmp.c_str(), to.c_str() ) == 0 )
		return true;
	unlink( tmp.c_str() );
	return false;
}

std::string SelfPath()
{
	char exe[4096];
	const ssize_t n = readlink( "/proc/self/exe", exe, sizeof exe - 1 );
	return n > 0 ? std::string( exe, (size_t)n ) : std::string();
}

// Glyph and title, kept inside 88 % of the width so the text never touches the edges.
bool WriteArt( const std::string &path, int w, int h, double glyphFraction, double glyphY, double titleY, double titleSize )
{
	Canvas c( w, h );
	c.Clear( 0.07, 0.08, 0.11 );
	DrawGlyph( c, w / 2.0, h * glyphY, w * glyphFraction );
	const std::string title = "FrameMover";
	const double width = c.TextWidth( title, titleSize, true );
	if ( width > w * 0.88 )
		titleSize *= w * 0.88 / width;
	c.Text( title, ( w - c.TextWidth( title, titleSize, true ) ) / 2, h * titleY, titleSize, 1, 1, 1, true );
	return c.WritePng( path.c_str() );
}

// Launcher icon: the glyph's own rounded square on a transparent surround. The alpha channel matters: cairo writes opaque images as
// RGB, which Steam's icon loader rejected for the other apps' RGB icons.
bool WriteIcon( const std::string &path, int size )
{
	Canvas c( size, size );
	DrawGlyph( c, size / 2.0, size / 2.0, size * 0.875 );
	return c.WritePng( path.c_str() );
}

std::string ManifestJson( const Paths &p )
{
	return std::string( "{\n  \"source\": \"user\",\n  \"applications\": [\n    {\n" ) + "      \"app_key\": \"" + kAppKey + "\",\n" +
		"      \"launch_type\": \"binary\",\n" + "      \"binary_path_linux\": \"" + p.bin + "\",\n" +
		"      \"binary_path_linux_arm\": \"" + p.bin + "\",\n" + "      \"action_manifest_path\": \"" + p.actionManifest + "\",\n" +
		"      \"image_path\": \"header.png\",\n" + "      \"image_path_capsule\": \"capsule.png\",\n" +
		"      \"is_dashboard_overlay\": true,\n" + "      \"show_overlay_in_quicklaunch\": true,\n" +
		"      \"strings\": { \"en_us\": { \"name\": \"FrameMover\", \"description\": \"Move, turn and reset your playspace\" } }\n" +
		"    }\n  ]\n}\n";
}

// The Frame's launcher lists the user's desktop entries; the SteamVR manifest alone does not put the app there.
// The icon is referenced by theme name (hicolor), as the other Frame apps do, so the launcher resolves it the same way.
// Launching it again while it runs just asks the running copy to show its dashboard tab (see BecomeOnlyInstance).
std::string DesktopEntry( const Paths &p )
{
	return "[Desktop Entry]\nType=Application\nName=FrameMover\nComment=Move, turn and reset your playspace\n"
		   "Exec=\"" + p.bin + "\" --show-dashboard\nIcon=frame-mover\nTerminal=false\nCategories=Utility;\n";
}

void Run( const char *prog, std::vector<std::string> args )
{
	args.insert( args.begin(), prog );
	std::vector<char *> argv;
	for ( std::string &a : args )
		argv.push_back( a.data() );
	argv.push_back( nullptr );
	pid_t pid;
	if ( posix_spawnp( &pid, prog, nullptr, nullptr, argv.data(), environ ) == 0 )
		waitpid( pid, nullptr, 0 );
}

void RefreshLauncherCaches()
{
	Run( "update-desktop-database", { Home() + "/.local/share/applications" } );
	Run( "gtk-update-icon-cache", { "-f", "-t", Home() + "/.local/share/icons/hicolor" } );
}

std::string LockPath()
{
	const char *rt = getenv( "XDG_RUNTIME_DIR" );
	return ( rt && *rt ? std::string( rt ) : std::string( "/tmp" ) ) + "/frame-mover." + std::to_string( getuid() ) + ".lock";
}
} // namespace

int InstallApp()
{
	const Paths p;
	const std::string self = SelfPath();
	if ( self.empty() || Home().empty() )
	{
		fprintf( stderr, "cannot resolve the executable or home directory\n" );
		return 1;
	}
	const std::string srcData = FindDataDir();
	if ( srcData == p.dataDir && access( p.actionManifest.c_str(), R_OK ) != 0 )
	{
		fprintf( stderr, "cannot find the data directory (action_manifest.json) next to the program\n" );
		return 1;
	}

	MakeDirs( Home() + "/.local/bin" );
	MakeDirs( p.dataDir + "/bindings" );
	if ( self != p.bin && !CopyExecutable( self, p.bin ) )
	{
		fprintf( stderr, "cannot install %s\n", p.bin.c_str() );
		return 1;
	}
	if ( srcData != p.dataDir &&
		( !CopyFile( srcData + "/action_manifest.json", p.actionManifest ) || !CopyFile( srcData + "/bindings/frame_controller.json", p.binding ) ) )
	{
		fprintf( stderr, "cannot copy the action manifest and default bindings from %s\n", srcData.c_str() );
		return 1;
	}
	MakeDirs( Home() + "/.local/share/applications" );
	MakeDirs( p.iconDir );
	if ( !WriteIcon( p.icon, 128 ) || !WriteArt( p.header, 920, 430, 0.29, 0.385, 0.90, 56 ) ||
		!WriteArt( p.capsule, 600, 900, 0.62, 0.38, 0.69, 80 ) || !WriteFile( p.manifest, ManifestJson( p ) ) ||
		!WriteFile( p.desktop, DesktopEntry( p ) ) )
	{
		fprintf( stderr, "cannot write the SteamVR manifest, desktop entry, icon and artwork under %s\n", p.dataDir.c_str() );
		return 1;
	}
	RefreshLauncherCaches();
	printf( "installed %s and its data in %s, and the desktop entry %s\n", p.bin.c_str(), p.dataDir.c_str(), p.desktop.c_str() );
	const std::string backups = BackupChaperoneFile();
	if ( !backups.empty() )
		printf( "saved a copy of your current chaperone file in %s\n", backups.c_str() );

	EVRInitError err = VRInitError_None;
	VR_Init( &err, VRApplication_Utility );
	if ( err != VRInitError_None )
	{
		fprintf( stderr, "cannot connect to SteamVR (%s)%s: the files are installed, but SteamVR does not know about the app yet.\n"
						 "Start SteamVR and run `%s --install` again.\n",
			VR_GetVRInitErrorAsEnglishDescription( err ),
			err == VRInitError_Init_PathRegistryNotFound ? ", it cannot find ~/.config/openvr/openvrpaths.vrpath" : "", p.bin.c_str() );
		return 2;
	}
	IVRApplications *apps = VRApplications();
	EVRApplicationError ae = apps->AddApplicationManifest( p.manifest.c_str(), false );
	if ( ae == VRApplicationError_None )
		ae = apps->SetApplicationAutoLaunch( kAppKey, true );
	if ( ae != VRApplicationError_None )
		fprintf( stderr, "SteamVR rejected the manifest: %s\n", apps->GetApplicationsErrorNameFromEnum( ae ) );
	VR_Shutdown();
	if ( ae != VRApplicationError_None )
		return 1;
	printf( "registered with SteamVR: listed in the quick-launch bar, starts automatically with SteamVR\n" );
	return 0;
}

int UninstallApp()
{
	const Paths p;
	EVRInitError err = VRInitError_None;
	VR_Init( &err, VRApplication_Utility );
	if ( err == VRInitError_None )
	{
		IVRApplications *apps = VRApplications();
		apps->SetApplicationAutoLaunch( kAppKey, false );
		apps->RemoveApplicationManifest( p.manifest.c_str() );
		VR_Shutdown();
	}
	else
		fprintf( stderr, "cannot connect to SteamVR (%s): its registration cannot be removed now; it disappears once the manifest file is gone.\n",
			VR_GetVRInitErrorAsEnglishDescription( err ) );
	for ( const std::string &f : { p.manifest, p.icon, p.dataDir + "/icon.png" /* before the icon moved to the hicolor theme */, p.header, p.capsule,
			   p.actionManifest, p.binding, p.desktop } )
		unlink( f.c_str() );
	rmdir( ( p.dataDir + "/bindings" ).c_str() );
	rmdir( p.dataDir.c_str() ); // fails (and keeps everything) while chaperone backups are in it
	unlink( p.bin.c_str() );
	RefreshLauncherCaches();
	printf( "removed the program, its desktop entry and its SteamVR registration (settings in ~/.config/frame-mover and chaperone backups are kept)\n" );
	return 0;
}

bool BecomeOnlyInstance()
{
	const int fd = open( LockPath().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600 );
	if ( fd >= 0 && flock( fd, LOCK_EX | LOCK_NB ) == 0 )
	{
		const std::string pid = std::to_string( getpid() );
		if ( ftruncate( fd, 0 ) == 0 )
			(void)!write( fd, pid.c_str(), pid.size() );
		return true; // the descriptor stays open (close-on-exec) for the life of the process
	}
	// SteamVR starts the program again when its quick-launch tile is pressed: hand over to the running copy, which
	// opens its dashboard tab.
	char buf[32] = {};
	const ssize_t n = fd >= 0 ? read( fd, buf, sizeof buf - 1 ) : -1;
	const long pid = n > 0 ? atol( buf ) : 0;
	if ( fd >= 0 )
		close( fd );
	if ( pid > 1 && kill( (pid_t)pid, SIGUSR1 ) == 0 )
		fprintf( stderr, "frame-mover is already running; asked it to show its dashboard tab\n" );
	else
		fprintf( stderr, "frame-mover is already running\n" );
	return false;
}
