#include "buy_presets.h"
#include "hud.h"
#include "ammohistory.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/Controls.h"
#include "VGUI/GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "buy_preset_debug.h"
#include <math.h>
#include <string.h>

struct WeaponDisplayNameInfo
{
	WeaponIdType	id;
	const char*		displayName;
};

static const WeaponDisplayNameInfo weaponDisplayNameInfo[] =
{
	{ WEAPON_P228, "#Cstrike_TitlesTXT_P228" },
	{ WEAPON_GLOCK, "#Cstrike_TitlesTXT_Glock18" },
	{ WEAPON_GLOCK18, "#Cstrike_TitlesTXT_Glock18" },
	{ WEAPON_SCOUT, "#Cstrike_TitlesTXT_Scout" },
	{ WEAPON_XM1014, "#Cstrike_TitlesTXT_AutoShotgun" },
	{ WEAPON_MAC10, "#Cstrike_TitlesTXT_Mac10_Short" },
	{ WEAPON_AUG, "#Cstrike_TitlesTXT_Aug" },
	{ WEAPON_ELITE, "#Cstrike_TitlesTXT_Beretta96G" },
	{ WEAPON_FIVESEVEN, "#Cstrike_TitlesTXT_ESFiveSeven" },
	{ WEAPON_UMP45, "#Cstrike_TitlesTXT_KMUMP45" },
	{ WEAPON_SG550, "#Cstrike_TitlesTXT_SG550" },
	{ WEAPON_GALIL, "#Cstrike_TitlesTXT_Galil" },
	{ WEAPON_FAMAS, "#Cstrike_TitlesTXT_Famas" },
	{ WEAPON_USP, "#Cstrike_TitlesTXT_USP45" },
	{ WEAPON_AWP, "#Cstrike_TitlesTXT_ArcticWarfareMagnum" },
	{ WEAPON_MP5N, "#Cstrike_TitlesTXT_mp5navy" },
	{ WEAPON_M249, "#Cstrike_TitlesTXT_ESM249" },
	{ WEAPON_M3, "#Cstrike_TitlesTXT_Leone12" },
	{ WEAPON_M4A1, "#Cstrike_TitlesTXT_M4A1_Short" },
	{ WEAPON_TMP, "#Cstrike_TitlesTXT_tmp" },
	{ WEAPON_G3SG1, "#Cstrike_TitlesTXT_G3SG1" },
	{ WEAPON_DEAGLE, "#Cstrike_TitlesTXT_DesertEagle" },
	{ WEAPON_SG552, "#Cstrike_TitlesTXT_SG552" },
	{ WEAPON_AK47, "#Cstrike_TitlesTXT_AK47" },
	{ WEAPON_P90, "#Cstrike_TitlesTXT_ESC90" },
	{ WEAPON_SHIELDGUN, "#Cstrike_TitlesTXT_TactShield" },
	{ WEAPON_NONE, "#Cstrike_CurrentWeapon" }
};

const wchar_t* WeaponIDToDisplayName( WeaponIdType weaponID )
{
	for ( const WeaponDisplayNameInfo* info = weaponDisplayNameInfo; info->displayName; ++info )
	{
		if ( info->id == weaponID )
			return vgui2::localize()->Find( info->displayName );
	}

	return NULL;
}

BuyPresetWeapon::BuyPresetWeapon()
{
	m_name = NULL;
	m_weaponID = WEAPON_NONE;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = 0;
	m_fillAmmo = true;
}

BuyPresetWeapon::BuyPresetWeapon( WeaponIdType weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
	m_ammoType = AMMO_CLIPS;
	m_ammoAmount = weaponID != WEAPON_NONE;
	m_fillAmmo = true;
}

BuyPresetWeapon& BuyPresetWeapon::operator=( const BuyPresetWeapon& other )
{
	m_name = other.m_name;
	m_weaponID = other.m_weaponID;
	m_ammoType = other.m_ammoType;
	m_ammoAmount = other.m_ammoAmount;
	m_fillAmmo = other.m_fillAmmo;
	return *this;
}

void BuyPresetWeapon::SetWeaponID( WeaponIdType weaponID )
{
	m_name = WeaponIDToDisplayName( weaponID );
	m_weaponID = weaponID;
}

WeaponIdType ClientWeaponIDToGameWeaponID( ClientWeaponIdType weaponID )
{
	if ( weaponID < ID_P228 || weaponID > ID_MAX )
		return WEAPON_NONE;

	switch ( weaponID )
	{
	case ID_SHIELD:
		return WEAPON_SHIELDGUN;
	case ID_HEGRENADE:
	case ID_SMOKEGRENADE:
	case ID_FLASHBANG:
	case ID_KNIFE:
		return WEAPON_NONE;
	default:
		return (WeaponIdType)weaponID;
	}
}

int CalcClipsNeeded( const BuyPresetWeapon* weapon, const WeaponInfoStruct* info, const int* ammo )
{
	if ( !info || !weapon || !info->buyClipSize || info->ammoType < 0 )
		return 0;

	switch ( weapon->GetAmmoType() )
	{
	case AMMO_ROUNDS:
	{
		int rounds = weapon->GetAmmoAmount() - ammo[info->ammoType];
		if ( rounds > 0 )
			return (int)( ceil( (double)rounds ) / info->buyClipSize );
		return 0;
	}
	case AMMO_PERCENT:
	{
		float wantedRounds = ( weapon->GetAmmoAmount() * info->maxRounds - ammo[info->ammoType] ) * 0.01f;
		int rounds = (int)wantedRounds;
		if ( rounds > 0 )
			return (int)ceilf( (float)rounds / info->buyClipSize );
		return 0;
	}
	case AMMO_CLIPS:
	{
		int clips = weapon->GetAmmoAmount();
		int maxClips = (int)ceilf( (float)info->maxRounds / info->buyClipSize );
		if ( weapon->GetWeaponID() == WEAPON_NONE && clips >= NUM_CLIPS_FOR_CURRENT )
			clips = maxClips;
		if ( clips > maxClips )
			clips = maxClips;

		clips -= ammo[info->ammoType] / info->buyClipSize;
		if ( ammo[info->ammoType] != info->maxRounds && clips >= 0 )
			return clips;
		return 0;
	}
	default:
		return 0;
	}
}

bool CanBuyWeapon( ClientWeaponIdType currentPrimaryID, ClientWeaponIdType currentSecondaryID,
	WeaponIdType weaponID )
{
	if ( ( weaponID == WEAPON_ELITE && currentPrimaryID == ID_SHIELD )
		|| ( currentSecondaryID == ID_ELITES && weaponID == WEAPON_SHIELDGUN ) )
	{
		return false;
	}

	return CanBuyWeaponByMaptype( g_iTeamNumber, weaponID, IsASMapType() );
}

void FillClientAmmo( int* ammo )
{
	memset( ammo, 0, sizeof( int ) * AMMO_MAX_TYPES );
	for ( int id = ID_NONE; id <= ID_MAX; ++id )
	{
		WeaponIdType weaponID = ClientWeaponIDToGameWeaponID( (ClientWeaponIdType)id );
		if ( weaponID == WEAPON_NONE )
			continue;

		const WeaponInfoStruct* info = GetWeaponInfo( weaponID );
		int rounds = gWR.CountAmmo( gWR.GetWeapon( id )->iAmmoType );
		if ( rounds > 0 && info && ammo[info->ammoType] < rounds )
			ammo[info->ammoType] = rounds;
	}
}

ClientWeaponIdType GetClientWeaponID( bool primary )
{
	for ( int id = ID_NONE; id <= ID_MAX; ++id )
	{
		WEAPON* weapon = gWR.GetWeapon( id );
		if ( !gWR.GetWeaponSlot( weapon->iSlot, weapon->iSlotPos ) )
			continue;

		WeaponIdType weaponID = ClientWeaponIDToGameWeaponID( (ClientWeaponIdType)id );
		if ( primary ? IsPrimaryWeapon( weaponID ) : IsSecondaryWeapon( weaponID ) )
			return (ClientWeaponIdType)id;
	}

	return ID_NONE;
}

static std::vector<BuyPresetWeapon> CareerWeaponListForClass( WeaponClassType weaponClassID )
{
	std::vector<BuyPresetWeapon> weapons;

	switch ( weaponClassID )
	{
	case WEAPONCLASS_PISTOL:
	{
		WeaponIdType currentWeaponID = ClientWeaponIDToGameWeaponID( GetClientWeaponID( false ) );
		if ( currentWeaponID != WEAPON_NONE )
			weapons.push_back( BuyPresetWeapon( currentWeaponID ) );

		if ( g_iTeamNumber == 2 )
		{
			weapons.push_back( BuyPresetWeapon( WEAPON_USP ) );
			weapons.push_back( BuyPresetWeapon( WEAPON_GLOCK18 ) );
		}
		else
		{
			weapons.push_back( BuyPresetWeapon( WEAPON_GLOCK18 ) );
			weapons.push_back( BuyPresetWeapon( WEAPON_USP ) );
		}
		weapons.push_back( BuyPresetWeapon( WEAPON_DEAGLE ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_ELITE ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_FIVESEVEN ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_P228 ) );
		break;
	}
	case WEAPONCLASS_SUBMACHINEGUN:
		weapons.push_back( BuyPresetWeapon( WEAPON_P90 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_UMP45 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_MP5N ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_TMP ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_MAC10 ) );
		break;
	case WEAPONCLASS_SHOTGUN:
		weapons.push_back( BuyPresetWeapon( WEAPON_XM1014 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_M3 ) );
		break;
	case WEAPONCLASS_MACHINEGUN:
		weapons.push_back( BuyPresetWeapon( WEAPON_M249 ) );
		break;
	case WEAPONCLASS_RIFLE:
		weapons.push_back( BuyPresetWeapon( WEAPON_SG552 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_AUG ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_AK47 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_M4A1 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_GALIL ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_FAMAS ) );
		break;
	case WEAPONCLASS_SNIPERRIFLE:
		weapons.push_back( BuyPresetWeapon( WEAPON_AWP ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_SG550 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_G3SG1 ) );
		weapons.push_back( BuyPresetWeapon( WEAPON_SCOUT ) );
		break;
	default:
		break;
	}

	return weapons;
}

static std::vector<BuyPresetWeapon> CareerWeaponListOrganized(
	const std::vector<BuyPresetWeapon>& weapons, const std::vector<BuyPresetWeapon>& source )
{
	std::vector<BuyPresetWeapon> result = weapons;

	for ( int sourceIndex = (int)source.size() - 1; sourceIndex >= 0; --sourceIndex )
	{
		for ( unsigned int weaponIndex = 1; weaponIndex < result.size(); ++weaponIndex )
		{
			if ( source[sourceIndex].GetWeaponID() == result[weaponIndex].GetWeaponID() )
			{
				result.insert( result.begin(), source[sourceIndex] );
				result.erase( result.begin() + weaponIndex + 1 );
				break;
			}
		}
	}

	for ( unsigned int sourceIndex = 0; sourceIndex < source.size(); ++sourceIndex )
	{
		bool found = false;
		for ( unsigned int weaponIndex = 0; weaponIndex < result.size(); ++weaponIndex )
		{
			if ( source[sourceIndex].GetWeaponID() == result[weaponIndex].GetWeaponID() )
			{
				found = true;
				break;
			}
		}
		if ( !found )
			result.push_back( source[sourceIndex] );
	}

	if ( IsPresetCareerDebuggingEnabled() )
		gEngfuncs.Con_DPrintf( "CareerWeaponListWithClass() - weapons are:\n" );
	for ( unsigned int i = 0; i < result.size(); ++i )
	{
		if ( IsPresetCareerDebuggingEnabled() )
		{
			gEngfuncs.Con_DPrintf( "\t%s + %d%s clips\n", WeaponIDToAlias( result[i].GetWeaponID() ),
				result[i].GetAmmoAmount(), result[i].GetFillAmmo() ? "+" : "" );
		}
	}

	return result;
}

std::vector<BuyPresetWeapon> CareerWeaponList( const std::vector<BuyPresetWeapon>& source,
	bool isPrimary, ClientWeaponIdType currentClientID )
{
	if ( !g_pCareerUI || !g_pCareerUI->IsPlayingMatch() )
		return source;

	ITaskVec* tasks = g_pCareerUI->GetCurrentTaskVec();
	if ( !tasks )
		return source;

	ITaskVec pendingTasks;
	for ( ITaskVecIt it = tasks->begin(); it != tasks->end(); ++it )
	{
		ICareerTask* task = *it;
		const char* alias = task->GetWeaponName();
		WeaponIdType weaponID = AliasToWeaponID( alias );
		WeaponClassType weaponClassID = AliasToWeaponClass( alias );
		if ( weaponID != WEAPON_NONE )
			weaponClassID = WEAPONCLASS_NONE;

		bool isWeaponID = false;
		bool isWeaponClassID;
		if ( isPrimary )
		{
			if ( weaponID == WEAPON_SHIELDGUN )
				isWeaponID = true;
			else
			{
				const char* weaponAlias = WeaponIDToAlias( weaponID );
				if ( weaponAlias )
				{
					WeaponClassType weaponClass = AliasToWeaponClass( weaponAlias );
					isWeaponID = weaponClass >= WEAPONCLASS_SUBMACHINEGUN
						&& weaponClass <= WEAPONCLASS_SNIPERRIFLE;
				}
			}
			isWeaponClassID = weaponClassID >= WEAPONCLASS_SUBMACHINEGUN
				&& weaponClassID <= WEAPONCLASS_SNIPERRIFLE;
		}
		else
		{
			if ( weaponID != WEAPON_SHIELDGUN )
			{
				const char* weaponAlias = WeaponIDToAlias( weaponID );
				if ( weaponAlias )
					isWeaponID = AliasToWeaponClass( weaponAlias ) == WEAPONCLASS_PISTOL;
			}
			isWeaponClassID = weaponClassID == WEAPONCLASS_PISTOL;
		}

		if ( task->IsComplete() || ( !isWeaponID && !isWeaponClassID ) )
			continue;

		WeaponIdType currentWeaponID = ClientWeaponIDToGameWeaponID( currentClientID );
		bool isCurrentWeapon = weaponID != WEAPON_NONE && weaponID == currentWeaponID;
		if ( !isCurrentWeapon )
		{
			const char* currentAlias = WeaponIDToAlias( currentWeaponID );
			if ( !currentAlias || AliasToWeaponClass( currentAlias ) != weaponClassID
				|| weaponClassID == WEAPONCLASS_NONE )
			{
				pendingTasks.push_back( task );
				continue;
			}
			isCurrentWeapon = weaponID != WEAPON_NONE;
		}

		if ( isWeaponClassID )
		{
			std::vector<BuyPresetWeapon> classWeapons = CareerWeaponListForClass( weaponClassID );
			return CareerWeaponListOrganized( classWeapons, source );
		}

		std::vector<BuyPresetWeapon> result = source;
		BuyPresetWeapon weapon( weaponID );
		weapon.SetAmmoAmount( isCurrentWeapon ? 1 : 0 );
		result.insert( result.begin(), weapon );
		return result;
	}

	std::vector<BuyPresetWeapon> weapons;
	for ( ITaskVecIt it = pendingTasks.begin(); it != pendingTasks.end(); ++it )
	{
		const char* alias = ( *it )->GetWeaponName();
		WeaponIdType weaponID = AliasToWeaponID( alias );
		WeaponClassType weaponClassID = AliasToWeaponClass( alias );
		if ( weaponID != WEAPON_NONE )
			weapons.push_back( BuyPresetWeapon( weaponID ) );
		else if ( weaponClassID != WEAPONCLASS_NONE )
		{
			std::vector<BuyPresetWeapon> classWeapons = CareerWeaponListForClass( weaponClassID );
			for ( unsigned int i = 0; i < classWeapons.size(); ++i )
				weapons.push_back( classWeapons[i] );
		}
	}

	return CareerWeaponListOrganized( weapons, source );
}
