#include <initializer_list>

#include "playspace.h"

Vec3 RotateY( Vec3 v, double a )
{
	const double c = std::cos( a ), s = std::sin( a );
	return { c * v.x + s * v.z, v.y, -s * v.x + c * v.z };
}

void Playspace::Rebase( const Mat34 &standing, const Mat34 &seated )
{
	m_standing = standing;
	m_seated = seated;
	m_hasBase = true;
	Reset();
}

void Playspace::Reset()
{
	m_yaw = 0;
	m_t = {};
}

void Playspace::Translate( Vec3 d )
{
	m_t = m_t + d;
}

void Playspace::RotateAbout( Vec3 pivot, double angle )
{
	// D'(p) = R(a) * (D(p) - c) + c
	m_t = RotateY( m_t - pivot, angle ) + pivot;
	m_yaw += angle;
}

Mat34 Playspace::Apply( const Mat34 &base ) const
{
	Mat34 out;
	for ( int col = 0; col < 3; col++ )
	{
		const Vec3 r = RotateY( { base.m[0][col], base.m[1][col], base.m[2][col] }, m_yaw );
		out.m[0][col] = r.x;
		out.m[1][col] = r.y;
		out.m[2][col] = r.z;
	}
	const Vec3 t = RotateY( base.translation(), m_yaw ) + m_t;
	out.m[0][3] = t.x;
	out.m[1][3] = t.y;
	out.m[2][3] = t.z;
	return out;
}

bool Playspace::InRange() const
{
	for ( const Mat34 &m : { Standing(), Seated() } )
	{
		const Vec3 t = m.translation();
		if ( !std::isfinite( t.x ) || !std::isfinite( t.y ) || !std::isfinite( t.z ) || std::fabs( t.x ) > kMaxOffset ||
			std::fabs( t.y ) > kMaxOffset || std::fabs( t.z ) > kMaxOffset )
			return false;
	}
	return true;
}

void DragGesture::Update( bool held, bool valid, Vec3 handRaw, double multiplier, bool lockHeight, Playspace &ps )
{
	if ( !held )
	{
		m_active = false;
		return;
	}
	if ( !valid )
	{
		m_active = false; // re-anchor when tracking returns, so there is no jump
		return;
	}
	if ( m_active )
	{
		Vec3 d = ( handRaw - m_last ) * multiplier;
		if ( lockHeight )
			d.y = 0;
		ps.Translate( d );
	}
	m_active = true;
	m_last = handRaw;
}

namespace
{
constexpr double kMinRadius = 0.08;    // closer than this to the head the heading is too noisy to use
constexpr double kMaxStep = M_PI / 2;  // a larger heading jump in one sample is a tracking glitch

double Wrap( double a )
{
	while ( a > M_PI )
		a -= 2 * M_PI;
	while ( a <= -M_PI )
		a += 2 * M_PI;
	return a;
}
} // namespace

void TurnGesture::Update( bool held, bool valid, Vec3 handRaw, Vec3 hmdRaw, Playspace &ps )
{
	const Vec3 rel = handRaw - hmdRaw;
	if ( !held || !valid || std::hypot( rel.x, rel.z ) < kMinRadius )
	{
		m_active = false;
		return;
	}
	const double angle = std::atan2( rel.x, rel.z );
	if ( m_active )
	{
		const double step = Wrap( angle - m_lastAngle );
		if ( std::fabs( step ) <= kMaxStep )
			ps.RotateAbout( hmdRaw, step );
	}
	m_active = true;
	m_lastAngle = angle;
}

bool DoublePressGate::Update( bool held, double now )
{
	if ( held && !m_prevHeld ) // pressed
	{
		m_pressedAt = now;
		m_active = m_lastWasTap && now - m_releasedAt <= kWindow;
	}
	else if ( !held && m_prevHeld ) // released
	{
		m_releasedAt = now;
		m_lastWasTap = !m_active && now - m_pressedAt <= kWindow; // a short press arms the next one; the activating press does not
		m_active = false;
	}
	m_prevHeld = held;
	return held && m_active;
}

Mat34 ZeroPoseFromPoses( const Mat34 &raw, const Mat34 &uni )
{
	Mat34 z;
	for ( int r = 0; r < 3; r++ ) // R_z = R_raw * R_uni^T
		for ( int c = 0; c < 3; c++ )
			z.m[r][c] = raw.m[r][0] * uni.m[c][0] + raw.m[r][1] * uni.m[c][1] + raw.m[r][2] * uni.m[c][2];
	for ( int r = 0; r < 3; r++ ) // t_z = t_raw - R_z * t_uni
		z.m[r][3] = raw.m[r][3] - ( z.m[r][0] * uni.m[0][3] + z.m[r][1] * uni.m[1][3] + z.m[r][2] * uni.m[2][3] );
	return z;
}

bool NearPose( const Mat34 &a, const Mat34 &b, double posTol, double rotTolRad )
{
	const Vec3 d = a.translation() - b.translation();
	if ( std::sqrt( d.x * d.x + d.y * d.y + d.z * d.z ) > posTol )
		return false;
	double trace = 0; // trace(R_a * R_b^T) = 1 + 2 cos(angle)
	for ( int r = 0; r < 3; r++ )
		for ( int c = 0; c < 3; c++ )
			trace += a.m[r][c] * b.m[r][c];
	const double cosA = std::fmax( -1.0, std::fmin( 1.0, ( trace - 1.0 ) / 2.0 ) );
	return std::acos( cosA ) <= rotTolRad;
}
