#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "bot/bot.h"

extern BOOL g_fGameOver;
extern int gmsgBlinkAcct;
extern int gmsgStatusIcon;

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
CHalfLifeTraining::CHalfLifeTraining()
{
	PRECACHE_MODEL("models/w_weaponbox.mdl");
}

BOOL CHalfLifeTraining::IsMultiplayer()
{
	return FALSE;
}

void CHalfLifeTraining::HostageDied()
{
	CBaseEntity *pPlayer = UTIL_PlayerByIndex(1);
	if (pPlayer)
		pPlayer->pev->radsuit_finished = gpGlobals->time + 3;
}

bool CHalfLifeTraining::PlayerCanBuy(CBasePlayer *pPlayer)
{
	return (pPlayer->m_signals.GetState() & SIGNAL_BUY) != 0;
}

edict_t *CHalfLifeTraining::GetPlayerSpawnSpot(CBasePlayer *pPlayer)
{
	CBaseEntity *pSpot = UTIL_FindEntityByClassname(NULL, "info_player_start");
	if (!pSpot || FNullEnt(pSpot->edict()))
	{
		ALERT(at_error, "PutClientInServer: no info_player_start on level");
		return INDEXENT(0);
	}
	pPlayer->pev->origin = pSpot->pev->origin + Vector(0, 0, 1);
	pPlayer->pev->v_angle = g_vecZero;
	pPlayer->pev->velocity = g_vecZero;
	pPlayer->pev->angles = pSpot->pev->angles;
	pPlayer->pev->punchangle = g_vecZero;
	pPlayer->pev->fixangle = 1;
	return pSpot->edict();
}

void CHalfLifeTraining::PlayerSpawn(CBasePlayer *pPlayer)
{
	if (!pPlayer->m_bNotKilled)
	{
		fInBuyArea = TRUE;
		fVisitedBuyArea = FALSE;
		FillAccountTime = 0;
		pPlayer->m_iTeam = CT;
		pPlayer->m_iJoiningState = JOINED;
		pPlayer->m_bNotKilled = true;
		pPlayer->pev->body = 0;
		pPlayer->m_iModelName = MODEL_URBAN;
		fVGUIMenus = pPlayer->m_bVGUIMenus;
		SET_MODEL(pPlayer->edict(), "models/player.mdl");
		CBaseEntity *pEquip = NULL;
		while ((pEquip = UTIL_FindEntityByClassname(pEquip, "game_player_equip")) != NULL)
			pEquip->Touch(pPlayer);
		pPlayer->SetPlayerModel(false);
		pPlayer->Spawn();
		pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS | HIDEHUD_HEALTH | HIDEHUD_TIMER | HIDEHUD_MONEY;
	}
}

int CHalfLifeTraining::ItemShouldRespawn(CItem *pItem)
{
	return GR_ITEM_RESPAWN_NO;
}

BOOL CHalfLifeTraining::FPlayerCanRespawn(CBasePlayer *pPlayer)
{
	return TRUE;
}

void CHalfLifeTraining::PlayerKilled(CBasePlayer *pVictim, entvars_t *pKiller, entvars_t *pInflictor)
{
	SET_VIEW(pVictim->edict(), pVictim->edict());
	FireTargets("game_playerdie", pVictim, pVictim, USE_TOGGLE, 0);
}

void CHalfLifeTraining::PlayerThink(CBasePlayer *pPlayer)
{
	if (pPlayer->pev->radsuit_finished != 0 && gpGlobals->time > pPlayer->pev->radsuit_finished)
		SERVER_COMMAND("reload\n");
	if (!pPlayer->m_iAccount && pPlayer->pev->scale != 0)
		pPlayer->m_iAccount = (int)pPlayer->pev->scale;
	if (pPlayer->m_iTeam == UNASSIGNED)
	{
		pPlayer->SetProgressBarTime(0);
		pPlayer->m_bHasDefuser = pPlayer->pev->ideal_yaw != 0;
	}
	m_iHostagesRescued = 0;
	m_bFreezePeriod = FALSE;
	m_iRoundTimeSecs = (int)(gpGlobals->time + 1);
	g_fGameOver = FALSE;
	pPlayer->m_iTeam = CT;
	pPlayer->m_bCanShoot = true;
	pPlayer->m_fLastMovement = gpGlobals->time;
	if (pPlayer->m_pActiveItem)
		pPlayer->m_iHideHUD &= ~HIDEHUD_WEAPONS;
	else
		pPlayer->m_iHideHUD |= HIDEHUD_WEAPONS;

	if (pPlayer->HasNamedPlayerItem("weapon_c4"))
	{
		if (pPlayer->AmmoInventory(pPlayer->GetAmmoIndex("C4")) <= 0)
		{
			pPlayer->m_bHasC4 = false;
			CBasePlayerItem *pItem = pPlayer->m_pActiveItem;
			if (FClassnameIs(pItem->pev, "weapon_c4"))
			{
				pPlayer->pev->weapons &= ~(1U << pItem->m_iId);
				pPlayer->RemovePlayerItem(pItem);
				pItem->Drop();
			}
		}
		else
			pPlayer->m_bHasC4 = true;
	}
	if (!pPlayer->m_bVGUIMenus && fVGUIMenus)
		pPlayer->m_bVGUIMenus = true;
	CBaseEntity *pEntity = NULL;
	while ((pEntity = UTIL_FindEntityByClassname(pEntity, "grenade")) != NULL)
	{
		CGrenade *pGrenade = (CGrenade *)pEntity;
		if (pGrenade->m_pentCurBombTarget)
		{
			pGrenade->m_bStartDefuse = true;
			break;
		}
	}

	if (pPlayer->m_signals.GetState() & SIGNAL_BUY)
	{
		if (!fInBuyArea)
		{
			FillAccountTime = 1;
			if (!fVisitedBuyArea)
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, pPlayer->edict());
					WRITE_BYTE(2);
					WRITE_STRING("buyzone");
					WRITE_BYTE(0);
					WRITE_BYTE(160);
					WRITE_BYTE(0);
				MESSAGE_END();
			}
		}
		fInBuyArea = TRUE;
		if (pPlayer->m_iAccount < 16000 && FillAccountTime == 0)
			FillAccountTime = gpGlobals->time + 5;
		if (FillAccountTime != 0 && gpGlobals->time > FillAccountTime)
		{
			if (!fVisitedBuyArea)
			{
				MESSAGE_BEGIN(MSG_ONE, gmsgBlinkAcct, NULL, pPlayer->edict());
					WRITE_BYTE(3);
				MESSAGE_END();
				MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, NULL, pPlayer->edict());
					WRITE_BYTE(1);
					WRITE_STRING("buyzone");
					WRITE_BYTE(0);
					WRITE_BYTE(160);
					WRITE_BYTE(0);
				MESSAGE_END();
				fVisitedBuyArea = TRUE;
			}
			pPlayer->AddAccount(16000 - pPlayer->m_iAccount, true);
			FillAccountTime = 0;
		}
	}
	else if (fInBuyArea && fVisitedBuyArea)
		fInBuyArea = FALSE;
	pPlayer->pev->scale = (float)pPlayer->m_iAccount;
	pPlayer->pev->ideal_yaw = (float)pPlayer->m_bHasDefuser;
	TheBots->OnEvent(EVENT_PLAYER_CHANGED_TEAM, pPlayer);
}

void CHalfLifeTraining::CheckMapConditions()
{
}

void CHalfLifeTraining::CheckWinConditions()
{
	CBaseEntity *pEntity = NULL;
	if (m_bBombDefused)
	{
		while ((pEntity = UTIL_FindEntityByClassname(pEntity, "grenade")) != NULL)
		{
			CGrenade *pBomb = (CGrenade *)pEntity;
			if (pBomb->m_bIsC4 && pBomb->m_bJustBlew)
			{
				pBomb->m_bJustBlew = false;
				m_bBombDefused = false;
				FireTargets(STRING(pBomb->pev->target), CBaseEntity::Instance(pBomb->pev->owner), CBaseEntity::Instance(pBomb->pev->owner), USE_TOGGLE, 0);
				break;
			}
		}
	}
	else if (m_bTargetBombed)
	{
		while ((pEntity = UTIL_FindEntityByClassname(pEntity, "grenade")) != NULL)
		{
			CGrenade *pBomb = (CGrenade *)pEntity;
			if (pBomb->m_bIsC4 && pBomb->m_bJustBlew)
			{
				if (pBomb->pev->noise1)
				{
					pBomb->m_bJustBlew = false;
					m_bTargetBombed = false;
					FireTargets(STRING(pBomb->pev->noise1), CBaseEntity::Instance(pBomb->pev->owner), CBaseEntity::Instance(pBomb->pev->owner), USE_TOGGLE, 0);
				}
				break;
			}
		}
	}

	pEntity = CBaseEntity::Instance(FIND_ENTITY_BY_STRING(NULL, "classname", "hostage_entity"));
	while (pEntity)
	{
		if (pEntity->pev->deadflag == DEAD_RESPAWNABLE && !pEntity->pev->noise1)
			break;
		pEntity = UTIL_FindEntityByClassname(pEntity, "hostage_entity");
	}
	if (pEntity)
	{
		UTIL_SetSize(pEntity->pev, Vector(-16, -16, 0), Vector(16, 16, 72));
		CBaseEntity *pRescue = CBaseEntity::Instance(FIND_ENTITY_BY_STRING(NULL, "classname", "func_hostage_rescue"));
		if (pRescue)
		{
			pEntity->pev->noise1 = 1;
			FireTargets(STRING(pRescue->pev->target), NULL, NULL, USE_TOGGLE, 0);
		}
	}
}

class CBaseGrenCatch : public CBaseEntity
{
public:
	void Spawn();
	void Think();
	void Touch(CBaseEntity *pOther);
	void KeyValue(KeyValueData *pkvd);
	int Save(CSave &save);
	int Restore(CRestore &restore);
	int ObjectCaps() { return 0; }
	static TYPEDESCRIPTION m_SaveData[];

private:
	int m_NeedGrenadeType;
	string_t sTriggerOnGrenade;
	string_t sDisableOnGrenade;
	bool m_fSmokeTouching;
	bool m_fFlashTouched;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS(func_grencatch, CBaseGrenCatch);

TYPEDESCRIPTION CBaseGrenCatch::m_SaveData[] =
{
	DEFINE_FIELD(CBaseGrenCatch, m_NeedGrenadeType, FIELD_INTEGER),
	DEFINE_FIELD(CBaseGrenCatch, m_fSmokeTouching, FIELD_BOOLEAN),
	DEFINE_FIELD(CBaseGrenCatch, m_fFlashTouched, FIELD_BOOLEAN),
	DEFINE_FIELD(CBaseGrenCatch, sTriggerOnGrenade, FIELD_STRING),
	DEFINE_FIELD(CBaseGrenCatch, sDisableOnGrenade, FIELD_STRING)
};

IMPLEMENT_SAVERESTORE(CBaseGrenCatch, CBaseEntity);

void CBaseGrenCatch::Spawn()
{
	pev->solid = SOLID_TRIGGER;
	pev->flags |= FL_WORLDBRUSH;
	pev->effects |= EF_NODRAW;
	SET_MODEL(edict(), STRING(pev->model));
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CBaseGrenCatch::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "triggerongrenade"))
	{
		sTriggerOnGrenade = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "disableongrenade"))
	{
		sDisableOnGrenade = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	if (FStrEq(pkvd->szKeyName, "grenadetype"))
	{
		if (FStrEq(pkvd->szValue, "smoke"))
			m_NeedGrenadeType = 1;
		else if (FStrEq(pkvd->szValue, "flash"))
			m_NeedGrenadeType = 2;
		pkvd->fHandled = TRUE;
	}
	else
		pkvd->fHandled = FALSE;
}

void CBaseGrenCatch::Touch(CBaseEntity *pOther)
{
	if (pOther && strstr(STRING(pOther->pev->model), "flash"))
		m_fFlashTouched = true;
}

void CBaseGrenCatch::Think()
{
	bool wasSmokeTouching = m_fSmokeTouching;
	m_fSmokeTouching = false;
	CBaseEntity *pGrenade = NULL;
	while ((pGrenade = UTIL_FindEntityByClassname(pGrenade, "grenade")) != NULL)
	{
		Vector oldMaxs = pGrenade->pev->maxs;
		Vector oldMins = pGrenade->pev->mins;
		UTIL_SetSize(pGrenade->pev, Vector(-8, -8, 0), Vector(8, 8, 0));
		if (pGrenade->Intersects(this) && strstr(STRING(pGrenade->pev->model), "smoke") && pGrenade->pev->velocity.Length() == 0)
			m_fSmokeTouching = true;
		pGrenade->pev->maxs = oldMaxs;
		pGrenade->pev->mins = oldMins;
	}
	if ((m_NeedGrenadeType == 1 && !wasSmokeTouching && m_fSmokeTouching) || (m_NeedGrenadeType == 2 && m_fFlashTouched))
	{
		FireTargets(STRING(sTriggerOnGrenade), this, this, USE_TOGGLE, 0);
		if (m_NeedGrenadeType == 1)
		{
			CBaseEntity *pTarget = NULL;
			while ((pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(sDisableOnGrenade))) != NULL)
			{
				pTarget->pev->team = pTarget->pev->solid;
				pTarget->pev->solid = SOLID_NOT;
			}
		}
		else if (m_NeedGrenadeType == 2)
			pev->flags |= FL_KILLME;
	}
	if (wasSmokeTouching && !m_fSmokeTouching)
	{
		CBaseEntity *pTarget = NULL;
		while ((pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(sDisableOnGrenade))) != NULL)
		{
			pTarget->pev->solid = pTarget->pev->team;
			pTarget->pev->team = 0;
			UTIL_SetOrigin(pTarget->pev, pTarget->pev->origin);
		}
	}
	pev->nextthink = gpGlobals->time + 0.1f;
}

class CFuncWeaponCheck : public CBaseEntity
{
public:
	void Spawn();
	void Touch(CBaseEntity *pOther);
	void KeyValue(KeyValueData *pkvd);
	int Save(CSave &save);
	int Restore(CRestore &restore);
	static TYPEDESCRIPTION m_SaveData[];

private:
	string_t sTriggerWithItems;
	string_t sTriggerNoItems;
	string_t sMaster;
	string_t sItemName[32];
	int iItemCount;
	int iAnyWeapon;
#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
};

LINK_ENTITY_TO_CLASS(func_weaponcheck, CFuncWeaponCheck);

TYPEDESCRIPTION CFuncWeaponCheck::m_SaveData[] =
{
	DEFINE_FIELD(CFuncWeaponCheck, sTriggerWithItems, FIELD_STRING),
	DEFINE_FIELD(CFuncWeaponCheck, sTriggerNoItems, FIELD_STRING),
	DEFINE_FIELD(CFuncWeaponCheck, iItemCount, FIELD_INTEGER),
	DEFINE_FIELD(CFuncWeaponCheck, sMaster, FIELD_STRING),
	DEFINE_ARRAY(CFuncWeaponCheck, sItemName, FIELD_STRING, 32),
	DEFINE_FIELD(CFuncWeaponCheck, iAnyWeapon, FIELD_INTEGER)
};

IMPLEMENT_SAVERESTORE(CFuncWeaponCheck, CBaseEntity);

void CFuncWeaponCheck::Spawn()
{
	pev->dmgtime = 0;
	pev->solid = SOLID_TRIGGER;
	pev->flags |= FL_WORLDBRUSH;
	pev->effects |= EF_NODRAW;
	SET_MODEL(edict(), STRING(pev->model));
}

void CFuncWeaponCheck::Touch(CBaseEntity *pOther)
{
	if (!UTIL_IsMasterTriggered(sMaster, pOther) || !pOther || !pOther->IsPlayer())
		return;
	CBasePlayer *pPlayer = (CBasePlayer *)pOther;
	for (int i = 0; i < iItemCount; ++i)
	{
		if (iAnyWeapon)
		{
			if (pPlayer->HasNamedPlayerItem(STRING(sItemName[i])))
				break;
		}
		else if (!pPlayer->HasNamedPlayerItem(STRING(sItemName[i])))
		{
			if (gpGlobals->time > pev->dmgtime && pev->speed > -1)
			{
				FireTargets(STRING(sTriggerNoItems), pOther, pOther, USE_TOGGLE, 0);
				pev->dmgtime = gpGlobals->time + pev->speed;
				if (pev->speed == 0)
					pev->speed = -1;
			}
			return;
		}
	}
	FireTargets(STRING(sTriggerWithItems), pOther, pOther, USE_TOGGLE, 0);
	SUB_Remove();
}

void CFuncWeaponCheck::KeyValue(KeyValueData *pkvd)
{
	if (FStrEq(pkvd->szKeyName, "trigger_items"))
		sTriggerWithItems = ALLOC_STRING(pkvd->szValue);
	else if (FStrEq(pkvd->szKeyName, "trigger_noitems"))
		sTriggerNoItems = ALLOC_STRING(pkvd->szValue);
	else if (FStrEq(pkvd->szKeyName, "trigger_noitems_delay"))
		pev->speed = atof(pkvd->szValue);
	else if (strstr(pkvd->szKeyName, "item"))
	{
		if (iItemCount < (int)ARRAYSIZE(sItemName))
			sItemName[iItemCount++] = ALLOC_STRING(pkvd->szValue);
	}
	else if (FStrEq(pkvd->szKeyName, "master"))
		sMaster = ALLOC_STRING(pkvd->szValue);
	else if (FStrEq(pkvd->szKeyName, "any_weapon"))
		iAnyWeapon = atoi(pkvd->szValue);
	else
	{
		pkvd->fHandled = FALSE;
		return;
	}
	pkvd->fHandled = TRUE;
}
