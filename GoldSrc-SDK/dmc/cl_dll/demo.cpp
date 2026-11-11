//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "demo.h"
#include "demo_api.h"
#include <memory.h>
#include <stdlib.h>

extern "C" 
{
	void EXPORT Demo_ReadBuffer( int size, unsigned char *buffer );
}

/*
=====================
Demo_WriteBuffer

Write some data to the demo stream
=====================
*/
void Demo_WriteBuffer( int type, int size, unsigned char *buffer )
{
	const int bufferSize = 32 * 1024;
	if ( size < 0 || size > bufferSize - (int)sizeof( int ) || ( size && !buffer ) )
		return;

	int pos = 0;
	unsigned char* buf = (unsigned char*)malloc( bufferSize );
	if ( !buf )
		return;
	*( int * )&buf[pos] = type;
	pos+=sizeof( int );

	if ( size )
		memcpy( &buf[pos], buffer, size );

	// Write full buffer out
	gEngfuncs.pDemoAPI->WriteBuffer( size + sizeof( int ), buf );
	free( buf );
}

/*
=====================
Demo_ReadBuffer

Engine wants us to parse some data from the demo stream
=====================
*/
void EXPORT Demo_ReadBuffer( int size, unsigned char *buffer )
{
	if ( !buffer || size < (int)sizeof( int ) )
		return;
	int type;
	int i = 0;

	type = *( int * )buffer;
	i += sizeof( int );
	switch ( type )
	{
	case TYPE_USER:
		break;
	default:
		gEngfuncs.Con_DPrintf( "Unknown demo buffer type, skipping.\n" );
		break;
	}
}
