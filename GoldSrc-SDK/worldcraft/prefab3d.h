//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PREFAB3D_H
#define PREFAB3D_H
#pragma once

#include "Prefabs.h"

class CMapWorld;
class Vector;

class CPrefab3D : public CPrefab
{
	public:

		CPrefab3D();
		~CPrefab3D();

		virtual bool IsLoaded(void);
		void FreeData();

		void CenterOnZero();

		CMapClass *Create(void);
		CMapClass *CreateInBox(BoundBox *pBox);
		CMapClass *CreateAtPoint(const Vector &point);
		CMapClass *CreateAtPointAroundOrigin( Vector const &point );

		int GetType(void) { return pt3D; }
		inline CMapWorld *GetWorld(void);
		inline void SetWorld(CMapWorld *pWorld);

	protected:

		// prefab data:
		CMapWorld *m_pWorld;
};

//-----------------------------------------------------------------------------
// Purpose: Returns a world containing the objects that make up this prefab.
//-----------------------------------------------------------------------------
CMapWorld *CPrefab3D::GetWorld(void)
{
	return(m_pWorld);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPrefab3D::SetWorld(CMapWorld *pWorld)
{
	FreeData();
	m_pWorld = pWorld;
}

class CPrefabRMF : public CPrefab3D
{
	public:

		CPrefabRMF();
		~CPrefabRMF();

		int Init(LPCTSTR pszFilename, BOOL bLoadNow = FALSE, DWORD = 0);
		int Load(DWORD dwFlags = 0);
		int Save(LPCTSTR pszFilename, DWORD = 0);

		int Init(fstream &file, BOOL bLoadNow = FALSE, DWORD = 0);
		int Save(fstream &file, DWORD = 0);

	private:

		int DoLoad(fstream&, DWORD = 0);
		int DoSave(fstream&, DWORD = 0);
};

#endif // PREFAB3D_H
