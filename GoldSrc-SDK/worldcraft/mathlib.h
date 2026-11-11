//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef HAMMER_MATHLIB_H
#define HAMMER_MATHLIB_H
#pragma once

#include <assert.h>
#include <float.h>
#include "vector.h"
#include "../utils/common/mathlib.h"

#ifndef Assert
#define Assert assert
#endif
#ifndef M_PI
#define M_PI Q_PI
#endif
#define DEG2RAD( angle ) ( ( angle ) * ( Q_PI / 180.0 ) )
#define RAD2DEG( angle ) ( ( angle ) * ( 180.0 / Q_PI ) )

#define PITCH 0
#define YAW 1
#define ROLL 2

inline float VectorNormalize( Vector& v )
{
	double flLength = sqrt( ( (double)v.x * v.x + (double)v.y * v.y ) + (double)v.z * v.z );
	if ( flLength != 0 )
	{
		double flScale = 1.0 / flLength;
		v.x = (float)( flScale * v.x );
		v.y = (float)( flScale * v.y );
		v.z = (float)( flScale * v.z );
	}
	return (float)flLength;
}
inline float VectorLength( const Vector& v )
{
	return (float)sqrt( ( (double)v.x * v.x + (double)v.y * v.y ) + (double)v.z * v.z );
}
inline void VectorMA( const Vector& start, float scale, const Vector& direction, Vector& out )
{
	out.x = (float)( (double)scale * direction.x + start.x );
	out.y = (float)( (double)scale * direction.y + start.y );
	out.z = (float)( (double)scale * direction.z + start.z );
}
inline void CrossProduct( const Vector& first, const Vector& second, Vector& out )
{
	out.x = (float)( (double)second.z * first.y - (double)first.z * second.y );
	out.y = (float)( (double)first.z * second.x - (double)first.x * second.z );
	out.z = (float)( (double)first.x * second.y - (double)second.x * first.y );
}
inline void VectorClear( Vector& v )
{
	v.Init();
}

inline void VectorNegate( Vector& vector )
{
	vector.Negate();
}

inline void VectorNegate( const Vector& in, Vector& out )
{
	out = -in;
}

inline void AngleVectors( const Vector& angles, Vector* pForward, Vector* pRight = NULL, Vector* pUp = NULL )
{
	matrix3x4_t matrix;
	AngleMatrix( angles.Base(), matrix );
	if ( pForward )
		pForward->Init( matrix[0][0], matrix[1][0], matrix[2][0] );
	if ( pRight )
		pRight->Init( -matrix[0][1], -matrix[1][1], -matrix[2][1] );
	if ( pUp )
		pUp->Init( matrix[0][2], matrix[1][2], matrix[2][2] );
}

inline void VectorAngles( const Vector& forward, Vector& angles )
{
	if ( forward.x == 0 && forward.y == 0 )
		angles.Init( forward.z > 0 ? 90.0f : 270.0f, 0, 0 );
	else
	{
		angles.Init( (float)RAD2DEG( atan2( forward.z, forward.Length2D() ) ),
					 (float)RAD2DEG( atan2( forward.y, forward.x ) ), 0 );
		if ( angles.x < 0 )
			angles.x += 360;
		if ( angles.y < 0 )
			angles.y += 360;
	}
}

inline void ConcatTransforms( const matrix3x4_t& first, const matrix3x4_t& second, matrix3x4_t& out )
{
	R_ConcatTransforms( first, second, out );
}

inline void MatrixAngles( const matrix3x4_t& matrix, QAngle& angles )
{
	float horizontal = sqrtf( matrix[0][0] * matrix[0][0] + matrix[1][0] * matrix[1][0] );
	angles.x = (float)RAD2DEG( atan2( -matrix[2][0], horizontal ) );
	if ( horizontal > 0.001f )
	{
		angles.y = (float)RAD2DEG( atan2( matrix[1][0], matrix[0][0] ) );
		angles.z = (float)RAD2DEG( atan2( matrix[2][1], matrix[2][2] ) );
	}
	else
	{
		angles.y = (float)RAD2DEG( atan2( -matrix[0][1], matrix[1][1] ) );
		angles.z = 0;
	}
}

#endif
