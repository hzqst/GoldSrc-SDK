#include "d_local.h"
#include "r_local.h"

int d_vrectx;
int d_vrecty;
int d_vrectright_particle;
int d_vrectbottom_particle;
int d_vox_min;
int d_vox_max;
int d_vrectright_vox;
int d_vrectbottom_vox;
int d_y_aspect_shift;
int d_pix_min;
int d_pix_max;
int d_pix_shift;
int d_scantable[MAXHEIGHT];
zbuf_t* zspantable[MAXHEIGHT];

void D_Patch()
{
}

/*
================
D_ViewChanged
================
*/
void D_ViewChanged()
{
	int rowbytes = r_dowarp ? WARP_WIDTH : vid.rowbytes;
	scale_for_mip = xscale;
	if ( yscale > xscale )
		scale_for_mip = yscale;

	d_zwidth = vid.width;
	d_zrowbytes = vid.width * sizeof( *d_pzbuffer );
	d_pix_min = r_refdef.vrect.width / 320;
	if ( d_pix_min < 1 )
		d_pix_min = 1;
	d_vox_min = r_refdef.vrect.width / 200;
	d_pix_max = (int)( r_refdef.vrect.width / 80.0 + 0.5 );
	d_vox_max = (int)( r_refdef.vrect.width / 17.77777777777778 + 0.5 );
	d_pix_shift = 8 - (int)( r_refdef.vrect.width / 320.0 + 0.5 );
	if ( d_pix_max < 1 )
		d_pix_max = 1;

	d_y_aspect_shift = pixelAspect > 1.4;
	d_vrectx = r_refdef.vrect.x;
	d_vrecty = r_refdef.vrect.y;
	d_vrectright_particle = r_refdef.vrectright - d_pix_max;
	d_vrectright_vox = r_refdef.vrectright - d_vox_max;
	d_vrectbottom_particle = r_refdef.vrectbottom - ( d_pix_max << d_y_aspect_shift );
	d_vrectbottom_vox = r_refdef.vrectbottom - ( d_vox_max << d_y_aspect_shift );

	for ( int i = 0; i < (int)vid.height; i++ )
	{
		d_scantable[i] = i * rowbytes;
		zspantable[i] = d_pzbuffer + i * vid.width;
	}
}
