/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
****/


#ifdef WIN32
#include <windows.h>
#else
#include "../../engine/win32_types.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int ReadBmpFile( char* szFile, unsigned char** ppbPalette, unsigned char** ppbBits, int* pwidth, int* pheight )
{
	int rc = 0;
	FILE* pfile = NULL;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256] = { 0 };
	ULONG cbPalBytes;
	ULONG cbBmpBits;
	long position;
	BYTE* pbBmpBits = NULL;
	BYTE* palette = NULL;

	if ( !szFile || !ppbPalette || !ppbBits || !pwidth || !pheight )
		return -1000;

	pfile = fopen( szFile, "rb" );
	if ( !pfile )
		return -1;

	// Read file header
	if ( fread( &bmfh, sizeof( bmfh ), 1, pfile ) != 1 )
	{
		rc = -2;
		goto GetOut;
	}
	if ( bmfh.bfReserved1 || bmfh.bfReserved2 )
	{
		rc = -2000;
		goto GetOut;
	}
	// Read info header
	if ( fread( &bmih, sizeof( bmih ), 1, pfile ) != 1 )
	{
		rc = -3;
		goto GetOut;
	}
	if ( bmih.biSize != sizeof( bmih ) || bmih.biPlanes != 1 || bmih.biClrUsed > 256 )
	{
		rc = -3000;
		goto GetOut;
	}
	// Bogus bit depth?  Only 8-bit supported.
	if ( bmih.biBitCount != 8 )
	{
		rc = -4;
		goto GetOut;
	}
	// Bogus compression?  Only non-compressed supported.
	if ( bmih.biCompression != BI_RGB )
	{
		rc = -5;
		goto GetOut;
	}

	cbPalBytes = ( bmih.biClrUsed ? bmih.biClrUsed : 256 ) * sizeof( RGBQUAD );
	// Read palette (256 entries)
	if ( fread( rgrgbPalette, cbPalBytes, 1, pfile ) != 1 )
	{
		rc = -6;
		goto GetOut;
	}

	position = ftell( pfile );
	if ( position < 0 || bmfh.bfSize < (ULONG)position )
	{
		rc = -7;
		goto GetOut;
	}
	cbBmpBits = bmfh.bfSize - (ULONG)position;
	pbBmpBits = malloc( cbBmpBits );
	if ( !pbBmpBits )
	{
		rc = -8;
		goto GetOut;
	}
	if ( fread( pbBmpBits, cbBmpBits, 1, pfile ) != 1 )
	{
		rc = -7;
		goto GetOut;
	}

	palette = malloc( sizeof( rgrgbPalette ) );
	if ( !palette )
	{
		rc = -8;
		goto GetOut;
	}
	memcpy( palette, rgrgbPalette, sizeof( rgrgbPalette ) );
	*ppbPalette = palette;
	*ppbBits = pbBmpBits;
	*pwidth = bmih.biWidth;
	*pheight = bmih.biHeight;
	palette = NULL;
	pbBmpBits = NULL;
	printf( "w %ld h %ld s %lu\n", bmih.biWidth, bmih.biHeight, cbBmpBits );

GetOut:
	free( palette );
	free( pbBmpBits );
	fclose( pfile );
	return rc;
}
