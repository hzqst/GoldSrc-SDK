//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "MapAnimator.h"
#include "MapDoc.h"
#include "MapEntity.h"

IMPLEMENT_MAPCLASS( CMapAnimator )

static void QuaternionTransform( const float* quaternion, matrix4_t& matrix )
{
	double x = quaternion[0];
	double y = quaternion[1];
	double z = quaternion[2];
	double w = quaternion[3];
	matrix[0][0] = (float)( 1.0 - 2.0 * y * y - 2.0 * z * z );
	matrix[1][0] = (float)( 2.0 * ( z * w + x * y ) );
	matrix[2][0] = (float)( 2.0 * z * x - 2.0 * w * y );
	matrix[0][1] = (float)( 2.0 * x * y - 2.0 * z * w );
	matrix[1][1] = (float)( 1.0 - 2.0 * x * x - 2.0 * z * z );
	matrix[2][1] = (float)( 2.0 * ( z * y + x * w ) );
	matrix[0][2] = (float)( 2.0 * ( z * x + w * y ) );
	matrix[1][2] = (float)( 2.0 * z * y - 2.0 * x * w );
	matrix[2][2] = (float)( 1.0 - 2.0 * x * x - 2.0 * y * y );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapAnimator::CMapAnimator()
{
	IdentityMatrix( m_Transform );
	m_bAnimating = false;
	m_pCurrentKeyFrame = this;
	m_nTimeModifier = 0;
	m_nPositionInterpolator = 0;
	m_nRotationInterpolator = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapKeyFrame from a set
//			of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass* CMapAnimator::Create( CHelperInfo*, CMapEntity* )
{
	return new CMapAnimator;
}

size_t CMapAnimator::GetSize()
{
	return sizeof( *this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass* CMapAnimator::Copy( bool bUpdateDependencies )
{
	CMapAnimator* pCopy = new CMapAnimator;
	pCopy->CopyFrom( this, bUpdateDependencies );
	return pCopy;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObj - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass* CMapAnimator::CopyFrom( CMapClass* pObject, bool bUpdateDependencies )
{
	CMapAnimator* pFrom = dynamic_cast<CMapAnimator*>( pObject );
	if ( !pFrom )
		return this;

	CMapKeyFrame::CopyFrom( pObject, bUpdateDependencies );
	m_Transform = pFrom->m_Transform;
	m_bAnimating = false;
	m_pCurrentKeyFrame = NULL;	// keyframe it's currently at
	m_nTimeModifier = pFrom->m_nTimeModifier;
	m_nPositionInterpolator = pFrom->m_nPositionInterpolator;
	m_nRotationInterpolator = pFrom->m_nRotationInterpolator;
	return this;
}

bool CMapAnimator::GetTransformMatrix( matrix4_t& matrix )
{
	if ( !m_bAnimating )
		return false;
	matrix = m_Transform;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Notifies that the entity this is attached to has had a key change
// Input  : key - 
//			value - 
//-----------------------------------------------------------------------------
void CMapAnimator::OnParentKeyChanged( LPCSTR key, LPCSTR value )
{
	if ( !stricmp( key, "TimeModifier" ) )
		m_nTimeModifier = atoi( value );
	else if ( !stricmp( key, "PositionInterpolator" ) )
	{
		m_nPositionInterpolator = atoi( value );
		// HACK: Force everything in the path to update. Better to follow our path and update only it.
		UpdateAllDependencies( this );
	}
	else if ( !stricmp( key, "RotationInterpolator" ) )
		m_nRotationInterpolator = atoi( value );
	CMapKeyFrame::OnParentKeyChanged( key, value );
}

void CMapAnimator::RebuildPath()
{
	CMapKeyFrame::RebuildPath();
	CMapKeyFrame* pFrame = this;
	while ( pFrame )
	{
		CMapKeyFrame* pPrevious = pFrame;
		pFrame = pFrame->m_pNextKeyFrame;
		if ( dynamic_cast<CMapAnimator*>( pFrame ) )
		{
			AfxMessageBox( "Warning: a set of keyframes can have only one movement behavior, "
				"which can appear in the set only once.\nThe keyvalue 'NextKey' pointing at "
				"the offending move behavior has been cleared." );
			//
			// Get the entity parent of this keyframe so we can query keyvalues.
			//
			CMapEntity* pEntity = dynamic_cast<CMapEntity*>( pPrevious->GetParent() );
			if ( pEntity )
				pEntity->SetKeyValue( "NextKey", "" );
			m_bRebuildPath = true;
			RebuildPath();
		}
	}
}

void CMapAnimator::UpdateAnimation( float time )
{
	CMapDoc* pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc || !pDoc->IsAnimating() || !Parent || !Parent->IsSelected() )
	{
		m_bAnimating = false;
		return;
	}

	m_bAnimating = true;
	double total = GetTotalTime();
	float segmentTime = (float)( (double)time / total * total );
	CMapKeyFrame* pFrame = this;
	while ( pFrame->m_flNextTime <= segmentTime )
	{
		if ( pFrame->m_flNextTime < 0.01f )
		{
			segmentTime = 0;
			break;
		}
		segmentTime -= pFrame->m_flNextTime;
		pFrame = pFrame->GetNextKeyFrame();
	}
	m_pCurrentKeyFrame = pFrame;
	Vector position, origin;
	float quaternion[4];
	Interpolate( pFrame, segmentTime, position, quaternion, m_nPositionInterpolator, m_nRotationInterpolator );
	GetOrigin( origin );
	matrix4_t rotation, translation, combined;
	IdentityMatrix( rotation );
	IdentityMatrix( translation );
	for ( int i = 0; i < 3; i++ )
		translation[i][3] = -origin[i];
	QuaternionTransform( quaternion, rotation );
	MultiplyMatrix( rotation, translation, combined );
	// transform back to our new position
	IdentityMatrix( translation );
	for ( int i = 0; i < 3; i++ )
		translation[i][3] = position[i];
	MultiplyMatrix( translation, combined, m_Transform );
}
