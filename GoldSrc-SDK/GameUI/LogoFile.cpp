#include "common/common.h"

#include "filesystem.h"
#include "const.h"
#include "../engine/utlbuffer.h"
#include "vgui_controls/Controls.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGO_BUFFER_SIZE	20480
#define LOGO_PALETTE_SIZE	256
#define LOGO_MIP_LEVELS	4

typedef struct
{
	char	identification[4]; // should be WAD2 or 2DAW
	int		numlumps;
	int		infotableofs;
} wadinfo_t;

typedef struct
{
	int		filepos;
	int		disksize;
	int		size; // uncompressed
	char	type;
	char	compression;
	char	pad1;
	char	pad2;
	char	name[16]; // must be null terminated
} lumpinfo_t;

typedef struct
{
	char			name[16];
	unsigned int	width;
	unsigned int	height;
	unsigned int	offsets[LOGO_MIP_LEVELS];
} miptex_t;

typedef struct _TargaHeader
{
	byte			id_length;
	byte			colormap_type;
	byte			image_type;
	unsigned short	colormap_index;
	unsigned short	colormap_length;
	byte			colormap_size;
	unsigned short	x_origin;
	unsigned short	y_origin;
	unsigned short	width;
	unsigned short	height;
	byte			pixel_size;
	byte			attributes;
} TargaHeader;

byte		pixdata[LOGO_PALETTE_SIZE];
float		linearpalette[LOGO_PALETTE_SIZE][3];
float		d_red, d_green, d_blue;
int			colors_used;
int			color_used[LOGO_PALETTE_SIZE];
float		maxdistortion;
byte		palLogo[LOGO_PALETTE_SIZE * 3];
TargaHeader	targa_header;

int fgetLittleShort( unsigned char** p )
{
	byte b1, b2;

	b1 = *( ( *p )++ );
	b2 = *( ( *p )++ );

	return (short)( b1 + ( b2 << 8 ) );
}

int fgetLittleLong( unsigned char** p )
{
	byte b1, b2, b3, b4;

	b1 = *( ( *p )++ );
	b2 = *( ( *p )++ );
	b3 = *( ( *p )++ );
	b4 = *( ( *p )++ );

	return b1 + ( b2 << 8 ) + ( b3 << 16 ) + ( b4 << 24 );
}

byte AveragePixels( int )
{
	return pixdata[0];
}

/*
==============
GrabMip

filename MIP x y width height
must be multiples of sixteen
==============
*/
int GrabMip( LPBITMAPINFO lpbmi, int dibSize, unsigned char* lump_p, char* lumpname, COLORREF crf, int* width, int* height )
{
	int x, y, xl, yl, xh, yh, w, h;
	unsigned char *screen_p, *source;
	miptex_t* qtex;
	int miplevel, mipstep;
	int xx, yy;
	int count;
	int byteimagewidth, byteimageheight;
	unsigned char* byteimage;

	unsigned char* lump_start = lump_p;

	xl = yl = 0;
	w = lpbmi->bmiHeader.biWidth;
	h = lpbmi->bmiHeader.biHeight;

	*width = w;
	*height = h;

	byteimage = (unsigned char*)lpbmi + sizeof( BITMAPINFOHEADER ) + LOGO_PALETTE_SIZE * sizeof( RGBQUAD );

	if ( ( w & 15 ) || ( h & 15 ) )
		return 0; //Error ("line %i: miptex sizes must be multiples of 16", scriptline);

	if ( (size_t)( sizeof( BITMAPINFOHEADER ) + LOGO_PALETTE_SIZE * sizeof( RGBQUAD ) ) + (size_t)w * (size_t)h > (size_t)dibSize )
		return 0;

	xh = xl + w;
	yh = yl + h;

	qtex = (miptex_t*)lump_p;
	qtex->width = (unsigned)( w );
	qtex->height = (unsigned)( h );
	strcpy( qtex->name, lumpname );

	lump_p = (byte*)&qtex->offsets[4];

	byteimagewidth = w;
	byteimageheight = h;

	source = (byte*)lump_p;
	qtex->offsets[0] = (unsigned int)( (byte*)lump_p - (byte*)qtex );

	// We're reading from a dib, so go bottom up
	screen_p = byteimage + ( h - 1 ) * w;
	for ( y = yl; y < yh; y++ )
	{
		for ( x = xl; x < xh; x++ )
			*lump_p++ = *screen_p++;

		screen_p -= 2 * w;
	}

	// calculate gamma corrected linear palette
	int i;
	for ( i = 0; i < LOGO_PALETTE_SIZE; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			float f;
			f = (float)( palLogo[i * 3 + j] / 255.0 );
			linearpalette[i][j] = f;
		}
	}

	maxdistortion = 0;

	colors_used = LOGO_PALETTE_SIZE;

	for ( i = 0; i < LOGO_PALETTE_SIZE; i++ )
		color_used[i] = 1;

	//
	// subsample for greater mip levels
	//
	for ( miplevel = 1; miplevel < LOGO_MIP_LEVELS; miplevel++ )
	{
		d_red = d_green = d_blue = 0; // no distortion yet
		qtex->offsets[miplevel] = (unsigned int)( lump_p - lump_start );

		mipstep = 1 << miplevel;

		for ( y = 0; y < h; y += mipstep )
		{
			for ( x = 0; x < w; x += mipstep )
			{
				count = 0;
				for ( yy = 0; yy < mipstep; yy++ )
					for ( xx = 0; xx < mipstep; xx++ )
						pixdata[count++] = source[( y + yy ) * w + x + xx];

				*lump_p++ = AveragePixels( count );
			}
		}
	}

	// Write out palette in 16bit mode
	*(unsigned short*)lump_p = LOGO_PALETTE_SIZE; // palette size
	lump_p += sizeof( short );

	memcpy( lump_p, &palLogo[0], 765 );
	lump_p += 765;

	*lump_p++ = (unsigned char)( crf & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 8 ) & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 16 ) & 0xFF );

	return (int)( lump_p - lump_start );
}

bool UpdateLogoWAD( void* dib, int dibSize, int red, int green, int blue )
{
	char logoName[32];
	sprintf( logoName, "LOGO" );
	char* name = &logoName[0];
	if ( !*name || !dib )
		return false;

	byte* lump = (byte*)malloc( LOGO_BUFFER_SIZE );
	if ( !lump )
		return false;
	memset( lump, 0, LOGO_BUFFER_SIZE );

	if ( dibSize < (int)( sizeof( BITMAPINFOHEADER ) + LOGO_PALETTE_SIZE * sizeof( RGBQUAD ) ) )
	{
		free( lump );
		return false;
	}

	CUtlBuffer buffer( 0, LOGO_BUFFER_SIZE, false );
	int width, height;
	COLORREF color = (byte)red | ( (byte)green << 8 ) | ( (byte)blue << 16 );

	LONG imageWide = ( (BITMAPINFO*)dib )->bmiHeader.biWidth;
	LONG imageTall = ( (BITMAPINFO*)dib )->bmiHeader.biHeight;
	if ( !( ( imageWide | imageTall ) & 15 ) && imageWide > 0 && imageTall > 0
		&& imageTall > ( LOGO_BUFFER_SIZE - 810 ) * 64 / 85 / imageWide )
	{
		free( lump );
		return false;
	}

	int lumpSize = GrabMip( (BITMAPINFO*)dib, dibSize, lump, name, color, &width, &height );
	if ( !lumpSize )
	{
		return false;
	}

	bool sizeValid = false;
	if ( width == height )
	{
		if ( width == 16 || width == 32 || width == 64 )
			sizeValid = true;
	}
	if ( !sizeValid )
	{
		free( lump );
		return false;
	}

	while ( lumpSize & 3 )
		lumpSize++;

	wadinfo_t header;
	memcpy( header.identification, "WAD3", sizeof( header.identification ) );
	header.numlumps = 1;
	header.infotableofs = 0;
	buffer.Put( &header, sizeof( header ) );

	lumpinfo_t info;
	memset( &info, 0, sizeof( info ) );
	strcpy( info.name, name );
	info.filepos = sizeof( header );
	info.disksize = lumpSize;
	info.size = lumpSize;
	info.type = 64;
	buffer.Put( lump, lumpSize );
	// Write info table
	buffer.Put( &info, sizeof( info ) );
	int end = buffer.TellPut();
	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	header.infotableofs = lumpSize + sizeof( header );
	buffer.Put( &header, sizeof( header ) );
	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, end );

	FileHandle_t file = vgui2::filesystem()->Open( "tempdecal.wad", "wb", "GAMECONFIG" );
	if ( file )
	{
		vgui2::filesystem()->Write( buffer.Base(), buffer.TellPut(), file );
		vgui2::filesystem()->Close( file );
	}

	free( lump );
	return true;
}

static unsigned char ScaleByAlpha( unsigned char channel, unsigned char alpha )
{
	if ( !alpha )
		return 0;

	return (unsigned char)(int)( 255.0f / alpha * channel );
}

int GrabMipFromTGA( unsigned char* byteimage, unsigned char* lump_p, char* lumpname, COLORREF crf, int width, int height )
{
	int x, y, xl, yl, xh, yh, w, h;
	unsigned char* source;
	miptex_t* qtex;
	int miplevel, mipstep;
	int xx, yy;
	int count;
	int byteimagewidth, byteimageheight;
	unsigned char* lump_start = lump_p;

	xl = yl = 0;
	w = width;
	h = height;

	if ( ( w & 15 ) || ( h & 15 ) )
		return 0;

	xh = xl + w;
	yh = yl + h;

	qtex = (miptex_t*)lump_p;
	qtex->width = (unsigned)( w );
	qtex->height = (unsigned)( h );
	strcpy( qtex->name, lumpname );

	lump_p = (byte*)&qtex->offsets[4];

	byteimagewidth = w;
	byteimageheight = h;

	source = (byte*)lump_p;
	qtex->offsets[0] = (unsigned int)( (byte*)lump_p - (byte*)qtex );

	unsigned char* screen_p = byteimage;
	for ( y = yl; y < yh; y++ )
	{
		for ( x = xl; x < xh; x++ )
		{
			if ( crf & 0xFF )
				*lump_p = ScaleByAlpha( screen_p[0], screen_p[3] );
			else if ( ( crf >> 8 ) & 0xFF )
				*lump_p = ScaleByAlpha( screen_p[1], screen_p[3] );
			else if ( ( crf >> 16 ) & 0xFF )
				*lump_p = ScaleByAlpha( screen_p[2], screen_p[3] );

			lump_p++;
			screen_p += 4;
		}
	}

	// calculate gamma corrected linear palette
	int i;
	for ( i = 0; i < LOGO_PALETTE_SIZE; i++ )
	{
		for ( int j = 0; j < 3; j++ )
		{
			float f;
			f = (float)( palLogo[i * 3 + j] / 255.0 );
			linearpalette[i][j] = f;
		}
	}

	maxdistortion = 0;

	colors_used = LOGO_PALETTE_SIZE;

	for ( i = 0; i < LOGO_PALETTE_SIZE; i++ )
		color_used[i] = 1;

	// subsample for greater mip levels
	for ( miplevel = 1; miplevel < LOGO_MIP_LEVELS; miplevel++ )
	{
		d_red = d_green = d_blue = 0; // no distortion yet
		qtex->offsets[miplevel] = (unsigned int)( lump_p - lump_start );

		mipstep = 1 << miplevel;

		for ( y = 0; y < h; y += mipstep )
		{
			for ( x = 0; x < w; x += mipstep )
			{
				count = 0;
				for ( yy = 0; yy < mipstep; yy++ )
					for ( xx = 0; xx < mipstep; xx++ )
						pixdata[count++] = source[( y + yy ) * w + x + xx];

				*lump_p++ = AveragePixels( count );
			}
		}
	}

	// Write out palette in 16bit mode
	*(unsigned short*)lump_p = LOGO_PALETTE_SIZE; // palette size
	lump_p += sizeof( short );

	memcpy( lump_p, &palLogo[0], 765 );
	lump_p += 765;

	*lump_p++ = (unsigned char)( crf & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 8 ) & 0xFF );
	*lump_p++ = (unsigned char)( ( crf >> 16 ) & 0xFF );

	return (int)( lump_p - lump_start );
}

bool LoadTGA( const char* szFilename, byte* buffer, int bufferSize, int* width, int* height, int errFail );

bool UpdateLogoWADFromTGA( const char* tga )
{
	char logoname[32];
	strcpy( logoname, "LOGO" );

	COLORREF r = 0x000000FF;
	COLORREF g = 0x0000FF00;
	COLORREF b = 0x00FF0000;

	CUtlBuffer sourceBuffer( 0, LOGO_BUFFER_SIZE, false );
	int width, height;
	if ( !LoadTGA( tga, (unsigned char*)sourceBuffer.Base(), sourceBuffer.Size(), &width, &height, false ) )
		return false;

	CUtlBuffer rBuffer( 0, LOGO_BUFFER_SIZE, false );
	CUtlBuffer gBuffer( 0, LOGO_BUFFER_SIZE, false );
	CUtlBuffer bBuffer( 0, LOGO_BUFFER_SIZE, false );
	memset( rBuffer.Base(), 0, rBuffer.Size() );
	memset( gBuffer.Base(), 0, gBuffer.Size() );
	memset( bBuffer.Base(), 0, bBuffer.Size() );
	int length_r = GrabMipFromTGA( (unsigned char*)sourceBuffer.Base(), (unsigned char*)rBuffer.Base(), logoname, r, width, height );
	while ( length_r & 3 )
		length_r++;
	int length_g = GrabMipFromTGA( (unsigned char*)sourceBuffer.Base(), (unsigned char*)gBuffer.Base(), logoname, g, width, height );
	while ( length_g & 3 )
		length_g++;
	int length_b = GrabMipFromTGA( (unsigned char*)sourceBuffer.Base(), (unsigned char*)bBuffer.Base(), logoname, b, width, height );
	while ( length_b & 3 )
		length_b++;

	if ( length_r == 0 || length_g == 0 || length_b == 0 )
		return false;
	if ( width != height )
		return false;
	if ( width != 16 && width != 32 && width != 64 )
		return false;

	// Write Header
	wadinfo_t header;
	header.identification[0] = 'W';
	header.identification[1] = 'A';
	header.identification[2] = 'D';
	header.identification[3] = '3';
	header.numlumps = 1;
	header.infotableofs = length_r + length_g + length_b + sizeof( wadinfo_t );

	CUtlBuffer buffer( 0, LOGO_BUFFER_SIZE, false );
	buffer.Put( &header, sizeof( wadinfo_t ) );

	// Write Lump
	buffer.Put( rBuffer.Base(), length_r );
	buffer.Put( rBuffer.Base(), length_g );
	buffer.Put( rBuffer.Base(), length_b );

	// Fill Ino info table
	lumpinfo_t info;
	memset( &info, 0, sizeof( info ) );
	strcpy( info.name, logoname );
	info.filepos = (int)sizeof( wadinfo_t );
	info.size = info.disksize = length_r;
	info.type = 64;
	info.compression = 0;

	buffer.Put( &info, sizeof( lumpinfo_t ) );

	info.size = info.disksize = length_b;
	info.filepos = (int)sizeof( wadinfo_t ) + length_r + length_g;

	// Output to file
	FileHandle_t file = vgui2::filesystem()->Open( "tempdecal.wad", "wb", "GAMECONFIG" );
	if ( file )
	{
		vgui2::filesystem()->Write( buffer.Base(), buffer.TellPut(), file );
		vgui2::filesystem()->Close( file );
	}

	return true;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
bool LoadTGA( const char* szFilename, byte* buffer, int bufferSize, int* width, int* height, int errFail )
{
	int columns, rows, numPixels;
	byte* pixbuf;
	byte* targa_rgba;
	int row, column;
	unsigned char* tgafile_mem_buffer;
	unsigned char* p;

	(void)errFail;

	FileHandle_t f = vgui2::filesystem()->Open( szFilename, "rb" );
	int fileLen = vgui2::filesystem()->Size( f );

#ifdef _MSC_VER
#pragma warning( suppress : 6255 )
#endif
	tgafile_mem_buffer = (unsigned char*)_alloca( fileLen );
	if ( !tgafile_mem_buffer )
		return false;

	vgui2::filesystem()->Read( tgafile_mem_buffer, fileLen, f );
	vgui2::filesystem()->Close( f );

	unsigned char* end = tgafile_mem_buffer + fileLen;
	p = tgafile_mem_buffer;

	if ( width )
		*width = 0;
	if ( height )
		*height = 0;

	if ( fileLen < 18 )
		return false;

	targa_header.id_length = *p++;
	targa_header.colormap_type = *p++;
	targa_header.image_type = *p++;

	targa_header.colormap_index = (unsigned short)fgetLittleShort( &p );
	targa_header.colormap_length = (unsigned short)fgetLittleShort( &p );
	targa_header.colormap_size = *p++;
	targa_header.x_origin = (unsigned short)fgetLittleShort( &p );
	targa_header.y_origin = (unsigned short)fgetLittleShort( &p );
	targa_header.width = (unsigned short)fgetLittleShort( &p );
	targa_header.height = (unsigned short)fgetLittleShort( &p );
	targa_header.pixel_size = *p++;
	targa_header.attributes = *p++;

	if ( ( targa_header.image_type != 2 && targa_header.image_type != 10 ) || targa_header.colormap_type != 0
		|| ( targa_header.pixel_size != 32 && targa_header.pixel_size != 24 ) )
		return false;

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = (int)( (unsigned int)columns * rows );

	if ( width )
		*width = targa_header.width;
	if ( height )
		*height = targa_header.height;

	if ( ( numPixels * ( targa_header.pixel_size == 24 ) ? 3 : 4 ) > bufferSize )
		return false;

	if ( (unsigned int)columns * (unsigned int)rows > (unsigned int)bufferSize / 4 )
		return false;

	targa_rgba = buffer;

	if ( end - p < targa_header.id_length )
		return false;

	if ( targa_header.id_length != 0 )
		p += targa_header.id_length; // skip TARGA image comment

	if ( targa_header.image_type == 2 ) // Uncompressed, RGB images
	{
		if ( end - p < numPixels * ( targa_header.pixel_size / 8 ) )
			return false;

		for ( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;
			for ( column = 0; column < columns; column++ )
			{
				unsigned char red, green, blue, alphabyte;
				switch ( targa_header.pixel_size )
				{
				case 24:
					blue = *p++;
					green = *p++;
					red = *p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					blue = *p++;
					green = *p++;
					red = *p++;
					alphabyte = *p++;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if ( targa_header.image_type == 10 ) // Runlength encoded RGB images
	{
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;
		for ( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;
			for ( column = 0; column < columns; )
			{
				if ( p >= end )
					return false;

				packetHeader = *p++;
				packetSize = 1 + ( packetHeader & 0x7f );

				if ( packetHeader & 0x80 ) // run-length packet
				{

					if ( end - p < targa_header.pixel_size / 8 )
						return false;

					switch ( targa_header.pixel_size )
					{
					case 24:
						blue = *p++;
						green = *p++;
						red = *p++;
						alphabyte = 255;
						break;
					case 32:
						blue = *p++;
						green = *p++;
						red = *p++;
						alphabyte = *p++;
						break;
					}

					for ( j = 0; j < packetSize; j++ )
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if ( column == columns ) // run spans across rows
						{
							column = 0;
							if ( row > 0 )
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else // non run-length packet
				{
					for ( j = 0; j < packetSize; j++ )
					{
						if ( end - p < targa_header.pixel_size / 8 )
							return false;

						switch ( targa_header.pixel_size )
						{
						case 24:
							blue = *p++;
							green = *p++;
							red = *p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							blue = *p++;
							green = *p++;
							red = *p++;
							alphabyte = *p++;
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						}
						column++;
						if ( column == columns ) // pixel packet run spans across rows
						{
							column = 0;
							if ( row > 0 )
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
		breakOut:;
		}
	}

	if ( width )
		*width = targa_header.width;
	if ( height )
		*height = targa_header.height;

	return true;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif
