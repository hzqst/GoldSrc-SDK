#include "VGUI_MiniApp.h"

#include <cstdlib>
#include <cstring>

namespace vgui
{

namespace
{

// Unnamed MiniApps point here; every other name is owned and freed with free().
char emptyName[] = "";

char* duplicateName( const char* name )
{
	if ( name == NULL || name[0] == 0 )
	{
		return emptyName;
	}

#if defined( _WIN32 )
	char* copy = _strdup( name );
#else
	char* copy = strdup( name );
#endif

	return copy != NULL ? copy : emptyName;
}

void freeName( char* name )
{
	if ( name != emptyName )
	{
		free( name );
	}
}

}

MiniApp::MiniApp()
{
	_name = emptyName;
}

// The original vgui.dll exports a shallow copy constructor and operator=;
// copies own their name so that setName and the destructor never free a
// shared one.
MiniApp::MiniApp( const MiniApp& other )
{
	_name = duplicateName( other._name );
}

#if VGUI_ABI_COMPAT_MODE != 1
MiniApp::~MiniApp()
{
	freeName( _name );
}
#endif

MiniApp& MiniApp::operator=( const MiniApp& other )
{
	if ( this != &other )
	{
		char* newName = duplicateName( other._name );
		freeName( _name );
		_name = newName;
	}

	return *this;
}

void MiniApp::getName( char* name, int nameLen )
{
	vgui_strcpy( name, nameLen, _name );
}

void MiniApp::setName( const char* name )
{
	// Copy first: name may point into the current _name.
	char* newName = duplicateName( name );
	freeName( _name );
	_name = newName;
}

}
