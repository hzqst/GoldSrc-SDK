#include "d_local.h"

float d_sdivzstepu;
float d_tdivzstepu;
float d_zistepu;
float d_sdivzstepv;
float d_tdivzstepv;
float d_zistepv;
float d_sdivzorigin;
float d_tdivzorigin;
float d_ziorigin;

fixed16_t sadjust;
fixed16_t tadjust;
fixed16_t bbextents;
fixed16_t bbextentt;

pixel_t* cacheblock;
int cachewidth;
pixel_t* d_viewbuffer;
zbuf_t* d_pzbuffer;
unsigned int d_zrowbytes;
unsigned int d_zwidth;
