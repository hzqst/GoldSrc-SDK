// bmpwrite.cpp -- export model textures as indexed bitmaps

#include "mdldec.h"

#ifdef _WIN32
#include <windows.h>
#else
#include "../../engine/win32_types.h"
#endif

void CMDLDecompiler::WriteTextures()
{
	byte* data = m_pModelData;
	int dataSize = m_nModelSize;
	studiohdr_t* header = (studiohdr_t*)data;
	if ( !header->numtextures )
	{
		data = m_pTextureData;
		dataSize = m_nTextureSize;
		header = (studiohdr_t*)data;
	}

	mstudiotexture_t* textures = (mstudiotexture_t*)( data + header->textureindex );
	const byte* dataEnd = data + dataSize;
	for ( int i = 0; i < header->numtextures; i++ )
	{
		// guard: numtextures/textureindex come from the file; stop when the texture
		// descriptor leaves the loaded buffer.
		if ( (const byte*)&textures[i] < data || (const byte*)&textures[i + 1] > dataEnd )
			break;
		mstudiotexture_t* texture = &textures[i];
		byte* pixels = data + texture->index;
		// guard: texture->index/width/height come from the file; skip a texture whose
		// 8-bit pixel data plus its 256-colour palette do not fit in the loaded buffer.
		// WriteBMP reads width*height pixel bytes then 256*3 palette bytes and allocates
		// width*height bytes, so a garbage size would read past the buffer (and give
		// malloc a wild size).
		if ( texture->width < 0 || texture->height < 0 || pixels < data || pixels > dataEnd
			|| (long long)texture->width * texture->height + 256 * 3 > dataEnd - pixels )
			continue;
		WriteBMP( pixels, pixels + texture->width * texture->height, texture->name, texture );
	}
}

void CMDLDecompiler::WriteBMP( const byte* pixels, const byte* palette, const char* name,
		const mstudiotexture_t* texture )
{
	char basename[68];
	char filename[268];

	// The original also extracts the base name, then writes under the full texture name.
	ExtractFileBase( name, basename, sizeof( basename ) );
	// guard: output path (command line) plus a name from the file can exceed filename[268]; the
	// native sprintf overruns it. Take the existing "cannot write" path when it does not fit.
	int written = _snprintf( filename, sizeof( filename ), "%s%s", m_szOutputPath, name );
	filename[sizeof( filename ) - 1] = 0;
	if ( written < 0 || written >= (int)sizeof( filename ) )
	{
		Log( 2, "ERROR: Cannot write texture file %s\r\n", filename );
		return;
	}
	FILE* file = fopen( filename, "wb" );
	if ( !file )
	{
		Log( 2, "ERROR: Cannot write texture file %s\r\n", filename );
		return;
	}

	RGBQUAD colors[256];
	BITMAPFILEHEADER fileHeader;
	BITMAPINFOHEADER infoHeader;
	int width = texture->width;
	int size = width * texture->height;
	int paletteSize = sizeof( colors );
	fileHeader.bfType = 0x4D42;
	fileHeader.bfSize = size + paletteSize + sizeof( fileHeader ) + sizeof( infoHeader );
	fileHeader.bfReserved1 = 0;
	fileHeader.bfReserved2 = 0;
	fileHeader.bfOffBits = paletteSize + sizeof( fileHeader ) + sizeof( infoHeader );
	fwrite( &fileHeader, sizeof( fileHeader ), 1, file );

	infoHeader.biSize = sizeof( infoHeader );
	infoHeader.biWidth = width;
	infoHeader.biHeight = texture->height;
	infoHeader.biPlanes = 1;
	infoHeader.biBitCount = 8;
	infoHeader.biCompression = BI_RGB;
	infoHeader.biSizeImage = 0;
	infoHeader.biXPelsPerMeter = 0;
	infoHeader.biYPelsPerMeter = 0;
	infoHeader.biClrUsed = 256;
	infoHeader.biClrImportant = 0;
	fwrite( &infoHeader, sizeof( infoHeader ), 1, file );

	const byte* source = palette;
	for ( int i = 0; i < (int)infoHeader.biClrUsed; i++ )
	{
		colors[i].rgbRed = *source++;
		colors[i].rgbGreen = *source++;
		colors[i].rgbBlue = *source++;
		colors[i].rgbReserved = 0;
	}
	paletteSize = infoHeader.biClrUsed * sizeof( RGBQUAD );
	fwrite( colors, paletteSize, 1, file );

	byte* flipped = (byte*)malloc( size );
	source = pixels + texture->width * ( texture->height - 1 );
	for ( int i = 0; i < infoHeader.biHeight; i++ )
	{
		memcpy( flipped + i * width, source, texture->width );
		source -= texture->width;
	}
	fwrite( flipped, size, 1, file );
	free( flipped );
	fclose( file );
	Log( 0, "Texture: %s\r\n", filename );
}
