#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "game_controls/iclientvgui.h"
#include "cl_dll/IVGuiClientDll.h"
#include "utlstack.h"

static_assert( std::is_abstract<IClientVGUI>::value, "Client VGUI must be an interface" );
static_assert( std::has_virtual_destructor<IClientVGUI>::value, "Preserve IBaseInterface destructor" );
static_assert( std::is_same<decltype( &IClientVGUI::Initialize ), void ( IClientVGUI::* )( CreateInterfaceFn*, int )>::value, "Initialize ABI" );
static_assert( std::is_same<decltype( &IClientVGUI::SetParent ), void ( IClientVGUI::* )( vgui2::VPANEL )>::value, "SetParent ABI" );
static_assert( std::is_same<decltype( &IClientVGUI::UseVGUI1 ), bool ( IClientVGUI::* )()>::value, "UseVGUI1 ABI" );
static_assert( offsetof( VGuiLibraryInterface_t, CallEnghudClientCmd ) == 15 * sizeof( void* ), "Client command callback offset" );
static_assert( offsetof( VGuiLibraryInterface_t, CallEnghudGetPlayerInfo ) == 24 * sizeof( void* ), "Player info callback offset" );
static_assert( sizeof( VGuiLibraryInterface_t ) == 29 * sizeof( void* ), "Callback table layout" );

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

int main()
{
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
	std::puts( "VGUI interface signatures, callback layout and stack lifetime checks passed" );
}
