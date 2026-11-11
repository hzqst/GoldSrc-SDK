//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "MapHelper.h"

class CHelperInfo;
class CMapAnimator;

class CMapKeyFrame : public CMapHelper
{
public:

	DECLARE_MAPCLASS( CMapKeyFrame )

	CMapKeyFrame();
	virtual ~CMapKeyFrame();
	//
	// Factories.
	//
	static CMapClass* Create( CHelperInfo* pInfo, CMapEntity* pParent );

	virtual size_t GetSize();
	virtual CMapClass* Copy( bool bUpdateDependencies );
	virtual CMapClass* CopyFrom( CMapClass* pFrom, bool bUpdateDependencies );
	virtual void GetOrigin( Vector& origin );
	virtual void SetOrigin( Vector& origin );
	virtual void Render3D( CRender3D* pRender );
	virtual void OnParentKeyChanged( LPCSTR key, LPCSTR value );
	virtual void UpdateDependencies( CMapWorld* pWorld, CMapClass* pObject );
	virtual void OnClone( CMapClass* pClone, CMapWorld* pWorld,
		CMapObjectList& OriginalList, CMapObjectList& NewList );
	virtual void OnRemoveFromWorld( CMapWorld* pWorld, bool bNotifyChildren );
	virtual void OnUndoRedo() { m_bRebuildPath = true; }
	virtual bool IsVisualElement() { return false; }
	virtual bool ShouldRenderLast() { return false; }

	void GetQuaternion( float* quaternion );
	double GetTotalTime();
	CMapKeyFrame* GetNextKeyFrame();
	CMapKeyFrame* GetPreviousKeyFrame();
	virtual CMapAnimator* GetAnimator( CMapKeyFrame* pStart = NULL );
	virtual void RebuildPath();
	static void Interpolate( CMapKeyFrame* pFrame, float time, Vector& origin,
		float* quaternion, int positionInterpolator, int rotationInterpolator );

protected:

	void CalculateNextTime();
	void Unlink();

	float			m_Quaternion[4];
	QAngle			m_Angles;
	float			m_flNextTime;
	float			m_flMoveSpeed;
	CMapKeyFrame*	m_pNextKeyFrame; // The next keyframe in our path.
	CMapKeyFrame*	m_pPreviousKeyFrame;
	Vector			m_Path[6];
	bool			m_bRebuildPath;

	friend class CMapAnimator;
};
