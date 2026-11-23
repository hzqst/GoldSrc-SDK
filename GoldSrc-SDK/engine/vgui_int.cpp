#include "vgui_int.h"
#include "vgui_intwrap2.h"
#include "vgui_EngineSurface.h"
#include "const.h"
#include "filesystem.h"
#ifdef _WIN32
#include <Windows.h>
#endif
#include <cstdlib>
#include <cstring>

void VGui_Startup()
{
	VGuiWrap_Startup();
	VGuiWrap2_Startup();
}

void VGui_Shutdown()
{
	VGuiWrap_Shutdown();
	VGuiWrap2_Shutdown();
	EngineSurface::freeEngineSurface();
}

void VGui_CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if ( !VGuiWrap2_CallEngineSurfaceAppHandler( event, userData ) )
		VGuiWrap_CallEngineSurfaceAppHandler( event, userData );
}

void* VGui_GetPanel()
{
	return VGuiWrap_GetPanel();
}

void VGui_ReleaseMouse()
{
	VGuiWrap_ReleaseMouse();
	VGuiWrap2_ReleaseMouse();
}

void VGui_GetMouse()
{
	VGuiWrap_GetMouse();
	VGuiWrap2_GetMouse();
}

void VGui_SetVisible( int state )
{
	VGuiWrap_SetVisible( state );
	VGuiWrap2_SetVisible( state );
}

void VGui_Paint()
{
	if ( VGuiWrap2_UseVGUI1() )
		VGuiWrap_Paint( !VGuiWrap2_IsGameUIVisible() );
	else
		VGuiWrap_Paint( 0 );
	VGuiWrap2_Paint();
}

int VGui_GameUIKeyPressed()
{
	return VGuiWrap2_GameUIKeyPressed();
}

int VGui_Key_Event( int down, int keynum, const char* currentBinding )
{
	return VGuiWrap2_Key_Event( down, keynum, currentBinding ) != 0;
}

int VGui_LoadBMP( FileHandle_t file, byte* buffer, int bufferSize, int* width, int* height )
{
	constexpr unsigned int BMP_MAGIC = 0x4D42;
	constexpr unsigned int BMP_COMPRESSION_RGB = 0;
	constexpr unsigned int BMP_PALETTE_ENTRIES = 256;
	constexpr unsigned long long BMP_ROW_ALIGNMENT = 4;
	constexpr unsigned long long BMP_OUTPUT_BYTES = 4;

	BITMAPFILEHEADER bmfh;
	int loaded = 0;

	unsigned int fileSize = FS_Size( file );
	if ( bufferSize > 0
		 && fileSize >= sizeof( bmfh ) + sizeof( BITMAPINFOHEADER )
		 && FS_Read( &bmfh, sizeof( bmfh ), 1, file ) == (int)sizeof( bmfh )
		 && bmfh.bfType == BMP_MAGIC )
	{
		unsigned int bodySize = fileSize - (unsigned int)sizeof( bmfh );
		byte* image = (byte*)malloc( bodySize );
		if ( image && FS_Read( image, (int)bodySize, 1, file ) == (int)bodySize )
		{
			const BITMAPINFOHEADER* bmih = (const BITMAPINFOHEADER*)image;
			unsigned int paletteEntries = bmih->biClrUsed ? bmih->biClrUsed : BMP_PALETTE_ENTRIES;
			unsigned long long pitch =
				( (unsigned long long)bmih->biWidth + BMP_ROW_ALIGNMENT - 1 ) & ~( BMP_ROW_ALIGNMENT - 1 );
			unsigned long long paletteEnd =
				(unsigned long long)bmih->biSize + (unsigned long long)paletteEntries * sizeof( RGBQUAD );
			unsigned long long pixelsStart = (unsigned long long)bmfh.bfOffBits;

			bool validHeader = bmih->biSize >= sizeof( BITMAPINFOHEADER )
				&& bmih->biBitCount == 8
				&& bmih->biCompression == BMP_COMPRESSION_RGB
				&& bmih->biWidth > 0
				&& bmih->biHeight > 0
				&& paletteEntries <= BMP_PALETTE_ENTRIES
				&& paletteEnd <= bodySize;

			bool validPixels = validHeader
				&& pixelsStart >= sizeof( bmfh )
				&& pixelsStart - sizeof( bmfh ) + pitch * (unsigned long long)bmih->biHeight <= bodySize
				&& (unsigned long long)bmih->biWidth * (unsigned long long)bmih->biHeight * BMP_OUTPUT_BYTES
					<= (unsigned long long)bufferSize;

			if ( validPixels )
			{
				RGBQUAD palette[BMP_PALETTE_ENTRIES];
				memset( palette, 0, sizeof( palette ) );
				memcpy( palette, image + bmih->biSize, paletteEntries * sizeof( RGBQUAD ) );

				*width = bmih->biWidth;
				*height = bmih->biHeight;

				const byte* pixels = image + ( pixelsStart - sizeof( bmfh ) );
				for ( int i = 0; i < *height; i++ )
				{
					const byte* src = pixels + (unsigned long long)( *height - i - 1 ) * pitch;
					byte* dest = buffer + 4 * ( i * *width );
					for ( int j = 0; j < *width; j++, dest += 4 )
					{
						dest[0] = palette[src[j]].rgbRed;
						dest[1] = palette[src[j]].rgbGreen;
						dest[2] = palette[src[j]].rgbBlue;
						dest[3] = 255;
					}
				}

				loaded = 1;
			}
		}

		free( image );
	}

	FS_Close( file );
	return loaded;
}
