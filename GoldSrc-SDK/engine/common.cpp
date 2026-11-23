#include "common.h"
#include "engine_apiproxy.h"
#include "bspfile.h"
#include "conprint.h"
#include "delta.h"
#include "filesystem.h"
#include "info.h"
#include "mathlib.h"
#include "mem.h"
#include "modinfo.h"
#include "net.h"
#include "server.h"
#include "sys.h"
#include "unicode_strtools.h"
#include "zone.h"
#include "../common/usercmd.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\common.c" )

#if !defined( _WIN32 )
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _vsnprintf
#define _vsnprintf vsnprintf
#endif
#endif

#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
#include <cassert>
#include <xmmintrin.h>
#endif

int FileSystem_AddFallbackGameDir( const char* gameDirectory );

const char com_empty_string[] = "";

void COM_AddAppDirectory( char* baseDir, const char* )
{
	FS_AddSearchPath( baseDir, "PLATFORM" );
}

void COM_Log( const char* filename, const char* format, ... )
{
	constexpr int LOG_FORMAT_BYTES = 1024;
	char string[LOG_FORMAT_BYTES];
	va_list args;
	va_start( args, format );
	_vsnprintf( string, sizeof( string ), format, args );
	va_end( args );
	FileHandle_t file = FS_Open( filename ? filename : "c:\\hllog.txt", "a+t" );
	if ( file )
	{
		FS_FPrintf( file, "%s", string );
		FS_Close( file );
	}
}

int com_argc;
char** com_argv;
char serverinfo[SERVERINFO_BUFFER_SIZE];
char com_cmdline[CMDLINE_LENGTH];
char com_gamedir[GAME_DIRECTORY_BYTES];
char com_clientfallback[GAME_DIRECTORY_BYTES];
char gpszVersionString[VERSION_STRING_BYTES];
char gpszProductString[VERSION_STRING_BYTES];
/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
cache_user_t* loadcache;
byte* loadbuf;
int loadsize;
int ( *LittleLong )( int );
static const byte swaptest[sizeof( std::uint16_t )] = { 1, 0 };
/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

qboolean bigendien;
short ( *BigShort )( short value );
short ( *LittleShort )( short value );
int ( *BigLong )( int value );
float ( *BigFloat )( float value );
float ( *LittleFloat )( float value );
char com_token[COM_TOKEN_SIZE];
qboolean com_ignorecolons;
int msg_readcount;
qboolean msg_badread;
static qboolean s_com_token_unget;

typedef struct bf_read_s
{
	int msg_readcount;
	sizebuf_t* pbuf;
	int nBitFieldReadStartByte;
	int nBytesRead;
	int nCurInputBit;
	byte* pInByte;
} bf_read_t;

static_assert( sizeof( bf_read_t ) == 24 );
static bf_read_t bfread;

typedef struct
{
	int nCurOutputBit;
	byte* pOutByte;
	sizebuf_t* pbuf;
} bf_write_t;

static_assert( sizeof( bf_write_t ) == 12 );
static_assert( offsetof( bf_write_t, nCurOutputBit ) == 0 );
static_assert( offsetof( bf_write_t, pOutByte ) == 4 );
static_assert( offsetof( bf_write_t, pbuf ) == 8 );
static bf_write_t bfwrite;

static constexpr unsigned int BITS_PER_WORD = sizeof( std::uint32_t ) * CHAR_BIT;
static constexpr std::uint32_t BITTABLE[BITS_PER_WORD + 1] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096,
	8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216, 33554432, 67108864,
	134217728, 268435456, 536870912, 1073741824, 2147483648u, 0 };

static constexpr std::uint32_t ROWBITTABLE[BITS_PER_WORD + 1] = { 0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047,
	4095, 8191, 16383, 32767, 65535, 131071, 262143, 524287, 1048575, 2097151, 4194303, 8388607, 16777215, 33554431,
	67108863, 134217727, 268435455, 536870911, 1073741823, 2147483647, 4294967295u };

static constexpr std::uint32_t INVBITTABLE[BITS_PER_WORD + 1] = { 4294967294u, 4294967293u, 4294967291u, 4294967287u,
	4294967279u, 4294967263u, 4294967231u, 4294967167u, 4294967039u, 4294966783u, 4294966271u, 4294965247u, 4294963199u,
	4294959103u, 4294950911u, 4294934527u, 4294901759u, 4294836223u, 4294705151u, 4294443007u, 4293918719u, 4292870143u,
	4290772991u, 4286578687u, 4278190079u, 4261412863u, 4227858431u, 4160749567u, 4026531839u, 3758096383u, 3221225471u,
	2147483647u, 4294967295u };

static constexpr int NUM_SAFE_ARGVS = 7;
static char* largv[MAX_NUM_ARGVS + NUM_SAFE_ARGVS + 1];
static const char* const argvdummy = " ";
static const char* const safeargvs[NUM_SAFE_ARGVS] = {
	"-stdvid", "-nolan", "-nosound", "-nocdaudio", "-nojoy", "-nomouse", "-dibonly" };

static constexpr int STRING_NOT_EQUAL = -1;

char* Info_Serverinfo()
{
	return serverinfo;
}

int Q_strcmp( const char* first, const char* second )
{
	if ( !first || !second )
		return STRING_NOT_EQUAL;

	while ( *first == *second )
	{
		if ( *first == '\0' )
			return 0;               // strings are equal

		++first;
		++second;
	}

	return STRING_NOT_EQUAL;
}

int Q_strlen( const char* text )
{
	int length = 0;

	if ( text )
	{
		while ( text[length] != '\0' )
			++length;
	}

	return length;
}

int COM_CheckParm( const char* parameter )
{
	int argumentCount = com_argc;
	for ( int index = 1; index < argumentCount; ++index )
	{
		if ( Q_strcmp( parameter, com_argv[index] ) == 0 )
			return index;
	}
	return 0;
}

void COM_ParseDirectoryFromCmd( const char* command, char* directory, int length, const char* defaultDirectory )
{
	int argument = COM_CheckParm( command );
	const char* source = defaultDirectory;
	if ( argument )
	{
		std::uint32_t lastArgument = static_cast<std::uint32_t>( com_argc ) - 1u;
		if ( argument < static_cast<int>( lastArgument ) )
		{
			const char* value = com_argv[argument + 1];
			char firstCharacter = *value;
			if ( firstCharacter != '-' && firstCharacter != '+' )
				source = value;
		}
	}

	if ( source )
		Q_strncpy_s( directory, source, length );
	else
		*directory = '\0';

	int directoryLength = Q_strlen( directory );
	if ( directoryLength > 0 )
	{
		char lastCharacter = directory[directoryLength - 1];
		if ( lastCharacter == '\\' || lastCharacter == '/' )
			directory[directoryLength - 1] = '\0';
	}
}

qboolean COM_SetupDirectories()
{
	constexpr int BASE_DIRECTORY_BYTES = 512;
	constexpr int DIRECTORY_INFO_LIMIT = 512;
	char basedir[BASE_DIRECTORY_BYTES];
	com_clientfallback[0] = '\0';
	com_gamedir[0] = '\0';
	COM_ParseDirectoryFromCmd( "-basedir", basedir, sizeof( basedir ), "valve" );
	COM_ParseDirectoryFromCmd( "-game", com_gamedir, sizeof( com_gamedir ), basedir );
	const char* gameDirectory = com_gamedir[0] ? com_gamedir : nullptr;
	if ( !FileSystem_SetGameDirectory( basedir, gameDirectory ) )
		return 0;

	Info_SetValueForStarKey( serverinfo, "*gamedir", com_gamedir, DIRECTORY_INFO_LIMIT );
	return 1;
}

/*
================
COM_InitArgv
================
*/
void COM_InitArgv( int argc, char** argv )
{
	int length = 0;
	for ( int argument = 0; argument < argc && argument < MAX_NUM_ARGVS && length < CMDLINE_LENGTH - 1; ++argument )
	{
		int sourceIndex = 0;
		while ( length < CMDLINE_LENGTH - 1 )
		{
			char character = argv[argument][sourceIndex];
			if ( !character )
			{
				com_cmdline[length++] = ' ';
				break;
			}
			++sourceIndex;
			com_cmdline[length++] = character;
		}
	}
	com_cmdline[length] = '\0';

	qboolean safe = 0;
	int argumentCount = 0;
	com_argc = 0;
	while ( argumentCount < argc && argumentCount < MAX_NUM_ARGVS )
	{
		largv[argumentCount] = argv[argumentCount];
		if ( Q_strcmp( "-safe", argv[argumentCount] ) == 0 )
			safe = 1;
		com_argc = ++argumentCount;
	}

	if ( safe )
	{
		for ( int index = 0; index < NUM_SAFE_ARGVS; ++index )
			largv[argumentCount + index] = const_cast<char*>( safeargvs[index] );
		argumentCount += NUM_SAFE_ARGVS;
		com_argc = argumentCount;
	}
	largv[argumentCount] = const_cast<char*>( argvdummy );
	com_argv = largv;
}

void Q_strcpy( char* destination, const char* source )
{
	if ( !destination )
		return;

	while ( source && *source )
	{
		*destination++ = *source++;
	}
	*destination = '\0';
}

void Q_strcat( char* destination, const char* source )
{
	if ( !destination )
		return;

	while ( *destination )
		++destination;

	Q_strcpy( destination, source );
}

void COM_StripExtension( const char* in, char* out )
{
	bool extensionChecked = false;

	for ( int index = Q_strlen( in ); index >= 0; --index )
	{
		char character = in[index];

		if ( !extensionChecked && character == '.' )
		{
			character = '\0';
			extensionChecked = true;
		}
		else if ( character == '/' || character == '\\' )
		{
			extensionChecked = true;
		}

		out[index] = character;
	}
}

char* COM_FileBase_s( const char* path, char* out, int maxBytes )
{
	if ( !path )
		return nullptr;
	int length = Q_strlen( path );
	if ( length <= 0 )
		return nullptr;

	int end = length - 1;
	int dot = end;
	while ( dot > 0 && path[dot] != '.' && path[dot] != '/' && path[dot] != '\\' )
		--dot;
	if ( path[dot] == '.' )
		end = dot - 1;

	int start = length - 1;
	while ( start >= 0 && path[start] != '/' && path[start] != '\\' )
		--start;
	++start;

	int baseLength = end - start + 1;
	if ( maxBytes >= 0 && baseLength >= maxBytes )
		return nullptr;
	Q_strncpy( out, path + start, baseLength );
	out[baseLength] = '\0';
	return out;
}

char* COM_FileBase( const char* path, char* out )
{
	constexpr int UNBOUNDED_FILE_BASE_SIZE = -1;
	return COM_FileBase_s( path, out, UNBOUNDED_FILE_BASE_SIZE );
}

char* COM_FileExtension( const char* path )
{
	constexpr int FILE_EXTENSION_BUFFER_SIZE = 8;
	constexpr int NO_EXTENSION_POSITION = -1;
	static char extension[FILE_EXTENSION_BUFFER_SIZE];
	if ( !path )
		return const_cast<char*>( com_empty_string );

	int lastDot = NO_EXTENSION_POSITION;
	for ( int index = 0; path[index]; ++index )
	{
		if ( path[index] == '.' )
			lastDot = index;
	}
	if ( lastDot < 0 )
		return const_cast<char*>( com_empty_string );

	const char* input = path + lastDot;
	while ( *input && *input != '.' )
		++input;
	if ( !*input )
		return const_cast<char*>( com_empty_string );
	++input;

	int length = 0;
	while ( length < FILE_EXTENSION_BUFFER_SIZE - 1 )
	{
		char character = *input;
		if ( !character )
			break;
		extension[length++] = character;
		++input;
	}
	extension[length] = '\0';
	return extension;
}

void COM_FixSlashes( char* filename )
{
	for ( ; *filename; ++filename )
	{
#if defined( _WIN32 )
		if ( *filename == '/' )
			*filename = '\\';
#else
		if ( *filename == '\\' )
			*filename = '/';
#endif
	}
}

static constexpr int CASE_INSENSITIVE_COMPARE_LIMIT = 99999;

#if defined( _WIN32 )
static int Q_CompareIgnoreCase( const char* first, const char* second, std::uint32_t count )
{
	while ( count != 0 )
	{
		--count;
		int firstCharacter = static_cast<signed char>( *first++ );
		int secondCharacter = static_cast<signed char>( *second++ );
		if ( firstCharacter != secondCharacter )
		{
			if ( firstCharacter >= 'a' && firstCharacter <= 'z' )
				firstCharacter -= 'a' - 'A';
			if ( secondCharacter >= 'a' && secondCharacter <= 'z' )
				secondCharacter -= 'a' - 'A';
			if ( firstCharacter != secondCharacter )
				return STRING_NOT_EQUAL;
		}
		if ( firstCharacter == '\0' )
			return 0;
	}
	return 0;
}
#endif

int Q_strncmp( const char* first, const char* second, int count )
{
	if ( !first || !second )
		return STRING_NOT_EQUAL;

	std::uint32_t remaining = static_cast<std::uint32_t>( count );
	while ( remaining != 0 )
	{
		char character = *first;
		--remaining;

		if ( character != *second )
			return STRING_NOT_EQUAL;
		if ( character == '\0' )
			return 0;               // strings are equal

		++first;
		++second;
	}

	return 0;
}

void Q_strncpy( char* destination, const char* source, int count )
{
	std::uint32_t remaining = static_cast<std::uint32_t>( count );
	const volatile char* input = source;
	while ( input )
	{
		char character = *input;
		if ( character == '\0' )
			break;
		if ( remaining-- == 0 )
			break;

		*destination++ = character;
		++input;
	}

	if ( remaining != 0 && remaining <= INT_MAX )
		*destination = '\0';
}

void Q_strncpy_s( char* destination, const char* source, int count )
{
	Q_strncpy( destination, source, count );
	std::uint32_t lastIndex = static_cast<std::uint32_t>( count ) - 1u;
	destination[static_cast<int>( lastIndex )] = '\0';
}

int Q_strnicmp( const char* first, const char* second, int count )
{
	if ( !first )
		return STRING_NOT_EQUAL;
	if ( !second )
		return 1;

#if defined( _WIN32 )
	return Q_CompareIgnoreCase( first, second, static_cast<std::uint32_t>( count ) );
#else
	return _strnicmp( first, second, count );
#endif
}

int Q_strcasecmp( const char* first, const char* second )
{
	if ( !first )
		return STRING_NOT_EQUAL;
	if ( !second )
		return 1;

#if defined( _WIN32 )
	return Q_CompareIgnoreCase( first, second, CASE_INSENSITIVE_COMPARE_LIMIT );
#else
	return _strnicmp( first, second, CASE_INSENSITIVE_COMPARE_LIMIT );
#endif
}

char* Q_strrchr( char* text, char character )
{
	if ( !text )
		return nullptr;

	char* end = text;
	while ( *end )
		++end;

	while ( end != text )
	{
		--end;
		if ( *end == character )
			return end;
	}
	return nullptr;
}

char* Q_strstr( const char* text, const char* substring )
{
	return const_cast<char*>( std::strstr( text, substring ) );
}

char* va( const char* format, ... )
{
	constexpr int VA_BUFFER_COUNT = 16;
	constexpr int VA_FORMAT_SIZE = 1024;
	static char string[VA_BUFFER_COUNT][VA_FORMAT_SIZE];
	static int current;

	va_list arguments;
	va_start( arguments, format );
	current = ( current + 1 ) & ( VA_BUFFER_COUNT - 1 );
	_vsnprintf( string[current], VA_FORMAT_SIZE, format, arguments );
	string[current][VA_FORMAT_SIZE - 1] = 0;
	va_end( arguments );
	return string[current];
}

char* vstr( vec_t* vector )
{
	constexpr int VECTOR_STRING_COUNT = 16;
	constexpr int VECTOR_STRING_BYTES = 1024;
	static char string[VECTOR_STRING_COUNT][VECTOR_STRING_BYTES];
	static int current;

	double z = vector[2];
	current = ( current + 1 ) & ( VECTOR_STRING_COUNT - 1 );
	double y = vector[1];
	double x = vector[0];
	_snprintf( string[current], VECTOR_STRING_BYTES, "%.4f %.4f %.4f", x, y, z );
	return string[current];
}

int Q_atoi( const char* text )
{
	constexpr std::uint32_t DECIMAL_BASE = 10;
	constexpr std::uint32_t HEXADECIMAL_BASE = 16;
	constexpr int HEX_LETTER_DIGIT_START = 10;
	constexpr int HEXADECIMAL_PREFIX_LENGTH = 2;

	int sign = 1;
	if ( *text == '-' )
	{
		sign = -1;
		++text;
	}

	std::uint32_t value = 0;
//
// check for hex
//
	if ( text[0] == '0' && ( text[1] == 'x' || text[1] == 'X' ) )
	{
		text += HEXADECIMAL_PREFIX_LENGTH;
		for ( ;; )
		{
			int character = static_cast<signed char>( *text++ );
			int digit;
			if ( character >= '0' && character <= '9' )
				digit = character - '0';
			else if ( character >= 'a' && character <= 'f' )
				digit = character - 'a' + HEX_LETTER_DIGIT_START;
			else if ( character >= 'A' && character <= 'F' )
				digit = character - 'A' + HEX_LETTER_DIGIT_START;
			else
				break;

			value = value * HEXADECIMAL_BASE + digit;
		}
	}
	else if ( *text == '\'' )
	{
		return sign * static_cast<signed char>( text[1] );
	}
	else
	{
		for ( int character = static_cast<signed char>( *text ); character >= '0' && character <= '9';
			character = static_cast<signed char>( *++text ) )
		{
			value = value * DECIMAL_BASE + character - '0';
		}
	}

	if ( sign < 0 )
		value = 0u - value;
	return static_cast<int>( value );
}

float Q_atof( const char* text )
{
	static constexpr int NO_DECIMAL_POINT = -1;
	static constexpr int HEXADECIMAL_PREFIX_LENGTH = 2;
	static constexpr double DECIMAL_BASE = 10.0;
	static constexpr double HEXADECIMAL_BASE = 16.0;
	static constexpr double HEX_LETTER_DIGIT_START = 10.0;

	int sign = 1;
	int character = static_cast<signed char>( *text );
	if ( character == '-' )
	{
		sign = -1;
		++text;
		character = static_cast<signed char>( *text );
	}

	double value = 0.0;
	if ( character == '0' )
	{
		char prefix = text[1];
		if ( prefix == 'x' || prefix == 'X' )
		{
			text += HEXADECIMAL_PREFIX_LENGTH;
			while ( true )
			{
				character = static_cast<signed char>( *text++ );
				if ( character >= '0' && character <= '9' )
				{
					value *= HEXADECIMAL_BASE;
					value += character;
					value -= '0';
				}
				else if ( character >= 'a' && character <= 'f' )
				{
					value *= HEXADECIMAL_BASE;
					value += character;
					value -= 'a';
					value += HEX_LETTER_DIGIT_START;
				}
				else if ( character >= 'A' && character <= 'F' )
				{
					value *= HEXADECIMAL_BASE;
					value += character;
					value -= 'A';
					value += HEX_LETTER_DIGIT_START;
				}
				else
				{
					return static_cast<float>( sign * value );
				}
			}
		}
	}
	else if ( character == '\'' )
	{
		int characterValue = static_cast<signed char>( text[1] );
		return static_cast<float>( sign * characterValue );
	}

	int totalDigits = 0;
	int decimalPoint = NO_DECIMAL_POINT;
	while ( true )
	{
		int currentCharacter = character;
		++text;
		if ( currentCharacter == '.' || currentCharacter == ',' )
		{
			character = static_cast<signed char>( *text );
			decimalPoint = totalDigits;
			continue;
		}
		if ( currentCharacter < '0' || currentCharacter > '9' )
			break;

		character = static_cast<signed char>( *text );
		++totalDigits;
		value *= DECIMAL_BASE;
		value += currentCharacter;
		value -= '0';
	}

	if ( decimalPoint != NO_DECIMAL_POINT )
	{
		for ( int remaining = totalDigits - decimalPoint; remaining > 0; --remaining )
			value /= DECIMAL_BASE;
	}

	return static_cast<float>( sign * value );
}

void COM_UngetToken()
{
	s_com_token_unget = 1;
}

const char* COM_Parse( const char* data )
{
	if ( s_com_token_unget )
	{
		s_com_token_unget = 0;
		return data;
	}

	com_token[0] = '\0';
	if ( !data )
		return nullptr;

	int character;
	for ( ;; )
	{
		for ( ;; )
		{
			uchar32 codepoint;
			qboolean conversionFailed = V_UTF8ToUChar32( data, &codepoint );
			if ( codepoint > ' ' || conversionFailed )
				break;
			if ( !codepoint )
				return nullptr;
			data = Q_UnicodeAdvance( data, 1 );
		}

		character = static_cast<signed char>( *data );
		if ( character != '/' || data[1] != '/' )
			break;
		do
		{
			++data;
			character = static_cast<signed char>( *data );
		} while ( character && character != '\n' );
	}

	int length = 0;
	if ( character == '"' )
	{
		++data;
		character = static_cast<signed char>( *data++ );
		while ( character && character != '"' && length < COM_TOKEN_SIZE - 1 )
		{
			com_token[length++] = static_cast<char>( character );
			character = static_cast<signed char>( *data++ );
		}
	}
	else
	{
		qboolean ignoreColons = com_ignorecolons;
		if ( character == '{' || character == '}' || character == ')' || character == '(' || character == '\''
			 || ( character == ':' && !ignoreColons ) || character == ',' )
		{
			com_token[0] = static_cast<char>( character );
			com_token[1] = '\0';
			return data + 1;
		}

		do
		{
			++data;
			com_token[length++] = static_cast<char>( character );
			character = static_cast<signed char>( *data );
		} while ( character != '{' && character != '}' && character != ')' && character != '(' && character != '\''
				  && ( ignoreColons || character != ':' ) && character != ',' && length < COM_TOKEN_SIZE - 1
				  && character != '\0' && ( character < 0 || character > ' ' ) );
	}

	com_token[length] = '\0';
	return data;
}

const char* COM_ParseLine( const char* data )
{
	if ( s_com_token_unget )
	{
		s_com_token_unget = 0;
		return data;
	}

	com_token[0] = '\0';
	if ( !data )
		return nullptr;

	int length = 0;
	int character = static_cast<signed char>( *data );
	do
	{
		++data;
		com_token[length++] = static_cast<char>( character );
		character = static_cast<signed char>( *data );
	} while ( character >= ' ' && length < COM_TOKEN_SIZE - 1 );

	com_token[length] = '\0';
	if ( !character )
		return nullptr;

	character = static_cast<signed char>( *data );
	while ( character < ' ' )
	{
		if ( !character )
			return nullptr;
		character = static_cast<signed char>( *++data );
	}
	return data;
}

static constexpr int MEMORY_WORD_BYTES = sizeof( std::uint32_t );
static constexpr int MEMORY_WORD_SHIFT = 2;
static constexpr std::uintptr_t MEMORY_WORD_ALIGNMENT_MASK = MEMORY_WORD_BYTES - 1;

int Q_memcmp( const void* first, const void* second, int count )
{
	const unsigned char* firstBytes = static_cast<const unsigned char*>( first );
	const unsigned char* secondBytes = static_cast<const unsigned char*>( second );
	int wordCount = count >> MEMORY_WORD_SHIFT;
	for ( int index = 0; index < wordCount; ++index )
	{
		std::uint32_t firstWord;
		std::uint32_t secondWord;
		int offset = index * MEMORY_WORD_BYTES;
		std::memcpy( &firstWord, firstBytes + offset, sizeof( firstWord ) );
		std::memcpy( &secondWord, secondBytes + offset, sizeof( secondWord ) );
		if ( firstWord != secondWord )
			return STRING_NOT_EQUAL;
	}

	int remainder = count & ( MEMORY_WORD_BYTES - 1 );
	for ( int index = 0; index < remainder; ++index )
	{
		if ( firstBytes[index] != secondBytes[index] )
			return STRING_NOT_EQUAL;
	}
	return 0;
}

void Q_memcpy( void* destination, const void* source, int count )
{
	if ( count <= 0 )
		return;

	unsigned char* out = static_cast<unsigned char*>( destination );
	const unsigned char* in = static_cast<const unsigned char*>( source );
	std::uintptr_t alignment = reinterpret_cast<std::uintptr_t>( destination );
	alignment |= reinterpret_cast<std::uintptr_t>( source );
	alignment |= static_cast<std::uintptr_t>( count );

	if ( ( alignment & MEMORY_WORD_ALIGNMENT_MASK ) == 0 )
	{
		for ( int offset = 0; offset < count; offset += MEMORY_WORD_BYTES )
		{
			std::uint32_t word;
			std::memcpy( &word, in + offset, sizeof( word ) );
			std::memcpy( out + offset, &word, sizeof( word ) );
		}
		return;
	}

	for ( int offset = 0; offset < count; ++offset )
		out[offset] = in[offset];
}

void Q_memset( void* destination, int fill, int count )
{
	if ( count <= 0 )
		return;

	unsigned char* out = static_cast<unsigned char*>( destination );
	std::uintptr_t alignment = reinterpret_cast<std::uintptr_t>( destination );
	alignment |= static_cast<std::uintptr_t>( count );

	if ( ( alignment & MEMORY_WORD_ALIGNMENT_MASK ) == 0 )
	{
		const std::uint32_t originalValue = static_cast<std::uint32_t>( fill );
		std::uint32_t word = originalValue;

		for ( int byteIndex = 1; byteIndex < MEMORY_WORD_BYTES; ++byteIndex )
			word = ( word << CHAR_BIT ) | originalValue;

		for ( int offset = 0; offset < count; offset += MEMORY_WORD_BYTES )
			std::memcpy( out + offset, &word, sizeof( word ) );
		return;
	}

	const unsigned char byteValue = static_cast<unsigned char>( fill );
	for ( int offset = 0; offset < count; ++offset )
		out[offset] = byteValue;
}

static constexpr int MIN_SIZEBUF_ALLOCATION = 256;

void SZ_Alloc( const char* name, sizebuf_t* buffer, int startSize )
{
	if ( startSize < MIN_SIZEBUF_ALLOCATION )
		startSize = MIN_SIZEBUF_ALLOCATION;

	buffer->buffername = name;
	buffer->data = static_cast<byte*>( Hunk_AllocName( startSize, name ) );
	buffer->maxsize = startSize;
	buffer->cursize = 0;
	buffer->flags = 0;
}

void SZ_Clear( sizebuf_t* buffer )
{
	buffer->flags &= static_cast<std::uint16_t>( ~FSB_OVERFLOWED );
	buffer->cursize = 0;
}

void* SZ_GetSpace( sizebuf_t* buffer, int length )
{
	int previousSize = buffer->cursize;
	std::uint32_t totalSize = static_cast<std::uint32_t>( previousSize );
	totalSize += static_cast<std::uint32_t>( length );

	if ( static_cast<int>( totalSize ) > buffer->maxsize )
	{
		if ( !( buffer->flags & FSB_ALLOWOVERFLOW ) )
		{
			if ( !buffer->maxsize )
				Sys_Error( "SZ_GetSpace:  Tried to write to an uninitialized sizebuf_t: %s",
					buffer->buffername ? buffer->buffername : "???" );
			Sys_Error( "SZ_GetSpace: overflow without FSB_ALLOWOVERFLOW set on %s",
				buffer->buffername ? buffer->buffername : "???" );
		}

		if ( length > buffer->maxsize )
		{
			Con_DPrintf( "SZ_GetSpace: %i is > full buffer size on %s, ignoring", length,
				buffer->buffername ? buffer->buffername : "???" );
		}

		Con_Printf( "SZ_GetSpace: overflow on %s\n", buffer->buffername ? buffer->buffername : "???" );
		buffer->flags |= FSB_OVERFLOWED;
		previousSize = 0;
	}

	std::uint32_t newSize = static_cast<std::uint32_t>( previousSize );
	newSize += static_cast<std::uint32_t>( length );
	buffer->cursize = static_cast<int>( newSize );
	return buffer->data + previousSize;
}

void SZ_Write( sizebuf_t* buffer, const void* data, int length )
{
	void* destination = SZ_GetSpace( buffer, length );
	if ( !( buffer->flags & FSB_OVERFLOWED ) )
		Q_memcpy( destination, data, length );
}

void SZ_Print( sizebuf_t* buffer, const char* text )
{
	int length = Q_strlen( text );
	std::uint32_t copyLength = static_cast<std::uint32_t>( length ) + 1u;
	byte* destination;
	if ( !buffer->cursize || buffer->data[buffer->cursize - 1] )
		destination = static_cast<byte*>( SZ_GetSpace( buffer, static_cast<int>( copyLength ) ) );
	else
		destination = static_cast<byte*>( SZ_GetSpace( buffer, length ) ) - 1;

	if ( !( buffer->flags & FSB_OVERFLOWED ) )
		Q_memcpy( destination, text, static_cast<int>( copyLength ) );
}

int LongSwap( int value )
{
	constexpr int LONG_BYTES = sizeof( value );
	std::uint32_t bits = static_cast<std::uint32_t>( value );
	std::uint32_t swappedValue = 0;
	for ( int index = 0; index < LONG_BYTES; ++index )
	{
		swappedValue = ( swappedValue << CHAR_BIT ) | static_cast<byte>( bits );
		bits >>= CHAR_BIT;
	}
	return static_cast<int>( swappedValue );
}

float FloatSwap( float value )
{
	int bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	bits = LongSwap( bits );
	float swappedValue;
	std::memcpy( &swappedValue, &bits, sizeof( swappedValue ) );
	return swappedValue;
}

float FloatNoSwap( float value )
{
	return value;
}

void MSG_WriteByte( sizebuf_t* buffer, int value )
{
	byte* destination = static_cast<byte*>( SZ_GetSpace( buffer, 1 ) );
	*destination = static_cast<byte>( value );
}

void MSG_WriteShort( sizebuf_t* buffer, int value )
{
	byte* destination = static_cast<byte*>( SZ_GetSpace( buffer, sizeof( std::uint16_t ) ) );
	std::uint16_t bits = static_cast<std::uint16_t>( value );
	destination[0] = static_cast<byte>( bits );
	destination[1] = static_cast<byte>( bits >> CHAR_BIT );
}

void MSG_WriteLong( sizebuf_t* buffer, int value )
{
	byte* destination = static_cast<byte*>( SZ_GetSpace( buffer, sizeof( std::uint32_t ) ) );
	std::uint32_t bits = static_cast<std::uint32_t>( value );
	destination[1] = static_cast<byte>( bits >> CHAR_BIT );
	destination[0] = static_cast<byte>( bits );
	destination[2] = static_cast<byte>( bits >> ( CHAR_BIT * 2 ) );
	destination[3] = static_cast<byte>( bits >> ( CHAR_BIT * 3 ) );
}

void MSG_WriteFloat( sizebuf_t* buffer, float value )
{
	int bits;
	std::memcpy( &bits, &value, sizeof( bits ) );
	int encodedValue = LittleLong( bits );
	void* destination = SZ_GetSpace( buffer, sizeof( encodedValue ) );
	if ( !( buffer->flags & FSB_OVERFLOWED ) )
		std::memcpy( destination, &encodedValue, sizeof( encodedValue ) );
}

void MSG_WriteBuf( sizebuf_t* buffer, int length, const void* data )
{
	if ( data )
		SZ_Write( buffer, data, length );
}

void MSG_WriteCoord( sizebuf_t* buffer, float value )
{
	constexpr float COORDINATE_SCALE = 8.0f;
	float scaledCoordinate = value * COORDINATE_SCALE;
	int packedCoordinate = static_cast<int>( scaledCoordinate );
	MSG_WriteShort( buffer, packedCoordinate );
}

void MSG_WriteAngle( sizebuf_t* buffer, float value )
{
	constexpr double DEGREES_PER_TURN = 360.0;
	constexpr float ANGLE_STEPS_PER_TURN = 256.0f;
	byte* destination = static_cast<byte*>( SZ_GetSpace( buffer, 1 ) );
	float wrappedAngle = static_cast<float>( std::fmod( static_cast<double>( value ), DEGREES_PER_TURN ) );
	float scaledAngle = wrappedAngle * ANGLE_STEPS_PER_TURN;
	float angleSteps = scaledAngle / static_cast<float>( DEGREES_PER_TURN );
	int packedAngle = static_cast<int>( angleSteps );
	*destination = static_cast<byte>( packedAngle );
}

void MSG_WriteHiresAngle( sizebuf_t* buffer, float value )
{
	constexpr double DEGREES_PER_TURN = 360.0;
	constexpr float ANGLE_STEPS_PER_TURN = 65536.0f;
	float wrappedAngle = static_cast<float>( std::fmod( static_cast<double>( value ), DEGREES_PER_TURN ) );
	float scaledAngle = wrappedAngle * ANGLE_STEPS_PER_TURN;
	float angleSteps = scaledAngle / static_cast<float>( DEGREES_PER_TURN );
	int packedAngle = static_cast<int>( angleSteps );
	MSG_WriteShort( buffer, packedAngle );
}

void MSG_WriteString( sizebuf_t* buffer, const char* text )
{
	std::uint32_t length = 1;
	if ( text )
		length += static_cast<std::uint32_t>( Q_strlen( text ) );

	byte* destination = static_cast<byte*>( SZ_GetSpace( buffer, static_cast<int>( length ) ) );
	if ( buffer->flags & FSB_OVERFLOWED )
		return;

	if ( text )
		Q_memcpy( destination, text, static_cast<int>( length ) );
	else
		*destination = '\0';
}

static constexpr int MESSAGE_READ_ERROR = -1;
static constexpr int MESSAGE_STRING_BUFFER_SIZE = 8192;

int MSG_ReadByte()
{
	int previousCount = msg_readcount;
	std::uint32_t count = static_cast<std::uint32_t>( previousCount ) + 1;
	int nextCount = static_cast<int>( count );
	if ( nextCount > net_message.cursize )
	{
		msg_badread = 1;
		return MESSAGE_READ_ERROR;
	}

	int value = net_message.data[previousCount];
	msg_readcount = nextCount;
	return value;
}

int MSG_ReadChar()
{
	int previousCount = msg_readcount;
	std::uint32_t count = static_cast<std::uint32_t>( previousCount ) + 1;
	int nextCount = static_cast<int>( count );
	if ( nextCount > net_message.cursize )
	{
		msg_badread = 1;
		return MESSAGE_READ_ERROR;
	}

	int value = static_cast<signed char>( net_message.data[previousCount] );
	msg_readcount = nextCount;
	return value;
}

int MSG_ReadShort()
{
	int previousCount = msg_readcount;
	std::uint32_t count = static_cast<std::uint32_t>( previousCount ) + sizeof( std::int16_t );
	int nextCount = static_cast<int>( count );
	if ( nextCount > net_message.cursize )
	{
		msg_badread = 1;
		return MESSAGE_READ_ERROR;
	}

	const byte* data = net_message.data + previousCount;
	std::uint16_t highByte = data[1];
	std::uint16_t lowByte = data[0];
	msg_readcount = nextCount;
	return static_cast<std::int16_t>( ( highByte << CHAR_BIT ) | lowByte );
}

int MSG_ReadLong()
{
	int previousCount = msg_readcount;
	std::uint32_t count = static_cast<std::uint32_t>( previousCount ) + sizeof( std::int32_t );
	int nextCount = static_cast<int>( count );
	if ( nextCount > net_message.cursize )
	{
		msg_badread = 1;
		return MESSAGE_READ_ERROR;
	}

	const byte* data = net_message.data + previousCount;
	std::uint32_t highByte = data[3];
	std::uint32_t upperByte = data[2];
	std::uint32_t lowerByte = data[1];
	std::uint32_t lowByte = data[0];
	std::uint32_t value = ( highByte << CHAR_BIT ) | upperByte;
	value = ( value << CHAR_BIT ) | lowerByte;
	msg_readcount = nextCount;
	return static_cast<int>( ( value << CHAR_BIT ) | lowByte );
}

float MSG_ReadFloat()
{
	int previousCount = msg_readcount;
	const byte* data = net_message.data + previousCount;
	std::uint32_t lowByte = data[0];
	std::uint32_t lowerByte = data[1];
	std::uint32_t upperByte = data[2];
	std::uint32_t highByte = data[3];
	std::uint32_t bits =
		lowByte | ( lowerByte << CHAR_BIT ) | ( upperByte << ( CHAR_BIT * 2 ) ) | ( highByte << ( CHAR_BIT * 3 ) );
	std::uint32_t count = static_cast<std::uint32_t>( previousCount ) + sizeof( std::int32_t );
	msg_readcount = static_cast<int>( count );
	int decodedBits = LittleLong( static_cast<int>( bits ) );
	float value;
	std::memcpy( &value, &decodedBits, sizeof( value ) );
	return value;
}

float MSG_ReadHiresAngle()
{
	constexpr double DEGREES_PER_TURN = 360.0;
	constexpr double ANGLE_STEPS_PER_TURN = 65536.0;
	int packedAngle = MSG_ReadShort();
	if ( msg_badread )
		return 0.0f;

	return static_cast<float>( static_cast<double>( packedAngle ) * ( DEGREES_PER_TURN / ANGLE_STEPS_PER_TURN ) );
}

float MSG_ReadCoord( sizebuf_t* )
{
	constexpr double COORDINATE_STEPS_PER_UNIT = 8.0;
	int packedCoordinate = MSG_ReadShort();
	if ( msg_badread )
		return 0.0f;

	return static_cast<float>( static_cast<double>( packedCoordinate ) * ( 1.0 / COORDINATE_STEPS_PER_UNIT ) );
}

int MSG_ReadBuf( int size, void* buffer )
{
	int previousCount = msg_readcount;
	std::uint32_t count = static_cast<std::uint32_t>( previousCount ) + static_cast<std::uint32_t>( size );
	if ( static_cast<int>( count ) > net_message.cursize )
	{
		msg_badread = 1;
		return MESSAGE_READ_ERROR;
	}

	Q_memcpy( buffer, net_message.data + previousCount, size );
	count = static_cast<std::uint32_t>( msg_readcount ) + static_cast<std::uint32_t>( size );
	msg_readcount = static_cast<int>( count );
	return 1;
}

char* MSG_ReadString()
{
	static char string[MESSAGE_STRING_BUFFER_SIZE];
	int readCount = msg_readcount;
	int messageSize = net_message.cursize;
	int length = 0;

	while ( length < MESSAGE_STRING_BUFFER_SIZE - 1 )
	{
		std::uint32_t count = static_cast<std::uint32_t>( readCount ) + 1;
		int nextCount = static_cast<int>( count );
		if ( nextCount > messageSize )
		{
			msg_badread = 1;
			break;
		}

		byte character = net_message.data[readCount];
		readCount = nextCount;
		msg_readcount = nextCount;
		if ( character == UCHAR_MAX || character == '\0' )
			break;

		string[length++] = static_cast<char>( character );
	}

	string[length] = '\0';
	return string;
}

void MSG_BeginReading()
{
	msg_readcount = 0;
	msg_badread = 0;
}

int MSG_CurrentBit()
{
	std::uint32_t bitCount;
	if ( bfread.pbuf )
	{
		bitCount = static_cast<std::uint32_t>( bfread.nBytesRead ) * CHAR_BIT;
		bitCount += static_cast<std::uint32_t>( bfread.nCurInputBit );
	}
	else
		bitCount = static_cast<std::uint32_t>( msg_readcount ) * CHAR_BIT;
	return static_cast<int>( bitCount );
}

void MSG_EndBitReading( sizebuf_t* buffer )
{
	int readCount = bfread.msg_readcount;
	qboolean badRead = msg_badread;
	if ( readCount > buffer->cursize )
		badRead = 1;
	msg_readcount = readCount;
	bfread.nBitFieldReadStartByte = 0;
	msg_badread = badRead;
	bfread.nCurInputBit = 0;
	bfread.nBytesRead = 0;
	bfread.pInByte = nullptr;
	bfread.pbuf = nullptr;
}

qboolean MSG_IsBitReading()
{
	return bfread.pbuf != NULL;
}

void MSG_StartBitReading( sizebuf_t* buffer )
{
	int readCount = msg_readcount;
	bfread.msg_readcount = readCount;
	bfread.nBitFieldReadStartByte = readCount;
	bfread.nCurInputBit = 0;
	bfread.nBytesRead = 0;
	byte* input = buffer->data + readCount;
	bfread.pbuf = buffer;
	std::uint32_t nextCount = static_cast<std::uint32_t>( readCount ) + 1;
	int nextReadCount = static_cast<int>( nextCount );
	bfread.pInByte = input;
	qboolean badRead = msg_badread;
	bfread.msg_readcount = nextReadCount;
	if ( nextReadCount > buffer->cursize )
		badRead = 1;
	msg_badread = badRead;
}

int MSG_ReadOneBit()
{
	if ( msg_badread )
		return 1;

	int bit = bfread.nCurInputBit;
	int readCount = bfread.msg_readcount;
	byte* input = bfread.pInByte;
	if ( bit >= CHAR_BIT )
	{
		std::uint32_t bytesRead = static_cast<std::uint32_t>( bfread.nBytesRead ) + 1;
		bfread.nBytesRead = static_cast<int>( bytesRead );
		std::uint32_t count = static_cast<std::uint32_t>( readCount ) + 1;
		readCount = static_cast<int>( count );
		bfread.msg_readcount = readCount;
		++input;
		bit = 0;
		bfread.nCurInputBit = 0;
		bfread.pInByte = input;
	}

	if ( readCount > bfread.pbuf->cursize )
	{
		msg_badread = 1;
		return 1;
	}

	byte value = *input;
	bfread.nCurInputBit = bit + 1;
	return ( value & BITTABLE[bit] ) != 0;
}

std::uint32_t MSG_ReadBits( int numbits )
{
	if ( msg_badread )
		return 1;

	byte* input = bfread.pInByte;
	int bit = bfread.nCurInputBit;
	std::uint32_t readCount = static_cast<std::uint32_t>( bfread.msg_readcount );
	std::uint32_t bytesRead = static_cast<std::uint32_t>( bfread.nBytesRead );
	if ( bit >= CHAR_BIT )
	{
		++readCount;
		++bytesRead;
		bfread.msg_readcount = static_cast<int>( readCount );
		++input;
		bit = 0;
		bfread.nCurInputBit = 0;
		bfread.nBytesRead = static_cast<int>( bytesRead );
		bfread.pInByte = input;
	}

	std::uint32_t totalBits = static_cast<std::uint32_t>( numbits ) + static_cast<std::uint32_t>( bit );
	int remainingBits = totalBits & ( CHAR_BIT - 1 );
	std::uint32_t word;
	std::memcpy( &word, input, sizeof( word ) );
	std::uint32_t value = word >> bit;
	if ( totalBits > BITS_PER_WORD )
	{
		input += sizeof( word );
		bfread.pInByte = input;
		readCount += sizeof( word );
		std::uint32_t nextWord;
		std::memcpy( &nextWord, input, sizeof( nextWord ) );
		value |= ( nextWord & ROWBITTABLE[remainingBits] ) << ( BITS_PER_WORD - bit );
		bytesRead += sizeof( word );
		bfread.nCurInputBit = remainingBits;
	}
	else
	{
		value &= ROWBITTABLE[numbits];
		std::uint32_t wholeBytes = totalBits / CHAR_BIT;
		if ( remainingBits == 0 )
		{
			--wholeBytes;
			remainingBits = CHAR_BIT;
		}
		bfread.nCurInputBit = remainingBits;
		input += static_cast<int>( wholeBytes );
		readCount += wholeBytes;
		bfread.pInByte = input;
		bytesRead += wholeBytes;
	}

	sizebuf_t* buffer = bfread.pbuf;
	bfread.nBytesRead = static_cast<int>( bytesRead );
	bfread.msg_readcount = static_cast<int>( readCount );
	if ( static_cast<int>( readCount ) > buffer->cursize )
	{
		msg_badread = 1;
		return 1;
	}
	return value;
}

int MSG_ReadSBits( int numBits )
{
	int negative = MSG_ReadOneBit();
	std::uint32_t magnitude = MSG_ReadBits( numBits - 1 );
	return static_cast<int>( negative ? 0u - magnitude : magnitude );
}

std::uint32_t MSG_PeekBits( int numBits )
{
	bf_read_t saved = bfread;
	std::uint32_t value = MSG_ReadBits( numBits );
	bfread = saved;
	return value;
}

int MSG_ReadBitData( void* destination, int length )
{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitReading(), 1370 );
#endif
	byte* output = static_cast<byte*>( destination );
	for ( int index = 0; index < length; ++index )
		output[index] = static_cast<byte>( MSG_ReadBits( CHAR_BIT ) );
	return length;
}

float MSG_ReadAngle()
{
	constexpr double DEGREES_PER_TURN = 360.0;
	constexpr double ANGLE_STEPS_PER_TURN = 256.0;
	int previousCount = msg_readcount;
	std::uint32_t count = static_cast<std::uint32_t>( previousCount ) + 1;
	int nextCount = static_cast<int>( count );
	if ( nextCount > net_message.cursize )
	{
		msg_badread = 1;
		return 0.0f;
	}

	qboolean badRead = msg_badread;
	int packedAngle = static_cast<signed char>( net_message.data[previousCount] );
	msg_readcount = nextCount;
	if ( badRead )
		return 0.0f;

	return static_cast<float>( static_cast<double>( packedAngle ) * ( DEGREES_PER_TURN / ANGLE_STEPS_PER_TURN ) );
}

float MSG_ReadBitAngle( int numbits )
{
	constexpr double DEGREES_PER_TURN = 360.0;
	std::uint32_t packedAngle = MSG_ReadBits( numbits );
	float angleSteps = static_cast<float>( packedAngle );
	unsigned int shift = static_cast<unsigned int>( numbits ) & ( BITS_PER_WORD - 1 );
	std::uint32_t angleStepBits = 1u << shift;
	float stepsPerTurn = static_cast<float>( static_cast<int>( angleStepBits ) );
	double degreesPerStep = DEGREES_PER_TURN / static_cast<double>( stepsPerTurn );
	return static_cast<float>( static_cast<double>( angleSteps ) * degreesPerStep );
}

float MSG_ReadBitCoord()
{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitReading(), 1388 );
#endif
	constexpr int COORDINATE_INTEGER_BITS = 12;
	constexpr int COORDINATE_FRACTION_BITS = 3;
	constexpr double COORDINATE_FRACTION_SCALE = 0.125;
	int integer = MSG_ReadOneBit();
	int fraction = MSG_ReadOneBit();
	if ( !integer && !fraction )
		return 0.0f;
	int negative = MSG_ReadOneBit();
	if ( integer )
	{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
		integer = static_cast<int>( MSG_ReadBits( COORDINATE_INTEGER_BITS ) );
#else
		integer = static_cast<int>( MSG_ReadBits_0( COORDINATE_INTEGER_BITS ) );
#endif
	}
	if ( fraction )
	{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
		fraction = static_cast<int>( MSG_ReadBits( COORDINATE_FRACTION_BITS ) );
#else
		fraction = static_cast<int>( MSG_ReadBits_0( COORDINATE_FRACTION_BITS ) );
#endif
	}
	float fractionalValue = static_cast<float>( fraction );
	double scaledFraction = static_cast<double>( fractionalValue ) * COORDINATE_FRACTION_SCALE;
	float coordinate = static_cast<float>( scaledFraction + static_cast<double>( integer ) );
	return negative ? -coordinate : coordinate;
}

void MSG_ReadBitVec3Coord( vec_t* coordinates )
{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitReading(), 1432 );
#endif
	int x = MSG_ReadOneBit();
	int y = MSG_ReadOneBit();
	int z = MSG_ReadOneBit();
	if ( x )
		coordinates[0] = MSG_ReadBitCoord();
	if ( y )
		coordinates[1] = MSG_ReadBitCoord();
	if ( z )
		coordinates[2] = MSG_ReadBitCoord();
}

char* MSG_ReadBitString()
{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitReading(), 1350 );
#endif
	constexpr int BIT_STRING_BUFFER_SIZE = 8192;
	static char string[BIT_STRING_BUFFER_SIZE];
	qboolean badRead = msg_badread;
	string[0] = 0;
	char* output = string;
	char* outputEnd = string + BIT_STRING_BUFFER_SIZE - 1;
	while ( !badRead )
	{
		char character = static_cast<char>( MSG_ReadBits( CHAR_BIT ) );
		badRead = msg_badread;
		if ( badRead || !character )
			break;
		if ( output < outputEnd )
			*output++ = character;
	}
	*output = 0;
	return string;
}

void MSG_StartBitWriting( sizebuf_t* buffer )
{
	bfwrite.nCurOutputBit = 0;
	bfwrite.pOutByte = buffer->data + buffer->cursize;
	bfwrite.pbuf = buffer;
}

qboolean MSG_IsBitWriting()
{
	return bfwrite.pbuf != NULL;
}

void MSG_WriteOneBit( int value )
{
	int bit = bfwrite.nCurOutputBit;
	if ( bit >= CHAR_BIT )
	{
		SZ_GetSpace( bfwrite.pbuf, 1 );
		bit = 0;
		bfwrite.nCurOutputBit = 0;
		++bfwrite.pOutByte;
	}
	byte* output = bfwrite.pOutByte;
	if ( !( bfwrite.pbuf->flags & FSB_OVERFLOWED ) )
	{
		if ( value )
			*output |= static_cast<byte>( BITTABLE[bit] );
		else
			*output &= static_cast<byte>( INVBITTABLE[bit] );
		++bfwrite.nCurOutputBit;
	}
}

static void MergeBitOutputWord( std::uint32_t data )
{
	std::uint32_t mask = ROWBITTABLE[bfwrite.nCurOutputBit];
	byte* output = bfwrite.pOutByte;
	std::uint32_t word;
	std::memcpy( &word, output, sizeof( word ) );
	word &= mask;
	std::memcpy( output, &word, sizeof( word ) );
	output = bfwrite.pOutByte;
	std::uint32_t shifted = data << bfwrite.nCurOutputBit;
	std::memcpy( &word, output, sizeof( word ) );
	word |= shifted;
	std::memcpy( output, &word, sizeof( word ) );
}

void MSG_WriteBits( std::uint32_t data, int numBits )
{
	if ( numBits < static_cast<int>( BITS_PER_WORD ) && data >= ( 1u << numBits ) )
		data = ROWBITTABLE[numBits];
	int advancedBytes = 0;
	if ( bfwrite.nCurOutputBit >= CHAR_BIT )
	{
		++bfwrite.pOutByte;
		bfwrite.nCurOutputBit = 0;
		advancedBytes = 1;
	}
	std::uint32_t totalBits =
		static_cast<std::uint32_t>( bfwrite.nCurOutputBit ) + static_cast<std::uint32_t>( numBits );
	int remainingBits = totalBits & ( CHAR_BIT - 1 );
	if ( totalBits > BITS_PER_WORD )
	{
		SZ_GetSpace( bfwrite.pbuf, advancedBytes + static_cast<int>( sizeof( std::uint32_t ) ) );
		if ( bfwrite.pbuf->flags & FSB_OVERFLOWED )
			return;
		MergeBitOutputWord( data );
		unsigned int spillShift = BITS_PER_WORD - bfwrite.nCurOutputBit;
		bfwrite.nCurOutputBit = remainingBits;
		bfwrite.pOutByte += sizeof( std::uint32_t );
		std::uint32_t spill = data >> spillShift;
		std::uint32_t word = 0;
		std::memcpy( bfwrite.pOutByte, &word, sizeof( word ) );
		byte* output = bfwrite.pOutByte;
		std::memcpy( &word, output, sizeof( word ) );
		word |= spill;
		std::memcpy( output, &word, sizeof( word ) );
	}
	else
	{
		std::uint32_t wholeBytes = totalBits / CHAR_BIT;
		if ( remainingBits == 0 )
			--wholeBytes;
		SZ_GetSpace( bfwrite.pbuf, static_cast<int>( wholeBytes ) + advancedBytes );
		if ( bfwrite.pbuf->flags & FSB_OVERFLOWED )
			return;
		MergeBitOutputWord( data );
		if ( remainingBits == 0 )
			remainingBits = CHAR_BIT;
		bfwrite.pOutByte += static_cast<int>( wholeBytes );
		bfwrite.nCurOutputBit = remainingBits;
	}
}

void MSG_EndBitWriting( sizebuf_t* )
{
	if ( bfwrite.pbuf->flags & FSB_OVERFLOWED )
		return;
	*bfwrite.pOutByte &= static_cast<byte>( UCHAR_MAX >> ( CHAR_BIT - bfwrite.nCurOutputBit ) );
	SZ_GetSpace( bfwrite.pbuf, 1 );
	bfwrite.nCurOutputBit = 0;
	bfwrite.pOutByte = nullptr;
	bfwrite.pbuf = nullptr;
}

void MSG_WriteSBits( int data, int numbits )
{
	if ( numbits < static_cast<int>( BITS_PER_WORD ) )
	{
		int limit = static_cast<int>( ( 1u << ( numbits - 1 ) ) - 1u );
		if ( data > limit )
			data = limit;
		else if ( data < -limit )
			data = -limit;
	}

	MSG_WriteOneBit( data < 0 );
	std::uint32_t magnitude = static_cast<std::uint32_t>( data );
	if ( data < 0 )
		magnitude = 0u - magnitude;
	MSG_WriteBits( magnitude, numbits - 1 );
}

void MSG_WriteBitData( void* source, int length )
{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitWriting(), 1109 );
#endif
	const byte* input = static_cast<const byte*>( source );
	for ( int index = 0; index < length; ++index )
	{
		byte value = input[index];
		MSG_WriteBits( value, CHAR_BIT );
	}
}

void MSG_WriteBitString( const char* text )
{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitWriting(), 1093 );
#endif
	byte character = static_cast<byte>( *text );
	while ( character )
	{
		MSG_WriteBits( character, CHAR_BIT );
		character = static_cast<byte>( *++text );
	}
	MSG_WriteBits( 0, CHAR_BIT );
}

static std::uint32_t MSG_CoordinateMagnitude( int value )
{
	std::uint32_t magnitude = static_cast<std::uint32_t>( value );
	return value < 0 ? 0u - magnitude : magnitude;
}

void MSG_WriteBitCoord( float coordinate )
{
	constexpr int COORDINATE_INTEGER_BITS = 12;
	constexpr int COORDINATE_FRACTION_BITS = 3;
	constexpr float COORDINATE_RESOLUTION = 0.125f;

	int truncated = static_cast<int>( coordinate );
	std::uint32_t integer = MSG_CoordinateMagnitude( truncated );
	int scaled = static_cast<int>( coordinate * 8.0f );
	std::uint32_t fraction = MSG_CoordinateMagnitude( scaled ) & 7u;
	qboolean negative = coordinate <= -COORDINATE_RESOLUTION;
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitWriting(), 1414 );
#endif
	MSG_WriteOneBit( static_cast<int>( integer ) );
	MSG_WriteOneBit( static_cast<int>( fraction ) );
	if ( integer || fraction )
	{
		MSG_WriteOneBit( negative );
		if ( integer )
			MSG_WriteBits( integer, COORDINATE_INTEGER_BITS );
		if ( fraction )
			MSG_WriteBits( fraction, COORDINATE_FRACTION_BITS );
	}
}

void MSG_WriteBitVec3Coord( const vec_t* coordinates )
{
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( MSG_IsBitWriting(), 1450 );
#endif
	constexpr float COORDINATE_RESOLUTION = 0.125f;
	float x = coordinates[0];
	qboolean hasX = x >= COORDINATE_RESOLUTION || x <= -COORDINATE_RESOLUTION;
	float y = coordinates[1];
	qboolean hasY = y >= COORDINATE_RESOLUTION || y <= -COORDINATE_RESOLUTION;
	float z = coordinates[2];
	qboolean hasZ = z >= COORDINATE_RESOLUTION || z <= -COORDINATE_RESOLUTION;
	MSG_WriteOneBit( hasX );
	MSG_WriteOneBit( hasY );
	MSG_WriteOneBit( hasZ );
	if ( hasX )
		MSG_WriteBitCoord( coordinates[0] );
	if ( hasY )
		MSG_WriteBitCoord( coordinates[1] );
	if ( hasZ )
		MSG_WriteBitCoord( coordinates[2] );
}

void MSG_WriteBitAngle( float angle, int numBits )
{
	constexpr int MAX_BIT_ANGLE_BITS = sizeof( std::uint32_t ) * CHAR_BIT;
	constexpr int DEGREES_PER_TURN = 360;
	if ( numBits >= MAX_BIT_ANGLE_BITS )
		Sys_Error( "Can't write bit angle with 32 bits precision\n" );
	std::uint32_t steps = 1u << numBits;
	double wrapped = std::fmod( static_cast<double>( angle ), static_cast<double>( DEGREES_PER_TURN ) );
	double unsignedSteps = static_cast<double>( steps );
	float wrappedAngle = static_cast<float>( wrapped );
	float stepsPerTurn = static_cast<float>( unsignedSteps );
	float scaledAngle = wrappedAngle * stepsPerTurn;
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	int packedAngle = _mm_cvtt_ss2si( _mm_set_ss( scaledAngle ) );
#else
	int packedAngle = static_cast<int>( scaledAngle );
#endif
	std::uint32_t value = static_cast<std::uint32_t>( packedAngle / DEGREES_PER_TURN ) & ( steps - 1 );
	MSG_WriteBits( value, numBits );
}

short ShortSwap( short value )
{
	std::uint16_t bits = static_cast<std::uint16_t>( value );
	return static_cast<short>( ( bits >> CHAR_BIT ) | ( bits << CHAR_BIT ) );
}

short ShortNoSwap( short value )
{
	return value;
}

int LongNoSwap( int value )
{
	return value;
}

void COM_BitOpsInit()
{
	bfwrite.nCurOutputBit = 0;
	bfwrite.pOutByte = nullptr;
	bfwrite.pbuf = nullptr;
	bfread = {};
}

void COM_Init( char* )
{
	std::uint16_t byteOrder;
	std::memcpy( &byteOrder, swaptest, sizeof( byteOrder ) );
	if ( byteOrder == 1 )
	{
		bigendien = 0;
		BigShort = ShortSwap;
		LittleShort = ShortNoSwap;
		BigLong = LongSwap;
		LittleLong = LongNoSwap;
		BigFloat = FloatSwap;
		LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = 1;
		BigShort = ShortNoSwap;
		LittleShort = ShortSwap;
		BigFloat = FloatNoSwap;
		BigLong = LongNoSwap;
		LittleLong = LongSwap;
		LittleFloat = FloatSwap;
	}
	bfwrite = {};
	bfread = {};
}

enum
{
	LOADFILE_ZONE = 0,
	LOADFILE_HUNK = 1,
	LOADFILE_TEMP = 2,
	LOADFILE_CACHE = 3,
	LOADFILE_STACK = 4,
	LOADFILE_HEAP = 5
};

byte* COM_LoadFile( char* path, int usehunk, int* pLength )
{
	constexpr int FILE_LOAD_BASE_BYTES = 260;
	constexpr int FILE_LOAD_TAG_LENGTH = 32;

	g_engdstAddrs.COM_LoadFile( &path, &usehunk, &pLength );
	if ( pLength )
		*pLength = 0;

	FileHandle_t file = FS_Open( path, "rb" );
	if ( !file )
		return nullptr;

	int length = static_cast<int>( FS_Size( file ) );
	char base[FILE_LOAD_BASE_BYTES];
	if ( !COM_FileBase_s( path, base, FILE_LOAD_BASE_BYTES ) )
		Sys_Error( "COM_LoadFile: bad path name for %s", path );
	base[FILE_LOAD_TAG_LENGTH] = '\0';

	byte* buffer;
	switch ( usehunk )
	{
	case LOADFILE_ZONE:
		buffer = static_cast<byte*>( Z_Malloc( length + 1 ) );
		break;
	case LOADFILE_HUNK:
		buffer = static_cast<byte*>( Hunk_AllocName( length + 1, base ) );
		break;
	case LOADFILE_TEMP:
		buffer = static_cast<byte*>( Hunk_TempAlloc( length + 1 ) );
		break;
	case LOADFILE_CACHE:
		buffer = static_cast<byte*>( Cache_Alloc( loadcache, length + 1, base ) );
		break;
	case LOADFILE_STACK:
		if ( length + 1 <= loadsize )
			buffer = loadbuf;
		else
			buffer = static_cast<byte*>( Hunk_TempAlloc( length + 1 ) );
		break;
	case LOADFILE_HEAP:
		buffer = static_cast<byte*>( Mem_Malloc( length + 1 ) );
		break;
	default:
		Sys_Error( "COM_LoadFile: bad usehunk" );
	}

	if ( !buffer )
		Sys_Error( "COM_LoadFile: not enough space for %s", path );

	buffer[length] = '\0';
	FS_Read( buffer, length, 1, file );
	FS_Close( file );
	if ( pLength )
		*pLength = length;
	return buffer;
}

byte* COM_LoadHunkFile( char* filename )
{
	return COM_LoadFile( filename, LOADFILE_HUNK, nullptr );
}

void COM_LoadCacheFile( char* path, cache_user_t* user )
{
	loadcache = user;
	COM_LoadFile( path, LOADFILE_CACHE, nullptr );
}

byte* COM_LoadFileForMe( char* filename, int* pLength )
{
	return COM_LoadFile( filename, LOADFILE_HEAP, pLength );
}

// uses temp hunk if larger than bufsize
byte* COM_LoadStackFile( char* path, void* buffer, int bufsize, int* length )
{
	loadbuf = static_cast<byte*>( buffer );
	loadsize = bufsize;
	return COM_LoadFile( path, LOADFILE_STACK, length );
}

/*
============
COM_CreatePath

Only used for CopyFile
============
*/
void COM_CreatePath( char* path )
{
	char* cursor = path + 1;
	char separator = *cursor;
	while ( separator )
	{
		if ( separator == '/' || separator == '\\' )
		{       // create the directory
			*cursor = 0;
			FS_CreateDirHierarchy( path, nullptr );
			*cursor = separator;
		}
		separator = *++cursor;
	}
}

int COM_ExpandFilename( char* filename )
{
	char localPath[GAME_DIRECTORY_BYTES];
	FS_GetLocalPath( filename, localPath, sizeof( localPath ) );

	const char* source = localPath;
	char* destination = filename;
	char character;
	do
	{
		character = *source++;
		*destination++ = character;
	} while ( character );

	return std::strlen( filename ) != 0;
}

void COM_DefaultExtension( char* path, char* extension )
{
	char* suffix = path + Q_strlen( path ) - 1;
	char character = *suffix;
	while ( character != '/' )
	{
		if ( suffix == path )
			break;
		if ( character == '.' )
			return;                 // it has an extension
		character = *--suffix;
	}

	std::size_t count = GAME_DIRECTORY_BYTES - std::strlen( suffix );
	std::strncat( path, extension, count );
}

char* Q_strlwr( char* text )
{
	char character = *text;
	while ( character )
	{
		*text = static_cast<char>( std::tolower( character ) );
		character = *++text;
	}
	return text;
}

byte* COM_LoadFileLimit( char* path, int pos, int cbmax, int* pcbread, FileHandle_t* phFile )
{
	*pcbread = 0;
	FileHandle_t file = *phFile;
	if ( !file )
	{
		file = FS_Open( path, "rb" );
		if ( !file )
			return nullptr;
	}

	int size = static_cast<int>( FS_Size( file ) );
	if ( pos > size )
		Sys_Error( "COM_LoadFileLimit: invalid seek position for %s", path );
	FS_Seek( file, pos, FILESYSTEM_SEEK_HEAD );

	if ( size > cbmax )
		size = cbmax;
	byte* buffer = static_cast<byte*>( Hunk_TempAlloc( size + 1 ) );
	if ( !buffer )
	{
		if ( path )
			Sys_Error( "COM_LoadFileLimit: not enough space for %s", path );
		FS_Close( file );
		*phFile = nullptr;
		return nullptr;
	}

	buffer[size] = '\0';
	FS_Read( buffer, size, 1, file );
	*pcbread = size;
	*phFile = file;
	return buffer;
}

int COM_CompareFileTime( char* filename1, char* filename2, int* iCompare )
{
	*iCompare = 0;
	if ( !filename1 || !filename2 )
		return 0;

	int32 firstTime = FS_GetFileTime( filename1 );
	int32 secondTime = FS_GetFileTime( filename2 );
	if ( firstTime < secondTime )
		*iCompare = -1;
	else if ( firstTime > secondTime )
		*iCompare = 1;
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szGameDir - 
//-----------------------------------------------------------------------------
void COM_GetGameDir( char* szGameDir )
{
	if ( szGameDir )
		_snprintf( szGameDir, GAME_DIRECTORY_BYTES - 1, "%s", com_gamedir );
}

void COM_FreeFile( void* buffer )
{
	g_engdstAddrs.COM_FreeFile( &buffer );
	if ( buffer )
		Mem_Free( buffer );
}

void COM_ListMaps( char* pszSubString )
{
	constexpr int MAP_NAME_BUFFER_BYTES = 64;
	std::size_t prefixLength = 0;
	if ( pszSubString && *pszSubString )
		prefixLength = std::strlen( pszSubString );

	Con_Printf( "-------------\n" );
	dheader_t header;
	char localPath[GAME_DIRECTORY_BYTES];
	char mapName[MAP_NAME_BUFFER_BYTES];
	char wildcard[MAP_NAME_BUFFER_BYTES];
	for ( int showOutdated = 1; showOutdated >= 0; --showOutdated )
	{
		std::strcpy( wildcard, "maps/*.bsp" );
		for ( const char* filename = Sys_FindFirst( wildcard, nullptr ); filename; filename = Sys_FindNext( nullptr ) )
		{
			unsigned int pathLength =
				static_cast<unsigned int>( _snprintf( localPath, sizeof( localPath ), "maps/%s", filename ) );
			if ( pathLength >= sizeof( localPath ) )
			{
				Con_Printf( "Map name too long: %s\n", filename );
				continue;
			}

			FS_GetLocalPath( localPath, localPath, sizeof( localPath ) );
			if ( !std::strstr( localPath, com_gamedir )
				 || ( prefixLength && _strnicmp( filename, pszSubString, prefixLength ) != 0 ) )
				continue;

			std::memset( &header, 0, sizeof( header ) );
			unsigned int nameLength =
				static_cast<unsigned int>( _snprintf( mapName, sizeof( mapName ), "maps/%s", filename ) );
			if ( nameLength >= sizeof( mapName ) )
			{
				Con_Printf( "Map name too long: %s\n", filename );
				continue;
			}

			FileHandle_t file = FS_Open( mapName, "rb" );
			if ( file )
			{
				FS_Read( &header, sizeof( header ), 1, file );
				FS_Close( file );
			}
			else
			{
				Con_Printf( "Could not open map %s\n", mapName );
			}

			if ( header.version == BSPVERSION )
			{
				if ( !showOutdated )
					Con_Printf( "%s\n", filename );
			}
			else if ( showOutdated )
			{
				Con_Printf( "OUTDATED:  %s\n", filename );
			}
		}
		Sys_FindClose();
	}
}

qboolean Q_striprefix( const char* text, const char* prefix )
{
	if ( !text )
		return 0;
	if ( !prefix )
		return 0;

	while ( *text )
	{
		if ( !*prefix )
			break;
		int textCharacter = std::tolower( *text );
		int prefixCharacter = std::tolower( *prefix );
		if ( textCharacter != prefixCharacter )
			break;
		++text;
		++prefix;
	}
	return *prefix == '\0';
}

char* COM_BinPrintf( byte* buffer, int length )
{
	constexpr int BINARY_PRINT_BUFFER_BYTES = 4096;
	constexpr int BINARY_PRINT_CHUNK_BYTES = 10;
	static char output[BINARY_PRINT_BUFFER_BYTES];
	char chunk[BINARY_PRINT_CHUNK_BYTES];
	Q_memset( output, 0, sizeof( output ) );
	size_t used = 0;

	for ( int index = 0; index < length; ++index )
	{
		_snprintf( chunk, sizeof( chunk ), "%02x", buffer[index] );
		for ( const char* digit = chunk; *digit && used < sizeof( output ) - 1; ++digit )
			output[used++] = *digit;
		output[used] = 0;
	}
	return output;
}

constexpr int WAVE_CHUNK_ID_BYTES = 4;

typedef struct
{
	char chunkID[WAVE_CHUNK_ID_BYTES];
	long chunkSize;
	short wFormatTag;
	unsigned short wChannels;
	unsigned long dwSamplesPerSec;
	unsigned long dwAvgBytesPerSec;
	unsigned short wBlockAlign;
	unsigned short wBitsPerSample;
} FormatChunk;

static_assert( sizeof( FormatChunk ) == 24 );

unsigned int COM_GetApproxWavePlayLength( const char* filepath )
{
	constexpr int WAVE_HEADER_READ_BYTES = 128;
	constexpr int RIFF_FORMAT_ID_OFFSET = WAVE_CHUNK_ID_BYTES + sizeof( long );
	constexpr int RIFF_FORMAT_CHUNK_OFFSET = RIFF_FORMAT_ID_OFFSET + WAVE_CHUNK_ID_BYTES;
	constexpr unsigned int MILLISECONDS_PER_SECOND = 1000;

	FileHandle_t file = FS_Open( filepath, "rb" );
	if ( !file )
		return 0;

	unsigned int fileLength = FS_Size( file );
	char buffer[WAVE_HEADER_READ_BYTES + 1];
	FS_Read( buffer, WAVE_HEADER_READ_BYTES, 1, file );
	FS_Close( file );
	buffer[WAVE_HEADER_READ_BYTES] = '\0';

	if ( _strnicmp( buffer, "RIFF", WAVE_CHUNK_ID_BYTES ) != 0 )
		return 0;
	if ( _strnicmp( buffer + RIFF_FORMAT_ID_OFFSET, "WAVE", WAVE_CHUNK_ID_BYTES ) != 0 )
		return 0;

	FormatChunk format;
	std::memcpy( &format, buffer + RIFF_FORMAT_CHUNK_OFFSET, sizeof( format ) );
	if ( _strnicmp( format.chunkID, "fmt ", WAVE_CHUNK_ID_BYTES ) != 0 )
		return 0;
	if ( !format.dwAvgBytesPerSec )
	{
		Con_Printf( "COM_GetApproxWavePlayLength: bad AvgBytesPerSec\n" );
		return 0;
	}

	unsigned int dataBytes = fileLength - WAVE_HEADER_READ_BYTES;
	unsigned int bytesPerMillisecond = format.dwAvgBytesPerSec / MILLISECONDS_PER_SECOND;
	if ( bytesPerMillisecond )
		return dataBytes / bytesPerMillisecond;
	return MILLISECONDS_PER_SECOND * ( dataBytes / format.dwAvgBytesPerSec );
}

void COM_NormalizeAngles( vec_t* angles )
{
	constexpr float HALF_TURN_DEGREES = 180.0f;
	constexpr double DEGREES_PER_TURN = 360.0;

	for ( int i = 0; i < 3; ++i )
	{
		float angle = angles[i];
		if ( angle > HALF_TURN_DEGREES || angle < -HALF_TURN_DEGREES )
		{
			double remainder = std::fmod( static_cast<double>( angle ), DEGREES_PER_TURN );
			if ( remainder > HALF_TURN_DEGREES )
				remainder -= DEGREES_PER_TURN;
			else if ( remainder < -HALF_TURN_DEGREES )
				remainder += DEGREES_PER_TURN;
			angles[i] = static_cast<float>( remainder );
		}
	}
}

qboolean COM_HasFileExtension( const char* path, const char* extension )
{
	if ( !path || !extension )
		return false;
	if ( *extension == '.' )
		++extension;
	return Q_strcmp( COM_FileExtension( path ), extension ) == 0;
}

void COM_AddExtension( char* path, const char* extension, int size )
{
	if ( !path || !extension || size <= 0 )
		return;
	const char* suffix = extension;
	if ( *suffix == '.' )
		++suffix;
	if ( Q_strcmp( COM_FileExtension( path ), suffix ) == 0 )
		return;
	if ( *extension != '.' )
		std::strncat( path, ".", size - 1 );
	std::strncat( path, extension, size - 1 );
}

void COM_CopyFileChunk( FileHandle_t destination, FileHandle_t source, int size )
{
	constexpr int FILE_COPY_BUFFER_BYTES = 1024;
	byte buffer[FILE_COPY_BUFFER_BYTES];
	while ( size > FILE_COPY_BUFFER_BYTES )
	{
		FS_Read( buffer, sizeof( buffer ), 1, source );
		FS_Write( buffer, sizeof( buffer ), 1, destination );
		size -= FILE_COPY_BUFFER_BYTES;
	}
	FS_Read( buffer, size, 1, source );
	FS_Write( buffer, size, 1, destination );
	FS_Flush( source );
	FS_Flush( destination );
}

byte COM_Nibble( char c )
{
	if ( c >= '0' && c <= '9' )
		return c - '0';
	if ( c >= 'A' && c <= 'F' )
		return c - 'A' + 10;
	if ( c >= 'a' && c <= 'f' )
		return c - 'a' + 10;
	return '0';
}

void COM_HexConvert( const char* text, int length, byte* output )
{
	constexpr int HEX_DIGIT_BITS = 4;
	for ( int i = 0; i < length; i += 2 )
		*output++ = static_cast<byte>( ( COM_Nibble( text[i] ) << HEX_DIGIT_BITS ) | COM_Nibble( text[i + 1] ) );
}

static const byte mungify_table[] = {
	0x7A, 0x64, 0x05, 0xF1, 0x1B, 0x9B, 0xA0, 0xB5, 0xCA, 0xED, 0x61, 0x0D, 0x4A, 0xDF, 0x8E, 0xC7 };

static const byte mungify_table2[] = {
	0x05, 0x61, 0x7A, 0xED, 0x1B, 0xCA, 0x0D, 0x9B, 0x4A, 0xF1, 0x64, 0xC7, 0xB5, 0x8E, 0xDF, 0xA0 };

static const byte mungify_table3[] = {
	0x20, 0x07, 0x13, 0x61, 0x03, 0x45, 0x17, 0x72, 0x0A, 0x2D, 0x48, 0x0C, 0x4A, 0x12, 0xA9, 0xB5 };

static void COM_MungeWithTable( byte* data, int length, int sequence, const byte* table, std::size_t tableSize )
{
	constexpr int MUNGE_WORD_BYTES = sizeof( int );
	constexpr byte MUNGE_BYTE_MASK = 0xA5;
	int wordCount = length / MUNGE_WORD_BYTES;
	for ( int i = 0; i < wordCount; ++i )
	{
		int word;
		Q_memcpy( &word, data, sizeof( word ) );
		word = LongSwap( word ^ ~sequence );
		byte* bytes = reinterpret_cast<byte*>( &word );
		for ( int j = 0; j < MUNGE_WORD_BYTES; ++j )
			bytes[j] ^= MUNGE_BYTE_MASK | j | ( j << j ) | table[( i + j ) % tableSize];
		word ^= sequence;
		Q_memcpy( data, &word, sizeof( word ) );
		data += sizeof( word );
	}
}

void COM_Munge( byte* data, int length, int sequence )
{
	COM_MungeWithTable( data, length, sequence, mungify_table, sizeof( mungify_table ) );
}

void COM_Munge2( byte* data, int length, int sequence )
{
	COM_MungeWithTable( data, length, sequence, mungify_table2, sizeof( mungify_table2 ) );
}

void COM_Munge3( byte* data, int length, int sequence )
{
	COM_MungeWithTable( data, length, sequence, mungify_table3, sizeof( mungify_table3 ) );
}

static void COM_UnMungeWithTable( byte* data, int length, int sequence, const byte* table, int tableSize )
{
	constexpr int MUNGE_WORD_BYTES = sizeof( int );
	constexpr byte MUNGE_BYTE_MASK = 0xA5;
	int wordCount = length / MUNGE_WORD_BYTES;
	for ( int i = 0; i < wordCount; ++i )
	{
		int word;
		Q_memcpy( &word, data, sizeof( word ) );
		word ^= sequence;
		byte* bytes = reinterpret_cast<byte*>( &word );
		for ( int j = 0; j < MUNGE_WORD_BYTES; ++j )
			bytes[j] ^= MUNGE_BYTE_MASK | j | ( j << j ) | table[( i + j ) % tableSize];
		word = LongSwap( word ) ^ ~sequence;
		Q_memcpy( data, &word, sizeof( word ) );
		data += sizeof( word );
	}
}

void COM_UnMunge( byte* data, int length, int sequence )
{
	COM_UnMungeWithTable( data, length, sequence, mungify_table, sizeof( mungify_table ) );
}

void COM_UnMunge2( byte* data, int length, int sequence )
{
	COM_UnMungeWithTable( data, length, sequence, mungify_table2, sizeof( mungify_table2 ) );
}

void COM_UnMunge3( byte* data, int length, int sequence )
{
	COM_UnMungeWithTable( data, length, sequence, mungify_table3, sizeof( mungify_table3 ) );
}

qboolean SZ_HasSpace( sizebuf_t* buffer, int length )
{
	return buffer->cursize + length <= buffer->maxsize;
}

void COM_ExtendedExplainDisconnection( qboolean print, const char* format, ... )
{
	constexpr int DISCONNECT_FORMAT_BYTES = 1024;
	static char string[DISCONNECT_FORMAT_BYTES];
	va_list args;
	va_start( args, format );
	_vsnprintf( string, sizeof( string ), format, args );
	va_end( args );
	Q_strncpy( gszExtendedDisconnectReason, string, sizeof( gszExtendedDisconnectReason ) - 1 );
	gszExtendedDisconnectReason[sizeof( gszExtendedDisconnectReason ) - 1] = '\0';
	if ( print && gszExtendedDisconnectReason[0] != '#' )
		Con_Printf( "%s\n", gszExtendedDisconnectReason );
}

void MSG_WriteUsercmd( sizebuf_t* buffer, usercmd_t* to, usercmd_t* from )
{
	delta_t** ppdesc = DELTA_LookupRegistration( const_cast<char*>( "usercmd_t" ) );
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( ppdesc && *ppdesc, 871 );
#endif
	MSG_StartBitWriting( buffer );
	DELTA_WriteDelta( reinterpret_cast<byte*>( from ), reinterpret_cast<byte*>( to ), 1, *ppdesc, nullptr );
	MSG_EndBitWriting( buffer );
}

void MSG_ReadUsercmd( usercmd_t* to, usercmd_t* from )
{
	delta_t* pdesc = SV_LookupDelta( const_cast<char*>( "usercmd_t" ) );
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	ASSERT_AT( pdesc, 1748 );
#endif
	MSG_StartBitReading( &net_message );
	DELTA_ParseDelta( reinterpret_cast<byte*>( from ), reinterpret_cast<byte*>( to ), pdesc, sizeof( usercmd_t ) );
	MSG_EndBitReading( &net_message );
	COM_NormalizeAngles( to->viewangles );
}

int COM_FileSize( char* filename )
{
	FileHandle_t file = FS_Open( filename, "rb" );
	if ( !file )
		return -1;

	unsigned int size = FS_Size( file );
	FS_Close( file );
	return static_cast<int>( size );
}

int COM_EntsForPlayerSlots( int maxClients )
{
	constexpr std::uint32_t EXTRA_EDICTS_PER_PLAYER = 15;
	int baseEdicts = gmodinfo.num_edicts;
	const int parameter = COM_CheckParm( "-num_edicts" );
	if ( parameter )
	{
		if ( baseEdicts <= std::atoi( com_argv[parameter + 1] ) )
			baseEdicts = std::atoi( com_argv[parameter + 1] );
	}
	const std::uint32_t totalEdicts = static_cast<std::uint32_t>( baseEdicts )
									  + EXTRA_EDICTS_PER_PLAYER * static_cast<std::uint32_t>( maxClients )
									  - EXTRA_EDICTS_PER_PLAYER;
	return static_cast<int>( totalEdicts );
}

byte* COM_LoadTempFile( char* path, int* length )
{
	return COM_LoadFile( path, LOADFILE_TEMP, length );
}

int Q_strncasecmp( const char* first, const char* second, std::size_t count )
{
	if ( !first )
		return STRING_NOT_EQUAL;
	if ( !second )
		return 1; // strings not equal
	return _strnicmp( first, second, count );
}

int Q_stricmp( const char* first, const char* second )
{
	if ( !first )
		return STRING_NOT_EQUAL;
	if ( !second )
		return 1;
	return _strnicmp( first, second, CASE_INSENSITIVE_COMPARE_LIMIT );
}

int Q_FileNameCmp( char* file1, char* file2 )
{
	char firstCharacter = *file1;
	char secondCharacter = *file2;
	for ( ;; )
	{
		if ( firstCharacter == '/' )
		{
			if ( secondCharacter != '\\' )
			{
				if ( std::tolower( static_cast<unsigned char>( secondCharacter ) )
					 != std::tolower( static_cast<unsigned char>( '/' ) ) )
					return STRING_NOT_EQUAL;
				if ( !secondCharacter )
					return 0;
			}
		}
		else if ( firstCharacter != '\\' || secondCharacter != '/' )
		{
			if ( std::tolower( static_cast<unsigned char>( firstCharacter ) )
				 != std::tolower( static_cast<unsigned char>( secondCharacter ) ) )
				return STRING_NOT_EQUAL;
			if ( !firstCharacter )
				return 0;
			if ( !secondCharacter )
				return 0;
		}

		firstCharacter = *++file1;
		secondCharacter = *++file2;
	}
}

std::uint64_t Q_strtoull( char* text )
{
	constexpr std::uint64_t DECIMAL_BASE = 10;
	unsigned char character = static_cast<unsigned char>( *text );
	while ( character == ' ' || character == '\t' )
		character = static_cast<unsigned char>( *++text );

	bool negative = false;
	if ( character == '+' )
		character = static_cast<unsigned char>( *++text );
	else if ( character == '-' )
		negative = true;

	std::uint64_t value = 0;
	if ( negative )
	{
		++text;
		while ( static_cast<unsigned char>( static_cast<unsigned char>( *text ) - '0' ) <= 9 )
		{
			unsigned int digit = static_cast<unsigned char>( *text ) - '0';
			value = value * DECIMAL_BASE - digit;
			++text;
		}
		return value;
	}

	if ( static_cast<unsigned char>( character - '0' ) <= 9 )
	{
		do
		{
			value = value * DECIMAL_BASE + ( character - '0' );
			character = static_cast<unsigned char>( *++text );
		} while ( static_cast<unsigned char>( character - '0' ) <= 9 );
	}
	return value;
}

void COM_ExplainDisconnection( qboolean print, const char* format, ... )
{
	constexpr int DISCONNECT_FORMAT_BYTES = 1024;
	static char string[DISCONNECT_FORMAT_BYTES];
	va_list arguments;
	va_start( arguments, format );
	_vsnprintf( string, sizeof( string ), format, arguments );
	va_end( arguments );
#if ( defined( HL25_HW_ENGINE ) || defined( SW_RENDERER ) ) && defined( _WIN32 )
	Q_strncpy( gszDisconnectReason, string, DISCONNECT_REASON_BYTES - 1 );
#else
	std::strncpy( gszDisconnectReason, string, DISCONNECT_REASON_BYTES - 1 );
#endif
	gfExtendedError = true;
	gszDisconnectReason[DISCONNECT_REASON_BYTES - 1] = '\0';
	if ( print && gszDisconnectReason[0] != '#' )
		Con_Printf( "%s\n", gszDisconnectReason );
}

byte* MSG_WriteChar( sizebuf_t* buffer, int value )
{
	byte* destination = static_cast<byte*>( SZ_GetSpace( buffer, 1 ) );
	*destination = static_cast<byte>( value );
	return destination;
}

void MSG_WriteWord( sizebuf_t* buffer, int value )
{
	byte* destination = static_cast<byte*>( SZ_GetSpace( buffer, sizeof( std::uint16_t ) ) );
	std::uint16_t bits = static_cast<std::uint16_t>( value );
	destination[0] = static_cast<byte>( bits );
	destination[1] = static_cast<byte>( bits >> CHAR_BIT );
}

void MSG_ReadVec3Coord( sizebuf_t* buffer, vec_t* coordinates )
{
	if ( !bfread.pbuf )
	{
		MSG_StartBitReading( buffer );
		MSG_ReadBitVec3Coord( coordinates );
		MSG_EndBitReading( buffer );
		return;
	}

	MSG_ReadBitVec3Coord( coordinates );
}

int MSG_ReadWord()
{
	if ( msg_readcount + 1 >= net_message.cursize )
	{
		msg_badread = 1;
		return MESSAGE_READ_ERROR;
	}

	const byte* data = net_message.data + msg_readcount;
	int highByte = data[1];
	int lowByte = data[0];
	msg_readcount += static_cast<int>( sizeof( std::uint16_t ) );
	return ( highByte << CHAR_BIT ) + lowByte;
}

char* MSG_ReadStringLine()
{
	constexpr int LINE_BUFFER_SIZE = 2048;
	static char string[LINE_BUFFER_SIZE];
	int readCount = msg_readcount;
	int messageSize = net_message.cursize;
	const byte* data = net_message.data;
	int length = 0;
	do
	{
		if ( messageSize <= readCount )
		{
			msg_badread = 1;
			string[length] = '\0';
			return string;
		}

		int character = static_cast<signed char>( data[readCount] );
		++readCount;
		msg_readcount = readCount;
		if ( character == -1 || character == 0 || character == '\n' )
		{
			string[length] = '\0';
			return string;
		}

		string[length++] = static_cast<char>( character );
	} while ( length != LINE_BUFFER_SIZE - 1 );

	string[LINE_BUFFER_SIZE - 1] = '\0';
	return string;
}

void MSG_SkipString()
{
	int readCount = msg_readcount;
	while ( readCount < net_message.cursize )
	{
		int character = static_cast<signed char>( net_message.data[readCount++] );
		msg_readcount = readCount;
		if ( character == -1 || character == 0 )
			return;
	}
	msg_badread = 1;
}

void MSG_WriteVec3Coord( sizebuf_t* buffer, const vec_t* coordinates )
{
	if ( !bfwrite.pbuf )
	{
		MSG_StartBitWriting( buffer );
		MSG_WriteBitVec3Coord( coordinates );
		MSG_EndBitWriting( buffer );
		return;
	}

	MSG_WriteBitVec3Coord( coordinates );
}

//============================================================================
char* COM_SkipPath( char* pathname )
{
	char* fileName = pathname;
	char character = *pathname;
	if ( !character )
		return fileName;

	char* cursor = pathname + 1;
	do
	{
		if ( character == '\\' || character == '/' )
			fileName = cursor;
		character = *cursor++;
	} while ( character );
	return fileName;
}

char* COM_LastFileExtension( char* path )
{
	if ( !path || !*path )
		return const_cast<char*>( com_empty_string );

#if defined( _WIN32 )
	return COM_FileExtension( path );
#else
	constexpr int FILE_EXTENSION_BUFFER_SIZE = 8;
	constexpr int NO_EXTENSION_POSITION = -1;
	static char extension[FILE_EXTENSION_BUFFER_SIZE];

	int lastDot = NO_EXTENSION_POSITION;
	for ( int index = 0; path[index]; ++index )
	{
		if ( path[index] == '.' )
			lastDot = index;
	}
	if ( lastDot < 0 )
		return const_cast<char*>( com_empty_string );

	char* input = path + lastDot;
	while ( *input && *input != '.' )
		++input;
	if ( !*input )
		return const_cast<char*>( com_empty_string );
	++input;

	int length = 0;
	while ( length < FILE_EXTENSION_BUFFER_SIZE - 1 )
	{
		char character = *input;
		if ( !character )
			break;
		extension[length++] = character;
		++input;
	}
	extension[length] = '\0';
	return extension;
#endif
}

int COM_TokenWaiting( char* buffer )
{
	unsigned char character = static_cast<unsigned char>( *buffer );
	if ( character == '\0' || character == '\n' )
		return 0;

	while ( std::isspace( character ) && !std::isalnum( character ) )
	{
		character = static_cast<unsigned char>( *++buffer );
		if ( character == '\0' || character == '\n' )
			return 0;
	}
	return 1;
}

int memsearch( byte* start, int count, int search )
{
	if ( count <= 0 )
		return -1;
	if ( *start == search )
		return 0;

	int index = 0;
	while ( count != ++index )
	{
		if ( start[index] == search )
			return index;
	}
	return -1;
}

void COM_WriteFile( char* filename, void* data, int length )
{
	char name[GAME_DIRECTORY_BYTES];
	std::snprintf( name, sizeof( name ), "%s", filename );
	for ( char* cursor = name; *cursor; ++cursor )
	{
		if ( *cursor == '\\' )
			*cursor = '/';
	}
	COM_CreatePath( name );

	FileHandle_t file = FS_Open( name, "wb" );
	if ( file )
	{
		Sys_Printf( const_cast<char*>( "COM_WriteFile: %s\n" ), name );
		FS_Write( data, length, 1, file );
		FS_Close( file );
	}
	else
	{
		Sys_Printf( const_cast<char*>( "COM_WriteFile: failed on %s\n" ), name );
	}
}

void COM_CopyFile( char* sourcePath, char* destinationPath )
{
	constexpr unsigned int COPY_CHUNK_BYTES = 4096;
	FileHandle_t source = FS_Open( sourcePath, "rb" );
	if ( !source )
		return;

	unsigned int remaining = FS_Size( source );
	COM_CreatePath( destinationPath );
	FileHandle_t destination = FS_Open( destinationPath, "wb" );
	char buffer[COPY_CHUNK_BYTES];
	while ( remaining > COPY_CHUNK_BYTES - 1 )
	{
		FS_Read( buffer, COPY_CHUNK_BYTES, 1, source );
		FS_Write( buffer, COPY_CHUNK_BYTES, 1, destination );
		remaining -= COPY_CHUNK_BYTES;
		if ( !remaining )
			break;
	}
	if ( remaining )
	{
		FS_Read( buffer, static_cast<int>( remaining ), 1, source );
		FS_Write( buffer, static_cast<int>( remaining ), 1, destination );
	}
	FS_Close( source );
	FS_Close( destination );
}

void COM_Shutdown()
{
}

void COM_AddDefaultDir( char* directory )
{
	if ( directory && *directory )
		FileSystem_AddFallbackGameDir( directory );
}

void COM_StripTrailingSlash( char* path )
{
	int length = static_cast<int>( std::strlen( path ) );
	if ( length > 0 )
	{
		char* tail = &path[length - 1];
		if ( *tail == '\\' || *tail == '/' )
			*tail = '\0';
	}
}

std::uint32_t MSG_ReadBits_0( int numbits )
{
	byte* input = bfread.pInByte;
	int bit = bfread.nCurInputBit;
	std::uint32_t readCount = static_cast<std::uint32_t>( bfread.msg_readcount );
	std::uint32_t bytesRead = static_cast<std::uint32_t>( bfread.nBytesRead );
	if ( bit > CHAR_BIT - 1 )
	{
		++readCount;
		++bytesRead;
		bfread.msg_readcount = static_cast<int>( readCount );
		++input;
		bit = 0;
		bfread.nCurInputBit = 0;
		bfread.nBytesRead = static_cast<int>( bytesRead );
		bfread.pInByte = input;
	}

	std::uint32_t totalBits = static_cast<std::uint32_t>( numbits ) + static_cast<std::uint32_t>( bit );
	int remainingBits = static_cast<int>( totalBits & ( CHAR_BIT - 1 ) );
	std::uint32_t word;
	std::memcpy( &word, input, sizeof( word ) );
	std::uint32_t value = word >> static_cast<unsigned int>( bit );
	if ( totalBits <= BITS_PER_WORD )
	{
		value &= ROWBITTABLE[numbits];
		std::uint32_t wholeBytes = totalBits / CHAR_BIT;
		if ( remainingBits == 0 )
		{
			--wholeBytes;
			remainingBits = CHAR_BIT;
		}
		bfread.nCurInputBit = remainingBits;
		input += static_cast<int>( wholeBytes );
		readCount += wholeBytes;
		bfread.pInByte = input;
		bytesRead += wholeBytes;
	}
	else
	{
		std::uint32_t nextWord;
		std::memcpy( &nextWord, input + sizeof( word ), sizeof( nextWord ) );
		value |= ( nextWord & ROWBITTABLE[remainingBits] ) << ( BITS_PER_WORD - static_cast<unsigned int>( bit ) );
		input += sizeof( word );
		readCount += sizeof( word );
		bytesRead += sizeof( word );
		bfread.pInByte = input;
		bfread.nCurInputBit = remainingBits;
	}

	bfread.nBytesRead = static_cast<int>( bytesRead );
	bfread.msg_readcount = static_cast<int>( readCount );
	if ( bfread.pbuf->cursize < static_cast<int>( readCount ) )
	{
		msg_badread = 1;
		return 1;
	}
	return value;
}
