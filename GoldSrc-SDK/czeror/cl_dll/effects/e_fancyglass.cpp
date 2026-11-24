#include "hud.h"
#include "cl_util.h"
#include "e_effectclasses.h"

GlassHandle g_GlassPanes[40];

void CreateTriangle(Vector position, Vector normal, float width, float height, int index)
{
	gEngfuncs.GetClientTime();
	Vector angles;
	VectorAngles(normal, angles);
	Vector forward, right, up;
	gEngfuncs.pfnAngleVectors(angles, forward, right, up);
	gEngfuncs.pfnRandomLong(0, static_cast<int>(width));
	gEngfuncs.pfnRandomLong(0, static_cast<int>(height));
	g_GlassPanes[index].ClearAll();

	for (int y = 0; y < height; y += 10)
	{
		for (int x = 0; x < width; x += 10)
		{
			Vector origin = position + up * static_cast<float>(y) + right * static_cast<float>(x);
			model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/glass_01.spr")));
			CTriGlass *particle = new (std::nothrow) CTriGlass;
			if (!particle)
				return;
			particle->InitializeSprite(origin, normal, sprite, 20.0f, 220.0f);
			particle->m_iYDecay = 0;
			particle->m_iRendermode = kRenderTransAdd;
			particle->m_iYpos = y;
			particle->m_iXDecay = 0;
			particle->m_flActivatePhysicsTime = 0.0f;
			particle->m_iXpos = x;
			particle->m_iGlassWidth = static_cast<int>(width);
			particle->m_iGlassHeight = static_cast<int>(height);
			particle->m_flDieTime = gEngfuncs.GetClientTime() + 3.0f;
			particle->m_vColor = Vector(255.0f, 255.0f, 255.0f);
			particle->m_vAngles = angles;
			particle->m_flGravity = 0.0f;
			particle->m_vAngles.z = gEngfuncs.pfnRandomFloat(0.0f, 360.0f);
			particle->m_iFrame = gEngfuncs.pfnRandomLong(0, 2);
			particle->SetCollisionFlags(TRI_COLLIDEWORLD);
			particle->m_flFadeSpeed = 0.0f;
			particle->m_vVelocity = Vector(0, 0, 0);
			particle->m_flBounceFactor = 0.2f;
			particle->SetCullFlag(CULL_FRUSTUM_PLANE | CULL_PVS);
			particle->SetLightFlag(LIGHT_INTENSITY);
			particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
			particle->m_iUser1 = 0;
			particle->m_iUser2 = 0;
			g_GlassPanes[index].particles[x / 10][y / 10] = particle;
		}
	}
}

void DecayGlass(float xDecay, float yDecay, int size, int index, float xDecay2, float yDecay2, Vector direction)
{
	float time = gEngfuncs.GetClientTime();
	for (int y = 0; y < 40; ++y)
	{
		for (int x = 0; x < 40; ++x)
		{
			CTriGlass *particle = g_GlassPanes[index].particles[x][y];
			if (!particle)
				break;
			float xDistance = static_cast<float>(x * 10) - xDecay;
			float yDistance = static_cast<float>(y * 10) - yDecay;
			float distance = sqrtf(xDistance * xDistance + yDistance * yDistance);
			if (distance >= size)
			{
				particle->m_flActivatePhysicsTime = (20 - y) / 100.0f + time;
			}
			else
			{
				float xDistance2 = static_cast<float>(x * 10) - xDecay2;
				float yDistance2 = static_cast<float>(y * 10) - yDecay2;
				(void)sqrt(xDistance2 * xDistance2 + yDistance2 * yDistance2);
				particle->m_vVelocity = direction * -gEngfuncs.pfnRandomFloat(150.0f, 250.0f);
				particle->m_flGravity = 0.3f;
				particle->m_flSize = gEngfuncs.pfnRandomFloat(5.0f, 10.0f);
			}
		}
	}
}

void CTriGlass::Think(float time)
{
	CBaseParticle::Think(time);
	if (m_flActivatePhysicsTime != 0.0f && time > m_flActivatePhysicsTime)
	{
		Vector angles;
		VectorAngles(m_vAngles, angles);
		Vector forward, right, up;
		gEngfuncs.pfnAngleVectors(m_vAngles, forward, right, up);
		float speed = gEngfuncs.pfnRandomFloat(-50.0f, 50.0f);
		SetCollisionFlags(TRI_COLLIDEWORLD);
		m_flActivatePhysicsTime = 0.0f;
		m_vVelocity = forward * speed;
		m_flGravity = gEngfuncs.pfnRandomFloat(0.8f, 0.85f);
		float currentTime = gEngfuncs.GetClientTime();
		m_flDieTime = gEngfuncs.pfnRandomFloat(3.0f, 6.0f) + currentTime;
		m_flSize = gEngfuncs.pfnRandomFloat(m_flSize * 0.25f, m_flSize);
		m_vAVelocity.z = gEngfuncs.pfnRandomFloat(-1.5f, 1.5f);
		m_vAVelocity.y = gEngfuncs.pfnRandomFloat(-1.5f, 1.5f);
		m_vAVelocity.x = gEngfuncs.pfnRandomFloat(-1.5f, 1.5f);
	}
}

void CTriGlass::Touch(Vector, Vector, int)
{
	if (m_iUser1 != 1)
	{
		m_iUser1 = 1;
		float time = gEngfuncs.GetClientTime();
		m_flDieTime = gEngfuncs.pfnRandomFloat(0.1f, 0.5f) + time;
	}
}
