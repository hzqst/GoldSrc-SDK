#include "common.h"
#include "filesystem.h"
#include "mem.h"
#include "zone.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#include <cstdlib>
#include <cstring>

#define DIB_HEADER_MARKER 0x4D42

void LoadBMP8( FileHandle_t hFile, byte** pPalette, int*, byte** pImage, int* nWidth, int* nHeight )
{
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmih;
	RGBQUAD rgrgbPalette[256];
	unsigned int cbBmpBits;
	unsigned int biTrueWidth;
	unsigned int biNumPixels;
	unsigned int i;
	int cbPalBytes;
	int row;
	byte* pbBmpBits;
	byte* pb;
	byte* src;
	byte* dest;

	*pImage = NULL;
	*pPalette = NULL;
	if ( nWidth )
		*nWidth = 0;
	if ( nHeight )
		*nHeight = 0;

	if ( FS_Read( &bmfh, sizeof( bmfh ), 1, hFile ) != (int)sizeof( bmfh ) || bmfh.bfReserved1 || bmfh.bfReserved2 )
		goto GetOut;

	if ( bmfh.bfSize > FS_Size( hFile ) || FS_Read( &bmih, sizeof( bmih ), 1, hFile ) != (int)sizeof( bmih )
		 || bmih.biSize != sizeof( bmih ) || bmih.biPlanes != 1 || bmih.biBitCount != 8 || bmih.biCompression != BI_RGB
		 || bmih.biWidth <= 0 || bmih.biHeight <= 0 )
		goto GetOut;

	// Figure out how many entires are actually in the table
	if ( !bmih.biClrUsed )
		bmih.biClrUsed = 256;
	else if ( bmih.biClrUsed > 256 )
		goto GetOut;

	cbPalBytes = bmih.biClrUsed * sizeof( RGBQUAD );
	if ( FS_Read( rgrgbPalette, cbPalBytes, 1, hFile ) != cbPalBytes )
		goto GetOut;

	cbBmpBits = bmfh.bfSize - FS_Tell( hFile );

	// data is actually stored with the width being rounded up to a multiple of 4
	biTrueWidth = ( (unsigned int)bmih.biWidth + 3 ) & ~3u;
	biNumPixels = bmih.biHeight * biTrueWidth;
	if ( biNumPixels < biTrueWidth || biNumPixels < (unsigned int)bmih.biHeight || biNumPixels > cbBmpBits )
		goto GetOut;

	// convert to a packed 768 byte palette
	*pPalette = (byte*)malloc( 256 * 3 );
	if ( !*pPalette )
		goto GetOut;

	Q_memset( *pPalette, 0, 256 * 3 );

	// Copy over used entries
	pb = *pPalette;
	for ( i = 0; i < bmih.biClrUsed; i++ )
	{
		*pb++ = rgrgbPalette[i].rgbRed;
		*pb++ = rgrgbPalette[i].rgbGreen;
		*pb++ = rgrgbPalette[i].rgbBlue;
	}

	// Read bitmap bits (remainder of file)
	pbBmpBits = (byte*)Mem_Malloc( cbBmpBits );
	if ( pbBmpBits && FS_Read( pbBmpBits, cbBmpBits, 1, hFile ) == (int)cbBmpBits )
	{
		*pImage = (byte*)Mem_Malloc( cbBmpBits );

		src = pbBmpBits + biTrueWidth * ( bmih.biHeight - 1 );
		dest = *pImage;
		for ( row = 0; row < bmih.biHeight; row++ )
		{
			memmove( dest, src, biTrueWidth );
			src -= biTrueWidth;
			dest += biTrueWidth;
		}

		Mem_Free( pbBmpBits );

		if ( nWidth )
			*nWidth = bmih.biWidth;
		if ( nHeight )
			*nHeight = bmih.biHeight;
	}
	else
	{
		free( *pPalette );
		*pPalette = NULL;
		if ( pbBmpBits )
			Mem_Free( pbBmpBits );
	}

GetOut:
	if ( hFile )
		FS_Close( hFile );
}

byte* LoadBMP16( FileHandle_t fin, qboolean is15bit )
{
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;
	unsigned int paletteBytes;
	unsigned int imageBytes;
	unsigned int doubled;
	unsigned int rows;
	unsigned int row;
	unsigned int redShift, redMask, greenShift, greenMask;
	int width;
	int row16, row8;
	int srcStep;
	byte* palette;
	byte* image8;
	byte* image16;
	byte* srcRow;
	byte* dstRow;
	byte* src;
	byte* entry;
	unsigned short* pixel;
	unsigned short* end;

	if ( FS_Read( &fileHeader, sizeof( fileHeader ), 1, fin ) != 1 || fileHeader.bfType != DIB_HEADER_MARKER
		 || fileHeader.bfReserved1 || fileHeader.bfReserved2 || FS_Read( &infoHeader, sizeof( infoHeader ), 1, fin ) != 1
		 || infoHeader.biSize < sizeof( infoHeader ) || infoHeader.biPlanes != 1 || infoHeader.biCompression != BI_RGB )
	{
		FS_Close( fin );
		return NULL;
	}

	if ( infoHeader.biBitCount == 16 )
	{
		int imageSize = infoHeader.biSizeImage;

		FS_Seek( fin, 0, (FileSystemSeek_t)fileHeader.bfOffBits );
		image16 = (byte*)Hunk_AllocName( imageSize, "SKYBOX" );
		if ( image16 && FS_Read( image16, imageSize, 1, fin ) != imageSize )
			image16 = NULL;
		FS_Close( fin );
		return image16;
	}

	if ( infoHeader.biBitCount != 8 )
	{
		FS_Close( fin );
		return NULL;
	}

	paletteBytes = 256 * 4;
	if ( infoHeader.biClrUsed )
		paletteBytes = infoHeader.biClrUsed * 4;

	palette = (byte*)Mem_Malloc( paletteBytes );
	if ( !palette )
	{
		FS_Close( fin );
		return NULL;
	}
	if ( FS_Read( palette, paletteBytes, 1, fin ) != (int)paletteBytes )
	{
		FS_Close( fin );
		return NULL;
	}

	imageBytes = infoHeader.biSizeImage;
	image8 = (byte*)Mem_Malloc( imageBytes );
	FS_Seek( fin, 0, (FileSystemSeek_t)fileHeader.bfOffBits );
	if ( !image8 )
	{
		FS_Close( fin );
		return NULL;
	}
	if ( FS_Read( image8, imageBytes, 1, fin ) != (int)imageBytes )
	{
		Mem_Free( image8 );
		Mem_Free( palette );
		FS_Close( fin );
		return NULL;
	}

	width = infoHeader.biWidth;
	doubled = (unsigned int)width * 2 + 3;
	row16 = doubled & ~3u;
	row8 = ( (unsigned int)width + ( (int)doubled < 0 ? 6 : 3 ) ) & ~3u;

	rows = infoHeader.biHeight;
	if ( infoHeader.biHeight < 0 )
		rows = 0 - rows;

	image16 = (byte*)Hunk_AllocName( row16 * rows, "SKYBOX" );
	if ( !image16 )
	{
		FS_Close( fin );
		return NULL;
	}

	srcRow = image8;
	srcStep = row8;
	if ( infoHeader.biHeight > 0 )
	{
		srcRow = image8 + ( imageBytes - row8 );
		srcStep = -row8;
	}

	if ( is15bit )
	{
		redShift = 7;
		redMask = 0x7C00;
		greenShift = 2;
		greenMask = 0x3E0;
	}
	else
	{
		redShift = 8;
		redMask = 0xF800;
		greenShift = 3;
		greenMask = 0x7E0;
	}

	dstRow = image16;
	for ( row = 0; row < rows; row++ )
	{
		pixel = (unsigned short*)dstRow;
		end = pixel + width;
		for ( src = srcRow; pixel != end; src++ )
		{
			entry = palette + *src * 4;

			entry[2] |= entry[2] >> 6;
			entry[1] |= entry[1] >> 6;
			entry[0] |= entry[0] >> 6;

			*pixel++ = (unsigned short)( ( entry[0] >> 3 ) | ( ( entry[2] << redShift ) & redMask )
										 | ( ( entry[1] << greenShift ) & greenMask ) );
		}

		dstRow += row16;
		srcRow += srcStep;
	}

	FS_Close( fin );
	return image16;
}
