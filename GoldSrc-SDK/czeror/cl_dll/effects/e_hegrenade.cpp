#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "e_effectclasses.h"

void CreateDustRing(Vector origin, Vector velocity, float scale)
{
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/gas_puff_01.spr")));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: sprites/gas_puff_01.spr\n");
		return;
	}
	CTriangleDust *particle = new (std::nothrow) CTriangleDust;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, scale, 235.0f);
	particle->m_flGravity = 0.0f;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_vVelocity = velocity;
	particle->m_flSize = scale;
	float spin = gEngfuncs.pfnRandomFloat(1.0f, -3.0f);
	particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
	particle->m_flActivateTime = gEngfuncs.GetClientTime() + 0.15f;
	particle->m_flScaleSpeed = 0.2f;
	particle->m_flFadeSpeed = 0.0f;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 2.0f;
	particle->m_flMass = gEngfuncs.pfnRandomFloat(2.0f, 3.0f);
	particle->m_vColor = Vector(150, 150, 120);
	particle->m_iFrame = gEngfuncs.pfnRandomLong(0, 1);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
}

void CreateGrenadeSmoke(Vector origin, Vector velocity, bool insideSmoke)
{
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/gas_puff_01.spr")));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: sprites/gas_puff_01.spr\n");
		return;
	}
	float scale = gEngfuncs.pfnRandomFloat(140.0f, 180.0f);
	CHEGrenadeSmoke *particle = new (std::nothrow) CHEGrenadeSmoke;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, scale, 255.0f);
	particle->m_flGravity = 0.0f;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_vVelocity = velocity * 0.5f;
	particle->m_flSize = scale;
	particle->m_flFadeSpeed = 0.0f;
	particle->m_flScaleSpeed = 0.2f;
	particle->m_iFrame = gEngfuncs.pfnRandomLong(0, 1);
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 6.5f;
	if (insideSmoke)
	{
		float spin = gEngfuncs.pfnRandomFloat(-1.5f, 1.5f);
		particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
		particle->m_flActivateTime = gEngfuncs.GetClientTime() + 0.2f;
		particle->m_bInsideSmoke = true;
		particle->m_vColor = Vector(75, 75, 75);
	}
	else
	{
		float spin = gEngfuncs.pfnRandomFloat(-1.7f, 1.7f);
		particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
		particle->m_flActivateTime = gEngfuncs.GetClientTime() + 0.05f;
		particle->m_bInsideSmoke = false;
		float color = gEngfuncs.pfnRandomFloat(75.5f, 100.5f);
		particle->m_vColor = Vector(color, color, color);
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 6.0f;
	}
	particle->m_flMass = gEngfuncs.pfnRandomFloat(5.0f, 10.0f);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
}

void CreateHEGrenadeFlash(Vector origin)
{
	const char *spriteName = "sprites/zerog-frag1.spr";
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(spriteName)));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: %s\n", spriteName);
		return;
	}
	CreateHEGrenadeFlashGlow(origin, 1);
	CreateHEGrenadeFlashGlow(origin, 1);
	CreateHEGrenadeFlashGlow(origin, 3);
	gEngfuncs.GetLocalPlayer();
	CBaseParticle *particle = new (std::nothrow) CBaseParticle;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 0), sprite, 175.0f, 234.0f);
	particle->SetCollisionFlags(TRI_ANIMATEDIE);
	particle->m_iRendermode = kRenderTransAdd;
	particle->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->m_vAngles.z = 55.0f;
	particle->m_flFadeSpeed = 0.0f;
	particle->m_iFrame = 0;
	particle->m_vColor = Vector(255, 225, 225);
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 10.1f;
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
	particle->m_iFramerate = 90;
	particle->m_iNumFrames = sprite->numframes;
}

void CreateHEGrenadeFlashGlow(Vector origin, int flare)
{
	char spriteName[64];
	sprintf(spriteName, "sprites/flare%d.spr", flare);
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(spriteName)));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: %s\n", spriteName);
		return;
	}
	gEngfuncs.GetLocalPlayer();
	CBaseParticle *particle = new (std::nothrow) CBaseParticle;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 0), sprite, flare == 1 ? 75.0f : 135.0f, 255.0f);
	particle->m_iRendermode = kRenderTransAdd;
	particle->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	if (flare == 1)
		particle->m_flScaleSpeed = 35.0f;
	else
	{
		particle->m_vAngles.z = 90.0f;
		particle->m_flScaleSpeed = 55.0f;
	}
	particle->m_flFadeSpeed = 0.0f;
	particle->m_iFrame = 0;
	particle->m_vColor = Vector(255, 225, 175);
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 0.15f;
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
}

void CTriangleDust::Think(float time)
{
	if (m_flActivateTime != 0.0f && time >= m_flActivateTime)
	{
		m_flActivateTime = 0.0f;
		m_flGravity = -0.035f;
		VectorScale(m_vVelocity, 0.05f, m_vVelocity);
	}
	if (m_vAVelocity.z != 0.0f)
	{
		m_vAVelocity.z = (float)(m_vAVelocity.z - 0.01);
		if (m_vAVelocity.z < 0)
			m_vAVelocity.z = 0;
	}
	CBaseParticle::Think(time);
}

void CHEGrenadeSmoke::Think(float time)
{
	if (m_flBrightness >= 105.0f && m_flDieTime - time < 3.0f)
	{
		m_flScaleSpeed = 0.0f;
		m_vAVelocity = Vector(0, 0, 0);
	}
	CBaseParticle::Think(time);
	if (m_flActivateTime != 0.0f && time >= m_flActivateTime && !m_bInsideSmoke)
	{
		m_flActivateTime = 0.0f;
		m_vVelocity = m_vVelocity * 0.2f;
	}
}

void CHEGrenadeSmoke::Touch(Vector, Vector normal, int)
{
	float speed = m_vVelocity.Length();
	Vector direction = speed == 0.0f ? Vector(0, 0, 0) : m_vVelocity.Normalize();
	Vector tangent = CrossProduct(CrossProduct(direction, normal), normal);
	m_vVelocity = tangent.Normalize() * (min(speed, 300.0f) * 1.5f);
}
