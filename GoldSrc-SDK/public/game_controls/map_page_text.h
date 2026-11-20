#pragma once

// A map page file is either narrow text or UTF-16LE text that starts with a byte order mark.
// Both decoders drop '\r', so CRLF files show the same on every platform.

// Returns true when the count bytes read from the file are UTF-16LE text.
inline bool IsWideMapPageText( const char* bytes, int count )
{
	return count >= 2 && static_cast<unsigned char>( bytes[0] ) == 0xFF && static_cast<unsigned char>( bytes[1] ) == 0xFE;
}

// Drops '\r' from the count bytes in place and terminates the text; bytes must hold count + 1 chars.
inline void DecodeNarrowMapPageText( char* bytes, int count )
{
	int length = 0;
	for ( int i = 0; i < count; ++i )
	{
		if ( bytes[i] != '\r' )
			bytes[length++] = bytes[i];
	}
	bytes[length] = '\0';
}

// Decodes the whole code units after the byte order mark and drops a trailing odd byte;
// text must hold count / 2 wide chars, the byte order mark's slot holding the terminator.
inline void DecodeWideMapPageText( const char* bytes, int count, wchar_t* text )
{
	int length = 0;
	for ( int i = 2; i + 1 < count; i += 2 )
	{
		wchar_t ch = static_cast<wchar_t>( static_cast<unsigned char>( bytes[i] ) | ( static_cast<unsigned char>( bytes[i + 1] ) << 8 ) );
		if ( ch != L'\r' )
			text[length++] = ch;
	}
	text[length] = L'\0';
}
