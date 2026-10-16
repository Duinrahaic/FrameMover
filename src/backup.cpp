#include "backup.h"
#include "files.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include <dirent.h>
#include <unistd.h>

using namespace files;

std::string BackupChaperoneFile()
{
	const std::string src = Home() + "/.config/openvr/config/chaperone_info.vrchap";
	std::string content;
	if ( Home().empty() || !ReadFile( src, content ) || content.empty() )
		return "";

	const std::string dir = Home() + "/.local/share/frame-mover/backup";
	MakeDirs( dir );

	std::vector<std::string> timestamped;
	if ( DIR *d = opendir( dir.c_str() ) )
	{
		while ( dirent *e = readdir( d ) )
		{
			const std::string name = e->d_name;
			if ( name.size() < 8 || name.compare( name.size() - 7, 7, ".vrchap" ) != 0 )
				continue;
			std::string other;
			if ( ReadFile( dir + "/" + name, other ) && other == content )
			{
				closedir( d );
				return dir; // this exact chaperone is already saved
			}
			if ( name.rfind( "chaperone_info.2", 0 ) == 0 ) // timestamped copies we made; "initial" is never pruned
				timestamped.push_back( name );
		}
		closedir( d );
	}

	char stamp[64];
	timespec now;
	if ( clock_gettime( CLOCK_REALTIME, &now ) != 0 )
		return "";
	struct tm tmv;
	if ( !localtime_r( &now.tv_sec, &tmv ) )
		return "";
	const size_t n = strftime( stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tmv );
	snprintf( stamp + n, sizeof stamp - n, "-%09ld", now.tv_nsec );
	if ( !WriteFile( dir + "/chaperone_info." + stamp + ".vrchap", content ) )
		return ""; // never prune old backups after a failed write
	std::sort( timestamped.begin(), timestamped.end() );
	for ( size_t i = 9; i < timestamped.size(); i++ )
		unlink( ( dir + "/" + timestamped[i - 9] ).c_str() );
	return dir;
}
