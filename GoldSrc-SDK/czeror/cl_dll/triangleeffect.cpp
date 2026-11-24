#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "triangleeffect.h"
#include "effects/e_effectclasses.h"

double g_dFrameRate;
float g_flOldFPSTime;

void CreateSpark(Vector origin, Vector normal, const char *spriteName)
{
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(spriteName)));
	if (!sprite)
	{
		gEngfuncs.Con_DPrintf("Couldn't load Sprite: %s\n", spriteName);
		return;
	}
	Vector angles, forward, right, up;
	VectorAngles(normal, angles);
	gEngfuncs.pfnAngleVectors(angles, forward, right, up);
	float speed = gEngfuncs.pfnRandomFloat(20.0f, 80.0f);
	float upSpeed = gEngfuncs.pfnRandomFloat(-speed, speed);
	float rightSpeed = gEngfuncs.pfnRandomFloat(-speed, speed);
	float forwardSpeed = gEngfuncs.pfnRandomFloat(speed * 0.5f, speed);
	Vector velocity = up * upSpeed + right * rightSpeed + forward * forwardSpeed;
	CBaseParticle *particle = new (std::nothrow) CBaseParticle;
	if (!particle)
		return;
	particle->InitializeSprite(origin, Vector(0, 0, 1), sprite, 0.25f, 255.0f);
	particle->m_iAfterDampFlags = TRI_COLLIDEWORLD;
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->m_vAngles = angles;
	particle->m_vVelocity = velocity;
	particle->m_flGravity = 0.4f;
	particle->m_flDampingVelocity = 0.5f;
	particle->m_flAfterDampGrav = 0.7f;
	particle->m_iRendermode = kRenderTransAdd;
	particle->m_flBrightness = 255.0f;
	particle->m_vAVelocity = Vector(0, 0, 0);
	particle->m_vColor = Vector(255, 255, 255);
	particle->m_iFrame = 0;
	particle->m_flFadeSpeed = 5.0f;
	particle->m_flSize = 1.5f;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 0.2f;
}

void CreateParticles(Vector origin, Vector direction, int count, float speed, int noise, int renderMode,
	float gravity, float scale, float scaleSpeed, float brightness, float fadeSpeed, float life,
	int type, int texture, int flags, Vector angles, Vector angularVelocity, Vector color)
{
	int collisionFlags = 0;
	if (flags & 1)
		collisionFlags |= TRI_COLLIDEKILL;
	if (flags & 2)
		collisionFlags |= TRI_COLLIDEWORLD;
	if (flags & 4)
		collisionFlags |= TRI_COLLIDEALL;
	if (flags & 64)
		collisionFlags |= TRI_SPIRAL;
	if (flags & 8)
		collisionFlags |= TRI_ANIMATEDIE;
	if (flags & 128)
	{
		const int extendedParticleFlags = 0x128000;
		collisionFlags |= extendedParticleFlags;
	}
	float variation = noise / 10.0f;
	float verticalVariation = min(variation * 1.5f, 1.0f);
	model_s *sprite = gEngfuncs.hudGetModelByIndex(texture);
	float time = gEngfuncs.GetClientTime();
	float particleScale = scale / 10.0f;
	for (int i = 0; i < count; ++i)
	{
		Vector velocity;
		velocity.x = direction.x + gEngfuncs.pfnRandomFloat(-variation, variation);
		velocity.y = direction.y + gEngfuncs.pfnRandomFloat(-variation, variation);
		velocity.z = direction.z + gEngfuncs.pfnRandomFloat(0.0f, verticalVariation);
		float particleSpeed = gEngfuncs.pfnRandomFloat(speed * 2.0f * 0.8f, speed * 2.0f * 1.2f);
		velocity = velocity * particleSpeed;
		CBaseParticle *particle;
		switch (type)
		{
		case PARTICLE_SPRITE:
			particle = g_pParticleMan->CreateParticle(origin, Vector(0, 0, 0), sprite, particleScale, brightness, "CreateParticles");
			if (particle)
				particle->SetRenderFlag(RENDER_FACEPLAYER);
			break;
		case PARTICLE_PLANE:
		case PARTICLE_CYLINDER:
		case PARTICLE_DISK:
		case PARTICLE_CONE:
			particle = g_pParticleMan->CreateParticle(origin, angles, sprite, particleScale, brightness, "CreateParticles");
			break;
		case PARTICLE_SPHERE:
		case PARTICLE_WATERDROP:
			particle = g_pParticleMan->CreateParticle(origin, Vector(0, 0, 0), sprite, particleScale, brightness, "CreateParticles");
			break;
		default:
			return;
		}
		if (!particle)
			return;
		particle->m_flGravity = gravity;
		particle->m_flSize = particleScale;
		particle->m_flDieTime = time + life;
		particle->m_iRendermode = renderMode;
		particle->m_vVelocity = velocity;
		particle->SetCollisionFlags(collisionFlags);
		particle->m_flScaleSpeed = scaleSpeed / 10.0f;
		particle->m_flBrightness = brightness;
		particle->m_flFadeSpeed = fadeSpeed;
		particle->m_vAngles = angles;
		particle->m_vAVelocity = angularVelocity;
		particle->m_vColor = color;
		if (flags & 16)
			particle->m_iFrame = gEngfuncs.pfnRandomLong(0, sprite->numframes - 1);
		else if (sprite->numframes > 1)
		{
			particle->m_iFramerate = 15;
			particle->m_iNumFrames = sprite->numframes;
		}
	}
}
