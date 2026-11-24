/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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
//
// Ammo.cpp
//
// implementation of CHudAmmo class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "pm_shared.h"

#include <string.h>
#include <stdio.h>

#include "ammohistory.h"
#include "../public/game_controls/iviewport.h"
#include "VGUI/icstrikeviewport.h"
#include "shared_util.h"
#include "weapontype.h"

WEAPON *gpActiveSel;	// NULL means off, 1 means just the menu bar, otherwise
						// this points to the active weapon menu item
WEAPON *gpLastSel;		// Last weapon menu selection

client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

WeaponsResource gWR;

int g_weaponselect = 0;

int g_iPlayerFlags;
int g_iWeaponFlags;
float g_flPlayerSpeed;
int g_iShotsFired;
cvar_t *cl_dynamiccrosshair;
extern bool g_bHoldingShield;

void WeaponsResource :: LoadAllWeaponSprites( void )
{
	for (int i = 0; i < MAX_WEAPONS; i++)
	{
		if ( rgWeapons[i].iId )
			LoadWeaponSprites( &rgWeapons[i] );
	}
}

int WeaponsResource :: CountAmmo( int iId )
{
	if ( iId < 0 )
		return 0;

	return riAmmo[iId];
}

int WeaponsResource :: HasAmmo( WEAPON *p )
{
	if ( !p )
		return FALSE;

	// weapons with no max ammo can always be selected
	if ( p->iMax1 == -1 )
		return TRUE;

	return (p->iAmmoType == -1) || p->iClip > 0 || CountAmmo(p->iAmmoType)
		|| CountAmmo(p->iAmmo2Type) || ( p->iFlags & WEAPON_FLAGS_SELECTONEMPTY );
}


void WeaponsResource :: LoadWeaponSprites( WEAPON *pWeapon )
{
	int i, iRes;

	if (ScreenWidth >= 640)
		iRes = 640;
	else
		iRes = 320;

	char sz[128];

	if ( !pWeapon )
		return;

	memset( &pWeapon->rcActive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcInactive, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo, 0, sizeof(wrect_t) );
	memset( &pWeapon->rcAmmo2, 0, sizeof(wrect_t) );
	pWeapon->hInactive = 0;
	pWeapon->hActive = 0;
	pWeapon->hAmmo = 0;
	pWeapon->hAmmo2 = 0;

	sprintf(sz, "sprites/%s.txt", pWeapon->szName);
	client_sprite_t *pList = SPR_GetList(sz, &i);

	if (!pList)
		return;

	client_sprite_t *p;

	p = GetSpriteList( pList, "crosshair", iRes, i );
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hCrosshair = SPR_Load(sz);
		pWeapon->rcCrosshair = p->rc;
	}
	else
		pWeapon->hCrosshair = NULL;

	p = GetSpriteList(pList, "autoaim", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hAutoaim = SPR_Load(sz);
		pWeapon->rcAutoaim = p->rc;
	}
	else
		pWeapon->hAutoaim = 0;

	p = GetSpriteList( pList, "zoom", iRes, i );
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hZoomedCrosshair = SPR_Load(sz);
		pWeapon->rcZoomedCrosshair = p->rc;
	}
	else
	{
		pWeapon->hZoomedCrosshair = pWeapon->hCrosshair; //default to non-zoomed crosshair
		pWeapon->rcZoomedCrosshair = pWeapon->rcCrosshair;
	}

	p = GetSpriteList(pList, "zoom_autoaim", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hZoomedAutoaim = SPR_Load(sz);
		pWeapon->rcZoomedAutoaim = p->rc;
	}
	else
	{
		pWeapon->hZoomedAutoaim = pWeapon->hZoomedCrosshair;  //default to zoomed crosshair
		pWeapon->rcZoomedAutoaim = pWeapon->rcZoomedCrosshair;
	}

	p = GetSpriteList(pList, "weapon", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hInactive = SPR_Load(sz);
		pWeapon->rcInactive = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hInactive = 0;

	p = GetSpriteList(pList, "weapon_s", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hActive = SPR_Load(sz);
		pWeapon->rcActive = p->rc;
	}
	else
		pWeapon->hActive = 0;

	p = GetSpriteList(pList, "ammo", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hAmmo = SPR_Load(sz);
		pWeapon->rcAmmo = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hAmmo = 0;

	p = GetSpriteList(pList, "ammo2", iRes, i);
	if (p)
	{
		sprintf(sz, "sprites/%s.spr", p->szSprite);
		pWeapon->hAmmo2 = SPR_Load(sz);
		pWeapon->rcAmmo2 = p->rc;

		gHR.iHistoryGap = max( gHR.iHistoryGap, pWeapon->rcActive.bottom - pWeapon->rcActive.top );
	}
	else
		pWeapon->hAmmo2 = 0;

	gEngfuncs.COM_FreeFile( pList );
}

// Returns the first weapon for a given slot.
WEAPON *WeaponsResource :: GetFirstPos( int iSlot )
{
	WEAPON *pret = NULL;

	for (int i = 0; i < MAX_WEAPON_POSITIONS; i++)
	{
		if ( rgSlots[iSlot][i] )
		{
			pret = rgSlots[iSlot][i];
			break;
		}
	}

	return pret;
}


WEAPON* WeaponsResource :: GetNextActivePos( int iSlot, int iSlotPos )
{
	if ( iSlotPos >= MAX_WEAPON_POSITIONS || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;

	WEAPON *p = gWR.rgSlots[ iSlot ][ iSlotPos+1 ];

	if ( !p )
		return GetNextActivePos( iSlot, iSlotPos + 1 );

	return p;
}


int giBucketHeight, giBucketWidth, giABHeight, giABWidth; // Ammo Bar width and height

HSPRITE ghsprBuckets;					// Sprite for top row of weapons menu

DECLARE_MESSAGE(m_Ammo, CurWeapon );	// Current weapon and clip
DECLARE_MESSAGE(m_Ammo, WeaponList);	// new weapon type
DECLARE_MESSAGE(m_Ammo, AmmoX);			// update known ammo type's count
DECLARE_MESSAGE(m_Ammo, AmmoPickup);	// flashes an ammo pickup record
DECLARE_MESSAGE(m_Ammo, WeapPickup);    // flashes a weapon pickup record
DECLARE_MESSAGE(m_Ammo, HideWeapon);	// hides the weapon, ammo, and crosshair displays temporarily
DECLARE_MESSAGE(m_Ammo, ItemPickup);
DECLARE_MESSAGE(m_Ammo, Crosshair);
DECLARE_MESSAGE(m_accountBalance, Money);
DECLARE_MESSAGE(m_accountBalance, BlinkAcct);

DECLARE_COMMAND(m_Ammo, Slot1);
DECLARE_COMMAND(m_Ammo, Slot2);
DECLARE_COMMAND(m_Ammo, Slot3);
DECLARE_COMMAND(m_Ammo, Slot4);
DECLARE_COMMAND(m_Ammo, Slot5);
DECLARE_COMMAND(m_Ammo, Slot6);
DECLARE_COMMAND(m_Ammo, Slot7);
DECLARE_COMMAND(m_Ammo, Slot8);
DECLARE_COMMAND(m_Ammo, Slot9);
DECLARE_COMMAND(m_Ammo, Slot10);
DECLARE_COMMAND(m_Ammo, Close);
DECLARE_COMMAND(m_Ammo, NextWeapon);
DECLARE_COMMAND(m_Ammo, PrevWeapon);
DECLARE_COMMAND(m_Ammo, Adjust_Crosshair);

// width of ammo fonts
#define AMMO_SMALL_WIDTH 10
#define AMMO_LARGE_WIDTH 20

#define HISTORY_DRAW_TIME	"5"

int CHudAmmo::Init(void)
{
	gHUD.AddHudElem(this);

	HOOK_MESSAGE(CurWeapon);
	HOOK_MESSAGE(WeaponList);
	HOOK_MESSAGE(AmmoPickup);
	HOOK_MESSAGE(WeapPickup);
	HOOK_MESSAGE(ItemPickup);
	HOOK_MESSAGE(HideWeapon);
	HOOK_MESSAGE(AmmoX);
	HOOK_MESSAGE(Crosshair);

	HOOK_COMMAND("slot1", Slot1);
	HOOK_COMMAND("slot2", Slot2);
	HOOK_COMMAND("slot3", Slot3);
	HOOK_COMMAND("slot4", Slot4);
	HOOK_COMMAND("slot5", Slot5);
	HOOK_COMMAND("slot6", Slot6);
	HOOK_COMMAND("slot7", Slot7);
	HOOK_COMMAND("slot8", Slot8);
	HOOK_COMMAND("slot9", Slot9);
	HOOK_COMMAND("slot10", Slot10);
	HOOK_COMMAND("cancelselect", Close);
	HOOK_COMMAND("invnext", NextWeapon);
	HOOK_COMMAND("invprev", PrevWeapon);
	HOOK_COMMAND("adjust_crosshair", Adjust_Crosshair);
	gHUD.m_iHideHUDDisplay = 0;

	Reset();

	CVAR_CREATE( "hud_drawhistory_time", HISTORY_DRAW_TIME, 0 );
	CVAR_CREATE( "hud_fastswitch", "0", FCVAR_ARCHIVE );		// controls whether or not weapons can be selected in one keypress
	CVAR_CREATE("cl_observercrosshair", "1", 0);
	CVAR_CREATE("cl_crosshair_color", "50 250 50", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_translucent", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_crosshair_size", "0", FCVAR_ARCHIVE);

	m_iFlags |= HUD_ACTIVE | HUD_INTERMISSION;
	m_bObserverCrosshair = 0;
	m_flCrosshairDistance = 0;
	m_iAmmoLastCheck = 0;
	m_iAlpha = 200;
	m_R = 50;
	m_G = 250;
	m_B = 50;
	m_cvarR = m_cvarG = m_cvarB = -1;
	m_iCurrentCrosshair = 0;
	m_bAdditive = true;
	m_iCrosshairScaleBase = 1024;

	gWR.Init();
	gHR.Init();

	return 1;
};

void CHudAmmo::Reset(void)
{
	m_fFade = 0;
	m_iFlags |= HUD_ACTIVE; //!!!

	gpActiveSel = NULL;
	gHUD.m_iHideHUDDisplay &= HIDEHUD_CROSSHAIR;

	gWR.Reset();
	gHR.Reset();
}

int CHudAmmo::VidInit(void)
{
	// Load sprites for buckets (top row of weapon menu)
	m_HUD_bucket0 = gHUD.GetSpriteIndex( "bucket1" );
	m_HUD_selection = gHUD.GetSpriteIndex( "selection" );

	ghsprBuckets = gHUD.GetSprite(m_HUD_bucket0);
	giBucketWidth = gHUD.GetSpriteRect(m_HUD_bucket0).right - gHUD.GetSpriteRect(m_HUD_bucket0).left;
	giBucketHeight = gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top;

	gHR.iHistoryGap = max( gHR.iHistoryGap, gHUD.GetSpriteRect(m_HUD_bucket0).bottom - gHUD.GetSpriteRect(m_HUD_bucket0).top);

	// If we've already loaded weapons, let's get new sprites
	gWR.LoadAllWeaponSprites();

	int nScale = ScreenWidth >= 640 ? 2 : 1;
	giABWidth = 10 * nScale;
	giABHeight = 2 * nScale;
	int count;
	client_sprite_t *sprites = SPR_GetList("sprites/observer.txt", &count);
	if (sprites)
	{
		client_sprite_t *crosshair = GetSpriteList(sprites, "crosshair", ScreenWidth >= 640 ? 640 : 320, count);
		if (crosshair)
		{
			char name[32];
			sprintf(name, "sprites/%s.spr", crosshair->szSprite);
			m_hObserverCrosshair = SPR_Load(name);
			m_rcObserverCrosshair = crosshair->rc;
		}
		else
		{
			m_hObserverCrosshair = 0;
		}
		gEngfuncs.COM_FreeFile( sprites );
	}

	return 1;
}

//
// Think:
//  Used for selection of weapon menu item.
//
void CHudAmmo::Think(void)
{
	if ( gHUD.m_fPlayerDead )
		return;

	if ( gHUD.m_iWeaponBits != gWR.iOldWeaponBits )
	{
		gWR.iOldWeaponBits = gHUD.m_iWeaponBits;

		for (int i = MAX_WEAPONS-1; i > 0; i-- )
		{
			WEAPON *p = gWR.GetWeapon(i);

			if ( p )
			{
				if ( gHUD.m_iWeaponBits & ( 1 << p->iId ) )
					gWR.PickupWeapon( p );
				else
					gWR.DropWeapon( p );
			}
		}
	}

	if (!gpActiveSel)
		return;

	// has the player selected one?
	if (gHUD.m_iKeyBits & IN_ATTACK)
	{
		if (gpActiveSel != (WEAPON *)1)
		{
			ServerCmd(gpActiveSel->szName);
			g_weaponselect = gpActiveSel->iId;
		}

		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		gHUD.m_iKeyBits &= ~IN_ATTACK;

		PlaySound("common/wpn_select.wav", 1);
	}

}

//
// Helper function to return a Ammo pointer from id
//

HSPRITE* WeaponsResource :: GetAmmoPicFromWeapon( int iAmmoId, wrect_t& rect )
{
	for ( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if ( rgWeapons[i].iAmmoType == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo;
			return &rgWeapons[i].hAmmo;
		}
		else if ( rgWeapons[i].iAmmo2Type == iAmmoId )
		{
			rect = rgWeapons[i].rcAmmo2;
			return &rgWeapons[i].hAmmo2;
		}
	}

	return NULL;
}


// Menu Selection Code

void WeaponsResource :: SelectSlot( int iSlot, int fAdvance, int iDirection )
{
	if ( gHUD.m_Menu.m_fMenuDisplayed && (fAdvance == FALSE) && (iDirection == 1) )
	{ // menu is overriding slot use commands
		gHUD.m_Menu.SelectMenuItem( iSlot + 1 );  // slots are one off the key numbers
		return;
	}

	if ( iSlot > MAX_WEAPON_SLOTS )
		return;

	if ( gHUD.m_fPlayerDead || gHUD.m_iHideHUDDisplay & ( HIDEHUD_WEAPONS | HIDEHUD_ALL ) )
		return;

	if (!(gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)) ))
		return;

	if ( ! ( gHUD.m_iWeaponBits & ~(1<<(WEAPON_SUIT)) ))
		return;

	WEAPON *p = NULL;
	bool fastSwitch = CVAR_GET_FLOAT( "hud_fastswitch" ) != 0;

	if ( (gpActiveSel == NULL) || (gpActiveSel == (WEAPON *)1) || (iSlot != gpActiveSel->iSlot) )
	{
		PlaySound( "common/wpn_hudon.wav", 1 );
		p = GetFirstPos( iSlot );

		if ( p && fastSwitch ) // check for fast weapon switch mode
		{
			// if fast weapon switch is on, then weapons can be selected in a single keypress
			// but only if there is only one item in the bucket
			WEAPON *p2 = GetNextActivePos( p->iSlot, p->iSlotPos );
			if ( !p2 )
			{	// only one active item in bucket, so change directly to weapon
				ServerCmd( p->szName );
				return;
			}
		}
	}
	else
	{
		PlaySound("common/wpn_moveselect.wav", 1);
		if ( gpActiveSel )
			p = GetNextActivePos( gpActiveSel->iSlot, gpActiveSel->iSlotPos );
		if ( !p )
			p = GetFirstPos( iSlot );
	}


	if ( !p )  // no selection found
	{
		// just display the weapon list, unless fastswitch is on just ignore it
		if ( !fastSwitch )
			gpActiveSel = (WEAPON *)1;
		else
			gpActiveSel = NULL;
	}
	else
		gpActiveSel = p;
}

//------------------------------------------------------------------------
// Message Handlers
//------------------------------------------------------------------------

//
// AmmoX  -- Update the count of a known type of ammo
//
int CHudAmmo::MsgFunc_AmmoX(const char *, int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	if ( iIndex >= MAX_AMMO_TYPES )
		return 0;

	gWR.SetAmmo( iIndex, abs(iCount) );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

int CHudAmmo::MsgFunc_AmmoPickup( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();
	int iCount = READ_BYTE();

	// Add ammo to the history
	gHR.AddToHistory( HISTSLOT_AMMO, iIndex, abs(iCount) );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

int CHudAmmo::MsgFunc_WeapPickup( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	int iIndex = READ_BYTE();

	if ( iIndex >= MAX_WEAPONS )
		return 0;

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_WEAP, iIndex );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

int CHudAmmo::MsgFunc_ItemPickup( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );
	const char *szName = READ_STRING();

	// Add the weapon to the history
	gHR.AddToHistory( HISTSLOT_ITEM, szName );
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}


int CHudAmmo::MsgFunc_HideWeapon( const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	gHUD.m_iHideHUDDisplay = READ_BYTE();

	if (gEngfuncs.IsSpectateOnly())
		return 1;

	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_CROSSHAIR) || !gHUD.m_iHideHUDDisplay)
	{
		static wrect_t nullrc;
		SetCrosshair( 0, nullrc, 0, 0, 0 );
	}
	else
	{
		if (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL))
		{
			if (!m_bObserverCrosshair)
			{
				static wrect_t nullrc;
				gpActiveSel = NULL;
				SetCrosshair(0, nullrc, 0, 0, 0);
			}
		}
		else
		{
			if (m_pWeapon && !g_bHoldingShield)
				SetCrosshair(m_pWeapon->hCrosshair, m_pWeapon->rcCrosshair, 255, 255, 255);
			m_bObserverCrosshair = 0;
		}
	}

	return 1;
}

//
//  CurWeapon: Update hud state with the current weapon and clip count. Ammo
//  counts are updated with AmmoX. Server assures that the Weapon ammo type
//  numbers match a real ammo type.
//
int CHudAmmo::MsgFunc_CurWeapon(const char *, int iSize, void *pbuf )
{
	static wrect_t nullrc;
	int fOnTarget = FALSE;

	BEGIN_READ( pbuf, iSize );

	int iState = READ_BYTE();
	int iId = READ_CHAR();
	int iClip = READ_CHAR();

	// detect if we're also on target
	if ( iState > 1 )
	{
		fOnTarget = TRUE;
	}

	if ( iId < 1 && !m_bObserverCrosshair )
	{
		SetCrosshair(0, nullrc, 0, 0, 0);
		return 0;
	}

	if ( g_iUser1 != OBS_IN_EYE )
	{
		// Is player dead???
		if ((iId == -1) && (iClip == -1))
		{
			gHUD.m_fPlayerDead = TRUE;
			gpActiveSel = NULL;
			return 1;
		}
		gHUD.m_fPlayerDead = FALSE;
	}

	// The observer crosshair passes id 0 through; negative ids would index before the array
	if ( iId < 0 || iId >= MAX_WEAPONS )
		return 0;

	WEAPON *pWeapon = gWR.GetWeapon( iId );

	if ( !pWeapon )
		return 0;

	if ( iClip < -1 )
		pWeapon->iClip = abs(iClip);
	else
		pWeapon->iClip = iClip;


	if ( iState == 0 )	// we're not the current weapon, so update no more
		return 1;

	m_pWeapon = pWeapon;

	if ( gHUD.m_iFOV >= 90 )
	{ // normal crosshairs
		SetCrosshair(0, nullrc, 0, 0, 0);
	}
	else
	{ // zoomed crosshairs
		if (fOnTarget && m_pWeapon->hZoomedAutoaim)
			SetCrosshair(m_pWeapon->hZoomedAutoaim, m_pWeapon->rcZoomedAutoaim, 255, 255, 255);
		else
			SetCrosshair(m_pWeapon->hZoomedCrosshair, m_pWeapon->rcZoomedCrosshair, 255, 255, 255);

	}

	m_fFade = 200.0f; //!!!
	m_iFlags |= HUD_ACTIVE;
	m_bObserverCrosshair = 0;
	gCSViewPortInterface->UpdateBuyPresets();

	return 1;
}

//
// WeaponList -- Tells the hud about a new weapon type.
//
int CHudAmmo::MsgFunc_WeaponList(const char *, int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	WEAPON Weapon;

	strncpy( Weapon.szName, READ_STRING(), MAX_WEAPON_NAME );
	Weapon.szName[ sizeof(Weapon.szName) - 1 ] = '\0';

	Weapon.iAmmoType = (int)READ_CHAR();

	Weapon.iMax1 = READ_BYTE();
	if (Weapon.iMax1 == 255)
		Weapon.iMax1 = -1;

	Weapon.iAmmo2Type = READ_CHAR();
	Weapon.iMax2 = READ_BYTE();
	if (Weapon.iMax2 == 255)
		Weapon.iMax2 = -1;

	Weapon.iSlot = READ_CHAR();
	Weapon.iSlotPos = READ_CHAR();
	Weapon.iId = READ_CHAR();
	Weapon.iFlags = READ_BYTE();
	Weapon.iClip = 0;

	if (Weapon.iId < 0 || Weapon.iId >= MAX_WEAPONS)
		return 0;

	if (Weapon.iSlot < 0 || Weapon.iSlot >= MAX_WEAPON_SLOTS+1)
		return 0;

	if (Weapon.iSlotPos < 0 || Weapon.iSlotPos >= MAX_WEAPON_POSITIONS+1)
		return 0;

	if (Weapon.iAmmoType < -1 || Weapon.iAmmoType >= MAX_AMMO_TYPES)
		return 0;

	if (Weapon.iAmmo2Type < -1 || Weapon.iAmmo2Type >= MAX_AMMO_TYPES)
		return 0;

	if (Weapon.iAmmoType >= 0 && Weapon.iMax1 == 0)
		return 0;

	if (Weapon.iAmmo2Type >= 0 && Weapon.iMax2 == 0)
		return 0;

	gWR.AddWeapon( &Weapon );

	return 1;

}

//------------------------------------------------------------------------
// Command Handlers
//------------------------------------------------------------------------
// Slot button pressed
void CHudAmmo::SlotInput( int iSlot )
{
	if ( gViewPortInterface && gViewPortInterface->SlotInput( iSlot ) )
		return;

	gWR.SelectSlot(iSlot, FALSE, 1);
}

void CHudAmmo::UserCmd_Slot1(void)
{
	SlotInput( 0 );
}

void CHudAmmo::UserCmd_Slot2(void)
{
	SlotInput( 1 );
}

void CHudAmmo::UserCmd_Slot3(void)
{
	SlotInput( 2 );
}

void CHudAmmo::UserCmd_Slot4(void)
{
	SlotInput( 3 );
}

void CHudAmmo::UserCmd_Slot5(void)
{
	SlotInput( 4 );
}

void CHudAmmo::UserCmd_Slot6(void)
{
	SlotInput( 5 );
}

void CHudAmmo::UserCmd_Slot7(void)
{
	SlotInput( 6 );
}

void CHudAmmo::UserCmd_Slot8(void)
{
	SlotInput( 7 );
}

void CHudAmmo::UserCmd_Slot9(void)
{
	SlotInput( 8 );
}

void CHudAmmo::UserCmd_Slot10(void)
{
	SlotInput( 9 );
}

void CHudAmmo::UserCmd_Close(void)
{
	if (gpActiveSel)
	{
		gpLastSel = gpActiveSel;
		gpActiveSel = NULL;
		PlaySound("common/wpn_hudoff.wav", 1);
	}
	else
		EngineClientCmd("escape");
}


// Selects the next item in the weapon menu
void CHudAmmo::UserCmd_NextWeapon(void)
{
	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = 0;
	int slot = 0;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos + 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot < MAX_WEAPON_SLOTS; slot++ )
		{
			for ( ; pos < MAX_WEAPON_POSITIONS; pos++ )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp )
				{
					if (CVAR_GET_FLOAT("hud_fastswitch") == 1.0f)
					{
						PlaySound("common/wpn_hudon.wav", 1);
						ServerCmd(wsp->szName);
						gpActiveSel = NULL;
					}
					else
					{
						gpActiveSel = wsp;
					}
					return;
				}
			}

			pos = 0;
		}

		slot = 0;  // start looking from the first slot again
	}

	gpActiveSel = NULL;
}

// Selects the previous item in the menu
void CHudAmmo::UserCmd_PrevWeapon(void)
{
	if ( gHUD.m_fPlayerDead || (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)) )
		return;

	if ( !gpActiveSel || gpActiveSel == (WEAPON*)1 )
		gpActiveSel = m_pWeapon;

	int pos = MAX_WEAPON_POSITIONS-1;
	int slot = MAX_WEAPON_SLOTS-1;
	if ( gpActiveSel )
	{
		pos = gpActiveSel->iSlotPos - 1;
		slot = gpActiveSel->iSlot;
	}

	for ( int loop = 0; loop <= 1; loop++ )
	{
		for ( ; slot >= 0; slot-- )
		{
			for ( ; pos >= 0; pos-- )
			{
				WEAPON *wsp = gWR.GetWeaponSlot( slot, pos );

				if ( wsp )
				{
					if (CVAR_GET_FLOAT("hud_fastswitch") == 1.0f)
					{
						PlaySound("common/wpn_hudon.wav", 1);
						ServerCmd(wsp->szName);
						gpActiveSel = NULL;
					}
					else
					{
						gpActiveSel = wsp;
					}
					return;
				}
			}

			pos = MAX_WEAPON_POSITIONS-1;
		}

		slot = MAX_WEAPON_SLOTS-1;
	}

	gpActiveSel = NULL;
}



//-------------------------------------------------------------------------
// Drawing code
//-------------------------------------------------------------------------

int CHudAmmo::Draw(float flTime)
{
	int a, x, y, r, g, b;
	int AmmoWidth;
	static wrect_t nullrc;

	if (gEngfuncs.IsSpectateOnly())
	{
		if (g_iUser1 != OBS_IN_EYE)
		{
			SetCrosshair(0, nullrc, 0, 0, 0);
			return 1;
		}
		WEAPON *sniper = gWR.GetWeapon(WEAPON_AWP);
		if (gHUD.m_iFOV > 44 || !sniper->hZoomedCrosshair)
		{
			SetCrosshair(0, nullrc, 0, 0, 0);
			DrawCrosshair(flTime, WEAPON_NONE);
		}
		else
		{
			SetCrosshair(sniper->hZoomedCrosshair, sniper->rcZoomedCrosshair, 255, 255, 255);
		}
		return 1;
	}
	if (gHUD.m_iIntermission)
	{
		SetCrosshair(0, nullrc, 0, 0, 0);
		return 1;
	}

	if (g_iUser1 != OBS_IN_EYE)
	{
		if (!(gHUD.m_iWeaponBits & (1U << WEAPON_SUIT))
			|| (gHUD.m_iHideHUDDisplay & (HIDEHUD_WEAPONS | HIDEHUD_ALL)))
			return 1;

		// Draw Weapon Menu
		DrawWList(flTime);

		// Draw ammo pickup history
		gHR.DrawAmmoHistory(flTime);
		if (!(m_iFlags & HUD_ACTIVE))
			return 0;
	}

	if (!m_pWeapon)
		return 0;

	WEAPON *pw = m_pWeapon; // shorthand
	if (gHUD.m_iFOV > 54 && !(gHUD.m_iHideHUDDisplay & HIDEHUD_CROSSHAIR)
		&& pw->iId != WEAPON_SCOUT && pw->iId != WEAPON_SG550 && pw->iId != WEAPON_AWP && pw->iId != WEAPON_G3SG1)
		DrawCrosshair(flTime, pw->iId);
	if (g_iUser1 == OBS_IN_EYE)
		return 1;

	// SPR_Draw Ammo
	if ((pw->iAmmoType < 0) && (pw->iAmmo2Type < 0))
		return 0;


	int iFlags = DHN_DRAWZERO; // draw 0 values

	AmmoWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;

	a = max<int>( MIN_ALPHA, m_fFade );

	if (m_fFade > 0)
		m_fFade -= (gHUD.m_flTimeDelta * 20);

	UnpackRGB(r,g,b, RGB_YELLOWISH);

	ScaleColors(r, g, b, a );

	y = ScreenHeight - gHUD.m_iFontHeight - gHUD.m_iFontHeight/2;

	// Does weapon have any ammo at all?
	if (m_pWeapon->iAmmoType > 0)
	{
		int iIconWidth = m_pWeapon->rcAmmo.right - m_pWeapon->rcAmmo.left;

		if (pw->iClip >= 0)
		{
			// room for the number and the '|' and the current ammo

			x = ScreenWidth - (8 * AmmoWidth) - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, pw->iClip, r, g, b);

			wrect_t rc;
			rc.top = 0;
			rc.left = 0;
			rc.right = AmmoWidth;
			rc.bottom = 100;

			int iBarWidth =  AmmoWidth/10;

			x += AmmoWidth/2;

			UnpackRGB(r,g,b, RGB_YELLOWISH);

			// draw the | bar
			FillRGBA(x, y, iBarWidth, gHUD.m_iFontHeight, r, g, b, a);

			x += iBarWidth + AmmoWidth/2;;

			// GL Seems to need this
			ScaleColors(r, g, b, a );
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);


		}
		else
		{
			// SPR_Draw a bullets only line
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags | DHN_3DIGITS, gWR.CountAmmo(pw->iAmmoType), r, g, b);
		}

		// Draw the ammo Icon
		int iOffset = (m_pWeapon->rcAmmo.bottom - m_pWeapon->rcAmmo.top)/8;
		SPR_Set(m_pWeapon->hAmmo, r, g, b);
		SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo);
	}

	// Does weapon have seconday ammo?
	if (pw->iAmmo2Type > 0)
	{
		int iIconWidth = m_pWeapon->rcAmmo2.right - m_pWeapon->rcAmmo2.left;

		// Do we have secondary ammo?
		if ((pw->iAmmo2Type != 0) && (gWR.CountAmmo(pw->iAmmo2Type) > 0))
		{
			y -= gHUD.m_iFontHeight + gHUD.m_iFontHeight/4;
			x = ScreenWidth - 4 * AmmoWidth - iIconWidth;
			x = gHUD.DrawHudNumber(x, y, iFlags|DHN_3DIGITS, gWR.CountAmmo(pw->iAmmo2Type), r, g, b);

			// Draw the ammo Icon
			SPR_Set(m_pWeapon->hAmmo2, r, g, b);
			int iOffset = (m_pWeapon->rcAmmo2.bottom - m_pWeapon->rcAmmo2.top)/8;
			SPR_DrawAdditive(0, x, y - iOffset, &m_pWeapon->rcAmmo2);
		}
	}
	return 1;
}


//
// Draws the ammo bar on the hud
//
int DrawBar(int x, int y, int width, int height, float f)
{
	int r, g, b;

	if (f >= 0 && f != 0)
	{
		if (f > 1)
			f = 1;
		int w = f * width;

		// Always show at least one pixel if we have ammo.
		if (w <= 0)
			w = 1;
		UnpackRGB(r, g, b, RGB_GREENISH);
		FillRGBA(x, y, w, height, r, g, b, 255);
		x += w;
		width -= w;
	}

	UnpackRGB(r, g, b, RGB_YELLOWISH);

	FillRGBA(x, y, width, height, r, g, b, 128);

	return (x + width);
}



void DrawAmmoBar(WEAPON *p, int x, int y, int width, int height)
{
	if ( !p )
		return;

	if (p->iAmmoType != -1)
	{
		if (!gWR.CountAmmo(p->iAmmoType))
			return;

		float f = (float)gWR.CountAmmo(p->iAmmoType)/(float)p->iMax1;

		x = DrawBar(x, y, width, height, f);


		// Do we have secondary ammo too?

		if (p->iAmmo2Type != -1)
		{
			f = (float)gWR.CountAmmo(p->iAmmo2Type)/(float)p->iMax2;

			x += 5; //!!!

			DrawBar(x, y, width, height, f);
		}
	}
}




//
// Draw Weapon Menu
//
int CHudAmmo::DrawWList(float)
{
	int r,g,b,x,y,a,i;

	if ( !gpActiveSel )
		return 0;

	int iActiveSlot;

	if ( gpActiveSel == (WEAPON *)1 )
		iActiveSlot = -1;	// current slot has no weapons
	else
		iActiveSlot = gpActiveSel->iSlot;

	x = ScreenWidth >= 640 ? 138 : 70;
	y = 10; //!!!


	// Ensure that there are available choices in the active slot
	if ( iActiveSlot > 0 )
	{
		if ( !gWR.GetFirstPos( iActiveSlot ) )
		{
			gpActiveSel = (WEAPON *)1;
			iActiveSlot = -1;
		}
	}

	// Draw top line
	for ( i = 0; i < MAX_WEAPON_SLOTS; i++ )
	{
		int iWidth;

		UnpackRGB(r,g,b, RGB_YELLOWISH);

		if ( iActiveSlot == i )
			a = 255;
		else
			a = 192;

		ScaleColors(r, g, b, 255);
		SPR_Set(gHUD.GetSprite(m_HUD_bucket0 + i), r, g, b );

		// make active slot wide enough to accomodate gun pictures
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos(iActiveSlot);
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;
			else
				iWidth = giBucketWidth;
		}
		else
			iWidth = giBucketWidth;

		SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_bucket0 + i));

		x += iWidth + 5;
	}


	a = 128; //!!!
	x = ScreenWidth >= 640 ? 138 : 70;

	// Draw all of the buckets
	for (i = 0; i < MAX_WEAPON_SLOTS; i++)
	{
		y = giBucketHeight + 10;

		// If this is the active slot, draw the bigger pictures,
		// otherwise just draw boxes
		if ( i == iActiveSlot )
		{
			WEAPON *p = gWR.GetFirstPos( i );
			int iWidth = giBucketWidth;
			if ( p )
				iWidth = p->rcActive.right - p->rcActive.left;

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				UnpackRGB( r,g,b, RGB_YELLOWISH );

				// if active, then we must have ammo.

				if ( gpActiveSel == p )
				{
					if (gWR.HasAmmo(p))
						ScaleColors(r, g, b, 192);
					else
					{
						UnpackRGB(r, g, b, RGB_REDISH);
						ScaleColors(r, g, b, 128);
					}
					SPR_Set(p->hActive, r, g, b );
					SPR_DrawAdditive(0, x, y, &p->rcActive);

					SPR_Set(gHUD.GetSprite(m_HUD_selection), r, g, b );
					SPR_DrawAdditive(0, x, y, &gHUD.GetSpriteRect(m_HUD_selection));
				}
				else
				{
					// Draw Weapon if Red if no ammo

					if ( gWR.HasAmmo(p) )
						ScaleColors(r, g, b, 192);
					else
					{
						UnpackRGB(r,g,b, RGB_REDISH);
						ScaleColors(r, g, b, 128);
					}

					SPR_Set( p->hInactive, r, g, b );
					SPR_DrawAdditive( 0, x, y, &p->rcInactive );
				}

				// Draw Ammo Bar

				DrawAmmoBar(p, x + giABWidth/2, y + 3, giABWidth, giABHeight);

				y += p->rcActive.bottom - p->rcActive.top + 5;
			}

			x += iWidth + 5;

		}
		else
		{
			// Draw Row of weapons.

			UnpackRGB(r,g,b, RGB_YELLOWISH);

			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				WEAPON *p = gWR.GetWeaponSlot( i, iPos );

				if ( !p || !p->iId )
					continue;

				if ( gWR.HasAmmo(p) )
				{
					UnpackRGB(r,g,b, RGB_YELLOWISH);
					a = 128;
				}
				else
				{
					UnpackRGB(r,g,b, RGB_REDISH);
					a = 96;
				}

				FillRGBA( x, y, giBucketWidth, giBucketHeight, r, g, b, a );

				y += giBucketHeight + 5;
			}

			x += giBucketWidth + 5;
		}
	}

	return 1;

}


int CHudAmmo::MsgFunc_Crosshair(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int show = READ_BYTE();
	if (gEngfuncs.IsSpectateOnly() || CVAR_GET_FLOAT("cl_observercrosshair") == 0)
		return 1;
	if (show)
	{
		SetCrosshair(m_hObserverCrosshair, m_rcObserverCrosshair, 255, 255, 255);
	}
	else
	{
		static wrect_t nullrc;
		SetCrosshair(0, nullrc, 0, 0, 0);
	}
	m_bObserverCrosshair = 1;
	return 1;
}

int CHudAccountBalance::Init()
{
	HOOK_MESSAGE(Money);
	HOOK_MESSAGE(BlinkAcct);
	gHUD.AddHudElem(this);
	Reset();
	return 1;
}

void CHudAccountBalance::Reset()
{
	m_fFade = 0;
	m_fFadeFast = 0;
	m_iFlags |= HUD_ACTIVE;
	m_fBlinkTime = 0;
	m_iAccount = 0;
	m_iAccountDelta = 0;
	m_iBlinkCount = 0;
}

int CHudAccountBalance::VidInit()
{
	m_HUD_dollar = gHUD.GetSpriteIndex("dollar");
	m_HUD_minus = gHUD.GetSpriteIndex("minus");
	m_HUD_plus = gHUD.GetSpriteIndex("plus");
	return 1;
}

int CHudAccountBalance::GetBalance()
{
	return m_iAccount;
}

int CHudAccountBalance::MsgFunc_Money(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int account = READ_LONG();
	int show = READ_BYTE();
	m_iAccountDelta = account - m_iAccount;
	m_iAccount = account;
	m_bShowDelta = show;
	if (m_iAccountDelta)
	{
		m_fFade = 5;
		m_fFadeFast = 2.5f;
	}
	else if (show)
	{
		m_bShowDelta = 0;
	}
	gCSViewPortInterface->UpdateBuyPresets();
	return 1;
}

int CHudAccountBalance::MsgFunc_BlinkAcct(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	m_iBlinkCount = 2 * READ_BYTE();
	m_fBlinkTime = 0;
	return 1;
}

int CHudAccountBalance::Draw(float time)
{
	if (gEngfuncs.IsSpectateOnly())
		return 1;
	int r = 255, g = 160, b = 0;
	int alpha = MIN_ALPHA;
	if (m_fFade != 0)
	{
		m_fFade = (float)(m_fFade - gHUD.m_flTimeDelta);
		if (m_fFade <= 0)
		{
			m_fFade = 0;
			m_iAccountDelta = 0;
		}
		else
		{
			double fade = m_fFade / 5.0;
			alpha = (int)(float)(128.0 * fade + MIN_ALPHA);
			double remaining = (5.0 - m_fFade) / 5.0;
			if (m_iAccountDelta > 0)
			{
				r = (int)(float)(255.0 * remaining + 0.0 * fade);
				g = (int)(float)(160.0 * remaining + fade * 255.0);
				b = (int)(float)(0.0 * fade + remaining * 0.0);
			}
			else if (m_iAccountDelta)
			{
				r = (int)(float)(255.0 * remaining + 255.0 * fade);
				g = (int)(float)(160.0 * remaining + fade * 0.0);
				b = (int)(float)(fade * 0.0 + remaining * 0.0);
			}
		}
	}
	ScaleColors(r, g, b, alpha);
	int deltaR = r, deltaG = g, deltaB = b;
	if (m_bShowDelta && m_fFadeFast != 0)
	{
		m_fFadeFast = (float)(m_fFadeFast - gHUD.m_flTimeDelta);
		if (m_fFadeFast <= 0)
		{
			m_fFadeFast = 0;
			m_bShowDelta = 0;
		}
		else
		{
			deltaR = r;
			deltaG = g;
			deltaB = b;
			ScaleColors(deltaR, deltaG, deltaB, (int)(m_fFadeFast / 2.5 * 256.0));
		}
	}
	if (m_iBlinkCount > 0)
	{
		if (time > m_fBlinkTime)
		{
			m_iBlinkCount--;
			m_fBlinkTime = (float)(time + 0.1);
		}
		if (m_iBlinkCount & 1)
		{
			r = 255;
			g = 160;
			b = 0;
			ScaleColors(r, g, b, 255);
		}
	}
	if ((gHUD.m_iHideHUDDisplay & HIDEHUD_MONEY) || g_iUser1)
		return 1;
	wrect_t &number = gHUD.GetSpriteRect(gHUD.m_HUD_number_0);
	int x = ScreenWidth - (4 * (ScreenWidth > 639) + 12) - 5 * (number.right - number.left);
	int y = ScreenHeight - ((gHUD.m_iWeaponBits & (1U << WEAPON_SUIT)) ? 3 : 5) * gHUD.m_iFontHeight;
	wrect_t &dollar = gHUD.GetSpriteRect(m_HUD_dollar);
	SPR_Set(gHUD.GetSprite(m_HUD_dollar), r, g, b);
	SPR_DrawAdditive(0, x + dollar.left - dollar.right, y, &dollar);
	gHUD.DrawHudNumber(x, y, m_iAccount, r, g, b);
	if (m_bShowDelta)
	{
		int sprite = m_iAccountDelta < 0 ? m_HUD_minus : m_HUD_plus;
		int deltaY = y - (gHUD.m_iFontHeight / 2 + gHUD.m_iFontHeight);
		wrect_t &rect = gHUD.GetSpriteRect(sprite);
		SPR_Set(gHUD.GetSprite(sprite), deltaR, deltaG, deltaB);
		SPR_DrawAdditive(0, x + rect.left - rect.right, deltaY, &rect);
		gHUD.DrawHudNumber(x, deltaY, abs(m_iAccountDelta), deltaR, deltaG, deltaB);
	}
	return 1;
}

int GetWeaponMaxSpeed(int weapon)
{
	switch (weapon)
	{
	case WEAPON_AUG:
	case WEAPON_GALIL:
	case WEAPON_FAMAS:
	case WEAPON_M249:
	case WEAPON_M4A1:
	case WEAPON_SG552:
	case WEAPON_AK47:
		return 140;
	case WEAPON_P90:
		return 170;
	default:
		return 0;
	}
}

int GetWeaponAccuracyFlags(int weapon)
{
	switch (weapon)
	{
	case WEAPON_P228:
	case WEAPON_FIVESEVEN:
	case WEAPON_DEAGLE:
		return 7;
	case WEAPON_MAC10:
	case WEAPON_UMP45:
	case WEAPON_MP5N:
	case WEAPON_TMP:
		return 1;
	case WEAPON_AUG:
	case WEAPON_GALIL:
	case WEAPON_M249:
	case WEAPON_SG552:
	case WEAPON_AK47:
	case WEAPON_P90:
		return 3;
	case WEAPON_FAMAS:
		return (g_iWeaponFlags & 0x10) ? 19 : 3;
	case WEAPON_USP:
		return (g_iWeaponFlags & 1) ? 15 : 7;
	case WEAPON_GLOCK18:
		return (g_iWeaponFlags & 2) ? 23 : 7;
	case WEAPON_M4A1:
		return (g_iWeaponFlags & 4) ? 11 : 3;
	default:
		return 0;
	}
}

void CHudAmmo::CalculateCrosshairColor()
{
	const char *text = CVAR_GET_STRING("cl_crosshair_color");
	if (!text)
		return;
	text = SharedParse(text);
	const char *token = SharedGetToken();
	if (!token)
		return;
	int r = atoi(token);
	text = SharedParse(text);
	token = SharedGetToken();
	if (!token)
		return;
	int g = atoi(token);
	SharedParse(text);
	token = SharedGetToken();
	if (!token)
		return;
	int b = atoi(token);
	if (r != m_cvarR || g != m_cvarG || b != m_cvarB)
	{
		m_R = max(0, min(255, r));
		m_G = max(0, min(255, g));
		m_B = max(0, min(255, b));
		m_cvarR = r;
		m_cvarG = g;
		m_cvarB = b;
	}
}

void CHudAmmo::CalculateCrosshairDrawMode()
{
	float translucent = CVAR_GET_FLOAT("cl_crosshair_translucent");
	if (translucent == 0)
		m_bAdditive = false;
	else if (translucent == 1)
		m_bAdditive = true;
	else
		gEngfuncs.Con_Printf("usage: cl_crosshair_translucent <1|0>\n");
}

void CHudAmmo::CalculateCrosshairSize()
{
	const char *text = CVAR_GET_STRING("cl_crosshair_size");
	if (!text)
		return;
	int size = atoi(text);
	if ((unsigned int)size >= 4 || (!size && strcmp(text, "0")))
		size = -1;
	if (!stricmp(text, "auto"))
		size = 0;
	else if (!stricmp(text, "small"))
		size = 1;
	else if (!stricmp(text, "medium"))
		size = 2;
	else if (!stricmp(text, "large"))
		size = 3;
	switch (size)
	{
	case -1:
		gEngfuncs.Con_Printf("usage: cl_crosshair_size <auto|small|medium|large>\n");
		break;
	case 0:
		m_iCrosshairScaleBase = ScreenWidth <= 640 ? 1024 : (ScreenWidth <= 1023 ? 800 : 640);
		break;
	case 1:
		m_iCrosshairScaleBase = 1024;
		break;
	case 2:
		m_iCrosshairScaleBase = 800;
		break;
	default:
		m_iCrosshairScaleBase = 640;
		break;
	}
}

void CHudAmmo::UserCmd_Adjust_Crosshair()
{
	int newCrosshair = (m_iCurrentCrosshair + 1) % 5;
	if (gEngfuncs.Cmd_Argc() > 1)
		newCrosshair = atoi(gEngfuncs.Cmd_Argv(1)) % 10;
	m_iCurrentCrosshair = newCrosshair;

	switch (m_iCurrentCrosshair)
	{
	case 1:
		m_R = 250;
		m_G = 50;
		m_B = 50;
		m_bAdditive = true;
		break;
	case 2:
		m_R = 50;
		m_G = 50;
		m_B = 250;
		m_bAdditive = true;
		break;
	case 3:
		m_R = 250;
		m_G = 250;
		m_B = 50;
		m_bAdditive = true;
		break;
	case 4:
		m_R = 50;
		m_G = 250;
		m_B = 250;
		m_bAdditive = true;
		break;
	case 5:
		m_R = 50;
		m_G = 250;
		m_B = 50;
		m_bAdditive = false;
		break;
	case 6:
		m_R = 250;
		m_G = 50;
		m_B = 50;
		m_bAdditive = false;
		break;
	case 7:
		m_R = 50;
		m_G = 50;
		m_B = 250;
		m_bAdditive = false;
		break;
	case 8:
		m_R = 250;
		m_G = 250;
		m_B = 50;
		m_bAdditive = false;
		break;
	case 9:
		m_R = 50;
		m_G = 250;
		m_B = 250;
		m_bAdditive = false;
		break;
	default:
		m_R = 50;
		m_G = 250;
		m_B = 50;
		m_bAdditive = true;
		break;
	}

	char command[255];
	command[0] = 0;
	sprintf(command, "%d %d %d", m_R, m_G, m_B);
	gEngfuncs.Cvar_Set("cl_crosshair_color", command);
	if (m_bAdditive)
		gEngfuncs.Cvar_Set("cl_crosshair_translucent", "1");
	else
		gEngfuncs.Cvar_Set("cl_crosshair_translucent", "0");
}

int CHudAmmo::DrawCrosshair(float, int weapon)
{
	int distance = 4;
	int delta = 3;
	switch (weapon)
	{
	case WEAPON_P228:
	case WEAPON_HEGRENADE:
	case WEAPON_SMOKEGRENADE:
	case WEAPON_FIVESEVEN:
	case WEAPON_USP:
	case WEAPON_GLOCK18:
	case WEAPON_AWP:
	case WEAPON_FLASHBANG:
	case WEAPON_DEAGLE:
		distance = 8;
		break;
	case WEAPON_SCOUT:
	case WEAPON_SG550:
	case WEAPON_SG552:
		distance = 5;
		break;
	case WEAPON_XM1014:
		distance = 9;
		delta = 4;
		break;
	case WEAPON_C4:
	case WEAPON_UMP45:
	case WEAPON_M249:
		distance = 6;
		break;
	case WEAPON_MAC10:
		distance = 9;
		break;
	case WEAPON_AUG:
		distance = 3;
		break;
	case WEAPON_MP5N:
		distance = 6;
		delta = 2;
		break;
	case WEAPON_M3:
		distance = 8;
		delta = 6;
		break;
	case WEAPON_TMP:
	case WEAPON_KNIFE:
	case WEAPON_P90:
		distance = 7;
		break;
	case WEAPON_G3SG1:
		distance = 6;
		delta = 4;
		break;
	case WEAPON_AK47:
		delta = 4;
		break;
	}
	int accuracy = GetWeaponAccuracyFlags(weapon);
	if (accuracy && cl_dynamiccrosshair && cl_dynamiccrosshair->value != 0
		&& !(gHUD.m_iHideHUDDisplay & HIDEHUD_WEAPONS))
	{
		if (!(g_iPlayerFlags & FL_ONGROUND) && (accuracy & 1))
			distance *= 2;
		else if ((g_iPlayerFlags & FL_DUCKING) && (accuracy & 4))
			distance = (int)(float)(distance * 0.5);
		else if ((int)g_flPlayerSpeed > GetWeaponMaxSpeed(weapon) && (accuracy & 2))
			distance = (int)(float)(distance * 1.5);
		if (accuracy & 8)
			distance = (int)(float)(distance * 1.4f);
		if (accuracy & 16)
			distance = (int)(float)(distance * 1.4f);
	}
	if (m_iAmmoLastCheck >= g_iShotsFired)
	{
		m_flCrosshairDistance = (float)(m_flCrosshairDistance - (0.013 * m_flCrosshairDistance + 0.1));
		m_iAlpha += 2;
	}
	else
	{
		m_flCrosshairDistance += (float)delta;
		m_iAlpha -= 40;
		if (m_flCrosshairDistance > 15)
			m_flCrosshairDistance = 15;
		if (m_iAlpha < 120)
			m_iAlpha = 120;
	}
	if (g_iShotsFired > 600)
		g_iShotsFired = 1;
	m_iAmmoLastCheck = g_iShotsFired;
	if ((float)distance > m_flCrosshairDistance)
		m_flCrosshairDistance = (float)distance;
	if (m_iAlpha > 255)
		m_iAlpha = 255;
	int length = (int)(float)((m_flCrosshairDistance - distance) * 0.5) + 5;
	CalculateCrosshairColor();
	CalculateCrosshairDrawMode();
	CalculateCrosshairSize();
	double scaledDistance = m_flCrosshairDistance;
	if (ScreenWidth != m_iCrosshairScaleBase)
	{
		scaledDistance = scaledDistance * ScreenWidth / m_iCrosshairScaleBase;
		length = ScreenWidth * length / m_iCrosshairScaleBase;
	}
	int gap = (int)(float)scaledDistance;
	int r = m_R, g = m_G, b = m_B;
	if (gHUD.m_NightVision.m_fOn)
	{
		r = 250;
		g = 50;
		b = 50;
	}
	void (*fill)(int, int, int, int, int, int, int, int) = gEngfuncs.pfnFillRGBA;
	if (gHUD.m_NightVision.m_fOn || !m_bAdditive)
		fill = gEngfuncs.pfnFillRGBABlend;
	fill(ScreenWidth / 2 - gap - length + 1, ScreenHeight / 2, length, 1, r, g, b, m_iAlpha);
	fill(ScreenWidth / 2 + gap, ScreenHeight / 2, length, 1, r, g, b, m_iAlpha);
	fill(ScreenWidth / 2, ScreenHeight / 2 - gap - length + 1, 1, length, r, g, b, m_iAlpha);
	fill(ScreenWidth / 2, ScreenHeight / 2 + gap, 1, length, r, g, b, m_iAlpha);
	return 1;
}


/* =================================
	GetSpriteList

Finds and returns the matching 
sprite name 'psz' and resolution 'iRes'
in the given sprite list 'pList'
iCount is the number of items in the pList
================================= */
client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount)
{
	if (!pList)
		return NULL;

	int i = iCount;
	client_sprite_t *p = pList;

	while(i--)
	{
		if ((p->iRes == iRes) && (!strcmp(psz, p->szName)))
			return p;
		p++;
	}

	return NULL;
}
