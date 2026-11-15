#include "vgui_int.h"
#include "vgui_intwrap2.h"
#include "vgui_EngineSurface.h"
#include "const.h"
#include "filesystem.h"
#ifdef _WIN32
#include <Windows.h>
#endif
#include <cstdlib>

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

int VGui_LoadBMP( FileHandle_t file, byte* buffer, int, int* width, int* height )
{
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER* bmih;
	RGBQUAD* palette;
	byte *image, *pixels, *src, *dest;
	unsigned int size;
	int i, j, pitch;
	int loaded = 0;

	size = FS_Size( file );
	FS_Read( &bmfh, sizeof( bmfh ), 1, file );
	if ( bmfh.bfType == 0x4D42 )
	{
		size -= sizeof( bmfh );
		image = (byte*)malloc( size );
		FS_Read( image, (int)size, 1, file );

		bmih = (BITMAPINFOHEADER*)image;
		palette = (RGBQUAD*)( image + sizeof( BITMAPINFOHEADER ) );
		pixels = image + ( bmfh.bfOffBits - sizeof( bmfh ) );

		*width = bmih->biWidth;
		*height = bmih->biHeight;

		pitch = *width;
		if ( pitch % 4 )
			pitch += ( pitch + 4 ) % 4;

		for ( i = 0; i < *height; i++ )
		{
			src = pixels + ( *height - i - 1 ) * pitch;
			dest = buffer + 4 * ( i * *width );
			for ( j = 0; j < *width; j++, dest += 4 )
			{
				dest[0] = palette[src[j]].rgbRed;
				dest[1] = palette[src[j]].rgbGreen;
				dest[2] = palette[src[j]].rgbBlue;
				dest[3] = 255;
			}
		}

		loaded = 1;
		free( image );
	}

	FS_Close( file );
	return loaded;
}
