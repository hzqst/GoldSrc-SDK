#ifndef CLIPBOARDTEXT_H
#define CLIPBOARDTEXT_H

#ifdef _WIN32
#pragma once
#endif

#include <string.h>
#include <wchar.h>

// copies the bytes of text from offset on into buf, cut to fit bufLen including the terminator
// the bytes are not decoded, so a cut or an offset may split a multibyte character
// returns the bytes copied without the terminator, and 0 when offset is not within text
inline int CopyClipboardText( const char* text, int length, int offset, char* buf, int bufLen )
{
	buf[0] = 0;
	if ( offset < 0 || offset >= length )
	{
		return 0;
	}

	int count = length - offset;
	if ( count > bufLen - 1 )
	{
		count = bufLen - 1;
	}

	memcpy( buf, text + offset, count );
	buf[count] = 0;
	return count;
}

// copies the wide characters of text from offset on into buf, cut at a whole character to fit capacity including the terminator
// returns the wide characters copied including the terminator, and 0 when offset is past the end of text
inline int CopyClipboardText( const wchar_t* text, int length, int offset, wchar_t* buf, int capacity )
{
	buf[0] = 0;
	if ( offset < 0 || offset > length )
	{
		return 0;
	}

	int count = length - offset;
	if ( count > capacity - 1 )
	{
		count = capacity - 1;
		// do not keep the high half of a surrogate pair without its low half
		if ( sizeof( wchar_t ) == 2 && count > 0 && text[offset + count - 1] >= 0xD800 && text[offset + count - 1] <= 0xDBFF )
		{
			count--;
		}
	}

	wmemcpy( buf, text + offset, count );
	buf[count] = 0;
	return count + 1;
}

#endif // CLIPBOARDTEXT_H
