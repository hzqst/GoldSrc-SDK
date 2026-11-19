#include "VGUI_String.h"

#include <stdio.h>
#include <string.h>

namespace vgui
{

String::String()
{
	_text = new char[1];
	_text[0] = 0;
}

String::String( const char* text )
{
	int count;

	if ( text == NULL )
	{
		text = "";
	}

	count = getCount( text );
	_text = new char[count + 1];
	memcpy( _text, text, count + 1 );
}

String::String( const String& src )
{
	int count;

	count = getCount( src._text );
	_text = new char[count + 1];
	memcpy( _text, src._text, count + 1 );
}

String::~String()
{
	delete[] _text;
	_text = NULL;
}

String& String::operator=( const String& src )
{
	char* text;
	int   count;

	if ( this == &src )
	{
		return *this;
	}

	count = getCount( src._text );
	text = new char[count + 1];
	memcpy( text, src._text, count + 1 );
	delete[] _text;
	_text = text;
	return *this;
}

int String::getCount( const char* text )
{
	int count;

	for ( count = 0; text[count] != 0; ++count )
	{
	}

	return count;
}

int String::getCount()
{
	return getCount( _text );
}

String String::operator+( String text )
{
	return operator+( text._text );
}

String String::operator+( const char* text )
{
	int    count1;
	int    count2;
	String result;

	count1 = getCount();
	count2 = getCount( text );

	delete[] result._text;
	result._text = new char[count1 + count2 + 1];
	memcpy( result._text, _text, count1 );
	memcpy( result._text + count1, text, count2 );
	result._text[count1 + count2] = 0;
	return result;
}

bool String::operator==( String text )
{
	const char* a;
	const char* b;

	a = _text;
	b = text._text;
	for ( ;; )
	{
		if ( *a != *b )
		{
			return false;
		}
		if ( *a == 0 )
		{
			break;
		}
		a++;
		b++;
	}

	return true;
}

bool String::operator==( const char* text )
{
	const char* a;

	a = _text;
	for ( ;; )
	{
		if ( *a != *text )
		{
			return false;
		}
		if ( *a == 0 )
		{
			break;
		}
		a++;
		text++;
	}

	return true;
}

char String::operator[]( int index )
{
	return _text[index];
}

const char* String::getChars()
{
	return _text;
}

void String::test()
{
	printf( "{\n" );

	String aa( "aa" );
	printf( "aa [%s]\n", aa._text );

	String bb( "bb" );
	printf( "bb [%s]\n", bb._text );

	String cc( "cc" );
	printf( "cc [%s]\n", cc._text );

	String dd( "dd" );
	printf( "dd [%s]\n", dd._text );

	String aacc = aa + cc;
	printf( "aacc [%s]\n", aacc._text );

	String ee( "aa" );
	printf( "}\n" );
}

}
