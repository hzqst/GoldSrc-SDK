#ifndef VGUI_STRING_H
#define VGUI_STRING_H

#include "VGUI.h"

namespace vgui
{

class VGUIAPI String
{
public:
	String();
	String( const char* text );
	String( const String& src );
	String& operator=( const String& src );
	~String();

private:
	int getCount( const char* text );

public:
	int getCount();

	String operator+( String text );
	String operator+( const char* text );
	bool operator==( String text );
	bool operator==( const char* text );
	char operator[]( int index );

	const char* getChars();
	static void test();

private:
	char*	_text;
};

}

#endif
