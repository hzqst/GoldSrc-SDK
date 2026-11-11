//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "Border.h"
#include "KeyValues.h"
#include "Scheme.h"
#include "VPanel.h"
#include "vgui_internal.h"

#include <vgui/ISurface.h>

#include <stdio.h>
#include <string.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui2;

Border::Border()
{
	_inset[0] = 0;
	_inset[1] = 0;
	_inset[2] = 0;
	_inset[3] = 0;
	_name = NULL;

	memset( _sides, 0, sizeof( _sides ) );
}

Border::~Border()
{
	int i;

	delete[] _name;

	for ( i = 0; i < 4; i++ )
	{
		delete[] _sides[i].lines;
	}
}

void Border::SetInset( int left, int top, int right, int bottom )
{
	_inset[SIDE_LEFT] = left;
	_inset[SIDE_TOP] = top;
	_inset[SIDE_RIGHT] = right;
	_inset[SIDE_BOTTOM] = bottom;
}

void Border::GetInset( int& left, int& top, int& right, int& bottom )
{
	left = _inset[SIDE_LEFT];
	top = _inset[SIDE_TOP];
	right = _inset[SIDE_RIGHT];
	bottom = _inset[SIDE_BOTTOM];
}

void Border::Paint( int x, int y, int wide, int tall )
{
	Paint( x, y, wide, tall, -1, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Border::Paint( VPANEL panel )
{
	int wide;
	int tall;

	( (vgui2::VPanel*)panel )->GetSize( wide, tall );
	Paint( 0, 0, wide, tall, -1, 0, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Draws the border with the specified size
// Input  : *panel - 
//			x - 
//			y - 
//			wide - 
//			tall - 
//-----------------------------------------------------------------------------
void Border::Paint( int x, int y, int wide, int tall, int breakSide, int breakStart, int breakEnd )
{
	int i;
	for ( i = 0; i < _sides[SIDE_LEFT].count; i++ )
	{
		line_t* line = &_sides[SIDE_LEFT].lines[i];
		g_pSurface->DrawSetColor( line->col[0], line->col[1], line->col[2], line->col[3] );

		if ( breakSide == SIDE_LEFT )
		{
			// split into two section
			if ( breakStart > 0 )
			{
				g_pSurface->DrawFilledRect( x + i, y + line->startOffset, x + i + 1, y + breakStart );
			}

			if ( breakEnd < tall - line->endOffset )
			{
				g_pSurface->DrawFilledRect( x + i, y + breakEnd + 1, x + i + 1, tall - line->endOffset );
			}
		}
		else
		{
			g_pSurface->DrawFilledRect( x + i, y + line->startOffset, x + i + 1, tall - line->endOffset );
		}
	}

	// draw top
	for ( i = 0; i < _sides[SIDE_TOP].count; i++ )
	{
		line_t* line = &_sides[SIDE_TOP].lines[i];
		g_pSurface->DrawSetColor( line->col[0], line->col[1], line->col[2], line->col[3] );

		if ( breakSide == SIDE_TOP )
		{
			// split into two section
			if ( breakStart > 0 )
			{
				g_pSurface->DrawFilledRect( x + line->startOffset, y + i, x + breakStart, y + i + 1 );
			}

			if ( breakEnd < wide - line->endOffset )
			{
				g_pSurface->DrawFilledRect( x + breakEnd + 1, y + i, wide - line->endOffset, y + i + 1 );
			}
		}
		else
		{
			g_pSurface->DrawFilledRect( x + line->startOffset, y + i, wide - line->endOffset, y + i + 1 );
		}
	}

	// draw right
	for ( i = 0; i < _sides[SIDE_RIGHT].count; i++ )
	{
		line_t* line = &_sides[SIDE_RIGHT].lines[i];
		g_pSurface->DrawSetColor( line->col[0], line->col[1], line->col[2], line->col[3] );
		g_pSurface->DrawFilledRect( wide - ( i + 1 ), y + line->startOffset,
				( wide - ( i + 1 ) ) + 1, tall - line->endOffset );
	}

	// draw bottom
	for ( i = 0; i < _sides[SIDE_BOTTOM].count; i++ )
	{
		line_t* line = &_sides[SIDE_BOTTOM].lines[i];
		g_pSurface->DrawSetColor( line->col[0], line->col[1], line->col[2], line->col[3] );
		g_pSurface->DrawFilledRect( x + line->startOffset, tall - ( i + 1 ),
				wide - line->endOffset, ( tall - ( i + 1 ) ) + 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : const char
//-----------------------------------------------------------------------------
const char* Border::GetName()
{
	if ( _name )
	{
		return _name;
	}

	return "";
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void Border::SetName( const char* name )
{
	if ( _name )
	{
		delete[] _name;
	}

	int len = strlen( name ) + 1;
	_name = new char[len];
	strcpy( _name, name );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Border::ApplySchemeSettings( IScheme* pScheme, KeyValues* inResourceData )
{
	// load inset information
	const char* insetString = inResourceData->GetString( "inset", "0 0 0 0" );
	int left;
	int top;
	int right;
	int bottom;

	GetInset( left, top, right, bottom );
	sscanf( insetString, "%d %d %d %d", &left, &top, &right, &bottom );
	SetInset( left, top, right, bottom );

	// get the border information from the scheme
	ParseSideSettings( SIDE_LEFT, inResourceData->FindKey( "Left", false ), pScheme );
	ParseSideSettings( SIDE_TOP, inResourceData->FindKey( "Top", false ), pScheme );
	ParseSideSettings( SIDE_RIGHT, inResourceData->FindKey( "Right", false ), pScheme );
	ParseSideSettings( SIDE_BOTTOM, inResourceData->FindKey( "Bottom", false ), pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : side_index - 
//			*inResourceData - 
//-----------------------------------------------------------------------------
void Border::ParseSideSettings( int side_index, KeyValues* inResourceData, IScheme* pScheme )
{
	int count;
	KeyValues* kv;
	int index;

	if ( !inResourceData )
	{
		return;
	}

	count = 0;
	for ( kv = inResourceData->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
	{
		count++;
	}

	// allocate memory
	_sides[side_index].count = count;
	_sides[side_index].lines = new line_t[count];

	index = 0;
	for ( kv = inResourceData->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
	{
		line_t* line = &_sides[side_index].lines[index];
		const char* col;
		int start = 0;
		int end = 0;

		col = kv->GetString( "color", NULL );
		line->col = pScheme->GetColor( col, Color( 0, 0, 0, 0 ) );

		col = kv->GetString( "offset", NULL );
		if ( col )
		{
			sscanf( col, "%d %d", &start, &end );
		}

		line->startOffset = start;
		line->endOffset = end;
		index++;
	}
}
