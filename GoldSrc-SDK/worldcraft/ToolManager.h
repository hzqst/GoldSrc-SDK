//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "ToolInterface.h"
#include "utlvector.h"

class CToolDecal;
class CToolMagnify;
class CToolMaterial;
class CBaseTool;

class CToolManager
{
public:

	static bool Init();
	static void Shutdown();

	CBaseTool *GetTool();
	ToolID_t GetToolID();				// dvsFIXME: look for all calls to GetToolID and try to move behavior into the tools

	CBaseTool *GetToolForID(ToolID_t eToolID);

	void SetTool(ToolID_t nToolID);
	void PushTool(ToolID_t nToolID);
	void PopTool();

	inline int GetToolCount();
	inline CBaseTool *GetTool(int nIndex);

	void RemoveAllTools();
	void AddTool(CBaseTool *pTool);

private:

	CToolManager();
	~CToolManager();

	CUtlVector<CBaseTool *> m_Tools;			// List of ALL the tools.
	CUtlVector<CBaseTool *> m_InternalTools;	// List of the tools that are allocated by the tools manager.

    CBaseTool *m_pActiveTool;					// Pointer to the active new tool, NULL if none.
	ToolID_t m_eToolID;							// ID of the active tool. Kept separately for when switching documents.

	CUtlVector<ToolID_t> m_ToolIDStack;			// Stack of active tool IDs, for PushTool/PopTool.

	//
	// Tools that don't have any per-document context data are kept here.
	//
	CToolMagnify*	  m_pToolMagnify;
	CToolDecal *m_pToolDecal;
	CToolMaterial *m_pToolMaterial;
};

//-----------------------------------------------------------------------------
// Purpose: Accessor for iterating tools.
//-----------------------------------------------------------------------------
int CToolManager::GetToolCount()
{
	return m_Tools.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Accessor for iterating tools.
//-----------------------------------------------------------------------------
CBaseTool *CToolManager::GetTool(int nIndex)
{
	return m_Tools.Element(nIndex);
}

extern CToolManager* g_pToolManager;

#endif // TOOLMANAGER_H
