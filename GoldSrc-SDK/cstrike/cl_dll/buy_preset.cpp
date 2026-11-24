#include "buy_presets.h"
#include "hud.h"
#include "ammohistory.h"
#include "net_api.h"
#include "buy_preset_debug.h"
#include "VGUI/GameUI_Interfaces.h"
#include "GameUI/ICareerUI.h"
#include "vgui/ILocalize.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "shared_util.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

WeaponSet::WeaponSet()
{
	m_isRebuy = false;
	Reset();
}

void WeaponSet::Reset()
{
	BuyPresetWeapon blank;
	m_primaryWeapon = blank;
	m_secondaryWeapon = blank;
	m_armor = 0;
	m_helmet = false;
	m_smokeGrenade = false;
	m_HEGrenade = false;
	m_flashbangs = 0;
	m_defuser = false;
	m_nightvision = false;
	m_extraCash = 0;
	m_isSecondaryOptional = false;
	m_isArmorOptional = false;
	m_isSmokeOptional = false;
	m_isHEGrenadeOptional = false;
	m_isFlashbangOptional = false;
	m_isDefuserOptional = false;
	m_isNightvisionOptional = false;
	m_isCashOptional = true;
}

void WeaponSet::GetFromScratch( int& cost, WeaponSet& current, bool full ) const
{
	cost = 0;
	current.Reset();
	int ammo[AMMO_MAX_TYPES] = { 0 };
	const WeaponInfoStruct* primaryInfo = GetWeaponInfo( m_primaryWeapon.GetWeaponID() );
	if ( primaryInfo )
	{
		int clips = CalcClipsNeeded( &m_primaryWeapon, primaryInfo, ammo );
		cost += primaryInfo->cost;
		cost += clips * primaryInfo->clipCost;
		ammo[primaryInfo->ammoType] += clips * primaryInfo->buyClipSize;
		current.m_primaryWeapon = m_primaryWeapon;
		current.m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
		current.m_primaryWeapon.SetFillAmmo( false );
		current.m_primaryWeapon.SetAmmoAmount( clips );
	}

	if ( full || !m_isSecondaryOptional )
	{
		const WeaponInfoStruct* secondaryInfo = GetWeaponInfo( m_secondaryWeapon.GetWeaponID() );
		if ( secondaryInfo )
		{
			int clips = CalcClipsNeeded( &m_secondaryWeapon, secondaryInfo, ammo );
			cost += secondaryInfo->cost;
			cost += clips * secondaryInfo->clipCost;
			ammo[secondaryInfo->ammoType] += clips * secondaryInfo->buyClipSize;
			current.m_secondaryWeapon = m_secondaryWeapon;
			current.m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
			current.m_secondaryWeapon.SetFillAmmo( false );
			current.m_secondaryWeapon.SetAmmoAmount( clips );
		}
	}

	if ( ( full || !m_isArmorOptional ) && m_armor )
	{
		cost += m_helmet ? ASSAULTSUIT_PRICE : KEVLAR_PRICE;
		current.m_armor = m_armor;
		current.m_helmet = m_helmet;
	}
	if ( ( full || !m_isSmokeOptional ) && m_smokeGrenade )
	{
		cost += SMOKEGRENADE_PRICE;
		current.m_smokeGrenade = m_smokeGrenade;
	}
	if ( ( full || !m_isHEGrenadeOptional ) && m_HEGrenade )
	{
		cost += HEGRENADE_PRICE;
		current.m_HEGrenade = m_HEGrenade;
	}
	if ( full || !m_isFlashbangOptional )
	{
		cost += FLASHBANG_PRICE * m_flashbangs;
		current.m_flashbangs = m_flashbangs;
	}
	if ( ( full || !m_isDefuserOptional ) && m_defuser )
	{
		cost += DEFUSEKIT_PRICE;
		current.m_defuser = m_defuser;
	}
	if ( ( full || !m_isNightvisionOptional ) && m_nightvision )
	{
		cost += NVG_PRICE;
		current.m_nightvision = m_nightvision;
	}
	if ( full || !m_isCashOptional )
	{
		cost += m_extraCash;
		current.m_extraCash = m_extraCash;
	}
}

int WeaponSet::FullCost() const
{
	WeaponSet fullSet;
	int cost;
	GetFromScratch( cost, fullSet, true );
	return cost;
}

void WeaponSet::GenerateBuyCommands( char* commands ) const
{
	int remaining = BUY_PRESET_COMMAND_LEN;
	*commands = 0;
	ClientWeaponIdType primaryClientID = GetClientWeaponID( true );
	ClientWeaponIdType secondaryClientID = GetClientWeaponID( false );
	WeaponIdType primaryID = ClientWeaponIDToGameWeaponID( primaryClientID );
	WeaponIdType secondaryID = ClientWeaponIDToGameWeaponID( secondaryClientID );
	WeaponIdType weaponID = m_primaryWeapon.GetWeaponID();
	if ( GetWeaponInfo( weaponID == WEAPON_NONE ? primaryID : weaponID ) )
	{
		if ( weaponID != WEAPON_NONE && weaponID != primaryID )
			commands = BufPrintf( commands, remaining, "%s\n", WeaponIDToAlias( weaponID ) );
		for ( int clip = 0; clip < m_primaryWeapon.GetAmmoAmount(); ++clip )
			commands = BufPrintf( commands, remaining, "buyammo1\n" );
	}

	weaponID = m_secondaryWeapon.GetWeaponID();
	if ( GetWeaponInfo( weaponID == WEAPON_NONE ? secondaryID : weaponID ) )
	{
		if ( weaponID != WEAPON_NONE && weaponID != secondaryID )
			commands = BufPrintf( commands, remaining, "%s\n", WeaponIDToAlias( weaponID ) );
		for ( int clip = 0; clip < m_secondaryWeapon.GetAmmoAmount(); ++clip )
			commands = BufPrintf( commands, remaining, "buyammo2\n" );
	}

	if ( m_armor )
		commands = BufPrintf( commands, remaining, m_helmet ? "vesthelm\n" : "vest\n" );
	if ( m_smokeGrenade )
		commands = BufPrintf( commands, remaining, "sgren\n" );
	if ( m_HEGrenade )
		commands = BufPrintf( commands, remaining, "hegren\n" );
	for ( int grenade = 0; grenade < m_flashbangs; ++grenade )
		commands = BufPrintf( commands, remaining, "flash\n" );
	if ( m_defuser )
		commands = BufPrintf( commands, remaining, "defuser\n" );
	if ( m_nightvision )
		BufPrintf( commands, remaining, "nvgs\n" );
}

const char* ImageFnameFromWeaponID( WeaponIdType weaponID, bool isPrimary )
{
	switch ( weaponID )
	{
	case WEAPON_NONE:
		if ( g_pCareerUI && g_pCareerUI->IsPlayingMatch() )
			return isPrimary ? "gfx/vgui/buy_presets/defaultrifle_career" : "gfx/vgui/buy_presets/defaultpistol_career";
		return isPrimary ? "gfx/vgui/buy_presets/defaultrifle" : "gfx/vgui/buy_presets/defaultpistol";
	case WEAPON_P228: return "gfx/vgui/buy_presets/p228";
	case WEAPON_SCOUT: return "gfx/vgui/buy_presets/scout";
	case WEAPON_XM1014: return "gfx/vgui/buy_presets/xm1014";
	case WEAPON_MAC10: return "gfx/vgui/buy_presets/mac10";
	case WEAPON_AUG: return "gfx/vgui/buy_presets/aug";
	case WEAPON_ELITE: return "gfx/vgui/buy_presets/elites";
	case WEAPON_FIVESEVEN: return "gfx/vgui/buy_presets/fiveseven";
	case WEAPON_UMP45: return "gfx/vgui/buy_presets/ump45";
	case WEAPON_SG550: return "gfx/vgui/buy_presets/sg550";
	case WEAPON_GALIL: return "gfx/vgui/buy_presets/galil";
	case WEAPON_FAMAS: return "gfx/vgui/buy_presets/famas";
	case WEAPON_USP: return "gfx/vgui/buy_presets/usp45";
	case WEAPON_GLOCK18: return "gfx/vgui/buy_presets/glock18";
	case WEAPON_AWP: return "gfx/vgui/buy_presets/awp";
	case WEAPON_MP5N: return "gfx/vgui/buy_presets/mp5";
	case WEAPON_M249: return "gfx/vgui/buy_presets/m249";
	case WEAPON_M3: return "gfx/vgui/buy_presets/m3";
	case WEAPON_M4A1: return "gfx/vgui/buy_presets/m4a1";
	case WEAPON_TMP: return "gfx/vgui/buy_presets/tmp";
	case WEAPON_G3SG1: return "gfx/vgui/buy_presets/g3sg1";
	case WEAPON_DEAGLE: return "gfx/vgui/buy_presets/deserteagle";
	case WEAPON_SG552: return "gfx/vgui/buy_presets/sg552";
	case WEAPON_AK47: return "gfx/vgui/buy_presets/ak47";
	case WEAPON_P90: return "gfx/vgui/buy_presets/p90";
	case WEAPON_SHIELDGUN: return "gfx/vgui/buy_presets/shield";
	default: return "";
	}
}

BuyPreset::BuyPreset()
{
	SetName( L"" );
	m_isAutobuy = false;
}

BuyPreset::~BuyPreset()
{
}

void BuyPreset::SetName( const wchar_t* name )
{
	wcsncpy( m_name, name, MaxBuyPresetName );
	if ( !m_name[0] )
	{
		const wchar_t* blankName = vgui2::localize()->Find( "#Cstrike_BuyPresetBlank" );
		if ( blankName )
			wcsncpy( m_name, blankName, MaxBuyPresetName );
	}

	m_name[MaxBuyPresetName - 1] = 0;
}

static const char* ConstructWeaponString( const BuyPresetWeapon& weapon )
{
	static char weaponString[1024];
	weaponString[0] = 0;
	int remaining = sizeof( weaponString );
	BufPrintf( weaponString, remaining, "%s/%d%c", WeaponIDToAlias( weapon.GetWeaponID() ),
		weapon.GetAmmoAmount(), weapon.GetFillAmmo() ? '+' : '=' );
	return weaponString;
}

void BuyPreset::Save( KeyValues* data )
{
	KeyValues* preset = data->CreateNewKey();
	preset->SetWString( "PresetName", m_name );
	preset->SetInt( "Version", 3 );
	for ( WeaponSetList::const_iterator set = m_weaponList.begin(); set != m_weaponList.end(); ++set )
	{
		KeyValues* values = preset->CreateNewKey();
		values->SetString( "Primary", ConstructWeaponString( set->m_primaryWeapon ) );
		values->SetString( "Secondary", ConstructWeaponString( set->m_secondaryWeapon ) );
		values->SetString( "Equipment", SharedVarArgs( "vest%s/%d flash/%d sgren/%d hegren/%d defuser/%d nvgs/%d",
			set->m_helmet ? "helm" : "", set->m_armor, set->m_flashbangs, set->m_smokeGrenade,
			set->m_HEGrenade, set->m_defuser, set->m_nightvision ) );
	}
}

int BuyPreset::FullCost() const
{
	if ( m_weaponList.empty() )
		return 0;
	return m_weaponList.front().FullCost();
}

const WeaponSet* BuyPreset::GetSet( int index ) const
{
	if ( index >= 0 && (unsigned int)index < m_weaponList.size() )
		return &m_weaponList[index];
	return NULL;
}

void BuyPreset::DeleteSet( int index )
{
	if ( index < 0 || (unsigned int)index >= m_weaponList.size() )
		return;
	for ( unsigned int i = 1; i + 1 < m_weaponList.size(); ++i )
		m_weaponList[i] = m_weaponList[i + 1];
	m_weaponList.pop_back();
}

void BuyPreset::SwapSet( int first, int second )
{
	if ( first < 0 || second < 0 || (unsigned int)first >= m_weaponList.size()
		|| (unsigned int)second >= m_weaponList.size() )
	{
		return;
	}

	WeaponSet temporary = m_weaponList[first];
	m_weaponList[first] = m_weaponList[second];
	m_weaponList[second] = temporary;
}

void BuyPreset::ReplaceSet( int index, const WeaponSet& set )
{
	if ( index < 0 || (unsigned int)index > m_weaponList.size() )
		return;
	if ( (unsigned int)index == m_weaponList.size() )
		m_weaponList.push_back( set );
	else
		m_weaponList[index] = set;
}

static void ParseWeaponString( const char* str, std::vector<BuyPresetWeapon>& weapons, bool isPrimary )
{
	weapons.clear();
	if ( !str )
		return;

	const char* remainder = SharedParse( str );
	const char* token;
	const int BufLen = 32;
	char tmpBuf[BufLen];
	char weaponBuf[BufLen];
	char clipModifier;
	int numClips;
	while ( remainder )
	{
		token = SharedGetToken();
		if ( !token || strlen( token ) >= BufLen )
			return;

		strncpy( tmpBuf, token, BufLen );
		tmpBuf[BufLen - 1] = 0;
		char* tmp = tmpBuf;
		while ( *tmp )
		{
			if ( *tmp == '/' )
				*tmp = ' ';
			++tmp;
		}

		if ( sscanf( tmpBuf, "%s %d%c", weaponBuf, &numClips, &clipModifier ) != 3 )
			return;

		WeaponIdType weaponID = AliasToWeaponID( weaponBuf );
		const WeaponInfoStruct* info = GetWeaponInfo( weaponID );
		if ( info )
		{
			if ( numClips < 0 )
				numClips = 0;
			int maxClips = (int)ceil( (float)info->maxRounds / (float)info->buyClipSize );
			if ( numClips > maxClips )
				numClips = maxClips;

			if ( IsPrimaryWeapon( weaponID ) != isPrimary )
				return;
		}
		else
		{
			if ( numClips < 0 )
				numClips = 0;
			if ( numClips > NUM_CLIPS_FOR_CURRENT )
				numClips = NUM_CLIPS_FOR_CURRENT;
		}

		BuyPresetWeapon weapon( weaponID );
		weapon.SetAmmoType( AMMO_CLIPS );
		weapon.SetAmmoAmount( numClips );
		weapon.SetFillAmmo( clipModifier == '+' );
		weapons.push_back( weapon );

		remainder = SharedParse( remainder );
	}
}

void BuyPreset::Parse( KeyValues* data )
{
	m_name[0] = 0;
	m_weaponList.clear();

	if ( !data )
		return;

	wcsncpy( m_name, data->GetWString( "PresetName", L"" ), MaxBuyPresetName );
	m_name[MaxBuyPresetName - 1] = 0;
	if ( IsPresetDebuggingEnabled() )
		gEngfuncs.Con_DPrintf( "Parsing Buy Preset %ls\n", m_name );

	int version = data->GetInt( "Version", 0 );
	if ( version < 1 || version > 3 )
	{
		if ( IsPresetDebuggingEnabled() )
			gEngfuncs.Con_DPrintf( "Invalid preset version %d\n", version );
		return;
	}

	m_isAutobuy = false;

	int setIndex = 1;
	KeyValues* setData = data->FindKey( NumAsString( setIndex ), false );
	while ( setData )
	{
		const char* primaryString = setData->GetString( "Primary", NULL );
		const char* secondaryString = setData->GetString( "Secondary", NULL );
		const char* equipmentString = setData->GetString( "Equipment", NULL );

		std::vector<BuyPresetWeapon> weapons;
		ParseWeaponString( primaryString, weapons, true );
		if ( weapons.size() != 1 )
			return;

		WeaponSet ws;
		ws.m_primaryWeapon = weapons[0];

		ParseWeaponString( secondaryString, weapons, false );
		if ( weapons.size() )
			ws.m_secondaryWeapon = weapons[0];
		ws.m_isSecondaryOptional = false;

		if ( equipmentString )
		{
			const char* remainder = SharedParse( equipmentString );
			const char* token;
			const int BufLen = 32;
			char tmpBuf[BufLen];
			char itemBuf[BufLen];
			int arg;
			while ( remainder )
			{
				token = SharedGetToken();
				if ( !token || strlen( token ) >= BufLen )
					break;

				strncpy( tmpBuf, token, BufLen );
				tmpBuf[BufLen - 1] = 0;
				char* tmp = tmpBuf;
				while ( *tmp )
				{
					if ( *tmp == '/' )
						*tmp = ' ';
					++tmp;
				}

				if ( sscanf( tmpBuf, "%s %d", itemBuf, &arg ) != 2 )
					break;

				if ( !strcmp( itemBuf, "vest" ) )
				{
					ws.m_armor = ( arg > 0 ) ? 100 : 0;
					ws.m_helmet = false;
					ws.m_isArmorOptional = false;
				}
				else if ( !strcmp( itemBuf, "vesthelm" ) )
				{
					ws.m_armor = ( arg > 0 ) ? 100 : 0;
					ws.m_helmet = true;
					ws.m_isArmorOptional = false;
				}
				else if ( !strcmp( itemBuf, "defuser" ) )
				{
					ws.m_defuser = ( arg > 0 );
					ws.m_isDefuserOptional = false;
				}
				else if ( !strcmp( itemBuf, "nvgs" ) )
				{
					ws.m_nightvision = ( arg > 0 );
					ws.m_isNightvisionOptional = false;
				}
				else if ( !strcmp( itemBuf, "sgren" ) )
				{
					ws.m_smokeGrenade = ( arg > 0 );
					ws.m_isSmokeOptional = false;
				}
				else if ( !strcmp( itemBuf, "hegren" ) )
				{
					ws.m_HEGrenade = ( arg > 0 );
					ws.m_isHEGrenadeOptional = false;
				}
				else if ( !strcmp( itemBuf, "flash" ) )
				{
					int flashbangs = arg;
					if ( flashbangs < 0 )
						flashbangs = 0;
					if ( flashbangs > 2 )
						flashbangs = 2;
					ws.m_flashbangs = flashbangs;
					ws.m_isFlashbangOptional = false;
				}

				remainder = SharedParse( remainder );
			}
		}

		m_weaponList.push_back( ws );

		setData = data->FindKey( NumAsString( ++setIndex ), false );
	}
}

void WeaponSet::GetCurrent( int& cost, WeaponSet& current, bool withCareer ) const
{
	cost = 0;
	current.Reset();
	net_status_t status;
	gEngfuncs.pNetAPI->Status( &status );
	if ( !status.connected || !gEngfuncs.GetLocalPlayer()->player || (unsigned int)(g_iTeamNumber - 1) > 1 )
	{
		cost = -1;
		return;
	}
	if ( g_PlayerExtraInfo[gEngfuncs.GetLocalPlayer()->index].vip )
		return;

	if ( !m_isArmorOptional )
	{
		if ( m_armor > gHUD.m_Battery.m_iBat )
		{
			cost += KEVLAR_PRICE;
			current.m_armor = 100;
		}
		if ( m_helmet && m_armor > 0 && !gHUD.m_Battery.m_iArmorType )
		{
			cost += HELMET_PRICE;
			current.m_helmet = true;
		}
	}
	bool hasSmoke = gWR.HasAmmo( gWR.GetWeapon( ID_SMOKEGRENADE ) ) != 0;
	if ( !m_isSmokeOptional && m_smokeGrenade && !gWR.HasAmmo( gWR.GetWeapon( ID_SMOKEGRENADE ) ) )
	{
		hasSmoke = true;
		cost += SMOKEGRENADE_PRICE;
		current.m_smokeGrenade = true;
	}
	bool hasHE = gWR.HasAmmo( gWR.GetWeapon( ID_HEGRENADE ) ) != 0;
	if ( !m_isHEGrenadeOptional && m_HEGrenade && !hasHE )
	{
		hasHE = true;
		cost += HEGRENADE_PRICE;
		current.m_HEGrenade = true;
	}
	int flashbangs = gWR.CountAmmo( gWR.GetWeapon( ID_FLASHBANG )->iAmmoType );
	if ( !m_isFlashbangOptional && flashbangs < m_flashbangs && m_flashbangs )
	{
		cost += FLASHBANG_PRICE * (m_flashbangs - flashbangs);
		current.m_flashbangs = m_flashbangs - flashbangs;
		flashbangs = m_flashbangs;
	}
	if ( !m_isDefuserOptional && m_defuser && g_iTeamNumber == 2 && !TheBuyPresets->HasDefuser() && IsDEMapType() )
	{
		cost += DEFUSEKIT_PRICE;
		current.m_defuser = true;
	}
	if ( !m_isNightvisionOptional && m_nightvision && !TheBuyPresets->HasNightvision() )
	{
		cost += NVG_PRICE;
		current.m_nightvision = true;
	}
	if ( !m_isCashOptional && m_extraCash )
	{
		cost += m_extraCash;
		current.m_extraCash = m_extraCash;
	}
	if ( withCareer && !m_isRebuy )
	{
		if ( g_pCareerUI->IsPlayingMatch() )
		{
			ITaskVec* tasks = g_pCareerUI->GetCurrentTaskVec();
			if ( tasks )
			{
				for ( ITaskVec::const_iterator it = tasks->begin(); it != tasks->end(); ++it )
				{
					ICareerTask* task = *it;
					if ( task && !task->IsComplete() && !stricmp( task->GetTaskName(), "killblind" ) )
					{
						if ( flashbangs < 2 )
						{
							cost += FLASHBANG_PRICE * (2 - flashbangs);
							current.m_flashbangs += 2 - flashbangs;
							flashbangs = 2;
						}
						break;
					}
				}
			}
		}
		if ( !m_isRebuy && g_pCareerUI->IsPlayingMatch() )
		{
			ITaskVec* tasks = g_pCareerUI->GetCurrentTaskVec();
			if ( tasks )
			{
				for ( ITaskVec::const_iterator it = tasks->begin(); it != tasks->end(); ++it )
				{
					ICareerTask* task = *it;
					if ( task && !task->IsComplete() && (!stricmp( task->GetWeaponName(), "grenade" ) || !stricmp( task->GetWeaponName(), "hegren" )) )
					{
						if ( !hasHE )
						{
							hasHE = true;
							cost += HEGRENADE_PRICE;
							current.m_HEGrenade = true;
						}
						break;
					}
				}
			}
		}
	}

	ClientWeaponIdType primaryClientID = GetClientWeaponID( true );
	ClientWeaponIdType secondaryClientID = GetClientWeaponID( false );
	WeaponIdType primaryID = ClientWeaponIDToGameWeaponID( primaryClientID );
	WeaponIdType secondaryID = ClientWeaponIDToGameWeaponID( secondaryClientID );
	std::vector<BuyPresetWeapon> primaryList;
	if ( m_isRebuy && primaryID != WEAPON_NONE && primaryID != m_primaryWeapon.GetWeaponID() )
	{
		BuyPresetWeapon existing( primaryID );
		existing.SetAmmoType( AMMO_ROUNDS );
		existing.SetFillAmmo( false );
		existing.SetAmmoAmount( m_primaryWeapon.GetAmmoAmount() );
		primaryList.push_back( existing );
	}
	primaryList.push_back( m_primaryWeapon );
	std::vector<BuyPresetWeapon> primaries = withCareer ? CareerWeaponList( primaryList, true, primaryClientID ) : primaryList;
	std::vector<BuyPresetWeapon> secondaryList;
	secondaryList.push_back( m_secondaryWeapon );
	std::vector<BuyPresetWeapon> secondaries = withCareer ? CareerWeaponList( secondaryList, false, secondaryClientID ) : secondaryList;
	int nonWeaponCost = cost;
	int ammo[AMMO_MAX_TYPES] = { 0 };
	FillClientAmmo( ammo );
	int cash = gHUD.m_accountBalance.m_iAccount;
	const BuyPresetWeapon* primary = NULL;
	const BuyPresetWeapon* secondary = NULL;
	int primaryClips = 0;
	int secondaryClips = 0;
	int weaponCost = nonWeaponCost;

	if ( !m_isRebuy )
	{
		for ( std::vector<BuyPresetWeapon>::const_iterator it = primaries.begin(); it != primaries.end(); ++it )
		{
			primary = &*it;
			FillClientAmmo( ammo );
			WeaponIdType id = primary->GetWeaponID() == WEAPON_NONE ? primaryID : primary->GetWeaponID();
			const WeaponInfoStruct* info = GetWeaponInfo( id );
			primaryClips = 0;
			weaponCost = nonWeaponCost;
			if ( info )
			{
				primaryClips = CalcClipsNeeded( primary, info, ammo );
				int price = id == primaryID ? 0 : info->cost;
				weaponCost += price + primaryClips * info->clipCost;
				ammo[info->ammoType] += primaryClips * info->buyClipSize;
				if ( IsPresetCurrentCostDebuggingEnabled() )
					gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", price, WeaponIDToAlias( id ) );
				if ( IsPresetCurrentCostDebuggingEnabled() )
					gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", info->clipCost * primaryClips );
				bool canBuy = cash >= weaponCost && CanBuyWeapon( primaryClientID, secondaryClientID, id );
				if ( cash < weaponCost || (id != primaryID && !canBuy) )
				{
					weaponCost = nonWeaponCost;
					primaryClips = 0;
					primary = NULL;
					continue;
				}
			}
			else
				primary = NULL;
			if ( secondaries.empty() )
			{
				if ( IsPresetCurrentCostDebuggingEnabled() )
					gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (no secondary)\n" );
				break;
			}
			if ( m_isSecondaryOptional )
			{
				if ( IsPresetCurrentCostDebuggingEnabled() )
					gEngfuncs.Con_DPrintf( "\t\t\tDone buying required weapons (secondary is optional)\n" );
				break;
			}
			for ( std::vector<BuyPresetWeapon>::const_iterator jt = secondaries.begin(); jt != secondaries.end(); ++jt )
			{
				FillClientAmmo( ammo );
				if ( info )
				{
					weaponCost = nonWeaponCost + primaryClips * info->clipCost + (id == primaryID ? 0 : info->cost);
					ammo[info->ammoType] += primaryClips * info->buyClipSize;
				}
				else
					weaponCost = nonWeaponCost;
				WeaponIdType pistolID = jt->GetWeaponID() == WEAPON_NONE ? secondaryID : jt->GetWeaponID();
				const WeaponInfoStruct* pistolInfo = GetWeaponInfo( pistolID );
				if ( !pistolInfo )
					continue;
				secondaryClips = CalcClipsNeeded( &*jt, pistolInfo, ammo );
				int price = pistolID == secondaryID ? 0 : pistolInfo->cost;
				int combinedCost = weaponCost + price + secondaryClips * pistolInfo->clipCost;
				ammo[pistolInfo->ammoType] += secondaryClips * pistolInfo->buyClipSize;
				if ( IsPresetCurrentCostDebuggingEnabled() )
					gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", price, WeaponIDToAlias( pistolID ) );
				if ( IsPresetCurrentCostDebuggingEnabled() )
					gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", pistolInfo->clipCost * secondaryClips );
				bool canBuy = cash >= combinedCost && CanBuyWeapon( primaryClientID, secondaryClientID, pistolID );
				if ( cash >= combinedCost && (pistolID == secondaryID || canBuy) )
				{
					secondary = &*jt;
					weaponCost = combinedCost;
					if ( IsPresetCurrentCostDebuggingEnabled() )
						gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (primary && secondary)\n" );
					break;
				}
			}
			if ( secondary )
				break;
		}
	}
	cost = weaponCost;
	if ( primary )
	{
		current.m_primaryWeapon = *primary;
		current.m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
		current.m_primaryWeapon.SetAmmoAmount( primaryClips );
	}
	else if ( !m_isRebuy )
	{
		for ( std::vector<BuyPresetWeapon>::const_iterator it = primaries.begin(); it != primaries.end(); ++it )
		{
			if ( it->GetWeaponID() != WEAPON_NONE )
			{
				cost = -1;
				current.Reset();
				return;
			}
		}
	}
	if ( secondary )
	{
		current.m_secondaryWeapon = *secondary;
		current.m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
		current.m_secondaryWeapon.SetAmmoAmount( secondaryClips );
	}
	else if ( !m_isSecondaryOptional && !m_isRebuy )
	{
		for ( std::vector<BuyPresetWeapon>::const_iterator it = secondaries.begin(); it != secondaries.end(); ++it )
		{
			if ( it->GetWeaponID() != WEAPON_NONE )
			{
				cost = -1;
				current.Reset();
				return;
			}
		}
	}

	const char* cursor = SharedParse( TheBuyPresets->GetRebuyString() );
	for ( ; cursor; cursor = SharedParse( cursor ) )
	{
		const char* token = SharedGetToken();
		if ( !token || !*token )
			return;
		if ( !stricmp( token, "PrimaryWeapon" ) )
		{
			if ( m_isRebuy && !primary )
			{
				for ( std::vector<BuyPresetWeapon>::const_iterator it = primaries.begin(); it != primaries.end(); ++it )
				{
					FillClientAmmo( ammo );
					if ( it->GetWeaponID() == WEAPON_NONE )
						continue;
					const WeaponInfoStruct* info = GetWeaponInfo( it->GetWeaponID() );
					BuyPresetWeapon weapon = *it;
					weapon.SetAmmoType( AMMO_CLIPS );
					weapon.SetAmmoAmount( 1 );
					weapon.SetFillAmmo( false );
					int clips = CalcClipsNeeded( &weapon, info, ammo );
					int newCost = cost + info->cost + clips * info->clipCost;
					ammo[info->ammoType] += clips * info->buyClipSize;
					if ( IsPresetCurrentCostDebuggingEnabled() )
						gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", info->cost, WeaponIDToAlias( it->GetWeaponID() ) );
					if ( IsPresetCurrentCostDebuggingEnabled() )
						gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", info->clipCost * clips );
					bool canBuy = cash >= newCost && (CanBuyWeapon( primaryClientID, secondaryClientID, it->GetWeaponID() ) || primaryID == it->GetWeaponID());
					if ( !canBuy )
						continue;
					if ( IsPresetCurrentCostDebuggingEnabled() )
						gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (optional primary)\n" );
					cost = newCost;
					current.m_primaryWeapon = *it;
					current.m_primaryWeapon.SetAmmoAmount( clips );
					current.m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
					primary = &*it;
					break;
				}
				if ( !primary && primaryID != WEAPON_NONE )
				{
					current.m_primaryWeapon = BuyPresetWeapon( primaryID );
					current.m_primaryWeapon.SetAmmoType( AMMO_CLIPS );
					current.m_primaryWeapon.SetAmmoAmount( 1 );
					current.m_primaryWeapon.SetFillAmmo( false );
					primary = &current.m_primaryWeapon;
				}
			}
		}
		else if ( !stricmp( token, "PrimaryAmmo" ) )
		{
			if ( !primary )
				continue;
			WeaponIdType id = primary->GetWeaponID() == WEAPON_NONE ? primaryID : primary->GetWeaponID();
			const WeaponInfoStruct* info = NULL;
			int rounds = 0;
			if ( m_isRebuy && primary->GetAmmoType() == AMMO_ROUNDS )
			{
				info = GetWeaponInfo( id );
				if ( info ) rounds = primary->GetAmmoAmount() - ammo[info->ammoType];
			}
			else if ( primary->GetFillAmmo() )
			{
				info = GetWeaponInfo( id );
				if ( info ) rounds = info->maxRounds - ammo[info->ammoType];
			}
			if ( info && info->buyClipSize > 0 && rounds > 0 )
			{
				int clips = (int)ceilf( (float)rounds / info->buyClipSize );
				while ( clips > 0 && cash >= cost + info->clipCost )
				{
					current.m_primaryWeapon.SetAmmoAmount( current.m_primaryWeapon.GetAmmoAmount() + 1 );
					ammo[info->ammoType] += info->buyClipSize < info->maxRounds ? info->buyClipSize : info->maxRounds;
					cost += info->clipCost;
					--clips;
				}
			}
		}
		else if ( !stricmp( token, "SecondaryWeapon" ) )
		{
			if ( (m_isRebuy || m_isSecondaryOptional) && !secondary && !secondaries.empty() )
			{
				const WeaponInfoStruct* primaryInfo = primary ? GetWeaponInfo( primary->GetWeaponID() ) : NULL;
				for ( std::vector<BuyPresetWeapon>::const_iterator it = secondaries.begin(); it != secondaries.end(); ++it )
				{
					FillClientAmmo( ammo );
					if ( primaryInfo )
						ammo[primaryInfo->ammoType] += current.m_primaryWeapon.GetAmmoAmount() * primaryInfo->buyClipSize;
					WeaponIdType id = it->GetWeaponID() == WEAPON_NONE ? secondaryID : it->GetWeaponID();
					const WeaponInfoStruct* info = GetWeaponInfo( id );
					if ( !info ) continue;
					int clips = CalcClipsNeeded( &*it, info, ammo );
					int price = id == secondaryID ? 0 : info->cost;
					int newCost = cost + price + clips * info->clipCost;
					ammo[info->ammoType] += clips * info->buyClipSize;
					if ( IsPresetCurrentCostDebuggingEnabled() )
						gEngfuncs.Con_DPrintf( "\t%5.5d (%s)\n", price, WeaponIDToAlias( id ) );
					if ( IsPresetCurrentCostDebuggingEnabled() )
						gEngfuncs.Con_DPrintf( "\t%5.5d (ammo)\n", info->clipCost * clips );
					bool canBuy = cash >= newCost && CanBuyWeapon( primaryClientID, secondaryClientID, id );
					if ( cash >= newCost && (id == secondaryID || canBuy) )
					{
						if ( IsPresetCurrentCostDebuggingEnabled() )
							gEngfuncs.Con_DPrintf( "\t\t\tDone buying weapons (optional secondary)\n" );
						cost = newCost;
						current.m_secondaryWeapon = *it;
						current.m_secondaryWeapon.SetAmmoAmount( clips );
						current.m_secondaryWeapon.SetAmmoType( AMMO_CLIPS );
						secondary = &*it;
						break;
					}
				}
			}
		}
		else if ( !stricmp( token, "SecondaryAmmo" ) )
		{
			if ( secondary && secondary->GetFillAmmo() )
			{
				WeaponIdType id = secondary->GetWeaponID() == WEAPON_NONE ? secondaryID : secondary->GetWeaponID();
				const WeaponInfoStruct* info = GetWeaponInfo( id );
				if ( info && info->buyClipSize > 0 )
				{
					int clips = (int)ceilf( (float)(info->maxRounds - ammo[info->ammoType]) / info->buyClipSize );
					while ( clips > 0 && cash >= cost + info->clipCost )
					{
						current.m_secondaryWeapon.SetAmmoAmount( current.m_secondaryWeapon.GetAmmoAmount() + 1 );
						ammo[info->ammoType] += info->buyClipSize < info->maxRounds ? info->buyClipSize : info->maxRounds;
						cost += info->clipCost;
						--clips;
					}
				}
			}
		}
		else if ( !stricmp( token, "Defuser" ) )
		{
			if ( !current.m_defuser && m_defuser && g_iTeamNumber == 2 && !TheBuyPresets->HasDefuser() && IsDEMapType() && cash > cost + DEFUSEKIT_PRICE - 1 )
			{
				cost += DEFUSEKIT_PRICE;
				current.m_defuser = true;
			}
		}
		else if ( !stricmp( token, "Armor" ) )
		{
			if ( !current.m_armor && m_armor > gHUD.m_Battery.m_iBat && cash > cost + KEVLAR_PRICE - 1 )
			{
				cost += KEVLAR_PRICE;
				current.m_armor = 100;
			}
			if ( !current.m_helmet && m_helmet && m_armor > 0 && !gHUD.m_Battery.m_iArmorType && cash > cost + KEVLAR_PRICE - 1 )
			{
				cost += HELMET_PRICE;
				current.m_helmet = true;
			}
		}
		else if ( !stricmp( token, "HEGrenade" ) )
		{
			if ( m_HEGrenade && !hasHE && cash > cost + HEGRENADE_PRICE - 1 )
			{
				cost += HEGRENADE_PRICE;
				hasHE = true;
				current.m_HEGrenade = true;
			}
		}
		else if ( !stricmp( token, "Flashbang" ) )
		{
			for ( int needed = m_flashbangs - flashbangs; needed > 0; --needed )
			{
				if ( cash > cost + FLASHBANG_PRICE - 1 )
				{
					cost += FLASHBANG_PRICE;
					++current.m_flashbangs;
					++flashbangs;
				}
			}
		}
		else if ( !stricmp( token, "SmokeGrenade" ) )
		{
			if ( m_smokeGrenade && !hasSmoke && cash > cost + SMOKEGRENADE_PRICE - 1 )
			{
				cost += SMOKEGRENADE_PRICE;
				hasSmoke = true;
				current.m_smokeGrenade = true;
			}
		}
		else if ( !stricmp( token, "NightVision" ) )
		{
			if ( !current.m_nightvision && m_nightvision && !TheBuyPresets->HasNightvision() && cash > cost + NVG_PRICE - 1 )
			{
				cost += NVG_PRICE;
				current.m_nightvision = true;
			}
		}
		else if ( !stricmp( token, "ExtraCash" ) && !current.m_extraCash && m_extraCash )
		{
			int reserved = cash - cost;
			if ( reserved > m_extraCash ) reserved = m_extraCash;
			cost += reserved;
			current.m_extraCash = reserved;
		}
	}
	if ( cash >= cost )
		cost -= current.m_extraCash;
	else
	{
		cost = -1;
		current.Reset();
	}
}
