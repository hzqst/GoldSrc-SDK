#include "fileimage.h"

#include <limits.h>
#include <string.h>

#define TGA_RUN_LENGTH_IMAGE	((unsigned char)10)
#define TGA_RUN_FLAG			((unsigned char)0x80)
#define TGA_RUN_MASK			((unsigned char)0x7f)
#define TGA_PIXEL_BYTES			4

namespace
{

#pragma pack( push, 1 )
struct TgaHeader
{
	unsigned char	idLength;
	unsigned char	colorMapType;
	unsigned char	imageType;
	unsigned short	colorMapStart;
	unsigned short	colorMapLength;
	unsigned char	colorMapDepth;
	unsigned short	xOrigin;
	unsigned short	yOrigin;
	unsigned short	wide;
	unsigned short	tall;
	unsigned char	pixelDepth;
	unsigned char	descriptor;
};
#pragma pack( pop )

void WriteRun( FILE* file, const unsigned char* pixel, int count )
{
	const unsigned char header = (unsigned char)( ( count - 1 ) | TGA_RUN_FLAG );
	fwrite( &header, 1, sizeof( header ), file );
	fwrite( pixel, 1, TGA_PIXEL_BYTES, file );
}

}

FileImageStream_Memory::FileImageStream_Memory( void* data, int length )
	: _data( (const unsigned char*)data ), _length( length ), _position( 0 ), _error( false )
{
}

void FileImageStream_Memory::Read( void* output, int length )
{
	unsigned char* bytes = (unsigned char*)output;
	for ( int i = 0; i < length; ++i )
	{
		if ( _position < _length )
			bytes[i] = _data[_position++];
		else
		{
			bytes[i] = 0;
			_error = true;
		}
	}
}

bool FileImageStream_Memory::ErrorStatus()
{
	const bool error = _error;
	_error = false;
	return error;
}

void FileImage::Clear()
{
	delete[] m_pData;
	m_pData = NULL;
	m_Width = m_Height = 0;
}

bool Load32BitTGA( FileImageStream* stream, FileImage* image )
{
	image->Clear();
	TgaHeader header = {};
	stream->Read( &header, sizeof( header ) );
	if ( header.pixelDepth != TGA_PIXEL_BYTES * CHAR_BIT || header.imageType != TGA_RUN_LENGTH_IMAGE )
		return false;

	for ( int i = 0; i < header.idLength; ++i )
	{
		unsigned char ignored;
		stream->Read( &ignored, 1 );
	}

	image->m_Width = header.wide;
	image->m_Height = header.tall;
	image->m_pData = new unsigned char[TGA_PIXEL_BYTES * image->m_Width * image->m_Height];
	if ( image->m_pData == NULL )
		return false;

	for ( int y = image->m_Height - 1; y >= 0; --y )
	{
		unsigned char* destination = image->m_pData + y * image->m_Width * TGA_PIXEL_BYTES;
		int x = 0;
		while ( x < image->m_Width )
		{
			unsigned char packet;
			stream->Read( &packet, 1 );
			const int count = ( packet & TGA_RUN_MASK ) + 1;
			x += count;
			if ( x > image->m_Width )
				return false;

			unsigned char pixel[TGA_PIXEL_BYTES];
			if ( ( packet & TGA_RUN_FLAG ) != 0 )
				stream->Read( pixel, TGA_PIXEL_BYTES );

			for ( int i = 0; i < count; ++i )
			{
				if ( ( packet & TGA_RUN_FLAG ) == 0 )
					stream->Read( pixel, TGA_PIXEL_BYTES );

				memcpy( destination, pixel, TGA_PIXEL_BYTES );
				destination += TGA_PIXEL_BYTES;
			}
		}
	}

	// A truncated stream pads its reads with zeros; reject it instead of returning a blank image.
	return !stream->ErrorStatus();
}

void Save32BitTGA( FILE* file, FileImage* image )
{
	TgaHeader header = {};
	header.imageType = TGA_RUN_LENGTH_IMAGE;
	header.wide = (unsigned short)image->m_Width;
	header.tall = (unsigned short)image->m_Height;
	header.pixelDepth = TGA_PIXEL_BYTES * CHAR_BIT;
	fwrite( &header, 1, sizeof( header ), file );
	for ( int y = image->m_Height - 1; y >= 0; --y )
	{
		const unsigned char* row = image->m_pData + y * image->m_Width * TGA_PIXEL_BYTES;
		int start = 0;
		for ( int x = 0; x < image->m_Width; ++x )
		{
			if ( x - start > TGA_RUN_MASK
				|| memcmp( row + x * TGA_PIXEL_BYTES, row + start * TGA_PIXEL_BYTES, TGA_PIXEL_BYTES ) != 0 )
			{
				WriteRun( file, row + start * TGA_PIXEL_BYTES, x - start );
				start = x;
			}
		}
		if ( start < image->m_Width )
			WriteRun( file, row + start * TGA_PIXEL_BYTES, image->m_Width - start );
	}
}
