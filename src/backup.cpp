#include "backup.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{
std::string Home()
{
	const char *h = getenv( "HOME" );
	return h ? h : "";
}

bool ReadAll( const std::string &path, std::string &out )
{
	std::ifstream in( path, std::ios::binary );
	if ( !in )
		return false;
	out.assign( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
	return true;
}

void MakeDirs( const std::string &dir )
{
	for ( size_t i = 1; i <= dir.size(); i++ )
		if ( i == dir.size() || dir[i] == '/' )
			mkdir( dir.substr( 0, i ).c_str(), 0755 );
}
} // namespace

std::string BackupChaperoneFile()
{
	const std::string src = Home() + "/.config/openvr/config/chaperone_info.vrchap";
	std::string content;
	if ( Home().empty() || !ReadAll( src, content ) || content.empty() )
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
			if ( ReadAll( dir + "/" + name, other ) && other == content )
			{
				closedir( d );
				return dir; // this exact chaperone is already saved
			}
			if ( name.rfind( "chaperone_info.2", 0 ) == 0 ) // timestamped copies we made; "initial" is never pruned
				timestamped.push_back( name );
		}
		closedir( d );
	}

	char stamp[32];
	const time_t t = time( nullptr );
	struct tm tmv;
	localtime_r( &t, &tmv );
	strftime( stamp, sizeof stamp, "%Y%m%d-%H%M%S", &tmv );
	{
		std::ofstream out( dir + "/chaperone_info." + stamp + ".vrchap", std::ios::binary | std::ios::trunc );
		out << content;
	}
	std::sort( timestamped.begin(), timestamped.end() );
	while ( timestamped.size() > 9 )
	{
		unlink( ( dir + "/" + timestamped.front() ).c_str() );
		timestamped.erase( timestamped.begin() );
	}
	return dir;
}
