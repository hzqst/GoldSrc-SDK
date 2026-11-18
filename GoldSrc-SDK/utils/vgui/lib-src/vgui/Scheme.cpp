#include "VGUI_Scheme.h"

#include "VGUI_Cursor.h"
#include "VGUI_Font.h"

#include <string.h>

namespace vgui
{

static void SetDefaultColors( Scheme* scheme )
{
	scheme->setColor( Scheme::sc_user, 0, 0, 0, 0 );
	scheme->setColor( Scheme::sc_black, 0, 0, 0, 0 );
	scheme->setColor( Scheme::sc_white, 255, 255, 255, 0 );
	scheme->setColor( Scheme::sc_primary1, 102, 102, 153, 0 );
	scheme->setColor( Scheme::sc_primary2, 153, 153, 204, 0 );
	scheme->setColor( Scheme::sc_primary3, 204, 204, 255, 0 );
	scheme->setColor( Scheme::sc_secondary1, 102, 102, 102, 0 );
	scheme->setColor( Scheme::sc_secondary2, 153, 153, 153, 0 );
	scheme->setColor( Scheme::sc_secondary3, 204, 204, 204, 0 );
}

void Scheme::setColor( SchemeColor sc, int r, int g, int b, int a )
{
	_color[sc][0] = r;
	_color[sc][1] = g;
	_color[sc][2] = b;
	_color[sc][3] = a;
}

void Scheme::getColor( SchemeColor sc, int& r, int& g, int& b, int& a )
{
	r = _color[sc][0];
	g = _color[sc][1];
	b = _color[sc][2];
	a = _color[sc][3];
}

void Scheme::setFont( SchemeFont sf, Font* font )
{
	if ( font != NULL )
		_font[sf] = font;
}

Font* Scheme::getFont( SchemeFont sf )
{
	return _font[sf];
}

void Scheme::setCursor( SchemeCursor scu, Cursor* cursor )
{
	if ( cursor != NULL )
		_cursor[scu] = cursor;
}

Cursor* Scheme::getCursor( SchemeCursor scu )
{
	return _cursor[scu];
}

Scheme::Scheme()
{
	SetDefaultColors( this );

	setFont( sf_primary1, new Font( "Arial", NULL, 0, 20, 0, 0.0f, 400, false, false, false, false ) );
	setFont( sf_primary2, new Font( "FixedSys", NULL, 0, 18, 0, 0.0f, 400, false, false, false, false ) );
	setFont( sf_primary3, new Font( "Arial", NULL, 0, 12, 0, 0.0f, 400, false, false, false, false ) );
	setFont( sf_secondary1, new Font( "Marlett", NULL, 0, 16, 0, 0.0f, 0, false, false, false, true ) );

	setCursor( scu_none, new Cursor( Cursor::DefaultCursor::dc_none ) );
	setCursor( scu_arrow, new Cursor( Cursor::DefaultCursor::dc_arrow ) );
	setCursor( scu_ibeam, new Cursor( Cursor::DefaultCursor::dc_ibeam ) );
	setCursor( scu_hourglass, new Cursor( Cursor::DefaultCursor::dc_hourglass ) );
	setCursor( scu_crosshair, new Cursor( Cursor::DefaultCursor::dc_crosshair ) );
	setCursor( scu_up, new Cursor( Cursor::DefaultCursor::dc_up ) );
	setCursor( scu_sizenwse, new Cursor( Cursor::DefaultCursor::dc_sizenwse ) );
	setCursor( scu_sizenesw, new Cursor( Cursor::DefaultCursor::dc_sizenesw ) );
	setCursor( scu_sizewe, new Cursor( Cursor::DefaultCursor::dc_sizewe ) );
	setCursor( scu_sizens, new Cursor( Cursor::DefaultCursor::dc_sizens ) );
	setCursor( scu_sizeall, new Cursor( Cursor::DefaultCursor::dc_sizeall ) );
	setCursor( scu_no, new Cursor( Cursor::DefaultCursor::dc_no ) );
	setCursor( scu_hand, new Cursor( Cursor::DefaultCursor::dc_hand ) );

#if VGUI_ABI_COMPAT_MODE != 1
	_font[sf_user] = NULL;
	_cursor[scu_user] = NULL;
	memcpy( _ownFont, _font, sizeof( _ownFont ) );
	memcpy( _ownCursor, _cursor, sizeof( _ownCursor ) );
#endif
}

#if VGUI_ABI_COMPAT_MODE != 1
Scheme::Scheme( const Scheme& other )
{
	memset( _ownCursor, 0, sizeof( _ownCursor ) );
	memset( _ownFont, 0, sizeof( _ownFont ) );
	*this = other;
}

Scheme::~Scheme()
{
	for ( int i = 0; i < sf_last; ++i )
		delete _ownFont[i];
	for ( int i = 0; i < scu_last; ++i )
		delete _ownCursor[i];
}

// Where the other scheme shows its own font or cursor, this one shows its own
// copy of it. No font or cursor is freed, as panels may still use it.
Scheme& Scheme::operator=( const Scheme& other )
{
	if ( this == &other )
		return *this;

	memcpy( _color, other._color, sizeof( _color ) );

	for ( int i = 0; i < sf_last; ++i )
	{
		if ( other._font[i] != NULL && other._font[i] == other._ownFont[i] )
		{
			if ( _ownFont[i] == NULL )
				_ownFont[i] = new Font( *other._ownFont[i] );
			_font[i] = _ownFont[i];
		}
		else
		{
			_font[i] = other._font[i];
		}
	}

	for ( int i = 0; i < scu_last; ++i )
	{
		if ( other._cursor[i] != NULL && other._cursor[i] == other._ownCursor[i] )
		{
			if ( _ownCursor[i] == NULL )
				_ownCursor[i] = new Cursor( other._ownCursor[i]->getDefaultCursor() );
			_cursor[i] = _ownCursor[i];
		}
		else
		{
			_cursor[i] = other._cursor[i];
		}
	}

	return *this;
}

void Scheme::restoreDefaults()
{
	SetDefaultColors( this );
	memcpy( _font, _ownFont, sizeof( _font ) );
	memcpy( _cursor, _ownCursor, sizeof( _cursor ) );
}

void* Scheme::operator new( size_t size )
{
	return ::operator new( size );
}

void Scheme::operator delete( void* memory )
{
	::operator delete( memory );
}
#endif

}
