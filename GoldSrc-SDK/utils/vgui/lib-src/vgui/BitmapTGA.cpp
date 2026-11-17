#include "VGUI_BitmapTGA.h"

#include "VGUI.h"
#include "VGUI_DataInputStream.h"

#include <limits.h>

#define TGA_TRUE_COLOR		2
#define TGA_RLE_TRUE_COLOR	10
#define TGA_RGB_BITS		24
#define TGA_RGBA_BITS		32
#define TGA_RUN_MASK		0x7F

namespace vgui
{

BitmapTGA::BitmapTGA( InputStream* is, bool invertAlpha )
{
	loadTGA( is, invertAlpha );
}

bool BitmapTGA::loadTGA( InputStream* is, bool invertAlpha )
{
	if ( is == NULL )
		return false;

	DataInputStream dis( is );
	bool success = false;

	uchar idLength = dis.readUChar( success );
	if ( !success )
		return false;

	uchar colorMapType = dis.readUChar( success );
	if ( !success )
		return false;

	uchar imageType = dis.readUChar( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	dis.readUChar( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	dis.readUShort( success );
	if ( !success )
		return false;

	int wide = dis.readUShort( success );
	if ( !success )
		return false;

	int tall = dis.readUShort( success );
	if ( !success )
		return false;

	uchar pixelSize = dis.readUChar( success );
	if ( !success )
		return false;

	dis.readUChar( success );
	if ( !success )
		return false;

	if ( imageType != TGA_TRUE_COLOR && imageType != TGA_RLE_TRUE_COLOR )
		return false;

	if ( colorMapType != 0 )
		return false;

	if ( pixelSize != TGA_RGB_BITS && pixelSize != TGA_RGBA_BITS )
		return false;

	if ( wide != 0 && tall > INT_MAX / VGUI_RGBA_BYTES / wide )
		return false;

	setSize( wide, tall );
	if ( _rgba == NULL )
		return false;

	if ( idLength != 0 )
		dis.seekRelative( idLength, success );

	if ( imageType == TGA_TRUE_COLOR )
	{
		int row = tall;
		int offset = VGUI_RGBA_BYTES * wide * ( tall - 1 );

		while ( row != 0 )
		{
			uchar* dest = &_rgba[offset];

			for ( int column = 0; column < wide; ++column )
			{
				if ( pixelSize == TGA_RGB_BITS )
				{
					dest[2] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[1] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[0] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[3] = 255;
					dest += VGUI_RGBA_BYTES;
				}
				else if ( pixelSize == TGA_RGBA_BITS )
				{
					dest[2] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[1] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[0] = dis.readUChar( success );
					if ( !success )
						return false;

					dest[3] = (uchar)( 255 - dis.readUChar( success ) );
					if ( !success )
						return false;

					if ( invertAlpha )
						dest[3] = (uchar)( 255 - dest[3] );

					dest += VGUI_RGBA_BYTES;
				}
			}

			--row;
			offset += -VGUI_RGBA_BYTES * wide;
		}

		return true;
	}

	int row = tall - 1;

	while ( row != -1 )
	{
		int column = 0;
		uchar* dest = &_rgba[VGUI_RGBA_BYTES * row * wide];

		while ( column < wide )
		{
			signed char packet = (signed char)dis.readUChar( success );
			if ( !success )
				return false;

			int count = ( packet & TGA_RUN_MASK ) + 1;

			if ( packet >= 0 )
			{
				for ( int i = 0; i < count; ++i )
				{
					if ( pixelSize == TGA_RGB_BITS )
					{
						dest[2] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[1] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[0] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[3] = 255;
						if ( invertAlpha )
							dest[3] = 0;

						dest += VGUI_RGBA_BYTES;
					}
					else if ( pixelSize == TGA_RGBA_BITS )
					{
						dest[2] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[1] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[0] = dis.readUChar( success );
						if ( !success )
							return false;

						dest[3] = dis.readUChar( success );
						if ( !success )
							return false;

						if ( invertAlpha )
							dest[3] = (uchar)( 255 - dest[3] );

						dest += VGUI_RGBA_BYTES;
					}

					++column;
					if ( column == wide )
					{
						if ( row == 0 )
							return true;

						--row;
						column = 0;
						dest = &_rgba[VGUI_RGBA_BYTES * row * wide];
					}
				}
			}
			else
			{
				uchar blue = 0;
				uchar green = 0;
				uchar red = 0;
				uchar alpha = 0;

				if ( pixelSize == TGA_RGB_BITS )
				{
					blue = dis.readUChar( success );
					if ( !success )
						return false;

					green = dis.readUChar( success );
					if ( !success )
						return false;

					red = dis.readUChar( success );
					if ( !success )
						return false;

					alpha = 255;
					if ( invertAlpha )
						alpha = 0;
				}
				else if ( pixelSize == TGA_RGBA_BITS )
				{
					blue = dis.readUChar( success );
					if ( !success )
						return false;

					green = dis.readUChar( success );
					if ( !success )
						return false;

					red = dis.readUChar( success );
					if ( !success )
						return false;

					alpha = dis.readUChar( success );
					if ( !success )
						return false;

					if ( invertAlpha )
						alpha = (uchar)( 255 - alpha );
				}

				for ( int i = 0; i < count; ++i )
				{
					dest[0] = red;
					dest[1] = green;
					dest[2] = blue;
					dest[3] = alpha;
					dest += VGUI_RGBA_BYTES;

					++column;
					if ( column == wide )
					{
						if ( row == 0 )
							return true;

						--row;
						column = 0;
						dest = &_rgba[VGUI_RGBA_BYTES * row * wide];
					}
				}
			}
		}

		--row;
	}

	return true;
}

}
