#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include "cl_entity.h"
#include "event_api.h"
#include "pmtrace.h"
#include "../../pm_shared/pm_defs.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "parsemsg.h"
#include "z_weather.h"
#include "effects/e_effectclasses.h"
#include "materials/z_materials.h"

extern engine_studio_api_t IEngineStudio;
extern Vector g_vPlayerVelocity;
extern Vector v_origin;
extern Vector v_angles;

CEnvironment g_Environment;
cvar_t *cl_weather;

int WaterEntryPoint(pmtrace_t *trace, float *source, float *result)
{
	if (gEngfuncs.PM_PointContents(trace->endpos, NULL) == gEngfuncs.PM_PointContents(source, NULL))
		return 0;
	Vector start(source), end(trace->endpos);
	Vector direction = end - start;
	while (direction.Length() > 4.0f)
	{
		Vector middle = start + direction * 0.5f;
		if (gEngfuncs.PM_PointContents(start, NULL) == gEngfuncs.PM_PointContents(middle, NULL))
			start = middle;
		else
			end = middle;
		direction = end - start;
	}
	start.CopyToArray(result);
	return 1;
}

float AngleMod(float angle)
{
	if (angle < 0.0f)
	{
		float turns = angle / 360.0f;
		return angle + 360.0f * (static_cast<int>(turns) + 1);
	}
	if (angle >= 360.0f)
	{
		float turns = angle / 360.0f;
		return angle - 360.0f * static_cast<int>(turns);
	}
	return angle;
}

float VecToYaw(Vector direction)
{
	if (direction.x == 0.0f && direction.y == 0.0f)
		return 0.0f;
	float yaw = static_cast<float>(static_cast<int>(atan2(direction.y, direction.x) * 180.0 / 3.1415));
	if (yaw < 0.0f)
		yaw += 360.0f;
	return yaw;
}

int CHudHealth::MsgFunc_ReceiveW(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	g_Environment.SetWeatherType(READ_BYTE());
	return 1;
}

int CHudHealth::MsgFunc_WindInfo(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	float minX = READ_COORD();
	float maxX = READ_COORD();
	float minY = READ_COORD();
	float maxY = READ_COORD();
	float minTime = READ_COORD();
	float maxTime = READ_COORD();
	g_Environment.SetWindDirectionConstraints(minX, minY, maxX, maxY);
	g_Environment.SetWindChangeTimeConstraints(minTime, maxTime);
	return 1;
}

int CHudHealth::MsgFunc_GrassInfo(const char *, int size, void *buffer)
{
	BEGIN_READ(buffer, size);
	const char *sprite = READ_STRING();
	float fadeDistance = READ_COORD();
	float minHeight = READ_COORD();
	float maxHeight = READ_COORD();
	int density = READ_LONG();
	g_Environment.SetGrassConstraints(sprite, fadeDistance, minHeight, maxHeight, density);
	return 1;
}

void CEnvironment::SetWindDirectionConstraints(float minX, float minY, float maxX, float maxY)
{
	m_fWindDirectionMinX = minX;
	m_fWindDirectionMaxX = maxX;
	m_fWindDirectionMinY = minY;
	m_fWindDirectionMaxY = maxY;
}

void CEnvironment::SetWindChangeTimeConstraints(float minimum, float maximum)
{
	m_fWindChangeTimeMin = minimum;
	m_fWindChangeTimeMax = maximum;
}

void CEnvironment::SetGrassConstraints(const char *sprite, float fadeDistance, float minHeight, float maxHeight, int density)
{
	strncpy(m_szGrassSpriteName, sprite, sizeof(m_szGrassSpriteName) - 1);
	m_fGrassFadeDist = fadeDistance;
	m_fGrassMinHeight = minHeight;
	m_fGrassMaxHeight = maxHeight;
	m_iGrassDensity = density;
	if (m_szGrassSpriteName[0])
	{
		m_pGrassSprite2 = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(m_szGrassSpriteName)));
		m_pGrassSprite1 = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load(m_szGrassSpriteName)));
	}
	else
	{
		m_pGrassSprite2 = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/grass_03.spr")));
		m_pGrassSprite1 = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/grass_01.spr")));
	}
	if (m_fGrassMinHeight > m_fGrassMaxHeight)
		m_fGrassMaxHeight = m_fGrassMinHeight;
}

void CEnvironment::SetupGrass()
{
	for (int i = 0; i < 512; ++i)
	{
		materials_t *material = g_pMaterialSystem.GetMaterialByIndex(i);
		if (material && material->m_chMaterialType == 'A')
		{
			m_bGrassActive = true;
			m_bPlaceInitialGrass = true;
			return;
		}
	}
}

void CEnvironment::Initialize()
{
	m_iWeatherType = 0;
	m_grassCount = 0;
	m_vWeatherOrigin = Vector(0, 0, 0);
	m_flWeatherTime = 0.0f;
	m_bGrassActive = false;
	m_bPlaceInitialGrass = true;
	m_vGrassOrigin = Vector(0, 0, 0);
	m_flGrassTime = 0.0f;
	m_fWindDirectionMinX = -80.0f;
	m_fWindDirectionMinY = -80.0f;
	m_fWindDirectionMaxX = 80.0f;
	m_fWindDirectionMaxY = 80.0f;
	m_fWindChangeTimeMin = 15.0f;
	m_fWindChangeTimeMax = 30.0f;
	m_flWeatherValue = cl_weather->value;
	SetupGrass();
	float windY = gEngfuncs.pfnRandomFloat(m_fWindDirectionMinY, m_fWindDirectionMaxY);
	float windX = gEngfuncs.pfnRandomFloat(m_fWindDirectionMinX, m_fWindDirectionMaxX);
	m_vWind = Vector(windX, windY, 0.0f);
	windX = gEngfuncs.pfnRandomFloat(m_fWindDirectionMinX, m_fWindDirectionMaxX);
	windY = gEngfuncs.pfnRandomFloat(m_fWindDirectionMinY, m_fWindDirectionMaxY);
	m_vDesiredWindDirection = Vector(windX, windY, 0.0f);
	m_flNextWindChangeTime = gEngfuncs.GetClientTime();
	m_pRainSprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/rain.spr")));
	m_pSnowSprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/snowflake.spr")));
	m_pRipple = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/ripple.spr")));
	m_pRainSplash = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/wsplash3.spr")));
}

void CEnvironment::UpdateWind()
{
	float time = gEngfuncs.GetClientTime();
	if (!(gEngfuncs.GetClientTime() < m_flNextWindChangeTime))
	{
		float windX = gEngfuncs.pfnRandomFloat(m_fWindDirectionMinX, m_fWindDirectionMaxX);
		float windY = gEngfuncs.pfnRandomFloat(m_fWindDirectionMinY, m_fWindDirectionMaxY);
		m_vDesiredWindDirection = Vector(windX, windY, 0.0f);
		float changeTime = gEngfuncs.GetClientTime();
		m_flNextWindChangeTime = changeTime + gEngfuncs.pfnRandomFloat(m_fWindChangeTimeMin, m_fWindChangeTimeMax);
		m_flDesiredWindSpeed = m_vDesiredWindDirection.Length();
		m_flIdealYaw = VecToYaw(m_vDesiredWindDirection.Normalize());
	}
	Vector direction = m_vWind.Normalize();
	Vector angles;
	VectorAngles(direction, angles);
	float yaw = AngleMod(angles.y);
	if (m_flIdealYaw != yaw)
	{
		float speed = (time - m_flOldTime) * 0.5f * 10.0f;
		float move = m_flIdealYaw - yaw;
		if (m_flIdealYaw > yaw)
		{
			if (move >= 180.0f)
				move -= 360.0f;
		}
		else if (move <= -180.0f)
			move += 360.0f;
		if (move > 0.0f)
			move = min(move, speed);
		else
			move = max(move, -speed);
		angles.y = AngleMod(yaw + move);
	}
	gEngfuncs.pfnAngleVectors(angles, direction, NULL, NULL);
	m_vWind = direction * m_flDesiredWindSpeed;
}

void CEnvironment::Update()
{
	m_vWeatherOrigin = gHUD.m_vecOrigin;
	gEngfuncs.pfnAngleVectors(v_angles, m_cameraForward, NULL, NULL);
	m_cameraForward = m_cameraForward.Normalize();
	if (cl_weather->value > 3.0f)
		gEngfuncs.Cvar_SetValue("cl_weather", 3.0f);
	m_flWeatherValue = cl_weather->value;
	if (!IEngineStudio.IsHardware())
		m_flWeatherValue = 0.0f;
	UpdateWind();
	if (m_bGrassActive)
		UpdateGrass();
	if (!(m_flWeatherTime > gEngfuncs.GetClientTime()))
	{
		if (m_iWeatherType == 2)
			UpdateSnow();
		if (m_iWeatherType == 1)
			UpdateRain();
	}
	m_flOldTime = gEngfuncs.GetClientTime();
}

void CEnvironment::UpdateGrass()
{
	if (!(m_flGrassTime <= gEngfuncs.GetClientTime()))
		return;
	if (!((gEngfuncs.GetLocalPlayer()->origin - m_vGrassOrigin).Length() >= 50.0f))
		return;
	Vector oldOrigin = m_vGrassOrigin;
	m_vGrassOrigin = gEngfuncs.GetLocalPlayer()->origin;
	m_flGrassTime = gEngfuncs.GetClientTime() + 0.3f;
	if (m_bPlaceInitialGrass)
	{
		PlaceGrassSquare(m_fGrassFadeDist + 50.0f, static_cast<int>(m_iGrassDensity * 1.2732395f));
		m_bPlaceInitialGrass = false;
		m_iGrassDensity = static_cast<int>((m_fGrassFadeDist * 100.0f + 2500.0f) / (m_fGrassFadeDist * m_fGrassFadeDist) * m_iGrassDensity);
	}
	else
		PlaceGrass(m_fGrassFadeDist, m_fGrassFadeDist + 50.0f, m_iGrassDensity, oldOrigin);
}

void CEnvironment::PlaceGrass(float innerRadius, float outerRadius, int quantity, const Vector &oldOrigin)
{
	for (int i = 0; i < quantity; ++i)
	{
		float radius = gEngfuncs.pfnRandomFloat(innerRadius, outerRadius);
		float angle = gEngfuncs.pfnRandomFloat(0.0f, 359.0f);
		float radians = angle * 3.1415927f / 180.0f;
		Vector origin(static_cast<float>(m_vGrassOrigin.x + cos(static_cast<double>(radians)) * radius),
			static_cast<float>(m_vGrassOrigin.y + sin(static_cast<double>(radians)) * radius), m_vGrassOrigin.z + 36.0f);
		if (!m_bPlaceInitialGrass && (oldOrigin - origin).Length() < m_fGrassFadeDist + 50.0f)
			continue;
		Vector end(origin.x, origin.y, -800.0f);
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, end, PM_WORLD_ONLY, -1, &trace);
		if (trace.startsolid || trace.allsolid)
			continue;
		float yaw = gEngfuncs.pfnRandomFloat(0.0f, 359.0f);
		int materialIndex = g_pMaterialSystem.GetMaterialIndexAtPos(&trace, trace.endpos, end);
		materials_t *material = g_pMaterialSystem.GetMaterialByIndex(materialIndex);
		if (material && material->m_chMaterialType == 'A')
		{
			float scale = gEngfuncs.pfnRandomFloat(m_fGrassMinHeight, m_fGrassMaxHeight);
			CreateGrassPiece(trace.endpos, Vector(0.0f, yaw, 0.0f), scale);
		}
	}
}

void CEnvironment::PlaceGrassSquare(float radius, int quantity)
{
	for (int i = 0; i < quantity; ++i)
	{
		Vector origin = m_vGrassOrigin;
		origin.x += gEngfuncs.pfnRandomFloat(-radius, radius);
		origin.y += gEngfuncs.pfnRandomFloat(-radius, radius);
		origin.z += 36.0f;
		Vector end(origin.x, origin.y, -800.0f);
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, end, PM_WORLD_ONLY, -1, &trace);
		float yaw = gEngfuncs.pfnRandomFloat(0.0f, 359.0f);
		int materialIndex = g_pMaterialSystem.GetMaterialIndexAtPos(&trace, trace.endpos, end);
		materials_t *material = g_pMaterialSystem.GetMaterialByIndex(materialIndex);
		if (material && material->m_chMaterialType == 'A')
		{
			float scale = gEngfuncs.pfnRandomFloat(m_fGrassMinHeight, m_fGrassMaxHeight);
			CreateGrassPiece(trace.endpos, Vector(0.0f, yaw, 0.0f), scale);
		}
	}
}

void CEnvironment::UpdateRain()
{
	float jitter = gEngfuncs.pfnRandomFloat(-0.1f, 0.1f);
	m_flWeatherTime = static_cast<float>(gEngfuncs.GetClientTime() + 0.3 + jitter);
	Vector movement = g_vPlayerVelocity.Normalize() * g_vPlayerVelocity.Length();
	int windParticle = 0;
	for (int i = 0; i < 150.0f * m_flWeatherValue; ++i)
	{
		float distance = gEngfuncs.pfnRandomFloat(0.0f, 400.0f);
		int side = gEngfuncs.pfnRandomLong(0, 5);
		Vector origin = m_vWeatherOrigin;
		origin.x += side > 2 ? distance : -distance;
		origin.y += gEngfuncs.pfnRandomFloat(-400.0f, 400.0f);
		origin.z += gEngfuncs.pfnRandomFloat(100.0f, 300.0f);
		origin = origin + movement;
		Vector end(origin.x, origin.y, 8000.0f);
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, end, PM_WORLD_ONLY, -1, &trace);
		const char *texture = gEngfuncs.pEventAPI->EV_TraceTexture(trace.ent, trace.endpos, end);
		if (texture && !strncmp(texture, "sky", 3))
		{
			CreateRaindrop(origin);
			if (windParticle == 15)
			{
				Vector windOrigin(origin.x, origin.y, gHUD.m_vecOrigin.z);
				if (gEngfuncs.pTriAPI->BoxInPVS(windOrigin, windOrigin))
				{
					end.z = 8000.0f;
					gEngfuncs.pEventAPI->EV_SetTraceHull(2);
					gEngfuncs.pEventAPI->EV_PlayerTrace(windOrigin, end, PM_WORLD_ONLY, -1, &trace);
					texture = gEngfuncs.pEventAPI->EV_TraceTexture(trace.ent, trace.endpos, end);
					if (texture && !strncmp(texture, "sky", 3))
					{
						end.z = -8000.0f;
						gEngfuncs.pEventAPI->EV_SetTraceHull(2);
						gEngfuncs.pEventAPI->EV_PlayerTrace(windOrigin, end, PM_WORLD_ONLY, -1, &trace);
						CreateWindParticle(trace.endpos);
					}
				}
				windParticle = 1;
			}
			else
				++windParticle;
		}
	}
}

void CEnvironment::UpdateSnow()
{
	m_flWeatherTime = static_cast<float>(gEngfuncs.GetClientTime() + 0.7);
	gEngfuncs.GetLocalPlayer();
	Vector movement = g_vPlayerVelocity.Normalize() * g_vPlayerVelocity.Length();
	for (int i = 0; i < 150.0f * m_flWeatherValue; ++i)
	{
		float distance = gEngfuncs.pfnRandomFloat(0.0f, 300.0f);
		int side = gEngfuncs.pfnRandomLong(0, 5);
		Vector origin = m_vWeatherOrigin;
		origin.x += side > 2 ? distance : -distance;
		origin.y += gEngfuncs.pfnRandomFloat(-300.0f, 300.0f);
		origin.z += gEngfuncs.pfnRandomFloat(100.0f, 300.0f);
		origin = origin + movement;
		Vector end(origin.x, origin.y, 8000.0f);
		pmtrace_t trace;
		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, end, PM_WORLD_ONLY, -1, &trace);
		const char *texture = gEngfuncs.pEventAPI->EV_TraceTexture(trace.ent, trace.endpos, end);
		if (texture && !strncmp(texture, "sky", 3))
			CreateSnowFlake(origin);
	}
}

void CEnvironment::CreateRaindrop(float *origin)
{
	if (!g_Environment.GetRainSprite())
		return;
	CRainDrop *particle = new (std::nothrow) CRainDrop;
	if (!particle)
		return;
	float time = gEngfuncs.GetClientTime();
	particle->InitializeSprite(Vector(origin), Vector(0, 0, 0), g_Environment.GetRainSprite(), 2.0f, 1.0f);
	strcpy(particle->m_szClassname, "particle_rain");
	particle->m_flStretchY = 40.0f;
	particle->m_flDieTime = time + 1.0f;
	particle->m_vVelocity.x = m_vWind.x;
	particle->m_vVelocity.y = m_vWind.y;
	particle->m_vVelocity.z = gEngfuncs.pfnRandomFloat(-500.0f, -800.0f);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD | TRI_ANIMATEDIE | TRI_WATERTRACE);
	particle->m_flGravity = 0.0f;
	particle->SetCullFlag(CULL_FRUSTUM_PLANE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->m_iRendermode = kRenderTransAlpha;
	particle->m_vColor = Vector(255, 255, 255);
	particle->m_bTouched = false;
}

void CEnvironment::CreateSnowFlake(float *origin)
{
	if (!g_Environment.GetSnowSprite())
		return;
	CSnowFlake *particle = new (std::nothrow) CSnowFlake;
	if (!particle)
		return;
	float size = gEngfuncs.pfnRandomFloat(2.0f, 2.5f);
	particle->InitializeSprite(Vector(origin), Vector(0, 0, 0), g_Environment.GetSnowSprite(), size, 1.0f);
	particle->m_iNumFrames = g_Environment.GetSnowSprite()->numframes;
	strcpy(particle->m_szClassname, "snow_particle");
	particle->m_vVelocity = Vector(m_vWind.x * 0.5f, m_vWind.y * 0.5f, -100.0f);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	float speed = gEngfuncs.pfnRandomFloat(0.0f, 1.0f);
	if (speed < 0.1f)
	{
		particle->m_vVelocity.x *= 0.5f;
		particle->m_vVelocity.y *= 0.5f;
	}
	else if (speed < 0.2f)
		particle->m_vVelocity.z = -65.0f;
	else if (speed < 0.3f)
		particle->m_vVelocity.z = -75.0f;
	particle->m_iRendermode = kRenderTransAdd;
	particle->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER);
	particle->m_flScaleSpeed = 0.0f;
	particle->m_flDampingTime = 0.0f;
	particle->m_iFrame = 0;
	particle->m_flMass = 1.0f;
	particle->m_flGravity = 0.0f;
	particle->m_flBounceFactor = 0.0f;
	particle->m_vColor = Vector(128, 128, 128);
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 3.0f;
	particle->m_bTouched = false;
	particle->m_bSpiral = gEngfuncs.pfnRandomLong(0, 1) != 0;
	float time = gEngfuncs.GetClientTime();
	particle->m_flSpiralTime = static_cast<float>(gEngfuncs.pfnRandomLong(2, 4)) + time;
}

model_s *CEnvironment::GetGrassSprites()
{
	return gEngfuncs.pfnRandomLong(0, 1) ? m_pGrassSprite1 : m_pGrassSprite2;
}

void CEnvironment::CreateGrassPiece(float *origin, Vector normal, float scale)
{
	if (!g_pParticleMan)
		return;
	model_s *sprite = GetGrassSprites();
	if (!sprite)
		sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/grass_03.spr")));
	if (!sprite)
		return;
	CGrassPiece *particle = new (std::nothrow) CGrassPiece;
	if (!particle)
		return;
	origin[2] += 10.0f;
	particle->InitializeSprite(Vector(origin), normal, sprite, scale, 1.0f);
	particle->m_iAfterDampFlags = TRI_COLLIDEWORLD;
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->m_flGravity = 0.0f;
	particle->m_iRendermode = kRenderTransAlpha;
	particle->SetCullFlag(CULL_FRUSTUM_PLANE | CULL_PVS);
	particle->SetLightFlag(LIGHT_COLOR);
	particle->m_vVelocity = Vector(0, 0, 0);
	particle->m_vAVelocity = Vector(0, 0, 0);
	particle->m_vAngles = normal;
	particle->m_flSize = scale;
	particle->m_flFadeSpeed = 0.0f;
	particle->m_vColor = Vector(125, 125, 125);
	particle->m_flFadeSpeed = -1.0f;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 99999.0f;
}

void CEnvironment::CreateWindParticle(float *origin)
{
	model_s *sprite = const_cast<model_s *>(gEngfuncs.GetSpritePointer(gEngfuncs.pfnSPR_Load("sprites/gas_puff_01.spr")));
	if (!sprite)
		return;
	CPartWind *particle = new (std::nothrow) CPartWind;
	if (!particle)
		return;
	origin[2] += 10.0f;
	float size = gEngfuncs.pfnRandomFloat(50.0f, 75.0f);
	particle->InitializeSprite(Vector(origin), Vector(0, 0, 0), sprite, size, 1.0f);
	strcpy(particle->m_szClassname, "wind_particle");
	particle->m_iNumFrames = sprite->numframes;
	particle->m_iFrame = gEngfuncs.pfnRandomLong(sprite->numframes / 2, sprite->numframes);
	particle->m_vVelocity.x = m_vWind.x / gEngfuncs.pfnRandomFloat(1.0f, 2.0f);
	particle->m_vVelocity.y = m_vWind.y / gEngfuncs.pfnRandomFloat(1.0f, 2.0f);
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->m_flGravity = 0.0f;
	if (gEngfuncs.pfnRandomFloat(0.0f, 1.0f) < 0.1f)
	{
		particle->m_vVelocity.x *= 0.5f;
		particle->m_vVelocity.y *= 0.5f;
	}
	particle->SetCollisionFlags(TRI_COLLIDEWORLD);
	particle->m_iRendermode = kRenderTransAlpha;
	particle->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
	particle->SetLightFlag(LIGHT_NONE);
	particle->SetRenderFlag(RENDER_FACEPLAYER_ROTATEZ);
	particle->m_vAVelocity.z = gEngfuncs.pfnRandomFloat(-1.0f, 1.0f);
	particle->m_flScaleSpeed = 0.4f;
	particle->m_flDampingTime = 0.0f;
	particle->m_iFrame = 0;
	particle->m_flMass = 1.0f;
	particle->m_flBounceFactor = 0.0f;
	particle->m_vColor = Vector(128, 128, 128);
	particle->m_flFadeSpeed = -1.0f;
	particle->m_flDieTime = gEngfuncs.GetClientTime() + 6.0f;
}

void CGrassPiece::Think(float time)
{
	Vector direction = m_vOrigin - v_origin;
	float distance = direction.Length();
	direction = direction.Normalize();
	float fadeDistance = g_Environment.GetGrassFadeDist();
	if (distance <= fadeDistance + 50.0f)
	{
		if (DotProduct(g_Environment.GetCameraForward(), direction) >= 0.5f)
		{
			if (distance <= fadeDistance - 255.0f)
				m_flBrightness = 255.0f;
			else
			{
				float brightness = 255.0f - (distance - (fadeDistance - 255.0f));
				m_flBrightness = brightness > 0.0f ? brightness : 1.0f;
			}
		}
		else
			m_flBrightness = 0.0f;
	}
	else
		m_flDieTime = time;
	if (time >= m_flDieTime)
		--g_Environment.m_grassCount;
	CBaseParticle::Think(time);
}

void CSnowFlake::Think(float time)
{
	float oldTime = g_Environment.GetOldTime();
	if (m_flBrightness < 130.0f && !m_bTouched)
		m_flBrightness += 4.5f;
	CBaseParticle::Fade(time);
	CBaseParticle::Spin(time);
	if (gEngfuncs.GetClientTime() >= m_flSpiralTime)
	{
		float currentTime = gEngfuncs.GetClientTime();
		int interval = gEngfuncs.pfnRandomLong(2, 4);
		m_bSpiral = !m_bSpiral;
		m_flSpiralTime = currentTime + interval;
	}
	if (m_bSpiral && !m_bTouched)
	{
		float delta = time - oldTime;
		double wave = sin(time * 5.0 + reinterpret_cast<int>(this));
		m_vOrigin.x += m_vVelocity.x * delta + static_cast<float>(wave * 2.0 * 0.3);
		m_vOrigin.y += m_vVelocity.y * delta;
		m_vOrigin.z += m_vVelocity.z * delta;
	}
	else
		CBaseParticle::CalculateVelocity(time);
	CBaseParticle::CheckCollision(time);
}

void CSnowFlake::Touch(Vector, Vector, int)
{
	if (!m_bTouched)
	{
		SetRenderFlag(RENDER_FACEPLAYER);
		m_flOriginalBrightness = m_flBrightness;
		m_bTouched = true;
		m_vVelocity = Vector(0, 0, 0);
		m_iRendermode = kRenderTransAdd;
		m_flFadeSpeed = 0.0f;
		m_flScaleSpeed = 0.0f;
		m_flDampingTime = 0.0f;
		m_iFrame = 0;
		m_flMass = 1.0f;
		m_flGravity = 0.0f;
		m_vColor = Vector(128, 128, 128);
		m_flDieTime = gEngfuncs.GetClientTime() + 0.5f;
		m_flTimeCreated = gEngfuncs.GetClientTime();
	}
}

void CRainDrop::Think(float time)
{
	Vector angles, forward, right, up;
	gEngfuncs.GetViewAngles(angles);
	gEngfuncs.pfnAngleVectors(angles, forward, right, up);
	m_vAngles.y = angles.y;
	m_vAngles.z = static_cast<float>(atan(DotProduct(m_vVelocity, right) / m_vVelocity.z) * 57.29746936176985);
	if (m_flBrightness < 155.0f)
		m_flBrightness = static_cast<float>(m_flBrightness + 6.5);
	CBaseParticle::Think(time);
}

void CRainDrop::Touch(Vector, Vector normal, int)
{
	if (m_bTouched)
		return;
	m_bTouched = true;
	Vector start = m_vOrigin + Vector(0, 0, 32);
	Vector end = m_vOrigin - Vector(0, 0, 16);
	pmtrace_t trace;
	gEngfuncs.pEventAPI->EV_PlayerTrace(start, end, PM_WORLD_ONLY, -1, &trace);
	Vector rippleAngles;
	Vector rippleNormal(normal.x, normal.y, -normal.z);
	VectorAngles(rippleNormal, rippleAngles);
	Vector point;
	if (WaterEntryPoint(&trace, start, point))
	{
		CBaseParticle *ripple = new (std::nothrow) CBaseParticle;
		if (ripple)
		{
			ripple->InitializeSprite(point, rippleAngles, g_Environment.GetRippleSprite(), 15.0f, 110.0f);
			ripple->m_iRendermode = kRenderTransAdd;
			ripple->m_flFadeSpeed = 2.0f;
			ripple->m_flScaleSpeed = 1.0f;
			ripple->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
			ripple->SetLightFlag(LIGHT_INTENSITY);
			ripple->m_vColor = Vector(255, 255, 255);
			ripple->m_flDieTime = gEngfuncs.GetClientTime() + 2.0f;
		}
	}
	float time = gEngfuncs.GetClientTime();
	CBaseParticle *splash = new (std::nothrow) CBaseParticle;
	if (splash)
	{
		float size = static_cast<float>(gEngfuncs.pfnRandomLong(20, 25));
		splash->InitializeSprite(m_vOrigin + normal * 5.0f, Vector(90, 0, 0), g_Environment.GetSplashSprite(), size, 125.0f);
		splash->m_flGravity = 0.1f;
		splash->m_iRendermode = kRenderTransAdd;
		splash->m_flMass = 1.0f;
		splash->SetCullFlag(CULL_FRUSTUM_SPHERE | CULL_PVS);
		splash->SetLightFlag(LIGHT_INTENSITY);
		splash->m_vColor = Vector(255, 255, 255);
		splash->m_iNumFrames = g_Environment.GetSplashSprite()->numframes - 1;
		splash->m_iFramerate = gEngfuncs.pfnRandomLong(30, 45);
		splash->m_flDieTime = time + 0.3f;
		splash->SetCollisionFlags(TRI_ANIMATEDIE);
		splash->SetRenderFlag(RENDER_FACEPLAYER);
	}
}

void CPartWind::Think(float time)
{
	if (m_flDieTime - time <= 3.0f)
	{
		if (m_flBrightness > 0.0f)
			m_flBrightness = static_cast<float>(m_flBrightness - (time - m_flTimeCreated) * 0.4);
		if (m_flBrightness < 0.0f)
		{
			m_flBrightness = 0.0f;
			m_flDieTime = gEngfuncs.GetClientTime();
		}
	}
	else if (m_flBrightness < 100.0f)
		m_flBrightness += (time - m_flTimeCreated) * 5.0f + 3.0f;
	CBaseParticle::Think(time);
}

void CPartWind::Touch(Vector, Vector normal, int)
{
	float speed = m_vVelocity.Length();
	Vector direction = speed == 0.0f ? Vector(0, 0, 0) : m_vVelocity.Normalize();
	Vector tangent = CrossProduct(CrossProduct(direction, normal), normal);
	m_vVelocity = tangent.Normalize() * (min(speed, 300.0f) * 1.5f);
}
