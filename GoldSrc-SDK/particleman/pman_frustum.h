#pragma once

enum FrustumSide
{
	RIGHT	= 0,
	LEFT	= 1,
	BOTTOM	= 2,
	TOP		= 3,
	BACK	= 4,
	FRONT	= 5
};

enum PlaneData
{
	A	= 0,
	B	= 1,
	C	= 2,
	D	= 3
};

class CFrustum
{
	float g_flFrustum[6][4];

public:
	void NormalizeFrustumPlane( float frustum[6][4], int side );
	void CalculateFrustum();
	bool PointInsideFrustum( float x, float y, float z );
	bool SphereInsideFrustum( float x, float y, float z, float radius );
	bool PlaneInsideFrustum( float x, float y, float z, float size );
	bool BoxInsideFrustum( float minX, float minY, float minZ, float maxX, float maxY, float maxZ );
};

extern CFrustum g_cFrustum;
