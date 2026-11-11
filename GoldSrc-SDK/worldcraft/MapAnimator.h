//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "MapKeyFrame.h"

class CMapAnimator : public CMapKeyFrame
{
public:

	DECLARE_MAPCLASS( CMapAnimator )

	CMapAnimator();
	//
	// Factories.
	//
	static CMapClass* Create( CHelperInfo* pInfo, CMapEntity* pParent );
	virtual size_t GetSize();
	virtual CMapClass* Copy( bool bUpdateDependencies );
	virtual CMapClass* CopyFrom( CMapClass* pFrom, bool bUpdateDependencies );
	virtual bool GetTransformMatrix( matrix4_t& matrix );
	virtual void OnParentKeyChanged( LPCSTR key, LPCSTR value );
	virtual void UpdateAnimation( float time );
	virtual void RebuildPath();
	virtual CMapAnimator* GetAnimator( CMapKeyFrame* pStart = NULL ) { return this; }

protected:

	matrix4_t		m_Transform;
	bool			m_bAnimating;
	CMapKeyFrame*	m_pCurrentKeyFrame; // keyframe it's currently at
	int				m_nTimeModifier;
	int				m_nPositionInterpolator;
	int				m_nRotationInterpolator;

	friend class CMapKeyFrame;
};
