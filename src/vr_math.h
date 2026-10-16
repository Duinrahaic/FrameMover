#pragma once
#include "playspace.h"
#include <openvr.h>

// Convert the model's doubles and OpenVR's float wire representation in one place.
template<class To, class From>
To MatrixCast( const From &src )
{
	To dst;
	for ( int r = 0; r < 3; ++r )
		for ( int c = 0; c < 4; ++c )
			dst.m[r][c] = src.m[r][c];
	return dst;
}
