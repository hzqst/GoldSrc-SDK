#include "server_static.h"
#include "cvar.h"
#include "filesystem.h"
#include "conprint.h"
#include "common.h"
#include "info.h"
#include "buildnum.h"
#include "protocol.h"
#include "net.h"
#include "cmd.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <cstring>

#define MAX_LOG_FILES 1000

typedef struct loglist
{
	server_log_t log;
	struct loglist* next;
} LOGLIST_T;

LOGLIST_T* firstLog;

cvar_t mp_logfile = { "mp_logfile", "1", FCVAR_SERVER };
cvar_t mp_logecho = { "mp_logecho", "1" };
cvar_t sv_log_onefile = { "sv_log_onefile", "0" };
cvar_t sv_log_singleplayer = { "sv_log_singleplayer", "0" };
cvar_t sv_logsecret = { "sv_logsecret", "0", FCVAR_PRIVILEGED };

void Log_PrintServerVars()
{
	cvar_t* var;

	if ( !svs.log.active )
		return;

	Log_Printf( "Server cvars start\n" );
	for ( var = cvar_vars; var; var = var->next )
	{
		if ( var->flags & FCVAR_SERVER )
			Log_Printf( "Server cvar \"%s\" = \"%s\"\n", var->name, var->string );
	}
	Log_Printf( "Server cvars end\n" );
}

void Log_Close()
{
	if ( svs.log.file )
	{
		Log_Printf( "Log file closed\n" );
		FS_Close( svs.log.file );
	}
	svs.log.file = NULL;
}

void Log_Printf( const char* format, ... )
{
	static char string[1024];
	va_list argptr;
	time_t ltime;
	struct tm* today;
	LOGLIST_T* list;

	if ( !svs.log.net_log && !firstLog && !svs.log.active )
		return;

	time( &ltime );
	today = localtime( &ltime );

	snprintf( string, sizeof( string ), "L %02i/%02i/%04i - %02i:%02i:%02i: ", today->tm_mon + 1, today->tm_mday,
		today->tm_year + 1900, today->tm_hour, today->tm_min, today->tm_sec );

	va_start( argptr, format );
	vsnprintf( string + Q_strlen( string ), sizeof( string ) - Q_strlen( string ), format, argptr );
	va_end( argptr );

	if ( svs.log.net_log )
		Netchan_OutOfBandPrint( NS_SERVER, svs.log.net_address, "log %s", string );

	for ( list = firstLog; list; list = list->next )
	{
		if ( sv_logsecret.value )
			Netchan_OutOfBandPrint( NS_SERVER, list->log.net_address, "%c%s%s", 'S', sv_logsecret.string, string );
		else
			Netchan_OutOfBandPrint( NS_SERVER, list->log.net_address, "log %s", string );
	}

	if ( svs.log.active && ( svs.maxclients > 1 || sv_log_singleplayer.value ) )
	{
		if ( mp_logecho.value )
			Con_Printf( "%s", string );
		if ( svs.log.file && mp_logfile.value )
			FS_FPrintf( svs.log.file, "%s", string );
	}
}

void Log_Open()
{
	time_t ltime;
	struct tm* today;
	const char* dir;
	char fileBase[260];
	char filename[260];
	FileHandle_t file;
	int i;

	if ( !svs.log.active || ( sv_log_onefile.value && svs.log.file ) )
		return;

	if ( !mp_logfile.value )
	{
		Con_Printf( "Server logging data to console.\n" );
		return;
	}

	Log_Close();

	time( &ltime );
	today = localtime( &ltime );

	dir = Cvar_VariableString( "logsdir" );
	if ( !dir || Q_strlen( dir ) <= 0 || Q_strstr( dir, ":" ) || Q_strstr( dir, ".." ) )
		snprintf( fileBase, sizeof( fileBase ), "logs/L%02i%02i", today->tm_mon + 1, today->tm_mday );
	else
		snprintf( fileBase, sizeof( fileBase ), "%s/L%02i%02i", dir, today->tm_mon + 1, today->tm_mday );

	for ( i = 0; i < MAX_LOG_FILES; i++ )
	{
		snprintf( filename, sizeof( filename ), "%s%03i.log", fileBase, i );
		COM_FixSlashes( filename );
		COM_CreatePath( filename );

		file = FS_OpenPathID( filename, "r", "GAMECONFIG" );
		if ( !file )
			break;
		FS_Close( file );
	}

	if ( i < MAX_LOG_FILES )
	{
		COM_CreatePath( filename );

		file = FS_OpenPathID( filename, "wt", "GAMECONFIG" );
		if ( file )
		{
			Con_Printf( "Server logging data to file %s\n", filename );
			svs.log.file = file;

			int build = build_number();
			Log_Printf( "Log file started (file \"%s\") (game \"%s\") (version \"%i/%s/%d\")\n", filename,
				Info_ValueForKey( Info_Serverinfo(), "*gamedir" ), PROTOCOL_VERSION, gpszVersionString, build );
			return;
		}
	}

	Con_Printf( "Unable to open logfiles under %s\nLogging disabled\n", fileBase );
	svs.log.active = 0;
}

void SV_AddLogAddress_f()
{
	const char* ip;
	int port;
	char text[260];
	netadr_t adr;
	LOGLIST_T* list;
	LOGLIST_T* node;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "logaddress_add:  usage\nlogaddress_add ip port\n" );
		for ( list = firstLog; list; list = list->next )
			Con_Printf( "current:  %s\n", NET_AdrToString( list->log.net_address ) );
		return;
	}

	ip = Cmd_Argv( 1 );
	port = Q_atoi( Cmd_Argv( 2 ) );
	if ( !port )
	{
		Con_Printf( "logaddress_add:  must specify a valid port\n" );
		return;
	}

	if ( !ip || !*ip )
	{
		Con_Printf( "logaddress_add:  unparseable address\n" );
		return;
	}

	snprintf( text, sizeof( text ), "%s:%i", ip, port );
	if ( !NET_StringToAdr( text, &adr ) )
	{
		Con_Printf( "logaddress_add:  unable to resolve %s\n", text );
		return;
	}

	if ( firstLog )
	{
		LOGLIST_T* tail = NULL;
		for ( list = firstLog; list; list = list->next )
		{
			if ( !memcmp( list->log.net_address.ip, adr.ip, sizeof( adr.ip ) ) && list->log.net_address.port == adr.port )
			{
				Con_Printf( "logaddress_add:  address already in list\n" );
				return;
			}
			tail = list;
		}

		node = (LOGLIST_T*)malloc( sizeof( LOGLIST_T ) );
		if ( node )
			tail->next = node;
	}
	else
	{
		firstLog = (LOGLIST_T*)malloc( sizeof( LOGLIST_T ) );
		node = firstLog;
	}

	if ( !node )
	{
		Con_Printf( "logaddress_add:  error allocating new node\n" );
		return;
	}

	node->log.net_address = adr;
	node->next = NULL;
	Con_Printf( "logaddress_add:  %s\n", NET_AdrToString( adr ) );
}

void SV_DelLogAddress_f()
{
	const char* ip;
	int port;
	char text[260];
	netadr_t adr;
	LOGLIST_T* list;
	LOGLIST_T* prev;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "logaddress_del:  usage\nlogaddress_del ip port\n" );
		for ( list = firstLog; list; list = list->next )
			Con_Printf( "current:  %s\n", NET_AdrToString( list->log.net_address ) );
		return;
	}

	ip = Cmd_Argv( 1 );
	port = Q_atoi( Cmd_Argv( 2 ) );
	if ( !port )
	{
		Con_Printf( "logaddress_del:  must specify a valid port\n" );
		return;
	}

	if ( !ip || !*ip )
	{
		Con_Printf( "logaddress_del:  unparseable address\n" );
		return;
	}

	snprintf( text, sizeof( text ), "%s:%i", ip, port );
	if ( !NET_StringToAdr( text, &adr ) )
	{
		Con_Printf( "logaddress_del:  unable to resolve %s\n", text );
		return;
	}

	list = firstLog;
	if ( !list )
	{
		Con_Printf( "logaddress_del:  No addresses added yet\n" );
		return;
	}

	prev = NULL;
	while ( memcmp( list->log.net_address.ip, adr.ip, sizeof( adr.ip ) ) || list->log.net_address.port != adr.port )
	{
		prev = list;
		list = list->next;
		if ( !list )
		{
			Con_Printf( "logaddress_del:  Couldn't find address in list\n" );
			return;
		}
	}

	if ( prev )
		prev->next = list->next;
	else
		firstLog = list->next;

	Con_Printf( "deleting:  %s\n", NET_AdrToString( adr ) );
	free( list );
}

void SV_SetLogAddress_f()
{
	const char* ip;
	int port;
	char text[260];
	netadr_t adr;

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "logaddress:  usage\nlogaddress ip port\n" );
		if ( svs.log.active )
			Con_Printf( "current:  %s\n", NET_AdrToString( svs.log.net_address ) );
		return;
	}

	ip = Cmd_Argv( 1 );
	port = Q_atoi( Cmd_Argv( 2 ) );
	if ( !port )
	{
		Con_Printf( "logaddress:  must specify a valid port\n" );
		return;
	}

	if ( !ip || !*ip )
	{
		Con_Printf( "logaddress:  unparseable address\n" );
		return;
	}

	snprintf( text, sizeof( text ), "%s:%i", ip, port );
	if ( !NET_StringToAdr( text, &adr ) )
	{
		Con_Printf( "logaddress:  unable to resolve %s\n", text );
		return;
	}

	svs.log.net_log = 1;
	svs.log.net_address = adr;
	Con_Printf( "logaddress:  %s\n", NET_AdrToString( adr ) );
}

void SV_ServerLog_f()
{
	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "usage:  log < on | off >\n" );
		Con_Printf( svs.log.active ? "currently logging\n" : "not currently logging\n" );
		return;
	}

	if ( !Q_strcasecmp( Cmd_Argv( 1 ), "off" ) )
	{
		if ( svs.log.active )
		{
			if ( svs.log.file )
				Log_Close();
			Con_Printf( "Server logging disabled.\n" );
			svs.log.active = 0;
		}
	}
	else if ( !Q_strcasecmp( Cmd_Argv( 1 ), "on" ) )
	{
		svs.log.active = 1;
		Log_Open();
	}
	else
	{
		Con_Printf( "log:  unknown parameter %s, 'on' and 'off' are valid\n", Cmd_Argv( 1 ) );
	}
}
