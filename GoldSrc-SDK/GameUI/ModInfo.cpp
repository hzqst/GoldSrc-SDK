//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "ModInfo.h"
#include "GameUI_Interface.h"

#include "vgui/KeyValues.h"
#include "FileSystem.h"
#include "controls.h"
#include "wrect.h"
#include "cl_dll.h"

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#include <strings.h>
#define stricmp strcasecmp
#endif
#include <string.h>

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CModInfo* ModInfo()
{
	static CModInfo s_ModInfo;
	return &s_ModInfo;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CModInfo::CModInfo()
{
	m_pModData = new KeyValues( "ModData" );
}

CModInfo::~CModInfo()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModInfo::FreeModInfo()
{
	if ( m_pModData )
	{
		m_pModData->deleteThis();
		m_pModData = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char* CModInfo::GetStartMap()
{
	return m_pModData->GetString( "startmap", "c0a0" );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char* CModInfo::GetTrainMap()
{
	return m_pModData->GetString( "trainmap", "t0a0" );
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::IsMultiplayerOnly()
{
	return stricmp( m_pModData->GetString( "type", "" ), "multiplayer_only" ) == 0;
}

bool CModInfo::IsSinglePlayerOnly()
{
	return stricmp( m_pModData->GetString( "type", "" ), "singleplayer_only" ) == 0;
}

bool CModInfo::BShowAnimatedTitle()
{
	return stricmp( m_pModData->GetString( "animated_title", "0" ), "1" ) == 0;
}

bool CModInfo::BHasHDBackground()
{
	return stricmp( m_pModData->GetString( "hd_background", "0" ), "1" ) == 0;
}

bool CModInfo::BShowSimpleLoadingDialog()
{
	return IsSinglePlayerOnly() || stricmp( m_pModData->GetString( "game", "0" ), "Opposing Force" ) == 0;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoModels()
{
	return stricmp( m_pModData->GetString( "nomodels", "0" ), "1" ) == 0;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
bool CModInfo::NoHiModel()
{
	return stricmp( m_pModData->GetString( "nohimodel", "0" ), "1" ) == 0;
}

const char* CModInfo::GetFallbackDir()
{
	return m_pModData->GetString( "fallback_dir", "" );
}

bool CModInfo::UseFallbackDirMaps()
{
	return stricmp( m_pModData->GetString( "fallback_maps", "1" ), "1" ) == 0;
}

bool CModInfo::GetDetailedTexture()
{
	return stricmp( m_pModData->GetString( "detailed_textures", "0" ), "1" ) == 0;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
const char* CModInfo::GetGameDescription()
{
	return m_pModData->GetString( "game", "Half-Life" );
}

const char* CModInfo::GetMPFilter()
{
	return m_pModData->GetString( "mpfilter", "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CModInfo::LoadGameInfoFromBuffer( const char* buffer, int )
{
	char token[1024];
	char key[256];
	char value[256];
	char* cursor = (char*)buffer;

	while ( cursor )
	{
		cursor = engine->COM_ParseFile( cursor, token );
		if ( !token[0] )
			break;

		strcpy( key, token );
		cursor = engine->COM_ParseFile( cursor, token );
		strcpy( value, token );
		m_pModData->SetString( key, value );
	}
}

void CModInfo::LoadCurrentGameInfo()
{
	// walk through and add the key/value pairs to the keyvalues object
	FileHandle_t file = vgui2::filesystem()->Open( "liblist.gam", "rb" );
	if ( !file )
		return;

	int size = vgui2::filesystem()->Size( file );
	if ( size > 0 )
	{
#ifdef _MSC_VER
#pragma warning( suppress: 6255 )
#endif
		char* buffer = (char*)alloca( size + 1 );
		vgui2::filesystem()->Read( buffer, size, file );
		buffer[size] = 0;
		LoadGameInfoFromBuffer( buffer, size );
	}

	vgui2::filesystem()->Close( file );
}
