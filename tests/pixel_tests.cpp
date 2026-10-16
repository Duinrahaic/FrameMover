#include "pixels.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace
{
void Check( bool condition, const char *message )
{
	if ( !condition )
	{
		fprintf( stderr, "%s\n", message );
		exit( 1 );
	}
}
}

int main()
{
	// Exercise every alpha/channel combination, including malformed premultiplied
	// values that the original converter clamps, against its original arithmetic.
	for ( unsigned a = 0; a <= 255; ++a )
		for ( unsigned c = 0; c <= 255; ++c )
		{
			const uint8_t source[] = { (uint8_t)c, (uint8_t)( 255 - c ), (uint8_t)( c ^ 0x55 ), (uint8_t)a };
			uint8_t result[4];
			CopyARGB32ToRGBA( source, result, 1, 1, 4 );
			for ( unsigned i = 0; i < 3; ++i )
			{
				const unsigned expected = a ? std::min( 255u, source[2 - i] * 255u / a ) : 0;
				Check( result[i] == expected, "conversion differs from original converter" );
			}
			Check( result[3] == a, "alpha was changed" );
		}

	const uint8_t rows[] = {
		0, 0, 255, 255, 0, 128, 0, 128, 99, 99, 99, 99,
		255, 0, 0, 255, 80, 90, 100, 0, 99, 99, 99, 99
	};
	std::array<uint8_t, 18> result;
	result.fill( 42 );
	CopyARGB32ToRGBA( rows, result.data() + 1, 2, 2, 12 );
	const std::array<uint8_t, 18> expected = {
		42, 255, 0, 0, 255, 0, 255, 0, 128, 0, 0, 255, 255, 0, 0, 0, 0, 42
	};
	Check( result == expected, "RGBA order, row padding or destination bounds are wrong" );
	CopyARGB32ToRGBA( rows, result.data(), 0, 2, 12 );
	CopyARGB32ToRGBA( rows, result.data(), 2, 0, 12 );
	Check( result == expected, "empty images changed the destination" );
	puts( "pixel conversion tests passed" );
}
