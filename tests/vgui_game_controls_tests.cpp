#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <type_traits>

#include "game_controls/iclientvgui.h"
#include "game_controls/map_page_text.h"
#include "cl_dll/IVGuiClientDll.h"
#include "utlstack.h"
#include "game_controls/viewport_message_reader.h"
#include "game_controls/viewport_player_info.h"

static_assert( std::is_abstract<IClientVGUI>::value, "Client VGUI must be an interface" );
static_assert( std::has_virtual_destructor<IClientVGUI>::value, "Preserve IBaseInterface destructor" );
static_assert( std::is_same<decltype( &IClientVGUI::Initialize ), void ( IClientVGUI::* )( CreateInterfaceFn*, int )>::value, "Initialize ABI" );
static_assert( std::is_same<decltype( &IClientVGUI::SetParent ), void ( IClientVGUI::* )( vgui2::VPANEL )>::value, "SetParent ABI" );
static_assert( std::is_same<decltype( &IClientVGUI::UseVGUI1 ), bool ( IClientVGUI::* )()>::value, "UseVGUI1 ABI" );
static_assert( offsetof( VGuiLibraryInterface_t, CallEnghudClientCmd ) == 15 * sizeof( void* ), "Client command callback offset" );
static_assert( offsetof( VGuiLibraryInterface_t, CallEnghudGetPlayerInfo ) == 24 * sizeof( void* ), "Player info callback offset" );
static_assert( offsetof( VGuiLibraryInterface_t, SetPlayerScoreInfo ) == 29 * sizeof( void* ), "Existing callbacks retain their offsets" );
static_assert( sizeof( VGuiLibraryInterface_t ) == 31 * sizeof( void* ), "Callback table layout" );

static void Check( bool condition, const char* message )
{
	if ( !condition )
	{
		std::fprintf( stderr, "%s\n", message );
		std::exit( EXIT_FAILURE );
	}
}

struct Item
{
	static int live;
	int value;
	explicit Item( int number ) : value( number ) { ++live; }
	Item( const Item& other ) : value( other.value ) { ++live; }
	~Item() { --live; }
};

int Item::live = 0;

struct CompactPlayer
{
	short frags, deaths, playerclass, teamnumber;
	char teamname[16];
};

struct PaddedPlayer
{
	char padding[64];
	int teamnumber;
	char teamname[32];
	int playerclass, deaths, frags;
};

template<class Player>
void PlayerLayoutChecks()
{
	Player players[3] = {};
	players[0].frags = 77;
	players[2].frags = 99;
	UpdateViewportPlayerScore( players, 1, 42, -2, 5, -1 );
	Check( players[1].frags == 42 && players[1].deaths == -2 && players[1].playerclass == 5 && players[1].teamnumber == 0, "Score callback uses real player fields" );
	Check( players[0].frags == 77 && players[2].frags == 99, "Score callback preserves adjacent players" );
	char longTeam[512];
	std::memset( longTeam, 't', sizeof( longTeam ) - 1 );
	longTeam[sizeof( longTeam ) - 1] = 0;
	UpdateViewportPlayerTeam( players, 1, longTeam );
	Check( std::strlen( players[1].teamname ) == sizeof( players[1].teamname ) - 1, "Team callback bounds and terminates names" );
	Check( !players[0].teamname[0] && !players[2].teamname[0], "Team callback preserves adjacent players" );
	unsigned char snapshot[sizeof( players )];
	std::memcpy( snapshot, players, sizeof( players ) );
	for ( int index : { -1, 0, 3 } )
	{
		UpdateViewportPlayerScore( players, index, 1, 2, 3, 4 );
		UpdateViewportPlayerTeam( players, index, "invalid" );
	}
	UpdateViewportPlayerTeam( players, 1, nullptr );
	Check( !std::memcmp( snapshot, players, sizeof( players ) ), "Callbacks reject invalid player indices and null teams" );
}

static void MessageReaderChecks()
{
	const unsigned char data[] = { 0xfe, 0xff, 'o', 'k', 0, 0x7f };
	ViewportMessageReader reader( data, sizeof( data ) );
	Check( reader.ReadShort() == -2, "Signed little-endian short" );
	Check( !std::strcmp( "ok", reader.ReadString() ) && reader.IsValid(), "Bounded message string" );
	Check( reader.ReadByte() == 127 && reader.IsValid(), "Final byte is readable" );
	Check( reader.ReadByte() == -1 && !reader.IsValid(), "Read past message end rejected" );
	for ( int size : { 0, 1 } )
	{
		ViewportMessageReader shortRead( data, size );
		Check( shortRead.ReadShort() == -1 && !shortRead.IsValid(), "Truncated short rejected" );
	}
	ViewportMessageReader unterminated( data + 2, 2 );
	Check( !unterminated.ReadString()[0] && !unterminated.IsValid(), "Unterminated string rejected" );
	ViewportMessageReader nullRead( nullptr, 1 ), negativeSize( data, -1 );
	Check( !nullRead.IsValid() && !negativeSize.IsValid(), "Invalid input rejected" );
	ViewportMessageReader emptyString( data + 4, 1 );
	Check( !emptyString.ReadString()[0] && emptyString.IsValid(), "Empty terminated string accepted" );
}

// Decodes count bytes of a map page file the way LoadMapPage does.
static std::string NarrowMapPage( const char* data, int count )
{
	std::string bytes( data, count );
	bytes += '#'; // the slot the terminator must go in
	Check( !IsWideMapPageText( bytes.data(), count ), "Narrow map page detected as wide" );
	DecodeNarrowMapPageText( &bytes[0], count );
	return bytes.c_str();
}

static std::wstring WideMapPage( const char* data, int count )
{
	const wchar_t sentinel = L'#';
	Check( IsWideMapPageText( data, count ), "Wide map page not detected" );
	std::wstring text( count / 2 + 1, sentinel );
	DecodeWideMapPageText( data, count, &text[0] );
	Check( text[count / 2] == sentinel, "Wide map page decoded past count / 2 chars" );
	return text.c_str();
}

static void MapPageTextChecks()
{
	Check( NarrowMapPage( "ABCDE", 5 ) == "ABCDE", "Odd sized narrow map page keeps its last char" );
	Check( NarrowMapPage( "ABCD", 4 ) == "ABCD", "Even sized narrow map page keeps its last char" );
	Check( NarrowMapPage( "", 0 ).empty(), "Empty map page is empty text" );
	Check( NarrowMapPage( "A\r\nB\r\n", 6 ) == "A\nB\n", "Narrow map page drops CR" );
	Check( NarrowMapPage( "\xFF", 1 ) == "\xFF", "One byte map page is narrow" );

	const char wide[] = "\xFF\xFE" "A\0B\0" "\x1A\x4E" "\r\0\n\0" "C\0";
	Check( WideMapPage( wide, sizeof( wide ) - 1 ) == L"AB\x4E1A\nC", "Wide map page decodes UTF-16LE, keeps 0x1A bytes and drops CR" );
	Check( WideMapPage( wide, sizeof( wide ) - 2 ) == L"AB\x4E1A\n", "Wide map page drops a trailing odd byte" );
	Check( WideMapPage( "\xFF\xFE", 2 ).empty(), "Byte order mark alone is empty text" );
	Check( WideMapPage( "\xFF\xFE\xFF", 3 ).empty(), "Byte order mark and an odd byte is empty text" );
}

int main()
{
	MapPageTextChecks();
	MessageReaderChecks();
	PlayerLayoutChecks<CompactPlayer>();
	PlayerLayoutChecks<PaddedPlayer>();
	Check( std::strcmp( "VClientVGUI001", CLIENTVGUI_INTERFACE_VERSION ) == 0, "Client interface version changed" );
	{
		CUtlStack<Item> stack( 1, 1 );
		Item first( 10 ), second( 20 ), popped( 0 );
		Check( stack.Count() == 0 && !stack.IsIdxValid( -1 ) && !stack.IsIdxValid( 0 ), "Empty stack indices" );
		Check( stack.Push( first ) == 0 && stack.Push( second ) == 1, "Push indices across growth" );
		Check( stack.Count() == 2 && stack.Top().value == 20 && stack.Element( 0 ).value == 10, "Stack order" );
		stack.Pop( popped );
		Check( popped.value == 20 && stack.Count() == 1 && Item::live == 4, "Pop(T&) copies and destroys top" );
		stack.Pop();
		Check( stack.Count() == 0 && Item::live == 3, "Pop() destroys top" );
		stack.Push( first );
		stack.Clear();
		Check( stack.Count() == 0 && Item::live == 3, "Clear destroys elements" );
		stack.Push( second );
		stack.Purge();
		Check( stack.Count() == 0 && Item::live == 3, "Purge destroys elements" );
		stack.Push( first );
		Check( stack.Top().value == 10, "Stack reusable after purge" );
	}
	Check( Item::live == 0, "Stack destructor leaked elements" );
	std::puts( "VGUI interface signatures, callback layout, stack lifetime and map page text checks passed" );
}
