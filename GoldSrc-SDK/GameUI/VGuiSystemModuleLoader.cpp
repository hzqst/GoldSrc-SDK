//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "VGuiSystemModuleLoader.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"

#include "common/IVGuiModule.h"
#include "FileSystem.h"
#include "vgui/ILocalize.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#include <strings.h>
#define stricmp strcasecmp
#endif
#include <string.h>

CVGuiSystemModuleLoader g_VModuleLoader;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CVGuiSystemModuleLoader, IVGuiModuleLoader,
	"VGuiModuleLoader003", g_VModuleLoader );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CVGuiSystemModuleLoader::CVGuiSystemModuleLoader()
{
	m_bModulesInitialized = false;
	m_bPlatformShouldRestartAfterExit = false;
	m_pPlatformModuleData = NULL;
}

CVGuiSystemModuleLoader::~CVGuiSystemModuleLoader()
{
}

//-----------------------------------------------------------------------------
// Purpose: returns number of modules loaded
//-----------------------------------------------------------------------------
int CVGuiSystemModuleLoader::GetModuleCount()
{
	return m_Modules.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the string menu name (unlocalized) of a module
//			moduleIndex is of the range [0, GetModuleCount())
//-----------------------------------------------------------------------------
const char* CVGuiSystemModuleLoader::GetModuleLabel( int moduleIndex )
{
	return m_Modules[moduleIndex].data->GetString( "MenuName", "< unknown >" );
}

//-----------------------------------------------------------------------------
// Purpose: returns a modules interface factory
//-----------------------------------------------------------------------------
CreateInterfaceFn CVGuiSystemModuleLoader::GetModuleFactory( int moduleIndex )
{
	return Sys_GetFactory( m_Modules[moduleIndex].module );
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule( int moduleIndex )
{
	if ( !m_Modules.IsValidIndex( moduleIndex ) )
		return false;

	m_Modules[moduleIndex].moduleInterface->Activate();
	if ( g_pTaskbar )
	{
		wchar_t titleBuffer[1024];
		wchar_t* title = vgui2::localize()->Find( m_Modules[moduleIndex].data->GetName() );
		if ( !title )
		{
			vgui2::localize()->ConvertANSIToUnicode( m_Modules[moduleIndex].data->GetName(),
				titleBuffer, sizeof( titleBuffer ) );
			title = titleBuffer;
		}

		g_pTaskbar->SetTitle( m_Modules[moduleIndex].moduleInterface->GetPanel(), title );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: activates a module by name
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ActivateModule( const char* moduleName )
{
	for ( int i = 0; i < GetModuleCount(); i++ )
	{
		if ( !stricmp( GetModuleLabel( i ), moduleName ) ||
			!stricmp( m_Modules[i].data->GetName(), moduleName ) )
		{
			ActivateModule( i );
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: sets the the platform should update and restart when it quits
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::SetPlatformToRestart()
{
	m_bPlatformShouldRestartAfterExit = true;
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the module loader has acquired the platform mutex and loaded the modules
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsPlatformReady()
{
	return m_bModulesInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::ShouldPlatformRestart()
{
	return m_bPlatformShouldRestartAfterExit;
}

bool CVGuiSystemModuleLoader::InitializeAllModules( CreateInterfaceFn* factories, int count )
{
	bool success = true;
	// Init vgui in the modules
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
		if ( !m_Modules[i].moduleInterface->Initialize( factories, count ) )
		{
			vgui2::ivgui()->DPrintf2( "Platform Error: module failed to initialize\n" );
			success = false;
		}
	}

#ifdef _MSC_VER
#pragma warning( suppress: 6255 )
#endif
	CreateInterfaceFn* moduleFactories = (CreateInterfaceFn*)alloca(
		( count + m_Modules.Count() ) * sizeof( CreateInterfaceFn ) );
	for ( int i = 0; i < count; i++ )
		moduleFactories[i] = factories[i];
	for ( int i = 0; i < m_Modules.Count(); i++ )
		moduleFactories[count + i] = Sys_GetFactory( m_Modules[i].module );

	// give the modules a chance to link themselves together
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
		if ( !m_Modules[i].moduleInterface->PostInitialize( moduleFactories, count + m_Modules.Count() ) )
		{
			vgui2::ivgui()->DPrintf2( "Platform Error: module failed to initialize\n" );
			success = false;
		}

		m_Modules[i].moduleInterface->SetParent( g_pTaskbar->GetVPanel() );
		g_pTaskbar->AddTask( m_Modules[i].moduleInterface->GetPanel() );
	}

	m_bModulesInitialized = true;
	return success;
}

bool CVGuiSystemModuleLoader::LoadPlatformModules( CreateInterfaceFn* factories, int count, bool useSteamModules )
{
	// load platform menu
	KeyValues* platformData = new KeyValues( "Platform" );
	if ( !platformData->LoadFromFile( vgui2::filesystem(), "steam/games/PlatformMenu.vdf", "PLATFORM" ) )
	{
		platformData->deleteThis();
		return false;
	}

	bool success = true;
	// walk the platform menu loading all the interfaces
	KeyValues* menu = platformData->FindKey( "Menu", true );
	for ( KeyValues* data = menu->GetFirstSubKey(); data; data = data->GetNextKey() )
	{
		// see if we should skip steam modules
		if ( !useSteamModules && data->GetInt( "SteamApp", 0 ) )
			continue;

		const char* interfaceName = data->GetString( "interface" );
		if ( !stricmp( interfaceName, "VGuiModuleTracker001" ) )
			continue;

		const char* moduleName = data->GetString( "dll" );
#ifndef _WIN32
		char dllPath[512];
		strncpy( dllPath, moduleName, sizeof( dllPath ) );
		char* extension = strrchr( dllPath, '.' );
		if ( extension )
			*extension = 0;
		strcat( dllPath, "_linux.so" );
		moduleName = dllPath;
#endif

		// load the dll
		char localPath[512];
		if ( !vgui2::filesystem()->GetLocalPath( moduleName, localPath, sizeof( localPath ) ) )
		{
			vgui2::ivgui()->DPrintf2( "Platform Error: couldn't find %s, not loading\n", data->GetString( "dll" ) );
			success = false;
			continue;
		}

		vgui2::filesystem()->GetLocalCopy( localPath );
		CSysModule* module = Sys_LoadModule( localPath );
		if ( !module )
		{
			vgui2::ivgui()->DPrintf2( "Platform Error: bad module '%s', not loading\n", data->GetString( "dll" ) );
			success = false;
			continue;
		}

		CreateInterfaceFn factory = Sys_GetFactory( module );
		IVGuiModule* moduleInterface = (IVGuiModule*)factory( interfaceName, NULL );
		if ( !moduleInterface )
		{
			vgui2::ivgui()->DPrintf2( "Platform Error: module version ('%s, %s) invalid, not loading\n",
				data->GetString( "dll" ), data->GetString( "interface" ) );
			success = false;
			continue;
		}

		// store off the module
		int index = m_Modules.AddToTail();
		m_Modules[index].module = module;
		m_Modules[index].moduleInterface = moduleInterface;
		m_Modules[index].data = data;
	}

	m_pPlatformModuleData = platformData;
	bool initialized = InitializeAllModules( factories, count );
	return success && initialized;
}

//-----------------------------------------------------------------------------
// Purpose: gives all platform modules a chance to Shutdown gracefully
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::ShutdownPlatformModules()
{
	// static include guard to prevent recursive calls
	static bool runningFunction = false;
	if ( runningFunction )
		return;

	runningFunction = true;
	for ( int i = 0; i < m_Modules.Count(); i++ )
		m_Modules[i].moduleInterface->Deactivate();

	// give all the modules notice of quit
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
		vgui2::ivgui()->PostMessage( m_Modules[i].moduleInterface->GetPanel(),
			new KeyValues( "Command", "command", "Quit" ), 0 );
	}

	for ( int i = 0; i < m_Modules.Count(); i++ )
		m_Modules[i].moduleInterface->Shutdown();
	runningFunction = false;
}

void CVGuiSystemModuleLoader::DeactivatePlatformModules()
{
	for ( int i = 0; i < m_Modules.Count(); i++ )
		m_Modules[i].moduleInterface->Deactivate();
}

//-----------------------------------------------------------------------------
// Purpose: Reenables all the deactivated platform modules
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::ReactivatePlatformModules()
{
	for ( int i = 0; i < m_Modules.Count(); i++ )
		m_Modules[i].moduleInterface->Reactivate();
}

//-----------------------------------------------------------------------------
// Purpose: Disables and unloads platform
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::UnloadPlatformModules()
{
	for ( int i = 0; i < m_Modules.Count(); i++ )
		Sys_UnloadModule( m_Modules[i].module );
	m_Modules.RemoveAll();

	if ( m_pPlatformModuleData )
	{
		m_pPlatformModuleData->deleteThis();
		m_pPlatformModuleData = NULL;
	}
}

void CVGuiSystemModuleLoader::RunFrame()
{
	for ( int i = 0; i < m_Modules.Count(); i++ )
		m_Modules[i].moduleInterface->RunFrame();
}

//-----------------------------------------------------------------------------
// Purpose: brings the specified module to the foreground
//-----------------------------------------------------------------------------
bool CVGuiSystemModuleLoader::IsModuleHidden( int moduleIndex )
{
	return m_Modules[moduleIndex].data->GetInt( "Hidden", 0 ) != 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVGuiSystemModuleLoader::PostMessageToAllModules( KeyValues* message )
{
	for ( int i = 0; i < m_Modules.Count(); i++ )
	{
		vgui2::ivgui()->PostMessage( m_Modules[i].moduleInterface->GetPanel(), message->MakeCopy(), 0 );
	}
	message->deleteThis();
}
