#include "hud.h"
#include "cl_util.h"
#include "r_efx.h"
#include "e_effectclasses.h"

extern int blowTorchFiring;
extern Vector g_vBlowTorchAttachment;

void CreatePilotFlame(Vector origin)
{
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/flare1.spr")));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: sprites/adrian/flare1.spr\n");
		return;
	}
	CPartBlowTorchPilot *particle = new (std::nothrow) CPartBlowTorchPilot;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, blowTorchFiring == 2 ? 1.1f : 1.0f, 255.0f);
	particle->m_flGravity = -0.2f;
	particle->m_iRendermode = kRenderTransAdd;
	particle->m_flFadeSpeed = 0.0f;
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
	particle->m_flScaleSpeed = -0.4f;
	particle->m_vColor = Vector(150, 150, 255);
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 0.06f;
	if (blowTorchFiring == 2)
	{
		particle->m_flScaleSpeed = -0.1f;
		particle->m_flDieTime = gEngfuncs.GetClientTime() + 0.12f;
		particle->m_vColor = Vector(255, 175, 175);
	}
	particle->m_flForward = 0.0f;
}

void CreateBlowTorchGlow()
{
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/flare1.spr")));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: sprites/adrian/flare1.spr\n");
		return;
	}
	CPartBlowTorchGlow *particle = new (std::nothrow) CPartBlowTorchGlow;
	if (!particle)
		return;
	float size = gEngfuncs.pfnRandomFloat(4.0f, 5.0f);
	if (blowTorchFiring == 2)
		size = gEngfuncs.pfnRandomFloat(11.0f, 12.0f);
	particle->InitializeSprite(g_vBlowTorchAttachment, Vector(0, 0, 1), sprite, size, 220.0f);
	particle->m_flGravity = -0.2f;
	particle->m_iRendermode = kRenderTransAdd;
	particle->m_flFadeSpeed = 0.0f;
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
	particle->m_flMass = gEngfuncs.pfnRandomFloat(2.0f, 3.0f);
	particle->m_flScaleSpeed = 0.05f;
	particle->m_vColor = Vector(25, 15, 15);
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 0.08f;
}

void UpdateBlowTorchEffect()
{
	static float flOldTime;
	float flTimeDelta = gEngfuncs.GetClientTime() - flOldTime;
	if (gEngfuncs.GetClientTime() != flOldTime && blowTorchFiring > 0)
	{
		CreateBlowTorchGlow();
		dlight_t *light = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
		light->origin = g_vBlowTorchAttachment;
		if (blowTorchFiring == 2)
		{
			light->radius = 75.0f;
			light->decay = 300.0f;
			light->color.r = 255;
		}
		else
		{
			light->radius = 50.0f;
			light->decay = 25.0f;
			light->color.r = 225;
		}
		light->color.g = 190;
		light->color.b = 40;
		light->die = gEngfuncs.GetClientTime() + flTimeDelta * 2;
		CreatePilotFlame(g_vBlowTorchAttachment);
		flOldTime = gEngfuncs.GetClientTime();
	}
}

void SetBlowTorchAttachment(cl_entity_t *entity)
{
	if (blowTorchFiring > 0)
	{
		cl_entity_t *viewModel = gEngfuncs.GetViewModel();
		if (viewModel == entity)
			g_vBlowTorchAttachment = viewModel->attachment[0];
	}
}

void CPartBlowTorchGlow::Think(float time)
{
	m_vOrigin = g_vBlowTorchAttachment;
	CBaseParticle::Think(time);
}

void CPartBlowTorchPilot::Think(float time)
{
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors(gHUD.m_vecAngles, forward, right, up);
	if (blowTorchFiring == 2)
	{
		float vertical = gEngfuncs.pfnRandomFloat(-0.065f, 0.065f);
		float horizontal = gEngfuncs.pfnRandomFloat(-0.08f, 0.08f);
		m_vOrigin = forward * m_flForward - right * horizontal - up * vertical + g_vBlowTorchAttachment;
	}
	else
	{
		float vertical = gEngfuncs.pfnRandomFloat(-0.05f, 0.05f);
		float horizontal = gEngfuncs.pfnRandomFloat(-0.05f, 0.05f);
		m_vOrigin = forward * (m_flForward / 1.5f) - right * horizontal + up * vertical + g_vBlowTorchAttachment;
	}
	m_flForward += 0.8f;
	CBaseParticle::Think(time);
	if (blowTorchFiring == 0)
		m_flDieTime = time;
}
