#ifndef VGUI_SCHEME_H
#define VGUI_SCHEME_H

#include "VGUI.h"

namespace vgui
{

class Cursor;
class Font;

class VGUIAPI Scheme
{
public:
	enum SchemeColor
	{
		sc_user		= 0,
		sc_black,
		sc_white,
		sc_primary1,
		sc_primary2,
		sc_primary3,
		sc_secondary1,
		sc_secondary2,
		sc_secondary3,

		sc_last
	};

	enum SchemeFont
	{
		sf_user		= 0,
		sf_primary1,
		sf_primary2,
		sf_primary3,
		sf_secondary1,

		sf_last
	};

	enum SchemeCursor
	{
		scu_user		= 0,
		scu_none,
		scu_arrow,
		scu_ibeam,
		scu_hourglass,
		scu_crosshair,
		scu_up,
		scu_sizenwse,
		scu_sizenesw,
		scu_sizewe,
		scu_sizens,
		scu_sizeall,
		scu_no,
		scu_hand,

		scu_last
	};

	Scheme();
#if VGUI_ABI_COMPAT_MODE != 1
	// A Scheme owns the cursors its constructor makes and keeps them, also
	// after setCursor replaces them, as panels and surfaces may still use
	// them. A copy gets its own. Fonts and the cursors given to setCursor
	// are not owned.
	Scheme( const Scheme& other );
	virtual ~Scheme();

	// Schemes are freed through the virtual destructor from any module, so
	// they are always allocated and freed on vgui.dll's heap.
	static void* operator new( size_t size );
	static void operator delete( void* memory );
#endif

	virtual void setColor( SchemeColor sc, int r, int g, int b, int a );
	virtual void getColor( SchemeColor sc, int& r, int& g, int& b, int& a );

	virtual void setFont( SchemeFont sf, Font* font );
	virtual Font* getFont( SchemeFont sf );

	virtual void setCursor( SchemeCursor scu, Cursor* cursor );
	virtual Cursor* getCursor( SchemeCursor scu );

private:
	int		_color[sc_last][4];
	Font*	_font[sf_last];
	Cursor*	_cursor[scu_last];
#if VGUI_ABI_COMPAT_MODE != 1
	Cursor*	_ownCursor[scu_last];
#endif

public:
#if VGUI_ABI_COMPAT_MODE != 1
	Scheme& operator=( const Scheme& other );
#else
	Scheme& operator=( const Scheme& ) = default;
#endif
};

}

#endif
