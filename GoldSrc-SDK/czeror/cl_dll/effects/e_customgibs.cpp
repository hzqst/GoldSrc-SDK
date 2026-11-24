#include "hud.h"
#include "cl_util.h"
#include "e_effectclasses.h"

void CreateCustomGib(Vector origin, Vector normal, const char *spriteName, float velocityModifier, materials_t *material)
{
	if (!material)
		return;
	char spritePath[64];
	const char *path = spriteName;
	if (!spriteName[0])
	{
		int number = gEngfuncs.pfnRandomLong(1, material->m_iDebriSpriteNum);
		sprintf(spritePath, "%s%d.spr", material->m_szTextName, number);
		path = spritePath;
	}
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(path)));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: %s\n", spriteName);
		return;
	}
	Vector angles, forward, right, up;
	VectorAngles(normal, angles);
	gEngfuncs.pfnAngleVectors(angles, forward, right, up);
	float scale = gEngfuncs.pfnRandomFloat(0.5f, 5.0f);
	float speed = gEngfuncs.pfnRandomFloat(50.0f, 200.0f) * velocityModifier * material->m_flVelMod;
	float dampingModifier = 1.0f;
	if (material->m_chMaterialType == 'X')
		dampingModifier = normal.z <= 0.0f ? 1.5f : 5.0f;
	float upSpeed = gEngfuncs.pfnRandomFloat(-speed, speed);
	float rightSpeed = gEngfuncs.pfnRandomFloat(-speed, speed);
	float forwardSpeed = gEngfuncs.pfnRandomFloat(speed * 0.5f, speed);
	Vector velocity = up * upSpeed + right * rightSpeed + forward * (forwardSpeed * dampingModifier);
	velocity.z = gEngfuncs.pfnRandomFloat(-speed / 5.0f, speed * 1.25f);
	CTriangleCustomGib *particle = new (std::nothrow) CTriangleCustomGib;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, scale, 255.0f);
	float spinZ = gEngfuncs.pfnRandomFloat(-5.0f, 5.0f);
	float spinY = gEngfuncs.pfnRandomFloat(-5.0f, 5.0f);
	float spinX = gEngfuncs.pfnRandomFloat(-5.0f, 5.0f);
	particle->m_flGravity = material->m_flGravity;
	particle->m_flDampingVelocity = material->m_flDampingVelocity;
	particle->m_flAfterDampGrav = material->m_flAfterDampGravity;
	particle->m_iRendermode = material->m_iRendermode;
	particle->SetCollisionFlags(material->m_iCollisionFlags);
	particle->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
	particle->SetLightFlag(LIGHT_INTENSITY);
	particle->m_iAfterDampFlags = material->m_iAfterDampFlags;
	particle->m_vVelocity = velocity;
	particle->m_vAVelocity = Vector(spinX, spinY, spinZ);
	particle->m_flBrightness = material->m_flBrightness;
	particle->m_vColor = material->m_vColor;
	particle->m_iFrame = material->m_iFrame;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + material->m_flLife;
	particle->m_flFadeSpeed = material->m_flFadeSpeed;
	if (material->m_flDampingTime != 0.0f)
		particle->m_flActivateTime = gEngfuncs.GetClientTime() + material->m_flDampingTime;
	particle->m_flSize = material->m_flSize != 0.0f ? material->m_flSize : scale;
}

void CTriangleCustomGib::Think(float time)
{
	CBaseParticle::Think(time);
	if (m_flActivateTime != 0.0f && time > m_flActivateTime)
	{
		SetCollisionFlags(m_iAfterDampFlags);
		m_flGravity = m_flAfterDampGrav;
		m_flActivateTime = 0.0f;
		m_vVelocity = m_vVelocity * m_flDampingVelocity;
	}
}

void CTriangleCustomGib::Touch(Vector, Vector normal, int)
{
	if (GetCollisionFlags() == 0)
	{
		if (normal.z > 0.5f)
		{
			m_vAVelocity = Vector(0, 0, 0);
			m_vVelocity = Vector(0, 0, 0);
			m_flGravity = 0.0f;
			m_flDieTime = gEngfuncs.GetClientTime() + 20.0f;
		}
		else
			m_flFadeSpeed = 6.0f;
	}
}
