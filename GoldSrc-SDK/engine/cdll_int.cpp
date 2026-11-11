#include "engine_apiproxy.h"
#include "client.h"
#include "common.h"
#include "cvar.h"
#include "gl_model.h"
#include "vgui_int.h"
#include "info.h"
#include "cmd.h"
#include "protocol.h"
#include "cl_demo.h"
#include "vgui_intwrap2.h"
#include "sys.h"
#include "conprint.h"
#include "blob.h"
#include "filesystem.h"
#include "pm_defs.h"
#include "engine_studio.h"
#include "net_api.h"
#include "vid.h"
#include "cl_draw.h"
#include "glquake.h"
#include "tmessage.h"
#include "keys.h"
#include "view.h"
#include "server.h"
#include "usermsg.h"
#include "ivoicetweak.h"
#include "cd.h"
#include "interface.h"
#include "ivideomode.h"
#include "enginecallbacks.h"
#include "../external/sdl2/SDL.h"
#include "../common/event_flags.h"
#include "../common/entity_types.h"
#include "../common/triangleapi.h"
#include "../common/event_api.h"
#include "../common/demo_api.h"
#include "../common/r_studioint.h"

#include <cstdlib>
#include <cstring>
#if !defined( _WIN32 )
#include <dlfcn.h>
#endif

#if defined( _WIN32 )
#define CDLL_INT_SOURCE_PATH "c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\cdll_int.c"
#else
#define CDLL_INT_SOURCE_PATH "../engine/cdll_int.c"
#endif

extern int window_center_x, window_center_y;
extern cvar_t sv_gravity;
extern cvar_t* cl_righthand;
extern cl_enginefunc_t cl_enginefuncs;
extern playermove_t g_clmove;
extern IVoiceTweak g_VoiceTweakAPI;
extern SDL_Window** pmainwindow;

model_t* R_LoadMapSprite( const char* filename );
extern "C" void SetMouseEnable( qboolean enabled );
model_t* SPR_GetModelPointer( HSPRITE sprite );
void SPR_Init();
void SPR_Shutdown();
void ClientDLL_CheckStudioInterface( HINSTANCE clientModule );

cldll_func_t cl_funcs;
qboolean fClientLoaded;

cvar_t* joy_supported;
cvar_t* joy_forwardsensitivity;
cvar_t* joy_sidesensitivity;
cvar_t* joy_pitchsensitivity;
cvar_t* joy_yawsensitivity;

HINSTANCE hClientDLL;
BlobFootprint_t g_blobfootprintClient;
char g_szfullClientName[512];

cmdalias_t* GetAliasesList()
{
	return cmd_alias;
}

int GetTrackerIDForPlayer( int playerSlot )
{
	g_engdstAddrs.GetTrackerIDForPlayer( &playerSlot );
	return 0;
}

int GetPlayerForTrackerID( int trackerID )
{
	g_engdstAddrs.GetPlayerForTrackerID( &trackerID );
	return 0;
}

cvar_t* GetFirstCVarPtr()
{
	return cvar_vars;
}

unsigned int GetFirstCmdFunctionHandle()
{
	return (unsigned int)Cmd_GetFirstCmd();
}

unsigned int GetNextCmdFunctionHandle( unsigned int handle )
{
	return (unsigned int)( (cmd_function_t*)handle )->next;
}

const char* GetCmdFunctionName( unsigned int handle )
{
	return ( (cmd_function_t*)handle )->name;
}

char* COM_ParseFile( char* data, char* token )
{
	g_engdstAddrs.COM_ParseFile( &data, &token );

	data = (char*)COM_Parse( data );
	Q_strncpy( token, com_token, 1024 );
	return data;
}

int ClientDLL_ExpandFileName( const char* filename, char* buffer, int bufferSize )
{
	char path[512];

	g_engdstAddrs.COM_ExpandFilename( &filename, &buffer, &bufferSize );

	Q_strncpy( path, filename, sizeof( path ) - 1 );
	path[sizeof( path ) - 1] = '\0';

	if ( !COM_ExpandFilename( path ) )
		return 0;

	COM_FixSlashes( path );
	Q_strncpy( buffer, path, bufferSize );
	buffer[bufferSize - 1] = '\0';
	return 1;
}

const char* PlayerInfo_ValueForKey( int playerNum, const char* key )
{
	player_info_t* player;

	g_engdstAddrs.PlayerInfo_ValueForKey( &playerNum, &key );

	if ( playerNum > cl.maxclients || playerNum < 1 )
		return NULL;

	player = &cl.players[playerNum - 1];
	if ( !player->name[0] )
		return NULL;

	return Info_ValueForKey( player->userinfo, key );
}

const char* LocalPlayerInfo_ValueForKey( const char* key )
{
	g_engdstAddrs.LocalPlayerInfo_ValueForKey( &key );
	return Info_ValueForKey( cls.userinfo, key );
}

void PlayerInfo_SetValueForKey( const char* key, const char* value )
{
	if ( !strcmp( Info_ValueForKey( cls.userinfo, key ), value ) )
		return;

	g_engdstAddrs.PlayerInfo_SetValueForKey( &key, &value );
	Info_SetValueForKey( cls.userinfo, key, value, sizeof( cls.userinfo ) );

	if ( cls.state > ca_connecting )
	{
		MSG_WriteByte( &cls.netchan.message, clc_stringcmd );
		SZ_Print( &cls.netchan.message, va( "setinfo \"%s\" \"%s\"\n", key, value ) );
	}
}

qboolean GetPlayerUniqueID( int playerNum, char* playerID )
{
	player_info_t* player;

	g_engdstAddrs.GetPlayerUniqueID( &playerNum, &playerID );

	playerNum -= 1;
	if ( playerNum < 0 || playerNum >= cl.maxclients )
		return FALSE;

	player = &cl.players[playerNum];
	if ( !player->userinfo[0] || !player->name[0] )
		return FALSE;

	Q_memcpy( playerID, player->hashedcdkey, sizeof( player->hashedcdkey ) );
	return TRUE;
}

int hudIsNoClipping()
{
	g_engdstAddrs.IsNoClipping();
	return cl.frames[cl.parsecountmod].playerstate[cl.playernum].movetype == MOVETYPE_NOCLIP;
}

cl_entity_t* hudGetLocalPlayer()
{
	g_engdstAddrs.GetLocalPlayer();
	if ( !cl_entities )
		return NULL;
	return &cl_entities[cl.playernum + 1];
}

cl_entity_t* hudGetViewModel()
{
	g_engdstAddrs.GetViewModel();
	return &cl.viewent;
}

cl_entity_t* hudGetEntityByIndex( int idx )
{
	g_engdstAddrs.GetEntityByIndex( &idx );

	if ( idx < 0 || idx >= cl.max_edicts )
		return NULL;

	return &cl_entities[idx];
}

model_t* CL_LoadModel( const char* modelname, int* index )
{
	int i;

	g_engdstAddrs.CL_LoadModel( &modelname, &index );

	for ( i = 1; cl.model_precache[i]; i++ )
	{
		if ( !Q_strcasecmp( modelname, cl.model_precache[i]->name ) )
		{
			model_t* model = CL_GetModelByIndex( i );
			if ( index )
				*index = i;
			return model;
		}
	}

	if ( index )
		*index = -1;
	return NULL;
}

const model_t* hudGetSpritePointer( HSPRITE hSprite )
{
	g_engdstAddrs.GetSpritePointer( &hSprite );
	return SPR_GetModelPointer( hSprite );
}

float hudGetClientOldTime()
{
	return (float)cl.oldtime;
}

float hudGetServerGravityValue()
{
	return sv_gravity.value;
}

model_t* hudGetModelByIndex( int index )
{
	return CL_GetModelByIndex( index );
}

const char* hudPhysInfo_ValueForKey( const char* key )
{
	g_engdstAddrs.PhysInfo_ValueForKey( &key );
	return Info_ValueForKey( cls.physinfo, key );
}

const char* hudServerInfo_ValueForKey( const char* key )
{
	g_engdstAddrs.ServerInfo_ValueForKey( &key );
	return Info_ValueForKey( cl.serverinfo, key );
}

int hudCenterX()
{
	g_engdstAddrs.GetWindowCenterX();
	return window_center_x;
}

int hudCenterY()
{
	g_engdstAddrs.GetWindowCenterY();
	return window_center_y;
}

void hudGetViewAngles( float* angles )
{
	g_engdstAddrs.GetViewAngles( &angles );
	VectorCopy( cl.viewangles, angles );
}

void hudSetViewAngles( float* angles )
{
	g_engdstAddrs.SetViewAngles( &angles );
	VectorCopy( angles, cl.viewangles );
}

int hudGetMaxClients()
{
	g_engdstAddrs.GetMaxClients();
	return cl.maxclients;
}

float hudGetClientTime()
{
	g_engdstAddrs.GetClientTime();
	return (float)cl.time;
}

float hudGetClientMaxspeed()
{
	g_engdstAddrs.GetClientMaxspeed();
	return cl.maxspeed;
}

const char* hudGetGameDir()
{
	g_engdstAddrs.pfnGetGameDirectory();
	return com_gamedir;
}

const char* hudGetLevelName()
{
	g_engdstAddrs.pfnGetLevelName();

	if ( cls.state < ca_connected )
		return com_empty_string;

	return cl.levelname;
}

void hudGetScreenFade( screenfade_t* fade )
{
	g_engdstAddrs.pfnGetScreenFade( &fade );
	*fade = cl.sf;
}

void hudSetScreenFade( screenfade_t* fade )
{
	g_engdstAddrs.pfnSetScreenFade( &fade );
	cl.sf = *fade;
}

client_sprite_t* SPR_GetList( char* psz, int* piCount )
{
	const char* pfile;
	int iCount, i;
	client_sprite_t *ps, *pret;

	g_engdstAddrs.pfnSPR_GetList( &psz, &piCount );

	pfile = (const char*)COM_LoadFile( psz, 2, NULL );
	if ( !pfile )
		return NULL;

	pfile = COM_Parse( pfile );
	iCount = atoi( com_token );
	if ( !iCount )
		return NULL;

	pret = (client_sprite_t*)calloc( sizeof( client_sprite_t ) * iCount, 1 );
	if ( !pret )
		return NULL;

	ps = pret;
	for ( i = 0; i < iCount; i++, ps++ )
	{
		pfile = COM_Parse( pfile );
		Q_strncpy( ps->szName, com_token, sizeof( ps->szName ) - 1 );
		ps->szName[sizeof( ps->szName ) - 1] = '\0';

		pfile = COM_Parse( pfile );
		ps->iRes = atoi( com_token );

		pfile = COM_Parse( pfile );
		Q_strncpy( ps->szSprite, com_token, sizeof( ps->szSprite ) - 1 );
		ps->szSprite[sizeof( ps->szSprite ) - 1] = '\0';

		pfile = COM_Parse( pfile );
		ps->rc.left = atoi( com_token );
		pfile = COM_Parse( pfile );
		ps->rc.top = atoi( com_token );
		pfile = COM_Parse( pfile );
		ps->rc.right = ps->rc.left + atoi( com_token );
		pfile = COM_Parse( pfile );
		ps->rc.bottom = ps->rc.top + atoi( com_token );
	}

	if ( piCount )
		*piCount = iCount;

	return pret;
}

void ClientDLL_ReadDemoBuffer( int size, byte* buffer )
{
	if ( cl_funcs.pReadDemoBuffer )
		cl_funcs.pReadDemoBuffer( size, buffer );
}

struct kbutton_s* ClientDLL_FindKey( const char* name )
{
	if ( cl_funcs.pFindKey )
		return cl_funcs.pFindKey( name );
	return NULL;
}

int ClientDLL_Key_Event( int down, int keynum, const char* pszCurrentBinding )
{
	if ( !VGui_Key_Event( down, keynum, pszCurrentBinding ) )
		return 0;

	if ( cl_funcs.pKeyEvent )
		return cl_funcs.pKeyEvent( down, keynum, pszCurrentBinding );
	return 1;
}

void ClientDLL_DemoUpdateClientData( client_data_t* cdat )
{
	if ( cl_funcs.pHudUpdateClientDataFunc && cl_funcs.pHudUpdateClientDataFunc( cdat, (float)cl.time )
		 && !cls.spectator )
	{
		VectorCopy( cdat->viewangles, cl.viewangles );
		scr_fov_value = cdat->fov;
	}
}

void ClientDLL_ClearStates( void )
{
	if ( fClientLoaded && cl_funcs.pIN_ClearStates )
		cl_funcs.pIN_ClearStates();
}

void ClientDLL_CAM_Think()
{
	if ( cls.state == ca_dedicated || cls.state == ca_disconnected )
		return;

	if ( cl_funcs.pCamThink )
		cl_funcs.pCamThink();
}

void ClientDLL_ActivateMouse()
{
	if ( fClientLoaded && cl_funcs.pIN_ActivateMouse )
		cl_funcs.pIN_ActivateMouse();
}

void ClientDLL_DeactivateMouse()
{
	if ( fClientLoaded && cl_funcs.pIN_DeactivateMouse )
		cl_funcs.pIN_DeactivateMouse();
}

void ClientDLL_IN_Accumulate()
{
	if ( fClientLoaded && cl_funcs.pIN_Accumulate )
		cl_funcs.pIN_Accumulate();
}

void ClientDLL_Frame( double time )
{
	if ( fClientLoaded && cl_funcs.pHudFrame )
		cl_funcs.pHudFrame( time );
}

void ClientDLL_MouseEvent( int mstate )
{
	if ( fClientLoaded && cl_funcs.pIN_MouseEvent )
		cl_funcs.pIN_MouseEvent( mstate );
}

int ClientDLL_IsThirdPerson()
{
	if ( cl_funcs.pCL_IsThirdPerson )
		return cl_funcs.pCL_IsThirdPerson();
	return 0;
}

void ClientDLL_ChatInputPosition( int* x, int* y )
{
	if ( cl_funcs.pChatInputPosition )
		cl_funcs.pChatInputPosition( x, y );
}

void ClientDLL_MoveClient( struct playermove_s* ppmove )
{
	if ( cl_funcs.pClientMove )
		cl_funcs.pClientMove( ppmove, 0 );
}

void ClientDLL_GetCameraOffsets( float* ofs )
{
	if ( cl_funcs.pCL_GetCameraOffsets )
		cl_funcs.pCL_GetCameraOffsets( ofs );
}

void ClientDLL_ClientMoveInit( struct playermove_s* ppmove )
{
	if ( cl_funcs.pClientMoveInit )
		cl_funcs.pClientMoveInit( ppmove );
}

char ClientDLL_ClientTextureType( char* name )
{
	if ( cl_funcs.pClientTextureType )
		return cl_funcs.pClientTextureType( name );
	return 0;
}

int ClientDLL_GetHullBounds( int hullnumber, float* mins, float* maxs )
{
	if ( cl_funcs.pGetHullBounds )
		return cl_funcs.pGetHullBounds( hullnumber, mins, maxs );
	return 0;
}

struct cl_entity_s* ClientDLL_GetUserEntity( int index )
{
	if ( cl_funcs.pGetUserEntity )
		return cl_funcs.pGetUserEntity( index );
	return NULL;
}

void ClientDLL_VoiceStatus( int entindex, qboolean bTalking )
{
	if ( cl_funcs.pVoiceStatus )
		cl_funcs.pVoiceStatus( entindex, bTalking );
}

void ClientDLL_DirectorMessage( int iSize, void* pbuf )
{
	if ( cl_funcs.pDirectorMessage )
		cl_funcs.pDirectorMessage( iSize, pbuf );
}

void ClientDLL_CreateMove( float frametime, struct usercmd_s* cmd, int active )
{
	if ( !joy_supported || !joy_supported->value )
	{
		if ( joy_forwardsensitivity && joy_forwardsensitivity->value )
			Cvar_Set( "joyforwardsensitivity", "0" );
		if ( joy_sidesensitivity && joy_sidesensitivity->value )
			Cvar_Set( "joysidesensitivity", "0" );
		if ( joy_pitchsensitivity && joy_pitchsensitivity->value )
			Cvar_Set( "joypitchsensitivity", "0" );
		if ( joy_yawsensitivity && joy_yawsensitivity->value )
			Cvar_Set( "joyyawsensitivity", "0" );
	}

	if ( cl_funcs.pCL_CreateMove )
		cl_funcs.pCL_CreateMove( frametime, cmd, active );
}

void ClientDLL_PostRunCmd( struct local_state_s* from, struct local_state_s* to, struct usercmd_s* cmd, int runfuncs,
	double time, unsigned int random_seed )
{
	if ( cl_funcs.pPostRunCmd )
		cl_funcs.pPostRunCmd( from, to, cmd, runfuncs, time, random_seed );
	else if ( cl_lw.value )
		Cvar_SetValue( "cl_lw", 0.0f );
}

void ClientDLL_CreateEntities()
{
	if ( cl_funcs.pCreateEntities )
		cl_funcs.pCreateEntities();
}

void ClientDLL_DrawNormalTriangles()
{
	if ( cl_funcs.pDrawNormalTriangles )
		cl_funcs.pDrawNormalTriangles();
	tri.RenderMode( kRenderNormal );
}

void ClientDLL_DrawTransparentTriangles()
{
	if ( cl_funcs.pDrawTransparentTriangles )
		cl_funcs.pDrawTransparentTriangles();
	tri.RenderMode( kRenderNormal );
}

void ClientDLL_CalcRefdef( struct ref_params_s* pparams )
{
	if ( cl_funcs.pCalcRefdef )
		cl_funcs.pCalcRefdef( pparams );
}

void ClientDLL_StudioEvent( const struct mstudioevent_s* event, const struct cl_entity_s* entity )
{
	if ( cl_funcs.pStudioEvent )
		cl_funcs.pStudioEvent( event, entity );
}

void ClientDLL_TxferLocalOverrides( struct entity_state_s* state, const struct clientdata_s* client )
{
	if ( cl_funcs.pTxferLocalOverrides )
		cl_funcs.pTxferLocalOverrides( state, client );
}

void ClientDLL_ProcessPlayerState( struct entity_state_s* dst, const struct entity_state_s* src )
{
	if ( cl_funcs.pProcessPlayerState )
		cl_funcs.pProcessPlayerState( dst, src );
}

void ClientDLL_TxferPredictionData( struct entity_state_s* ps, const struct entity_state_s* pps,
	struct clientdata_s* pcd, const struct clientdata_s* ppcd, struct weapon_data_s* wd,
	const struct weapon_data_s* pwd )
{
	if ( cl_funcs.pTxferPredictionData )
		cl_funcs.pTxferPredictionData( ps, pps, pcd, ppcd, wd, pwd );
}

int ClientDLL_ConnectionlessPacket(
	const struct netadr_s* source, const char* args, char* response_buffer, int* response_buffer_size )
{
	if ( cl_funcs.pConnectionlessPacket )
		return cl_funcs.pConnectionlessPacket( source, args, response_buffer, response_buffer_size );
	return 0;
}

static int CL_AddVisibleEntity( int type, cl_entity_t* ent )
{
	if ( type == ET_BEAM )
	{
		if ( cl_numbeamentities >= MAX_BEAMENTS )
			return 0;
		cl_beamentities[cl_numbeamentities++] = ent;
	}
	else
	{
		if ( cl_numvisedicts >= MAX_VISEDICTS )
			return 0;
		cl_visedicts[cl_numvisedicts++] = ent;
	}
	return 1;
}

int ClientDLL_AddEntity( int type, cl_entity_s* ent )
{
	const char* modelname;
	vec3_t mins, maxs;

	if ( !cl_funcs.pAddEntity )
		return 0;

	modelname = ent->model ? ent->model->name : com_empty_string;
	if ( !cl_funcs.pAddEntity( type, ent, modelname ) )
		return 0;

	if ( CL_IsSpectateOnly() && !CL_IsDevOverviewMode() && type != ET_TEMPENTITY && type != ET_BEAM
		 && ent->curstate.movetype != MOVETYPE_FOLLOW )
	{
		VectorAdd( ent->origin, ent->model->mins, mins );
		VectorAdd( ent->origin, ent->model->maxs, maxs );
		if ( !PVSNode( cl.worldmodel->nodes, mins, maxs ) )
			return 0;
	}

	return CL_AddVisibleEntity( type, ent );
}

void ClientDLL_HudInit()
{
	if ( !cl_funcs.pHudInitFunc )
		Sys_Error( CDLL_INT_SOURCE_PATH ", line %d: could not link client DLL for HUD initialization", 1126 );

	cl_funcs.pHudInitFunc();
	ClientDLL_CheckStudioInterface( hClientDLL );

	cl_righthand = Cvar_FindVar( "cl_righthand" );
	joy_supported = Cvar_FindVar( "joysupported" );
	joy_forwardsensitivity = Cvar_FindVar( "joyforwardsensitivity" );
	joy_sidesensitivity = Cvar_FindVar( "joysidesensitivity" );
	joy_pitchsensitivity = Cvar_FindVar( "joypitchsensitivity" );
	joy_yawsensitivity = Cvar_FindVar( "joyyawsensitivity" );
}

void ClientDLL_CheckStudioInterface( HINSTANCE hModule )
{
	R_ResetStudio();

	if ( !fClientLoaded )
		return;

	Sys_GetProcAddress( hModule, "HUD_GetStudioModelInterface" );
	if ( cl_funcs.pStudioInterface
		 && !cl_funcs.pStudioInterface( STUDIO_INTERFACE_VERSION, &pStudioAPI, &engine_studio_api ) )
	{
		Con_DPrintf( "Couldn't get client .dll studio model rendering interface.  Version mismatch?\n" );
		R_ResetStudio();
	}
}

void ClientDLL_HudRedraw( int intermission )
{
	if ( !VGuiWrap2_IsGameUIVisible() && cl_funcs.pHudRedrawFunc )
		cl_funcs.pHudRedrawFunc( (float)cl.time, intermission );
}

void ClientDLL_HudVidInit()
{
	if ( !cl_funcs.pHudVidInitFunc )
		Sys_Error( CDLL_INT_SOURCE_PATH ", line %d: could not link client DLL for HUD Vid initialization", 1107 );

	SPR_Init();
	cl_funcs.pHudVidInitFunc();
}

void ClientDLL_TempEntUpdate( double frametime, double client_time, double cl_gravity, tempent_s** ppTempEntFree,
	tempent_s** ppTempEntActive, int ( *Callback_AddVisibleEntity )( cl_entity_s* ),
	void ( *Callback_TempEntPlaySound )( tempent_s*, float ) )
{
	if ( cl_funcs.pTempEntUpdate )
		cl_funcs.pTempEntUpdate( frametime, client_time, cl_gravity, ppTempEntFree, ppTempEntActive,
			Callback_AddVisibleEntity, Callback_TempEntPlaySound );
}

void ClientDLL_UpdateClientData()
{
	client_data_t cdat;
	client_data_t oldcdat;

	if ( cls.state != ca_active || cls.demoplayback )
		return;

	Q_memset( &cdat, 0, sizeof( cdat ) );

	VectorCopy( cl.viewangles, cdat.viewangles );
	VectorCopy( cl_entities[cl.viewentity].origin, cdat.origin );

	cdat.iWeaponBits = cl.weapons;
	cdat.fov = scr_fov_value;

	if ( cls.demorecording )
		oldcdat = cdat;

	if ( cl_funcs.pHudUpdateClientDataFunc && cl_funcs.pHudUpdateClientDataFunc( &cdat, (float)cl.time ) )
	{
		VectorCopy( cdat.viewangles, cl.viewangles );
		scr_fov_value = cdat.fov;
	}

	if ( cls.demorecording )
		CL_WriteDLLUpdate( &oldcdat );
}

unsigned short hudPrecacheEvent( int type, const char* psz )
{
	int i;

	g_engdstAddrs.pfnPrecacheEvent( &type, &psz );

	for ( i = 1; cl.event_precache[i].filename; i++ )
	{
		if ( !Q_strcasecmp( cl.event_precache[i].filename, psz ) )
			return (unsigned short)i;
	}

	return 0;
}

void hudWeaponAnim( int iAnim, int body )
{
	g_engdstAddrs.pfnWeaponAnim( &iAnim, &body );

	cl.weaponsequence = iAnim;
	cl.weaponstarttime = 0.0f;
	cl.viewent.curstate.body = body;

	if ( cls.demorecording )
		CL_DemoAnim( iAnim, body );
}

void COM_AddAppDirectoryToSearchPath( const char* pszBaseDir, const char* appName )
{
	g_engdstAddrs.COM_AddAppDirectoryToSearchPath( &pszBaseDir, &appName );
	COM_AddAppDirectory( (char*)pszBaseDir, appName );
}

void ClientDLL_Shutdown()
{
	Net_APIShutDown();
	SPR_Shutdown();

	if ( cl_funcs.pShutdown )
		cl_funcs.pShutdown();

	if ( cls.fSecureClient )
	{
		FreeBlob( &g_blobfootprintClient );
	}
	else
	{
#if !defined( _WIN32 )
		dlclose( hClientDLL );
#else
		FreeLibrary( hClientDLL );
#endif
		hClientDLL = NULL;
	}

	Q_memset( &cl_funcs, 0, sizeof( cl_funcs ) );
	Cvar_RemoveHudCvars();
	Cmd_RemoveHudCmds();
	CL_ShutDownUsrMessages();
	fClientLoaded = FALSE;

	if ( hClientDLL )
	{
#if !defined( _WIN32 )
		dlclose( hClientDLL );
#else
		FreeLibrary( hClientDLL );
#endif
	}
	hClientDLL = NULL;
}

int LoadSecureClient( char* pszDllName )
{
	void* hModule;

	cl_funcs.pInitFunc = (INITIALIZE_FUNC)&g_modfuncs;
	cl_funcs.pHudVidInitFunc = (HUD_VIDINIT_FUNC)&g_cldstAddrs;

	hModule = NLoadBlobFile( pszDllName, &g_blobfootprintClient, &cl_funcs, 1 );
	if ( hModule )
	{
		hClientDLL = (HINSTANCE)hModule;
		fClientLoaded = TRUE;
	}

	return hModule != NULL;
}

template <typename ExportFunction>
static void ClientDLL_LoadRequiredExport( ExportFunction& func, const char* pszName )
{
	func = (ExportFunction)Sys_GetProcAddress( hClientDLL, pszName );
	if ( !func )
		Sys_Error( "could not link client.dll function %s\n", pszName );
}

void LoadInsecureClient( char* pszDllName )
{
	hClientDLL = (HMODULE)FS_LoadLibrary( pszDllName );
	if ( !hClientDLL )
		Sys_Error( "could not load library %s", pszDllName );

	fClientLoaded = TRUE;

	ClientDLL_LoadRequiredExport( cl_funcs.pInitFunc, "Initialize" );
	ClientDLL_LoadRequiredExport( cl_funcs.pHudVidInitFunc, "HUD_VidInit" );
	ClientDLL_LoadRequiredExport( cl_funcs.pHudInitFunc, "HUD_Init" );
	ClientDLL_LoadRequiredExport( cl_funcs.pHudRedrawFunc, "HUD_Redraw" );
	ClientDLL_LoadRequiredExport( cl_funcs.pHudUpdateClientDataFunc, "HUD_UpdateClientData" );
	ClientDLL_LoadRequiredExport( cl_funcs.pHudResetFunc, "HUD_Reset" );
	ClientDLL_LoadRequiredExport( cl_funcs.pClientMove, "HUD_PlayerMove" );
	ClientDLL_LoadRequiredExport( cl_funcs.pClientMoveInit, "HUD_PlayerMoveInit" );
	ClientDLL_LoadRequiredExport( cl_funcs.pClientTextureType, "HUD_PlayerMoveTexture" );
	ClientDLL_LoadRequiredExport( cl_funcs.pIN_ActivateMouse, "IN_ActivateMouse" );
	ClientDLL_LoadRequiredExport( cl_funcs.pIN_DeactivateMouse, "IN_DeactivateMouse" );
	ClientDLL_LoadRequiredExport( cl_funcs.pIN_MouseEvent, "IN_MouseEvent" );
	ClientDLL_LoadRequiredExport( cl_funcs.pIN_ClearStates, "IN_ClearStates" );
	ClientDLL_LoadRequiredExport( cl_funcs.pIN_Accumulate, "IN_Accumulate" );
	ClientDLL_LoadRequiredExport( cl_funcs.pCL_CreateMove, "CL_CreateMove" );
	ClientDLL_LoadRequiredExport( cl_funcs.pCL_IsThirdPerson, "CL_IsThirdPerson" );
	ClientDLL_LoadRequiredExport( cl_funcs.pCL_GetCameraOffsets, "CL_CameraOffset" );
	ClientDLL_LoadRequiredExport( cl_funcs.pCamThink, "CAM_Think" );
	ClientDLL_LoadRequiredExport( cl_funcs.pFindKey, "KB_Find" );
	ClientDLL_LoadRequiredExport( cl_funcs.pCalcRefdef, "V_CalcRefdef" );
	ClientDLL_LoadRequiredExport( cl_funcs.pAddEntity, "HUD_AddEntity" );
	ClientDLL_LoadRequiredExport( cl_funcs.pCreateEntities, "HUD_CreateEntities" );
	ClientDLL_LoadRequiredExport( cl_funcs.pDrawNormalTriangles, "HUD_DrawNormalTriangles" );
	ClientDLL_LoadRequiredExport( cl_funcs.pDrawTransparentTriangles, "HUD_DrawTransparentTriangles" );
	ClientDLL_LoadRequiredExport( cl_funcs.pStudioEvent, "HUD_StudioEvent" );
	ClientDLL_LoadRequiredExport( cl_funcs.pShutdown, "HUD_Shutdown" );
	ClientDLL_LoadRequiredExport( cl_funcs.pTxferLocalOverrides, "HUD_TxferLocalOverrides" );
	ClientDLL_LoadRequiredExport( cl_funcs.pProcessPlayerState, "HUD_ProcessPlayerState" );
	ClientDLL_LoadRequiredExport( cl_funcs.pTxferPredictionData, "HUD_TxferPredictionData" );
	ClientDLL_LoadRequiredExport( cl_funcs.pReadDemoBuffer, "Demo_ReadBuffer" );
	ClientDLL_LoadRequiredExport( cl_funcs.pConnectionlessPacket, "HUD_ConnectionlessPacket" );
	ClientDLL_LoadRequiredExport( cl_funcs.pGetHullBounds, "HUD_GetHullBounds" );
	ClientDLL_LoadRequiredExport( cl_funcs.pHudFrame, "HUD_Frame" );
	ClientDLL_LoadRequiredExport( cl_funcs.pKeyEvent, "HUD_Key_Event" );
	ClientDLL_LoadRequiredExport( cl_funcs.pPostRunCmd, "HUD_PostRunCmd" );
	ClientDLL_LoadRequiredExport( cl_funcs.pTempEntUpdate, "HUD_TempEntUpdate" );
	ClientDLL_LoadRequiredExport( cl_funcs.pGetUserEntity, "HUD_GetUserEntity" );

	cl_funcs.pVoiceStatus = (HUD_VOICESTATUS_FUNC)Sys_GetProcAddress( hClientDLL, "HUD_VoiceStatus" );
	cl_funcs.pDirectorMessage = (HUD_DIRECTORMESSAGE_FUNC)Sys_GetProcAddress( hClientDLL, "HUD_DirectorMessage" );
	cl_funcs.pStudioInterface =
		(HUD_STUDIO_INTERFACE_FUNC)Sys_GetProcAddress( hClientDLL, "HUD_GetStudioModelInterface" );
	cl_funcs.pChatInputPosition =
		(HUD_CHATINPUTPOSITION_FUNC)Sys_GetProcAddress( hClientDLL, "HUD_ChatInputPosition" );
	cl_funcs.pClientFactory = (CLIENTFACTORY)Sys_GetProcAddress( hClientDLL, "ClientFactory" );
	cl_funcs.pGetPlayerTeam = (HUD_GETPLAYERTEAM)Sys_GetProcAddress( hClientDLL, "HUD_GetPlayerTeam" );
}

int NCallDummy( int ijump, int cnArg, ... )
{
	(void)ijump;
	(void)cnArg;
	return 0;
}

#if defined( _WIN32 )
static LONG CALLBACK ClientDLL_HeapCorruptionHandler( EXCEPTION_POINTERS* exceptionInfo )
{
	const DWORD heapCorruptionException = 0xC0000374;

	if ( exceptionInfo->ExceptionRecord->ExceptionCode == heapCorruptionException )
		return EXCEPTION_CONTINUE_EXECUTION;

	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void ClientDLL_Init()
{
	char szDllName[sizeof( g_szfullClientName )];

#if !defined( _WIN32 )
	strcpy( szDllName, "cl_dlls/client.so" );
#else
	strcpy( szDllName, "cl_dlls\\client.dll" );
#endif

	if ( fClientLoaded )
		ClientDLL_Shutdown();

#if defined( _WIN32 )
	if ( !Q_strcasecmp( com_gamedir, "ts" ) )
		AddVectoredExceptionHandler( 1, ClientDLL_HeapCorruptionHandler );
#endif

	COM_FixSlashes( szDllName );
	Q_strcpy( g_szfullClientName, szDllName );
	COM_ExpandFilename( g_szfullClientName );
	COM_FixSlashes( g_szfullClientName );

	cls.fSecureClient = FALSE;
	if ( !LoadSecureClient( g_szfullClientName ) && !fClientLoaded )
		LoadInsecureClient( g_szfullClientName );

	HookServerMsg( "ScreenShake", V_ScreenShake );
	HookServerMsg( "ScreenFade", V_ScreenFade );

	cl_funcs.pInitFunc( &cl_enginefuncs, CLDLL_INTERFACE_VERSION );
	ClientDLL_ClientMoveInit( &g_clmove );
	CL_GetPlayerHulls();
}

int CL_CreateVisibleEntity( int type, cl_entity_t* ent )
{
	g_engdstAddrs.CL_CreateVisibleEntity( &type, &ent );
	return CL_AddVisibleEntity( type, ent );
}

void hudPlaybackEvent( int flags, const edict_t* pInvoker, unsigned short eventindex, float delay, float* origin,
	float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 )
{
	event_args_t args;

	g_engdstAddrs.pfnPlaybackEvent( &flags, &pInvoker, &eventindex, &delay, &origin, &angles, &fparam1, &fparam2,
		&iparam1, &iparam2, &bparam1, &bparam2 );

	if ( flags & FEV_SERVER )
		return;

	Q_memset( &args, 0, sizeof( args ) );

	VectorCopy( origin, args.origin );
	VectorCopy( angles, args.angles );

	args.iparam1 = iparam1;
	args.iparam2 = iparam2;
	args.bparam1 = bparam1;
	args.bparam2 = bparam2;

	VectorCopy( cl.simvel, args.velocity );
	args.entindex = cl.playernum + 1;
	args.ducking = cl.usehull == 1;

	args.fparam1 = fparam1;
	args.fparam2 = fparam2;

	CL_QueueEvent( flags, eventindex, delay, &args );

	if ( cls.demorecording )
		CL_DemoEvent( flags, eventindex, delay, &args );
}

static void SDL_AdjustMousePosition( int* x, int* y )
{
	int windowWidth, windowHeight;
	int videoWidth, videoHeight;
	int xOffset, yOffset;

	if ( VideoMode_IsWindowed() )
		return;

	SDL_GetWindowSize( *pmainwindow, &windowWidth, &windowHeight );
	VideoMode_GetCurrentVideoMode( &videoWidth, &videoHeight, NULL );

	*x = (int)( (float)videoWidth / (float)windowWidth * (float)*x );
	*y = (int)( (float)videoHeight / (float)windowHeight * (float)*y );

	xOffset = *x - videoWidth / 2;
	*x = (int)( *x + ( GetXMouseAspectRatioAdjustment() - 1.0 ) * (float)xOffset );

	yOffset = *y - videoHeight / 2;
	*y = (int)( *y + ( GetYMouseAspectRatioAdjustment() - 1.0 ) * (float)yOffset );
}

void hudGetMousePosition( int* mx, int* my )
{
	g_engdstAddrs.GetMousePosition( &mx, &my );
	SDL_GetMouseState( mx, my );
	SDL_AdjustMousePosition( mx, my );
}

void SDL_GetMousePos( POINT* ppt )
{
	int *x, *y;

	g_engdstAddrs.pfnGetMousePos( &ppt );

	x = (int*)&ppt->x;
	y = (int*)&ppt->y;
	SDL_GetMouseState( x, y );
	SDL_AdjustMousePosition( x, y );
}

void SDL_SetMousePos( int x, int y )
{
	SDL_WarpMouseInWindow( *pmainwindow, x, y );
}

cl_enginefunc_t cl_enginefuncs =
{
	SPR_Load,
	SPR_Frames,
	SPR_Height,
	SPR_Width,
	SPR_Set,
	SPR_Draw,
	SPR_DrawHoles,
	SPR_DrawAdditive,
	SPR_EnableScissor,
	SPR_DisableScissor,
	SPR_GetList,
	Draw_FillRGBA,
	hudGetScreenInfo,
	SetCrosshair,
	hudRegisterVariable,
	hudGetCvarFloat,
	hudGetCvarString,
	hudAddCommand,
	hudHookUserMsg,
	hudServerCmd,
	hudClientCmd,
	hudGetPlayerInfo,
	hudPlaySoundByName,
	hudPlaySoundByIndex,
	AngleVectors,
	TextMessageGet,
	TextMessageDrawCharacter,
	Draw_String,
	Draw_SetTextColor,
	hudDrawConsoleStringLen,
	hudConsolePrint,
	hudCenterPrint,
	hudCenterX,
	hudCenterY,
	hudGetViewAngles,
	hudSetViewAngles,
	hudGetMaxClients,
	Cvar_SetValue,
	Cmd_Argc,
	(pfnEngSrc_Cmd_Argv_t)Cmd_Argv,
	(pfnEngSrc_Con_Printf_t)Con_Printf,
	(pfnEngSrc_Con_DPrintf_t)Con_DPrintf,
	Con_NPrintf,
	Con_NXPrintf,
	hudPhysInfo_ValueForKey,
	hudServerInfo_ValueForKey,
	hudGetClientMaxspeed,
	hudCheckParm,
	Key_Event,
	hudGetMousePosition,
	hudIsNoClipping,
	hudGetLocalPlayer,
	hudGetViewModel,
	hudGetEntityByIndex,
	hudGetClientTime,
	V_CalcShake,
	V_ApplyShake,
	PM_PointContents,
	PM_WaterEntity,
	PM_TraceLine,
	CL_LoadModel,
	CL_CreateVisibleEntity,
	hudGetSpritePointer,
	hudPlaySoundByNameAtLocation,
	hudPrecacheEvent,
	hudPlaybackEvent,
	hudWeaponAnim,
	RandomFloat,
	RandomLong,
	CL_HookEvent,
	Con_IsVisible,
	hudGetGameDir,
	Cvar_FindVar,
	Key_NameForBinding,
	hudGetLevelName,
	hudGetScreenFade,
	hudSetScreenFade,
	VGuiWrap_GetPanel,
	VGui_ViewportPaintBackground,
	COM_LoadFile,
	COM_ParseFile,
	COM_FreeFile,
	&tri,
	&efx,
	&eventapi,
	&demoapi,
	&netapi,
	&g_VoiceTweakAPI,
	CL_IsSpectateOnly,
	R_LoadMapSprite,
	COM_AddAppDirectoryToSearchPath,
	ClientDLL_ExpandFileName,
	PlayerInfo_ValueForKey,
	PlayerInfo_SetValueForKey,
	GetPlayerUniqueID,
	GetTrackerIDForPlayer,
	GetPlayerForTrackerID,
	hudServerCmdUnreliable,
	SDL_GetMousePos,
	SDL_SetMousePos,
	SetMouseEnable,
	GetFirstCVarPtr,
	GetFirstCmdFunctionHandle,
	GetNextCmdFunctionHandle,
	GetCmdFunctionName,
	hudGetClientOldTime,
	hudGetServerGravityValue,
	hudGetModelByIndex,
	SetFilterMode,
	SetFilterColor,
	SetFilterBrightness,
	SequenceGet,
	SPR_DrawGeneric,
	SequencePickSentence,
	VGUI2_DrawStringClient,
	VGUI2_DrawStringReverseClient,
	LocalPlayerInfo_ValueForKey,
	VGUI2_Draw_Character,
	VGUI2_Draw_CharacterAdd,
	COM_GetApproxWavePlayLength,
	VguiWrap2_GetCareerUI,
	(pfnEngSrc_Cvar_Set_t)Cvar_Set,
	VGuiWrap2_IsInCareerMatch,
	hudPlaySoundVoiceByName,
	PrimeMusicStream,
	Sys_FloatTime,
	ProcessTutorMessageDecayBuffer,
	ConstructTutorMessageDecayBuffer,
	ResetTutorMessageDecayData,
	hudPlaySoundByNameAtPitch,
	Draw_FillRGBABlend,
	GetGameAppID,
	GetAliasesList,
	VguiWrap2_GetMouseDelta,
	hudFilteredClientCmd
};
