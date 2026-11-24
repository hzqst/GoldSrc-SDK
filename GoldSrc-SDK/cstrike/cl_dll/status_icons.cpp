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
// status_icons.cpp
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "event_api.h"
#include <string.h>

DECLARE_MESSAGE(m_StatusIcons, StatusIcon)

int CHudStatusIcons::Init()
{
	HOOK_MESSAGE(StatusIcon);
	gHUD.AddHudElem(this);
	Reset();
	return 1;
}

int CHudStatusIcons::VidInit()
{
	int index = gHUD.GetSpriteIndex("cross");
	m_iCrossWidth = gHUD.GetSpriteRect(index).right - gHUD.GetSpriteRect(index).left;
	return 1;
}

void CHudStatusIcons::Reset()
{
	memset(m_IconList, 0, sizeof(m_IconList));
	m_iFlags &= ~HUD_ACTIVE;
	m_bFlashOn = 1;
	m_tmNextFlash = 0;
}

// Draw status icons along the left-hand side of the screen
int CHudStatusIcons::Draw(float time)
{
	if (gEngfuncs.IsSpectateOnly())
		return 1;
	int x = m_iCrossWidth / 2;
	int y = ScreenHeight / 2;

	// loop through icon list, and draw any valid icons drawing up from the middle of screen
	for (int i = 0; i < MAX_ICONSPRITES; i++)
	{
		if (time > m_tmNextFlash)
		{
			m_tmNextFlash = (float)(time + 0.1);
			m_bFlashOn = 1 - m_bFlashOn;
		}
		if (m_IconList[i].spr)
		{
			y -= m_IconList[i].rc.bottom - m_IconList[i].rc.top + 5;
			if (!m_bFlashOn && m_IconList[i].bFlash)
				SPR_Set(m_IconList[i].spr, 250, 0, 0);
			else
				SPR_Set(m_IconList[i].spr, m_IconList[i].r, m_IconList[i].g, m_IconList[i].b);
			SPR_DrawAdditive(0, x, y, &m_IconList[i].rc);
		}
	}
	return 1;
}

// Message handler for StatusIcon message
// accepts five values:
//		byte   : TRUE = ENABLE icon, FALSE = DISABLE icon
//		string : the sprite name to display
//		byte   : red
//		byte   : green
//		byte   : blue
int CHudStatusIcons::MsgFunc_StatusIcon(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int enable = READ_BYTE();
	char *name = READ_STRING();
	if (enable)
	{
		unsigned char r = (unsigned char)READ_BYTE();
		unsigned char g = (unsigned char)READ_BYTE();
		unsigned char b = (unsigned char)READ_BYTE();
		EnableIcon(name, r, g, b, enable == 2);
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		DisableIcon(name);
	}
	return 1;
}

void CHudStatusIcons::EnableIcon(char *name, unsigned char r, unsigned char g, unsigned char b, bool flash)
{
	int i;
	// check to see if the sprite is in the current list
	for (i = 0; i < MAX_ICONSPRITES; i++)
	{
		if (!stricmp(m_IconList[i].szSpriteName, name))
			break;
	}
	if (i == MAX_ICONSPRITES)
	{
		// icon not in list, so find an empty slot to add to
		for (i = 0; i < MAX_ICONSPRITES; i++)
		{
			if (!m_IconList[i].spr)
				break;
		}
	}
	if (i == MAX_ICONSPRITES)
		i = 0;
	int index = gHUD.GetSpriteIndex(name);
	m_IconList[i].spr = gHUD.GetSprite(index);
	m_IconList[i].rc = gHUD.GetSpriteRect(index);
	m_IconList[i].r = r;
	m_IconList[i].g = g;
	m_IconList[i].b = b;
	m_IconList[i].bFlash = flash;
	strncpy(m_IconList[i].szSpriteName, name, sizeof(m_IconList[i].szSpriteName) - 1);
	m_IconList[i].szSpriteName[sizeof(m_IconList[i].szSpriteName) - 1] = 0;

	// Hack: Play Timer sound when a grenade icon is played (in 0.8 seconds)
	if (strstr(m_IconList[i].szSpriteName, "grenade"))
	{
		cl_entity_t *player = gEngfuncs.GetLocalPlayer();
		gEngfuncs.pEventAPI->EV_PlaySound(player->index, player->origin, CHAN_STATIC,
			"weapons/timer.wav", 1, ATTN_NORM, 0, PITCH_NORM);
	}
}

void CHudStatusIcons::DisableIcon(char *name)
{
	// find the sprite is in the current list
	for (int i = 0; i < MAX_ICONSPRITES; i++)
	{
		if (!stricmp(m_IconList[i].szSpriteName, name))
		{
			// clear the item from the list
			memset(&m_IconList[i], 0, sizeof(m_IconList[i]));
			return;
		}
	}
}

bool CHudStatusIcons::InBuyZone() const
{
	for (int i = 0; i < MAX_ICONSPRITES; i++)
	{
		if (!stricmp(m_IconList[i].szSpriteName, "buyzone"))
			return true;
	}
	return false;
}
