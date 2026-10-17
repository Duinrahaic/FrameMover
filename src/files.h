#pragma once

#include <cstdlib>
#include <cstdio>
#include <string>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace files
{
inline std::string Home()
{
	const char *home = getenv( "HOME" );
	return home ? home : "";
}

inline void MakeDirs( const std::string &dir )
{
	for ( size_t i = 1; i <= dir.size(); i++ )
		if ( i == dir.size() || dir[i] == '/' )
		{
#ifdef _WIN32
			_mkdir( dir.substr( 0, i ).c_str() );
#else
			mkdir( dir.substr( 0, i ).c_str(), 0755 );
#endif
		}
}

inline bool ReadFile( const std::string &path, std::string &out )
{
	FILE *in = std::fopen( path.c_str(), "rb" );
	if ( !in )
		return false;
	out.clear();
	char buffer[4096];
	for ( size_t n; ( n = std::fread( buffer, 1, sizeof buffer, in ) ) != 0; )
		out.append( buffer, n );
	const bool good = !std::ferror( in );
	return std::fclose( in ) == 0 && good;
}

inline bool WriteFile( const std::string &path, const std::string &text )
{
	FILE *out = std::fopen( path.c_str(), "wb" );
	if ( !out )
		return false;
	const bool good = std::fwrite( text.data(), 1, text.size(), out ) == text.size() && !std::ferror( out );
	return std::fclose( out ) == 0 && good; // also report a full disk or delayed write error
}
} // namespace files
