#include "pman_frustum.h"

#include "wrect.h"
#include "cl_dll.h"
#include "triangleapi.h"

#include <math.h>

#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX 0x0BA6
#endif

#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif

CFrustum g_cFrustum;

void CFrustum::NormalizeFrustumPlane( float frustum[6][4], int side )
{
	float magnitude = (float)sqrt( (double)( frustum[side][A] * frustum[side][A] +
								   frustum[side][B] * frustum[side][B] +
								   frustum[side][C] * frustum[side][C] ) );

	frustum[side][A] /= magnitude;
	frustum[side][B] /= magnitude;
	frustum[side][C] /= magnitude;
	frustum[side][D] /= magnitude;
}

void CFrustum::CalculateFrustum()
{
	float proj[16];
	float modl[16];
	float clip[16];

	gEngfuncs.pTriAPI->GetMatrix( GL_PROJECTION_MATRIX, proj );
	gEngfuncs.pTriAPI->GetMatrix( GL_MODELVIEW_MATRIX, modl );

	clip[ 0] = modl[ 0] * proj[ 0] + modl[ 1] * proj[ 4] + modl[ 2] * proj[ 8] + modl[ 3] * proj[12];
	clip[ 1] = modl[ 0] * proj[ 1] + modl[ 1] * proj[ 5] + modl[ 2] * proj[ 9] + modl[ 3] * proj[13];
	clip[ 2] = modl[ 0] * proj[ 2] + modl[ 1] * proj[ 6] + modl[ 2] * proj[10] + modl[ 3] * proj[14];
	clip[ 3] = modl[ 0] * proj[ 3] + modl[ 1] * proj[ 7] + modl[ 2] * proj[11] + modl[ 3] * proj[15];

	clip[ 4] = modl[ 4] * proj[ 0] + modl[ 5] * proj[ 4] + modl[ 6] * proj[ 8] + modl[ 7] * proj[12];
	clip[ 5] = modl[ 4] * proj[ 1] + modl[ 5] * proj[ 5] + modl[ 6] * proj[ 9] + modl[ 7] * proj[13];
	clip[ 6] = modl[ 4] * proj[ 2] + modl[ 5] * proj[ 6] + modl[ 6] * proj[10] + modl[ 7] * proj[14];
	clip[ 7] = modl[ 4] * proj[ 3] + modl[ 5] * proj[ 7] + modl[ 6] * proj[11] + modl[ 7] * proj[15];

	clip[ 8] = modl[ 8] * proj[ 0] + modl[ 9] * proj[ 4] + modl[10] * proj[ 8] + modl[11] * proj[12];
	clip[ 9] = modl[ 8] * proj[ 1] + modl[ 9] * proj[ 5] + modl[10] * proj[ 9] + modl[11] * proj[13];
	clip[10] = modl[ 8] * proj[ 2] + modl[ 9] * proj[ 6] + modl[10] * proj[10] + modl[11] * proj[14];
	clip[11] = modl[ 8] * proj[ 3] + modl[ 9] * proj[ 7] + modl[10] * proj[11] + modl[11] * proj[15];

	clip[12] = modl[12] * proj[ 0] + modl[13] * proj[ 4] + modl[14] * proj[ 8] + modl[15] * proj[12];
	clip[13] = modl[12] * proj[ 1] + modl[13] * proj[ 5] + modl[14] * proj[ 9] + modl[15] * proj[13];
	clip[14] = modl[12] * proj[ 2] + modl[13] * proj[ 6] + modl[14] * proj[10] + modl[15] * proj[14];
	clip[15] = modl[12] * proj[ 3] + modl[13] * proj[ 7] + modl[14] * proj[11] + modl[15] * proj[15];

	g_flFrustum[RIGHT][A] = clip[ 3] - clip[ 0];
	g_flFrustum[RIGHT][B] = clip[ 7] - clip[ 4];
	g_flFrustum[RIGHT][C] = clip[11] - clip[ 8];
	g_flFrustum[RIGHT][D] = clip[15] - clip[12];
	NormalizeFrustumPlane( g_flFrustum, RIGHT );

	g_flFrustum[LEFT][A] = clip[ 3] + clip[ 0];
	g_flFrustum[LEFT][B] = clip[ 7] + clip[ 4];
	g_flFrustum[LEFT][C] = clip[11] + clip[ 8];
	g_flFrustum[LEFT][D] = clip[15] + clip[12];
	NormalizeFrustumPlane( g_flFrustum, LEFT );

	g_flFrustum[BOTTOM][A] = clip[ 3] + clip[ 1];
	g_flFrustum[BOTTOM][B] = clip[ 7] + clip[ 5];
	g_flFrustum[BOTTOM][C] = clip[11] + clip[ 9];
	g_flFrustum[BOTTOM][D] = clip[15] + clip[13];
	NormalizeFrustumPlane( g_flFrustum, BOTTOM );

	g_flFrustum[TOP][A] = clip[ 3] - clip[ 1];
	g_flFrustum[TOP][B] = clip[ 7] - clip[ 5];
	g_flFrustum[TOP][C] = clip[11] - clip[ 9];
	g_flFrustum[TOP][D] = clip[15] - clip[13];
	NormalizeFrustumPlane( g_flFrustum, TOP );

	g_flFrustum[BACK][A] = clip[ 3] - clip[ 2];
	g_flFrustum[BACK][B] = clip[ 7] - clip[ 6];
	g_flFrustum[BACK][C] = clip[11] - clip[10];
	g_flFrustum[BACK][D] = clip[15] - clip[14];
	NormalizeFrustumPlane( g_flFrustum, BACK );

	g_flFrustum[FRONT][A] = clip[ 3] + clip[ 2];
	g_flFrustum[FRONT][B] = clip[ 7] + clip[ 6];
	g_flFrustum[FRONT][C] = clip[11] + clip[10];
	g_flFrustum[FRONT][D] = clip[15] + clip[14];
	NormalizeFrustumPlane( g_flFrustum, FRONT );
}

bool CFrustum::PointInsideFrustum( float x, float y, float z )
{
	for ( int i = 0; i < 6; i++ )
	{
		if ( g_flFrustum[i][A] * x + g_flFrustum[i][B] * y + g_flFrustum[i][C] * z + g_flFrustum[i][D] <= 0 )
			return false;
	}

	return true;
}

bool CFrustum::SphereInsideFrustum( float x, float y, float z, float radius )
{
	for ( int i = 0; i < 6; i++ )
	{
		if ( g_flFrustum[i][A] * x + g_flFrustum[i][B] * y + g_flFrustum[i][C] * z + g_flFrustum[i][D] <= -radius )
			return false;
	}

	return true;
}

bool CFrustum::PlaneInsideFrustum( float x, float y, float z, float size )
{
	return BoxInsideFrustum( x - size, y - size, z - size, x + size, y + size, z + size );
}

bool CFrustum::BoxInsideFrustum( float minX, float minY, float minZ, float maxX, float maxY, float maxZ )
{
	for ( int i = 0; i < 6; i++ )
	{
		if ( g_flFrustum[i][A] * minX + g_flFrustum[i][B] * minY + g_flFrustum[i][C] * minZ + g_flFrustum[i][D] > 0 )
			continue;
		if ( g_flFrustum[i][A] * maxX + g_flFrustum[i][B] * minY + g_flFrustum[i][C] * minZ + g_flFrustum[i][D] > 0 )
			continue;
		if ( g_flFrustum[i][A] * minX + g_flFrustum[i][B] * maxY + g_flFrustum[i][C] * minZ + g_flFrustum[i][D] > 0 )
			continue;
		if ( g_flFrustum[i][A] * maxX + g_flFrustum[i][B] * maxY + g_flFrustum[i][C] * minZ + g_flFrustum[i][D] > 0 )
			continue;
		if ( g_flFrustum[i][A] * minX + g_flFrustum[i][B] * minY + g_flFrustum[i][C] * maxZ + g_flFrustum[i][D] > 0 )
			continue;
		if ( g_flFrustum[i][A] * maxX + g_flFrustum[i][B] * minY + g_flFrustum[i][C] * maxZ + g_flFrustum[i][D] > 0 )
			continue;
		if ( g_flFrustum[i][A] * minX + g_flFrustum[i][B] * maxY + g_flFrustum[i][C] * maxZ + g_flFrustum[i][D] > 0 )
			continue;
		if ( g_flFrustum[i][A] * maxX + g_flFrustum[i][B] * maxY + g_flFrustum[i][C] * maxZ + g_flFrustum[i][D] > 0 )
			continue;

		return false;
	}

	return true;
}
