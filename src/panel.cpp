#include "panel.h"

#include <cmath>
#include <cstdio>

namespace
{
constexpr double kLeftX = 40, kLeftW = 740;
constexpr double kRightX = 840, kRightW = 720;

std::string Fmt( const char *f, double a )
{
	char b[64];
	snprintf( b, sizeof b, f, a );
	return b;
}

// Shortens `s` with an ellipsis until it fits `maxWidth` at the given size.
std::string Fit( Canvas &c, std::string s, double size, double maxWidth )
{
	if ( c.TextWidth( s, size ) <= maxWidth )
		return s;
	while ( !s.empty() && c.TextWidth( s + "...", size ) > maxWidth )
		s.pop_back();
	return s + "...";
}

// "label     [-]  value  [+]"
void Stepper( Canvas &c, ButtonPanel &bp, double y, const std::string &label, const std::string &value, std::function<void()> dec,
	std::function<void()> inc )
{
	c.Text( label, kLeftX, y + 34, 25, 0.85, 0.88, 0.92 );
	bp.Add( { 470, y, 70, 48, "-", false, std::move( dec ) } );
	c.Text( value, 550 + ( 160 - c.TextWidth( value, 25, true ) ) / 2, y + 34, 25, 1, 1, 1, true );
	bp.Add( { 710, y, 70, 48, "+", false, std::move( inc ) } );
}

void Toggle( ButtonPanel &bp, double x, double y, const std::string &label, bool on, std::function<void()> flip )
{
	bp.Add( { x, y, 362, 50, label + ( on ? ":  ON" : ":  OFF" ), on, std::move( flip ) } );
}
} // namespace

void DrawPanel( Canvas &c, ButtonPanel &bp, const PanelModel &m, const PanelCallbacks &cb, int hovered )
{
	bp.Clear();
	c.Clear( 0.07, 0.08, 0.11 );

	// ---- left column: state, reset, quick moves, settings -------------------------------------------------------------
	c.Text( "FrameMover", kLeftX, 64, 42, 1, 1, 1, true );

	std::string status;
	double sr = 0.55, sg = 0.85, sb = 0.6;
	switch ( m.state )
	{
	case MoverState::Ready:
		status = m.settings.doublePress ? "Ready. Double-press and hold the grab button, then move your hand."
										: "Ready. Hold the grab button and move your hand.";
		break;
	case MoverState::PausedBySetup:
		status = "Paused: SteamVR is editing the chaperone (room setup).";
		sr = 0.95, sg = 0.75, sb = 0.3;
		break;
	default:
		status = "Waiting for SteamVR to report your playspace.";
		sr = 0.95, sg = 0.75, sb = 0.3;
		break;
	}
	c.Text( Fit( c, status, 22, kLeftW ), kLeftX, 102, 22, sr, sg, sb );

	bp.Add( { kLeftX, 124, kLeftW, 84, "Reset to original playspace", false, cb.reset } );

	if ( m.moved )
	{
		const double dist = std::sqrt( m.offset.x * m.offset.x + m.offset.y * m.offset.y + m.offset.z * m.offset.z );
		c.Text( Fit( c,
					"Moved " + Fmt( "%.2f m", dist ) + " (x " + Fmt( "%+.2f", m.offset.x ) + ", y " + Fmt( "%+.2f", m.offset.y ) + ", z " +
						Fmt( "%+.2f", m.offset.z ) + "), turned " + Fmt( "%.1f", m.yawDegrees ) + "\xC2\xB0",
					23, kLeftW ),
			kLeftX, 246, 23, 1, 1, 1 );
	}
	else
		c.Text( "Your playspace is exactly as you saved it.", kLeftX, 246, 23, 0.7, 0.75, 0.8 );

	const Settings &s = m.settings;
	const std::string snap = Fmt( "%.0f", s.snapDegrees ) + "\xC2\xB0";
	bp.Add( { kLeftX, 266, 362, 58, "Raise me 5 cm", false, [cb] { if ( cb.raise ) cb.raise( 0.05 ); } } );
	bp.Add( { 418, 266, 362, 58, "Lower me 5 cm", false, [cb] { if ( cb.raise ) cb.raise( -0.05 ); } } );
	bp.Add( { kLeftX, 334, 362, 58, "Turn left " + snap, false, [cb, s] { if ( cb.turn ) cb.turn( -s.snapDegrees ); } } );
	bp.Add( { 418, 334, 362, 58, "Turn right " + snap, false, [cb, s] { if ( cb.turn ) cb.turn( s.snapDegrees ); } } );

	auto change = [cb, s]( auto fn ) {
		return [cb, s, fn] {
			Settings n = s;
			fn( n );
			if ( cb.settingsChanged )
				cb.settingsChanged( Clamped( n ) );
		};
	};
	Stepper( c, bp, 424, "Grab speed", Fmt( "%.2f", s.dragMultiplier ) + "x",
		change( [] ( Settings &n ) { n.dragMultiplier = std::round( ( n.dragMultiplier - 0.25f ) * 4 ) / 4; } ),
		change( [] ( Settings &n ) { n.dragMultiplier = std::round( ( n.dragMultiplier + 0.25f ) * 4 ) / 4; } ) );
	Stepper( c, bp, 482, "Snap turn angle", snap, change( [] ( Settings &n ) { n.snapDegrees -= 15; } ),
		change( [] ( Settings &n ) { n.snapDegrees += 15; } ) );
	Stepper( c, bp, 540, "Raise / lower speed", Fmt( "%.1f", s.heightSpeed ) + " m/s", change( [] ( Settings &n ) { n.heightSpeed -= 0.1f; } ),
		change( [] ( Settings &n ) { n.heightSpeed += 0.1f; } ) );

	Toggle( bp, kLeftX, 612, "Lock height", s.lockHeight, change( [] ( Settings &n ) { n.lockHeight = !n.lockHeight; } ) );
	Toggle( bp, 418, 612, "Haptics", s.haptics, change( [] ( Settings &n ) { n.haptics = !n.haptics; } ) );
	Toggle( bp, kLeftX, 674, "Left hand grabs", s.leftHand, change( [] ( Settings &n ) { n.leftHand = !n.leftHand; } ) );
	Toggle( bp, 418, 674, "Right hand grabs", s.rightHand, change( [] ( Settings &n ) { n.rightHand = !n.rightHand; } ) );

	bp.Add( { kLeftX, 736, kLeftW, 50, std::string( "Double-press + hold to grab:  " ) + ( s.doublePress ? "ON" : "OFF" ), s.doublePress,
		change( [] ( Settings &n ) { n.doublePress = !n.doublePress; } ) } );

	Toggle( bp, kLeftX, 794, "Huge chaperone", s.largeChaperone, change( [] ( Settings &n ) { n.largeChaperone = !n.largeChaperone; } ) );
	Toggle( bp, 418, 794, "No chaperone", s.disableChaperone, change( [] ( Settings &n ) { n.disableChaperone = !n.disableChaperone; } ) );

	c.Text( "Nothing is saved to SteamVR: Reset restores exactly the playspace", kLeftX, 868, 20, 0.55, 0.6, 0.66 );
	c.Text( "and chaperone you had set.", kLeftX, 892, 20, 0.55, 0.6, 0.66 );

	// ---- right column: key binds ---------------------------------------------------------------------------------------
	c.Text( "Button bindings", kRightX, 64, 34, 1, 1, 1, true );
	if ( m.controllersConnected )
		c.Text( "SteamVR Input: changes you make in the editor show up here at once", kRightX, 102, 20, 0.55, 0.6, 0.66 );
	else
		c.Text( "No controller connected yet: bindings appear once SteamVR sees one.", kRightX, 102, 20, 0.95, 0.75, 0.3 );
	double y = 150;
	if ( !m.inputError.empty() )
	{
		c.Text( Fit( c, "SteamVR Input unavailable: " + m.inputError, 20, kRightW ), kRightX, y, 20, 0.95, 0.5, 0.5 );
		y += 36;
	}
	for ( const auto &b : m.bindings )
	{
		const bool bound = b.second != "not bound" && b.second.rfind( "unavailable", 0 ) != 0;
		c.Text( b.first, kRightX, y, 24, 0.95, 0.96, 0.98, true );
		c.Text( Fit( c, b.second, 21, kRightW ), kRightX, y + 28, 21, bound ? 0.6 : 0.95, bound ? 0.8 : 0.6, bound ? 1.0 : 0.5 );
		y += 72;
	}
	bp.Add( { kRightX, 782, kRightW, 70, "Edit bindings in SteamVR", false, cb.editBindings } );

	bp.Draw( c, hovered );
}

void DrawGlyph( Canvas &c, double cx, double cy, double size )
{
	const double k = size / 112.0;
	cairo_t *cr = c.cr();
	cairo_save( cr );
	cairo_translate( cr, cx, cy );
	cairo_scale( cr, k, k );
	c.RoundRect( -56, -56, 112, 112, 24, 0.15, 0.42, 0.72 );
	cairo_set_source_rgb( cr, 1, 1, 1 );
	cairo_set_line_width( cr, 9 );
	cairo_set_line_cap( cr, CAIRO_LINE_CAP_ROUND );
	cairo_set_line_join( cr, CAIRO_LINE_JOIN_ROUND );
	for ( int i = 0; i < 4; i++ )
	{
		cairo_save( cr );
		cairo_rotate( cr, i * M_PI / 2 );
		cairo_move_to( cr, 0, -8 );
		cairo_line_to( cr, 0, -36 );
		cairo_stroke( cr );
		cairo_move_to( cr, -11, -27 );
		cairo_line_to( cr, 0, -38 );
		cairo_line_to( cr, 11, -27 );
		cairo_stroke( cr );
		cairo_restore( cr );
	}
	cairo_restore( cr );
}
