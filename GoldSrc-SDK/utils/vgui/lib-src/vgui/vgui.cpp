#include "VGUI.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The global new / delete are the CRT's. These hooks are not exported and are
// kept only as declared entry points: switching the allocator after anything was
// allocated would free those blocks with the wrong function.
void vgui_setMalloc( void* ( * )( unsigned int ) )
{
}

void vgui_setFree( void ( * )( void* ) )
{
}

namespace vgui
{

void vgui_strcpy( char* dst, int dstLen, const char* src )
{
	if ( dst == NULL || dstLen <= 0 )
	{
		return;
	}

	if ( src == NULL )
	{
		src = "";
	}

	int srcLen = (int)strlen( src ) + 1;
	if ( srcLen > dstLen )
	{
		srcLen = dstLen;
	}
	// src may point into dst, e.g. TextImage::setText with its own text.
	memmove( dst, src, srcLen - 1 );
	dst[srcLen - 1] = 0;
}

char* vgui_strdup( const char* src )
{
	int len = (int)strlen( src );
	char* dst = new char[len + 1];
	memcpy( dst, src, len );
	dst[len] = 0;
	return dst;
}

int vgui_printf( const char* format, ... )
{
	char buf[2048] = {};
	va_list argList;
	va_start( argList, format );
	int ret = vsnprintf( buf, sizeof( buf ), format, argList );
	va_end( argList );
	if ( ret < 0 )
	{
		buf[0] = 0;
	}
	printf( "%s", buf );
	return ret;
}

}
