#include "hud.h"
#include "cl_util.h"
#include "r_studioint.h"
#include "e_effectclasses.h"

extern engine_studio_api_t IEngineStudio;

void CreateGasSmoke(Vector origin, Vector velocity, bool insideSmoke, bool spawnInside, bool blowable)
{
	const char *spriteName = IEngineStudio.IsHardware() ? "sprites/gas_puff_01.spr" : "sprites/black_smoke4.spr";
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(spriteName)));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: sprites/gas_puff_01.spr\n");
		return;
	}
	float scale = gEngfuncs.pfnRandomFloat(175.0f, 225.0f);
	CPartSmokeGrenade *particle = new (std::nothrow) CPartSmokeGrenade;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, scale, 190.0f);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->m_flGravity = 0.0f;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_vRingOrigin = origin;
	particle->m_vVelocity = velocity;
	particle->m_flSize = scale;
	particle->m_flFadeSpeed = -1.0f;
	particle->m_iFrame = IEngineStudio.IsHardware() ? gEngfuncs.pfnRandomLong(0, 1) : gEngfuncs.pfnRandomLong(15, 25);
	particle->m_flScaleSpeed = 0.1f;
	if (insideSmoke)
	{
		float spin = gEngfuncs.pfnRandomFloat(-0.8f, 0.8f);
		particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
		particle->m_flDampingTime = 0.0f;
		particle->m_bInsideSmoke = true;
		particle->m_flScaleSpeed = 0.05f;
		particle->m_bSpawnInside = spawnInside;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 30.0f;
	}
	else
	{
		float spin = gEngfuncs.pfnRandomFloat(-1.0f, 1.0f);
		particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
		particle->m_flDampingTime = gEngfuncs.GetClientTime() + 0.001f;
		particle->m_bInsideSmoke = false;
		particle->m_bSpawnInside = spawnInside;
		particle->m_bDampMod = false;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 20.0f;
	}
	particle->m_flMass = gEngfuncs.pfnRandomFloat(1.0f, 1.5f);
	float color = gEngfuncs.pfnRandomFloat(155.0f, 175.0f);
	particle->m_vColor = Vector(color, color, color);
	particle->SetCullFlag(CULL_FRUSTUM_PLANE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
	particle->m_bAffectedByForce = !blowable;
}

void CreateGasInside(Vector origin)
{
	Vector forward, right, up;
	for (int angle = 0; angle != 360; angle += 45)
	{
		gEngfuncs.pfnAngleVectors(Vector(0.0f, static_cast<float>(angle), 0.0f), forward, right, up);
		forward = forward.Normalize();
		float radius = gEngfuncs.pfnRandomFloat(90.0f, 110.0f);
		CreateGasSmoke(origin + forward * radius + up * 45.0f, Vector(0, 0, 0), true, false, true);
	}
	for (int angle = 0; angle != 360; angle += 45)
	{
		gEngfuncs.pfnAngleVectors(Vector(0.0f, static_cast<float>(angle), 0.0f), forward, right, up);
		forward = forward.Normalize();
		CreateGasSmoke(origin + forward * 90.0f + up * 60.0f, Vector(0, 0, 0), true, false, true);
	}
	gEngfuncs.pfnAngleVectors(Vector(0, 45, 0), forward, right, up);
	forward = forward.Normalize();
	CreateGasSmoke(origin + forward * 45.0f + up * 55.0f, Vector(0, 0, 0), true, false, true);
	gEngfuncs.pfnAngleVectors(Vector(0, 270, 0), forward, right, up);
	forward = forward.Normalize();
	CreateGasSmoke(origin + forward * 45.0f + up * 55.0f, Vector(0, 0, 0), true, false, true);
	CreateGasSmoke(origin, Vector(0, 0, 0), true, false, true);
}

void CreatePoisonSmoke(Vector origin, Vector velocity, bool insideSmoke, bool spawnInside, bool blowable)
{
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/gas_puff_01.spr")));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: sprites/gas_puff_01.spr\n");
		return;
	}
	float scale = gEngfuncs.pfnRandomFloat(175.0f, 225.0f);
	CPoisonSmoke *particle = new (std::nothrow) CPoisonSmoke;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, scale, 190.0f);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->m_flGravity = 0.0f;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_vRingOrigin = origin;
	particle->m_vVelocity = velocity;
	particle->m_flSize = scale;
	particle->m_flFadeSpeed = -1.0f;
	particle->m_iFrame = gEngfuncs.pfnRandomLong(0, 1);
	particle->m_flScaleSpeed = 0.1f;
	if (insideSmoke)
	{
		float spin = gEngfuncs.pfnRandomFloat(-0.8f, 0.8f);
		particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
		particle->m_flDampingTime = 0.0f;
		particle->m_bInsideSmoke = true;
		particle->m_flScaleSpeed = 0.05f;
		particle->m_bSpawnInside = spawnInside;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 30.0f;
	}
	else
	{
		float spin = gEngfuncs.pfnRandomFloat(-1.0f, 1.0f);
		particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
		particle->m_flDampingTime = gEngfuncs.GetClientTime() + 0.001f;
		particle->m_bInsideSmoke = false;
		particle->m_bSpawnInside = spawnInside;
		particle->m_bDampMod = false;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 20.0f;
	}
	particle->m_flMass = gEngfuncs.pfnRandomFloat(1.0f, 1.5f);
	particle->m_vColor = Vector(209, 203, 118);
	particle->SetCullFlag(CULL_FRUSTUM_PLANE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
	particle->m_bAffectedByForce = !blowable;
}

void CreatePoisonInside(Vector origin)
{
	Vector forward, right, up;
	for (int angle = 0; angle != 360; angle += 45)
	{
		gEngfuncs.pfnAngleVectors(Vector(0.0f, static_cast<float>(angle), 0.0f), forward, right, up);
		forward = forward.Normalize();
		float radius = gEngfuncs.pfnRandomFloat(90.0f, 110.0f);
		CreatePoisonSmoke(origin + forward * radius + up * 45.0f, Vector(0, 0, 0), true, false, true);
	}
	for (int angle = 0; angle != 360; angle += 45)
	{
		gEngfuncs.pfnAngleVectors(Vector(0.0f, static_cast<float>(angle), 0.0f), forward, right, up);
		forward = forward.Normalize();
		CreatePoisonSmoke(origin + forward * 90.0f + up * 60.0f, Vector(0, 0, 0), true, false, true);
	}
	gEngfuncs.pfnAngleVectors(Vector(0, 45, 0), forward, right, up);
	forward = forward.Normalize();
	CreatePoisonSmoke(origin + forward * 45.0f + up * 55.0f, Vector(0, 0, 0), true, false, true);
	gEngfuncs.pfnAngleVectors(Vector(0, 270, 0), forward, right, up);
	forward = forward.Normalize();
	CreatePoisonSmoke(origin + forward * 45.0f + up * 55.0f, Vector(0, 0, 0), true, false, true);
	CreatePoisonSmoke(origin, Vector(0, 0, 0), true, false, true);
}

void CPoisonSmoke::Think(float time)
{
	float remaining = m_flDieTime - time;
	if (m_flBrightness >= 255.0f && remaining < 3.0f)
	{
		m_flScaleSpeed = 0.0f;
		m_vAVelocity = Vector(0, 0, 0);
		m_flTimeCreated = time;
	}
	if (remaining <= 5.0f)
	{
		if (m_flBrightness > 0.0f)
			m_flBrightness -= (time - m_flTimeCreated) * 0.1f;
		if (m_flBrightness < 0.0f)
		{
			m_flBrightness = 0.0f;
			m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	else
	{
		if (m_flBrightness < 255.0f && m_flFadeSpeed == 0.0f)
			m_flBrightness += (time - m_flTimeCreated) * 5.0f + (m_bInsideSmoke ? 8.0f : 10.0f);
		if (m_flBrightness > 255.0f)
			m_flBrightness = 255.0f;
	}
	CBaseParticle::Think(time);
	if (m_flDampingTime != 0.0f && time >= m_flDampingTime)
	{
		if (!m_bDampMod)
		{
			m_bDampMod = true;
			m_flDampingTime = gEngfuncs.GetClientTime() + 0.2f;
			if (m_bSpawnInside)
				CreatePoisonInside(m_vRingOrigin);
		}
		if (m_flDampingTime != 0.0f && time >= m_flDampingTime && m_bDampMod)
		{
			m_flDampingTime = 0.0f;
			m_vVelocity = m_vVelocity * 0.035f;
		}
	}
}

void CPartSmokeGrenade::Think(float time)
{
	float remaining = m_flDieTime - time;
	if (m_flBrightness >= 255.0f && remaining < 3.0f)
	{
		m_flScaleSpeed = 0.0f;
		m_vAVelocity = Vector(0, 0, 0);
		m_flTimeCreated = time;
	}
	if (remaining <= 5.0f)
	{
		if (m_flBrightness > 0.0f)
			m_flBrightness -= (time - m_flTimeCreated) * 0.1f;
		if (m_flBrightness < 0.0f)
		{
			m_flBrightness = 0.0f;
			m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	else
	{
		if (m_flBrightness < 255.0f && m_flFadeSpeed == 0.0f)
			m_flBrightness += (time - m_flTimeCreated) * 5.0f + (m_bInsideSmoke ? 8.0f : 10.0f);
		if (m_flBrightness > 255.0f)
			m_flBrightness = 255.0f;
	}
	CBaseParticle::Think(time);
	if (m_flDampingTime != 0.0f && time >= m_flDampingTime)
	{
		if (!m_bDampMod)
		{
			m_bDampMod = true;
			m_flDampingTime = gEngfuncs.GetClientTime() + 0.2f;
			if (m_bSpawnInside)
				CreateGasInside(m_vRingOrigin);
		}
		if (m_flDampingTime != 0.0f && time >= m_flDampingTime && m_bDampMod)
		{
			m_flDampingTime = 0.0f;
			m_vVelocity = m_vVelocity * 0.035f;
		}
	}
}
