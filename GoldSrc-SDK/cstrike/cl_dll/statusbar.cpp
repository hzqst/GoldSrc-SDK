/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
// statusbar.cpp
//
// generic text status bar, set by game dll
// runs across bottom of screen
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <stdlib.h>
#include <string.h>

DECLARE_MESSAGE(m_StatusBar, StatusText)
DECLARE_MESSAGE(m_StatusBar, StatusValue)

float *GetClientColor(int clientIndex);
extern float g_ColorYellow[3];

int CHudStatusBar::Init()
{
	gHUD.AddHudElem(this);
	HOOK_MESSAGE(StatusText);
	HOOK_MESSAGE(StatusValue);
	Reset();
	CVAR_CREATE("hud_centerid", "0", FCVAR_ARCHIVE);
	return 1;
}

int CHudStatusBar::VidInit()
{
	// Load sprites here

	return 1;
}

void CHudStatusBar::Reset()
{
	m_iFlags &= ~HUD_ACTIVE;  // start out inactive
	m_szStatusText[0][0] = 0;
	memset(m_iStatusValues, 0, sizeof(m_iStatusValues));
	m_iStatusValues[0] = 1;  // 0 is the special index, which always returns true
	for (int i = 0; i < MAX_PLAYERS; i++)
		g_PlayerExtraInfo[i].showhealth = 0;
	m_pflNameColors[0] = g_ColorYellow;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4701)
#endif
void CHudStatusBar::ParseStatusString(int line)
{
	char text[128] = {};
	char replacement[128];
	gHUD.m_TextMessage.LocaliseTextString(m_szStatusText[line], text, sizeof(text));
	memset(m_szStatusBar[line], 0, sizeof(m_szStatusBar[line]));
	char *src = text;
	char *dst = m_szStatusBar[line];
	char *dstStart = dst;
	while (*src)
	{
		while (*src == '\n')
			src++;  // skip over any newlines
		if (src - text >= 128 || dst - dstStart >= 127)
			break;
		int index = atoi(src);
		if (index >= 0 && index < 8 && m_iStatusValues[index])
		{  // parse this line and append result to the status bar
			while (*src >= '0' && *src <= '9')
				src++;
			if (*src == '\n' || !*src)
				continue;

			// copy the text, char by char, until we hit a % or a \n
			while (*src != '\n' && *src)
			{
				if (dst - dstStart >= 127)
					break;
				if (*src != '%')
				{  // just copy the character
					*dst++ = *src++;
					continue;
				}
				char type = *++src;
				if (type == '%')
				{
					*dst++ = *src++;
					continue;
				}
				if (type == 0)
					break;

				// move over descriptor, then get and move over the index
				index = atoi(++src);
				while (*src >= '0' && *src <= '9')
					src++;
				if (index < 0 || index >= 8)
					continue;
				int value = m_iStatusValues[index];
				client_textmessage_t *message;
				replacement[0] = 0;
				switch (type)
				{
				case 'p':
					if (value < 1 || value > MAX_PLAYERS)
					{
						strcpy(replacement, "******");
						break;
					}
					gEngfuncs.pfnGetPlayerInfo(value, &g_PlayerInfoList[value]);
					if (g_PlayerInfoList[value].name)
					{
						strncpy(replacement, g_PlayerInfoList[value].name, sizeof(replacement));
						replacement[sizeof(replacement) - 1] = 0;
						gHUD.m_Health.m_iPlayerLastPointedAt = value;
						m_pflNameColors[line] = GetClientColor(value);
					}
					else
					{
						strcpy(replacement, "******");
					}
					break;
				case 'i':  // number
					if (gHUD.m_Health.m_iPlayerLastPointedAt >= 0 && gHUD.m_Health.m_iPlayerLastPointedAt <= MAX_PLAYERS)
					{
						g_PlayerExtraInfo[gHUD.m_Health.m_iPlayerLastPointedAt].health = value;
						g_PlayerExtraInfo[gHUD.m_Health.m_iPlayerLastPointedAt].showhealth = gHUD.m_flTime + 5.0f;
					}
					_snprintf(replacement, sizeof(replacement), "%d", value);
					replacement[sizeof(replacement) - 1] = 0;
					break;
				case 'h':
					message = TextMessageGet("Health");
					if (message)
					{
						strncpy(replacement, message->pMessage, sizeof(replacement));
						replacement[sizeof(replacement) - 1] = 0;
					}
					else
						strcpy(replacement, "Health");
					break;
				case 'c':
					switch (value)
					{
					case 1:
						message = TextMessageGet("Friend");
						if (message)
						{
							strncpy(replacement, message->pMessage, sizeof(replacement));
							replacement[sizeof(replacement) - 1] = 0;
						}
						else
							strcpy(replacement, "Friend");
						break;
					case 2:
						message = TextMessageGet("Enemy");
						if (message)
						{
							strncpy(replacement, message->pMessage, sizeof(replacement));
							replacement[sizeof(replacement) - 1] = 0;
						}
						else
							strcpy(replacement, "Enemy");
						break;
					case 3:
						message = TextMessageGet("Hostage");
						if (message)
						{
							strncpy(replacement, message->pMessage, sizeof(replacement));
							replacement[sizeof(replacement) - 1] = 0;
						}
						else
							strcpy(replacement, "Hostage");
						break;
					default:
						replacement[0] = 0;
						break;
					}
					break;
				default:
					continue;
				}
				for (char *copy = replacement; *copy && dst - dstStart < 127; copy++, dst++)
					*dst = *copy;
			}
		}
		else
		{
			// skip to next line of text
			while (*src && *src != '\n')
				src++;
		}
	}
	*dst = 0;
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

int CHudStatusBar::MsgFunc_StatusText(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	if (READ_BYTE() != 0)
		return 1;
	strncpy(m_szStatusText[0], READ_STRING(), sizeof(m_szStatusText[0]));
	m_szStatusText[0][127] = 0;
	m_iFlags |= HUD_ACTIVE;
	m_bReparseString = TRUE;
	return 1;
}

// Message handler for StatusText message
// accepts two values:
//		byte: index into the status value array
//		short: value to store
int CHudStatusBar::MsgFunc_StatusValue(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	int index = READ_BYTE();
	if (index >= 1 && index <= 7)
	{
		m_iStatusValues[index] = READ_SHORT();
		m_bReparseString = TRUE;
	}
	return 1;
}

int CHudStatusBar::Draw(float)
{
	if (m_bReparseString)
	{
		m_pflNameColors[0] = g_ColorYellow;
		ParseStatusString(0);
		m_bReparseString = FALSE;
	}
	if (g_iUser1 > 0)
		return 0;
	int width, height;
	GetConsoleStringSize(m_szStatusBar[0], &width, &height);
	int x, y;
	if (CVAR_GET_FLOAT("hud_centerid") == 0)
	{
		x = 10;
		y = ScreenHeight - (int)(gHUD.m_iFontHeight * 2.75);
	}
	else
	{
		x = max(2, ScreenWidth - width) / 2;
		y = (int)(float)(CVAR_GET_FLOAT("hud_centerid") * (float)height + (float)(ScreenHeight / 2));
	}
	if (m_pflNameColors[0])
		gEngfuncs.pfnDrawSetTextColor(m_pflNameColors[0][0], m_pflNameColors[0][1], m_pflNameColors[0][2]);
	DrawConsoleString(x, y, m_szStatusBar[0]);
	return 1;
}
