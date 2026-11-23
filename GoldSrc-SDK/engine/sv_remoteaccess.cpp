#include "utlbuffer.h"
#include "utllinkedlist.h"
#include "interface.h"
#include "cmd.h"
#include "cvar.h"
#include "conprint.h"
#include "common.h"
#include "filesystem.h"
#include "server.h"
#include "sys.h"
#include "host.h"
#include "net.h"
#include "filter.h"
#include "eiface.h"

#if defined( _WIN32 )
#include <cassert>
#endif

#include <cstdio>
#include <cstring>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"engine\\sv_remoteaccess.cpp" )

#define SERVERDATA_REQUESTVALUE 0
#define SERVERDATA_SETVALUE 1
#define SERVERDATA_EXECCOMMAND 2

#define SERVERDATA_RESPONSE_VALUE 0
#define SERVERDATA_UPDATE 1

class IGameServerData : public IBaseInterface
{
public:
	virtual void WriteDataRequest( const void* buffer, int bufferSize ) = 0;
	virtual int ReadDataResponse( void* buffer, int bufferSize ) = 0;
};

class CServerRemoteAccess : public IGameServerData
{
public:
	struct DataResponse_t
	{
		CUtlBuffer packet;

		DataResponse_t() : packet( 0, 0, false )
		{
		}
	};

	CServerRemoteAccess();
	~CServerRemoteAccess();

	void WriteDataRequest( const void* buffer, int bufferSize );
	int ReadDataResponse( void* buffer, int bufferSize );
	void SendMessageToAdminUI( const char* message );

private:
	void SetValue( const char* variable, const char* value );
	void ExecCommand( const char* cmdString );
	const char* LookupStringValue( const char* variable );
	void GetUserBanList( CUtlBuffer& value );
	void GetPlayerList( CUtlBuffer& value );
	void GetMapList( CUtlBuffer& value );
	bool LookupValue( const char* variable, CUtlBuffer& value );
	void RequestValue( int requestID, const char* variable );

	CUtlLinkedList<DataResponse_t, int> m_ResponsePackets;
	int m_iBytesSent;
	int m_iBytesReceived;
};

extern cvar_t mapcyclefile;
void GetStatsString( char* buffer, int bufferSize );

CServerRemoteAccess::CServerRemoteAccess() : m_ResponsePackets( 0, 0 ), m_iBytesSent( 0 ), m_iBytesReceived( 0 )
{
}

CServerRemoteAccess::~CServerRemoteAccess()
{
}

void CServerRemoteAccess::WriteDataRequest( const void* buffer, int bufferSize )
{
	m_iBytesReceived += bufferSize;

	CUtlBuffer cmd( buffer, bufferSize, false );
	int requestID = cmd.GetInt();
	int requestType = cmd.GetInt();

	switch ( requestType )
	{
	case SERVERDATA_SETVALUE:
	{
		char variable[256];
		char value[512];
		cmd.GetString( variable, sizeof( variable ) );
		cmd.GetString( value, sizeof( value ) );
		SetValue( variable, value );
		break;
	}
	case SERVERDATA_EXECCOMMAND:
	{
		char command[512];
		cmd.GetString( command, sizeof( command ) );
		ExecCommand( command );
		break;
	}
	case SERVERDATA_REQUESTVALUE:
	{
		char variable[512];
		cmd.GetString( variable, sizeof( variable ) );
		RequestValue( requestID, variable );
		break;
	}
#if defined( _WIN32 )
	default:
		ASSERT_AT( !("Unknown requestType in CServerRemoteAccess::WriteDataRequest()"), 189 );
		break;
#endif
	}
}

int CServerRemoteAccess::ReadDataResponse( void* buffer, int bufferSize )
{
	int i = m_ResponsePackets.Head();
	if ( !m_ResponsePackets.IsValidIndex( i ) )
		return 0;

	CUtlBuffer& response = m_ResponsePackets[i].packet;

	int bytesToCopy = response.TellPut();
#if defined( _WIN32 )
	ASSERT_AT( bufferSize >= bytesToCopy, 205 );
#endif
	if ( bufferSize < bytesToCopy )
		bytesToCopy = 0;
	else if ( bytesToCopy > 0 )
		memcpy( buffer, response.Base(), bytesToCopy );

	m_iBytesSent += bytesToCopy;
	m_ResponsePackets.Remove( i );
	return bytesToCopy;
}

void CServerRemoteAccess::SetValue( const char* variable, const char* value )
{
	if ( !_stricmp( variable, "map" ) )
	{
		Cbuf_AddText( "changelevel " );
		Cbuf_AddText( value );
		Cbuf_AddText( "\n" );
		Cbuf_Execute();
		return;
	}

	if ( !_stricmp( variable, "mapcycle" ) )
	{
		FileHandle_t f = FS_Open( mapcyclefile.string, "wt" );
		if ( !f )
		{
			Con_Printf( "Couldn't write to read-only file %s, using file _dev_mapcycle.txt instead.\n", mapcyclefile.string );
			Cvar_DirectSet( &mapcyclefile, "_temp_mapcycle.txt" );
			f = FS_Open( mapcyclefile.string, "wt" );
		}

		if ( f )
		{
			FS_Write( value, (int)strlen( value ) + 1, 1, f );
			FS_Close( f );
		}
		return;
	}

	cvar_t* var = Cvar_FindVar( variable );
	if ( var )
		Cvar_DirectSet( var, value );
}

void CServerRemoteAccess::ExecCommand( const char* cmdString )
{
	Cbuf_AddText( cmdString );
	Cbuf_AddText( "\n" );
	Cbuf_Execute();
}

const char* CServerRemoteAccess::LookupStringValue( const char* variable )
{
	static char s_ReturnBuf[32];

	cvar_t* var = Cvar_FindVar( variable );
	if ( var )
		return var->string;

	if ( !_stricmp( variable, "map" ) )
		return sv.name;

	if ( !_stricmp( variable, "playercount" ) )
	{
		int i, count;
		client_t* client;

		count = 0;
		for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
		{
			if ( client->active || client->spawned || client->connected )
				count++;
		}

		snprintf( s_ReturnBuf, sizeof( s_ReturnBuf ) - 1, "%d", count );
		return s_ReturnBuf;
	}

	if ( !_stricmp( variable, "maxplayers" ) )
	{
		snprintf( s_ReturnBuf, sizeof( s_ReturnBuf ) - 1, "%d", svs.maxclients );
		return s_ReturnBuf;
	}

	if ( !_stricmp( variable, "gamedescription" ) )
		return gEntityInterface.pfnGetGameDescription();

	return NULL;
}

void CServerRemoteAccess::GetUserBanList( CUtlBuffer& value )
{
	int i;

	for ( i = 0; i < numuserfilters; i++ )
		value.Printf( "%i %s : %.3f min\n", i + 1, SV_GetIDString( &userfilters[i].userid ), userfilters[i].banTime );

	for ( i = 0; i < numipfilters; i++ )
	{
		signed char* b = (signed char*)&ipfilters[i].compare;
		value.Printf( "%i %i.%i.%i.%i : %.3f min\n", numuserfilters + i + 1, b[0], b[1], b[2], b[3], ipfilters[i].banTime );
	}

	value.PutChar( 0 );
}

void CServerRemoteAccess::GetPlayerList( CUtlBuffer& value )
{
	int i;
	client_t* client;

	for ( i = 0, client = svs.clients; i < svs.maxclients; i++, client++ )
	{
		if ( !client->active || !client->name[0] )
			continue;

		int time = (int)( realtime - client->netchan.connect_time );
		int loss = (int)client->packet_loss;
		int frags = (int)client->edict->v.frags;
#if defined( _WIN32 )
		float pingMilliseconds = 1000.0f * client->latency;
		int ping = static_cast<int>( pingMilliseconds );
#else
		int ping = (int)( 1000.0 * client->latency );
#endif
		const char* id = SV_GetIDString( &client->network_userid );
		const char* adr = NET_AdrToString( client->netchan.remote_address );

		value.Printf( "\"%s\" %s %s %d %d %d %d\n", client->name, id, adr, ping, loss, frags, time );
	}

	value.PutChar( 0 );
}

void CServerRemoteAccess::GetMapList( CUtlBuffer& value )
{
	char mapwild[64];
	char curDir[4096];
	char mapName[4096];
#if defined( _WIN32 )
	constexpr int localPathBytes = 260;
#else
	constexpr int localPathBytes = sizeof( curDir );
#endif
	const char* findfn;
	char* extension;

	strcpy( mapwild, "maps/*.bsp" );
	for ( findfn = Sys_FindFirst( mapwild, NULL ); findfn; findfn = Sys_FindNext( NULL ) )
	{
		snprintf( curDir, localPathBytes, "maps/%s", findfn );
		FS_GetLocalPath( curDir, curDir, localPathBytes );
		if ( !strstr( curDir, com_gamedir ) )
			continue;

		strcpy( mapName, findfn );
		extension = strstr( mapName, ".bsp" );
		if ( extension )
			*extension = 0;

		value.PutString( mapName );
		value.PutString( "\n" );
	}
	Sys_FindClose();

	value.PutChar( 0 );
}

bool CServerRemoteAccess::LookupValue( const char* variable, CUtlBuffer& value )
{
#if defined( _WIN32 )
	ASSERT_AT( value.IsText(), 312 );
#endif
	const char* strval = LookupStringValue( variable );
	if ( strval )
	{
		value.PutString( strval );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "stats" ) )
	{
		char stats[512];
		GetStatsString( stats, sizeof( stats ) );
		value.PutString( stats );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "banlist" ) )
	{
		GetUserBanList( value );
		return true;
	}

	if ( !_stricmp( variable, "playerlist" ) )
	{
		GetPlayerList( value );
		return true;
	}

	if ( !_stricmp( variable, "maplist" ) )
	{
		GetMapList( value );
		return true;
	}

	if ( !_stricmp( variable, "uptime" ) )
	{
		value.PutInt( (int)( Sys_FloatTime() - Host_GetStartTime() ) );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "ipaddress" ) )
	{
		value.PutString( NET_AdrToString( net_local_adr ) );
		value.PutChar( 0 );
		return true;
	}

	if ( !_stricmp( variable, "mapcycle" ) )
	{
		int len = 0;
		byte* data = COM_LoadFileForMe( mapcyclefile.string, &len );
		if ( data && len )
		{
			value.PutString( (const char*)data );
			value.PutChar( 0 );
			COM_FreeFile( data );
		}
		return true;
	}

	value.PutChar( 0 );
	return false;
}

void CServerRemoteAccess::RequestValue( int requestID, const char* variable )
{
	CUtlBuffer value( 0, 256, true );
	LookupValue( variable, value );

	int i = m_ResponsePackets.AddToTail();
	CUtlBuffer& response = m_ResponsePackets[i].packet;

	response.PutInt( requestID );
	response.PutInt( SERVERDATA_RESPONSE_VALUE );
	response.PutString( variable );
#if defined( _WIN32 )
	ASSERT_AT( value.TellPut() > 0, 246 );
#endif
	response.PutInt( value.TellPut() );
	if ( value.TellPut() )
		response.Put( value.Base(), value.TellPut() );
}

void CServerRemoteAccess::SendMessageToAdminUI( const char* message )
{
	int i = m_ResponsePackets.AddToTail();
	CUtlBuffer& response = m_ResponsePackets[i].packet;

	response.PutInt( 0 );
	response.PutInt( SERVERDATA_UPDATE );
	response.PutString( message );
}

CServerRemoteAccess g_ServerRemoteAccess;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CServerRemoteAccess, IGameServerData, "GameServerData001", g_ServerRemoteAccess );

void NotifyDedicatedServerUI( const char* message )
{
	g_ServerRemoteAccess.SendMessageToAdminUI( message );
}
