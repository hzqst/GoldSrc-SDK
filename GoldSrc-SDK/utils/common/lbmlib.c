/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
****/

// lbmlib.c

#ifdef WIN32
#include <windows.h>
#else
#include "../../engine/win32_types.h"
#endif
#include <stdio.h>
#include <limits.h>

#include "cmdlib.h"
#include "lbmlib.h"



/*
============================================================================

						LBM STUFF

============================================================================
*/


#define FORMID ('F'+('O'<<8)+((int)'R'<<16)+((int)'M'<<24))
#define ILBMID ('I'+('L'<<8)+((int)'B'<<16)+((int)'M'<<24))
#define PBMID  ('P'+('B'<<8)+((int)'M'<<16)+((int)' '<<24))
#define BMHDID ('B'+('M'<<8)+((int)'H'<<16)+((int)'D'<<24))
#define BODYID ('B'+('O'<<8)+((int)'D'<<16)+((int)'Y'<<24))
#define CMAPID ('C'+('M'<<8)+((int)'A'<<16)+((int)'P'<<24))


bmhd_t  bmhd;

int    Align (int l)
{
	if (l&1)
		return l+1;
	return l;
}



/*
================
=
= LBMRLEdecompress
=
= Source must be evenly aligned!
=
================
*/

byte  *LBMRLEDecompress (byte *source,byte *unpacked, int bpwidth)
{
	int     count;
	byte    b,rept;

	count = 0;

	do
	{
		rept = *source++;

		if (rept > 0x80)
		{
			rept = (rept^0xff)+2;
			if ( rept > bpwidth - count )
				Error( "Decompression exceeded width!\n" );
			b = *source++;
			memset(unpacked,b,rept);
			unpacked += rept;
		}
		else if (rept < 0x80)
		{
			rept++;
			if ( rept > bpwidth - count )
				Error( "Decompression exceeded width!\n" );
			memcpy(unpacked,source,rept);
			unpacked += rept;
			source += rept;
		}
		else
			rept = 0;               // rept of 0x80 is NOP

		count += rept;

	} while (count<bpwidth);

	if (count>bpwidth)
		Error ("Decompression exceeded width!\n");


	return source;
}


#define BPLANESIZE      128
byte    bitplanes[9][BPLANESIZE];       // max size 1024 by 9 bit planes


/*
=================
=
= MungeBitPlanes8
=
= This destroys the bit plane data!
=
=================
*/

void MungeBitPlanes8 (int width, byte *dest)
{
	*dest=width;	// shut up the compiler warning
	Error ("MungeBitPlanes8 not rewritten!");
#if 0
asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*7 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*6 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*5 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*4 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*3 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*2 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*1 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}


void MungeBitPlanes4 (int width, byte *dest)
{
	*dest=width;	// shut up the compiler warning
	Error ("MungeBitPlanes4 not rewritten!");
#if 0

asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     xor     al,al
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*3 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*2 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*1 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}


void MungeBitPlanes2 (int width, byte *dest)
{
	*dest=width;	// shut up the compiler warning
	Error ("MungeBitPlanes2 not rewritten!");
#if 0
asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     xor     al,al
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*1 +si],1
asm     rcl     al,1
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}


void MungeBitPlanes1 (int width, byte *dest)
{
	*dest=width;	// shut up the compiler warning
	Error ("MungeBitPlanes1 not rewritten!");
#if 0
asm     les     di,[dest]
asm     mov     si,-1
asm     mov     cx,[width]
mungebyte:
asm     inc     si
asm     mov     dx,8
mungebit:
asm     xor     al,al
asm     shl     [BYTE PTR bitplanes + BPLANESIZE*0 +si],1
asm     rcl     al,1
asm     stosb
asm     dec     cx
asm     jz      done
asm     dec     dx
asm     jnz     mungebit
asm     jmp     mungebyte

done:
#endif
}

int LoadBMP (const char* szFile, BYTE** ppbBits, BYTE** ppbPalette)
{
	int i, rc = 0;
	FILE* pfile = NULL;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256];
	ULONG cbBmpBits;
	BYTE* pbBmpBits = NULL;
	byte* pbPal = NULL;
	ULONG cbPalBytes;
	ULONG biTrueWidth;
	long paletteEnd;

	if ( !szFile || !ppbPalette || !ppbBits || ppbBits == ppbPalette )
		return -1000;

	*ppbBits = NULL;
	*ppbPalette = NULL;
	pfile = fopen( szFile, "rb" );
	if ( !pfile )
		return -1;

	// Read file header
	if ( fread( &bmfh, sizeof( bmfh ), 1, pfile ) != 1 )
	{
		rc = -2;
		goto GetOut;
	}
	if ( bmfh.bfType != MAKEWORD( 'B', 'M' ) || bmfh.bfReserved1 || bmfh.bfReserved2 )
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
	if ( bmih.biSize != sizeof( bmih ) || bmih.biPlanes != 1 || bmih.biWidth <= 0
		|| bmih.biHeight <= 0 || bmih.biWidth > USHRT_MAX || bmih.biHeight > USHRT_MAX )
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

	if ( !bmih.biClrUsed )
		bmih.biClrUsed = 256;
	if ( bmih.biClrUsed > 256 )
	{
		rc = -6;
		goto GetOut;
	}
	cbPalBytes = bmih.biClrUsed * sizeof( RGBQUAD );
	// Read palette (bmih.biClrUsed entries)
	if ( fread( rgrgbPalette, cbPalBytes, 1, pfile ) != 1 )
	{
		rc = -6;
		goto GetOut;
	}

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = ( bmih.biWidth + 3 ) & ~3;
	cbBmpBits = biTrueWidth * (ULONG)bmih.biHeight;
	paletteEnd = ftell( pfile );
	if ( paletteEnd < 0 || bmfh.bfOffBits < (ULONG)paletteEnd || bmfh.bfOffBits > LONG_MAX
		|| bmfh.bfSize < bmfh.bfOffBits || cbBmpBits > bmfh.bfSize - bmfh.bfOffBits
		|| fseek( pfile, (long)bmfh.bfOffBits, SEEK_SET ) )
	{
		rc = -7;
		goto GetOut;
	}

	pbPal = calloc( 256, 3 );
	pbBmpBits = malloc( cbBmpBits );
	if ( !pbPal || !pbBmpBits )
	{
		rc = -7;
		goto GetOut;
	}
	// Copy over used entries
	for ( i = 0; i < (int)bmih.biClrUsed; ++i )
	{
		pbPal[i * 3] = rgrgbPalette[i].rgbRed;
		pbPal[i * 3 + 1] = rgrgbPalette[i].rgbGreen;
		pbPal[i * 3 + 2] = rgrgbPalette[i].rgbBlue;
	}
	for ( i = 0; i < bmih.biHeight; ++i )
	{
		byte* row = pbBmpBits + (size_t)( bmih.biHeight - i - 1 ) * biTrueWidth;
		if ( fread( row, 1, biTrueWidth, pfile ) != biTrueWidth )
		{
			rc = -7;
			goto GetOut;
		}
	}

	bmhd.w = (WORD)bmih.biWidth;
	bmhd.h = (WORD)bmih.biHeight;
	// Set output parameters
	*ppbPalette = pbPal;
	*ppbBits = pbBmpBits;
	pbPal = NULL;
	pbBmpBits = NULL;

GetOut:
	free( pbPal );
	free( pbBmpBits );
	fclose( pfile );
	return rc;
}


int WriteBMPfile (char *szFile, byte *pbBits, int width, int height, byte *pbPalette)
{
	int i, rc = 0;
	FILE* pfile = NULL;
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256];
	ULONG cbBmpBits;
	BYTE* pbBmpBits = NULL;
	byte* pb;
	ULONG cbPalBytes;
	ULONG biTrueWidth;

	if ( !szFile || !pbPalette || !pbBits || width <= 0 || height <= 0 || width > INT_MAX - 3 )
		return -1000;

	biTrueWidth = ( width + 3 ) & ~3;
	cbPalBytes = sizeof( rgrgbPalette );
	if ( (ULONG)height > ( ULONG_MAX - sizeof( bmfh ) - sizeof( bmih ) - cbPalBytes ) / biTrueWidth )
		return -1000;
	cbBmpBits = biTrueWidth * (ULONG)height;
	pbBmpBits = calloc( cbBmpBits, 1 );
	if ( !pbBmpBits )
		return -7;

	// File exists?
	if ((pfile = fopen(szFile, "wb")) == NULL)
		{ rc = -1; goto GetOut; }

	// Bogus file header check
	bmfh.bfType = MAKEWORD( 'B', 'M' );
	bmfh.bfSize = sizeof bmfh + sizeof bmih + cbBmpBits + cbPalBytes;
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfOffBits = sizeof bmfh + sizeof bmih + cbPalBytes;

	// Write file header
	if (fwrite(&bmfh, sizeof bmfh, 1/*count*/, pfile) != 1)
		{ rc = -2; goto GetOut; }

	// Size of structure
	bmih.biSize = sizeof bmih;
	// Width
	bmih.biWidth = biTrueWidth;
	// Height
	bmih.biHeight = height;
	// Only 1 plane 
	bmih.biPlanes = 1;
	// Only 8-bit supported.
	bmih.biBitCount = 8;
	// Only non-compressed supported.
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;

	// huh?
	bmih.biXPelsPerMeter = 0;
	bmih.biYPelsPerMeter = 0;

	// Always full palette
	bmih.biClrUsed = 256;
	bmih.biClrImportant = 0;
	
	// Write info header
	if (fwrite(&bmih, sizeof bmih, 1/*count*/, pfile) != 1)
		{ rc = -3; goto GetOut; }
	

	// convert to expanded palette
	pb = pbPalette;

	// Copy over used entries
	for (i = 0; i < (int)bmih.biClrUsed; i++)
	{
		rgrgbPalette[i].rgbRed   = *pb++;
		rgrgbPalette[i].rgbGreen = *pb++;
		rgrgbPalette[i].rgbBlue  = *pb++;
        rgrgbPalette[i].rgbReserved = 0;
	}

	// Write palette (bmih.biClrUsed entries)
	cbPalBytes = bmih.biClrUsed * sizeof( RGBQUAD );
	if (fwrite(rgrgbPalette, cbPalBytes, 1/*count*/, pfile) != 1)
		{ rc = -6; goto GetOut; }


	for ( i = 0; i < height; ++i )
	{
		memcpy( pbBmpBits + (size_t)biTrueWidth * i,
			pbBits + (size_t)( height - i - 1 ) * width, width );
	}

	// Write bitmap bits (remainder of file)
	if (fwrite(pbBmpBits, cbBmpBits, 1/*count*/, pfile) != 1)
		{ rc = -7; goto GetOut; }

GetOut:
	free( pbBmpBits );
	if (pfile) 
		fclose(pfile);

	return rc;
}

/*
=================
=
= LoadLBM
=
=================
*/

void LoadLBM (char *filename, byte **picture, byte **palette)
{
	byte* LBMbuffer = NULL;
	byte* picbuffer = NULL;
	byte* cmapbuffer = NULL;
	byte* LBM_P;
	byte* LBMEND_P;
	int filelength, formtype, formlength;
	int headerRead = 0;
	char* reason = "Malformed LBM file";

	if ( !filename || !picture || !palette || picture == palette )
		Error( "LoadLBM: invalid parameters" );
	*picture = NULL;
	*palette = NULL;
	filelength = LoadFile( filename, (void**)&LBMbuffer );
	if ( !LBMbuffer || filelength < 12 || *(int*)LBMbuffer != LittleLong( FORMID ) )
		goto BadFile;

	formlength = BigLong( *(int*)( LBMbuffer + 4 ) );
	if ( formlength < 4 || formlength > filelength - 8 )
		goto BadFile;
	LBMEND_P = LBMbuffer + 8 + formlength;
	formtype = LittleLong( *(int*)( LBMbuffer + 8 ) );
	if ( formtype != ILBMID && formtype != PBMID )
		goto BadFile;
	LBM_P = LBMbuffer + 12;

	while ( LBM_P < LBMEND_P )
	{
		int chunktype, chunklength;
		if ( LBMEND_P - LBM_P < 8 )
			goto BadFile;
		chunktype = LittleLong( *(int*)LBM_P );
		chunklength = BigLong( *(int*)( LBM_P + 4 ) );
		LBM_P += 8;
		if ( chunklength < 0 || chunklength == INT_MAX || Align( chunklength ) > LBMEND_P - LBM_P )
			goto BadFile;

		switch ( chunktype )
		{
		case BMHDID:
			if ( chunklength < sizeof( bmhd ) )
				goto BadFile;
			memcpy( &bmhd, LBM_P, sizeof( bmhd ) );
			bmhd.w = BigShort( bmhd.w );
			bmhd.h = BigShort( bmhd.h );
			bmhd.x = BigShort( bmhd.x );
			bmhd.y = BigShort( bmhd.y );
			bmhd.pageWidth = BigShort( bmhd.pageWidth );
			bmhd.pageHeight = BigShort( bmhd.pageHeight );
			headerRead = 1;
			break;

		case CMAPID:
			if ( chunklength > 768 )
				goto BadFile;
			if ( !cmapbuffer )
				cmapbuffer = malloc( 768 );
			if ( !cmapbuffer )
			{
				reason = "LoadLBM: out of memory";
				goto BadFile;
			}
			memset( cmapbuffer, 0, 768 );
			memcpy( cmapbuffer, LBM_P, chunklength );
			break;

		case BODYID:
		{
			byte* body_p = LBM_P;
			byte* bodyEnd = LBM_P + chunklength;
			byte* pic_p;
			int rowsize, rowstride, planes, y, p;
			void ( *mungecall )( int, byte* ) = NULL;

			if ( !headerRead || !bmhd.w || !bmhd.h
				|| ( bmhd.compression != cm_none && bmhd.compression != cm_rle1 ) )
				goto BadFile;
			planes = 1;
			rowsize = bmhd.w;
			rowstride = Align( rowsize );
			if ( formtype == ILBMID )
			{
				planes = bmhd.nPlanes + ( bmhd.masking == ms_mask );
				rowsize = ( bmhd.w + 15 ) / 16 * 2;
				rowstride = rowsize;
				if ( planes > sizeof( bitplanes ) / sizeof( bitplanes[0] ) || rowsize > BPLANESIZE )
					goto BadFile;
				switch ( bmhd.nPlanes )
				{
				case 1: mungecall = MungeBitPlanes1; break;
				case 2: mungecall = MungeBitPlanes2; break;
				case 4: mungecall = MungeBitPlanes4; break;
				case 8: mungecall = MungeBitPlanes8; break;
				default:
					goto BadFile;
				}
			}

			free( picbuffer );
			picbuffer = malloc( (size_t)bmhd.w * bmhd.h );
			if ( !picbuffer )
			{
				reason = "LoadLBM: out of memory";
				goto BadFile;
			}
			pic_p = picbuffer;
			for ( y = 0; y < bmhd.h; ++y, pic_p += bmhd.w )
			{
				for ( p = 0; p < planes; ++p )
				{
					byte* dest = formtype == PBMID ? pic_p : bitplanes[p];
					if ( bmhd.compression == cm_rle1 )
					{
						byte* scan = body_p;
						int remaining = rowsize;
						while ( remaining )
						{
							int run, packed;
							if ( scan >= bodyEnd )
								goto BadFile;
								run = *scan++;
								if ( run == 128 )
									continue;
								packed = run > 128 ? 1 : run + 1;
								run = run > 128 ? 257 - run : run + 1;
								if ( run > remaining || packed > bodyEnd - scan )
									goto BadFile;
								scan += packed;
								remaining -= run;
						}
						body_p = LBMRLEDecompress( body_p, dest, rowsize );
					}
					else
					{
						if ( rowsize > bodyEnd - body_p
							|| rowstride > LBM_P + Align( chunklength ) - body_p )
							goto BadFile;
						memcpy( dest, body_p, rowsize );
						body_p += rowstride;
					}
				}
				if ( formtype == ILBMID )
				{
					if ( !mungecall )
						goto BadFile;
					mungecall( bmhd.w, pic_p );
				}
			}
			break;
		}
		}
		LBM_P += Align( chunklength );
	}
	if ( !picbuffer )
		goto BadFile;
	free( LBMbuffer );
	*picture = picbuffer;
	*palette = cmapbuffer;
	return;

BadFile:
	free( LBMbuffer );
	free( picbuffer );
	free( cmapbuffer );
	Error( "%s: %s", reason, filename );
}


/*
============================================================================

							WRITE LBM

============================================================================
*/

/*
==============
=
= WriteLBMfile
=
==============
*/

void WriteLBMfile (char *filename, byte *data, int width, int height, byte *palette)
{
	byte    *lbm, *lbmptr;
	int    *formlength, *bmhdlength, *cmaplength, *bodylength;
	int    length;
	bmhd_t  basebmhd;
	int rowbytes, bodybytes, y;

	if ( !filename || !data || !palette || width <= 0 || height <= 0
		|| width > USHRT_MAX || height > USHRT_MAX )
		Error( "WriteLBMfile: invalid parameters" );
	rowbytes = Align( width );
	if ( height > ( INT_MAX - 1000 ) / rowbytes )
		Error( "WriteLBMfile: image is too large" );
	bodybytes = rowbytes * height;
	lbm = lbmptr = malloc( bodybytes + 1000 );
	if ( !lbm )
		Error( "WriteLBMfile: out of memory" );

//
// start FORM
//
	*lbmptr++ = 'F';
	*lbmptr++ = 'O';
	*lbmptr++ = 'R';
	*lbmptr++ = 'M';

	formlength = (int*)lbmptr;
	lbmptr+=4;                      // leave space for length

	*lbmptr++ = 'P';
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = ' ';

//
// write BMHD
//
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = 'H';
	*lbmptr++ = 'D';

	bmhdlength = (int *)lbmptr;
	lbmptr+=4;                      // leave space for length

	memset (&basebmhd,0,sizeof(basebmhd));
	basebmhd.w = BigShort((short)width);
	basebmhd.h = BigShort((short)height);
	basebmhd.nPlanes = (BYTE)BigShort(8);
	basebmhd.xAspect = (BYTE)BigShort(5);
	basebmhd.yAspect = (BYTE)BigShort(6);
	basebmhd.pageWidth = BigShort((short)width);
	basebmhd.pageHeight = BigShort((short)height);

	memcpy (lbmptr,&basebmhd,sizeof(basebmhd));
	lbmptr += sizeof(basebmhd);

	length = lbmptr-(byte *)bmhdlength-4;
	*bmhdlength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// write CMAP
//
	*lbmptr++ = 'C';
	*lbmptr++ = 'M';
	*lbmptr++ = 'A';
	*lbmptr++ = 'P';

	cmaplength = (int *)lbmptr;
	lbmptr+=4;                      // leave space for length

	memcpy (lbmptr,palette,768);
	lbmptr += 768;

	length = lbmptr-(byte *)cmaplength-4;
	*cmaplength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// write BODY
//
	*lbmptr++ = 'B';
	*lbmptr++ = 'O';
	*lbmptr++ = 'D';
	*lbmptr++ = 'Y';

	bodylength = (int *)lbmptr;
	lbmptr+=4;                      // leave space for length

	for ( y = 0; y < height; ++y )
	{
		memcpy( lbmptr, data + (size_t)y * width, width );
		lbmptr += width;
		if ( width & 1 )
			*lbmptr++ = 0;
	}

	length = lbmptr-(byte *)bodylength-4;
	*bodylength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// done
//
	length = lbmptr-(byte *)formlength-4;
	*formlength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// write output file
//
	SaveFile (filename, lbm, lbmptr-lbm);
	free (lbm);
}

