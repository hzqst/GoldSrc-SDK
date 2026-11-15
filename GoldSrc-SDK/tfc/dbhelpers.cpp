#include "extdll.h"
#include "util.h"
#include "areadef.h"
#include "dbhelpers.h"
#include "tf_defs.h"
#include "common/dbhelpers_common.h"
#include "tier1/interface.h"

int g_nActiveAreas;

#ifndef _WIN32

#include "cbase.h"
#include "player.h"
#include <errno.h>
#include <unistd.h>

bool g_bDBSent;
bool g_bFirstSend = true;
int g_ClassPlayerCounts[DB_CLASS_COUNT];
int g_nClassPlayerSamples;
float g_fLastPlayerCountSampleTime;
int g_nTotalClients;
int g_nClientSamples;
float g_fLevelStartTime;
IGameDB* g_pDB;
static CSysModule* g_hGameDBDLL;

HTABLE g_hLevel;
HCOLUMN g_hLevel_LevelName;
HCOLUMN g_hLevel_GameLength;
HCOLUMN g_hLevel_WinningTeam;
HCOLUMN g_hLevel_NumClients;

HTABLE g_hLevelClass;
HCOLUMN g_hLevelClass_Parent;
HCOLUMN g_hLevelClass_ClassName;
HCOLUMN g_hLevelClass_ClassPercent;

HTABLE g_hLevelArea;
HCOLUMN g_hLevelArea_AreaName;
HCOLUMN g_hLevelArea_Parent;

HTABLE g_hLevelAreaClass;
HCOLUMN g_hLevelAreaClass_Parent;
HCOLUMN g_hLevelAreaClass_ClassName;
HCOLUMN g_hLevelAreaClass_DeathCount;
HCOLUMN g_hLevelAreaClass_PingTime;
HCOLUMN g_hLevelAreaClass_TimeInArea;

HTABLE g_hLevelAreaClassWeapon;
HCOLUMN g_hLevelAreaClassWeapon_Parent;
HCOLUMN g_hLevelAreaClassWeapon_WeaponName;
HCOLUMN g_hLevelAreaClassWeapon_NumShotsFired;

HTABLE g_hLevelAreaClassWeaponDamage;
HCOLUMN g_hLevelAreaClassWeaponDamage_Parent;
HCOLUMN g_hLevelAreaClassWeaponDamage_DamagedClassName;
HCOLUMN g_hLevelAreaClassWeaponDamage_DamageInflicted;
HCOLUMN g_hLevelAreaClassWeaponDamage_Kills;

cvar_t mp_datamine = { "mp_datamine", "1.0", FCVAR_ARCHIVE };

void DB_GameDBShutdown()
{
	if ( !g_hGameDBDLL )
		return;

	if ( g_pDB )
	{
		g_pDB->Release();
		g_pDB = NULL;
	}
	Sys_UnloadModule( g_hGameDBDLL );
	g_hGameDBDLL = NULL;
}

void DB_GameDBLoad()
{
	DB_GameDBShutdown();
	g_hGameDBDLL = Sys_LoadModule( "gamedb.so" );
	if ( g_hGameDBDLL )
	{
		CreateInterfaceFn factory = Sys_GetFactory( g_hGameDBDLL );
		if ( factory && ( g_pDB = (IGameDB*)factory( "GameDB", NULL ) ) != NULL )
			DB_Clear();
		else
		{
			Sys_UnloadModule( g_hGameDBDLL );
			g_hGameDBDLL = NULL;
		}
	}
	CVAR_REGISTER( &mp_datamine );
}

void DB_OnStartLevel()
{
	g_bDBSent = false;
	DB_Clear();
	g_fLevelStartTime = g_engfuncs.pfnTime();
	g_fLastPlayerCountSampleTime = g_fLevelStartTime;
	for ( int i = 0; i < DB_CLASS_COUNT; ++i )
		g_ClassPlayerCounts[i] = 0;
	g_nClassPlayerSamples = 0;
	g_nTotalClients = 0;
	g_nClientSamples = 0;
}

void DB_OnStartFrame()
{
	float time = g_engfuncs.pfnTime();
	if ( time - g_fLastPlayerCountSampleTime < 3 )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( UTIL_PlayerByIndex( i ) )
			++g_nTotalClients;
	}
	++g_nClientSamples;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer* pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->pev && (unsigned int)pPlayer->pev->playerclass < DB_CLASS_COUNT )
		{
			++g_ClassPlayerCounts[pPlayer->pev->playerclass];
			++g_nClassPlayerSamples;
		}
	}
	g_fLastPlayerCountSampleTime = time;
}

bool DB_AddFinalData()
{
	int teamCount = (int)number_of_teams;
	if ( !g_nClassPlayerSamples || !g_nClientSamples || (unsigned int)( teamCount - 1 ) > 4 )
		return false;

	for ( int i = 0; i < DB_CLASS_COUNT; ++i )
	{
		DB_SetFieldValueInt( DB_GetField( DB_GetRow_LevelClass( DB_GetLevelName(), sClassNames[i] ),
			g_hLevelClass_ClassPercent ), 100 * g_ClassPlayerCounts[i] / g_nClassPlayerSamples );
	}

	DB_SetFieldValueInt( DB_GetField( DB_GetRow_Level( DB_GetLevelName() ), g_hLevel_NumClients ),
		(int)( (float)g_nTotalClients / g_nClientSamples + 0.5f ) );
	DB_SetFieldValueInt( DB_GetField( DB_GetRow_Level( DB_GetLevelName() ), g_hLevel_GameLength ),
		(int)( (int)g_engfuncs.pfnTime() - g_fLevelStartTime ) );

	if ( !TeamFortress_SortTeams() )
	{
		DB_SetFieldValueInt( DB_GetField( DB_GetRow_Level( DB_GetLevelName() ), g_hLevel_WinningTeam ),
			g_iOrderedTeams[teamCount - 1] );
	}

	CAreaDef* uniqueAreas[256];
	int nUniqueAreas = 0;
	for ( CAreaDef* pArea = g_pActiveAreas; pArea; pArea = pArea->m_pNext )
	{
		CAreaDef* pMatch = NULL;
		for ( int i = 0; i < nUniqueAreas; ++i )
		{
			if ( !strcasecmp( uniqueAreas[i]->m_AreaName, pArea->m_AreaName ) )
			{
				pMatch = uniqueAreas[i];
				break;
			}
		}
		if ( pMatch )
			pMatch->MergeData( pArea );
		else if ( nUniqueAreas < (int)ARRAYSIZE( uniqueAreas ) )
			uniqueAreas[nUniqueAreas++] = pArea;
	}

	for ( int i = 0; i < nUniqueAreas; ++i )
	{
		CAreaDef* pArea = uniqueAreas[i];
		for ( int playerClass = 0; playerClass < DB_CLASS_COUNT; ++playerClass )
		{
			DBAreaClassData* pData = &pArea->m_ClassData[playerClass];
			if ( pData->m_DB_nPingTests > 0 )
			{
				DB_SetFieldValueInt( DB_GetField( DB_GetRow_LevelAreaClass( DB_GetLevelName(), pArea->m_AreaName,
					sClassNames[playerClass] ), g_hLevelAreaClass_PingTime ),
					pData->m_DB_PingTotal / pData->m_DB_nPingTests );
			}
			if ( pData->m_DB_TimeInArea > 0 )
			{
				DB_SetFieldValueInt( DB_GetField( DB_GetRow_LevelAreaClass( DB_GetLevelName(), pArea->m_AreaName,
					sClassNames[playerClass] ), g_hLevelAreaClass_TimeInArea ), pData->m_DB_TimeInArea );
			}
		}
	}
	return true;
}

void DB_Clear()
{
	if ( !g_pDB )
		return;

	for ( CAreaDef* pArea = g_pActiveAreas; pArea; pArea = pArea->m_pNext )
		pArea->ClearClassData();
	g_pDB->Term();

	if ( !g_pDB )
		return;

	g_hLevel = g_pDB->AddTable_VA( "Level", 4,
		"LevelName", DB_COLUMN_STRING, &g_hLevel_LevelName,
		"GameLength", DB_COLUMN_INTEGER, &g_hLevel_GameLength,
		"WinningTeam", DB_COLUMN_INTEGER, &g_hLevel_WinningTeam,
		"NumClients", DB_COLUMN_INTEGER, &g_hLevel_NumClients );
	g_hLevelClass = g_pDB->AddTable_VA( "LevelClass", 3,
		"ClassName", DB_COLUMN_STRING, &g_hLevelClass_ClassName,
		"ClassPercent", DB_COLUMN_INTEGER, &g_hLevelClass_ClassPercent,
		"Parent", DB_COLUMN_ROW, &g_hLevelClass_Parent );
	g_hLevelArea = g_pDB->AddTable_VA( "LevelArea", 2,
		"AreaName", DB_COLUMN_STRING, &g_hLevelArea_AreaName,
		"Parent", DB_COLUMN_ROW, &g_hLevelArea_Parent );
	g_hLevelAreaClass = g_pDB->AddTable_VA( "LevelAreaClass", 5,
		"ClassName", DB_COLUMN_STRING, &g_hLevelAreaClass_ClassName,
		"Parent", DB_COLUMN_ROW, &g_hLevelAreaClass_Parent,
		"DeathCount", DB_COLUMN_INTEGER, &g_hLevelAreaClass_DeathCount,
		"PingTime", DB_COLUMN_INTEGER, &g_hLevelAreaClass_PingTime,
		"TimeInArea", DB_COLUMN_INTEGER, &g_hLevelAreaClass_TimeInArea );
	g_hLevelAreaClassWeapon = g_pDB->AddTable_VA( "LevelAreaClassWeapon", 3,
		"WeaponName", DB_COLUMN_STRING, &g_hLevelAreaClassWeapon_WeaponName,
		"Parent", DB_COLUMN_ROW, &g_hLevelAreaClassWeapon_Parent,
		"NumShotsFired", DB_COLUMN_INTEGER, &g_hLevelAreaClassWeapon_NumShotsFired );
	g_hLevelAreaClassWeaponDamage = g_pDB->AddTable_VA( "LevelAreaClassWeaponDamage", 4,
		"DamagedClassName", DB_COLUMN_STRING, &g_hLevelAreaClassWeaponDamage_DamagedClassName,
		"Parent", DB_COLUMN_ROW, &g_hLevelAreaClassWeaponDamage_Parent,
		"DamageInflicted", DB_COLUMN_INTEGER, &g_hLevelAreaClassWeaponDamage_DamageInflicted,
		"Kills", DB_COLUMN_INTEGER, &g_hLevelAreaClassWeaponDamage_Kills );
}

bool DB_Transmit( bool bClear )
{
	g_bDBSent = true;
	g_bFirstSend = false;
	if ( !g_pDB )
		return false;
	if ( !DB_AddFinalData() )
	{
		g_engfuncs.pfnServerPrint( "DB_AddFinalData returned false. Not sending GameDB.\n" );
		return false;
	}

	bool result = true;
	if ( mp_datamine.value != 0 && ( IS_DEDICATED_SERVER() || mp_datamine.value == 2 ) )
	{
		FILE* pFile = fopen( "vmine.ip", "rt" );
		if ( !pFile )
		{
			g_engfuncs.pfnServerPrint( "DB_StartSending: can't open vmine.ip.\n" );
			result = false;
		}
		else
		{
			char ip[256];
			ip[0] = 0;
			fscanf( pFile, "%s", ip );
			fclose( pFile );

			char filename[256];
			char str[256];
			if ( !ip[0] || !GetUniqueFilename( "gameplay_", "db", filename ) )
				result = false;
			else if ( !g_pDB->SaveDB( filename ) )
			{
				sprintf( str, "DB_StartSending (%s): IGameDB::SaveDB failed.\n", filename );
				g_engfuncs.pfnServerPrint( str );
				result = false;
			}
			else
			{
				pid_t child = fork();
				if ( child > 0 )
				{
					sprintf( str, "DB_StartSending (%s): succeeded.\n", filename );
					g_engfuncs.pfnServerPrint( str );
				}
				else if ( child == 0 )
				{
					execl( "./sendgameinfo", "sendgameinfo", "-file", filename, "-ip", ip,
						"-bps", "4096", "-autodelete", (char*)NULL );
					result = false;
				}
				else
				{
					sprintf( str, "DB_StartSending (%s): CreateProcess failed.\n(%s)\n", filename, strerror( errno ) );
					result = false;
				}
			}
		}
	}

	if ( bClear )
		DB_Clear();
	return result;
}

HROW DB_GetRow_Level( const char* pLevelName )
{
	if ( !g_pDB )
		return NULL;
	return g_pDB->FindRow( g_hLevel, 1, true, g_hLevel_LevelName, pLevelName );
}

HROW DB_GetRow_LevelClass( const char* pLevelName, const char* pClassName )
{
	if ( !g_pDB )
		return NULL;
	HROW hLevel = DB_GetRow_Level( pLevelName );
	return g_pDB->FindRow( g_hLevelClass, 2, true, g_hLevelClass_ClassName, pClassName,
		g_hLevelClass_Parent, hLevel );
}

HROW DB_GetRow_LevelArea( const char* pLevelName, const char* pAreaName )
{
	if ( !g_pDB )
		return NULL;
	HROW hLevel = DB_GetRow_Level( pLevelName );
	return g_pDB->FindRow( g_hLevelArea, 2, true, g_hLevelArea_AreaName, pAreaName,
		g_hLevelArea_Parent, hLevel );
}

HROW DB_GetRow_LevelAreaClass( const char* pLevelName, const char* pAreaName, const char* pClassName )
{
	if ( !g_pDB )
		return NULL;
	HROW hArea = DB_GetRow_LevelArea( pLevelName, pAreaName );
	return g_pDB->FindRow( g_hLevelAreaClass, 2, true, g_hLevelAreaClass_ClassName, pClassName,
		g_hLevelAreaClass_Parent, hArea );
}

HROW DB_GetRow_LevelAreaClassWeapon( const char* pLevelName, const char* pAreaName, const char* pClassName,
	const char* pWeaponName )
{
	if ( !g_pDB )
		return NULL;
	HROW hClass = DB_GetRow_LevelAreaClass( pLevelName, pAreaName, pClassName );
	return g_pDB->FindRow( g_hLevelAreaClassWeapon, 2, true, g_hLevelAreaClassWeapon_WeaponName, pWeaponName,
		g_hLevelAreaClassWeapon_Parent, hClass );
}

HROW DB_GetRow_LevelAreaClassWeaponDamage( const char* pLevelName, const char* pAreaName, const char* pClassName,
	const char* pWeaponName, const char* pDamagedClassName )
{
	if ( !g_pDB )
		return NULL;
	HROW hWeapon = DB_GetRow_LevelAreaClassWeapon( pLevelName, pAreaName, pClassName, pWeaponName );
	return g_pDB->FindRow( g_hLevelAreaClassWeaponDamage, 2, true,
		g_hLevelAreaClassWeaponDamage_DamagedClassName, pDamagedClassName,
		g_hLevelAreaClassWeaponDamage_Parent, hWeapon );
}

HFIELD DB_GetField( HROW hRow, HCOLUMN hColumn )
{
	return g_pDB ? g_pDB->GetField( hRow, hColumn ) : NULL;
}

bool DB_SetFieldValueInt( HFIELD hField, int value )
{
	return g_pDB && g_pDB->SetFieldValueInt( hField, value );
}

bool DB_AddToField( HFIELD hField, int amount )
{
	int value;
	return g_pDB && g_pDB->GetFieldValueInt( hField, value ) && g_pDB->SetFieldValueInt( hField, value + amount );
}

const char* DB_GetLevelName()
{
	return STRING( gpGlobals->mapname );
}

CAreaDef* DB_GetArea( const Vector& pos )
{
	for ( CAreaDef* pArea = g_pActiveAreas; pArea; pArea = pArea->m_pNext )
	{
		if ( pArea->pev )
		{
			Vector local = pos - pArea->pev->origin;
			if ( local.x > pArea->pev->mins.x && local.y > pArea->pev->mins.y && local.z > pArea->pev->mins.z &&
				local.x < pArea->pev->maxs.x && local.y < pArea->pev->maxs.y && local.z < pArea->pev->maxs.z )
				return pArea;
		}
	}
	return &g_DefaultArea;
}

const char* DB_GetAreaName( const Vector& pos )
{
	return DB_GetArea( pos )->m_AreaName;
}

#endif
