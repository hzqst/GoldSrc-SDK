#ifndef VGUI_FONT_H
#define VGUI_FONT_H

#include "VGUI.h"

#define VGUI_FIRST_FONT_ID	100

namespace vgui
{

class BaseFontPlat;

class VGUIAPI Font
{
public:
	Font( const char* name, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol );
	Font( const char* name, void* pFileData, int fileDataLen, int tall, int wide, float rotation, int weight,
		bool italic, bool underline, bool strikeout, bool symbol );
#if VGUI_ABI_COMPAT_MODE != 1
	// A Font owns its name and holds a reference to its platform font, which
	// Fonts with the same parameters share. A copy has its own name and holds
	// another reference.
	Font( const Font& other );
	virtual ~Font();

	// Fonts are freed through the virtual destructor from any module, so
	// they are always allocated and freed on vgui.dll's heap.
	static void* operator new( size_t size );
	static void operator delete( void* memory );
#endif

#if defined( _WIN32 )
private:
#endif
	virtual void init( const char* name, void* pFileData, int fileDataLen, int tall, int wide,
		float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol );

public:
	BaseFontPlat* getPlat();
	virtual void getCharRGBA( int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba );
	virtual void getCharABCwide( int ch, int& a, int& b, int& c );
	virtual void getTextSize( const char* text, int& wide, int& tall );
	virtual int getTall();
#if !defined( _WIN32 )
	virtual int getWide();
#endif
	virtual int getId();

private:
	char*			_name;
	BaseFontPlat*	_plat;
	int				_id;

public:
#if VGUI_ABI_COMPAT_MODE != 1
	Font& operator=( const Font& other );
#else
	Font& operator=( const Font& ) = default;
#endif
};

}

#endif
