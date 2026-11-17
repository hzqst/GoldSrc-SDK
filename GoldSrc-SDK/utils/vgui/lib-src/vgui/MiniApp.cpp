#include "VGUI_MiniApp.h"

#include <cstring>

namespace vgui
{

MiniApp::MiniApp()
{
	_name = (char*)"";
}

void MiniApp::getName( char* name, int nameLen )
{
	vgui_strcpy( name, nameLen, _name );
}

void MiniApp::setName( const char* name )
{
#if defined( _WIN32 )
	_name = _strdup( name );
#else
	_name = strdup( name );
#endif
}

}
