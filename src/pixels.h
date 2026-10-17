#pragma once
#include <algorithm>
#include <cstdint>

// Cairo ARGB32 is premultiplied BGRA on the Frame; OpenVR expects straight RGBA.
inline void CopyARGB32ToRGBA( const uint8_t *src, uint8_t *dest, int width, int height, int stride )
{
	for ( int y = 0; y < height; ++y, src += stride, dest += width * 4 )
		for ( int x = 0; x < width; ++x )
		{
			const uint8_t *p = src + x * 4;
			uint8_t *d = dest + x * 4;
			const unsigned a = p[3];
			// The dashboard is opaque: avoid three divisions for almost every pixel.
			for ( unsigned i = 0; i < 3; ++i )
				d[i] = a == 255 ? p[2 - i] : a ? (uint8_t)std::min( 255u, p[2 - i] * 255u / a ) : 0;
			d[3] = (uint8_t)a;
		}
}
