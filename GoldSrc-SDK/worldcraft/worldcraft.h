//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: Defines the application object.
//
// $NoKeywords: $
//=============================================================================

#ifndef WORLDCRAFT_H
#define WORLDCRAFT_H
#ifdef _WIN32
#pragma once
#endif


#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include "RunCommands.h"


//
// Values for retrieving specific directories using GetDirectory.
//
enum DirIndex_t
{
	DIR_PROGRAM,			// The Worldcraft install directory.
	DIR_PREFABS,			// The directory for prefabs.
	DIR_GAME_EXE,			// The location of the game executable.
	DIR_MOD,				// The location of the mod currently being worked on.
	DIR_GAME				// The location of the base game currently being worked on.
};

//
// Values for scanning the search path using GetFirst/GetNextSearchDir. These
// are typically scanned in the following order: mod dir, game dir, Worldcraft dir.
//
enum SearchDir_t
{
	SEARCHDIR_DEFAULT = 0,	// Search directory for most resources.
	SEARCHDIR_SOUND,		// Search directory for sounds.
	SEARCHDIR_WADS,			// Search directory for WAD files.
	SEARCHDIR_SPRITES,		// Search directory for sprites.
	SEARCHDIR_SOUNDS,		// Search directory for sounds.
	SEARCHDIR_MODELS			// Search directory for models.
};


// combines a list of commands & a name:
class CCommandSequence
{
	public:

		CCommandArray m_Commands;
		char m_szName[128];
};


class CWorldcraft : public CWinApp
{
	public:

		CWorldcraft(void);
	   ~CWorldcraft(void);

		virtual BOOL PreTranslateMessage(MSG *pMsg);

		// Overrides
		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CWorldcraft)
		public:
		virtual BOOL InitInstance();
		virtual int ExitInstance();
		virtual CDocument* OpenDocumentFile(LPCTSTR lpszFileName);
		virtual void AddToRecentFileList(LPCTSTR lpszPathName);
		virtual BOOL OnIdle(LONG lCount);
		virtual int Run(void);
		//}}AFX_VIRTUAL

		void GetDirectory(DirIndex_t dir, char *p);
		void GetFirstSearchDir(SearchDir_t eDir, char *szDir, POSITION &pos);
		void GetNextSearchDir(SearchDir_t eDir, char *szDir, POSITION &pos);

		COLORREF GetProfileColor(const char *pszSection, const char *pszKey, int r, int g, int b);

		void OnActivateApp(bool bActive);
		bool IsActiveApp(void);

		void BeginImportWCSettings(void);
		void EndImportSettings(void);

		bool Is3DRenderEnabled(void);
		void Enable3DRender(bool bEnable);

		void Help(const char *pszTopic);

		// list of "command arrays" for compiling files:
		CTypedPtrArray<CPtrArray,CCommandSequence*> m_CmdSequences;
		void SaveSequences();
		void LoadSequences();



		CMultiDocTemplate *pMapDocTemplate;
		CMultiDocTemplate *pPakDocTemplate;

		//{{AFX_MSG(CWorldcraft)
		afx_msg void OnAppAbout();
		afx_msg void OnFileOpen();
		afx_msg void OnFileNew();
		//}}AFX_MSG

		DECLARE_MESSAGE_MAP()

	protected:

		void AppendSearchSubDir(SearchDir_t eDir, char *szDir);

		bool m_bActiveApp;
		bool m_bEnable3DRender;

		char m_szAppDir[MAX_PATH];
};


#define APP()		((CWorldcraft *)AfxGetApp())


#endif // WORLDCRAFT_H
