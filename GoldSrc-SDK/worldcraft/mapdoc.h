//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MAPDOC_H
#define MAPDOC_H
#ifdef _WIN32
#pragma once
#endif

#include "MapClass.h"
#include "MapEntity.h"
#include "GameData.h"

class Axes2;
class Selection3D;
class CToolBlock;
class Marker3D;
class Morph3D;
class Path3D;
class Clipper3D;
class CToolMagnify;
class CToolMaterial;
class Cordon3D;
class Camera3D;
class CMapDoc;
class CGameConfig;
class CHistory;
class CMapGroup;
class CMapView;
class CMapView3D;
class CMapView2D;

struct FindEntity_t;
struct FindGroup_t;
struct FindObjectNumber_t;
struct AddNonSelectedInfo_t;

enum MAPFORMAT;
enum ToolID_t;

enum
{
	MAPVIEW_UPDATE_DISPLAY = 0x01,
	MAPVIEW_UPDATE_OBJECTS		= 0x02,
	MAPVIEW_COLOR_ONLY			= 0x04,		// only update if view uses solid colors
	MAPVIEW_UPDATE_SELECTION	= 0x08,
	MAPVIEW_UPDATE_2D			= 0x10,
	MAPVIEW_UPDATE_3D			= 0x20,
	MAPVIEW_OPTIONS_CHANGED		= 0x40
};

//
// Flags for snap.
//
enum
{
	SNAP_HALF_GRID = 0x01,		// Snap on the half grid instead of on the grid.
};

// Flags for view2dinfo.wflags
enum
{
	VI_ZOOM = 0x01,
	VI_CENTER = 0x02
};

typedef struct
{
	WORD wFlags;
	float fZoom;
	Vector ptCenter;
} VIEW2DINFO;

//
// To pass as hint to UpdateAllViews.
//
class UpdateBox : public CObject
{
	public:

		UpdateBox(void) { Objects = NULL; }

		CMapObjectList *Objects;
		BoundBox Box;
};

class CMapDoc : public CDocument
{
	protected:

		CMapDoc(void);
		virtual ~CMapDoc();

		DECLARE_DYNCREATE(CMapDoc)

	public:

		bool IsAnimating() const { return m_bAnimating; }

		// attribs:
		bool m_bHidePaths;
		bool m_bSnapToGrid;
		bool m_bShowGrid;

		// pointfile stuff:
		enum
		{
			PFPNext = 1,
			PFPPrev = -1
		};

		int m_nPFPoints;
		int m_iCurPFPoint;
		Vector *m_pPFPoints;
		CString m_strLastPointFile;

		static inline CMapDoc *GetActiveMapDoc(void);
		static void SetActiveMapDoc(CMapDoc *pDoc);

	private:

		static CMapDoc *m_pMapDoc;

	public:

		inline int GetDocVersion(void);
		inline void DecrementDocVersion(void);

		//
		// Interface to the map document. Functions for object creation, deletion, etc.
		//
		CMapEntity *CreateEntity(const char *pszClassName, float x, float y, float z);
		bool DeleteEntity(const char *pszClassName, float x, float y, float z);
		CMapEntity *FindEntity(const char *pszClassName, float x, float y, float z);
		bool FindEntitiesByKeyValue(CMapEntityList *pFound, const char *szKey, const char *szValue);
		bool FindEntitiesByClassName(CMapEntityList *pFound, const char *szClassName);

		void Update(void);
		void SetModifiedFlag(BOOL bModified = TRUE);

		// other stuff
		float m_flCurrentTime;
		void UpdateCurrentTime( void );
		void UpdateAnimation();
		float GetTime( void ) { return m_flCurrentTime; }

		void GotoPFPoint(int iDirection);

		//
		// Camera tool.
		//
		void Camera_Get(Vector &vecPos, Vector &vecLookAt);
		void Camera_Update(const Vector &vecPos, const Vector &vecLookAt);

		//
		// Selection tool.
		//
		// TODO: clean up doc's interface to selection
		void ClearHitList();
		void AddHit(CMapClass *pObject);
		POSITION GetFirstHitPosition();
		CMapClass *GetNextHit(POSITION& p);
		void SetCurHit(int iIndex, BOOL bRedrawViews = TRUE);
		SelectMode_t Selection_GetMode(void);
		void Selection_SetMode(SelectMode_t eSelectMode);
		void Selection_Remove(CMapClass *pObject);
		void Selection_RemoveAll(void);
		int Selection_GetCount(void);
		CMapClass *Selection_GetObject(int nIndex);
		void Selection_GetBounds(Vector &vecMins, Vector &vecMaxs);
		void Selection_GetBoundsCenter(Vector &vecCenter);
		void Selection_UpdateBounds(void);
		bool Selection_IsSelected(CMapClass *pObj);
		CMapObjectList *Selection_GetList(void); // FIXME: HACK for interim support of old code, remove
		void Selection_GetLastValidBounds(Vector &vecMins, Vector &vecMaxs);

		enum
		{
			hitNext = -1,
			hitPrev = 2
		};

		// SelectObject/SelectFace parameters:
		enum
		{
			scToggle = 0x01,
			scSelect = 0x02,
			scUnselect = 0x04,
			scUpdateDisplay = 0x08,
			scClear = 0x10,				// Clear current before selecting
			scNoLift = 0x20,			// Don't lift face attributes into Face Properties dialog   dvs: lame!
			scNoApply = 0x40			// Don't apply face attributes from Face Properties dialog to selected face   dvs: lame!
		};

		//
		// Called by the tool manager when the active tool changes.
		//
		void ActivateTool(ToolID_t eNewTool);

		//
		// Cordon tool.
		//
		bool Cordon_IsCordoning(void);

		int Snap(float x);
		void Snap(CPoint &pt);
		void Snap(Vector &pt, int nFlags = 0);
		inline bool IsSnapEnabled(void);

		//
		// Face selection for face editing.
		//
		void SelectFace(CMapSolid *pSolid, int iFace, int cmd);
		void UpdateForApplicator(BOOL bApplicator);

		// cmd is from CMapView2d::updXXX
		void ToolUpdateViews(int cmd);

		void DeleteObject( CMapClass* pObject );
		void DeleteObjectList(CMapObjectList &List);

		// Object selection:
		void SelectObject(CMapClass *pobj, int cmd = scSelect);
		void SelectObjectList(CMapObjectList *pList);
		void SelectRegion(BoundBox *pBox, BOOL bInsideOnly);

		void CenterSelection(void);
		Vector GetSelectedCenter();

		void GetBestVisiblePoint(Vector &ptOrg);
		void Paste( CMapObjectList& Objects, CMapWorld* pSourceWorld, CMapWorld* pDestWorld,
					Vector vecOffset = vec3_origin, QAngle vecRotate = vec3_origin, CMapClass* pParent = NULL );
		void GetBestPastePoint(Vector &vecPasteOrigin);
		void NudgeObjects(Axes2& axes, UINT nChar, BOOL bSnap);
		void UpdateStatusbar();
		void SetView2dInfo(VIEW2DINFO& vi);
		void SetActiveView(CMapView *pViewActivate);
		void SetUndoActive(bool bActive);
		void UpdateTitle(CView*);

		void CountSolids();
		void CountSolids2();
		void ReplaceTextures(LPCTSTR pszFind, LPCTSTR pszReplace, BOOL bEverything, int iAction, BOOL bHidden);

		//
		// Functions for finding brushes and entities by ordinal. This allows us to go directly to
		// brushes that are reported by the compile tools.
		//
		CMapSolid *FindBrushNumber(CMapClass *pEntity, int nBrushNumber, BoundBox *pBoundBox, BOOL bVisiblesOnly);
		CMapClass *FindEntityNumber(int nEntityNumber, BoundBox *pBoundBox, BOOL bVisiblesOnly);

		bool Is3DGridEnabled(void) { return(m_bShow3DGrid); }


		inline MAPFORMAT GetMapFormat(void);
		inline CMapWorld *GetMapWorld(void);
		inline CGameConfig *GetGame(void);
		inline int GetGridSpacing(void) { return(max(m_nGridSpacing, 1)); }

		inline CHistory *GetDocHistory(void);

		inline int GetNextMapObjectID(void);

		static POSITION GetFirstDocPosition();
		static CMapDoc *GetNextDoc(POSITION &p);

		void SetMRU(CMapView2D *pView);
		void RemoveMRU(CMapView2D *pView);
		CTypedPtrList<CPtrList, CMapView2D*> MRU2DViews;

		void Update3DViews(void);
		BOOL SelectDocType(void);
		BOOL SaveModified(void);

		// Set edit prefab mode.
		void EditPrefab3D(DWORD dwPrefabID);

		//
		// Call these when modifying the document contents.
		//
		void AddObjectToWorld(CMapClass *pObject, CMapClass *pParent = NULL);
		BOOL FindObject(CMapClass *pObject);
		void RemoveObjectFromWorld(CMapClass *pMapClass, bool bNotifyChildren);
		void RenderPreloadObject(CMapClass *pObject);
		void UpdateAllObjects(void);
		void UpdateObject(CMapClass *pMapClass);
		void UpdateVisibilityAll( int flags = -1 );
		bool ShouldObjectBeVisible(CMapClass *pObject);
		void UpdateVisibility(CMapClass *pObject);

		//
		// Morph tool.
		//
		void Morph_SelectObject(CMapSolid *pSolid, unsigned int uCmd);
		void Morph_GetBounds(Vector &mins, Vector &maxs, bool bReset);
		int Morph_GetObjectCount(void);
		CSSolid *Morph_GetFirstObject(POSITION &pos);
		CSSolid *Morph_GetNextObject(POSITION &pos);

		// Displacement Map Functinos
		//void UpdateDispViews( void );

		//
		// List of VisGroups.
		//
		CVisGroup *VisGroups_AddGroup(CVisGroup *pGroup);
		CVisGroup *VisGroups_AddGroup(LPCTSTR pszName);
		int VisGroups_GetCount(void);
		POSITION VisGroups_GetHeadPosition(void);
		CVisGroup *VisGroups_GetNext(POSITION &pos);
		CVisGroup *VisGroups_GroupForID(DWORD id);
		void VisGroups_RemoveID(DWORD id);

		// ClassWizard generated virtual function overrides
		//{{AFX_VIRTUAL(CMapDoc)
		public:
		virtual BOOL OnNewDocument();
		virtual void DeleteContents();
		virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
		virtual void OnCloseDocument(void);
		virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
		//}}AFX_VIRTUAL

		BOOL Serialize(fstream &file, BOOL fIsStoring, BOOL bRMF);

		// Save a VMF file. saveFlags is a combination of SAVEFLAGS_ defines.

		inline bool IsLoading(void);

		void ExpandObjectKeywords(CMapClass *pObject, CMapWorld *pWorld);

		#ifdef _DEBUG
		virtual void AssertValid() const;
		virtual void Dump(CDumpContext& dc) const;
		#endif

		void UpdateAllCameras(Camera3D &camera);

	protected:

		void Initialize();

		//
		// Tools:
		//
  		Selection3D *m_pToolSelection;
  		CToolBlock *m_pToolBlock;
  		Marker3D *m_pToolMarker;
  		Morph3D *m_pToolMorph;
  		Path3D *m_pToolPath;
  		Clipper3D *m_pToolClipper;
		Cordon3D*	 m_pToolCordon;
		Camera3D *m_pToolCamera;

		int m_nGridSpacing;

		bool m_bLoading; // Set to true while we are being loaded from VMF.

		//
		// Search functions.
		//
		static BOOL FindEntityCallback(CMapClass *pObject, FindEntity_t *pFindInfo);
		static BOOL FindGroupCallback(CMapGroup *pGroup, FindGroup_t *pFindInfo);
		static BOOL FindObjectNumberCallback(CMapClass *pObject, FindObjectNumber_t *pFindInfo);

		static BOOL AddLeavesToListCallback(CMapClass *pObject, CMapObjectList *pList);
		static BOOL AddUnselectedToGroupCallback(CMapClass *pObject, AddNonSelectedInfo_t *pInfo);


		static BOOL UpdateVisibilityCallback(CMapClass *pObject, CMapDoc *pDoc);
		static BOOL ForceVisibilityCallback(CMapClass *pObject, bool bVisibility);

		CMapWorld *m_pWorld;				// The world that this document represents.
		CMapObjectList m_UpdateList;		// List of objects that have changed since the last call to Update.
		int m_nDocVersion;					// A number that increments every time the doc is modified after being saved.

		// Undo/Redo system.
		CHistory *m_pUndo;
		CHistory *m_pRedo;

		// Hit selection.
		CMapObjectList HitSel;
		int nHitSel;
		int iCurHit;

		int m_nNextMapObjectID;			// The ID that will be assigned to the next CMapClass object in this document.

		// Editing prefabs data.
		DWORD m_dwPrefabID;
		DWORD m_dwPrefabLibraryID;
		BOOL m_bEditingPrefab;
		bool m_bPrefab;					// true if this document IS a prefab, false if not.

		// Game configuration.
		CGameConfig *m_pGame;

		bool m_bShow3DGrid;				// Whether to render a grid in the 3D views.
		bool m_bHideItems;				// Whether to render point entities in all views.

		// Animation.
		bool m_bAnimating;
		float m_flAnimationTime;

		//
		// Visgroups.
		//
		CTypedPtrMap<CMapWordToPtr, WORD, CVisGroup *> m_VisGroups;

		//
		// Expands %i keyword in prefab targetnames to generate unique targetnames for this map.
		//
		bool ExpandTargetNameKeywords(char *szNewTargetName, const char *szOldTargetName, CMapWorld *pWorld);
		bool DoExpandKeywords(CMapClass *pObject, CMapWorld *pWorld, char *szOldKeyword, char *szNewKeyword);

		//{{AFX_MSG(CMapDoc)
		afx_msg void OnEditDelete();
		afx_msg void OnMapSnaptogrid();
		afx_msg void OnUpdateMapSnaptogrid(CCmdUI* pCmdUI);
		afx_msg void OnEditApplytexture();
		afx_msg void OnToolsSubtractselection();
		afx_msg void OnEditCopy();
		afx_msg void OnEditPaste();
		afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
		afx_msg void OnEditCut();
		afx_msg void OnToolsGroup();
		afx_msg void OnToolsUngroup();
		afx_msg void OnViewGrid();
		afx_msg void OnUpdateViewGrid(CCmdUI* pCmdUI);
		afx_msg void OnEditSelectall();
		afx_msg void OnFileSaveAs();
		afx_msg void OnFileSave();
		afx_msg void OnMapGridlower();
		afx_msg void OnMapGridhigher();
		afx_msg void OnEditToworld();
		afx_msg void OnEditToWorld();
		afx_msg void OnFileExport();
		afx_msg void OnFileExportAgain();
		afx_msg void OnEditMapproperties();
		afx_msg void OnFileRunmap();
		afx_msg void OnToolsHideitems();
		afx_msg void OnUpdateToolsSubtractselection(CCmdUI* pCmdUI);
		afx_msg void OnUpdateToolsHideitems(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditDelete(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditFunction(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditCut(CCmdUI* pCmdUI);
		afx_msg void OnUpdateEditMapproperties(CCmdUI* pCmdUI);
		afx_msg void OnMapInformation();
		afx_msg void OnViewCenteronselection();
		afx_msg void OnToolsHollow();
		afx_msg void OnEditPastespecial();
		afx_msg void OnUpdateEditPastespecial(CCmdUI* pCmdUI);
		afx_msg void OnEditSelnext();
		afx_msg void OnEditSelprev();
		afx_msg void OnViewHideselectedobjects();
		afx_msg void OnViewShowhiddenobjects();
		afx_msg void OnMapCheck();
		afx_msg void OnViewShowconnections();
		afx_msg void OnUpdateViewShowconnections(CCmdUI* pCmdUI);
		afx_msg void OnUpdateFileSave(CCmdUI* pCmdUI);
		afx_msg void OnToolsCreateprefab();
		afx_msg void OnInsertprefabOriginal();
		afx_msg void OnEditReplacetex();
		afx_msg void OnToolsSnapselectedtogrid();
		afx_msg void OnUpdateToolsSplitface(CCmdUI* pCmdUI);
		afx_msg void OnToolsSplitface();
		afx_msg void OnToolsTransform();
		afx_msg void OnToolsToggletexlock();
		afx_msg void OnUpdateToolsToggletexlock(CCmdUI* pCmdUI);
		afx_msg void OnToolsTextureAlignment(void);
		afx_msg void OnUpdateToolsTextureAlignment(CCmdUI *pCmdUI);
		afx_msg void OnToggleCordon();
		afx_msg void OnUpdateToggleCordon(CCmdUI* pCmdUI);
		afx_msg void OnEditCordon();
		afx_msg void OnUpdateEditCordon(CCmdUI* pCmdUI);
		afx_msg void OnViewHidenonselectedobjects();
		afx_msg void OnUpdateViewHidenonselectedobjects(CCmdUI* pCmdUI);
		afx_msg void OnToggleGroupignore();
		afx_msg void OnUpdateToggleGroupignore(CCmdUI* pCmdUI);
		afx_msg void OnVscaleToggle();
		afx_msg void OnMapEntityreport();
		afx_msg void OnToggleSelectbyhandle();
		afx_msg void OnUpdateToggleSelectbyhandle(CCmdUI* pCmdUI);
		afx_msg void OnToggleInfiniteselect();
		afx_msg void OnUpdateToggleInfiniteselect(CCmdUI* pCmdUI);
		afx_msg void OnFileExporttodxf();
		afx_msg void OnUpdateEditApplytexture(CCmdUI* pCmdUI);
		afx_msg void OnViewHidepaths();
		afx_msg void OnUpdateViewHidepaths(CCmdUI* pCmdUI);
		afx_msg void OnMapLoadpointfile();
		afx_msg void OnMapUnloadpointfile();
		afx_msg void OnUpdate3dViewUI(CCmdUI* pCmdUI);
		afx_msg BOOL OnChange3dViewType(UINT nID);
		afx_msg void OnEditToEntity();
		afx_msg BOOL OnUndoRedo(UINT nID);
		afx_msg void OnUpdateUndoRedo(CCmdUI* pCmdUI);
		afx_msg void OnChangeVertexscale();
		afx_msg void OnUpdateGroupEditFunction(CCmdUI* pCmdUI);
		afx_msg void OnUpdateFileExport(CCmdUI *pCmdUI);
		afx_msg void OnEditClearselection();
		afx_msg void OnUpdateToggle3DGrid(CCmdUI* pCmdUI);
		afx_msg void OnMapGotoBrush(void);
	public:
		afx_msg void OnToggle3DGrid();
		//}}AFX_MSG

		DECLARE_MESSAGE_MAP()
};

//-----------------------------------------------------------------------------
// Purpose:
// Output : inline void
//-----------------------------------------------------------------------------
void CMapDoc::DecrementDocVersion(void)
{
	if (m_nDocVersion > 0)
	{
		m_nDocVersion--;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if we are loading, false if not.
//-----------------------------------------------------------------------------
bool CMapDoc::IsLoading(void)
{
	return(m_bLoading);
}

//-----------------------------------------------------------------------------
// Purpose: Returns the document that is currently active.
//-----------------------------------------------------------------------------
CMapDoc *CMapDoc::GetActiveMapDoc(void)
{
	return(m_pMapDoc);
}

//-----------------------------------------------------------------------------
// Purpose: Returns the current version of the document that is being worked on.
//-----------------------------------------------------------------------------
int CMapDoc::GetDocVersion(void)
{
	return(m_nDocVersion);
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the game configuration for this document.
//-----------------------------------------------------------------------------
CGameConfig *CMapDoc::GetGame(void)
{
	return(m_pGame);
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the Undo system for this document.
//-----------------------------------------------------------------------------
CHistory *CMapDoc::GetDocHistory(void)
{
	return(m_pUndo);
}

//-----------------------------------------------------------------------------
// Purpose: Returns the map format of the game configuration for this document.
//-----------------------------------------------------------------------------
MAPFORMAT CMapDoc::GetMapFormat(void)
{
	if (m_pGame != NULL)
	{
		return(m_pGame->mapformat);
	}

	return ( mfHalfLife );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the world that this document edits.
//-----------------------------------------------------------------------------
CMapWorld *CMapDoc::GetMapWorld(void)
{
	return(m_pWorld);
}

//-----------------------------------------------------------------------------
// Purpose: All map objects in a given document are assigned a unique ID.
//-----------------------------------------------------------------------------
int CMapDoc::GetNextMapObjectID(void)
{
	return(m_nNextMapObjectID++);
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not grid snap is enabled. Called by the tools and
//			views to determine snap behavior.
//-----------------------------------------------------------------------------
bool CMapDoc::IsSnapEnabled(void)
{
	return m_bSnapToGrid;
}

#endif // MAPDOC_H
