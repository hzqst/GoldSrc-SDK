//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "hud.h"
#include "cl_util.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "vgui_CTFViewport.h"
#include "com_model.h"
#include "entity_types.h"
#include "studio_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern "C" int iJumpSpectator;
extern "C" float vJumpOrigin[3], vJumpAngles[3];
extern "C" int iHasNewViewOrigin;
extern "C" float vecNewViewOrigin[3], vecNewViewAngles[3];
extern void V_GetInEyePos(int entity, float* origin, float* angles);

void SpectatorMode()
{
	if (!gEngfuncs.IsSpectateOnly())
	{
		gEngfuncs.Con_Printf("specmode only in HLTV mode available.\n");
		return;
	}
	if (gEngfuncs.Cmd_Argc() < 2)
	{
		gEngfuncs.Con_Printf("usage:  specmode < modenumber >\n");
		return;
	}
	gHUD.m_Spectator.SetModes(atoi(gEngfuncs.Cmd_Argv(1)), 0);
}

int CHudSpectator::Init()
{
	gHUD.AddHudElem(this);
	m_iFlags |= HUD_ACTIVE;
	m_flNextObserverInput = 0;
	m_iObserverTarget = 0;
	m_zoomDelta = m_moveDelta = 0;
	memset(&m_OverviewData, 0, sizeof(m_OverviewData));
	memset(m_OverviewEntities, 0, sizeof(m_OverviewEntities));
	m_iMainMode = m_iInsetMode = 0;
	m_drawnames = CVAR_CREATE("cl_drawnames", "1", FCVAR_ARCHIVE);
	gEngfuncs.pfnAddCommand("specmode", SpectatorMode);
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Loads new icons
//-----------------------------------------------------------------------------
int CHudSpectator::VidInit()
{
	m_hsprPlayer = SPR_Load("sprites/iplayer.spr");
	m_hsprPlayerBlue = SPR_Load("sprites/iplayerblue.spr");
	m_hsprPlayerRed = SPR_Load("sprites/iplayerred.spr");
	m_hsprPlayerDead = SPR_Load("sprites/iplayerdead.spr");
	m_hsprUnkownMap = SPR_Load("sprites/tile.spr");
	m_hsprBeam = SPR_Load("sprites/laserbeam.spr");
	m_hsprCamera = SPR_Load("sprites/camera.spr");
	m_hCrosshair = SPR_Load("sprites/crosshairs.spr");
	return 1;
}

int CHudSpectator::Draw(float)
{
	if (gEngfuncs.IsSpectateOnly() != 1)
		return 1;
	if (m_zoomDelta != 0 && m_iMainMode != 3)
		m_mapZoom = max(0.5f, min(3.0f, m_mapZoom + m_zoomDelta));
	if (m_moveDelta != 0)
	{
		if (m_iMainMode == 3)
			return 1;
		vec3_t right;
		gEngfuncs.pfnAngleVectors(m_mapAngles, NULL, right, NULL);
		VectorNormalize(right);
		VectorMA(m_mapOrigin, m_moveDelta, right, m_mapOrigin);
	}
	if (m_iMainMode == 5 && m_drawnames->value)
	{
		gViewPort->GetAllPlayersInfo();


		// loop through all the players and draw additional infos to their sprites on the map
		for (int i = 0; i < MAX_PLAYERS; ++i)
		{
			if (m_vPlayerPos[i][2] < 0)	// marked as invisible ?
				continue;
			float x = m_vPlayerPos[i][0], y = m_vPlayerPos[i][1];
			if (m_iInsetMode && x > m_OverviewData.insetWindowX * ScreenWidth / 640.0f &&
				y > m_OverviewData.insetWindowY * ScreenHeight / 480.0f &&
				x < (m_OverviewData.insetWindowX + m_OverviewData.insetWindowWidth) * ScreenWidth / 640.0f &&
				y < (m_OverviewData.insetWindowY + m_OverviewData.insetWindowHeight) * ScreenHeight / 480.0f)
				continue;
			float red = 0.8f, green = 0.8f, blue = 0.8f;
			if (g_PlayerExtraInfo[i + 1].teamnumber == 1) { red = 1; green = 0.4f; blue = 0.4f; }
			else if (g_PlayerExtraInfo[i + 1].teamnumber == 2) { red = 0.4f; green = 0.4f; blue = 1; }
			char text[256];
			strcpy(text, g_PlayerInfoList[i + 1].name);
			gEngfuncs.pfnDrawSetTextColor(red, green, blue);
			gEngfuncs.pfnDrawConsoleString((int)(x - 3 * strlen(text)), (int)y, text);
		}
	}
	return 1;
}

void UTIL_StringToVector(float* vector, const char* string)
{
	char text[128];
	strcpy(text, string);
	char* position = text;
	int i;
	for (i = 0; i < 3; ++i)
	{
		vector[i] = (float)atof(position);
		while (*position && *position != ' ')
			++position;
		if (!*position)
			break;
		++position;
	}
	while (++i < 3)
		vector[i] = 0;
}

void CHudSpectator::SetSpectatorStartPosition()
{
	vec3_t position = {0, 0, 0};
	cl_entity_t* world = gEngfuncs.GetEntityByIndex(0);
	if (world)
	{
		char token[1024], key[256];
		char* data = world->model->entities;
		while (data)
		{
			data = gEngfuncs.COM_ParseFile(data, token);
			if (!*data || token[0] == '}')
				break;
			if (token[0] != '{')
				return;
			while (true)
			{
				data = gEngfuncs.COM_ParseFile(data, token);
				if (token[0] == '}')
					break;
				if (!data)
					return;
				strcpy(key, token);
				int length = (int)strlen(key);
				while (length && key[length - 1] == ' ')
					key[--length] = 0;
				data = gEngfuncs.COM_ParseFile(data, token);
				if (!data || token[0] == '}')
					return;
				if (!strcmp(key, "classname") && !strcmp(token, "info_player_start"))
					break;
				if (!strcmp(key, "origin"))
					UTIL_StringToVector(position, token);
			}
		}
	}
	VectorCopy(position, vJumpOrigin);
	VectorCopy(position, vecNewViewOrigin);
	iHasNewViewOrigin = iJumpSpectator = 1;
}

void CHudSpectator::FindNextPlayer(bool reverse)
{
	int start = m_iObserverTarget ? m_iObserverTarget : 1;
	int index = start;
	m_iObserverTarget = 0;

	// make sure we have player info
	gViewPort->GetAllPlayersInfo();
	do
	{
		index += reverse ? -1 : 1;
		if (index > MAX_PLAYERS) index = 1;
		else if (index <= 0) index = MAX_PLAYERS;
		if (IsActivePlayer(gEngfuncs.GetEntityByIndex(index)))
		{
			m_iObserverTarget = index;
			break;
		}
	} while (index != start);
	if (!m_iObserverTarget)
	{
		gEngfuncs.Con_Printf("No observer targets.\n");
		return;
	}
	if (m_iMainMode != 3)
	{
		g_iUser2 = m_iObserverTarget;
		g_iUser3 = 0;
	}
	gEngfuncs.Con_Printf("Now Tracking %s\n", g_PlayerInfoList[m_iObserverTarget].name);
}

void CHudSpectator::HandleButtonsDown(int)
{
}

void CHudSpectator::HandleButtonsUp(int)
{
}

void CHudSpectator::SetModes(int mainMode, int insetMode)
{
	if (mainMode < 1 || mainMode > 5)
	{
		gEngfuncs.Con_Printf("Invalid spectator mode.\n");
		return;
	}
	char text[32];
	if (m_iInsetMode != insetMode)
	{
		m_iInsetMode = insetMode;
		sprintf(text, "%c#Spec_Mode_Inset%d", 4, insetMode);
		gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, (int)strlen(text) + 1, text);
	}
	if (m_iMainMode != mainMode)
	{
		if (mainMode == 3)
		{
			g_iUser1 = 3;
			if (m_iMainMode == 2)
			{
				VectorCopy(vecNewViewOrigin, vJumpOrigin);
				VectorCopy(vecNewViewAngles, vJumpAngles);
				gEngfuncs.SetViewAngles(vJumpAngles);
				iJumpSpectator = 1;
			}
			if (m_iMainMode == 4)
			{
				V_GetInEyePos(m_iObserverTarget, vJumpOrigin, vJumpAngles);
				gEngfuncs.SetViewAngles(vJumpAngles);
				iJumpSpectator = 1;
			}
			g_iUser2 = m_iObserverTarget = g_iUser3 = 0;
		}
		else if (mainMode == 2 || mainMode == 4)
			g_iUser1 = 3;
		else if (mainMode == 5)
		{
			g_iUser1 = 3;
			m_mapZoom = m_OverviewData.zoom;
			m_mapOrigin = m_OverviewData.origin;
		}
		m_iMainMode = mainMode;
		sprintf(text, "%c#Spec_Mode%d", 4, mainMode);
		gHUD.m_TextMessage.MsgFunc_TextMsg(NULL, (int)strlen(text) + 1, text);
		if (g_iUser1 == 3 && m_iObserverTarget == 0 &&
			IsActivePlayer(gEngfuncs.GetEntityByIndex(m_lastPrimaryObject)))
		{
			g_iUser2 = m_iObserverTarget = m_lastPrimaryObject;
			g_iUser3 = m_lastSecondaryObject;
		}
		static wrect_t crosshairRect;
		if (mainMode == 3 || mainMode == 4)
		{
			crosshairRect.left = 24;
			crosshairRect.right = 48;
			crosshairRect.top = 0;
			crosshairRect.bottom = 24;
			SetCrosshair(m_hCrosshair, crosshairRect, 255, 255, 255);
		}
		else
		{
			memset(&crosshairRect, 0, sizeof(crosshairRect));
			SetCrosshair(0, crosshairRect, 0, 0, 0);
		}
	}
	if (m_iMainMode != 5 && (m_iInsetMode == 1 || m_iInsetMode == 2))
		SetModes(m_iMainMode, 3);
	if (m_iMainMode == 5 && m_iInsetMode == 3)
		SetModes(5, 0);
	gViewPort->UpdateSpectatorMenu();
}

void CHudSpectator::DirectorEvent(unsigned char command, unsigned, unsigned, unsigned)
{
	if (command)
	{
		gEngfuncs.Con_DPrintf("CHudSpectator::DirectorEvent: unknown director command.\n");
		return;
	}
	g_iPlayerClass = g_iTeamNumber = iJumpSpectator = 0;
	m_lastSecondaryObject = m_lastPrimaryObject = 0;
	m_iInsetMode = m_iMainMode = m_iObserverTarget = 0;
	m_flNextObserverInput = 0;
	memset(m_OverviewEntities, 0, sizeof(m_OverviewEntities));
	SetModes(2, 0);
	ParseOverviewFile();
	LoadMapSprites();
	SetSpectatorStartPosition();
}

void CHudSpectator::GetMapPosition(float* position)
{
	vec3_t forward;
	gEngfuncs.pfnAngleVectors(m_mapAngles, forward, NULL, NULL);
	VectorNormalize(forward);
	VectorScale(forward, -(4096.0f / m_mapZoom), forward);
	position[0] = m_mapOrigin[0] + forward[0];
	position[1] = m_mapOrigin[1] + forward[1];
	position[2] = (90.0f - m_mapAngles[0]) / 90.0f * m_mapOrigin[2] + forward[2];
}

bool CHudSpectator::IsActivePlayer(cl_entity_t * ent)
{
	return ( ent && 
			 ent->player && 
			 ent->curstate.solid != SOLID_NOT &&
			 ent != gEngfuncs.GetLocalPlayer() &&
			 g_PlayerInfoList[ent->index].name != NULL
			);
}


bool CHudSpectator::ParseOverviewFile( )
{
	char filename[255];
	char levelname[255];
	char token[1024];
	float height;
	
	char *pfile  = NULL;

	memset( &m_OverviewData, 0, sizeof(m_OverviewData));

	// fill in standrd values
	m_OverviewData.insetWindowX = 4;	// upper left corner
	m_OverviewData.insetWindowY = 4;
	m_OverviewData.insetWindowHeight = 180;
	m_OverviewData.insetWindowWidth = 240;
	m_OverviewData.origin[0] = 0.0f;
	m_OverviewData.origin[1] = 0.0f;
	m_OverviewData.origin[2] = 0.0f;
	m_OverviewData.zoom	= 1.0f;
	m_OverviewData.layers = 0;
	m_OverviewData.layersHeights[0] = 0.0f;
	strcpy(levelname, gEngfuncs.pfnGetLevelName() + 5);
	levelname[strlen(levelname)-4] = 0;
	
	sprintf(filename, "overviews/%s.txt", levelname );

	pfile = (char *)gEngfuncs.COM_LoadFile( filename, 5, NULL);

	if (!pfile)
	{
		gEngfuncs.Con_DPrintf("Couldn't open file %s. Using default values for overiew mode.\n", filename );
		return false;
	}
	
	
	while (true)
	{
		pfile = gEngfuncs.COM_ParseFile(pfile, token);

		if (!pfile)
			break;

		if ( !stricmp( token, "global" ) )
		{
			// parse the global data
			pfile = gEngfuncs.COM_ParseFile(pfile, token);
			if ( stricmp( token, "{" ) ) 
			{
				gEngfuncs.Con_Printf("Error parsing overview file %s. (expected { )\n", filename );
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile(pfile,token);

			while (stricmp( token, "}") )
			{
				if ( !stricmp( token, "zoom" ) )
				{
					pfile = gEngfuncs.COM_ParseFile(pfile,token);
					m_OverviewData.zoom = atof( token );
				} 
				else if ( !stricmp( token, "origin" ) )
				{
					pfile = gEngfuncs.COM_ParseFile(pfile, token); 
					m_OverviewData.origin[0] = atof( token );
					pfile = gEngfuncs.COM_ParseFile(pfile,token); 
					m_OverviewData.origin[1] = atof( token );
					pfile = gEngfuncs.COM_ParseFile(pfile, token); 
					m_OverviewData.origin[2] = atof( token );
				}
				else if ( !stricmp( token, "rotated" ) )
				{
					pfile = gEngfuncs.COM_ParseFile(pfile,token); 
					m_OverviewData.rotated = atoi( token );
				}
				else if ( !stricmp( token, "inset" ) )
				{
					pfile = gEngfuncs.COM_ParseFile(pfile,token); 
					m_OverviewData.insetWindowX = atof( token );
					pfile = gEngfuncs.COM_ParseFile(pfile,token); 
					m_OverviewData.insetWindowY = atof( token );
					pfile = gEngfuncs.COM_ParseFile(pfile,token); 
					m_OverviewData.insetWindowWidth = atof( token );
					pfile = gEngfuncs.COM_ParseFile(pfile,token); 
					m_OverviewData.insetWindowHeight = atof( token );

				}
				else
				{
					gEngfuncs.Con_Printf("Error parsing overview file %s. (%s unkown)\n", filename, token );
					return false;
				}

				pfile = gEngfuncs.COM_ParseFile(pfile,token); // parse next token

			}
		}
		else if ( !stricmp( token, "layer" ) )
		{
			// parse a layer data

			if ( m_OverviewData.layers == OVERVIEW_MAX_LAYERS )
			{
				gEngfuncs.Con_Printf("Error parsing overview file %s. ( too many layers )\n", filename );
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile(pfile,token);

				
			if ( stricmp( token, "{" ) ) 
			{
				gEngfuncs.Con_Printf("Error parsing overview file %s. (expected { )\n", filename );
				return false;
			}

			pfile = gEngfuncs.COM_ParseFile(pfile,token);

			while (stricmp( token, "}") )
			{
				if ( !stricmp( token, "image" ) )
				{
					pfile = gEngfuncs.COM_ParseFile(pfile,token);
					strcpy(m_OverviewData.layersImages[ m_OverviewData.layers ], token);
					
					
				} 
				else if ( !stricmp( token, "height" ) )
				{
					pfile = gEngfuncs.COM_ParseFile(pfile,token); 
					height = atof(token);
					m_OverviewData.layersHeights[ m_OverviewData.layers ] = height;
				}
				else
				{
					gEngfuncs.Con_Printf("Error parsing overview file %s. (%s unkown)\n", filename, token );
					return false;
				}

				pfile = gEngfuncs.COM_ParseFile(pfile,token); // parse next token
			}

			m_OverviewData.layers++;

		}
	}


	m_mapZoom = m_OverviewData.zoom;
	m_mapOrigin = m_OverviewData.origin;

	return true;

}

void CHudSpectator::LoadMapSprites()
{
	// right now only support for one map layer
	if (m_OverviewData.layers > 0 )
	{
		m_MapSprite = gEngfuncs.LoadMapSprite( m_OverviewData.layersImages[0] );
	}
	else
		m_MapSprite = NULL; // the standard "unkown map" sprite will be used instead
}

void CHudSpectator::DrawOverviewLayer()
{
	float screenaspect, xs, ys, xStep, yStep, x,y,z;
	int ix,iy,i,xTiles,yTiles,frame;

	qboolean	hasMapImage = m_MapSprite?TRUE:FALSE;
	model_t *   dummySprite = (struct model_s *)gEngfuncs.GetSpritePointer( m_hsprUnkownMap);

	if ( hasMapImage)
	{
		i = m_MapSprite->numframes / (4*3);
		i = sqrt((float)i);
		xTiles = i*4;
		yTiles = i*3;
	}
	else
	{
		xTiles = 8;
		yTiles = 6;
	}


	screenaspect = 4.0f/3.0f;	


	xs = m_OverviewData.origin[0];
	ys = m_OverviewData.origin[1];
	z  = ( 90.0f - m_mapAngles[0] ) / 90.0f;
	z *= m_OverviewData.layersHeights[0]; // gOverviewData.z_min - 32;	

	// i = r_overviewTexture + ( layer*OVERVIEW_X_TILES*OVERVIEW_Y_TILES );

	gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );
	gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, 1.0 );

	frame = 0;
	

	// rotated view ?
	if ( m_OverviewData.rotated )
	{
		xStep = (2*4096.0f / m_OverviewData.zoom ) / xTiles;
		yStep = -(2*4096.0f / (m_OverviewData.zoom* screenaspect) ) / yTiles;

		y = ys + (4096.0f / (m_OverviewData.zoom * screenaspect));

		for (iy = 0; iy < yTiles; iy++)
		{
			x = xs - (4096.0f / (m_OverviewData.zoom));

			for (ix = 0; ix < xTiles; ix++)
			{
				if (hasMapImage)
					gEngfuncs.pTriAPI->SpriteTexture( m_MapSprite, frame );
				else
					gEngfuncs.pTriAPI->SpriteTexture( dummySprite, 0 );

				gEngfuncs.pTriAPI->Begin( TRI_QUADS );
					gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
					gEngfuncs.pTriAPI->Vertex3f (x, y, z);

					gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
					gEngfuncs.pTriAPI->Vertex3f (x+xStep ,y,  z);

					gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
					gEngfuncs.pTriAPI->Vertex3f (x+xStep, y+yStep, z);

					gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
					gEngfuncs.pTriAPI->Vertex3f (x, y+yStep, z);
				gEngfuncs.pTriAPI->End();

				frame++;
				x+= xStep;
			}

			y+=yStep;
		}
	} 
	else
	{
		xStep = -(2*4096.0f / m_OverviewData.zoom ) / xTiles;
		yStep = -(2*4096.0f / (m_OverviewData.zoom* screenaspect) ) / yTiles;

				
		x = xs + (4096.0f / (m_OverviewData.zoom * screenaspect ));

		
		
		for (ix = 0; ix < yTiles; ix++)
		{
			
			y = ys + (4096.0f / (m_OverviewData.zoom));	
						
			for (iy = 0; iy < xTiles; iy++)	
			{
				if (hasMapImage)
					gEngfuncs.pTriAPI->SpriteTexture( m_MapSprite, frame );
				else
					gEngfuncs.pTriAPI->SpriteTexture( dummySprite, 0 );

				gEngfuncs.pTriAPI->Begin( TRI_QUADS );
					gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
					gEngfuncs.pTriAPI->Vertex3f (x, y, z);

					gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
					gEngfuncs.pTriAPI->Vertex3f (x+xStep ,y,  z);

					gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
					gEngfuncs.pTriAPI->Vertex3f (x+xStep, y+yStep, z);

					gEngfuncs.pTriAPI->TexCoord2f( 1, 0 );
					gEngfuncs.pTriAPI->Vertex3f (x, y+yStep, z);
				gEngfuncs.pTriAPI->End();

				frame++;
				
				y+=yStep;
			}

			x+= xStep;
			
		}
	}
}

void CHudSpectator::DrawOverviewEntities()
{
	int				i,ir,ig,ib;
	struct model_s *hSpriteModel;
	vec3_t			origin, angles, point, forward, right, left, up, world, screen, offset;
	float			x,y,z, r,g,b, sizeScale = 4.0f;
	cl_entity_t *	ent;
	float rmatrix[3][4];	// transformation matrix
	
	float			zScale = (90.0f - m_mapAngles[0] ) / 90.0f;
	

	z = m_OverviewData.layersHeights[0] * zScale;
	// get yellow/brown HUD color
	UnpackRGB(ir,ig,ib, RGB_YELLOWISH);
	r = (float)ir/255.0f;
	g = (float)ig/255.0f;
	b = (float)ib/255.0f;
	
	gEngfuncs.pTriAPI->CullFace( TRI_NONE );

	for (i=0; i < MAX_PLAYERS; i++ )
		m_vPlayerPos[i][2] = -1;	// mark as invisible 

	// draw all players
	for (i=0 ; i < MAX_OVERVIEW_ENTITIES ; i++)
	{
		if ( !m_OverviewEntities[i].hSprite )
			continue;

		hSpriteModel = (struct model_s *)gEngfuncs.GetSpritePointer( m_OverviewEntities[i].hSprite );
		ent = m_OverviewEntities[i].entity;
		
		gEngfuncs.pTriAPI->SpriteTexture( hSpriteModel, 0 );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransTexture );

		// see R_DrawSpriteModel
		// draws players sprite

		AngleVectors(ent->angles, right, up, NULL );

		VectorCopy(ent->origin,origin);

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );

		gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, 1.0 );
		
		gEngfuncs.pTriAPI->TexCoord2f (1, 0);
		VectorMA (origin,  16.0f * sizeScale, up, point);
		VectorMA (point,   16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv (point);

		gEngfuncs.pTriAPI->TexCoord2f (0, 0);
		
		VectorMA (origin,  16.0f * sizeScale, up, point);
		VectorMA (point,  -16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv (point);

		gEngfuncs.pTriAPI->TexCoord2f (0,1);
		VectorMA (origin, -16.0f * sizeScale, up, point);
		VectorMA (point,  -16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv (point);

		gEngfuncs.pTriAPI->TexCoord2f (1,1);
		VectorMA (origin, -16.0f * sizeScale, up, point);
		VectorMA (point,   16.0f * sizeScale, right, point);
		point[2] *= zScale;
		gEngfuncs.pTriAPI->Vertex3fv (point);

		gEngfuncs.pTriAPI->End ();

		
		if ( !ent->player)
			continue;
		// draw line under player icons
		origin[2] *= zScale;

		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		
		hSpriteModel = (struct model_s *)gEngfuncs.GetSpritePointer( m_hsprBeam );
		gEngfuncs.pTriAPI->SpriteTexture( hSpriteModel, 0 );
		
		gEngfuncs.pTriAPI->Color4f(r, g, b, 0.3);

		gEngfuncs.pTriAPI->Begin ( TRI_QUADS );
		gEngfuncs.pTriAPI->TexCoord2f (1, 0);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]+4, origin[1]+4, origin[2]-zScale);
		gEngfuncs.pTriAPI->TexCoord2f (0, 0);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]-4, origin[1]-4, origin[2]-zScale);
		gEngfuncs.pTriAPI->TexCoord2f (0, 1);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]-4, origin[1]-4,z);
		gEngfuncs.pTriAPI->TexCoord2f (1, 1);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]+4, origin[1]+4,z);
		gEngfuncs.pTriAPI->End ();

		gEngfuncs.pTriAPI->Begin ( TRI_QUADS );
		gEngfuncs.pTriAPI->TexCoord2f (1, 0);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]-4, origin[1]+4, origin[2]-zScale);
		gEngfuncs.pTriAPI->TexCoord2f (0, 0);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]+4, origin[1]-4, origin[2]-zScale);
		gEngfuncs.pTriAPI->TexCoord2f (0, 1);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]+4, origin[1]-4,z);
		gEngfuncs.pTriAPI->TexCoord2f (1, 1);
		gEngfuncs.pTriAPI->Vertex3f (origin[0]-4, origin[1]+4,z);
		gEngfuncs.pTriAPI->End ();

		// calculate screen position for name and infromation in hud::draw()
		if ( gEngfuncs.pTriAPI->WorldToScreen(origin,screen) )
			continue;	// object is behind viewer

		screen[0] = XPROJECT(screen[0]);
		screen[1] = YPROJECT(screen[1]);
		screen[2] = 0.0f;

		// calculate some offset under the icon
		origin[0]+=32.0f;
		origin[1]+=32.0f;
		
		gEngfuncs.pTriAPI->WorldToScreen(origin,offset);

		offset[0] = XPROJECT(offset[0]);
		offset[1] = YPROJECT(offset[1]);
		offset[2] = 0.0f;
			
		VectorSubtract(offset, screen, offset );

		int playerNum = ent->index - 1;

		m_vPlayerPos[playerNum][0] = screen[0];	
		m_vPlayerPos[playerNum][1] = screen[1] + Length(offset);	
		m_vPlayerPos[playerNum][2] = 1;	// mark player as visible 
	}

	if ( !m_iInsetMode )
		return;

	// get current camera position and angle

	if ( m_iInsetMode == 2 || m_iMainMode == 4 )
	{ 
		V_GetInEyePos( m_iObserverTarget, vecNewViewOrigin, vecNewViewAngles );
	}
	VectorCopy(vecNewViewOrigin, origin);
	VectorCopy(vecNewViewAngles, angles);

	
	// draw camera sprite

	x = origin[0];
	y = origin[1];
	z = origin[2];


	hSpriteModel = (struct model_s *)gEngfuncs.GetSpritePointer( m_hsprCamera );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->SpriteTexture( hSpriteModel, 0 );
	
	
	gEngfuncs.pTriAPI->Color4f( r, g, b, 1.0 );

	AngleVectors(angles, forward, NULL, NULL );
	VectorScale (forward, 512.0f, forward);
	
	offset[0] =  0.0f; 
	offset[1] = 45.0f; 
	offset[2] =  0.0f; 

	AngleMatrix(offset, rmatrix );
	VectorTransform(forward, rmatrix , right );

	offset[1]= -45.0f;
	AngleMatrix(offset, rmatrix );
	VectorTransform(forward, rmatrix , left );

	gEngfuncs.pTriAPI->Begin (TRI_TRIANGLES);
		gEngfuncs.pTriAPI->TexCoord2f( 0, 0 );
		gEngfuncs.pTriAPI->Vertex3f (x+right[0], y+right[1], (z+right[2]) * zScale);

		gEngfuncs.pTriAPI->TexCoord2f( 0, 1 );
		gEngfuncs.pTriAPI->Vertex3f (x, y, z  * zScale);
		
		gEngfuncs.pTriAPI->TexCoord2f( 1, 1 );
		gEngfuncs.pTriAPI->Vertex3f (x+left[0], y+left[1], (z+left[2]) * zScale);
	gEngfuncs.pTriAPI->End ();

}



void CHudSpectator::DrawOverview()
{
	// draw only in sepctator mode
	if ( gEngfuncs.IsSpectateOnly() != 1 )
		return;

	// Only draw the overview if Map Mode is selected for this view
	if ( m_iDrawCycle == 0 && m_iMainMode != 5 )
		return;

	if ( m_iDrawCycle == 1 && m_iInsetMode != 3 )
		return;

	DrawOverviewLayer();
	DrawOverviewEntities();
	CheckOverviewEntities();
}
void CHudSpectator::CheckOverviewEntities()
{
	double time = gEngfuncs.GetClientTime();

	// removes old entities from list
	for ( int i = 0; i< MAX_OVERVIEW_ENTITIES; i++ )
	{
		// remove entity from list if it is too old
		if ( m_OverviewEntities[i].killTime < time )
		{
			memset( &m_OverviewEntities[i], 0, sizeof (overviewEntity_t) );
		}
	}
}

bool CHudSpectator::AddOverviewEntity( int type, struct cl_entity_s *ent, const char *)
{
	HSPRITE	hSprite = 0;
	double  duration = -1.0f;	// duration -1 means show it only this frame;

	if ( !ent )
		return false;

	if ( type == ET_PLAYER )
	{
		if ( ent->curstate.solid != SOLID_NOT)
		{
			switch ( g_PlayerExtraInfo[ent->index].teamnumber )
			{
				// blue and red teams are swapped in CS and TFC
				case 1 : hSprite = m_hsprPlayerBlue; break;
				case 2 : hSprite = m_hsprPlayerRed; break;
				default : hSprite = m_hsprPlayer; break;
			}
		}
		else
			return false;	// it's an spectator
	}
	else if (type == ET_NORMAL)
	{
		return false;
	}
	else
		return false;	

	return AddOverviewEntityToList(hSprite, ent, gEngfuncs.GetClientTime() + duration );
}

void CHudSpectator::DeathMessage(int victim)
{
	// find out where the victim is
	cl_entity_t *pl = gEngfuncs.GetEntityByIndex(victim);

	if (pl && pl->player)
		AddOverviewEntityToList(m_hsprPlayerDead, pl, gEngfuncs.GetClientTime() + 2.0f );
}

bool CHudSpectator::AddOverviewEntityToList(HSPRITE sprite, cl_entity_t *ent, double killTime)
{
	for ( int i = 0; i< MAX_OVERVIEW_ENTITIES; i++ )
	{
		// find empty entity slot
		if ( m_OverviewEntities[i].entity == NULL)
		{
			m_OverviewEntities[i].entity = ent;
			m_OverviewEntities[i].hSprite = sprite;
			m_OverviewEntities[i].killTime = killTime;
			return true;
		}
	}

	return false;	// maximum overview entities reached
}
