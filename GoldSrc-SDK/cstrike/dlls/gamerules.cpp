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
//=========================================================
// GameRules.cpp
//=========================================================

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "skill.h"
#include "bot/cs_bot_manager.h"
#include "career_tasks.h"
#include "tutor.h"
#include "hostage/hostage.h"

extern edict_t *EntSelectSpawnPoint(CBaseEntity *pPlayer);

DLL_GLOBAL CGameRules *g_pGameRules = NULL;

CGameRules *InstallGameRules()
{
	SERVER_COMMAND("exec game.cfg\n");
	SERVER_EXECUTE();
	if (!gpGlobals->deathmatch)
		return new CHalfLifeTraining;
	return new CHalfLifeMultiplay;
}

// Frees the game rules and the managers they install, which otherwise live until the next map
void ShutdownGameRules()
{
	delete g_pGameRules;
	g_pGameRules = NULL;
	delete TheBots;
	TheBots = NULL;
	delete g_pHostages;
	g_pHostages = NULL;
	delete TheTutor;
	TheTutor = NULL;
	delete TheCareerTasks;
	TheCareerTasks = NULL;
}

void CGameRules::RefreshSkillData()
{
	int skill = (int)CVAR_GET_FLOAT("skill");
	if (skill < SKILL_EASY)
		skill = SKILL_EASY;
	else if (skill > SKILL_HARD)
		skill = SKILL_HARD;
	gSkillData.iSkillLevel = skill;
	ALERT(at_console, "\nGAME SKILL LEVEL:%d\n", skill);
	gSkillData.monDmg12MM = 8;
	gSkillData.monDmgMP5 = 3;
	gSkillData.monDmg9MM = 5;
	gSkillData.suitchargerCapacity = 75;
	gSkillData.batteryCapacity = 15;
	gSkillData.healthchargerCapacity = 50;
	gSkillData.healthkitCapacity = 15;
}

BOOL CGameRules::CanHaveAmmo(CBasePlayer *pPlayer, const char *pszAmmoName, int iMaxCarry)
{
	if (!pszAmmoName)
		return FALSE;
	int ammoIndex = pPlayer->GetAmmoIndex(pszAmmoName);
	return ammoIndex >= 0 && pPlayer->AmmoInventory(ammoIndex) < iMaxCarry;
}

//=========================================================
//=========================================================
edict_t *CGameRules::GetPlayerSpawnSpot(CBasePlayer *pPlayer)
{
	edict_t *pSpot = EntSelectSpawnPoint(pPlayer);
	pPlayer->pev->origin = VARS(pSpot)->origin + Vector(0, 0, 1);
	pPlayer->pev->v_angle = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = VARS(pSpot)->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = 1;
	return pSpot;
}

BOOL CGameRules::CanHavePlayerItem(CBasePlayer *pPlayer, CBasePlayerItem *pWeapon)
{
	// only living players can have items
	if (pPlayer->pev->deadflag != DEAD_NO)
		return FALSE;
	if (pPlayer->IsBot() && !TheCSBots()->IsWeaponUseable(pWeapon))
		return FALSE;

	ItemInfo &info = CBasePlayerItem::ItemInfoArray[pWeapon->m_iId];
	if (!info.pszAmmo1 || !CanHaveAmmo(pPlayer, info.pszAmmo1, info.iMaxAmmo1))
		return !pPlayer->HasPlayerItem(pWeapon);
	return TRUE;
}
