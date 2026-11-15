#include "cl_demo.h"
#include "client.h"
#include "cmd.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "DemoPlayerWrapper.h"
#include "filesystem.h"
#include "gl_model.h"
#include "host.h"
#include "mem.h"
#include "protocol.h"
#include "sound.h"
#include "sys.h"
#include "../common/demo_api.h"
#include "../common/event_args.h"
#include "../common/Sequence.h"
#include "shake.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined( _WIN32 )
#include <tuple>
#endif

#define DEMO_STARTUP			0
#define DEMO_NORMAL				1
#define DEMO_JUMPTIME			2
#define DEMO_HUD_COMMAND		3
#define DEMO_CLIENT_DATA		4
#define DEMO_STOP				5
#define DEMO_EVENT				6
#define DEMO_ANIMATION			7
#define DEMO_SOUND				8
#define DEMO_CLIENTDLL_MESSAGE	9

#define FDEMO_TITLE			0x01
#define FDEMO_CDTRACK		0x04
#define FDEMO_FADE_IN_SLOW	0x08
#define FDEMO_FADE_IN_FAST	0x10
#define FDEMO_FADE_OUT_SLOW	0x20
#define FDEMO_FADE_OUT_FAST	0x40

#define MAX_DEMO_FRAMES			32
#define MAX_DEMO_ENTRIES		1024
#define MAX_DEMO_NAME			256
#define MAX_POSSIBLE_MSG		65536
#define MAX_GAMEGAUGE_SECONDS	300
#define DEMO_HUD_COMMAND_LEN	64
#define COM_COPY_CHUNK_SIZE		1024

#pragma pack( push, 1 )
typedef struct demo_command_s
{
	byte cmd;
	float time;
	int frame;
} demo_command_t;

typedef struct demo_hud_command_s
{
	byte cmd;
	float time;
	int frame;
	char name[DEMO_HUD_COMMAND_LEN];
} demo_hud_command_t;

typedef struct cl_demo_data_s
{
	byte cmd;
	float time;
	int frame;
	vec3_t origin;
	vec3_t viewangles;
	int iWeaponBits;
	float fov;
} cl_demo_data_t;

typedef struct demo_anim_s
{
	byte cmd;
	float time;
	int frame;
	int anim;
	int body;
} demo_anim_t;

typedef struct demo_event_s
{
	byte cmd;
	float time;
	int frame;
	int flags;
	int idx;
	float delay;
} demo_event_t;

typedef struct demo_sound1_s
{
	byte cmd;
	float time;
	int frame;
	int channel;
	int length;
} demo_sound1_t;

typedef struct demo_sound2_s
{
	float volume;
	float attenuation;
	int flags;
	int pitch;
} demo_sound2_t;
#pragma pack( pop )

float g_demotimescale = 1.0f;
static demo_info_t dem_frames[MAX_DEMO_FRAMES];
demo_info_t g_rp;

char gDemoMessageBuffer[512];

client_textmessage_t tm_demomessage =
{
	0, // effect
	255, 255, 255, 255,
	255, 255, 255, 255,
	-1.0f, // x
	-1.0f, // y
	0.0f, // fadein
	0.0f, // fadeout
	0.0f, // holdtime
	0.0f, // fxTime,
	"__DEMOMESSAGE__", // pName message name.
	gDemoMessageBuffer // pMessage
};

cvar_t cl_gamegauge = { "cl_gg", "0" };

// Demo file i/o stuff
demodirectory_t demodir;
demoheader_t demoheader;
demoentry_t* pCurrentEntry;
int nCurEntryIndex;

int g_iMaxFPS;
int g_iMinFPS;
int g_rgFPS[MAX_GAMEGAUGE_SECONDS];
int g_iSecond;
int g_iFrames;
float g_flTime;

ScreenFade sf_demo;
qboolean bInFadeout;

extern float scr_con_current;

void CL_FixPointers( demo_info_t* info )
{
	info->rp.cmd = &info->cmd;
	info->rp.movevars = &info->movevars;
}

void CL_SetDemoViewInfo( ref_params_t* rp, float* view, int viewmodel )
{
	memcpy( &g_rp.rp, rp, sizeof( g_rp.rp ) );
	CL_FixPointers( &g_rp );
	memcpy( &g_rp.cmd, rp->cmd, sizeof( g_rp.cmd ) );
	memcpy( &g_rp.movevars, rp->movevars, sizeof( g_rp.movevars ) );
	VectorCopy( view, g_rp.view );
	g_rp.viewmodel = viewmodel;
}

void CL_GetDemoViewInfo_OLD( ref_params_t* rp, vec_t* view, int* viewmodel )
{
	int i;
	int viewport[4];
	movevars_t* pmovevars;
	usercmd_t* cmd;
	demo_info_t* prev;
	demo_info_t* older;
	float t, frac;
	vec3_t delta;

	for ( i = 0; i < 4; i++ )
		viewport[i] = rp->viewport[i];
	pmovevars = rp->movevars;
	cmd = rp->cmd;

	memcpy( rp, &dem_frames[0].rp, sizeof( *rp ) );

	for ( i = 0; i < 4; i++ )
		rp->viewport[i] = viewport[i];
	rp->cmd = cmd;
	rp->movevars = pmovevars;

	if ( cls.timedemo )
	{
		VectorCopy( dem_frames[0].view, view );
		*viewmodel = dem_frames[0].viewmodel;
		return;
	}

	t = (float)( CL_DemoInTime() - cls.demostarttime - 0.02 );

	prev = &dem_frames[0];
	older = NULL;
	for ( i = 1; i < MAX_DEMO_FRAMES - 1; i++ )
	{
		older = &dem_frames[i];
		CL_FixPointers( older );
		if ( older->timestamp == 0.0f || t > older->timestamp )
			break;
		prev = older;
	}

	if ( !older || older->timestamp == 0.0f || older->timestamp == prev->timestamp )
		return;

	frac = ( t - older->timestamp ) / ( prev->timestamp - older->timestamp );
	if ( frac > 1.0f )
		frac = 1.0f;
	else if ( frac < 0.0f )
		frac = 0.0f;

	VectorSubtract( prev->rp.vieworg, older->rp.vieworg, delta );
	VectorMA( older->rp.vieworg, frac, delta, rp->vieworg );

	for ( i = 0; i < 3; i++ )
	{
		delta[i] = prev->rp.viewangles[i] - older->rp.viewangles[i];
		if ( delta[i] > 180 )
			delta[i] -= 360;
		else if ( delta[i] < -180 )
			delta[i] += 360;
	}
	VectorMA( older->rp.viewangles, frac, delta, rp->viewangles );
	COM_NormalizeAngles( rp->viewangles );

	VectorSubtract( prev->rp.simvel, older->rp.simvel, delta );
	VectorMA( older->rp.simvel, frac, delta, rp->simvel );

	VectorSubtract( prev->rp.simorg, older->rp.simorg, delta );
	VectorMA( older->rp.simorg, frac, delta, rp->simorg );

	VectorSubtract( prev->rp.viewheight, older->rp.viewheight, delta );
	VectorMA( older->rp.viewheight, frac, delta, rp->viewheight );

	VectorSubtract( prev->view, older->view, delta );
	VectorMA( older->view, frac, delta, view );
	*viewmodel = prev->viewmodel;
}

void CL_GetDemoViewInfo( ref_params_t* rp, float* view, int* viewmodel )
{
	if ( DemoPlayer_IsActive() )
		DemoPlayer_GetDemoViewInfo( rp, view, viewmodel );
	else
		CL_GetDemoViewInfo_OLD( rp, view, viewmodel );
}

void CL_DemoFrameInfo( demo_info_t* info, float time )
{
	int i;

	for ( i = MAX_DEMO_FRAMES - 1; i > 0; i-- )
		Q_memcpy( &dem_frames[i], &dem_frames[i - 1], sizeof( demo_info_t ) );

	memcpy( &dem_frames[0], info, sizeof( demo_info_t ) );
	CL_FixPointers( &dem_frames[0] );
	dem_frames[0].timestamp = time;
}

void CL_DemoReadCmdInfo( float time )
{
	demo_info_t info;

	FS_Read( &info, sizeof( info ), 1, cls.demofile );

	memcpy( &g_rp.cmd, &info.cmd, sizeof( g_rp.cmd ) );
	memcpy( &g_rp.movevars, &info.movevars, sizeof( g_rp.movevars ) );
	VectorCopy( info.view, g_rp.view );
	g_rp.viewmodel = info.viewmodel;

	CL_DemoFrameInfo( &info, time );
}

float CL_DemoOutTime( void )
{
	return (float)realtime;
}

float CL_DemoInTime( void )
{
	return (float)( g_demotimescale * realtime );
}

int CL_DemoAPIRecording( void )
{
	return cls.demorecording != 0;
}

int CL_DemoAPIPlayback( void )
{
	return cls.demoplayback != 0;
}

int CL_DemoAPITimedemo( void )
{
	return cls.timedemo != 0;
}

demo_api_t demoapi = { CL_DemoAPIRecording, CL_DemoAPIPlayback, CL_DemoAPITimedemo, CL_WriteClientDLLMessage };
void CL_DemoWriteSequenceInfo( FileHandle_t fp, int length )
{
	sequence_info_t info;

	info.incoming_sequence = cls.netchan.incoming_sequence;
	info.incoming_acknowledged = cls.netchan.incoming_acknowledged;
	info.incoming_reliable_acknowledged = cls.netchan.incoming_reliable_acknowledged;
	info.incoming_reliable_sequence = cls.netchan.incoming_reliable_sequence;
	info.outgoing_sequence = cls.netchan.outgoing_sequence;
	info.reliable_sequence = cls.netchan.reliable_sequence;
	info.last_reliable_sequence = cls.netchan.last_reliable_sequence;
	info.length = LittleLong( length );

	FS_Write( &info, sizeof( info ), 1, fp );
}

void CL_DemoWriteCmdInfo( FileHandle_t fp )
{
	FS_Write( &g_rp, sizeof( g_rp ), 1, fp );
}

void CL_DemoCopyFileChunk( FILE* dst, FILE* src, int nSize )
{
	char copybuf[COM_COPY_CHUNK_SIZE];

	while ( nSize > COM_COPY_CHUNK_SIZE )
	{
		FS_Read( copybuf, COM_COPY_CHUNK_SIZE, 1, src );
		FS_Write( copybuf, COM_COPY_CHUNK_SIZE, 1, dst );
		nSize -= COM_COPY_CHUNK_SIZE;
	}

	FS_Read( copybuf, nSize, 1, src );
	FS_Write( copybuf, nSize, 1, dst );

	FS_Flush( src );
	FS_Flush( dst );
}

void CL_BeginDemoStartup( void )
{
	if ( cls.demoheader )
		FS_Close( cls.demoheader );

	cls.demoheader = FS_OpenPathID( "demoheader.dmf", "w+b", "GAMECONFIG" );
	if ( !cls.demoheader )
	{
		Con_DPrintf( "ERROR: couldn't open temporary header file.\n" );
		return;
	}

	Con_DPrintf( "Spooling demo header.\n" );
}

void CL_WriteDemoStartup( int start, sizebuf_t* msg )
{
	demo_command_t header;
	int len;

	if ( !cls.demoheader )
		return;

	len = msg->cursize - start;
	if ( len <= 0 )
		return;

	header.cmd = DEMO_STARTUP;
	header.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	header.frame = LittleLong( host_framecount - cls.demostartframe );
	FS_Write( &header, sizeof( header ), 1, cls.demoheader );

	CL_DemoWriteCmdInfo( cls.demoheader );
	CL_DemoWriteSequenceInfo( cls.demoheader, len );
	FS_Write( msg->data + start, len, 1, cls.demoheader );
}

/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length and view angles
====================
*/
void CL_WriteDemoMessage( int start, sizebuf_t* msg )
{
	demo_command_t header;
	int len;

	if ( !cls.demorecording || !cls.demofile )
		return;

	len = msg->cursize - start;
	if ( len < 0 )
		return;

	cls.demoframecount++;

	// Demo playback should read this as an incoming message.
	header.cmd = DEMO_NORMAL;
	header.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	header.frame = LittleLong( host_framecount - cls.demostartframe );
	FS_Write( &header, sizeof( header ), 1, cls.demofile );

	CL_DemoWriteCmdInfo( cls.demofile );
	CL_DemoWriteSequenceInfo( cls.demofile, len );
	FS_Write( msg->data + start, len, 1, cls.demofile );
}

void CL_WriteDLLUpdate( client_data_t* cdata )
{
	cl_demo_data_t data;

	if ( !cls.demofile )
		return;

	data.cmd = DEMO_CLIENT_DATA;
	data.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	data.frame = LittleLong( host_framecount - cls.demostartframe );
	VectorCopy( cdata->origin, data.origin );
	VectorCopy( cdata->viewangles, data.viewangles );
	data.iWeaponBits = cdata->iWeaponBits;
	data.fov = cdata->fov;

	FS_Write( &data, sizeof( data ), 1, cls.demofile );
}

void CL_DemoAnim( int anim, int body )
{
	demo_anim_t data;

	if ( !cls.demofile )
		return;

	data.cmd = DEMO_ANIMATION;
	data.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	data.frame = LittleLong( host_framecount - cls.demostartframe );
	data.anim = LittleLong( anim );
	data.body = LittleLong( body );

	FS_Write( &data, sizeof( data ), 1, cls.demofile );
}

void CL_DemoEvent( int flags, int idx, float delay, event_args_t* pargs )
{
	demo_event_t data;

	if ( !cls.demofile )
		return;

	data.cmd = DEMO_EVENT;
	data.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	data.frame = LittleLong( host_framecount - cls.demostartframe );
	data.flags = LittleLong( flags );
	data.idx = LittleLong( idx );
	data.delay = LittleFloat( delay );

	FS_Write( &data, sizeof( data ), 1, cls.demofile );
	FS_Write( pargs, sizeof( *pargs ), 1, cls.demofile );
}

void CL_DemoQueueSound( int channel, const char* sample, float volume, float attenuation, int flags, int pitch )
{
	demo_sound1_t data;
	demo_sound2_t data2;

	if ( !cls.demofile )
		return;

	data.cmd = DEMO_SOUND;
	data.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	data.frame = LittleLong( host_framecount - cls.demostartframe );
	data.channel = LittleLong( channel );
	data.length = LittleLong( Q_strlen( sample ) );
	FS_Write( &data, sizeof( data ), 1, cls.demofile );

	FS_Write( sample, Q_strlen( sample ), 1, cls.demofile );

	data2.volume = LittleFloat( volume );
	data2.attenuation = LittleFloat( attenuation );
	data2.flags = LittleLong( flags );
	data2.pitch = LittleLong( pitch );
	FS_Write( &data2, sizeof( data2 ), 1, cls.demofile );
}

void CL_DemoParseEvent( void )
{
	int value, flags, idx;
	float delay;
	event_args_t args;

	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	flags = LittleLong( value );
	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	idx = LittleLong( value );
	FS_Read( &delay, sizeof( delay ), 1, cls.demofile );
	delay = LittleFloat( delay );
	FS_Read( &args, sizeof( args ), 1, cls.demofile );

	CL_QueueEvent( flags, idx, delay, &args );
}

void CL_DemoParseAnim( void )
{
	int value, anim, body;

	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	anim = LittleLong( value );
	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	body = LittleLong( value );

	hudWeaponAnim( anim, body );
}

void CL_DemoPlaySound( int channel, char* sample, float attenuation, float volume, int flags, int pitch )
{
	sfx_t sentence;
	sfx_t* sfx;

	if ( flags & SND_SENTENCE )
	{
		snprintf( sentence.name, sizeof( sentence.name ), "!%s", rgpszrawsentence[Q_atoi( sample + 1 )] );
		sfx = &sentence;
	}
	else
	{
		sfx = CL_LookupSound( sample );
		if ( !sfx )
			return;
	}

	if ( channel == CHAN_STATIC )
		S_StartStaticSound( cl.playernum + 1, channel, sfx, cl.simorg, volume, attenuation, flags, pitch );
	else
		S_StartDynamicSound( cl.playernum + 1, channel, sfx, cl.simorg, volume, attenuation, flags, pitch );
}

void CL_DemoParseSound( void )
{
	char sample[256];
	int value, channel, flags, pitch;
	unsigned int length;
	float f, volume, attenuation;

	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	channel = LittleLong( value );

	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	length = (unsigned int)LittleLong( value );
	if ( length >= sizeof( sample ) )
		length = sizeof( sample ) - 1;
	FS_Read( sample, (int)length, 1, cls.demofile );
	sample[length] = 0;

	FS_Read( &f, sizeof( f ), 1, cls.demofile );
	volume = LittleFloat( f );
	FS_Read( &f, sizeof( f ), 1, cls.demofile );
	attenuation = LittleFloat( f );
	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	flags = LittleLong( value );
	FS_Read( &value, sizeof( value ), 1, cls.demofile );
	pitch = LittleLong( value );

	CL_DemoPlaySound( channel, sample, volume, attenuation, flags, pitch );
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void CL_ReadClientDLLData( void )
{
	byte buffer[32768];
	int size;
	unsigned int length;

	Q_memset( buffer, 0, sizeof( buffer ) );

	FS_Read( &size, sizeof( size ), 1, cls.demofile );
	length = (unsigned int)LittleLong( size );
	if ( length > sizeof( buffer ) )
		length = sizeof( buffer );

	FS_Read( buffer, (int)length, 1, cls.demofile );
	ClientDLL_ReadDemoBuffer( (int)length, buffer );
}

/*
====================
CL_FinishTimeDemo

====================
*/
void CL_FinishTimeDemo( void )
{
	int frames;
	float time;

	cls.timedemo = FALSE;

	// the first frame didn't count
	frames = host_framecount - cls.td_startframe - 1;
	time = (float)( realtime - cls.td_starttime );
	if ( time == 0.0f )
		time = 1.0f;

	Con_Printf( "%i frames %5.3f seconds %5.3f fps\n", frames, time, frames / time );
}

void CL_FinishGameGauge( void )
{
	int i;
	float time, fps;
	FileHandle_t fp;

	Cvar_Set( "cl_gg", "0" );

	time = (float)( realtime - cls.td_starttime );
	if ( time == 0.0f )
		time = 1.0f;

	// the first frame didn't count
	fps = ( host_framecount - cls.td_startframe - 1 ) / time;

	Con_Printf( "%5.2f Half-Life v%s\n", fps, gpszVersionString );
	Con_Printf( "%d Min\n", g_iMinFPS );
	Con_Printf( "%d Max\n", g_iMaxFPS );
	for ( i = 0; i < g_iSecond; i++ )
		Con_Printf( "%d Second %d\n", g_rgFPS[i], i + 1 );

	fp = FS_OpenPathID( "fps.txt", "w", "GAMECONFIG" );
	FS_FPrintf( fp, "%5.2f Half-Life v%s\n", fps, gpszVersionString );
	FS_FPrintf( fp, "%d Min\n", g_iMinFPS );
	FS_FPrintf( fp, "%d Max\n", g_iMaxFPS );
	for ( i = 0; i < g_iSecond; i++ )
		FS_FPrintf( fp, "%d Second %d\n", g_rgFPS[i], i + 1 );
	FS_Close( fp );

	if ( !COM_CheckParm( "-noquit" ) )
		Cbuf_AddText( "quit\n" );
}

/*
==============
CL_StopPlayback

Called when a demo file runs out, or the user starts a game
==============
*/
void CL_StopPlayback( void )
{
	if ( !cls.demoplayback )
		return;

	if ( DemoPlayer_IsActive() )
		DemoPlayer_Stop();
	else
		FS_Close( cls.demofile );

	cls.demoplayback = FALSE;
	cls.demofile = NULL;
	cls.state = ca_disconnected;
	cls.demoframecount = 0;

	if ( demodir.p_rgEntries )
		free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;

	if ( cl_gamegauge.value != 0.0f )
		CL_FinishGameGauge();

	if ( cls.timedemo )
		CL_FinishTimeDemo();

	Cbuf_AddText( "cd stop\n" );
}
void CL_PlayDemo_f( void )
{
	char name[MAX_DEMO_NAME];

	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 && Cmd_Argc() != 3 )
	{
		Con_Printf( "playdemo <demoname> <replayspeed>: plays a demo\n" );
		Con_Printf( "replayspeed == 1.0 default\n" );
		return;
	}

	g_demotimescale = 1.0f;
	if ( Cmd_Argc() == 3 )
	{
		g_demotimescale = Q_atof( Cmd_Argv( 2 ) );
		if ( g_demotimescale <= 0.001 )
			g_demotimescale = 1.0f;
		Con_Printf( "Playdemo:  Time scale %f\n", g_demotimescale );
	}

//
	// disconnect from server
//
	CL_Disconnect();
	DemoPlayer_SetActive( FALSE );

	// open the demo file
	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - sizeof( ".dem" ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Playing demo from %s.\n", name );
	cls.demofile = FS_Open( name, "rb" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open.\n" );
		cls.demonum = -1; // stop demo loop
		return;
	}

	FS_Read( &demoheader, sizeof( demoheader ), 1, cls.demofile );
	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) - 1 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		cls.demonum = -1;
		return;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "WARNING! demo protocol outdated.\nDemo file protocols %i:%i, Engine protocol is at %i:%i\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
	}

	// Now read in the directory structure.
	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( demodir.nEntries ), 1, cls.demofile );
	if ( demodir.nEntries < 1 || demodir.nEntries > MAX_DEMO_ENTRIES )
	{
		COM_ExplainDisconnection( true, "Error: Corrupt demo file." );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		cls.demonum = -1;
		CL_Disconnect();
		return;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, cls.demofile );

	nCurEntryIndex = 0;
	pCurrentEntry = demodir.p_rgEntries;
	FS_Seek( cls.demofile, pCurrentEntry->nOffset, FILESYSTEM_SEEK_HEAD );

	cls.forcetrack = pCurrentEntry->nCDTrack;
	cls.demostartframe = host_framecount;
	cls.demoplayback = TRUE;
	cls.state = ca_connected;
	cls.passive = FALSE;
	cls.spectator = FALSE;
	cls.demostarttime = CL_DemoInTime();

	Netchan_Setup( NS_CLIENT, &cls.netchan, net_from, -1, &cls, CL_GetFragmentSize );
	NET_ClearLagData( true, false );

	cls.demoframecount = 0;
	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = (float)realtime;

	DispatchDirectUserMsg( "InitHUD", 0, NULL );
}

/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f( void )
{
	byte cmd;
	float f, ft;
	int frame, curpos, i;
	char newname[MAX_PATH];
	char oldname[MAX_PATH];

	if ( cmd_source != src_command )
		return;

	if ( !cls.demorecording )
	{
		Con_Printf( "Not recording a demo.\n" );
		return;
	}

	cmd = DEMO_STOP;
	FS_Write( &cmd, sizeof( cmd ), 1, cls.demofile );

	ft = CL_DemoOutTime() - cls.demostarttime;
	f = LittleFloat( ft );
	FS_Write( &f, sizeof( f ), 1, cls.demofile );

	frame = LittleLong( host_framecount - cls.demostartframe );
	FS_Write( &frame, sizeof( frame ), 1, cls.demofile );

	curpos = (int)FS_Tell( cls.demofile );

	pCurrentEntry->nFileLength = curpos - pCurrentEntry->nOffset;
	pCurrentEntry->fTrackTime = ft;
	pCurrentEntry->nFrames = cls.demoframecount;

	//  Now write out the directory and free it and touch up the demo header.
	FS_Write( &demodir.nEntries, sizeof( demodir.nEntries ), 1, cls.demofile );
	for ( i = 0; i < demodir.nEntries; i++ )
		FS_Write( &demodir.p_rgEntries[i], sizeof( demoentry_t ), 1, cls.demofile );

	if ( demodir.p_rgEntries )
		free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;

	demoheader.nDirectoryOffset = curpos;
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demoheader, sizeof( demoheader ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	cls.demorecording = FALSE;

	cls.td_lastframe = host_framecount;

	if ( cls.demoappending )
	{
		Con_Printf( "Replacing old demo with appended version.\n" );
		Q_strncpy( newname, cls.demofilename, sizeof( newname ) - 1 );
		newname[sizeof( newname ) - 1] = 0;
		_unlink( cls.demofilename );

		COM_StripExtension( cls.demofilename, oldname );
		COM_DefaultExtension( oldname, ".dm2" );
#if defined( _WIN32 )
		std::ignore = rename( oldname, newname );
#else
		if ( rename( oldname, newname ) != 0 )
			Con_Printf( "Couldn't replace demo %s\n", newname );
#endif
	}

	Con_Printf( "Completed demo\n" );
	Con_DPrintf( "Recording time %.2f\nHost frames %i\n", ft, cls.td_lastframe - cls.td_startframe );
}

/*
====================
CL_TimeDemo_f

timedemo [demoname]
====================
*/
void CL_TimeDemo_f( void )
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "timedemo <demoname>: gets demo speeds\n" );
		return;
	}

	CL_PlayDemo_f();

	// cls.td_starttime will be grabbed at the second frame of the demo, so
	// all the loading time doesn't get counted
	cls.timedemo = TRUE;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;
	cls.td_starttime = (float)realtime;
}

void CL_GameGauge_f( void )
{
	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "gg <demoname> : Game Gauge 99\n" );
		return;
	}

	CL_PlayDemo_f();
	Cvar_Set( "cl_gg", "1" );

	g_iMaxFPS = 0;
	g_iMinFPS = INT_MAX;
	g_iFrames = host_framecount;
	cls.td_startframe = host_framecount;
	g_iSecond = 0;
	cls.td_starttime = (float)realtime;
	g_flTime = 0.0f;
	cls.timedemo = TRUE;
	cls.td_lastframe = -1;
}

void CL_GGSpeeds( float flCurTime )
{
	int frames;

	if ( g_flTime == 0.0f )
	{
		g_flTime = flCurTime;
		return;
	}

	if ( flCurTime - g_flTime < 1.0f )
		return;

	if ( scr_con_current == 0.0f )
	{
		frames = host_framecount - g_iFrames;
		if ( frames > g_iMaxFPS )
			g_iMaxFPS = frames;
		if ( frames < g_iMinFPS )
			g_iMinFPS = frames;

		if ( g_iSecond < MAX_GAMEGAUGE_SECONDS )
			g_rgFPS[g_iSecond++] = frames;
	}
	else
	{
		cls.td_starttime = (float)realtime;
		cls.td_startframe = host_framecount;
	}

	g_iFrames = host_framecount;
	g_flTime = flCurTime;
}

void CL_WavePlayLen_f( void )
{
	unsigned int msec;

	if ( cmd_source != src_command )
		return;

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf(
			"waveplaylen <wave file name>: returns approximate number of milliseconds a wave file will take to play.\n" );
		return;
	}

	msec = COM_GetApproxWavePlayLength( Cmd_Argv( 1 ) );
	if ( msec )
		Con_Printf( "Play time is approximately %dms\n", msec );
	else
		Con_Printf( "Unable to read %s, file may be missing or incorrectly formatted.\n", Cmd_Argv( 1 ) );
}

void CL_WriteClientDLLMessage( int size, byte* buffer )
{
	byte cmd;
	float f;
	int frame, len;

	if ( !cls.demorecording || !cls.demofile || size < 0 )
		return;

	cmd = DEMO_CLIENTDLL_MESSAGE;
	FS_Write( &cmd, sizeof( cmd ), 1, cls.demofile );

	f = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	FS_Write( &f, sizeof( f ), 1, cls.demofile );

	frame = LittleLong( host_framecount - cls.demostartframe );
	FS_Write( &frame, sizeof( frame ), 1, cls.demofile );

	len = LittleLong( size );
	FS_Write( &len, sizeof( len ), 1, cls.demofile );
	FS_Write( buffer, size, 1, cls.demofile );
}

void CL_RecordHUDCommand( char* cmdname )
{
	demo_hud_command_t data;

	data.cmd = DEMO_HUD_COMMAND;
	data.time = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	data.frame = LittleLong( host_framecount - cls.demostartframe );

	Q_memset( data.name, 0, sizeof( data.name ) );
	Q_strncpy( data.name, cmdname, sizeof( data.name ) - 1 );
	data.name[sizeof( data.name ) - 1] = 0;

	FS_Write( &data, sizeof( data ), 1, cls.demofile );
}

void CL_ReadNetchanState_OLD( void )
{
	FS_Read( &cls.netchan.incoming_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.incoming_acknowledged, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.incoming_reliable_acknowledged, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.incoming_reliable_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.outgoing_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.reliable_sequence, sizeof( int ), 1, cls.demofile );
	FS_Read( &cls.netchan.last_reliable_sequence, sizeof( int ), 1, cls.demofile );
}

void CL_ReadNetchanState( void )
{
	if ( DemoPlayer_IsActive() )
	{
		DemoPlayer_ReadNetchanState( &cls.netchan.incoming_sequence, &cls.netchan.incoming_acknowledged,
			&cls.netchan.incoming_reliable_acknowledged, &cls.netchan.incoming_reliable_sequence,
			&cls.netchan.outgoing_sequence, &cls.netchan.reliable_sequence, &cls.netchan.last_reliable_sequence );
	}
	else
	{
		CL_ReadNetchanState_OLD();
	}
}
void CL_DemoSectionStart( void )
{
	char cmd[128];

	if ( !pCurrentEntry )
		return;

	if ( pCurrentEntry->nFlags & FDEMO_CDTRACK )
	{
		snprintf( cmd, sizeof( cmd ), "cd stop\ncd play %i\n", pCurrentEntry->nCDTrack );
		Cbuf_AddText( cmd );
	}

	if ( pCurrentEntry->nFlags & FDEMO_FADE_IN_FAST )
	{
		sf_demo.duration = 2048;
		sf_demo.holdTime = 0;
		sf_demo.fadeFlags = FFADE_IN;
		sf_demo.r = sf_demo.g = sf_demo.b = 0;
		sf_demo.a = 255;
		V_ScreenFade( NULL, 0, &sf_demo );
	}

	if ( pCurrentEntry->nFlags & FDEMO_FADE_IN_SLOW )
	{
		sf_demo.duration = 8192;
		sf_demo.holdTime = 0;
		sf_demo.fadeFlags = FFADE_IN;
		sf_demo.r = sf_demo.g = sf_demo.b = 0;
		sf_demo.a = 255;
		V_ScreenFade( NULL, 0, &sf_demo );
	}
}

qboolean CL_SwitchToNextDemoSection( void )
{
	if ( nCurEntryIndex >= 1 && pCurrentEntry && ( pCurrentEntry->nFlags & FDEMO_CDTRACK ) )
		Cbuf_AddText( "cd stop\n" );

	if ( ++nCurEntryIndex >= demodir.nEntries )
	{
		// Done
		Host_EndGame( "End of demo.\n" );
		return FALSE;
	}

	pCurrentEntry = &demodir.p_rgEntries[nCurEntryIndex];
	cls.forcetrack = pCurrentEntry->nCDTrack;

	// Ready to continue reading, reset clock.
	FS_Seek( cls.demofile, pCurrentEntry->nOffset, FILESYSTEM_SEEK_HEAD );

	// Time is now relative to this chunk's clock.
	cls.demostartframe = host_framecount;
	cls.demoframecount = 0;
	cls.demostarttime = CL_DemoInTime();
	return TRUE;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
qboolean CL_ReadDemoMessage_OLD( void )
{
	static int tdlastdemoframe;
	byte msgbuf[MAX_POSSIBLE_MSG];
	char text[DEMO_HUD_COMMAND_LEN];
	client_data_t cdata;
	byte cmd;
	float f, t, ft;
	int frame, msglen, curpos;

	msglen = 0;

	if ( !cls.demofile )
	{
		cls.demoplayback = FALSE;
		CL_Disconnect_f();
		Con_Printf( "Tried to read a demo message with no demo file\n" );
		return FALSE;
	}

	for ( ;; )
	{
		curpos = (int)FS_Tell( cls.demofile );
		net_message.cursize = 0;

		FS_Read( &cmd, sizeof( cmd ), 1, cls.demofile );
		FS_Read( &f, sizeof( f ), 1, cls.demofile );
		f = LittleFloat( f );
		FS_Read( &frame, sizeof( frame ), 1, cls.demofile );
		frame = LittleLong( frame );

		t = CL_DemoInTime();
		ft = t - cls.demostarttime;

		if ( cmd != DEMO_STARTUP && cmd != DEMO_STOP && !cls.timedemo && f >= ft ) // Not ready for a message yet, put it back on the file.
		{
			FS_Seek( cls.demofile, curpos, FILESYSTEM_SEEK_HEAD );
			return FALSE;
		}

		switch ( cmd )
		{
		case DEMO_JUMPTIME:
			cls.demostarttime = t;
			cls.demostartframe = host_framecount;
			continue;

		case DEMO_STOP:
			if ( CL_SwitchToNextDemoSection() )
				continue;
			break;

		case DEMO_HUD_COMMAND:
			FS_Read( text, sizeof( text ), 1, cls.demofile );
			if ( ValidStuffText( text ) )
			{
				Cbuf_AddTextToUntrustedBuffer( text );
				Cbuf_AddTextToUntrustedBuffer( "\n" );
			}
			continue;

		case DEMO_CLIENT_DATA:
			FS_Read( &cdata, sizeof( cdata ), 1, cls.demofile );
			ClientDLL_DemoUpdateClientData( &cdata );
			scr_fov_value = cdata.fov;
			continue;

		case DEMO_EVENT:
			CL_DemoParseEvent();
			continue;

		case DEMO_ANIMATION:
			CL_DemoParseAnim();
			continue;

		case DEMO_SOUND:
			CL_DemoParseSound();
			continue;

		case DEMO_CLIENTDLL_MESSAGE:
			CL_ReadClientDLLData();
			continue;
		}

		break;
	}

	if ( cls.timedemo && tdlastdemoframe == host_framecount )
	{
		curpos = (int)FS_Tell( cls.demofile ) - (int)sizeof( demo_command_t );
		FS_Seek( cls.demofile, curpos, FILESYSTEM_SEEK_HEAD );
		return FALSE;
	}
	tdlastdemoframe = host_framecount;

	if ( host_framecount == cls.td_startframe + 1 )
		cls.td_starttime = (float)realtime;

	if ( nCurEntryIndex && cls.demoframecount == 0 )
		CL_DemoSectionStart();

	if ( nCurEntryIndex && cls.demoframecount == 1 )
	{
		bInFadeout = FALSE;
		if ( pCurrentEntry->nFlags & FDEMO_TITLE )
		{
			SetDemoMessage( pCurrentEntry->szDescription, 0.01f, 1.0f, 2.0f );
			CL_HudMessage( "__DEMOMESSAGE__" );
		}

		if ( !cls.timedemo )
			cls.demostarttime = CL_DemoInTime();
	}

	cls.demoframecount++;

	if ( nCurEntryIndex && !bInFadeout )
	{
		if ( ( pCurrentEntry->nFlags & FDEMO_FADE_OUT_FAST ) && ft + 0.5 >= pCurrentEntry->fTrackTime + 0.02 )
		{
			bInFadeout = TRUE;
			sf_demo.duration = 2048;
			sf_demo.holdTime = 409;
			sf_demo.fadeFlags = FFADE_OUT;
			sf_demo.r = sf_demo.g = sf_demo.b = 0;
			sf_demo.a = 255;
			V_ScreenFade( NULL, 0, &sf_demo );
		}

		if ( ( pCurrentEntry->nFlags & FDEMO_FADE_OUT_SLOW ) && !bInFadeout
			 && ft + 2.0 >= pCurrentEntry->fTrackTime + 0.02 )
		{
			bInFadeout = TRUE;
			sf_demo.duration = 8192;
			sf_demo.holdTime = 409;
			sf_demo.fadeFlags = FFADE_OUT;
			sf_demo.r = sf_demo.g = sf_demo.b = 0;
			sf_demo.a = 255;
			V_ScreenFade( NULL, 0, &sf_demo );
		}
	}

	CL_DemoReadCmdInfo( ft );
	CL_ReadNetchanState();

	if ( FS_Read( &msglen, sizeof( msglen ), 1, cls.demofile ) != (int)sizeof( msglen ) )
		Host_EndGame( "Bad demo length.\n" );

	msglen = LittleLong( msglen );
	if ( msglen < 0 )
		Host_EndGame( "Demo message length < 0.\n" );

	if ( msglen > MAX_POSSIBLE_MSG )
		Host_EndGame( "Demo message > MAX_POSSIBLE_MSG" );

	if ( msglen > 0 && FS_Read( msgbuf, msglen, 1, cls.demofile ) != msglen )
		Host_EndGame( "Error reading demo message data.\n" );

	// Simulate receipt in the message queue
	Q_memcpy( net_message.data, msgbuf, msglen );
	net_message.cursize = msglen;
	return TRUE;
}

qboolean CL_ReadDemoMessage( void )
{
	if ( !DemoPlayer_IsActive() )
		return CL_ReadDemoMessage_OLD();

	net_message.cursize = DemoPlayer_ReadDemoMessage( net_message.data, net_message.maxsize );
	if ( !net_message.cursize )
		return FALSE;

	DemoPlayer_ReadNetchanState( &cls.netchan.incoming_sequence, &cls.netchan.incoming_acknowledged,
		&cls.netchan.incoming_reliable_acknowledged, &cls.netchan.incoming_reliable_sequence,
		&cls.netchan.outgoing_sequence, &cls.netchan.reliable_sequence, &cls.netchan.last_reliable_sequence );
	return TRUE;
}
void CL_Record_f( void )
{
	int c, track, copysize, curpos, frame;
	char name[MAX_PATH];
	char mapname[MAX_PATH];
	char dllname[MAX_PATH];
	byte cmd;
	float f;

	if ( Cmd_Argc() < 2 )
	{
		Con_Printf( "record <demoname> <cd>\n" );
		return;
	}

	if ( cls.demorecording )
	{
		Con_Printf( "Already recording.\n" );
		return;
	}

	if ( cls.demoplayback )
	{
		Con_Printf( "Can't record during demo playback.\n" );
		return;
	}

	if ( !cls.demoheader || cls.state != ca_active )
	{
		Con_Printf(
			"You must be in an active map spawned on a machine that allows creation of files in %s\n", com_gamedir );
		return;
	}

	c = Cmd_Argc();
	if ( c != 2 && c != 3 )
	{
		Con_Printf( "record <demoname> <cd track>\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	track = -1;
	if ( c == 3 )
	{
		track = Q_atoi( Cmd_Argv( 2 ) );
		Con_Printf( "Forcing CD track to %i\n", track );
	}

	// open the demo file
	snprintf( name, sizeof( name ) - ( sizeof( ".dem" ) - 1 ), "%s", Cmd_Argv( 1 ) );
	COM_AddExtension( name, ".dem", sizeof( name ) );
	if ( !COM_HasFileExtension( name, "dem" ) )
	{
		Con_Printf( "Error: couldn't open demo file %s\n", name );
		return;
	}

	Con_Printf( "recording to %s.\n", name );
	cls.demofile = FS_OpenPathID( name, "wb", "GAMECONFIG" );
	if ( !cls.demofile )
	{
		Con_Printf( "Error: couldn't open demo file %s, demo files must have .dem extension.\n", name );
		return;
	}

	Q_memset( &demoheader, 0, sizeof( demoheader ) );
	Q_strncpy( demoheader.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) - 1 );

	COM_FileBase( cl.worldmodel->name, mapname );
	Q_strncpy( demoheader.szMapName, mapname, sizeof( demoheader.szMapName ) - 1 );
	demoheader.szMapName[sizeof( demoheader.szMapName ) - 1] = 0;

	Q_strncpy( dllname, com_gamedir, sizeof( dllname ) - 1 );
	dllname[sizeof( dllname ) - 1] = 0;
	COM_FileBase( dllname, demoheader.szDllDir );

	demoheader.mapCRC = cl.serverCRC;
	demoheader.nDemoProtocol = DEMO_PROTOCOL;
	demoheader.nNetProtocolVersion = PROTOCOL_VERSION;
	demoheader.nDirectoryOffset = 0;
	FS_Write( &demoheader, sizeof( demoheader ), 1, cls.demofile );

	Q_memset( &demodir, 0, sizeof( demodir ) );
	demodir.nEntries = 2;
	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * 2 );
	Q_memset( demodir.p_rgEntries, 0, sizeof( demoentry_t ) * demodir.nEntries );

	pCurrentEntry = demodir.p_rgEntries;
	Q_strcpy( pCurrentEntry->szDescription, "LOADING" );
	pCurrentEntry->nEntryType = DEMO_STARTUP;
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0.0f; // Startup takes 0 time.
	pCurrentEntry->nOffset = (int)FS_Tell( cls.demofile );

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = TRUE;
	cls.demorecording = TRUE;

	curpos = (int)FS_Tell( cls.demofile );

	// Finish off the startup info.
	cmd = DEMO_STOP;
	FS_Write( &cmd, sizeof( cmd ), 1, cls.demoheader );
	f = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	FS_Write( &f, sizeof( f ), 1, cls.demoheader );
	frame = LittleLong( host_framecount - cls.demostartframe );
	FS_Write( &frame, sizeof( frame ), 1, cls.demoheader );
	FS_Flush( cls.demoheader );

	// Now copy the stuff we cached from the server.
	copysize = (int)FS_Tell( cls.demoheader );
	FS_Seek( cls.demoheader, 0, FILESYSTEM_SEEK_HEAD );
	CL_DemoCopyFileChunk( (FILE*)cls.demofile, (FILE*)cls.demoheader, copysize );

	// Jump back to end, in case we record another demo for this session.
	FS_Seek( cls.demoheader, copysize, FILESYSTEM_SEEK_HEAD );

	cls.demoframecount = 0;
	cls.demostartframe = host_framecount;
	cls.demostarttime = CL_DemoOutTime();

	curpos = (int)FS_Tell( cls.demofile );
	pCurrentEntry->nFileLength = curpos - pCurrentEntry->nOffset;

	pCurrentEntry = &demodir.p_rgEntries[1];
	pCurrentEntry->nEntryType = DEMO_NORMAL;
	snprintf( pCurrentEntry->szDescription, sizeof( pCurrentEntry->szDescription ), "Playback" );
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0.0f;
	pCurrentEntry->nOffset = (int)FS_Tell( cls.demofile );

	// Demo playback should read this as an incoming message.
	// Write the client's realtime value out so we can synchronize the reads.
	cmd = DEMO_JUMPTIME;
	FS_Write( &cmd, sizeof( cmd ), 1, cls.demofile );
	f = LittleFloat( CL_DemoOutTime() - cls.demostarttime );
	FS_Write( &f, sizeof( f ), 1, cls.demofile );
	frame = LittleLong( host_framecount - cls.demostartframe );
	FS_Write( &frame, sizeof( frame ), 1, cls.demofile );

	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;
	cls.demoappending = FALSE;
}

void CL_AppendDemo_f( void )
{
	int c, track, copysize, frame;
	char name[MAX_PATH];
	char tempname[MAX_PATH];
	char mapname[MAX_PATH];
	FileHandle_t fp;
	byte cmd;
	float f;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Appenddemo only available when not running or recording a demo.\n" );
		return;
	}

	if ( cls.state != ca_active )
	{
		Con_Printf(
			"You must be in an active map spawned on a machine that allows creation of files in %s\n", com_gamedir );
		return;
	}

	c = Cmd_Argc();
	if ( c != 2 && c != 3 )
	{
		Con_Printf( "appenddemo <demoname> <cd track>\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	track = -1;
	if ( c == 3 )
	{
		track = Q_atoi( Cmd_Argv( 2 ) );
		Con_Printf( "Forcing CD track to %i\n", track );
	}

	// open the demo file
	snprintf( name, MAX_DEMO_NAME, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );
	if ( !COM_HasFileExtension( name, "dem" ) )
	{
		Con_Printf( "Error: couldn't open %s, demo files must have .dem extension.\n", name );
		return;
	}

	Con_Printf( "Appending to demo %s.\n", name );
	Q_strncpy( cls.demofilename, name, MAX_DEMO_NAME - 1 );
	cls.demofilename[MAX_DEMO_NAME - 1] = 0;

	fp = FS_OpenPathID( name, "rb", "GAMECONFIG" );
	if ( !fp )
	{
		Con_Printf( "Error:  couldn't open demo file %s for appending.\n", name );
		return;
	}

	COM_StripExtension( name, tempname );
	COM_AddExtension( tempname, ".dm2", sizeof( tempname ) );
	if ( !COM_HasFileExtension( tempname, "dm2" ) )
	{
		Con_Printf( "Error: could not open temporary file %s.\n", tempname );
		FS_Close( fp );
		return;
	}

	cls.demofile = FS_OpenPathID( tempname, "w+b", "GAMECONFIG" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open %s.\n", name );
		FS_Close( fp );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
	copysize = (int)FS_Tell( fp );
	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	CL_DemoCopyFileChunk( (FILE*)cls.demofile, (FILE*)fp, copysize );
	FS_Close( fp );

	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demoheader, sizeof( demoheader ), 1, cls.demofile );
	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) - 1 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		_unlink( tempname );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	COM_FileBase( cl.worldmodel->name, mapname );
	if ( Q_strcmp( demoheader.szMapName, mapname ) )
	{
		Con_Printf( "%s was recorded on a different map, cannot append.\n", name );
		_unlink( tempname );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		_unlink( tempname );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	// Now read in the directory structure.
	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( demodir.nEntries ), 1, cls.demofile );
	if ( demodir.nEntries < 1 || demodir.nEntries > MAX_DEMO_ENTRIES )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * ( demodir.nEntries + 1 ) );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, cls.demofile );

	nCurEntryIndex = demodir.nEntries++;
	pCurrentEntry = &demodir.p_rgEntries[nCurEntryIndex];
	Q_memset( pCurrentEntry, 0, sizeof( demoentry_t ) );

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );

	pCurrentEntry->nEntryType = DEMO_NORMAL;
	snprintf(
		pCurrentEntry->szDescription, sizeof( pCurrentEntry->szDescription ), "Playback '%i'", demodir.nEntries - 1 );
	pCurrentEntry->nFlags = 0;
	pCurrentEntry->nCDTrack = track;
	pCurrentEntry->fTrackTime = 0.0f;
	pCurrentEntry->nOffset = (int)FS_Tell( cls.demofile );

	cls.demostartframe = host_framecount;
	cls.demoframecount = 0;
	cls.demostarttime = (float)realtime;

	// Demo playback should read this as an incoming message.
	// Write the client's realtime value out so we can synchronize the reads.
	cmd = DEMO_JUMPTIME;
	FS_Write( &cmd, sizeof( cmd ), 1, cls.demofile );
	f = LittleFloat( (float)realtime - cls.demostarttime );
	FS_Write( &f, sizeof( f ), 1, cls.demofile );
	frame = LittleLong( host_framecount - cls.demostartframe );
	FS_Write( &frame, sizeof( frame ), 1, cls.demofile );

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = TRUE;
	cls.demorecording = TRUE;
	cls.td_startframe = host_framecount;
	cls.td_lastframe = -1;
	cls.demoappending = TRUE;
}

void CL_ListDemo_f( void )
{
	char name[MAX_PATH];
	char type[32];
	demoheader_t header;
	demodirectory_t dir;
	demoentry_t* entry;
	int i;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Listdemo only available when not running or recording.\n" );
		return;
	}

	// open the demo file
	snprintf( name, MAX_DEMO_NAME, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Demo contents for %s.\n", name );
	cls.demofile = FS_Open( name, "rb" );
	if ( !cls.demofile )
	{
		Con_Printf( "ERROR: couldn't open.\n" );
		return;
	}

	// Read in the header
	FS_Read( &header, sizeof( header ), 1, cls.demofile );
	if ( Q_strncmp( header.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) - 1 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	Con_Printf( "Demo file protocols %iN:%iD\nServer protocols at %iN:%iD\n", header.nNetProtocolVersion,
		header.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
	Con_Printf( "Map name :  %s\n", header.szMapName );
	Con_Printf( "Game .dll:  %s\n", header.szDllDir );

	// Now read in the directory structure.
	FS_Seek( cls.demofile, header.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &dir.nEntries, sizeof( dir.nEntries ), 1, cls.demofile );
	if ( dir.nEntries < 1 || dir.nEntries > MAX_DEMO_ENTRIES )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", dir.nEntries );
		FS_Close( cls.demofile );
		cls.demofile = NULL;
		return;
	}

	dir.p_rgEntries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * dir.nEntries );
	FS_Read( dir.p_rgEntries, sizeof( demoentry_t ) * dir.nEntries, 1, cls.demofile );

	Con_Printf( "\n" );

	for ( i = 0; i < dir.nEntries; i++ )
	{
		entry = &dir.p_rgEntries[i];

		snprintf( type, sizeof( type ), entry->nEntryType == DEMO_STARTUP ? "Start segment" : "Normal segment" );
		Con_Printf( "%i:  %s\n  \"%20s\"\nTime:  %.2f s.\nFrames:  %i\nSize:  %.2fK\n", i + 1, type,
			entry->szDescription, entry->fTrackTime, entry->nFrames, (float)entry->nFileLength / 1024.0 );

		if ( entry->nFlags )
			Con_Printf( "  Flags:\n" );
		if ( entry->nFlags & FDEMO_TITLE )
			Con_Printf( "     Title \"%s\"\n", entry->szDescription );
		if ( entry->nFlags & FDEMO_CDTRACK )
			Con_Printf( "     Play %i\n", entry->nCDTrack );
		if ( entry->nFlags & FDEMO_FADE_IN_SLOW )
			Con_Printf( "     Fade in slow\n" );
		if ( entry->nFlags & FDEMO_FADE_IN_FAST )
			Con_Printf( "     Fade in fast\n" );
		if ( entry->nFlags & FDEMO_FADE_OUT_SLOW )
			Con_Printf( "     Fade out slow\n" );
		if ( entry->nFlags & FDEMO_FADE_OUT_FAST )
			Con_Printf( "     Fade out fast\n" );

		Con_Printf( "\n" );
	}

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	Mem_Free( dir.p_rgEntries );
}
void CL_RemoveDemo_f( void )
{
	char name[MAX_PATH];
	char origname[MAX_PATH];
	char tempname[MAX_PATH];
	FileHandle_t fp;
	demoentry_t* entries;
	int segment, removed, count, i, j;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Removedemo only available when not running or recording a demo.\n" );
		return;
	}

	if ( Cmd_Argc() != 3 )
	{
		Con_Printf( "removedemo <demoname> <segment to remove>\nSegment 1 cannot be removed\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	segment = Q_atoi( Cmd_Argv( 2 ) );
	if ( segment <= 1 )
	{
		Con_Printf( "Cannot remove the STARTUP segment.\n" );
		return;
	}
	removed = segment - 1;

	// open the demo file
	snprintf( name, MAX_DEMO_NAME, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );
	if ( !COM_HasFileExtension( name, "dem" ) )
	{
		Con_Printf( "Error: couldn't open demo file %s, demo files must have .dem extension.\n", name );
		return;
	}

	Con_Printf( "Removing segment %i from demo %s.\n", segment, name );

	Q_strncpy( origname, name, MAX_DEMO_NAME - 1 );
	origname[MAX_DEMO_NAME - 1] = 0;

	fp = FS_Open( origname, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error: couldn't open demo file %s for segment removal.\n", name );
		return;
	}

	COM_StripExtension( name, tempname );
	COM_AddExtension( tempname, ".dm2", sizeof( tempname ) );

	if ( !COM_HasFileExtension( name, "dm2" ) )
	{
		Con_Printf( "Error: couldn't open temporary file %s.\n", name );
		FS_Close( fp );
		return;
	}

	cls.demofile = FS_OpenPathID( tempname, "w+b", "GAMECONFIG" );
	if ( !cls.demofile )
	{
		Con_Printf( "Error: couldn't open %s.\n", name );
		FS_Close( fp );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	FS_Read( &demoheader, sizeof( demoheader ), 1, fp );
	FS_Write( &demoheader, sizeof( demoheader ), 1, cls.demofile );

	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) - 1 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		goto failed;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		goto failed;
	}

	// Now read in the directory structure.
	FS_Seek( fp, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( demodir.nEntries ), 1, fp );
	if ( demodir.nEntries < 1 || demodir.nEntries > MAX_DEMO_ENTRIES )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		goto failed;
	}

	if ( demodir.nEntries == 1 )
	{
		Con_Printf( "ERROR: Can't remove last directory entry.\n" );
		goto failed;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, fp );

	count = demodir.nEntries - 1;
	entries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * count );
	for ( i = 0, j = 0; i < demodir.nEntries; i++ )
	{
		if ( i == removed )
			continue;

		memcpy( &entries[j], &demodir.p_rgEntries[i], sizeof( demoentry_t ) );
		entries[j].nOffset = (int)FS_Tell( cls.demofile );
		FS_Seek( fp, demodir.p_rgEntries[i].nOffset, FILESYSTEM_SEEK_HEAD );
		CL_DemoCopyFileChunk( (FILE*)cls.demofile, (FILE*)fp, entries[j].nFileLength );
		j++;
	}

	demoheader.nDirectoryOffset = (int)FS_Tell( cls.demofile );
	FS_Write( &count, sizeof( count ), 1, cls.demofile );
	for ( i = 0; i < count; i++ )
		FS_Write( &entries[i], sizeof( demoentry_t ), 1, cls.demofile );

	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demoheader, sizeof( demoheader ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	FS_Close( fp );

	Con_Printf( "Replacing old demo with edited version.\n" );
	_unlink( origname );
#if defined( _WIN32 )
	std::ignore = rename( tempname, origname );
#else
	if ( rename( tempname, origname ) != 0 )
		Con_Printf( "Couldn't replace demo %s\n", origname );
#endif

	Mem_Free( entries );
	Mem_Free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
	return;

failed:
	FS_Close( cls.demofile );
	FS_Close( fp );
	cls.demofile = NULL;
}

void CL_SwapDemo_f( void )
{
	char name[MAX_PATH];
	char origname[MAX_PATH];
	char tempname[MAX_PATH];
	FileHandle_t fp;
	demoentry_t temp;
	int seg1, seg2, idx1, idx2, copysize, i;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Swapdemo only available when not running or recording a demo.\n" );
		return;
	}

	if ( Cmd_Argc() != 4 )
	{
		Con_Printf( "Swapdemo <demoname> <seg#> <seg#>\nSwaps segments, segment 1 cannot be moved\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	seg1 = Q_atoi( Cmd_Argv( 2 ) );
	if ( seg1 <= 1 )
	{
		Con_Printf( "Cannot swap the STARTUP segment.\n" );
		return;
	}

	seg2 = Q_atoi( Cmd_Argv( 3 ) );
	idx1 = seg1 - 1;
	idx2 = seg2 - 1;

	// open the demo file
	snprintf( name, MAX_DEMO_NAME, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );
	if ( !COM_HasFileExtension( name, "dem" ) )
	{
		Con_Printf( "Error: couldn't open %s, demo files must have .dem extension.\n", name );
		return;
	}

	Con_Printf( "Swapping segment %i for %i from demo %s.\n", seg1, seg2, name );

	Q_strncpy( origname, name, MAX_DEMO_NAME - 1 );
	origname[MAX_DEMO_NAME - 1] = 0;

	fp = FS_Open( origname, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error: couldn't open demo file %s for swapping.\n", name );
		return;
	}

	COM_StripExtension( name, tempname );
	COM_AddExtension( tempname, ".dm2", sizeof( tempname ) );
	if ( !COM_HasFileExtension( tempname, "dm2" ) )
	{
		Con_Printf( "Error: couldn't open temporary file %s.\n", tempname );
		FS_Close( fp );
		return;
	}

	cls.demofile = FS_OpenPathID( tempname, "w+b", "GAMECONFIG" );
	if ( !cls.demofile )
	{
		Con_Printf( "Error: couldn't open %s.\n", name );
		FS_Close( fp );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
	copysize = (int)FS_Tell( fp );
	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	CL_DemoCopyFileChunk( (FILE*)cls.demofile, (FILE*)fp, copysize );

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	FS_Read( &demoheader, sizeof( demoheader ), 1, fp );
	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) - 1 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		goto failed;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		goto failed;
	}

	// Now read in the directory structure.
	FS_Seek( fp, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( demodir.nEntries ), 1, fp );
	if ( demodir.nEntries < 1 || demodir.nEntries > MAX_DEMO_ENTRIES )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		goto failed;
	}

	if ( idx1 >= demodir.nEntries || idx2 >= demodir.nEntries )
	{
		Con_Printf( "ERROR: demo only has %d entries\n", demodir.nEntries );
		goto failed;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, fp );

	memcpy( &temp, &demodir.p_rgEntries[idx1], sizeof( temp ) );
	memmove( &demodir.p_rgEntries[idx1], &demodir.p_rgEntries[idx2], sizeof( temp ) );
	memcpy( &demodir.p_rgEntries[idx2], &temp, sizeof( temp ) );

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demodir.nEntries, sizeof( demodir.nEntries ), 1, cls.demofile );
	for ( i = 0; i < demodir.nEntries; i++ )
		FS_Write( &demodir.p_rgEntries[i], sizeof( demoentry_t ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	FS_Close( fp );

	Con_Printf( "Replacing old demo with edited version.\n" );
	_unlink( origname );
#if defined( _WIN32 )
	std::ignore = rename( tempname, origname );
#else
	if ( rename( tempname, origname ) != 0 )
		Con_Printf( "Couldn't replace demo %s\n", origname );
#endif

	Mem_Free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
	return;

failed:
	FS_Close( cls.demofile );
	FS_Close( fp );
	_unlink( tempname );
	cls.demofile = NULL;
}
void CL_SetDemoInfo_f( void )
{
	char name[MAX_PATH];
	char origname[MAX_PATH];
	char tempname[MAX_PATH];
	FileHandle_t fp;
	demoentry_t* entry;
	const char* opt;
	const char* arg;
	int segment, copysize, c, i;
	int slowflag, fastflag;

	if ( cmd_source != src_command )
		return;

	if ( cls.demorecording || cls.demoplayback )
	{
		Con_Printf( "Setdemoinfo only available when not running or recording a demo.\n" );
		return;
	}

	if ( Cmd_Argc() < 3 )
	{
		Con_Printf( "Setdemoinfo <demoname> <seg#> <info ...>\n" );
		Con_Printf( "   title \"text\"\n" );
		Con_Printf( "   play tracknum\n" );
		Con_Printf( "   fade <in | out> <fast | slow>\n\n" );
		Con_Printf( "Use -option to disable, e.g., -title\n" );
		return;
	}

	if ( Q_strstr( Cmd_Argv( 1 ), ".." ) )
	{
		Con_Printf( "Relative pathnames are not allowed.\n" );
		return;
	}

	segment = Q_atoi( Cmd_Argv( 2 ) );
	if ( segment <= 1 )
	{
		Con_Printf( "Cannot Setdemoinfo the STARTUP segment.\n" );
		return;
	}

	// open the demo file
	snprintf( name, MAX_DEMO_NAME, "%s", Cmd_Argv( 1 ) );
	COM_DefaultExtension( name, ".dem" );
	if ( !COM_HasFileExtension( name, "dem" ) )
	{
		Con_Printf( "Error: couldn't open file %s, demo files must have .dem extension.\n", name );
		return;
	}

	Con_Printf( "Setting info for segment %i in demo %s.\n", segment, name );

	Q_strncpy( origname, name, MAX_DEMO_NAME - 1 );
	origname[MAX_DEMO_NAME - 1] = 0;

	fp = FS_Open( origname, "rb" );
	if ( !fp )
	{
		Con_Printf( "Error: couldn't open demo file %s for Setdemoinfo.\n", name );
		return;
	}

	COM_StripExtension( name, tempname );
	COM_AddExtension( tempname, ".dm2", sizeof( tempname ) );
	if ( !COM_HasFileExtension( tempname, "dm2" ) )
	{
		Con_Printf( "Error: couldn't open temporary file %s.\n", tempname );
		FS_Close( fp );
	}

	cls.demofile = FS_OpenPathID( tempname, "w+b", "GAMECONFIG" );
	if ( !cls.demofile )
	{
		Con_Printf( "Error: couldn't open %s.\n", name );
		FS_Close( fp );
		return;
	}

	FS_Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
	copysize = (int)FS_Tell( fp );
	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );
	CL_DemoCopyFileChunk( (FILE*)cls.demofile, (FILE*)fp, copysize );

	FS_Seek( fp, 0, FILESYSTEM_SEEK_HEAD );
	FS_Seek( cls.demofile, 0, FILESYSTEM_SEEK_HEAD );

	FS_Read( &demoheader, sizeof( demoheader ), 1, fp );
	if ( Q_strncmp( demoheader.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) - 1 ) )
	{
		Con_Printf( "%s is not a demo file\n", name );
		goto failed;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != DEMO_PROTOCOL )
	{
		Con_Printf( "ERROR: demo protocol outdated\nDemo file protocols %iN:%iD\nServer protocol is at %iN:%iD\n",
			demoheader.nNetProtocolVersion, demoheader.nDemoProtocol, PROTOCOL_VERSION, DEMO_PROTOCOL );
		goto failed;
	}

	// Now read in the directory structure.
	FS_Seek( fp, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Read( &demodir.nEntries, sizeof( demodir.nEntries ), 1, fp );
	if ( demodir.nEntries < 1 || demodir.nEntries > MAX_DEMO_ENTRIES )
	{
		Con_Printf( "ERROR: demo had bogus # of directory entries:  %i\n", demodir.nEntries );
		goto failed;
	}

	demodir.p_rgEntries = (demoentry_t*)Mem_Malloc( sizeof( demoentry_t ) * demodir.nEntries );
	FS_Read( demodir.p_rgEntries, sizeof( demoentry_t ) * demodir.nEntries, 1, fp );

	entry = &demodir.p_rgEntries[segment - 1];
	c = 3;
	for ( ;; )
	{
		opt = Cmd_Argv( c );
		if ( !opt || !opt[0] )
			break;
		c++;

		if ( !Q_strcasecmp( opt, "-TITLE" ) )
		{
			entry->nFlags &= ~FDEMO_TITLE;
		}
		else if ( !Q_strcasecmp( opt, "-PLAY" ) )
		{
			entry->nFlags &= ~FDEMO_CDTRACK;
		}
		else if ( !Q_strcasecmp( opt, "-FADE" ) )
		{
			entry->nFlags &= ~( FDEMO_FADE_IN_SLOW | FDEMO_FADE_IN_FAST | FDEMO_FADE_OUT_SLOW | FDEMO_FADE_OUT_FAST );
		}
		else if ( !Q_strcasecmp( opt, "TITLE" ) )
		{
			arg = Cmd_Argv( c++ );
			if ( arg && arg[0] )
			{
				Q_strncpy( entry->szDescription, arg, sizeof( entry->szDescription ) - 1 );
				entry->szDescription[sizeof( entry->szDescription ) - 1] = 0;
				entry->nFlags |= FDEMO_TITLE;
			}
			else
			{
				Con_Printf( "Title flag requires a double-quoted value.\n" );
			}
		}
		else if ( !Q_strcasecmp( opt, "PLAY" ) )
		{
			arg = Cmd_Argv( c++ );
			if ( arg && arg[0] )
			{
				entry->nCDTrack = Q_atoi( arg );
				entry->nFlags |= FDEMO_CDTRACK;
			}
			else
			{
				Con_Printf( "Play flag requires a cd track #.\n" );
			}
		}
		else if ( !Q_strcasecmp( opt, "FADE" ) )
		{
			arg = Cmd_Argv( c++ );
			if ( !arg || !arg[0] )
			{
				Con_Printf( "Fade flag requires a direction and speed (in fast, e.g.)\n" );
				continue;
			}

			if ( !Q_strcasecmp( arg, "IN" ) )
			{
				slowflag = FDEMO_FADE_IN_SLOW;
				fastflag = FDEMO_FADE_IN_FAST;
			}
			else if ( !Q_strcasecmp( arg, "OUT" ) )
			{
				slowflag = FDEMO_FADE_OUT_SLOW;
				fastflag = FDEMO_FADE_OUT_FAST;
			}
			else
			{
				Con_Printf( "Fade flag requires a direction (in or out).\n" );
				continue;
			}

			arg = Cmd_Argv( c++ );
			if ( arg && arg[0] && !Q_strcasecmp( arg, "FAST" ) )
				entry->nFlags |= fastflag;
			else if ( arg && arg[0] && !Q_strcasecmp( arg, "SLOW" ) )
				entry->nFlags |= slowflag;
			else
				Con_Printf( "Fade flag requires a speed (fast or slow).\n" );
		}
		else
		{
			Con_Printf( "Setdemoinfo, unrecognized flag:  %s\n", opt );
			break;
		}
	}

	FS_Seek( cls.demofile, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	FS_Write( &demodir.nEntries, sizeof( demodir.nEntries ), 1, cls.demofile );
	for ( i = 0; i < demodir.nEntries; i++ )
		FS_Write( &demodir.p_rgEntries[i], sizeof( demoentry_t ), 1, cls.demofile );

	FS_Close( cls.demofile );
	cls.demofile = NULL;
	FS_Close( fp );

	Con_Printf( "Replacing old demo with edited version.\n" );
	_unlink( origname );
#if defined( _WIN32 )
	std::ignore = rename( tempname, origname );
#else
	if ( rename( tempname, origname ) != 0 )
		Con_Printf( "Couldn't replace demo %s\n", origname );
#endif

	Mem_Free( demodir.p_rgEntries );
	demodir.p_rgEntries = NULL;
	demodir.nEntries = 0;
	return;

failed:
	FS_Close( cls.demofile );
	FS_Close( fp );
	_unlink( tempname );
	cls.demofile = NULL;
}

void CL_ViewDemo_f( void )
{
	char name[MAX_PATH];

	if ( cmd_source != src_command )
		return;

	DemoPlayer_SetActive( TRUE );
	if ( !DemoPlayer_IsActive() )
	{
		Con_Printf( "viewdemo not available.\n" );
		return;
	}

	DemoPlayer_ShowGUI();

	if ( Cmd_Argc() != 2 )
	{
		Con_Printf( "viewdemo <demoname>: shows a demo\n" );
		return;
	}

	CL_Disconnect();

	Q_strncpy( name, Cmd_Argv( 1 ), sizeof( name ) - sizeof( ".dem" ) );
	COM_DefaultExtension( name, ".dem" );

	Con_Printf( "Playing demo from %s.\n", name );
	if ( !DemoPlayer_LoadGame( name ) )
	{
		Con_Printf( "Viewdemo: couldn't load demo %s.\n", name );
		cls.demonum = -1;
		return;
	}

	cls.demoplayback = TRUE;
	cls.state = ca_connected;
	cls.passive = FALSE;
	cls.spectator = FALSE;

	Netchan_Setup( NS_CLIENT, &cls.netchan, net_from, -1, &cls, CL_GetFragmentSize );
	NET_ClearLagData( true, false );

	cls.lastoutgoingcommand = -1;
	cls.nextcmdtime = (float)realtime;

	DispatchDirectUserMsg( "InitHUD", 0, NULL );
}
