//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef GAMEDATA_H
#define GAMEDATA_H
#pragma once

#include <fstream.h>
#include "HelperInfo.h"
#include "TokenReader.h"
#include "GamePalette.h"
#include "GDClass.h"
#include "IEditorTexture.h"

class MDkeyvalue;
class GameData;

#define MAX_DIRECTORY_SIZE	32

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class GDmain
{
	public:

		BOOL InitFromTokens(TokenReader& tr, GameData*);

		char name[MAX_STRING];
		char palettename[MAX_STRING];
		char defaultclass[MAX_STRING];

		GameData *Parent;
};

//-----------------------------------------------------------------------------
// Purpose: Contains the set of data that is loaded from a single FGD file.
//-----------------------------------------------------------------------------
class GameData
{
	public:

		GameData();
		~GameData();

		BOOL Load(LPCTSTR pszFilename);

		GDclass *ClassForName(LPCTSTR pszName, int *piIndex = NULL);

		void ClearData();

		CTypedPtrList<CPtrList,GDclass*> Classes;
};

enum MAPFORMAT
{
	mfQuake = 0,
	mfHexen2,
	mfQuake2,
	mfHalfLife,
};

class CGameConfig
{
	public:

		CGameConfig();

		inline TEXTUREFORMAT GetTextureFormat(void);
		inline void SetTextureFormat(TEXTUREFORMAT eFormat);




		DWORD dwID;	// assigned on load

		char szName[128];
		int nGDFiles;
		MAPFORMAT mapformat;

		char szExecutable[128];
		char szDefaultPoint[128];
		char szDefaultSolid[128];
		char m_szCSG[128];
		char szBSP[128];
		char szLIGHT[128];
		char szVIS[128];
		char m_szGameExeDir[128];
		char szMapDir[128];
		char szBSPDir[128];
		char m_szModDir[128];
		char m_szGameDir[128];

		CStringArray GDFiles;
		GameData GD;	// gamedata files loaded
		CGamePalette Palette;

		BOOL Import(fstream &, float fVersion);
		void Save(fstream &);
		void CopyFrom(CGameConfig *pConfig);
		void LoadGDFiles(void);

	protected:

		TEXTUREFORMAT textureformat;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
TEXTUREFORMAT CGameConfig::GetTextureFormat(void)
{
	return(textureformat);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGameConfig::SetTextureFormat(TEXTUREFORMAT eFormat)
{
	textureformat = eFormat;
}






//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------


extern GameData*	pGD;
extern CGameConfig *g_pGameConfig;
extern float g_MAX_MAP_COORD;
extern float g_MIN_MAP_COORD;

CGameConfig* GetActiveGame( void );
void SetActiveGame(CGameConfig *pGame);

bool GDError(TokenReader &tr, char *error, ...);
bool GDSkipToken(TokenReader &tr, trtoken_t ttexpecting = TOKENNONE, LPCTSTR pszExpecting = NULL);
bool GDGetToken(TokenReader &tr, char *pszStore, int nSize, trtoken_t ttexpecting = TOKENNONE, LPCTSTR pszExpecting = NULL);
bool GDGetTokenDynamic(TokenReader &tr, char **pszStore, trtoken_t ttexpecting, LPCTSTR pszExpecting = NULL);

#endif // GAMEDATA_H
