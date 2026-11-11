/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// Robin, 4-22-98: Moved set_suicide_frame() here from player.cpp to allow us to 
//				   have one without a hardcoded player.mdl in tf_client.cpp

/*

===== client.cpp ========================================================

  client/server game specific stuff

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "client.h"
#include "spectator.h"
#include "soundent.h"
#include "gamerules.h"
#include "career_tasks.h"
#include "game.h"
#include "customentity.h"
#include "entity_state.h"
#include "weaponinfo.h"
#include "saverestore.h"
#include "usercmd.h"
#include "netadr.h"
#include "pm_shared.h"
#include "voice_gamemgr.h"
#include "bot/bot_manager.h"
#include "hitgroups.h"
#include "tutor.h"
#include "bot/bot_profile.h"
#include "bot/cs_bot_chatter.h"
#include "bot/nav_area.h"
#include "hostage/hostage_localnav.h"
#include "unicode_strtools.h"
#include "buildslave_assert.h"

#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\dlls\\client.cpp" )

extern BOOL g_fGameOver;
extern int g_iSkillLevel;
extern ULONG g_ulFrameCount;
extern int gmsgShowMenu;
extern int gmsgVGUIMenu;
extern int gmsgBlinkAcct;
extern int gmsgSayText;
extern int gmsgStatusIcon;
extern int gmsgTeamInfo;
extern int gmsgMoney;
extern int gmsgScoreInfo;
extern int gmsgSpectator;
extern int giPrecacheGrunt;
extern cvar_t startmoney;
extern cvar_t kick_percent;
extern CBotManager* TheBots;
extern const char* BotArgs[4];
extern bool UseBotArgs;
extern void LinkUserMessages();
extern void WriteSigonMessages();
extern char g_szMapBriefingText[512];
extern int gmsgBuyClose;
extern int gmsgADStop;
extern void UTIL_SetDprintfFlags( const char* flags );

#undef CMD_ARGC
#undef CMD_ARGV

static inline int CMD_ARGC()
{
	if ( !UseBotArgs )
		return g_engfuncs.pfnCmd_Argc();
	for ( int argument = 0; argument < 4; ++argument )
	{
		if ( !BotArgs[argument] )
			return argument;
	}
	return 4;
}

static inline const char* CMD_ARGV( int argument )
{
	if ( !UseBotArgs )
		return g_engfuncs.pfnCmd_Argv( argument );
	if ( argument < 4 )
		return BotArgs[argument];
	return NULL;
}

float g_flTimeLimit;
float g_flResetTime;
unsigned short m_usResetDecals;
unsigned short g_iShadowSprite;
bool g_bClientPrintEnable = true;
bool g_skipCareerInitialSpawn;
float PosX;
float PosY;
float PosZ;
float r;
float g;
float b;
int alpha;
bool nvg;

char* sPlayerModelFiles[] =
{
	"models/player.mdl",
	"models/player/leet/leet.mdl",
	"models/player/gign/gign.mdl",
	"models/player/vip/vip.mdl",
	"models/player/gsg9/gsg9.mdl",
	"models/player/guerilla/guerilla.mdl",
	"models/player/arctic/arctic.mdl",
	"models/player/sas/sas.mdl",
	"models/player/terror/terror.mdl",
	"models/player/urban/urban.mdl",
	"models/player/spetsnaz/spetsnaz.mdl",
	"models/player/militia/militia.mdl"
};

static int g_serveractive;

#define MAX_CLIENTS 32
#define MAX_ENTITIES 1380
#define HISTORY_NIGHTVISION (1 << 20)
#define SIGNAL_BUY (1 << 0)

typedef struct
{
	float m_fTimeEnteredPVS;
} ENTITYPVSSTATUS;

typedef struct
{
	ENTITYPVSSTATUS m_Status[MAX_ENTITIES];
	int headnode;
	int num_leafs;
	short leafnums[MAX_ENT_LEAFS];
} PLAYERPVSSTATUS;

static PLAYERPVSSTATUS g_PVSStatus[32];

typedef struct
{
	char name[32];
	int field;
} entity_field_alias_t;

static entity_field_alias_t entity_field_alias[] =
{
	{ "origin[0]", 0 },
	{ "origin[1]", 0 },
	{ "origin[2]", 0 },
	{ "angles[0]", 0 },
	{ "angles[1]", 0 },
	{ "angles[2]", 0 }
};

static entity_field_alias_t player_field_alias[] =
{
	{ "origin[0]", 0 },
	{ "origin[1]", 0 },
	{ "origin[2]", 0 }
};

entity_field_alias_t custom_entity_field_alias[] =
{
	{ "origin[0]", 0 },
	{ "origin[1]", 0 },
	{ "origin[2]", 0 },
	{ "angles[0]", 0 },
	{ "angles[1]", 0 },
	{ "angles[2]", 0 },
	{ "skin", 0 },
	{ "sequence", 0 },
	{ "animtime", 0 }
};

BOOL ClientConnect( edict_t* entity, const char* name, const char* address, char* rejectReason )
{
	return g_pGameRules->ClientConnected( entity, name, address, rejectReason );
}

/*
 * used by kill command and disconnect command
 * ROBIN: Moved here from player.cpp, to allow multiple player models
 */
void set_suicide_frame( entvars_t* pev )
{
	if ( !FStrEq( STRING( pev->model ), "models/player.mdl" ) )
		return; // allready gibbed

//	pev->frame		= $deatha11;
	pev->solid = SOLID_NOT;
	pev->movetype = MOVETYPE_TOSS;
	pev->deadflag = DEAD_DEAD;
	pev->nextthink = -1;
}

void ShowMenu( CBasePlayer* player, int validSlots, int displayTime, BOOL needMore, char* text )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgShowMenu, NULL, player->edict() );
		WRITE_SHORT( validSlots );
		WRITE_CHAR( displayTime );
		WRITE_BYTE( needMore );
		WRITE_STRING( text );
	MESSAGE_END();
}

void ShowVGUIMenu( CBasePlayer* player, int menu, int validSlots, char* oldMenu )
{
	if ( player->m_bVGUIMenus || menu > 34 )
	{
		MESSAGE_BEGIN( MSG_ONE, gmsgVGUIMenu, NULL, player->edict() );
			WRITE_BYTE( menu );
			WRITE_SHORT( validSlots );
			WRITE_CHAR( -1 );
			WRITE_BYTE( 0 );
			WRITE_STRING( " " );
		MESSAGE_END();
	}
	else
		ShowMenu( player, validSlots, -1, FALSE, oldMenu );
}

void PlayerPreThink( edict_t* entity )
{
	if ( entity && entity->pvPrivateData )
		( (CBasePlayer*)entity->pvPrivateData )->PreThink();
}

void PlayerPostThink( edict_t* entity )
{
	if ( entity && entity->pvPrivateData )
		( (CBasePlayer*)entity->pvPrivateData )->PostThink();
}

void ParmsNewLevel()
{
}

void ParmsChangeLevel()
{
	SAVERESTOREDATA* saveData = (SAVERESTOREDATA*)gpGlobals->pSaveData;
	if ( saveData )
		saveData->connectionCount = BuildChangeList( saveData->levelList, MAX_LEVEL_CONNECTIONS );
}

void PlayerCustomization( edict_t* entity, customization_t* customization )
{
	CBasePlayer* player = entity ? (CBasePlayer*)entity->pvPrivateData : NULL;
	if ( !player )
	{
		ALERT( at_console, "PlayerCustomization:  Couldn't get player!\n" );
		return;
	}
	if ( !customization )
	{
		ALERT( at_console, "PlayerCustomization:  NULL customization!\n" );
		return;
	}
	switch ( customization->resource.type )
	{
	case t_sound:
	case t_skin:
	case t_model:
		break;
	case t_decal:
		player->SetCustomDecalFrames( customization->nUserData2 );
		// Ignore for now.
		break;
	default:
		ALERT( at_console, "PlayerCustomization:  Unknown customization type!\n" );
		break;
	}
}

void SpectatorConnect( edict_t* entity )
{
	if ( entity && entity->pvPrivateData )
		( (CBaseSpectator*)entity->pvPrivateData )->SpectatorConnect();
}

void SpectatorDisconnect( edict_t* entity )
{
	if ( entity && entity->pvPrivateData )
		( (CBaseSpectator*)entity->pvPrivateData )->SpectatorDisconnect();
}

void SpectatorThink( edict_t* entity )
{
	if ( entity && entity->pvPrivateData )
		( (CBaseSpectator*)entity->pvPrivateData )->SpectatorThink();
}

void Sys_Error( const char* )
{
}

void CreateBaseline( int player, int entityIndex, entity_state_t* baseline, edict_t* entity,
	int playerModelIndex, Vector playerMins, Vector playerMaxs )
{
	baseline->origin = entity->v.origin;
	baseline->angles = entity->v.angles;
	baseline->frame = entity->v.frame;
	baseline->skin = (short)entity->v.skin;

	// render information
	baseline->rendermode = (byte)entity->v.rendermode;
	baseline->renderamt = (byte)(int)entity->v.renderamt;
	baseline->rendercolor.r = (byte)(int)entity->v.rendercolor.x;
	baseline->rendercolor.g = (byte)(int)entity->v.rendercolor.y;
	baseline->rendercolor.b = (byte)(int)entity->v.rendercolor.z;
	baseline->renderfx = (byte)entity->v.renderfx;
	if ( player )
	{
		baseline->mins = playerMins;
		baseline->maxs = playerMaxs;
		baseline->colormap = entityIndex;
		baseline->modelindex = playerModelIndex;
		baseline->friction = 1;
		baseline->movetype = MOVETYPE_WALK;
		baseline->scale = entity->v.scale;
		baseline->solid = SOLID_SLIDEBOX;
		baseline->framerate = 1;
		baseline->gravity = 1;
	}
	else
	{
		baseline->mins = entity->v.mins;
		baseline->maxs = entity->v.maxs;
		baseline->colormap = 0;
		baseline->modelindex = entity->v.modelindex; //SV_ModelIndex(pr_strings + entity->v.model);
		baseline->movetype = entity->v.movetype;
		baseline->scale = entity->v.scale;
		baseline->solid = (short)entity->v.solid;
		baseline->framerate = entity->v.framerate;
		baseline->gravity = entity->v.gravity;
	}
}

void Entity_FieldInit( delta_s* fields )
{
	for ( int field = 0; field < ARRAYSIZE( entity_field_alias ); ++field )
		entity_field_alias[field].field = DELTA_FINDFIELD( fields, entity_field_alias[field].name );
}

void Entity_Encode( delta_s* fields, const unsigned char* from, const unsigned char* to )
{
	static int initialized;
	if ( !initialized )
	{
		Entity_FieldInit( fields );
		initialized = 1;
	}
	const entity_state_t* previous = (const entity_state_t*)from;
	const entity_state_t* current = (const entity_state_t*)to;
	if ( current->number - 1 == ENGINE_CURRENT_PLAYER() )
	{
		for ( int field = 0; field < 3; ++field )
			DELTA_UNSETBYINDEX( fields, entity_field_alias[field].field );
	}
	if ( current->impacttime != 0 && current->starttime != 0 )
	{
		for ( int field = 0; field < ARRAYSIZE( entity_field_alias ); ++field )
			DELTA_UNSETBYINDEX( fields, entity_field_alias[field].field );
	}
	if ( current->movetype == MOVETYPE_FOLLOW && current->aiment )
	{
		for ( int field = 0; field < 3; ++field )
			DELTA_UNSETBYINDEX( fields, entity_field_alias[field].field );
	}
	else if ( previous->aiment != current->aiment )
	{
		for ( int field = 0; field < 3; ++field )
			DELTA_SETBYINDEX( fields, entity_field_alias[field].field );
	}
}

void Player_FieldInit( delta_s* fields )
{
	for ( int field = 0; field < ARRAYSIZE( player_field_alias ); ++field )
		player_field_alias[field].field = DELTA_FINDFIELD( fields, player_field_alias[field].name );
}

void Player_Encode( delta_s* fields, const unsigned char* from, const unsigned char* to )
{
	static int initialized;
	if ( !initialized )
	{
		Player_FieldInit( fields );
		initialized = 1;
	}
	const entity_state_t* previous = (const entity_state_t*)from;
	const entity_state_t* current = (const entity_state_t*)to;
	if ( current->number - 1 == ENGINE_CURRENT_PLAYER() )
	{
		for ( int field = 0; field < 3; ++field )
			DELTA_UNSETBYINDEX( fields, entity_field_alias[field].field );
	}
	if ( current->movetype == MOVETYPE_FOLLOW && current->aiment )
	{
		for ( int field = 0; field < 3; ++field )
			DELTA_UNSETBYINDEX( fields, entity_field_alias[field].field );
	}
	else if ( previous->aiment != current->aiment )
	{
		for ( int field = 0; field < 3; ++field )
			DELTA_SETBYINDEX( fields, entity_field_alias[field].field );
	}
}

void Custom_Entity_FieldInit( delta_s* fields )
{
	for ( int field = 0; field < ARRAYSIZE( custom_entity_field_alias ); ++field )
		custom_entity_field_alias[field].field = DELTA_FINDFIELD( fields, custom_entity_field_alias[field].name );
}

/*
==================
Custom_Encode

Callback for sending entity_state_t info ( for custom entities ) over network. 
FIXME:  Move to script
==================
*/
void Custom_Encode( delta_s* fields, const unsigned char* from, const unsigned char* to )
{
	static int initialized;
	if ( !initialized )
	{
		Custom_Entity_FieldInit( fields );
		initialized = 1;
	}
	const entity_state_t* previous = (const entity_state_t*)from;
	const entity_state_t* current = (const entity_state_t*)to;
	int beamType = current->rendermode & 0x0F;
	if ( beamType != BEAM_POINTS && beamType != BEAM_ENTPOINT )
	{
		for ( int field = 0; field < 3; ++field )
			DELTA_UNSETBYINDEX( fields, custom_entity_field_alias[field].field );
	}
	if ( beamType != BEAM_POINTS )
	{
		for ( int field = 3; field < 6; ++field )
			DELTA_UNSETBYINDEX( fields, custom_entity_field_alias[field].field );
	}
	if ( beamType != BEAM_ENTS && beamType != BEAM_ENTPOINT )
	{
		DELTA_UNSETBYINDEX( fields, custom_entity_field_alias[6].field );
		DELTA_UNSETBYINDEX( fields, custom_entity_field_alias[7].field );
	}
	if ( !memcmp( &previous->animtime, &current->animtime, sizeof( current->animtime ) ) )
		DELTA_UNSETBYINDEX( fields, custom_entity_field_alias[8].field );
}

/*
=================
RegisterEncoders

Allows game .dll to override network encoding of certain types of entities and tweak values, etc.
=================
*/
void RegisterEncoders()
{
	DELTA_ADDENCODER( "Entity_Encode", Entity_Encode );
	DELTA_ADDENCODER( "Custom_Encode", Custom_Encode );
	DELTA_ADDENCODER( "Player_Encode", Player_Encode );
}

void CmdStart( const edict_t* entity, const usercmd_s*, unsigned int randomSeed )
{
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( (entvars_t*)&entity->v ) );
	if ( !player )
		return;
	if ( player->pev->groupinfo )
		UTIL_SetGroupTrace( player->pev->groupinfo, GROUP_OP_AND );
	player->random_seed = randomSeed;
}

void CmdEnd( const edict_t* entity )
{
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( (entvars_t*)&entity->v ) );
	if ( !player )
		return;
	if ( player->pev->groupinfo )
		UTIL_UnsetGroupTrace();
	if ( player->pev->flags & FL_DUCKING )
		UTIL_SetSize( player->pev, Vector( -16, -16, -18 ), Vector( 16, 16, 32 ) );
}

int ConnectionlessPacket( const netadr_s*, const char*, char*, int* responseSize )
{
	*responseSize = 0;
	return 0;
}

int GetHullBounds( int hull, float*, float* )
{
	return (unsigned int)hull <= 2;
}

void CreateInstancedBaselines()
{
}

int AllowLagCompensation()
{
	return 1;
}

extern "C" int Q_strlen( const char* text )
{
	int length = 0;
	if ( text )
	{
		while ( text[length] )
			++length;
	}
	return length;
}

void BlinkAccount( CBasePlayer* player, int count )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgBlinkAcct, NULL, player->edict() );
		WRITE_BYTE( count );
	MESSAGE_END();
}

void DropPrimary( CBasePlayer* player )
{
	if ( player->HasShield() )
		player->DropShield( true );
	else if ( player->m_rgpPlayerItems[1] )
		player->DropPlayerItem( STRING( player->m_rgpPlayerItems[1]->pev->classname ) );
}

bool BuyGunAmmo( CBasePlayer& player, CBasePlayerItem& weapon, bool blinkMoney )
{
	if ( !player.CanPlayerBuy( true ) )
		return false;
	int ammoIndex = weapon.PrimaryAmmoIndex();
	if ( ammoIndex == -1 || player.m_rgAmmo[ammoIndex] >= CBasePlayerItem::ItemInfoArray[weapon.m_iId].iMaxAmmo1 )
		return false;
	int price;
	const char* item;
	switch ( weapon.m_iId )
	{
	case WEAPON_P228:
		price = AMMO_357SIG_PRICE;
		item = "ammo_357sig";
		break;
	case WEAPON_SCOUT:
	case WEAPON_G3SG1:
	case WEAPON_AK47:
		price = AMMO_762MM_PRICE;
		item = "ammo_762nato";
		break;
	case WEAPON_XM1014:
	case WEAPON_M3:
		price = AMMO_BUCKSHOT_PRICE;
		item = "ammo_buckshot";
		break;
	case WEAPON_MAC10:
	case WEAPON_UMP45:
	case WEAPON_USP:
		price = AMMO_45ACP_PRICE;
		item = "ammo_45acp";
		break;
	case WEAPON_AUG:
	case WEAPON_SG550:
	case WEAPON_GALIL:
	case WEAPON_FAMAS:
	case WEAPON_M4A1:
	case WEAPON_SG552:
		price = AMMO_556MM_PRICE;
		item = "ammo_556nato";
		break;
	case WEAPON_ELITE:
	case WEAPON_GLOCK18:
	case WEAPON_MP5N:
	case WEAPON_TMP:
		price = AMMO_9MM_PRICE;
		item = "ammo_9mm";
		break;
	case WEAPON_FIVESEVEN:
	case WEAPON_P90:
		price = AMMO_57MM_PRICE;
		item = "ammo_57mm";
		break;
	case WEAPON_AWP:
		price = AMMO_338MAG_PRICE;
		item = "ammo_338magnum";
		break;
	case WEAPON_M249:
		price = AMMO_556MM_PRICE;
		item = "ammo_556natobox";
		break;
	case WEAPON_DEAGLE:
		price = AMMO_50AE_PRICE;
		item = "ammo_50ae";
		break;
	default:
		ALERT( at_console, "Tried to buy ammo for an unrecognized gun\n" );
		return false;
	}
	if ( player.m_iAccount >= price )
	{
		player.GiveNamedItem( item );
		player.AddAccount( -price, true );
		return true;
	}
	if ( blinkMoney && g_bClientPrintEnable )
	{
		ClientPrint( player.pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
		BlinkAccount( &player, 2 );
	}
	return false;
}

bool BuyAmmo( CBasePlayer* player, int slot, bool blinkMoney )
{
	if ( !player->CanPlayerBuy( true ) || (unsigned int)( slot - 1 ) > 1 )
		return false;
	CBasePlayerItem* weapon = player->m_rgpPlayerItems[slot];
	if ( player->HasShield() && player->m_rgpPlayerItems[2] )
		weapon = player->m_rgpPlayerItems[2];
	if ( !weapon )
		return false;
	while ( weapon )
	{
		if ( !BuyGunAmmo( *player, *weapon, blinkMoney ) )
			return false;
		weapon = weapon->m_pNext;
	}
	return true;
}

int GetWeaponData( edict_t* entity, weapon_data_t* info )
{
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( &entity->v ) );
	memset( info, 0, 32 * sizeof( *info ) );
	if ( !player )
		return 1;
	for ( int slot = 0; slot < MAX_ITEM_TYPES; ++slot )
	{
		for ( CBasePlayerItem* item = player->m_rgpPlayerItems[slot]; item; item = item->m_pNext )
		{
			CBasePlayerWeapon* weapon = dynamic_cast<CBasePlayerWeapon*>( item->GetWeaponPtr() );
			if ( !weapon || !weapon->UseDecrement() )
				continue;
			ItemInfo itemInfo = {};
			weapon->GetItemInfo( &itemInfo );
			if ( (unsigned int)itemInfo.iId >= 32 )
				continue;
			weapon_data_t* state = &info[itemInfo.iId];
			state->m_iId = itemInfo.iId;
			state->m_iClip = weapon->m_iClip;
			state->m_flTimeWeaponIdle = weapon->m_flTimeWeaponIdle < -0.001f ? -0.001f : weapon->m_flTimeWeaponIdle;
			state->m_flNextPrimaryAttack = weapon->m_flNextPrimaryAttack < -0.001f ? -0.001f : weapon->m_flNextPrimaryAttack;
			state->m_flNextSecondaryAttack = weapon->m_flNextSecondaryAttack < -0.001f ? -0.001f : weapon->m_flNextSecondaryAttack;
			state->m_flNextReload = weapon->m_flNextReload >= -0.001f ? weapon->m_flNextReload : -0.001f;
			state->m_fInReload = weapon->m_fInReload;
			state->m_fInSpecialReload = weapon->m_fInSpecialReload;
			state->m_fInZoom = weapon->m_iShotsFired;
			state->m_fAimedDamage = weapon->m_flLastFire;
			state->m_iWeaponState = weapon->m_iWeaponState;
			state->fuser2 = weapon->m_flStartThrow;
			state->fuser3 = weapon->m_flReleaseThrow;
			state->iuser1 = weapon->m_iSwing;
		}
	}
	return 1;
}

void UpdateClientData( const edict_t* entity, int sendWeapons, clientdata_t* data )
{
	if ( !entity || !entity->pvPrivateData )
		return;
	entvars_t* pev = (entvars_t*)&entity->v;
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( pev ) );
	entvars_t* observer = NULL;
	if ( player->pev->iuser1 == OBS_IN_EYE && player->m_hObserverTarget )
	{
		observer = pev;
		pev = player->m_hObserverTarget->pev;
		player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( pev ) );
	}
	data->flags = pev->flags;
	data->health = pev->health;
	data->viewmodel = MODEL_INDEX( STRING( pev->viewmodel ) );
	data->waterlevel = pev->waterlevel;
	data->watertype = pev->watertype;
	data->weapons = pev->weapons;
	data->origin = pev->origin;
	data->velocity = pev->velocity;
	data->view_ofs = pev->view_ofs;
	data->punchangle = pev->punchangle;
	data->bInDuck = pev->bInDuck;
	data->flTimeStepSound = pev->flTimeStepSound;
	data->flDuckTime = pev->flDuckTime;
	data->flSwimTime = pev->flSwimTime;
	data->waterjumptime = (int)pev->teleport_time;
	strcpy( data->physinfo, ENGINE_GETPHYSINFO( entity ) );
	data->maxspeed = pev->maxspeed;
	data->fov = pev->fov;
	data->weaponanim = pev->weaponanim;
	data->pushmsec = pev->pushmsec;
	data->iuser1 = observer ? observer->iuser1 : pev->iuser1;
	data->iuser2 = observer ? observer->iuser2 : pev->iuser2;
	data->iuser3 = observer ? observer->iuser3 : pev->iuser3;
	data->fuser1 = pev->fuser1;
	data->fuser3 = pev->fuser3;
	data->fuser2 = pev->fuser2;
	if ( !sendWeapons || !player )
		return;
	data->ammo_shells = player->ammo_buckshot;
	data->ammo_nails = player->ammo_9mm;
	data->ammo_cells = player->ammo_556nato;
	data->ammo_rockets = player->ammo_556natobox;
	data->vuser2.x = (float)player->ammo_762nato;
	data->vuser2.y = (float)player->ammo_45acp;
	data->vuser2.z = (float)player->ammo_50ae;
	data->vuser3.x = (float)player->ammo_338mag;
	data->vuser3.y = (float)player->ammo_57mm;
	data->vuser3.z = (float)player->ammo_357sig;
	data->m_flNextAttack = player->m_flNextAttack;
	int flags = player->m_bCanShoot && !player->m_bIsDefusing ? 1 : 0;
	if ( g_pGameRules->IsFreezePeriod() )
		flags |= 2;
	if ( player->m_signals.GetState() & SIGNAL_BOMB )
		flags |= 4;
	if ( player->HasShield() )
		flags |= 8;
	if ( !player->pev->iuser1 && !observer )
		data->iuser3 = flags;
	if ( !player->m_pActiveItem )
		return;
	CBasePlayerWeapon* weapon = dynamic_cast<CBasePlayerWeapon*>( player->m_pActiveItem->GetWeaponPtr() );
	if ( weapon && weapon->UseDecrement() )
	{
		ItemInfo itemInfo = {};
		weapon->GetItemInfo( &itemInfo );
		data->m_iId = itemInfo.iId;
		if ( (unsigned int)weapon->m_iPrimaryAmmoType < 32 )
		{
			data->vuser4.x = (float)weapon->m_iPrimaryAmmoType;
			data->vuser4.y = (float)player->m_rgAmmo[weapon->m_iPrimaryAmmoType];
			return;
		}
	}
	data->vuser4.x = -1;
	data->vuser4.y = 0;
}

void ResetPlayerPVS( edict_t* client, int clientnum )
{
	ASSERT_AT( clientnum >=0 && clientnum < MAX_CLIENTS, 5227 );
	PLAYERPVSSTATUS* pvs = &g_PVSStatus[clientnum];
	memset( pvs, 0, sizeof( *pvs ) );
	pvs->headnode = client->headnode;
	pvs->num_leafs = client->num_leafs;
	memcpy( pvs->leafnums, client->leafnums, sizeof( pvs->leafnums ) );
}

bool CheckPlayerPVSLeafChanged( edict_t* client, int clientnum )
{
	ASSERT_AT( clientnum >=0 && clientnum < MAX_CLIENTS, 5241 );
	PLAYERPVSSTATUS* pvs = &g_PVSStatus[clientnum];
	if ( pvs->headnode != client->headnode || pvs->num_leafs != client->num_leafs )
		return true;
	for ( int i = 0; i < pvs->num_leafs; ++i )
	{
		if ( client->leafnums[i] != pvs->leafnums[i] )
			return true;
	}
	return false;
}

void MarkEntityInPVS( int clientnum, int entitynum, float time, bool inpvs )
{
	ASSERT_AT( clientnum >=0 && clientnum < MAX_CLIENTS, 5264 );
	ASSERT_AT( entitynum >= 0 && entitynum < MAX_ENTITIES, 5265 );
	PLAYERPVSSTATUS* pvs = &g_PVSStatus[clientnum];
	ENTITYPVSSTATUS* es = &pvs->m_Status[entitynum];
	es->m_fTimeEnteredPVS = inpvs ? time : 0;
}

bool CheckEntityRecentlyInPVS( int clientnum, int entitynum, float currenttime )
{
	ASSERT_AT( clientnum >=0 && clientnum < MAX_CLIENTS, 5276 );
	ASSERT_AT( entitynum >= 0 && entitynum < MAX_ENTITIES, 5277 );
	PLAYERPVSSTATUS* pvs = &g_PVSStatus[clientnum];
	ENTITYPVSSTATUS* es = &pvs->m_Status[entitynum];
	return es->m_fTimeEnteredPVS != 0 && es->m_fTimeEnteredPVS + 1.0f >= currenttime;
}

void SetupVisibility( edict_t* viewEntity, edict_t* client, unsigned char** pvs, unsigned char** pas )
{
	edict_t* view = viewEntity ? viewEntity : client;
	if ( client->v.flags & FL_PROXY )
	{
		*pvs = NULL;	// the spectator proxy sees
		*pas = NULL;	// and hears everything
		return;
	}
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( client ) );
	if ( player && player->pev->iuser2 && player->m_hObserverTarget &&
		( player->m_afPhysicsFlags & PFLAG_OBSERVER ) )
	{
		view = player->m_hObserverTarget->edict();
		UTIL_SetOrigin( player->pev, player->m_hObserverTarget->pev->origin );
	}
	Vector origin = view->v.origin + view->v.view_ofs;
	if ( view->v.flags & FL_DUCKING )
		origin = origin + ( VEC_HULL_MIN - VEC_DUCK_HULL_MIN );
	*pvs = ENGINE_SET_PVS( origin );
	*pas = ENGINE_SET_PAS( origin );
}

int AddToFullPack( entity_state_t* state, int index, edict_t* entity, edict_t* host,
	int hostFlags, int player, unsigned char* set )
{
	if ( ( entity->v.effects & EF_NODRAW ) && entity != host )
		return 0;

	// Ignore ents without valid / visible models
	if ( !entity->v.modelindex || !STRING( entity->v.model ) )
		return 0;
	if ( ( entity->v.flags & FL_SPECTATOR ) && entity != host )
		return 0;
	int hostNumber = ENTINDEX( host ) - 1;
	if ( CheckPlayerPVSLeafChanged( host, hostNumber ) )
		ResetPlayerPVS( host, hostNumber );
	if ( entity != host && !CheckEntityRecentlyInPVS( hostNumber, index, gpGlobals->time ) )
	{
		if ( !ENGINE_CHECK_VISIBILITY( entity, set ) )
		{
			MarkEntityInPVS( hostNumber, index, gpGlobals->time, false );
			return 0;
		}
		MarkEntityInPVS( hostNumber, index, gpGlobals->time, true );
	}
	if ( ( entity->v.flags & FL_SKIPLOCALHOST ) && ( hostFlags & 1 ) && host == entity->v.owner )
		return 0;
	if ( host->v.groupinfo )
	{
		UTIL_SetGroupTrace( host->v.groupinfo, GROUP_OP_AND );

		// Should always be set, of course
		if ( entity->v.groupinfo )
		{
			if ( g_groupop == GROUP_OP_AND && !( entity->v.groupinfo & host->v.groupinfo ) )
				return 0;
			if ( g_groupop == GROUP_OP_NAND && ( entity->v.groupinfo & host->v.groupinfo ) )
				return 0;
		}
		UTIL_UnsetGroupTrace();
	}
	memset( state, 0, sizeof( *state ) );
	state->number = index;
	state->entityType = entity->v.flags & FL_CUSTOMENTITY ? ENTITY_BEAM : ENTITY_NORMAL;
	state->animtime = (float)( (int)( 1000.0 * entity->v.animtime ) / 1000.0 );
	state->origin = entity->v.origin;
	state->angles = entity->v.angles;
	state->mins = entity->v.mins;
	state->maxs = entity->v.maxs;
	state->startpos = entity->v.startpos;
	state->endpos = entity->v.endpos;
	state->impacttime = entity->v.impacttime;
	state->starttime = entity->v.starttime;
	state->modelindex = entity->v.modelindex;
	state->frame = entity->v.frame;
	state->skin = (short)entity->v.skin;
	state->effects = entity->v.effects;
	if ( !player && entity->v.animtime != 0 && entity->v.velocity.x == 0 &&
		entity->v.velocity.y == 0 && entity->v.velocity.z == 0 )
		state->eflags |= EFLAG_SLERP;
	state->scale = entity->v.scale;
	state->solid = (short)entity->v.solid;
	state->colormap = entity->v.colormap;
	state->movetype = entity->v.movetype;
	state->sequence = entity->v.sequence;
	state->framerate = entity->v.framerate;
	state->body = entity->v.body;
	memcpy( state->controller, entity->v.controller, sizeof( entity->v.controller ) );
	memcpy( state->blending, entity->v.blending, sizeof( entity->v.blending ) );
	state->rendermode = entity->v.rendermode;
	state->renderamt = (int)entity->v.renderamt;
	state->renderfx = entity->v.renderfx;
	state->rendercolor.r = (byte)(int)entity->v.rendercolor.x;
	state->rendercolor.g = (byte)(int)entity->v.rendercolor.y;
	state->rendercolor.b = (byte)(int)entity->v.rendercolor.z;
	state->aiment = entity->v.aiment ? ENTINDEX( entity->v.aiment ) : 0;
	state->owner = 0;
	if ( entity->v.owner )
	{
		int owner = ENTINDEX( entity->v.owner );
		if ( owner > 0 && owner <= gpGlobals->maxClients )
			state->owner = owner;
	}
	if ( player )
	{
		state->basevelocity = entity->v.basevelocity;
		state->weaponmodel = MODEL_INDEX( STRING( entity->v.weaponmodel ) );
		state->gaitsequence = entity->v.gaitsequence;
		state->spectator = entity->v.flags & FL_SPECTATOR;
		state->friction = entity->v.friction;
		state->gravity = entity->v.gravity;
		state->usehull = ( entity->v.flags & FL_DUCKING ) != 0;
		state->health = (int)entity->v.health;
	}
	else
		state->playerclass = entity->v.playerclass;
	state->iuser4 = entity->v.iuser4;
	CBaseEntity* instance = entity ? (CBaseEntity*)entity->pvPrivateData : NULL;
	if ( instance && instance->Classify() != CLASS_NONE && instance->Classify() != CLASS_MACHINE )
		state->eflags |= EFLAG_FLESH_SOUND;
	else
		state->eflags &= ~EFLAG_FLESH_SOUND;
	return 1;
}

const char* GetGameDescription()
{
	return UTIL_IsGame( "czero" ) ? "Condition Zero" : "Counter-Strike";
}

int InconsistentFile( const edict_t*, const char* filename, char* message )
{
	// Server doesn't care?
	if ( CVAR_GET_FLOAT( "mp_consistency" ) != 1 )
		return 0;

	// Default behavior is to kick the player
	sprintf( message, "Server is enforcing file consistency for %s\n", filename );
	return 1;
}

void CheckStartMoney()
{
	if ( (int)startmoney.value > 16000 )
		CVAR_SET_FLOAT( "mp_startmoney", 16000 );
	else if ( (int)startmoney.value < 800 )
		CVAR_SET_FLOAT( "mp_startmoney", 800 );
}

extern "C" int CountTeams()
{
	int terrorists = 0;
	int counterTerrorists = 0;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player->m_iTeam == UNASSIGNED || ( entity->pev->flags & FL_DORMANT ) || player->m_iTeam == SPECTATOR )
			continue;
		if ( player->m_iTeam == CT )
			++counterTerrorists;
		else if ( player->m_iTeam == TERRORIST )
			++terrorists;
	}
	return counterTerrorists - terrorists;
}

extern "C" int CountTeamPlayers( int team )
{
	int count = 0;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		if ( entity->pev->flags & FL_DORMANT )
			continue;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player->m_iTeam == team )
			++count;
	}
	return count;
}

int CountPlayersInServer()
{
	int count = 0;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player->m_iTeam != UNASSIGNED )
			++count;
	}
	return count;
}

CBaseEntity* EntityFromUserID( int userID )
{
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player->m_iTeam != UNASSIGNED && GETPLAYERUSERID( entity->edict() ) == userID )
			return entity;
	}
	return NULL;
}

TeamName SelectDefaultTeam()
{
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	TeamName team;
	if ( rules->m_iNumTerrorist < rules->m_iNumCT )
		team = TERRORIST;
	else if ( rules->m_iNumTerrorist > rules->m_iNumCT )
		team = CT;
	else if ( rules->m_iNumTerroristWins < rules->m_iNumCTWins )
		team = TERRORIST;
	else if ( rules->m_iNumTerroristWins > rules->m_iNumCTWins )
		team = CT;
	else
		team = RANDOM_LONG( 0, 1 ) ? TERRORIST : CT;
	if ( !rules->TeamFull( team ) )
		return team;
	team = team == CT ? TERRORIST : CT;
	return rules->TeamFull( team ) ? UNASSIGNED : team;
}

void ListPlayers( CBasePlayer* current )
{
	char message[120] = "";
	char number[12];
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		if ( entity->pev->flags & FL_DORMANT )
			continue;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		_itoa( GETPLAYERUSERID( player->edict() ), number, 10 );
		strcpy( message, "\n" );
		strcat( message, number );
		strcat( message, " : " );
		strcat( message, STRING( player->pev->netname ) );
		ClientPrint( current->pev, HUD_PRINTCONSOLE, message );
	}
	ClientPrint( current->pev, HUD_PRINTCONSOLE, "\n" );
}

void ProcessKickVote( CBasePlayer* votingPlayer, CBasePlayer* kickPlayer )
{
	if ( !votingPlayer || !kickPlayer )
		return;
	TeamName team = votingPlayer->m_iTeam;
	int voteID = votingPlayer->m_iCurrentKickVote;
	int teamPlayers = CountTeamPlayers( team );
	if ( teamPlayers <= 2 )
		return;
	int votes = 0;
	CBaseEntity* entity = NULL;
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player && player->m_iTeam != UNASSIGNED && player->m_iTeam == votingPlayer->m_iTeam &&
			player->m_iCurrentKickVote == voteID )
			++votes;
	}
	float percentage = kick_percent.value;
	if ( percentage < 0 )
	{
		CVAR_SET_STRING( "mp_kickpercent", "0.0" );
		percentage = 0;
	}
	else if ( percentage > 1 )
	{
		CVAR_SET_STRING( "mp_kickpercent", "1.0" );
		percentage = 1;
	}
	if ( (int)( teamPlayers * percentage + 0.5 ) > votes )
		return;
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "#Game_kicked", STRING( kickPlayer->pev->netname ) );
	SERVER_COMMAND( UTIL_VarArgs( "kick # %d\n", voteID ) );
	while ( ( entity = UTIL_FindEntityByClassname( entity, "player" ) ) != NULL )
	{
		if ( FNullEnt( entity->edict() ) )
			break;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)entity->pev );
		if ( player && player->m_iTeam != UNASSIGNED && player->m_iTeam == votingPlayer->m_iTeam &&
			player->m_iCurrentKickVote == voteID )
			player->m_iCurrentKickVote = 0;
	}
}

void Radio1( CBasePlayer* player, int slot )
{
	if ( !( gpGlobals->time > player->m_flRadioTime ) || player->m_iRadioMessages <= 0 )
		return;
	--player->m_iRadioMessages;
	player->m_flRadioTime = (float)( gpGlobals->time + 1.5 );
	switch ( slot )
	{
	case 1:
		player->Radio( "%!MRAD_COVERME", "#Cover_me", 100, true );
		break;
	case 2:
		player->Radio( "%!MRAD_TAKEPOINT", "#You_take_the_point", 100, true );
		break;
	case 3:
		player->Radio( "%!MRAD_POSITION", "#Hold_this_position", 100, true );
		break;
	case 4:
		player->Radio( "%!MRAD_REGROUP", "#Regroup_team", 100, true );
		break;
	case 5:
		player->Radio( "%!MRAD_FOLLOWME", "#Follow_me", 100, true );
		break;
	case 6:
		player->Radio( "%!MRAD_HITASSIST", "#Taking_fire", 100, true );
		break;
	}
	TheBots->OnEvent( (GameEventType)( EVENT_START_RADIO_1 + slot ), player, NULL );
}

void Radio2( CBasePlayer* player, int slot )
{
	if ( !( gpGlobals->time > player->m_flRadioTime ) || player->m_iRadioMessages <= 0 )
		return;
	--player->m_iRadioMessages;
	player->m_flRadioTime = (float)( gpGlobals->time + 1.5 );
	switch ( slot )
	{
	case 1:
		player->Radio( "%!MRAD_GO", "#Go_go_go", 100, true );
		break;
	case 2:
		player->Radio( "%!MRAD_FALLBACK", "#Team_fall_back", 100, true );
		break;
	case 3:
		player->Radio( "%!MRAD_STICKTOG", "#Stick_together_team", 100, true );
		break;
	case 4:
		player->Radio( "%!MRAD_GETINPOS", "#Get_in_position_and_wait", 100, true );
		break;
	case 5:
		player->Radio( "%!MRAD_STORMFRONT", "#Storm_the_front", 100, true );
		break;
	case 6:
		player->Radio( "%!MRAD_REPORTIN", "#Report_in_team", 100, true );
		break;
	}
	TheBots->OnEvent( (GameEventType)( EVENT_START_RADIO_2 + slot ), player, NULL );
}

void Radio3( CBasePlayer* player, int slot )
{
	if ( !( gpGlobals->time > player->m_flRadioTime ) || player->m_iRadioMessages <= 0 )
		return;
	--player->m_iRadioMessages;
	player->m_flRadioTime = (float)( gpGlobals->time + 1.5 );
	switch ( slot )
	{
	case 1:
		if ( RANDOM_LONG( 0, 1 ) )
			player->Radio( "%!MRAD_AFFIRM", "#Affirmative", 100, true );
		else
			player->Radio( "%!MRAD_ROGER", "#Roger_that", 100, true );
		break;
	case 2:
		player->Radio( "%!MRAD_ENEMYSPOT", "#Enemy_spotted", 100, true );
		break;
	case 3:
		player->Radio( "%!MRAD_BACKUP", "#Need_backup", 100, true );
		break;
	case 4:
		player->Radio( "%!MRAD_CLEAR", "#Sector_clear", 100, true );
		break;
	case 5:
		player->Radio( "%!MRAD_INPOS", "#In_position", 100, true );
		break;
	case 6:
		player->Radio( "%!MRAD_REPRTINGIN", "#Reporting_in", 100, true );
		break;
	case 7:
		player->Radio( "%!MRAD_BLOW", "#Get_out_of_there", 100, true );
		break;
	case 8:
		player->Radio( "%!MRAD_NEGATIVE", "#Negative", 100, true );
		break;
	case 9:
		player->Radio( "%!MRAD_ENEMYDOWN", "#Enemy_down", 100, true );
		break;
	}
	TheBots->OnEvent( (GameEventType)( EVENT_START_RADIO_3 + slot ), player, NULL );
}

void ClientDisconnect( edict_t* entity )
{
	CBasePlayer* player = dynamic_cast<CBasePlayer*>( CBaseEntity::Instance( entity ) );
	if ( !g_fGameOver )
	{
		if ( entity->v.netname )
			UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Game_disconnected", STRING( entity->v.netname ) );
		CSound* sound = CSoundEnt::SoundPointerForIndex( CSoundEnt::ClientSoundIndex( entity ) );
		if ( sound )
			sound->Reset();
		entity->v.takedamage = DAMAGE_NO;
		entity->v.solid = SOLID_NOT;
		entity->v.flags = FL_DORMANT;
		if ( player )
			player->SetThink( NULL );
		UTIL_SetOrigin( &entity->v, entity->v.origin );
		g_pGameRules->ClientDisconnected( entity );
	}
	if ( player && player->IsBot() )
		TheBots->ClientDisconnect( player );
}

void respawn( entvars_t* pev, BOOL )
{
	if ( gpGlobals->coop || gpGlobals->deathmatch )
	{
		CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
		if ( rules->m_iTotalRoundsPlayed > 0 )
			rules->m_bSkipSpawn = false;
		CBasePlayer* player = GetClassPtr( (CBasePlayer*)pev );
		if ( rules->IsCareer() && rules->ShouldSkipSpawn() && player->IsAlive() )
			g_skipCareerInitialSpawn = true;
		player->Spawn();
		g_skipCareerInitialSpawn = false;
	}
	else if ( pev->deadflag > DEAD_NO )
		SERVER_COMMAND( "reload\n" );
}

void ClientKill( edict_t* entity )
{
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	CBasePlayer* player = (CBasePlayer*)CBaseEntity::Instance( &entity->v );
	if ( player->pev->iuser1 || player->m_iJoiningState != JOINED ||
		!( player->m_fNextSuicideTime <= gpGlobals->time ) )
		return;
	player->m_LastHitGroup = HITGROUP_GENERIC;
	player->m_fNextSuicideTime = gpGlobals->time + 1;
	entity->v.health = 0;
	player->Killed( &entity->v, GIB_NEVER );
	if ( rules->m_pVIP == player )
		rules->m_iConsecutiveVIP = 10;
}

void ClientUserInfoChanged( edict_t* entity, char* info )
{
	// Is the client spawned yet?
	if ( !entity->pvPrivateData )
		return;
	CBasePlayer* player = (CBasePlayer*)GET_PRIVATE( entity );
	const char* name = g_engfuncs.pfnInfoKeyValue( info, "name" );
	int index = player->entindex();
	if ( entity->v.netname && *STRING( entity->v.netname ) && !FStrEq( STRING( entity->v.netname ), name ) )
	{
		char cleanName[32];
		_snprintf( cleanName, sizeof( cleanName ), "%s", name );
		cleanName[sizeof( cleanName ) - 1] = 0;
		for ( char* character = cleanName; *character; ++character )
		{
			if ( *character == '%' || *character == '&' )
				*character = ' ';
		}
		if ( cleanName[0] == '#' )
			cleanName[0] = '*';
		if ( player->pev->deadflag )
		{
			player->m_bHasChangedName = true;
			_snprintf( player->m_szNewName, sizeof( player->m_szNewName ), "%s", cleanName );
			ClientPrint( player->pev, HUD_PRINTTALK, "#Name_change_at_respawn" );
			g_engfuncs.pfnSetClientKeyValue( index, info, "name", (char*)STRING( entity->v.netname ) );
		}
		else
		{
			g_engfuncs.pfnSetClientKeyValue( index, info, "name", cleanName );
			MESSAGE_BEGIN( MSG_BROADCAST, gmsgSayText );
				WRITE_BYTE( index );
				WRITE_STRING( "#Cstrike_Name_Change" );
				WRITE_STRING( STRING( entity->v.netname ) );
				WRITE_STRING( cleanName );
			MESSAGE_END();
			const char* team = GetTeam( player->m_iTeam );
			const char* authID = GETPLAYERAUTHID( entity );
			int userID = GETPLAYERUSERID( entity );
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" changed name to \"%s\"\n",
				STRING( entity->v.netname ), userID, authID, team, cleanName );
		}
	}
	g_pGameRules->ClientUserInfoChanged( GetClassPtr( (CBasePlayer*)&entity->v ), info );
}

void DropSecondary( CBasePlayer* player )
{
	if ( player->HasShield() )
	{
		if ( player->HasShield() && player->m_bShieldDrawn && player->m_pActiveItem )
			( (CBasePlayerWeapon*)player->m_pActiveItem )->SecondaryAttack();
		player->m_bShieldDrawn = false;
	}
	CBasePlayerItem* weapon = player->m_rgpPlayerItems[2];
	if ( weapon )
		player->DropPlayerItem( STRING( weapon->pev->classname ) );
}

bool CanBuyThis( CBasePlayer* player, int weapon )
{
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	bool shield = player->HasShield();
	if ( weapon == WEAPON_ELITE && shield )
		return false;
	if ( ( weapon == WEAPON_SHIELDGUN ) & player->HasShield() )
		return false;
	CBasePlayerItem* pistol = player->m_rgpPlayerItems[2];
	if ( pistol && pistol->m_iId == WEAPON_ELITE && weapon == WEAPON_SHIELDGUN )
		return false;
	CBasePlayerItem* primary = player->m_rgpPlayerItems[1];
	if ( ( primary && primary->m_iId == weapon ) || ( pistol && pistol->m_iId == weapon ) )
	{
		if ( g_bClientPrintEnable )
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Cstrike_Already_Own_Weapon" );
		return false;
	}
	if ( CanBuyWeaponByMaptype( player->m_iTeam, (WeaponIdType)weapon, rules->m_iMapHasVIPSafetyZone == 1 ) )
		return true;
	if ( g_bClientPrintEnable )
		ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Buy_This" );
	return false;
}

void BuyWeaponByWeaponID( CBasePlayer* player, WeaponIdType weaponID )
{
	if ( !player->CanPlayerBuy( true ) || !CanBuyThis( player, weaponID ) )
		return;
	const WeaponInfoStruct* info = GetWeaponInfo( weaponID );
	if ( !info || !info->entityName )
		return;
	if ( player->m_iAccount < info->cost )
	{
		if ( g_bClientPrintEnable )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
			BlinkAccount( player, 2 );
		}
		return;
	}
	if ( IsPrimaryWeapon( weaponID ) )
		DropPrimary( player );
	else
		DropSecondary( player );
	player->GiveNamedItem( info->entityName );
	player->AddAccount( -info->cost, true );
	if ( TheTutor )
		TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
}

void BuyPistol( CBasePlayer* player, int slot )
{
	if ( !player->CanPlayerBuy( true ) || (unsigned int)( slot - 1 ) > 4 )
		return;
	WeaponIdType weapon;
	int price;
	const char* item;
	switch ( slot )
	{
	case 1:
		weapon = WEAPON_GLOCK18;
		price = GLOCK18_PRICE;
		item = "weapon_glock18";
		break;
	case 2:
		weapon = WEAPON_USP;
		price = USP_PRICE;
		item = "weapon_usp";
		break;
	case 3:
		weapon = WEAPON_P228;
		price = P228_PRICE;
		item = "weapon_p228";
		break;
	case 4:
		weapon = WEAPON_DEAGLE;
		price = DEAGLE_PRICE;
		item = "weapon_deagle";
		break;
	default:
		if ( player->m_iTeam == CT )
		{
			weapon = WEAPON_FIVESEVEN;
			price = FIVESEVEN_PRICE;
			item = "weapon_fiveseven";
		}
		else
		{
			weapon = WEAPON_ELITE;
			price = ELITE_PRICE;
			item = "weapon_elite";
		}
		break;
	}
	if ( !CanBuyThis( player, weapon ) )
		return;
	if ( player->m_iAccount < price )
	{
		if ( g_bClientPrintEnable )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
			BlinkAccount( player, 2 );
		}
		return;
	}
	DropSecondary( player );
	player->GiveNamedItem( item );
	player->AddAccount( -price, true );
	if ( TheTutor )
		TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
}

void BuyShotgun( CBasePlayer* player, int slot )
{
	if ( !player->CanPlayerBuy( true ) || (unsigned int)( slot - 1 ) > 1 )
		return;
	WeaponIdType weapon = slot == 2 ? WEAPON_XM1014 : WEAPON_M3;
	int price = slot == 2 ? XM1014_PRICE : M3_PRICE;
	const char* item = slot == 2 ? "weapon_xm1014" : "weapon_m3";
	if ( !CanBuyThis( player, weapon ) )
		return;
	if ( player->m_iAccount < price )
	{
		if ( g_bClientPrintEnable )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
			BlinkAccount( player, 2 );
		}
		return;
	}
	DropPrimary( player );
	player->GiveNamedItem( item );
	player->AddAccount( -price, true );
	if ( TheTutor )
		TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
}

void BuySubMachineGun( CBasePlayer* player, int slot )
{
	if ( !player->CanPlayerBuy( true ) || (unsigned int)( slot - 1 ) > 3 )
		return;
	WeaponIdType weapon;
	int price;
	const char* item;
	switch ( slot )
	{
	case 2:
		weapon = WEAPON_MP5N;
		price = MP5NAVY_PRICE;
		item = "weapon_mp5navy";
		break;
	case 3:
		weapon = WEAPON_UMP45;
		price = UMP45_PRICE;
		item = "weapon_ump45";
		break;
	case 4:
		weapon = WEAPON_P90;
		price = P90_PRICE;
		item = "weapon_p90";
		break;
	default:
		weapon = player->m_iTeam == CT ? WEAPON_TMP : WEAPON_MAC10;
		price = player->m_iTeam == CT ? TMP_PRICE : MAC10_PRICE;
		item = player->m_iTeam == CT ? "weapon_tmp" : "weapon_mac10";
		break;
	}
	if ( !CanBuyThis( player, weapon ) )
		return;
	if ( player->m_iAccount < price )
	{
		if ( g_bClientPrintEnable )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
			BlinkAccount( player, 2 );
		}
		return;
	}
	DropPrimary( player );
	player->GiveNamedItem( item );
	player->AddAccount( -price, true );
	if ( TheTutor )
		TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
}

void BuyRifle( CBasePlayer* player, int slot )
{
	if ( !player->CanPlayerBuy( true ) )
		return;
	bool counterTerrorist = player->m_iTeam == CT;
	WeaponIdType weapon;
	int price;
	const char* item;
	switch ( slot )
	{
	case 1:
		weapon = counterTerrorist ? WEAPON_FAMAS : WEAPON_GALIL;
		price = counterTerrorist ? FAMAS_PRICE : GALIL_PRICE;
		item = counterTerrorist ? "weapon_famas" : "weapon_galil";
		break;
	case 2:
		weapon = counterTerrorist ? WEAPON_SCOUT : WEAPON_AK47;
		price = counterTerrorist ? SCOUT_PRICE : AK47_PRICE;
		item = counterTerrorist ? "weapon_scout" : "weapon_ak47";
		break;
	case 3:
		weapon = counterTerrorist ? WEAPON_M4A1 : WEAPON_SCOUT;
		price = counterTerrorist ? M4A1_PRICE : SCOUT_PRICE;
		item = counterTerrorist ? "weapon_m4a1" : "weapon_scout";
		break;
	case 4:
		weapon = counterTerrorist ? WEAPON_AUG : WEAPON_SG552;
		price = AUG_PRICE;
		item = counterTerrorist ? "weapon_aug" : "weapon_sg552";
		break;
	case 5:
		weapon = counterTerrorist ? WEAPON_SG550 : WEAPON_AWP;
		price = counterTerrorist ? SG550_PRICE : AWP_PRICE;
		item = counterTerrorist ? "weapon_sg550" : "weapon_awp";
		break;
	case 6:
		weapon = counterTerrorist ? WEAPON_AWP : WEAPON_G3SG1;
		price = counterTerrorist ? AWP_PRICE : G3SG1_PRICE;
		item = counterTerrorist ? "weapon_awp" : "weapon_g3sg1";
		break;
	default:
		return;
	}
	if ( !CanBuyThis( player, weapon ) )
		return;
	if ( player->m_iAccount < price )
	{
		if ( g_bClientPrintEnable )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
			BlinkAccount( player, 2 );
		}
		return;
	}
	DropPrimary( player );
	player->GiveNamedItem( item );
	player->AddAccount( -price, true );
	if ( TheTutor )
		TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
}

void BuyMachineGun( CBasePlayer* player, int slot )
{
	if ( !player->CanPlayerBuy( true ) || slot != 1 || !CanBuyThis( player, WEAPON_M249 ) )
		return;
	if ( player->m_iAccount < M249_PRICE )
	{
		if ( g_bClientPrintEnable )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
			BlinkAccount( player, 2 );
		}
		return;
	}
	DropPrimary( player );
	player->GiveNamedItem( "weapon_m249" );
	player->AddAccount( -M249_PRICE, true );
	if ( TheTutor )
		TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
}

void BuyItem( CBasePlayer* player, int slot )
{
	if ( !player->CanPlayerBuy( true ) )
		return;
	if ( (unsigned int)( slot - 1 ) > ( player->m_iTeam == CT ? 7U : 5U ) )
		return;
	int price;
	const char* item = NULL;
	const char* feedback = NULL;
	switch ( slot )
	{
	case 1:
		if ( !( player->pev->armorvalue < 100 ) )
		{
			if ( g_bClientPrintEnable )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Already_Have_Kevlar" );
			return;
		}
		price = KEVLAR_PRICE;
		item = "item_kevlar";
		if ( player->m_iKevlar == 2 )
			feedback = "#Already_Have_Helmet_Bought_Kevlar";
		break;
	case 2:
		item = "item_assaultsuit";
		if ( player->m_iKevlar == 2 )
		{
			if ( player->pev->armorvalue >= 100 )
			{
				if ( g_bClientPrintEnable )
					ClientPrint( player->pev, HUD_PRINTCENTER, "#Already_Have_Kevlar_Helmet" );
				return;
			}
			price = KEVLAR_PRICE;
			feedback = "#Already_Have_Helmet_Bought_Kevlar";
		}
		else if ( player->pev->armorvalue < 100 )
			price = ASSAULTSUIT_PRICE;
		else
		{
			price = HELMET_PRICE;
			feedback = "#Already_Have_Kevlar_Bought_Helmet";
		}
		break;
	case 3:
		if ( player->AmmoInventory( CBasePlayer::GetAmmoIndex( "Flashbang" ) ) > 1 )
		{
			if ( g_bClientPrintEnable )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Carry_Anymore" );
			return;
		}
		price = FLASHBANG_PRICE;
		item = "weapon_flashbang";
		break;
	case 4:
		if ( player->AmmoInventory( CBasePlayer::GetAmmoIndex( "HEGrenade" ) ) > 0 )
		{
			if ( g_bClientPrintEnable )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Carry_Anymore" );
			return;
		}
		price = HEGRENADE_PRICE;
		item = "weapon_hegrenade";
		break;
	case 5:
		if ( player->AmmoInventory( CBasePlayer::GetAmmoIndex( "SmokeGrenade" ) ) > 0 )
		{
			if ( g_bClientPrintEnable )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Carry_Anymore" );
			return;
		}
		price = SMOKEGRENADE_PRICE;
		item = "weapon_smokegrenade";
		break;
	case 6:
		if ( player->m_bHasNightVision )
		{
			if ( g_bClientPrintEnable )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Already_Have_One" );
			return;
		}
		price = NVG_PRICE;
		break;
	case 7:
		if ( player->m_iTeam != CT || !( (CHalfLifeMultiplay*)g_pGameRules )->m_bMapHasBombTarget )
			return;
		if ( player->m_bHasDefuser )
		{
			if ( g_bClientPrintEnable )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Already_Have_One" );
			return;
		}
		price = DEFUSEKIT_PRICE;
		break;
	case 8:
		if ( !CanBuyThis( player, WEAPON_SHIELDGUN ) )
			return;
		price = SHIELDGUN_PRICE;
		break;
	default:
		return;
	}
	if ( player->m_iAccount < price )
	{
		if ( g_bClientPrintEnable )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Not_Enough_Money" );
			BlinkAccount( player, 2 );
		}
		return;
	}
	if ( feedback && g_bClientPrintEnable )
		ClientPrint( player->pev, HUD_PRINTCENTER, feedback );
	if ( item )
	{
		player->GiveNamedItem( item );
		player->AddAccount( -price, true );
	}
	else if ( slot == 6 )
	{
		if ( !( player->m_flDisplayHistory & HISTORY_NIGHTVISION ) )
		{
			player->HintMessage( "#Hint_use_nightvision", false, false );
			player->m_flDisplayHistory |= HISTORY_NIGHTVISION;
		}
		EMIT_SOUND( player->edict(), CHAN_ITEM, "items/equip_nvg.wav", 1, ATTN_NORM );
		player->m_bHasNightVision = true;
		player->AddAccount( -price, true );
		SendItemStatus( player );
	}
	else if ( slot == 7 )
	{
		player->m_bHasDefuser = true;
		MESSAGE_BEGIN( MSG_ONE, gmsgStatusIcon, NULL, player->edict() );
			WRITE_BYTE( 1 );
			WRITE_STRING( "defuser" );
			WRITE_BYTE( 0 );
			WRITE_BYTE( 160 );
			WRITE_BYTE( 0 );
		MESSAGE_END();
		player->pev->body = 1;
		player->AddAccount( -price, true );
		EMIT_SOUND( player->edict(), CHAN_ITEM, "items/kevlar.wav", 1, ATTN_NORM );
		SendItemStatus( player );
		player->SetScoreboardAttributes( NULL );
	}
	else if ( slot == 8 )
	{
		DropPrimary( player );
		player->GiveShield( true );
		player->AddAccount( -price, true );
		EMIT_SOUND( player->edict(), CHAN_ITEM, "items/gunpickup2.wav", 1, ATTN_NORM );
	}
	if ( TheTutor )
		TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
}

BOOL HandleRadioAliasCommands( CBasePlayer* player, const char* command )
{
	if ( FStrEq( command, "coverme" ) )
		Radio1( player, 1 );
	else if ( FStrEq( command, "takepoint" ) )
		Radio1( player, 2 );
	else if ( FStrEq( command, "holdpos" ) )
		Radio1( player, 3 );
	else if ( FStrEq( command, "regroup" ) )
		Radio1( player, 4 );
	else if ( FStrEq( command, "followme" ) )
		Radio1( player, 5 );
	else if ( FStrEq( command, "takingfire" ) )
		Radio1( player, 6 );
	else if ( FStrEq( command, "go" ) )
		Radio2( player, 1 );
	else if ( FStrEq( command, "fallback" ) )
		Radio2( player, 2 );
	else if ( FStrEq( command, "sticktog" ) )
		Radio2( player, 3 );
	else if ( FStrEq( command, "getinpos" ) )
		Radio2( player, 4 );
	else if ( FStrEq( command, "stormfront" ) )
		Radio2( player, 5 );
	else if ( FStrEq( command, "report" ) )
		Radio2( player, 6 );
	else if ( FStrEq( command, "roger" ) )
		Radio3( player, 1 );
	else if ( FStrEq( command, "enemyspot" ) )
		Radio3( player, 2 );
	else if ( FStrEq( command, "needbackup" ) )
		Radio3( player, 3 );
	else if ( FStrEq( command, "sectorclear" ) )
		Radio3( player, 4 );
	else if ( FStrEq( command, "inposition" ) )
		Radio3( player, 5 );
	else if ( FStrEq( command, "reportingin" ) )
		Radio3( player, 6 );
	else if ( FStrEq( command, "getout" ) )
		Radio3( player, 7 );
	else if ( FStrEq( command, "negative" ) )
		Radio3( player, 8 );
	else if ( FStrEq( command, "enemydown" ) )
		Radio3( player, 9 );
	else
		return FALSE;
	return TRUE;
}

BOOL HandleBuyAliasCommands( CBasePlayer* player, const char* command )
{
	BOOL handled = FALSE;
	WeaponIdType weapon = WEAPON_NONE;
	const char* failureName = BuyAliasToWeaponID( command, &weapon );
	if ( weapon != WEAPON_NONE )
	{
		handled = TRUE;
		CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
		if ( CanBuyWeaponByMaptype( player->m_iTeam, weapon, rules->m_iMapHasVIPSafetyZone == 1 ) )
			BuyWeaponByWeaponID( player, weapon );
		else if ( failureName )
		{
			if ( g_bClientPrintEnable )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Alias_Not_Avail", failureName );
		}
		else if ( g_bClientPrintEnable )
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Buy_This" );
	}
	else if ( FStrEq( command, "primammo" ) )
	{
		handled = TRUE;
		if ( BuyAmmo( player, 1, true ) )
		{
			while ( BuyAmmo( player, 1, false ) )
				;
			if ( TheTutor )
				TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
		}
	}
	else if ( FStrEq( command, "secammo" ) )
	{
		handled = TRUE;
		if ( BuyAmmo( player, 2, true ) )
		{
			while ( BuyAmmo( player, 2, false ) )
				;
			if ( TheTutor )
				TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
		}
	}
	else if ( FStrEq( command, "vest" ) )
	{
		BuyItem( player, 1 );
		handled = TRUE;
	}
	else if ( FStrEq( command, "vesthelm" ) )
	{
		BuyItem( player, 2 );
		handled = TRUE;
	}
	else if ( FStrEq( command, "flash" ) )
	{
		BuyItem( player, 3 );
		handled = TRUE;
	}
	else if ( FStrEq( command, "hegren" ) )
	{
		BuyItem( player, 4 );
		handled = TRUE;
	}
	else if ( FStrEq( command, "sgren" ) )
	{
		BuyItem( player, 5 );
		handled = TRUE;
	}
	else if ( FStrEq( command, "nvgs" ) )
	{
		BuyItem( player, 6 );
		handled = TRUE;
	}
	else if ( FStrEq( command, "defuser" ) )
	{
		if ( player->m_iTeam == CT )
			BuyItem( player, 7 );
		else if ( g_bClientPrintEnable )
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Alias_Not_Avail", "#Bomb_Defusal_Kit" );
		handled = TRUE;
	}
	else if ( FStrEq( command, "shield" ) )
	{
		if ( player->m_iTeam == CT )
			BuyItem( player, 8 );
		else if ( g_bClientPrintEnable )
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Alias_Not_Avail", "#TactShield_Desc" );
		handled = TRUE;
	}
	player->BuildRebuyStruct();
	return handled;
}

void ClientPrecache()
{
	const char* sounds[] =
	{
		"weapons/dryfire_pistol.wav", "weapons/dryfire_rifle.wav",
		"player/pl_shot1.wav", "player/pl_die1.wav",
		"player/headshot1.wav", "player/headshot2.wav", "player/headshot3.wav",
		"player/bhit_flesh-1.wav", "player/bhit_flesh-2.wav", "player/bhit_flesh-3.wav",
		"player/bhit_kevlar-1.wav", "player/bhit_helmet-1.wav",
		"player/die1.wav", "player/die2.wav", "player/die3.wav", "player/death6.wav",
		"radio/locknload.wav", "radio/letsgo.wav", "radio/moveout.wav", "radio/com_go.wav",
		"radio/rescued.wav", "radio/rounddraw.wav",
		"items/kevlar.wav", "items/ammopickup2.wav", "items/nvg_on.wav", "items/nvg_off.wav", "items/equip_nvg.wav",
		"weapons/c4_beep1.wav", "weapons/c4_beep2.wav", "weapons/c4_beep3.wav",
		"weapons/c4_beep4.wav", "weapons/c4_beep5.wav", "weapons/c4_explode1.wav",
		"weapons/c4_plant.wav", "weapons/c4_disarm.wav", "weapons/c4_disarmed.wav",
		"weapons/explode3.wav", "weapons/explode4.wav", "weapons/explode5.wav",
		"player/sprayer.wav", "player/pl_fallpain2.wav", "player/pl_fallpain3.wav",
		"player/pl_snow1.wav", "player/pl_snow2.wav", "player/pl_snow3.wav",
		"player/pl_snow4.wav", "player/pl_snow5.wav", "player/pl_snow6.wav",
		"player/pl_step1.wav", "player/pl_step2.wav", "player/pl_step3.wav", "player/pl_step4.wav",
		"common/npc_step1.wav", "common/npc_step2.wav", "common/npc_step3.wav", "common/npc_step4.wav",
		"player/pl_metal1.wav", "player/pl_metal2.wav", "player/pl_metal3.wav", "player/pl_metal4.wav",
		"player/pl_dirt1.wav", "player/pl_dirt2.wav", "player/pl_dirt3.wav", "player/pl_dirt4.wav",
		"player/pl_duct1.wav", "player/pl_duct2.wav", "player/pl_duct3.wav", "player/pl_duct4.wav",
		"player/pl_grate1.wav", "player/pl_grate2.wav", "player/pl_grate3.wav", "player/pl_grate4.wav",
		"player/pl_slosh1.wav", "player/pl_slosh2.wav", "player/pl_slosh3.wav", "player/pl_slosh4.wav",
		"player/pl_tile1.wav", "player/pl_tile2.wav", "player/pl_tile3.wav", "player/pl_tile4.wav", "player/pl_tile5.wav",
		"player/pl_swim1.wav", "player/pl_swim2.wav", "player/pl_swim3.wav", "player/pl_swim4.wav",
		"player/pl_ladder1.wav", "player/pl_ladder2.wav", "player/pl_ladder3.wav", "player/pl_ladder4.wav",
		"player/pl_wade1.wav", "player/pl_wade2.wav", "player/pl_wade3.wav", "player/pl_wade4.wav",
		"debris/wood1.wav", "debris/wood2.wav", "debris/wood3.wav",
		"plats/train_use1.wav", "plats/vehicle_ignition.wav", "buttons/spark5.wav", "buttons/spark6.wav",
		"debris/glass1.wav", "debris/glass2.wav", "debris/glass3.wav",
		"items/flashlight1.wav", "items/flashlight1.wav", "common/bodysplat.wav",
		"player/pl_pain2.wav", "player/pl_pain4.wav", "player/pl_pain5.wav", "player/pl_pain6.wav", "player/pl_pain7.wav"
	};
	for ( int sound = 0; sound < ARRAYSIZE( sounds ); ++sound )
		PRECACHE_SOUND( (char*)sounds[sound] );
	int modelCount = UTIL_IsGame( "czero" ) ? ARRAYSIZE( sPlayerModelFiles ) : ARRAYSIZE( sPlayerModelFiles ) - 2;
	for ( int model = 0; model < modelCount; ++model )
		PRECACHE_MODEL( sPlayerModelFiles[model] );
	for ( int skin = 100; skin < 200; ++skin )
	{
		const char* model = TheBotProfiles->GetCustomSkinFname( skin );
		if ( !model )
			break;
		PRECACHE_MODEL( (char*)model );
	}
	const char* weaponModels[] =
	{
		"models/p_ak47.mdl", "models/p_aug.mdl", "models/p_awp.mdl", "models/p_c4.mdl", "models/w_c4.mdl",
		"models/p_deagle.mdl", "models/shield/p_shield_deagle.mdl",
		"models/p_flashbang.mdl", "models/shield/p_shield_flashbang.mdl",
		"models/p_hegrenade.mdl", "models/shield/p_shield_hegrenade.mdl",
		"models/p_glock18.mdl", "models/shield/p_shield_glock18.mdl",
		"models/p_p228.mdl", "models/shield/p_shield_p228.mdl",
		"models/p_smokegrenade.mdl", "models/shield/p_shield_smokegrenade.mdl",
		"models/p_usp.mdl", "models/shield/p_shield_usp.mdl",
		"models/p_fiveseven.mdl", "models/shield/p_shield_fiveseven.mdl",
		"models/p_knife.mdl", "models/shield/p_shield_knife.mdl",
		"models/w_flashbang.mdl", "models/w_hegrenade.mdl", "models/p_sg550.mdl", "models/p_g3sg1.mdl",
		"models/p_m249.mdl", "models/p_m3.mdl", "models/p_m4a1.mdl", "models/p_mac10.mdl", "models/p_mp5.mdl",
		"models/p_ump45.mdl", "models/p_p90.mdl", "models/p_scout.mdl", "models/p_sg552.mdl",
		"models/w_smokegrenade.mdl", "models/p_tmp.mdl", "models/p_elite.mdl", "models/p_xm1014.mdl",
		"models/p_galil.mdl", "models/p_famas.mdl", "models/p_shield.mdl", "models/w_shield.mdl"
	};
	for ( int model = 0; model < ARRAYSIZE( weaponModels ); ++model )
		PRECACHE_MODEL( (char*)weaponModels[model] );
	Vector zero = g_vecZero;
	Vector minimum( -38, -24, -41 );
	Vector maximum( 38, 24, 41 );
	for ( int model = 0; model < modelCount; ++model )
		ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, sPlayerModelFiles[model] );
	for ( int skin = 100; skin < 200; ++skin )
	{
		const char* model = TheBotProfiles->GetCustomSkinFname( skin );
		if ( !model )
			break;
		ENGINE_FORCE_UNMODIFIED( force_model_specifybounds_if_avail, minimum, maximum, model );
	}
	const char* exactFiles[] =
	{
		"sprites/black_smoke1.spr", "sprites/black_smoke2.spr", "sprites/black_smoke3.spr", "sprites/black_smoke4.spr",
		"sprites/fast_wallpuff1.spr", "sprites/smokepuff.spr", "sprites/gas_puff_01.spr",
		"sprites/scope_arc.tga", "sprites/scope_arc_nw.tga", "sprites/scope_arc_ne.tga", "sprites/scope_arc_sw.tga"
	};
	for ( int file = 0; file < ARRAYSIZE( exactFiles ); ++file )
		ENGINE_FORCE_UNMODIFIED( force_exactfile, zero, zero, exactFiles[file] );
	bool conditionZero = UTIL_IsGame( "czero" );
	minimum = Vector( conditionZero ? -13.0f : -12.0f, -6, -22 );
	maximum = Vector( conditionZero ? 13.0f : 12.0f, 6, 22 );
	const char* pistols[] =
	{
		"models/p_deagle.mdl", "models/p_p228.mdl", "models/p_elite.mdl",
		"models/p_usp.mdl", "models/p_fiveseven.mdl", "models/p_glock18.mdl"
	};
	for ( int model = 0; model < ARRAYSIZE( pistols ); ++model )
		ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, pistols[model] );
	minimum = Vector( conditionZero ? -26.0f : -25.0f, -19, -21 );
	maximum = Vector( conditionZero ? 26.0f : 25.0f, 23, 21 );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_xm1014.mdl" );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_m3.mdl" );
	minimum = Vector( -23, conditionZero ? -9.0f : -8.0f, -20 );
	maximum = Vector( 23, conditionZero ? 17.0f : 8.0f, 20 );
	const char* submachineGuns[] =
	{
		"models/p_mac10.mdl", "models/p_mp5.mdl", "models/p_ump45.mdl", "models/p_tmp.mdl", "models/p_p90.mdl"
	};
	for ( int model = 0; model < ARRAYSIZE( submachineGuns ); ++model )
		ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, submachineGuns[model] );
	minimum = conditionZero ? Vector( -38, -33, -22 ) : Vector( -31, -8, -21 );
	maximum = conditionZero ? Vector( 38, 15, 35 ) : Vector( 31, 12, 31 );
	const char* rifles[] =
	{
		"models/p_ak47.mdl", "models/p_aug.mdl", "models/p_awp.mdl", "models/p_g3sg1.mdl", "models/p_sg550.mdl",
		"models/p_m4a1.mdl", "models/p_scout.mdl", "models/p_sg552.mdl", "models/p_famas.mdl", "models/p_galil.mdl"
	};
	for ( int model = 0; model < ARRAYSIZE( rifles ); ++model )
		ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, rifles[model] );
	minimum = Vector( conditionZero ? -30.0f : -24.0f, -10, -20 );
	maximum = Vector( conditionZero ? 30.0f : 25.0f, conditionZero ? 11.0f : 10.0f, 20 );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_m249.mdl" );
	minimum = Vector( -7, -7, -15 );
	maximum = Vector( 7, 7, 15 );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_c4.mdl" );
	minimum = Vector( conditionZero ? -17.0f : -4.0f, -8, -3 );
	maximum = Vector( conditionZero ? 17.0f : 3.0f, 7, 3 );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/w_c4.mdl" );
	minimum = Vector( -7, conditionZero ? -3.0f : -2.0f, -18 );
	maximum = Vector( 7, 2, 18 );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_flashbang.mdl" );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_hegrenade.mdl" );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_smokegrenade.mdl" );
	minimum = Vector( -5, -5, conditionZero ? -7.0f : -5.0f );
	maximum = Vector( 5, 5, 14 );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/w_flashbang.mdl" );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/w_hegrenade.mdl" );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/w_smokegrenade.mdl" );
	minimum = Vector( -7, -11, -18 );
	maximum = Vector( 7, 6, 18 );
	ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, "models/p_knife.mdl" );
	minimum = Vector( conditionZero ? -21.0f : -16.0f, conditionZero ? -25.0f : -8.0f, -54 );
	maximum = Vector( conditionZero ? 21.0f : 16.0f, conditionZero ? 23.0f : 6.0f, 24 );
	const char* shieldModels[] =
	{
		"models/shield/p_shield_deagle.mdl", "models/shield/p_shield_fiveseven.mdl",
		"models/shield/p_shield_flashbang.mdl", "models/shield/p_shield_glock18.mdl",
		"models/shield/p_shield_hegrenade.mdl", "models/shield/p_shield_knife.mdl",
		"models/shield/p_shield_p228.mdl", "models/shield/p_shield_smokegrenade.mdl", "models/shield/p_shield_usp.mdl"
	};
	for ( int model = 0; model < ARRAYSIZE( shieldModels ); ++model )
		ENGINE_FORCE_UNMODIFIED( force_model_specifybounds, minimum, maximum, shieldModels[model] );
	const char* menuSounds[] =
	{
		"common/wpn_hudoff.wav", "common/wpn_hudon.wav", "common/wpn_moveselect.wav",
		"common/wpn_select.wav", "common/wpn_denyselect.wav",
		"player/geiger6.wav", "player/geiger5.wav", "player/geiger4.wav",
		"player/geiger3.wav", "player/geiger2.wav", "player/geiger1.wav"
	};
	for ( int sound = 0; sound < ARRAYSIZE( menuSounds ); ++sound )
		PRECACHE_SOUND( (char*)menuSounds[sound] );
	if ( giPrecacheGrunt )
		UTIL_PrecacheOther( "enemy_terrorist" );
	g_iShadowSprite = (unsigned short)PRECACHE_MODEL( "sprites/shadow_circle.spr" );
	const char* sprites[] =
	{
		"sprites/wall_puff1.spr", "sprites/wall_puff2.spr", "sprites/wall_puff3.spr", "sprites/wall_puff4.spr",
		"sprites/black_smoke1.spr", "sprites/black_smoke2.spr", "sprites/black_smoke3.spr", "sprites/black_smoke4.spr",
		"sprites/fast_wallpuff1.spr", "sprites/pistol_smoke1.spr", "sprites/pistol_smoke2.spr",
		"sprites/rifle_smoke1.spr", "sprites/rifle_smoke2.spr", "sprites/rifle_smoke3.spr"
	};
	for ( int sprite = 0; sprite < ARRAYSIZE( sprites ); ++sprite )
		PRECACHE_MODEL( (char*)sprites[sprite] );
	PRECACHE_GENERIC( "sprites/scope_arc.tga" );
	PRECACHE_GENERIC( "sprites/scope_arc_nw.tga" );
	PRECACHE_GENERIC( "sprites/scope_arc_ne.tga" );
	PRECACHE_GENERIC( "sprites/scope_arc_sw.tga" );
	m_usResetDecals = PRECACHE_EVENT( 1, "events/decal_reset.sc" );
}

void ClientPutInServer( edict_t* entity )
{
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	CBasePlayer* player = GetClassPtr( (CBasePlayer*)&entity->v );
	player->SetCustomDecalFrames( -1 );
	player->SetPrefsFromUserinfo( g_engfuncs.pfnGetInfoKeyBuffer( entity ) );
	if ( !g_pGameRules->IsMultiplayer() )
	{
		player->Spawn();
		return;
	}
	player->m_bNotKilled = true;
	player->m_iIgnoreGlobalChat = 0;
	player->m_iTeamKills = 0;
	player->m_bJustConnected = true;
	player->Spawn();
	player->m_iNumSpawns = 0;
	player->m_bTeamChanged = false;
	CheckStartMoney();
	player->m_iAccount = (int)startmoney.value;
	player->m_fGameHUDInitialized = FALSE;
	player->m_flDisplayHistory &= ~2u;
	player->pev->flags |= FL_SPECTATOR;
	player->pev->solid = SOLID_NOT;
	player->pev->movetype = MOVETYPE_NOCLIP;
	player->pev->effects = EF_NODRAW;
	player->pev->effects |= EF_NOINTERP;
	player->pev->takedamage = DAMAGE_NO;
	player->pev->deadflag = DEAD_DEAD;
	player->pev->velocity = g_vecZero;
	player->pev->punchangle = g_vecZero;
	player->m_iTeam = UNASSIGNED;
	player->m_iJoiningState = READINGLTEXT;
	player->pev->fixangle = 1;
	player->m_iHostagesKilled = 0;
	player->m_iMapVote = 0;
	player->m_iCurrentKickVote = 0;
	player->m_iModelName = MODEL_URBAN;
	player->m_iMenu = Menu_OFF;
	player->m_bContextHelp = true;
	player->m_bHasNightVision = false;
	player->m_fDeadTime = 0;
	player->has_disconnected = false;
	player->ClearAutoBuyData();
	player->m_rebuyString = NULL;
	g_engfuncs.pfnSetClientMaxspeed( player->edict(), 1 );
	SET_MODEL( player->edict(), "models/player.mdl" );
	player->SetThink( NULL );
	player->m_pIntroCamera = UTIL_FindEntityByClassname( NULL, "trigger_camera" );
	if ( rules && rules->m_bMapHasCameras == 2 )
		rules->m_bMapHasCameras = player->m_pIntroCamera ? 1 : 0;
	bool cameraPositioned = false;
	if ( player->m_pIntroCamera )
	{
		CBaseEntity* target = UTIL_FindEntityByTargetname( NULL, STRING( player->m_pIntroCamera->pev->target ) );
		if ( target && player->m_pIntroCamera )
		{
			Vector direction = (target->pev->origin - player->m_pIntroCamera->pev->origin).Normalize();
			Vector cameraAngles = UTIL_VecToAngles( direction );
			cameraAngles.x = -cameraAngles.x;
			UTIL_SetOrigin( player->pev, player->m_pIntroCamera->pev->origin );
			player->pev->angles = cameraAngles;
			player->pev->v_angle = player->pev->angles;
			player->m_fIntroCamTime = gpGlobals->time + 6;
			entity->v.view_ofs = g_vecZero;
			cameraPositioned = true;
		}
	}
	if ( !cameraPositioned )
	{
		if ( rules )
		{
			player->m_iTeam = CT;
			rules->GetPlayerSpawnSpot( player );
		}
		player->m_iTeam = UNASSIGNED;
		player->pev->v_angle = g_vecZero;
		player->pev->angles = gpGlobals->v_forward;
	}
	TheBots->OnEvent( EVENT_PLAYER_CHANGED_TEAM, player, NULL );
	player->m_iJoiningState = SHOWLTEXT;
	static char name[128];
	strcpy( name, STRING( player->pev->netname ) );
	for ( char* character = name; *character; ++character )
	{
		if ( *character == '%' )
			*character = ' ';
	}
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, "#Game_connected", name[0] ? name : "<unconnected>" );
}

void ServerActivate( edict_t* entities, int count, int maxClients )
{

	// Every call to ServerActivate should be matched by a call to ServerDeactivate
	g_serveractive = 1;
	EmptyEntityHashTable();
	for ( int index = 0; index < count; ++index )
	{
		edict_t* entity = &entities[index];
		if ( entity->free || index < maxClients || !entity->pvPrivateData )
			continue;
		CBaseEntity* instance = CBaseEntity::Instance( entity );
		if ( instance && !(instance->pev->flags & FL_DORMANT) )
		{
			AddEntityHashValue( &entity->v, STRING( entity->v.classname ), CLASSNAME );
			instance->Activate();
		}
		else
			ALERT( at_console, "Can't instance %s\n", STRING( entity->v.classname ) );
	}
	LinkUserMessages();
	CareerSessionRestoreTasks();
	WriteSigonMessages();
	if ( g_pGameRules )
		g_pGameRules->CheckMapConditions();
	TheBots->ServerActivate();
	if ( g_pHostages )
		g_pHostages->ServerActivate();
}

void ServerDeactivate()
{
	// It's possible that the engine will call this function more times than is necessary
	//  Therefore, only run it one time for each call to ServerActivate 
	if ( g_serveractive != 1 )
		return;
	g_serveractive = 0;
	CareerSessionOnServerDeactivate();
	g_pGameRules->ServerDeactivate();
	CLocalNav::Reset();
	TheBots->ServerDeactivate();
	if ( g_pHostages )
		g_pHostages->ServerDeactivate();
}

void StartFrame()
{
	if ( g_pGameRules )
		g_pGameRules->Think();
	if ( g_fGameOver )
		return;
	CLocalNav::Think();
	static cvar_t* skill;
	if ( !skill )
		skill = CVAR_GET_POINTER( "skill" );
	gpGlobals->teamplay = 1;
	g_iSkillLevel = skill ? (int)skill->value : 0;
	TheBots->StartFrame();
	if ( TheTutor )
		TheTutor->StartFrame( gpGlobals->time );
	++g_ulFrameCount;
}

void Host_Say( edict_t* entity, int teamOnly )
{
	const char* command = CMD_ARGV( 0 );
	CBasePlayer* sender = GetClassPtr( (CBasePlayer*)&entity->v );
	if ( sender->m_flLastTalk != 0 && gpGlobals->time - sender->m_flLastTalk < 0.66 )
		return;
	sender->m_flLastTalk = gpGlobals->time;
	int senderDead = sender->pev->deadflag;
	if ( !CMD_ARGC() )
		return;
	char temporary[256];
	char* message;
	if ( !stricmp( command, "say" ) || !stricmp( command, "say_team" ) )
	{
		if ( CMD_ARGC() <= 1 )
			return;
		message = (char*)CMD_ARGS();
	}
	else
	{
		if ( CMD_ARGC() > 1 )
			sprintf( temporary, "%s %s", command, CMD_ARGS() );
		else
			sprintf( temporary, "%s", command );
		message = temporary;
	}

// remove quotes if present
	if ( *message == '"' )
	{
		++message;
		message[strlen( message ) - 1] = 0;
	}
	if ( !message || !*message || !Q_UnicodeValidate( message ) )
		return;
	Q_StripUnprintableAndSpace( message );
	if ( !*message )
		return;
	const char* format;
	const char* consoleFormat;
	const char* placeName = NULL;
	bool consoleUsesPlaceName = false;
	if ( teamOnly != 1 )
	{
		if ( senderDead )
		{
			if ( sender->m_iTeam == SPECTATOR )
			{
				consoleFormat = "*SPEC* %s : %s";
				format = "#Cstrike_Chat_AllSpec";
			}
			else
			{
				consoleFormat = "*DEAD* %s : %s";
				format = "#Cstrike_Chat_AllDead";
			}
		}
		else
		{
			consoleFormat = "%s : %s";
			format = "#Cstrike_Chat_All";
		}
	}
	else
	{
		if ( sender->m_iTeam == CT || sender->m_iTeam == TERRORIST )
			placeName = TheBotPhrases->IDToName( TheNavAreaGrid.GetPlace( &sender->pev->origin ) );
		if ( sender->m_iTeam == CT )
		{
			if ( senderDead )
			{
				consoleFormat = "*DEAD*(Counter-Terrorist) %s : %s";
				format = "#Cstrike_Chat_CT_Dead";
			}
			else if ( placeName )
			{
				consoleFormat = "(Counter-Terrorist) %s @ %s : %s";
				format = "#Cstrike_Chat_CT_Loc";
				consoleUsesPlaceName = true;
			}
			else
			{
				consoleFormat = "(Counter-Terrorist) %s : %s";
				format = "#Cstrike_Chat_CT";
			}
		}
		else if ( sender->m_iTeam == TERRORIST )
		{
			if ( senderDead )
			{
				consoleFormat = "*DEAD*(Terrorist) %s : %s";
				format = "#Cstrike_Chat_T_Dead";
			}
			else if ( placeName )
			{
				consoleFormat = "(Terrorist) %s @ %s : %s";
				format = "#Cstrike_Chat_T_Loc";
				consoleUsesPlaceName = true;
			}
			else
			{
				consoleFormat = "(Terrorist) %s : %s";
				format = "#Cstrike_Chat_T";
			}
		}
		else
		{
			consoleFormat = "(Spectator) %s : %s";
			format = "#Cstrike_Chat_Spec";
		}
	}
	char text[128];
	text[0] = 0;
	int maxLength = 126 - (int)strlen( text ) - (int)strlen( format ) - 1;
	if ( placeName )
		maxLength -= 1 + (int)strlen( placeName );
	if ( maxLength < (int)strlen( message ) )
		message[maxLength] = 0;
	for ( char* character = message; *character; ++character )
	{
		if ( *character == '%' && (character[1] & 0xDF) != 0 && character[1] != 'l' )
			*character = ' ';
	}
	strcat( text, message );
	strcat( text, "\n" );
	CBaseEntity* receiverEntity = NULL;
	while ( (receiverEntity = UTIL_FindEntityByClassname( receiverEntity, "player" )) != NULL )
	{
		if ( FNullEnt( receiverEntity->edict() ) )
			break;
		if ( !receiverEntity->pev || entity == receiverEntity->edict() || !receiverEntity->IsNetClient() )
			continue;
		CBasePlayer* receiver = (CBasePlayer*)receiverEntity;
		if ( gpGlobals->deathmatch && ((CHalfLifeMultiplay*)g_pGameRules)->m_VoiceGameMgr.PlayerHasBlockedPlayer( receiver, sender ) )
			continue;
		if ( teamOnly && receiver->m_iTeam != sender->m_iTeam )
			continue;
		if ( (receiver->pev->deadflag != 0) != (senderDead != 0) && !(sender->pev->flags & FL_PROXY) )
			continue;
		if ( receiver->m_iIgnoreGlobalChat != 0 &&
			(receiver->m_iIgnoreGlobalChat != 1 || receiver->m_iTeam != sender->m_iTeam) )
			continue;
		MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, receiver->edict() );
			WRITE_BYTE( ENTINDEX( entity ) );
			WRITE_STRING( format );
			WRITE_STRING( "" );
			WRITE_STRING( text );
			if ( placeName )
				WRITE_STRING( placeName );
		MESSAGE_END();
	}
	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, entity->v.pContainingEntity );
		WRITE_BYTE( ENTINDEX( entity ) );
		WRITE_STRING( format );
		WRITE_STRING( "" );
		WRITE_STRING( text );
		if ( placeName )
			WRITE_STRING( placeName );
	MESSAGE_END();
	if ( consoleUsesPlaceName )
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( (char*)consoleFormat, STRING( sender->pev->netname ), placeName, text ) );
	else
		g_engfuncs.pfnServerPrint( UTIL_VarArgs( (char*)consoleFormat, STRING( sender->pev->netname ), text ) );
	if ( CVAR_GET_FLOAT( "mp_logmessages" ) != 0 )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" %s \"%s\"%s\n", STRING( sender->pev->netname ),
			GETPLAYERUSERID( sender->edict() ), GETPLAYERAUTHID( sender->edict() ), GetTeam( sender->m_iTeam ),
			teamOnly == 1 ? "say_team" : "say", message,
			sender->m_iTeam != SPECTATOR && senderDead ? " (dead)" : "" );
	}
}

void HandleMenu_ChooseAppearance( CBasePlayer* player, int slot )
{
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	int numSkins = UTIL_IsGame( "czero" ) ? 5 : 4;
	if ( slot <= 0 || slot > numSkins )
	{
		if ( !TheBotProfiles->GetCustomSkin( slot ) || !player->IsBot() )
			slot = RANDOM_LONG( 1, numSkins );
	}
	ModelName model;
	char* appearance;
	int modelFile;
	if ( player->m_iTeam == TERRORIST )
	{
		modelFile = 8;
		switch ( slot )
		{
		case 1: model = MODEL_TERROR; appearance = "terror"; break;
		case 2: model = MODEL_LEET; appearance = "leet"; break;
		case 3: model = MODEL_ARCTIC; appearance = "arctic"; break;
		case 4: model = MODEL_GUERILLA; appearance = "guerilla"; break;
		case 5:
			if ( UTIL_IsGame( "czero" ) )
			{
				model = MODEL_MILITIA;
				appearance = "militia";
				break;
			}
			[[fallthrough]];
		default:
			if ( TheBotProfiles->GetCustomSkinModelname( slot ) && player->IsBot() )
			{
				model = (ModelName)slot;
				appearance = (char*)TheBotProfiles->GetCustomSkinModelname( slot );
			}
			else
			{
				model = MODEL_TERROR;
				appearance = "terror";
			}
			break;
		}
	}
	else
	{
		modelFile = 9;
		switch ( slot )
		{
		case 1: model = MODEL_URBAN; appearance = "urban"; break;
		case 2: model = MODEL_GSG9; appearance = "gsg9"; break;
		case 3: model = MODEL_SAS; appearance = "sas"; break;
		case 4: model = MODEL_GIGN; appearance = "gign"; break;
		case 5:
			if ( UTIL_IsGame( "czero" ) )
			{
				model = MODEL_SPETSNAZ;
				appearance = "spetsnaz";
				break;
			}
			[[fallthrough]];
		default:
			if ( TheBotProfiles->GetCustomSkinModelname( slot ) && player->IsBot() )
			{
				model = (ModelName)slot;
				appearance = (char*)TheBotProfiles->GetCustomSkinModelname( slot );
			}
			else
			{
				model = MODEL_URBAN;
				appearance = "urban";
			}
			break;
		}
	}
	player->m_iMenu = Menu_OFF;
	if ( player->m_iJoiningState == JOINED )
		rules->CheckWinConditions();
	else if ( player->m_iJoiningState == PICKINGTEAM )
	{
		player->m_iJoiningState = GETINTOGAME;
		if ( rules->IsCareer() && !player->IsBot() )
			rules->CheckWinConditions();
	}
	player->pev->body = 0;
	player->m_iModelName = model;
	g_engfuncs.pfnSetClientKeyValue( player->entindex(), g_engfuncs.pfnGetInfoKeyBuffer( player->edict() ), "model", appearance );
	player->SetNewPlayerModel( sPlayerModelFiles[modelFile] );
	if ( rules->m_iMapHasVIPSafetyZone == 0 )
		rules->m_iMapHasVIPSafetyZone = UTIL_FindEntityByClassname( NULL, "func_vip_safetyzone" ) ? 1 : 2;
	if ( rules->m_iMapHasVIPSafetyZone == 1 && !rules->m_pVIP && player->m_iTeam == CT )
		player->MakeVIP();
}

void TeamChangeUpdate( CBasePlayer* player, int team_id )
{
	int t = team_id;
	if ( t == -1 )
		t = player->m_iTeam;
	MESSAGE_BEGIN( MSG_ALL, gmsgTeamInfo );
		WRITE_BYTE( player->entindex() );
		switch ( t )
		{
		case CT: WRITE_STRING( "CT" ); break;
		case TERRORIST: WRITE_STRING( "TERRORIST" ); break;
		case SPECTATOR: WRITE_STRING( "SPECTATOR" ); break;
		default: WRITE_STRING( "UNASSIGNED" ); break;
		}
	MESSAGE_END();
	if ( t != UNASSIGNED )
		player->SetScoreboardAttributes( NULL );
}

BOOL HandleMenu_ChooseTeam( CBasePlayer* player, int slot )
{
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	if ( player->m_bIsVIP && (player->pev->deadflag == DEAD_NO || rules->IsVIPQueueEmpty()) )
	{
		ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Switch_From_VIP" );
		CLIENT_COMMAND( player->edict(), "slot10\n" );
		return TRUE;
	}
	TeamName newTeam;
	switch ( slot )
	{
	case 1: newTeam = TERRORIST; break;
	case 2: newTeam = CT; break;
	case 3:
		if ( rules->m_iMapHasVIPSafetyZone != 1 || player->m_iTeam != CT )
			return FALSE;
		rules->AddToVIPQueue( player );
		CLIENT_COMMAND( player->edict(), "slot10\n" );
		return TRUE;
	case 5:
		newTeam = SelectDefaultTeam();
		if ( newTeam == UNASSIGNED )
		{
			if ( !(cv_bot_auto_vacate.value <= 0) && !player->IsBot() )
			{
				newTeam = RANDOM_LONG( 0, 1 ) ? CT : TERRORIST;
				if ( !UTIL_KickBotFromTeam( newTeam ) )
				{
					newTeam = newTeam == CT ? TERRORIST : CT;
					if ( !UTIL_KickBotFromTeam( newTeam ) )
						newTeam = UNASSIGNED;
				}
			}
			if ( newTeam == UNASSIGNED )
			{
				ClientPrint( player->pev, HUD_PRINTCENTER, "#All_Teams_Full" );
				return FALSE;
			}
		}
		break;
	case 6:
		if ( allow_spectators.value == 0 && !(player->pev->flags & FL_PROXY) )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Be_Spectator" );
			CLIENT_COMMAND( player->edict(), "slot10\n" );
			return FALSE;
		}
		if ( player->m_iTeam == SPECTATOR )
			return TRUE;
		{
			BOOL freezePeriod = g_pGameRules->IsFreezePeriod();
			if ( !freezePeriod && player->pev->deadflag == DEAD_NO )
			{
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Cannot_Be_Spectator" );
				CLIENT_COMMAND( player->edict(), "slot10\n" );
				return FALSE;
			}
			if ( freezePeriod && player->m_iTeam != UNASSIGNED && player->pev->deadflag == DEAD_NO )
			{
				ClientKill( player->edict() );
				player->pev->frags += 1;
			}
		}
		player->RemoveAllItems( TRUE );
		player->m_bHasC4 = false;
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" joined team \"SPECTATOR\"\n", STRING( player->pev->netname ),
			GETPLAYERUSERID( player->edict() ), GETPLAYERAUTHID( player->edict() ), GetTeam( player->m_iTeam ) );
		player->m_iAccount = 0;
		player->m_iTeam = SPECTATOR;
		player->m_iJoiningState = JOINED;
		MESSAGE_BEGIN( MSG_ONE, gmsgMoney, NULL, player->edict() );
			WRITE_LONG( player->m_iAccount );
			WRITE_BYTE( 0 );
		MESSAGE_END();
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgScoreInfo );
			WRITE_BYTE( player->entindex() );
			WRITE_SHORT( (int)player->pev->frags );
			WRITE_SHORT( player->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( 0 );
		MESSAGE_END();
		player->m_pIntroCamera = NULL;
		player->m_bTeamChanged = true;
		TheBots->OnEvent( EVENT_PLAYER_CHANGED_TEAM, player, NULL );
		TeamChangeUpdate( player, -1 );
		{
			edict_t* spawn = g_pGameRules->GetPlayerSpawnSpot( player );
			player->StartObserver( spawn->v.origin, spawn->v.angles );
		}
		MESSAGE_BEGIN( MSG_ALL, gmsgSpectator );
			WRITE_BYTE( player->entindex() );
			WRITE_BYTE( 1 );
		MESSAGE_END();
		if ( fadetoblack.value != 0 )
			UTIL_ScreenFade( player, g_vecZero, 0.001f, 0, 0, 0 );
		return TRUE;
	default:
		return FALSE;
	}
	if ( rules->TeamFull( newTeam ) &&
		(cv_bot_auto_vacate.value <= 0 || player->IsBot() || !UTIL_KickBotFromTeam( newTeam )) )
	{
		ClientPrint( player->pev, HUD_PRINTCENTER, newTeam == TERRORIST ? "#Terrorists_Full" : "#CTs_Full" );
		return FALSE;
	}
	if ( rules->TeamStacked( newTeam, player->m_iTeam ) )
	{
		ClientPrint( player->pev, HUD_PRINTCENTER, newTeam == TERRORIST ? "#Too_Many_Terrorists" : "#Too_Many_CTs" );
		return FALSE;
	}
	if ( !player->IsBot() )
	{
		TeamName humansTeam = UNASSIGNED;
		if ( !stricmp( humans_join_team.string, "CT" ) )
			humansTeam = CT;
		else if ( !stricmp( humans_join_team.string, "T" ) )
			humansTeam = TERRORIST;
		if ( humansTeam != UNASSIGNED && humansTeam != newTeam )
		{
			ClientPrint( player->pev, HUD_PRINTCENTER, newTeam == TERRORIST ? "#Humans_Join_Team_CT" : "#Humans_Join_Team_T" );
			return FALSE;
		}
	}
	if ( player->m_bTeamChanged && player->pev->deadflag != DEAD_NO )
	{
		ClientPrint( player->pev, HUD_PRINTCENTER, "#Only_1_Team_Change" );
		return FALSE;
	}
	if ( player->m_iTeam == SPECTATOR )
	{
		player->m_iIgnoreGlobalChat = 0;
		player->m_iTeamKills = 0;
		player->m_bNotKilled = true;
		CheckStartMoney();
		player->m_iAccount = (int)startmoney.value;
		player->pev->solid = SOLID_NOT;
		player->pev->movetype = MOVETYPE_NOCLIP;
		player->pev->effects = EF_NODRAW;
		player->pev->effects |= EF_NOINTERP;
		player->pev->takedamage = DAMAGE_NO;
		player->pev->deadflag = DEAD_DEAD;
		player->pev->velocity = g_vecZero;
		player->pev->punchangle = g_vecZero;
		player->m_iHostagesKilled = 0;
		player->m_iJoiningState = GETINTOGAME;
		player->m_bHasNightVision = false;
		player->m_fDeadTime = 0;
		player->has_disconnected = false;
		SendItemStatus( player );
		g_engfuncs.pfnSetClientMaxspeed( player->edict(), 1 );
		SET_MODEL( player->edict(), "models/player.mdl" );
	}
	if ( !rules->IsCareer() )
	{
		int slots = UTIL_IsGame( "czero" ) ? 63 : 31;
		ShowVGUIMenu( player, newTeam == CT ? 27 : 26, slots, newTeam == CT ? "#CT_Select" : "#Terrorist_Select" );
	}
	player->m_iMenu = Menu_ChooseAppearance;
	if ( player->pev->deadflag == DEAD_NO )
		ClientKill( player->edict() );
	TeamName oldTeam = player->m_iTeam;
	player->m_bTeamChanged = true;
	player->m_iTeam = newTeam;
	TheBots->OnEvent( EVENT_PLAYER_CHANGED_TEAM, player, NULL );
	TeamChangeUpdate( player, -1 );
	const char* name = STRING( player->pev->netname );
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, newTeam == TERRORIST ? "#Game_join_terrorist" : "#Game_join_ct",
		name[0] ? name : "<unconnected>" );
	UTIL_LogPrintf( "\"%s<%i><%s><%s>\" joined team \"%s\"\n", STRING( player->pev->netname ),
		GETPLAYERUSERID( player->edict() ), GETPLAYERAUTHID( player->edict() ), GetTeam( oldTeam ), GetTeam( newTeam ) );
	return TRUE;
}

void ClientCommand( edict_t* entity )
{
	const char* command = CMD_ARGV( 0 );
	if ( !entity->pvPrivateData )
		return;
	CHalfLifeMultiplay* rules = (CHalfLifeMultiplay*)g_pGameRules;
	CBasePlayer* player = GetClassPtr( (CBasePlayer*)&entity->v );
	if ( !strcmp( command, "say" ) || !strcmp( command, "say_team" ) || !strcmp( command, "fullupdate" ) )
	{
		TrackCommands tracked = !strcmp( command, "say" ) ? CMD_SAY :
			(!strcmp( command, "say_team" ) ? CMD_SAYTEAM : CMD_FULLUPDATE);
		if ( player->m_flLastCommandTime[tracked] <= gpGlobals->time )
		{
			player->m_flLastCommandTime[tracked] = (float)(gpGlobals->time + (tracked == CMD_FULLUPDATE ? 0.6 : 0.3));
			if ( tracked == CMD_FULLUPDATE )
				player->ForceClientDllUpdate();
			else
				Host_Say( entity, tracked == CMD_SAYTEAM ? 1 : 0 );
		}
		return;
	}
	if ( !strcmp( command, "vote" ) )
	{
		if ( player->m_flLastCommandTime[CMD_VOTE] > gpGlobals->time )
			return;
		player->m_flLastCommandTime[CMD_VOTE] = (float)(gpGlobals->time + 0.3);
		if ( !(player->m_flNextVoteTime <= gpGlobals->time) )
		{
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Wait_3_Seconds" );
			return;
		}
		player->m_flNextVoteTime = gpGlobals->time + 3;
		if ( player->m_iTeam == UNASSIGNED )
			return;
		int argumentCount = CMD_ARGC();
		const char* argument = CMD_ARGV( 1 );
		int userID;
		if ( strlen( argument ) - 1 > 5 || argumentCount != 2 || (userID = atoi( argument )) <= 0 )
		{
			ListPlayers( player );
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Game_vote_usage" );
			return;
		}
		if ( CountTeamPlayers( player->m_iTeam ) <= 2 )
		{
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Cannot_Vote_With_Less_Than_Three" );
			return;
		}
		CBaseEntity* votedEntity = EntityFromUserID( userID );
		if ( !votedEntity )
		{
			ListPlayers( player );
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Game_vote_player_not_found", UTIL_dtos1( userID ) );
			return;
		}
		CBasePlayer* votedPlayer = GetClassPtr( (CBasePlayer*)votedEntity->pev );
		if ( votedPlayer->m_iTeam != player->m_iTeam )
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Game_vote_players_on_your_team" );
		else if ( votedPlayer == player )
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Game_vote_not_yourself" );
		else
		{
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Game_vote_cast", UTIL_dtos1( userID ) );
			player->m_iCurrentKickVote = userID;
			ProcessKickVote( player, votedPlayer );
		}
		return;
	}
	if ( !strcmp( command, "listmaps" ) )
	{
		if ( player->m_flLastCommandTime[CMD_LISTMAPS] <= gpGlobals->time )
		{
			player->m_flLastCommandTime[CMD_LISTMAPS] = (float)(gpGlobals->time + 0.3);
			rules->DisplayMaps( player, 0 );
		}
		return;
	}
	if ( !strcmp( command, "votemap" ) )
	{
		if ( player->m_flLastCommandTime[CMD_VOTEMAP] > gpGlobals->time )
			return;
		player->m_flLastCommandTime[CMD_VOTEMAP] = (float)(gpGlobals->time + 0.3);
		if ( player->m_flNextVoteTime > gpGlobals->time )
		{
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Wait_3_Seconds" );
			return;
		}
		player->m_flNextVoteTime = gpGlobals->time + 3;
		if ( player->m_iTeam == UNASSIGNED )
			return;
		if ( gpGlobals->time < 180 )
		{
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Cannot_Vote_Map" );
			return;
		}
		bool invalidArguments = CMD_ARGC() != 2 || strlen( CMD_ARGV( 1 ) ) > 5;
		int map = atoi( CMD_ARGV( 1 ) );
		int mapCount = GetMapCount();
		if ( (unsigned int)(map - 1) > 99 || map > mapCount || invalidArguments )
		{
			rules->DisplayMaps( player, 0 );
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Game_votemap_usage" );
		}
		else if ( CountPlayersInServer() <= 1 )
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Cannot_Vote_Need_More_People" );
		else
		{
			if ( player->m_iMapVote )
			{
				--rules->m_iMapVotes[player->m_iMapVote - 1];
				if ( rules->m_iMapVotes[player->m_iMapVote - 1] < 0 )
					rules->m_iMapVotes[player->m_iMapVote - 1] = 0;
			}
			ClientPrint( player->pev, HUD_PRINTCONSOLE, "#Game_voted_for_map", UTIL_dtos1( map ) );
			player->m_iMapVote = map;
			rules->ProcessMapVote( player, map );
		}
		return;
	}
	if ( !strcmp( command, "timeleft" ) )
	{
		if ( gpGlobals->time <= player->m_iTimeCheckAllowed )
			return;
		if ( timelimit.value == 0 )
			ClientPrint( player->pev, HUD_PRINTTALK, "#Game_no_timelimit" );
		else
		{
			float remaining = g_flTimeLimit - gpGlobals->time;
			int seconds = (int)remaining;
			if ( seconds < 0 )
				seconds = 0;
			int minutes = seconds / 60;
			seconds %= 60;
			char secondsText[3];
			char* digits = UTIL_dtos2( seconds );
			secondsText[0] = seconds > 9 ? digits[0] : '0';
			secondsText[1] = seconds > 9 ? digits[1] : digits[0];
			secondsText[2] = 0;
			ClientPrint( player->pev, HUD_PRINTTALK, "#Game_timelimit", UTIL_dtos1( minutes ), secondsText );
		}
		float nextCheck = gpGlobals->time + 1;
		player->m_iTimeCheckAllowed = (int)nextCheck;
		return;
	}
	if ( !strcmp( command, "listplayers" ) )
	{
		if ( player->m_flLastCommandTime[CMD_LISTPLAYERS] <= gpGlobals->time )
		{
			player->m_flLastCommandTime[CMD_LISTPLAYERS] = (float)(gpGlobals->time + 0.3);
			ListPlayers( player );
		}
		return;
	}
	if ( !strcmp( command, "client_buy_open" ) )
	{
		if ( player->m_iMenu == Menu_OFF )
			player->m_iMenu = Menu_ClientBuy;
		if ( player->m_signals.GetState() & SIGNAL_BUY )
		{
			if ( TheTutor )
				TheTutor->OnEvent( EVENT_TUTOR_BUY_MENU_OPENNED, NULL, NULL );
		}
		else
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgBuyClose, NULL, player->edict() );
			MESSAGE_END();
		}
		return;
	}
	if ( !strcmp( command, "client_buy_close" ) )
	{
		if ( player->m_iMenu == Menu_ClientBuy )
			player->m_iMenu = Menu_OFF;
		return;
	}
	if ( !strcmp( command, "menuselect" ) )
	{
		int slot = atoi( CMD_ARGV( 1 ) );
		if ( !(player->m_iJoiningState != JOINED &&
			(player->m_iMenu == Menu_ChooseTeam || player->m_iMenu == Menu_ChooseAppearance)) && slot == 10 )
		{
			player->m_iMenu = Menu_OFF;
			return;
		}
		switch ( player->m_iMenu )
		{
		case Menu_OFF: return;
		case Menu_ChooseTeam:
			if ( !player->m_bVGUIMenus && !HandleMenu_ChooseTeam( player, slot ) )
			{
				ShowVGUIMenu( player, 2, player->m_iJoiningState == JOINED ? 531 : 19,
					player->m_iJoiningState == JOINED ? "#IG_Team_Select" : "#Team_Select" );
				player->m_iMenu = Menu_ChooseTeam;
			}
			break;
		case Menu_IGChooseTeam:
			if ( !player->m_bVGUIMenus )
				HandleMenu_ChooseTeam( player, slot );
			break;
		case Menu_ChooseAppearance:
			if ( !player->m_bVGUIMenus )
				HandleMenu_ChooseAppearance( player, slot );
			break;
		case Menu_Buy:
			if ( player->m_bVGUIMenus )
				break;
			switch ( slot )
			{
			case 1:
				ShowVGUIMenu( player, 29, 543, player->m_iTeam == CT ? "#CT_BuyPistol" : "#T_BuyPistol" );
				player->m_iMenu = Menu_BuyPistol;
				break;
			case 2:
				if ( rules->m_iMapHasVIPSafetyZone == 1 && player->m_iTeam == TERRORIST )
					ShowVGUIMenu( player, 30, 512, "#AS_BuyShotgun" );
				else
					ShowVGUIMenu( player, 30, 515, "#BuyShotgun" );
				player->m_iMenu = Menu_BuyShotgun;
				break;
			case 3:
				if ( rules->m_iMapHasVIPSafetyZone == 1 )
					ShowVGUIMenu( player, 32, player->m_iTeam == CT ? 527 : 517,
						player->m_iTeam == CT ? "#AS_CT_BuySubMachineGun" : "#AS_T_BuySubMachineGun" );
				else
					ShowVGUIMenu( player, 32, 527, player->m_iTeam == CT ? "#CT_BuySubMachineGun" : "#T_BuySubMachineGun" );
				player->m_iMenu = Menu_BuySubMachineGun;
				break;
			case 4:
				if ( rules->m_iMapHasVIPSafetyZone == 1 )
					ShowVGUIMenu( player, 31, player->m_iTeam == CT ? 541 : 531,
						player->m_iTeam == CT ? "#AS_CT_BuyRifle" : "#AS_T_BuyRifle" );
				else
					ShowVGUIMenu( player, 31, 575, player->m_iTeam == CT ? "#CT_BuyRifle" : "#T_BuyRifle" );
				player->m_iMenu = Menu_BuyRifle;
				break;
			case 5:
				if ( rules->m_iMapHasVIPSafetyZone == 1 && player->m_iTeam == TERRORIST )
					ShowVGUIMenu( player, 33, 512, "#AS_T_BuyMachineGun" );
				else
					ShowVGUIMenu( player, 33, 513, "#BuyMachineGun" );
				player->m_iMenu = Menu_BuyMachineGun;
				break;
			case 6:
			case 7:
				if ( player->m_signals.GetState() & SIGNAL_BUY )
				{
					int weaponSlot = slot == 6 ? 1 : 2;
					if ( BuyAmmo( player, weaponSlot, true ) )
					{
						while ( BuyAmmo( player, weaponSlot, false ) )
							;
						if ( TheTutor )
							TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
					}
					player->BuildRebuyStruct();
				}
				break;
			case 8:
				if ( player->m_signals.GetState() & SIGNAL_BUY )
				{
					if ( rules->m_bMapHasBombTarget )
						ShowVGUIMenu( player, 34, player->m_iTeam == CT ? 767 : 575,
							player->m_iTeam == CT ? "#DCT_BuyItem" : "#DT_BuyItem" );
					else
						ShowVGUIMenu( player, 34, player->m_iTeam == CT ? 703 : 575,
							player->m_iTeam == CT ? "#CT_BuyItem" : "#T_BuyItem" );
					player->m_iMenu = Menu_BuyItem;
				}
				break;
			default: return;
			}
			break;
		case Menu_BuyPistol: if ( !player->m_bVGUIMenus ) BuyPistol( player, slot ); break;
		case Menu_BuyRifle: if ( !player->m_bVGUIMenus ) BuyRifle( player, slot ); break;
		case Menu_BuyMachineGun: if ( !player->m_bVGUIMenus ) BuyMachineGun( player, slot ); break;
		case Menu_BuyShotgun: if ( !player->m_bVGUIMenus ) BuyShotgun( player, slot ); break;
		case Menu_BuySubMachineGun: if ( !player->m_bVGUIMenus ) BuySubMachineGun( player, slot ); break;
		case Menu_BuyItem: if ( !player->m_bVGUIMenus ) BuyItem( player, slot ); break;
		case Menu_Radio1: Radio1( player, slot ); break;
		case Menu_Radio2: Radio2( player, slot ); break;
		case Menu_Radio3: Radio3( player, slot ); break;
		default: ALERT( at_console, "ClientCommand(): Invalid menu selected\n" ); break;
		}
		return;
	}
	if ( !strcmp( command, "chooseteam" ) )
	{
		if ( player->m_iMenu != Menu_ChooseAppearance )
		{
			if ( player->m_bTeamChanged && player->pev->deadflag != DEAD_NO )
				ClientPrint( player->pev, HUD_PRINTCENTER, "#Only_1_Team_Change" );
			else if ( !rules->IsCareer() )
			{
				bool vipMenu = rules->m_iMapHasVIPSafetyZone == 1 && player->m_iJoiningState == JOINED && player->m_iTeam == CT;
				if ( g_pGameRules->IsFreezePeriod() || player->pev->deadflag != DEAD_NO )
					ShowVGUIMenu( player, 2, vipMenu ? 567 : 563, vipMenu ? "#IG_VIP_Team_Select_Spect" : "#IG_Team_Select_Spect" );
				else
					ShowVGUIMenu( player, 2, vipMenu ? 535 : 531, vipMenu ? "#IG_VIP_Team_Select" : "#IG_Team_Select" );
				player->m_iMenu = Menu_ChooseTeam;
			}
		}
		return;
	}
	if ( !strcmp( command, "showbriefing" ) )
	{
		if ( player->m_iMenu == Menu_OFF && g_szMapBriefingText[0] && player->m_iTeam != UNASSIGNED &&
			!(player->m_afPhysicsFlags & PFLAG_OBSERVER) )
		{
			player->MenuPrint( player, g_szMapBriefingText );
			player->m_bMissionBriefing = true;
		}
		return;
	}
	if ( !strcmp( command, "ignoremsg" ) )
	{
		if ( player->m_iIgnoreGlobalChat == 0 )
		{
			player->m_iIgnoreGlobalChat = 1;
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Ignore_Broadcast_Messages" );
		}
		else if ( player->m_iIgnoreGlobalChat == 1 )
		{
			player->m_iIgnoreGlobalChat = 2;
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Ignore_Broadcast_Team_Messages" );
		}
		else
		{
			player->m_iIgnoreGlobalChat = 0;
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Accept_All_Messages" );
		}
		return;
	}
	if ( !strcmp( command, "ignorerad" ) )
	{
		player->m_bIgnoreRadio = !player->m_bIgnoreRadio;
		ClientPrint( player->pev, HUD_PRINTCENTER, player->m_bIgnoreRadio ? "#Ignore_Radio" : "#Accept_Radio" );
		return;
	}
	if ( !strcmp( command, "become_vip" ) )
	{
		if ( player->m_iJoiningState == JOINED && player->m_iTeam == CT )
			rules->AddToVIPQueue( player );
		else
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Command_Not_Available" );
		return;
	}
	if ( !strcmp( command, "spectate" ) && (player->pev->flags & FL_PROXY) )
	{
		HandleMenu_ChooseTeam( player, 6 );
		return;
	}
	if ( !strcmp( command, "specmode" ) )
	{
		int mode = atoi( CMD_ARGV( 1 ) );
		if ( player->pev->iuser1 && player->m_canSwitchObserverModes )
			player->Observer_SetMode( mode );
		else
			player->m_iObserverLastMode = mode;
		if ( mode == 2 )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgADStop, NULL, player->edict() );
			MESSAGE_END();
		}
		return;
	}
	if ( !strcmp( command, "spec_set_ad" ) )
	{
		float autoDirector = (float)atof( CMD_ARGV( 1 ) );
		player->m_bObserverAutoDirector = autoDirector > 0;
		return;
	}
	if ( !strcmp( command, "follownext" ) )
	{
		if ( player->pev->iuser1 && player->m_canSwitchObserverModes )
			player->Observer_FindNextPlayer( atoi( CMD_ARGV( 1 ) ) != 0, NULL );
		return;
	}
	if ( !strcmp( command, "follow" ) )
	{
		if ( player->pev->iuser1 && player->m_canSwitchObserverModes )
			player->Observer_FindNextPlayer( false, CMD_ARGV( 1 ) );
		return;
	}
	if ( g_pGameRules->ClientCommand_DeadOrAlive( GetClassPtr( (CBasePlayer*)&entity->v ), command ) )
		return;
	if ( TheBots->ClientCommand( GetClassPtr( (CBasePlayer*)&entity->v ), command ) )
		return;
	if ( !strcmp( command, "mp_debug" ) )
	{
		UTIL_SetDprintfFlags( CMD_ARGV( 1 ) );
		return;
	}
	if ( !strcmp( command, "jointeam" ) )
	{
		if ( player->m_iMenu == Menu_ChooseAppearance )
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Command_Not_Available" );
		else
		{
			int slot = atoi( CMD_ARGV( 1 ) );
			if ( HandleMenu_ChooseTeam( player, slot ) )
				player->m_iMenu = slot == 6 || slot == 3 || player->m_bIsVIP ? Menu_OFF : Menu_ChooseAppearance;
			else
			{
				ShowVGUIMenu( player, 2, player->m_iJoiningState == JOINED ? 531 : 19,
					player->m_iJoiningState == JOINED ? "#IG_Team_Select" : "#Team_Select" );
				player->m_iMenu = Menu_ChooseTeam;
			}
		}
		return;
	}
	if ( !strcmp( command, "joinclass" ) )
	{
		int slot = atoi( CMD_ARGV( 1 ) );
		if ( player->m_iMenu == Menu_ChooseAppearance )
			HandleMenu_ChooseAppearance( player, slot );
		else
			ClientPrint( player->pev, HUD_PRINTCENTER, "#Command_Not_Available" );
		return;
	}
	if ( player->pev->deadflag != DEAD_NO )
		return;
	if ( !strcmp( command, "nightvision" ) )
	{
		if ( player->m_flLastCommandTime[CMD_NIGHTVISION] <= gpGlobals->time )
		{
			player->m_flLastCommandTime[CMD_NIGHTVISION] = (float)(gpGlobals->time + 0.3);
			if ( player->m_bHasNightVision )
			{
				bool enabled = !player->m_bNightVisionOn;
				const char* sound = enabled ? "items/nvg_on.wav" : "items/nvg_off.wav";
				EMIT_SOUND_DYN( player->edict(), CHAN_ITEM, sound, RANDOM_FLOAT( 0.92f, 1 ), ATTN_NORM, 0, PITCH_NORM );
				MESSAGE_BEGIN( MSG_ONE, gmsgNVGToggle, NULL, player->edict() );
					WRITE_BYTE( enabled ? 1 : 0 );
				MESSAGE_END();
				player->m_bNightVisionOn = enabled;
				for ( int index = 1; index <= gpGlobals->maxClients; ++index )
				{
					CBasePlayer* observer = (CBasePlayer*)UTIL_PlayerByIndex( index );
					if ( observer && observer->IsObservingPlayer( player ) )
					{
						EMIT_SOUND_DYN( observer->edict(), CHAN_ITEM, sound, RANDOM_FLOAT( 0.92f, 1 ), ATTN_NORM, 0, PITCH_NORM );
						MESSAGE_BEGIN( MSG_ONE, gmsgNVGToggle, NULL, observer->edict() );
							WRITE_BYTE( enabled ? 1 : 0 );
						MESSAGE_END();
						observer->m_bNightVisionOn = enabled;
					}
				}
			}
		}
		return;
	}
	if ( !strcmp( command, "radio1" ) || !strcmp( command, "radio2" ) || !strcmp( command, "radio3" ) )
	{
		int radio = command[5] - '1';
		ShowMenu( player, radio == 2 ? 1023 : 575, -1, FALSE, radio == 0 ? "#RadioA" : (radio == 1 ? "#RadioB" : "#RadioC") );
		player->m_iMenu = (_Menu)(Menu_Radio1 + radio);
		return;
	}
	if ( !strcmp( command, "drop" ) )
	{
		if ( player->HasShield() )
		{
			if ( player->m_pActiveItem && player->m_pActiveItem->m_iId == WEAPON_C4 )
				player->DropPlayerItem( "weapon_c4" );
			else
				player->DropShield( true );
		}
		else
			player->DropPlayerItem( CMD_ARGV( 1 ) );
		return;
	}
	if ( !strcmp( command, "fov" ) )
		return;
	if ( !strcmp( command, "use" ) )
	{
		const char* item = CMD_ARGV( 1 );
		GetClassPtr( (CBasePlayer*)&entity->v )->SelectItem( item );
		return;
	}
	if ( strstr( command, "weapon_" ) == command )
	{
		GetClassPtr( (CBasePlayer*)&entity->v )->SelectItem( command );
		return;
	}
	if ( !strcmp( command, "lastinv" ) )
	{
		GetClassPtr( (CBasePlayer*)&entity->v )->SelectLastItem();
		return;
	}
	if ( !strcmp( command, "buyammo1" ) || !strcmp( command, "buyammo2" ) )
	{
		if ( player->m_signals.GetState() & SIGNAL_BUY )
		{
			BuyAmmo( player, command[7] == '1' ? 1 : 2, true );
			player->BuildRebuyStruct();
			if ( TheTutor )
				TheTutor->OnEvent( EVENT_PLAYER_BOUGHT_SOMETHING, player, NULL );
		}
		return;
	}
	if ( !strcmp( command, "buyequip" ) )
	{
		if ( player->m_signals.GetState() & SIGNAL_BUY )
		{
			if ( rules->m_bMapHasBombTarget )
				ShowVGUIMenu( player, 34, player->m_iTeam == CT ? 767 : 575, player->m_iTeam == CT ? "#DCT_BuyItem" : "#DT_BuyItem" );
			else
				ShowVGUIMenu( player, 34, player->m_iTeam == CT ? 703 : 575, player->m_iTeam == CT ? "#CT_BuyItem" : "#T_BuyItem" );
			player->m_iMenu = Menu_BuyItem;
		}
		return;
	}
	if ( !strcmp( command, "buy" ) )
	{
		if ( player->m_signals.GetState() & SIGNAL_BUY )
		{
			ShowVGUIMenu( player, 28, 767, "#Buy" );
			player->m_iMenu = Menu_Buy;
			if ( TheBots )
				TheBots->OnEvent( EVENT_TUTOR_BUY_MENU_OPENNED, NULL, NULL );
		}
		return;
	}
	if ( !strcmp( command, "cl_setautobuy" ) )
	{
		player->ClearAutoBuyData();
		for ( int argument = 1; argument < CMD_ARGC(); ++argument )
			player->AddAutoBuyData( CMD_ARGV( argument ) );
		bool printEnabled = g_bClientPrintEnable;
		g_bClientPrintEnable = false;
		player->AutoBuy();
		g_bClientPrintEnable = printEnabled;
		return;
	}
	if ( !strcmp( command, "cl_setrebuy" ) )
	{
		if ( CMD_ARGC() == 2 )
		{
			player->InitRebuyData( CMD_ARGV( 1 ) );
			bool printEnabled = g_bClientPrintEnable;
			g_bClientPrintEnable = false;
			player->Rebuy();
			g_bClientPrintEnable = printEnabled;
		}
		return;
	}
	if ( !strcmp( command, "cl_autobuy" ) || !strcmp( command, "cl_rebuy" ) )
	{
		if ( player->m_signals.GetState() & SIGNAL_BUY )
		{
			bool printEnabled = g_bClientPrintEnable;
			g_bClientPrintEnable = false;
			if ( !strcmp( command, "cl_autobuy" ) )
				player->AutoBuy();
			else
				player->Rebuy();
			g_bClientPrintEnable = printEnabled;
		}
		return;
	}
	if ( !strcmp( command, "smartradio" ) )
		player->SmartRadio();
	else if ( !HandleBuyAliasCommands( player, command ) && !HandleRadioAliasCommands( player, command ) &&
		!g_pGameRules->ClientCommand( GetClassPtr( (CBasePlayer*)&entity->v ), command ) )
	{
		char unknown[128];
		strncpy( unknown, command, sizeof( unknown ) - 1 );
		unknown[sizeof( unknown ) - 1] = 0;
		ClientPrint( &entity->v, HUD_PRINTCONSOLE, "#Game_unknown_command", unknown );
	}
}
