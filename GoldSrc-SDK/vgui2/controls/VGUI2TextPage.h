#pragma once
#if USE_HLCLI

#include <algorithm>
#include <stdint.h>
#include <string>

namespace vgui2automation
{
struct TextPage
{
	std::wstring	text;
	int				length;
	int				next;
};

// Scalar offsets keep UTF-16 surrogate pairs together across page boundaries.
inline TextPage SliceText( const wchar_t* text, size_t length, int offset, int limit )
{
	offset = ( std::max )( 0, offset );
	limit = ( std::max )( 1, limit );
	size_t begin = length, end = length, pos = 0;
	int count = 0;
	for ( ; pos < length && text[pos]; ++count )
	{
		if ( count == offset )
			begin = pos;
		if ( static_cast<int64_t>( count ) == static_cast<int64_t>( offset ) + limit )
			end = pos;
		const unsigned ch = static_cast<unsigned>( text[pos++] );
		if ( sizeof( wchar_t ) == 2 && ch >= 0xd800 && ch <= 0xdbff && pos < length && text[pos] >= 0xdc00
			&& text[pos] <= 0xdfff )
			++pos;
	}
	begin = ( std::min )( begin, pos );
	end = ( std::min )( end, pos );
	const int64_t next = static_cast<int64_t>( offset ) + limit;
	return { std::wstring( text + begin, text + end ), count, next < count ? static_cast<int>( next ) : -1 };
}
} // namespace vgui2automation
#endif
