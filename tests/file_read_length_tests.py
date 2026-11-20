"""Exercise the production whole file readers of Host_Version, CBasePanel::ApplySchemeSettings and CLocalizedStringTable::AddFile
against a stub file system that reads fewer bytes than the file size, as text mode does when it turns CRLF into LF, and leaves
the rest of the buffer filled with stale tokens, so each reader terminates the text after the bytes actually read.

Run: python tests/file_read_length_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'
TIMEOUT_SECONDS = 60


def span(source, first, last):
    start = source.index(first)
    return source[start:source.index(last, start) + len(last)]


STUBS = r'''
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#include <strings.h>
#define stricmp strcasecmp
#define _alloca alloca
#endif

typedef void* FileHandle_t;
typedef unsigned short ucs2;

// text after the bytes Read returns; a reader that terminates at the file size parses it
static const char STALE[] = " ProductName=stale stale.tga scaled 7 9";
static const int STALE_BYTES = sizeof( STALE ) - 1;
static const int READ_ALL = INT_MIN;

static struct StubFile
{
	std::string data;
	unsigned int size;
	int readResult;
	bool mallocFails;
	bool closed;
} g_File;

static int g_FileTag;

static void SetFile( const std::string& data, unsigned int size, int readResult = READ_ALL )
{
	g_File.data = data;
	g_File.size = size;
	g_File.readResult = readResult;
	g_File.mallocFails = false;
	g_File.closed = false;
}

static unsigned int StubSize( FileHandle_t ) { return g_File.size; }

static int StubRead( void* output, int size, FileHandle_t )
{
	char* bytes = static_cast<char*>( output );
	int count = (int)g_File.data.size() < size ? (int)g_File.data.size() : size;
	memcpy( bytes, g_File.data.data(), count );
	for ( int i = count; i < size; ++i )
		bytes[i] = STALE[( i - count ) % STALE_BYTES];
	return g_File.readResult == READ_ALL ? count : g_File.readResult;
}

static void StubClose( FileHandle_t ) { g_File.closed = true; }

static void* TestMalloc( size_t size ) { return g_File.mallocFails ? NULL : malloc( size ); }

// splits on white space like COM_Parse and ParseFile; returns NULL when no token is left
static const char* NextToken( const char* data, char* token )
{
	token[0] = 0;
	if ( !data )
		return NULL;
	while ( *data && isspace( (unsigned char)*data ) )
		++data;
	if ( !*data )
		return NULL;
	int length = 0;
	while ( *data && !isspace( (unsigned char)*data ) )
		token[length++] = *data++;
	token[length] = 0;
	return data;
}

static void Expect( bool condition, const char* message )
{
	if ( !condition )
	{
		printf( "FAIL: %s\n", message );
		exit( 1 );
	}
}
'''

HOST_STUBS = r'''
#define VERSION_STRING_BYTES 32
#define GAME_DIRECTORY_BYTES 64
#define Q_strcpy strcpy
#define Q_strlen( s ) (int)strlen( s )
#define Q_strnicmp strncasecmp_stub
#define Q_strncpy strncpy
#ifndef _WIN32
#define _snprintf snprintf
#endif
constexpr int MAX_VERSION_KEYS = 2;
const char PATCH_VERSION_KEY[] = "PatchVersion=";
const char PRODUCT_NAME_KEY[] = "ProductName=";
static char gpszVersionString[VERSION_STRING_BYTES];
static char gpszProductString[VERSION_STRING_BYTES];
static char com_token[1024];

static int strncasecmp_stub( const char* a, const char* b, size_t n )
{
	for ( size_t i = 0; i < n; ++i )
	{
		int diff = tolower( (unsigned char)a[i] ) - tolower( (unsigned char)b[i] );
		if ( diff || !a[i] )
			return diff;
	}
	return 0;
}

static FileHandle_t FS_Open( const char*, const char* ) { return &g_FileTag; }
static unsigned int FS_Size( FileHandle_t file ) { return StubSize( file ); }
static int FS_Read( void* output, int size, int, FileHandle_t file ) { return StubRead( output, size, file ); }
static void FS_Close( FileHandle_t file ) { StubClose( file ); }
static void* Mem_Malloc( size_t size ) { return TestMalloc( size ); }
static const char* COM_Parse( const char* data ) { return NextToken( data, com_token ); }
static int COM_CheckParm( const char* ) { return 0; }
static void FS_GetInterfaceVersion( char* output, int ) { output[0] = 0; }
'''

BASEPANEL_STUBS = r'''
namespace vgui2
{
struct StubFileSystem
{
	unsigned int Size( FileHandle_t file ) { return StubSize( file ); }
	int Read( void* output, int size, FileHandle_t file ) { return StubRead( output, size, file ); }
	void Close( FileHandle_t file ) { StubClose( file ); }
	char* ParseFile( char* data, char* token, bool* ) { return const_cast<char*>( NextToken( data, token ) ); }
};
struct StubSurface
{
	std::vector<std::string> textures;
	int CreateNewTextureID() { textures.push_back( "" ); return (int)textures.size() - 1; }
	void DrawSetTextureFile( int id, const char* name, int, bool ) { textures[id] = name; }
	void DrawGetTextureSize( int, int& width, int& height ) { width = height = 1; }
};
static StubFileSystem g_FileSystem;
static StubSurface g_Surface;
static StubFileSystem* filesystem() { return &g_FileSystem; }
static StubSurface* surface() { return &g_Surface; }
}

template<class T>
struct CUtlVector
{
	std::vector<T> items;
	int AddToTail() { items.push_back( T() ); return (int)items.size() - 1; }
	T& operator[]( int i ) { return items[i]; }
	int Count() const { return (int)items.size(); }
};

struct CBasePanel
{
	struct bimage_t
	{
		int imageID;
		int x;
		int y;
		int width;
		int height;
		bool scaled;
		bool fit;
	};
	CUtlVector<bimage_t> m_ImageID;
	int m_iBaseResX = 0;
	int m_iBaseResY = 0;
	void ReadLayout( FileHandle_t file );
};

void CBasePanel::ReadLayout( FileHandle_t file )
{
'''

LOCALIZE_STUBS = r'''
struct IFileSystem
{
	unsigned int Size( FileHandle_t file ) { return StubSize( file ); }
	int Read( void* output, int size, FileHandle_t file ) { return StubRead( output, size, file ); }
	void Close( FileHandle_t file ) { StubClose( file ); }
};

#define malloc TestMalloc
static bool ReadTable( IFileSystem* fileSystem, FileHandle_t file, std::u16string& text )
{
	unsigned int fileSize;
	ucs2* data;
	ucs2* start;
'''

TESTS = r'''
#undef malloc
	for ( ; *start; ++start )
		text += (char16_t)*start;
	fileSystem->Close( file );
	free( data );
	return true;
}

static void HostVersionStopsAtBytesRead()
{
	SetFile( "PatchVersion=1.1.2.7\n", 40 );
	Host_Version();
	Expect( std::string( "1.1.2.7" ) == gpszVersionString, "the patch version is read" );
	Expect( std::string( "valve" ) == gpszProductString, "stale bytes after the read do not set the product name" );

	SetFile( "PatchVersion=1.1.2.7\nProductName=cstrike\n", 41 );
	Host_Version();
	Expect( std::string( "1.1.2.7" ) == gpszVersionString && std::string( "cstrike" ) == gpszProductString, "a whole read sets both keys" );

	SetFile( "PatchVersion=9\n", 40, -1 );
	Host_Version();
	Expect( std::string( "1.0.1.4" ) == gpszVersionString && std::string( "valve" ) == gpszProductString, "a failed read keeps the defaults" );

	SetFile( "PatchVersion=9\n", 40 );
	g_File.mallocFails = true;
	Host_Version();
	Expect( std::string( "1.0.1.4" ) == gpszVersionString && g_File.closed, "a failed allocation keeps the defaults and closes the file" );

	SetFile( "", UINT_MAX );
	Host_Version();
	Expect( std::string( "1.0.1.4" ) == gpszVersionString && g_File.closed, "an oversized file keeps the defaults and closes the file" );
	puts( "PASS: Host_Version parses only the bytes read" );
}

static void BasePanelLayoutStopsAtBytesRead()
{
	CBasePanel panel;
	SetFile( "resolution 640 480\n", 60 );
	panel.ReadLayout( &g_FileTag );
	Expect( 640 == panel.m_iBaseResX && 480 == panel.m_iBaseResY, "the base resolution is read" );
	Expect( 0 == panel.m_ImageID.Count(), "stale bytes after the read add no image" );

	CBasePanel whole;
	SetFile( "resolution 640 480\nbg.tga fit 1 2\n", 34 );
	whole.ReadLayout( &g_FileTag );
	Expect( 1 == whole.m_ImageID.Count() && whole.m_ImageID[0].fit && 2 == whole.m_ImageID[0].y, "a whole read adds its image" );

	CBasePanel failed;
	SetFile( "resolution 640 480\n", 60, -1 );
	failed.ReadLayout( &g_FileTag );
	Expect( 0 == failed.m_iBaseResX && 0 == failed.m_ImageID.Count(), "a failed read parses nothing" );
	puts( "PASS: CBasePanel parses only the layout bytes read" );
}

static std::string Utf16( const char16_t* text, bool trailingByte = false )
{
	std::string bytes( "\xFF\xFE", 2 );
	for ( ; *text; ++text )
	{
		bytes += (char)( *text & 0xFF );
		bytes += (char)( *text >> 8 );
	}
	if ( trailingByte )
		bytes += 'Z';
	return bytes;
}

static void LocalizeFileStopsAtBytesRead()
{
	IFileSystem fileSystem;
	std::u16string text;
	SetFile( Utf16( u"AB" ), 40 );
	Expect( ReadTable( &fileSystem, &g_FileTag, text ) && u"AB" == text, "stale bytes after the read are not part of the table" );

	text.clear();
	SetFile( Utf16( u"AB", true ), 41 );
	Expect( ReadTable( &fileSystem, &g_FileTag, text ) && u"AB" == text, "a trailing odd byte is dropped" );

	SetFile( Utf16( u"AB" ), 40, -1 );
	Expect( !ReadTable( &fileSystem, &g_FileTag, text ) && g_File.closed, "a failed read fails and closes the file" );

	SetFile( Utf16( u"AB" ), 40 );
	g_File.mallocFails = true;
	Expect( !ReadTable( &fileSystem, &g_FileTag, text ) && g_File.closed, "a failed allocation fails and closes the file" );

	SetFile( "", UINT_MAX );
	Expect( !ReadTable( &fileSystem, &g_FileTag, text ) && g_File.closed, "an oversized file fails and closes the file" );
	puts( "PASS: CLocalizedStringTable::AddFile reads only the bytes read" );
}

int main()
{
	HostVersionStopsAtBytesRead();
	BasePanelLayoutStopsAtBytesRead();
	LocalizeFileStopsAtBytesRead();
	return 0;
}
'''


def main():
    host = (ROOT / 'engine/host.cpp').read_text(encoding='utf-8')
    panel = (ROOT / 'GameUI/BasePanel.cpp').read_text(encoding='utf-8')
    table = (ROOT / 'vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    code = STUBS + HOST_STUBS
    code += span(host, 'void Host_Version()', 'std::free( buffer );\n\t}') + '\n}\n'
    code += BASEPANEL_STUBS
    code += span(panel, 'int size = vgui2::filesystem()->Size( file );', 'image.y = atoi( token );\n\t\t}\n\t}') + '\n}\n'
    code += LOCALIZE_STUBS
    code += span(table, 'fileSize = fileSystem->Size( file );', 'start = data + 1;') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-file-read-length-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
