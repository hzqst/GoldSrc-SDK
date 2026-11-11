//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: contains all the data entered about a mod in liblist.gam
//-----------------------------------------------------------------------------
class CModInfo
{
public:
	CModInfo();
	~CModInfo();

	void LoadCurrentGameInfo();
	// loads mod info from previously loaded liblist.gam
	void LoadGameInfoFromBuffer( const char* buffer, int bufferSize );
	void FreeModInfo();

	// data accessors
	const char* GetStartMap();
	const char* GetTrainMap();
	bool IsMultiplayerOnly();
	bool IsSinglePlayerOnly();
	bool BShowAnimatedTitle();
	bool BHasHDBackground();
	bool BShowSimpleLoadingDialog();
	bool NoModels();
	bool NoHiModel();
	const char* GetFallbackDir();
	bool UseFallbackDirMaps();
	bool GetDetailedTexture();
	const char* GetGameDescription();
	const char* GetMPFilter();

private:
	KeyValues* m_pModData;
};

CModInfo*	ModInfo();
