// mathlib.c -- math primitives
#include "mathlib.h"
#include "engine_apiproxy.h"
#include "gl_model.h"
#include "sys.h"
#include "../common/q_ftol.h"

#include <cmath>
#include <utility>

vec3_t vec3_origin;
int nanmask = 255 << 23;

/*
====================
AngleMatrix

====================
*/
void AngleMatrix( const float* angles, float matrix[3][4] )
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = (float)( angles[YAW] * ( M_PI * 2 / 360 ) );
	sy = (float)sin( (double)angle );
	cy = (float)cos( (double)angle );
	angle = (float)( angles[PITCH] * ( M_PI * 2 / 360 ) );
	sp = (float)sin( (double)angle );
	cp = (float)cos( (double)angle );
	angle = (float)( angles[ROLL] * ( M_PI * 2 / 360 ) );
	sr = (float)sin( (double)angle );
	cr = (float)cos( (double)angle );

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp * cy;
	matrix[1][0] = cp * sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr * sp * cy - cr * sy;
	matrix[1][1] = sr * sp * sy + cr * cy;
	matrix[2][1] = sr * cp;
	matrix[0][2] = cr * sp * cy - -sy * sr;
	matrix[1][2] = cr * sp * sy - sr * cy;
	matrix[2][2] = cr * cp;
	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}

float anglemod( float a )
{
	double steps;

	steps = a * ( 65536 / 360.0 );

	return (float)( ( 360.0 / 65536 ) * ( Q_ftol( steps ) & 65535 ) );
}

qboolean VectorCompare( const vec_t* v1, const vec_t* v2 )
{
	int i;

	for ( i = 0; i < 3; i++ )
	{
		if ( v1[i] != v2[i] )
			return 0;
	}

	return 1;
}

void VectorMA( const vec_t* veca, float scale, const vec_t* vecb, vec_t* vecc )
{
	vecc[0] = veca[0] + scale * vecb[0];
	vecc[1] = veca[1] + scale * vecb[1];
	vecc[2] = veca[2] + scale * vecb[2];
}

void VectorScale( const vec_t* in, float scale, vec_t* out )
{
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

float Length( const vec_t* v )
{
	int i;
	float length;

	length = 0;
	for ( i = 0; i < 3; i++ )
		length += v[i] * v[i];
	length = (float)sqrt( (double)length );

	return length;
}

float VectorNormalize( vec_t* v )
{
	float length, ilength;

	length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	length = (float)sqrt( (double)length );

	if ( length )
	{
		ilength = 1 / length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	return length;
}

void VectorAngles( const vec_t* forward, vec_t* angles )
{
	float tmp, yaw, pitch;

	if ( forward[1] == 0 && forward[0] == 0 )
	{
		yaw = 0;
		if ( forward[2] > 0 )
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (float)( atan2( (double)forward[1], (double)forward[0] ) * 180.0 / M_PI );
		if ( yaw < 0 )
			yaw += 360;

		tmp = (float)sqrt( (double)( forward[0] * forward[0] + forward[1] * forward[1] ) );
		pitch = (float)( atan2( (double)forward[2], (double)tmp ) * 180.0 / M_PI );
		if ( pitch < 0 )
			pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

void AngleVectors( const vec_t* angles, vec_t* forward, vec_t* right, vec_t* up )
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	g_engdstAddrs.pfnAngleVectors( &angles, &forward, &right, &up );

	angle = (float)( angles[YAW] * ( M_PI * 2 / 360 ) );
	sy = (float)sin( (double)angle );
	cy = (float)cos( (double)angle );
	angle = (float)( angles[PITCH] * ( M_PI * 2 / 360 ) );
	sp = (float)sin( (double)angle );
	cp = (float)cos( (double)angle );
	angle = (float)( angles[ROLL] * ( M_PI * 2 / 360 ) );
	sr = (float)sin( (double)angle );
	cr = (float)cos( (double)angle );

	if ( forward )
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if ( right )
	{
		right[0] = -1 * cr * -sy - sr * sp * cy;
		right[1] = -1 * cr * cy - sr * sp * sy;
		right[2] = -1 * sr * cp;
	}
	if ( up )
	{
		up[0] = cr * sp * cy - -sy * sr;
		up[1] = cr * sp * sy - sr * cy;
		up[2] = cr * cp;
	}
}

void AngleVectorsTranspose( const vec_t* angles, vec_t* forward, vec_t* right, vec_t* up )
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = (float)( angles[YAW] * ( M_PI * 2 / 360 ) );
	sy = (float)sin( (double)angle );
	cy = (float)cos( (double)angle );
	angle = (float)( angles[PITCH] * ( M_PI * 2 / 360 ) );
	sp = (float)sin( (double)angle );
	cp = (float)cos( (double)angle );
	angle = (float)( angles[ROLL] * ( M_PI * 2 / 360 ) );
	sr = (float)sin( (double)angle );
	cr = (float)cos( (double)angle );

	if ( forward )
	{
		forward[0] = cp * cy;
		forward[1] = sr * sp * cy - cr * sy;
		forward[2] = cr * sp * cy - -sy * sr;
	}
	if ( right )
	{
		right[0] = cp * sy;
		right[1] = sr * sp * sy + cr * cy;
		right[2] = cr * sp * sy - sr * cy;
	}
	if ( up )
	{
		up[0] = -sp;
		up[1] = sr * cp;
		up[2] = cr * cp;
	}
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide( vec_t* emins, vec_t* emaxs, mplane_t* p )
{
	int i;
	int sides;
	float dist1, dist2;
	vec3_t corners[2];

	if ( p->signbits > 7 )
		BOPS_Error();

	for ( i = 0; i < 3; i++ )
	{
		if ( p->signbits & ( 1 << i ) )
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[0][i] = emaxs[i];
			corners[1][i] = emins[i];
		}
	}

	dist1 = DotProduct( p->normal, corners[0] );
	dist2 = DotProduct( p->normal, corners[1] );

	sides = 0;
	if ( dist1 >= p->dist )
		sides = 1;
	if ( dist2 < p->dist )
		sides |= 2;

	return sides;
}

void CrossProduct( const vec_t* v1, const vec_t* v2, vec_t* cross )
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

void NormalizeAngles( float* angles )
{
	int i;

	for ( i = 0; i < 3; i++ )
	{
		if ( angles[i] > 180 )
			angles[i] = (float)( angles[i] - 360.0 );
		else if ( angles[i] < -180 )
			angles[i] = (float)( angles[i] + 360.0 );
	}
}

void InterpolateAngles( float* start, float* end, float* output, float frac )
{
	int i;
	float ang1, d;

	NormalizeAngles( start );
	NormalizeAngles( end );

	for ( i = 0; i < 3; i++ )
	{
		ang1 = start[i];
		d = end[i] - ang1;
		if ( d > 180 )
			d -= 360;
		else if ( d < -180 )
			d += 360;

		output[i] = ang1 + d * frac;
	}

	NormalizeAngles( output );
}

/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations( float in1[3][3], float in2[3][3], float out[3][3] )
{
	int i, j;

	for ( i = 0; i < 3; i++ )
	{
		for ( j = 0; j < 3; j++ )
			out[i][j] = in1[i][0] * in2[0][j] + in1[i][1] * in2[1][j] + in1[i][2] * in2[2][j];
	}
}

/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms( float in1[3][4], float in2[3][4], float out[3][4] )
{
	int i, j;

	for ( i = 0; i < 3; i++ )
	{
		for ( j = 0; j < 3; j++ )
			out[i][j] = in1[i][0] * in2[0][j] + in1[i][1] * in2[1][j] + in1[i][2] * in2[2][j];
		out[i][3] = in1[i][0] * in2[0][3] + in1[i][1] * in2[1][3] + in1[i][2] * in2[2][3] + in1[i][3];
	}
}

void VectorTransform( const vec_t* in1, float in2[3][4], vec_t* out )
{
	out[0] = DotProduct( in1, in2[0] ) + in2[0][3];
	out[1] = DotProduct( in1, in2[1] ) + in2[1][3];
	out[2] = DotProduct( in1, in2[2] ) + in2[2][3];
}

void VectorMatrix( vec_t* forward, vec_t* right, vec_t* up )
{
	vec3_t tmp;

	if ( forward[0] == 0 && forward[1] == 0 )
	{
		right[0] = 1;
		right[1] = 0;
		right[2] = 0;
		up[0] = -forward[2];
		up[1] = 0;
		up[2] = 0;
		return;
	}

	tmp[0] = 0;
	tmp[1] = 0;
	tmp[2] = 1;
	CrossProduct( forward, tmp, right );
	VectorNormalize( right );
	CrossProduct( right, forward, up );
	VectorNormalize( up );
}

int InvertMatrix( const float* m, float* out )
{
	float wtmp[4][8];
	float m0, m1, m2, m3, s;
	float *r0, *r1, *r2, *r3;
	int i, j;

	for ( i = 0; i < 4; i++ )
	{
		for ( j = 0; j < 4; j++ )
		{
			wtmp[i][j] = m[j * 4 + i];
			wtmp[i][4 + j] = i == j ? 1.0f : 0.0f;
		}
	}

	r0 = wtmp[0];
	r1 = wtmp[1];
	r2 = wtmp[2];
	r3 = wtmp[3];

	if ( fabs( (double)r3[0] ) > fabs( (double)r2[0] ) )
		std::swap( r3, r2 );
	if ( fabs( (double)r2[0] ) > fabs( (double)r1[0] ) )
		std::swap( r2, r1 );
	if ( fabs( (double)r1[0] ) > fabs( (double)r0[0] ) )
		std::swap( r1, r0 );
	if ( r0[0] == 0 )
		return 0;

	m1 = r1[0] / r0[0];
	m2 = r2[0] / r0[0];
	m3 = r3[0] / r0[0];
	for ( j = 1; j < 4; j++ )
	{
		r1[j] -= r0[j] * m1;
		r2[j] -= r0[j] * m2;
		r3[j] -= r0[j] * m3;
	}
	for ( j = 4; j < 8; j++ )
	{
		s = r0[j];
		if ( s != 0 )
		{
			r1[j] -= s * m1;
			r2[j] -= s * m2;
			r3[j] -= s * m3;
		}
	}

	if ( fabs( (double)r3[1] ) > fabs( (double)r2[1] ) )
		std::swap( r3, r2 );
	if ( fabs( (double)r2[1] ) > fabs( (double)r1[1] ) )
		std::swap( r2, r1 );
	if ( r1[1] == 0 )
		return 0;

	m2 = r2[1] / r1[1];
	m3 = r3[1] / r1[1];
	for ( j = 2; j < 4; j++ )
	{
		r2[j] -= m2 * r1[j];
		r3[j] -= m3 * r1[j];
	}
	for ( j = 4; j < 8; j++ )
	{
		s = r1[j];
		if ( s != 0 )
		{
			r2[j] -= s * m2;
			r3[j] -= s * m3;
		}
	}

	if ( fabs( (double)r3[2] ) > fabs( (double)r2[2] ) )
		std::swap( r3, r2 );
	if ( r2[2] == 0 )
		return 0;

	m3 = r3[2] / r2[2];
	for ( j = 3; j < 8; j++ )
		r3[j] -= m3 * r2[j];

	if ( r3[3] == 0 )
		return 0;

	s = (float)( 1.0 / (double)r3[3] );
	for ( j = 4; j < 8; j++ )
		r3[j] = s * r3[j];

	m2 = r2[3];
	s = (float)( 1.0 / (double)r2[2] );
	for ( j = 4; j < 8; j++ )
		r2[j] = ( r2[j] - m2 * r3[j] ) * s;

	m1 = r1[3];
	for ( j = 4; j < 8; j++ )
		r1[j] -= m1 * r3[j];

	m0 = r0[3];
	for ( j = 4; j < 8; j++ )
		r0[j] -= m0 * r3[j];

	m1 = r1[2];
	s = (float)( 1.0 / (double)r1[1] );
	for ( j = 4; j < 8; j++ )
		r1[j] = ( r1[j] - m1 * r2[j] ) * s;

	m0 = r0[2];
	for ( j = 4; j < 8; j++ )
		r0[j] -= m0 * r2[j];

	m0 = r0[1];
	s = (float)( 1.0 / (double)r0[0] );
	for ( j = 4; j < 8; j++ )
		r0[j] = ( r0[j] - m0 * r1[j] ) * s;

	for ( j = 0; j < 4; j++ )
	{
		out[j * 4 + 0] = r0[4 + j];
		out[j * 4 + 1] = r1[4 + j];
		out[j * 4 + 2] = r2[4 + j];
		out[j * 4 + 3] = r3[4 + j];
	}

	return 1;
}

/*
==================
BOPS_Error

Split out like this for ASM to call.
==================
*/
[[noreturn]] void BOPS_Error()
{
	Sys_Error( "BoxOnPlaneSide:  Bad signbits" );
}

void AngleIMatrix( const float* angles, float matrix[3][4] )
{
	float angle;
	float sr, sp, sy, cr, cp, cy;

	angle = (float)( angles[YAW] * ( M_PI * 2 / 360 ) );
	sy = (float)sin( (double)angle );
	cy = (float)cos( (double)angle );
	angle = (float)( angles[PITCH] * ( M_PI * 2 / 360 ) );
	sp = (float)sin( (double)angle );
	cp = (float)cos( (double)angle );
	angle = (float)( angles[ROLL] * ( M_PI * 2 / 360 ) );
	sr = (float)sin( (double)angle );
	cr = (float)cos( (double)angle );

	// matrix = (Z * Y) * X
	matrix[0][0] = cp * cy;
	matrix[0][1] = cp * sy;
	matrix[0][2] = -sp;
	matrix[1][0] = sr * sp * cy - cr * sy;
	matrix[1][1] = sr * sp * sy + cr * cy;
	matrix[1][2] = sr * cp;
	matrix[2][0] = cr * sp * cy + sr * sy;
	matrix[2][1] = cr * sp * sy - sr * cy;
	matrix[2][2] = cr * cp;
	matrix[0][3] = 0.0f;
	matrix[1][3] = 0.0f;
	matrix[2][3] = 0.0f;
}

vec_t _DotProduct( vec_t* v1, vec_t* v2 )
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void _VectorSubtract( vec_t* veca, vec_t* vecb, vec_t* out )
{
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

void _VectorAdd( vec_t* veca, vec_t* vecb, vec_t* out )
{
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

void _VectorCopy( vec_t* in, vec_t* out )
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void VectorInverse( vec_t* v )
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

int Q_log2( int val )
{
	int answer = 0;

	while ( val >>= 1 )
		answer++;

	return answer;
}

/*
===================
FloorDivMod

Returns mathematically correct (floor-based) quotient and remainder for
numer and denom, both of which should contain no fractional part. The
quotient must fit in 32 bits.
====================
*/
void FloorDivMod( double numer, double denom, int* quotient, int* rem )
{
	int q, r;
	double x;

	if ( denom <= 0.0 )
		Sys_Error( "FloorDivMod: bad denominator %d\n", denom );

	if ( numer >= 0.0 )
	{
		x = floor( numer / denom );
		q = (int)x;
		r = (int)floor( numer - x * denom );
	}
	else
	{
		//
		// perform operations with positive values, and fix mod to make floor-based
		//
		x = floor( -numer / denom );
		q = -(int)x;
		r = (int)floor( -numer - x * denom );
		if ( r )
		{
			q--;
			r = (int)denom - r;
		}
	}

	*quotient = q;
	*rem = r;
}

/*
===================
GreatestCommonDivisor
====================
*/
int GreatestCommonDivisor( int i1, int i2 )
{
	int r;

	while ( i1 && i2 )
	{
		if ( i1 > i2 )
		{
			r = i1 % i2;
			i1 = i2;
			i2 = r;
		}
		else
		{
			i2 %= i1;
		}
	}

	if ( !i1 )
		return i2;
	return i1;
}

/*
===================
Invert24To16

Inverts an 8.24 value to a 16.16 value
====================
*/
// TODO: move to nonintel.c
fixed16_t Invert24To16( fixed16_t val )
{
	if ( val < 256 )
		return -1;

	return (fixed16_t)( (double)0x10000 * (double)0x1000000 / (double)val + 0.5 );
}
