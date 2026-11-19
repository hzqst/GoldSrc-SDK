#ifndef VGUI_H
#define VGUI_H

#include <stddef.h>

#define VGUI_RGBA_BYTES	4

// 1 keeps the class layouts, vtables and exports of the original vgui.dll.
// Other values allow ABI-breaking fixes such as virtual destructors.
#ifndef VGUI_ABI_COMPAT_MODE
#define VGUI_ABI_COMPAT_MODE	1
#endif

// VGUI_STATIC builds the sources into the program itself, without exports.
#if defined( _WIN32 ) && !defined( VGUI_STATIC )
#if defined( VGUI_EXPORTS )
#define VGUIAPI __declspec( dllexport )
#else
#define VGUIAPI __declspec( dllimport )
#endif
#else
#define VGUIAPI
#endif

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef unsigned long	ulong;

void	vgui_setMalloc( void* ( *theMalloc )( unsigned int ) );
void	vgui_setFree( void ( *theFree )( void* ) );

namespace vgui
{

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef unsigned long	ulong;

VGUIAPI void	vgui_strcpy( char* dst, int dstLen, const char* src );
VGUIAPI char*	vgui_strdup( const char* src );
VGUIAPI int		vgui_printf( const char* format, ... );

VGUIAPI int		vgui_dprintf( const char* format, ... );
VGUIAPI int		vgui_dprintf2( const char* format, ... );

void	Font_Reset();

}

#endif
