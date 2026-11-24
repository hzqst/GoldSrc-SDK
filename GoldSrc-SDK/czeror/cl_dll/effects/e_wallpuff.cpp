#include "hud.h"
#include "cl_util.h"
#include "e_effectclasses.h"

void CreateDebrisWallPuff(Vector origin, Vector velocity, Vector color, int puff, bool blood)
{
	const char *spriteName = blood || puff == 0 ? "sprites/large_smoke_01_ind.spr" : "sprites/puff.spr";
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(spriteName)));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: sprites/large_smoke_01_ind.spr\n");
		return;
	}
	float scale = gEngfuncs.pfnRandomFloat(15.0f, 20.0f);
	if (puff > 0)
		scale = gEngfuncs.pfnRandomFloat(static_cast<float>(5 * puff), static_cast<float>(10 * puff));
	float brightness = blood ? 215.0f : (puff != 0 ? 100.0f : 255.0f);
	CTriangleWallPuff *particle = new (std::nothrow) CTriangleWallPuff;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, scale, brightness);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->m_flSize = scale;
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
	particle->m_flGravity = -0.1f;
	particle->m_iRendermode = kRenderTransAlpha;
	float spin = gEngfuncs.pfnRandomFloat(-1.5f, 1.5f);
	particle->m_vAVelocity = Vector(0.0f, 0.0f, spin);
	particle->m_flActivateTime = gEngfuncs.GetClientTime() + 0.1f;
	particle->m_flFadeSpeed = 7.5f;
	particle->m_flMass = gEngfuncs.pfnRandomFloat(2.0f, 3.0f);
	if (puff > 0)
		particle->m_vVelocity = velocity * static_cast<float>(30 * puff);
	else
		particle->m_vVelocity = velocity * 95.0f;
	if (puff <= 0 || blood)
	{
		particle->m_flScaleSpeed = 0.9f;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 3.0f;
	}
	else
	{
		particle->m_iNumFrames = 9;
		particle->m_iFramerate = 15;
		particle->m_flScaleSpeed = 0.5f;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 0.7f;
	}
	particle->m_vColor = blood ? Vector(80, 15, 0) : color;
}

void CTriangleWallPuff::Think(float time)
{
	if (m_iFrame <= 7)
		Animate(time);
	Contract(time);
	Expand(time);
	Fade(time);
	Spin(time);
	CheckCollision(time);
	CalculateVelocity(time);
	if (m_flActivateTime != 0.0f && time >= m_flActivateTime)
	{
		m_flActivateTime = 0.0f;
		m_flGravity = -0.02f;
		m_vAVelocity = Vector(0.0f, 0.0f, 0.4f);
		m_vVelocity = m_vVelocity * 0.1f;
	}
}
