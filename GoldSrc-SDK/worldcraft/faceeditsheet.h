//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FACEEDITSHEET_H
#define FACEEDITSHEET_H
#pragma once

#include "FaceEdit_MaterialPage.h"

class CMapFace;
class CMapSolid;

//=============================================================================
//
// Face Edit Sheet
//
class CFaceEditSheet : public CFaceEditMaterialPage
{
	DECLARE_DYNAMIC( CFaceEditSheet )

public:
	// solid/face selection structure
	typedef struct
	{
		CMapFace*  pMapFace;
		CMapSolid* pMapSolid;
	} StoredFace_t;

	CFaceEditSheet();
	virtual ~CFaceEditSheet();
	BOOL Create( CWnd* pParentWnd );
	void SetVisibility( bool bVisible );
	void UpdateControls();

	//=========================================================================
	//
	// Selection
	//
	enum
	{
		FACE_LIST_SIZE = 32
	};

	enum
	{
		id_SwitchModeStart = 0x100,
		ModeLiftSelect,
		ModeLift,
		ModeSelect,
		ModeApply,
		ModeApplyAll,
		ModeEdgeAlign,
		ModeAlignToView,
		id_SwitchModeEnd
	};

	enum
	{
		cfToggle = 0x01, // toggle - if selected, then unselect
		cfSelect = 0x02, // select
		cfUnselect = 0x04, // unselect
		cfClear = 0x08, // clear face list
		cfEdgeAlign = 0x10			// align face texture coordinates to 3d view alignment - should be here???
	};

	void ClickFace( CMapSolid* pSolid, int faceIndex, int cmd, int clickMode = -1 );
	void SetClickMode( int mode ) { m_ClickMode = mode; }
	int GetClickMode() { return m_ClickMode; }
	void EnableUpdate( bool bEnable );
	bool HasUpdateEnabled() { return m_bEnableUpdate; }
	int GetFaceListCount() { return m_FaceCount; }
	CMapFace* GetFaceListDataFace( int index ) { return m_Faces[index].pMapFace; }
	CMapSolid* GetFaceListDataSolid( int index ) { return m_Faces[index].pMapSolid; }
	//=========================================================================
	//
	// Virtual Overrides
	//
	//{{AFX_VIRTUAL( CFaceEditSheet )
	virtual BOOL PreTranslateMessage( MSG* pMsg );

protected:
	int									m_FaceCount;			// number of faces (solids) in list
	CArray<StoredFace_t, StoredFace_t&> m_Faces; // face list
	int									m_ClickMode;			// lame this is here!!!! -- see CMapDoc (SelectFace)
	bool								m_bEnableUpdate;		// update the dialog (true/false)

	void ClearFaceList();
	int FindFaceInList( CMapFace* pFace );
	//=========================================================================
	//
	// Message Map
	//
	//{{AFX_MSG( CFaceEditSheet )
	afx_msg void OnClose();
	DECLARE_MESSAGE_MAP()
};

#endif // FACEEDITSHEET_H
