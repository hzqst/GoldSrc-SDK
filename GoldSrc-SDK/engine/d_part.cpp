#include "d_local.h"
#include "r_local.h"
#include "../common/particledef.h"

/*
==============
D_EndParticles
==============
*/
void D_EndParticles()
{
}

void D_StartParticles()
{
}

/*
==============
D_DrawParticle
==============
*/
void D_DrawParticle( particle_t* particle )
{
	vec3_t local;
// transform point
	VectorSubtract( particle->org, r_origin, local );
	float depth = DotProduct( local, r_ppn );
	if ( depth < 8.0f )
		return;

	float zi = 1.0f / depth;
	int u = (int)( DotProduct( local, r_pright ) * zi + xcenter + 0.5 );
	int v = (int)( ycenter - DotProduct( local, r_pup ) * zi + 0.5 );
	qboolean voxel = particle->type == pt_vox_slowgrav || particle->type == pt_vox_grav;
	if ( voxel )
	{
		if ( v > d_vrectbottom_vox || u > d_vrectright_vox )
			return;
	}
	else if ( v > d_vrectbottom_particle || u > d_vrectright_particle )
	{
		return;
	}
	if ( v < d_vrecty || u < d_vrectx )
		return;

	float z = zi * 32768.0f;
	int izi = (int)z;
	int pix = izi >> d_pix_shift;
	const zbuf_t pixelDepth = D_DepthFromZi( zi );
	if ( voxel )
	{
		if ( pix < d_vox_min )
			pix = d_vox_min;
		else if ( pix > d_vox_max )
			pix = d_vox_max;
	}
	else
	{
		if ( pix < d_pix_min )
			pix = d_pix_min;
		else if ( pix > d_pix_max )
			pix = d_pix_max;
	}

	byte* dest = d_viewbuffer + d_scantable[v] + u * sizeof( particle->packedColor );
	zbuf_t* pz = zspantable[v] + u;
	for ( int count = pix << d_y_aspect_shift; count; count-- )
	{
		short* color = (short*)dest;
		for ( int i = 0; i < pix; i++ )
		{
			if ( pixelDepth >= pz[i] )
			{
				pz[i] = pixelDepth;
				color[i] = particle->packedColor;
			}
		}
		dest += screenwidth;
		pz += d_zwidth;
	}
}
