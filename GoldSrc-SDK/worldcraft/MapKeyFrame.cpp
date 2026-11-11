//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "stdafx.h"
#include "MapKeyFrame.h"
#include "MapAnimator.h"
#include "History.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapWorld.h"
#include "Render3D.h"

#include <gl/GL.h>

IMPLEMENT_MAPCLASS( CMapKeyFrame )

static void AnglesQuaternion( const QAngle& angles, float* quaternion )
{
	double sy = sin( angles[1] * 0.008726646259971648 );
	double cy = cos( angles[1] * 0.008726646259971648 );
	double sp = sin( angles[0] * 0.008726646259971648 );
	double cp = cos( angles[0] * 0.008726646259971648 );
	float sr = (float)sin( angles[2] * 0.008726646259971648 );
	float cr = (float)cos( angles[2] * 0.008726646259971648 );
	float rollPitch = (float)( sr * cp );
	float pitchRoll = (float)( cr * sp );
	quaternion[0] = (float)( rollPitch * cy - pitchRoll * sy );
	quaternion[1] = (float)( pitchRoll * cy + rollPitch * sy );
	float yawRoll = (float)( cr * cp );
	double pitchYaw = sp * sr;
	quaternion[2] = (float)( yawRoll * sy - pitchYaw * cy );
	quaternion[3] = (float)( pitchYaw * sy + yawRoll * cy );
}

static void SlerpQuaternion( const float* start, float* end, float fraction, float* out )
{
	double difference = 0;
	double sum = 0;
	for ( int i = 0; i < 4; i++ )
	{
		double delta = start[i] - end[i];
		difference += delta * delta;
		double total = start[i] + end[i];
		sum += total * total;
	}
	if ( difference > sum )
	{
		for ( int i = 0; i < 4; i++ )
			end[i] = -end[i];
	}

	float cosine = start[3] * end[3] + start[2] * end[2] + start[1] * end[1] + start[0] * end[0];
	if ( 1.0 + cosine <= 0.000001 )
	{
		out[0] = -end[1];
		out[1] = end[0];
		out[2] = -end[3];
		out[3] = end[2];
		double startScale = sin( ( 1.0 - fraction ) * 1.5707963267948966 );
		double endScale = sin( fraction * 1.5707963267948966 );
		for ( int i = 0; i < 3; i++ )
			out[i] = (float)( startScale * start[i] + endScale * out[i] );
		return;
	}

	double startScale;
	double endScale;
	if ( 1.0 - cosine <= 0.000001 )
	{
		startScale = 1.0 - fraction;
		endScale = fraction;
	}
	else
	{
		float angle = (float)acos( cosine );
		float sine = (float)sin( angle );
		startScale = sin( ( 1.0 - fraction ) * angle ) / sine;
		endScale = sin( angle * fraction ) / sine;
	}
	for ( int i = 0; i < 4; i++ )
		out[i] = (float)( startScale * start[i] + endScale * end[i] );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapKeyFrame::CMapKeyFrame()
{
	m_pNextKeyFrame = NULL;
	m_pPreviousKeyFrame = NULL;
	m_flNextTime = 0;
	m_flMoveSpeed = 0;
	m_bRebuildPath = false;
	m_Angles.Init();
	m_Quaternion[0] = m_Quaternion[1] = m_Quaternion[2] = 0;
	m_Quaternion[3] = 1;
}

CMapKeyFrame::~CMapKeyFrame()
{
	Unlink();
}

//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapKeyFrame from a set
//			of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass* CMapKeyFrame::Create( CHelperInfo*, CMapEntity* )
{
	return new CMapKeyFrame;
}

size_t CMapKeyFrame::GetSize()
{
	return sizeof( *this );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass* CMapKeyFrame::Copy( bool bUpdateDependencies )
{
	CMapKeyFrame* pCopy = new CMapKeyFrame;
	pCopy->CopyFrom( this, bUpdateDependencies );
	return pCopy;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObj - 
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass* CMapKeyFrame::CopyFrom( CMapClass* pObject, bool bUpdateDependencies )
{
	CMapKeyFrame* pFrom = dynamic_cast<CMapKeyFrame*>( pObject );
	if ( !pFrom )
		return this;

	CMapClass::CopyFrom( pObject, bUpdateDependencies );
	memcpy( m_Quaternion, pFrom->m_Quaternion, sizeof( m_Quaternion ) );
	m_Angles = pFrom->m_Angles;
	m_flNextTime = pFrom->m_flNextTime;
	m_flMoveSpeed = pFrom->m_flMoveSpeed;
	m_pNextKeyFrame = pFrom->m_pNextKeyFrame;
	m_pPreviousKeyFrame = pFrom->m_pPreviousKeyFrame;
	m_bRebuildPath = true;
	return this;
}

void CMapKeyFrame::GetOrigin( Vector& origin )
{
	if ( Parent )
		Parent->GetOrigin( origin );
	else
		CMapPoint::GetOrigin( origin );
}

//-----------------------------------------------------------------------------
// Purpose: Recalulates timings based on the new position
// Input  : *pfOrigin - 
//-----------------------------------------------------------------------------
void CMapKeyFrame::SetOrigin( Vector& )
{
	m_bRebuildPath = true;
	if ( m_pPreviousKeyFrame )
		m_pPreviousKeyFrame->m_bRebuildPath = true;
}

void CMapKeyFrame::GetQuaternion( float* quaternion )
{
	memcpy( quaternion, m_Quaternion, sizeof( m_Quaternion ) );
}

double CMapKeyFrame::GetTotalTime()
{
	if ( m_pNextKeyFrame )
		return m_flNextTime + m_pNextKeyFrame->GetTotalTime();
	return 0;
}

CMapKeyFrame* CMapKeyFrame::GetNextKeyFrame()
{
	return m_pNextKeyFrame ? m_pNextKeyFrame : this;
}

CMapKeyFrame* CMapKeyFrame::GetPreviousKeyFrame()
{
	return m_pPreviousKeyFrame ? m_pPreviousKeyFrame : this;
}

CMapAnimator* CMapKeyFrame::GetAnimator( CMapKeyFrame* pStart )
{
	if ( pStart == this )
		return NULL;
	if ( !pStart )
		pStart = this;
	return m_pPreviousKeyFrame ? m_pPreviousKeyFrame->GetAnimator( pStart ) : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Notifies that the entity this is attached to has had a key change
// Input  : key - 
//			value - 
//-----------------------------------------------------------------------------
void CMapKeyFrame::OnParentKeyChanged( LPCSTR key, LPCSTR value )
{
	if ( !stricmp( key, "NextKey" ) )
	{
		CMapWorld* pWorld = GetWorldObject( this );
		if ( pWorld )
			UpdateDependencies( pWorld, NULL );
	}
	else if ( !stricmp( key, "NextTime" ) )
		m_flNextTime = (float)atof( value );
	else if ( !stricmp( key, "MoveSpeed" ) )
	{
		m_flMoveSpeed = (float)atof( value );
		m_bRebuildPath = true;
	}
	else if ( !stricmp( key, "angles" ) )
	{
		(void)sscanf( value, "%f %f %f", &m_Angles[0], &m_Angles[1], &m_Angles[2] );
		AnglesQuaternion( m_Angles, m_Quaternion );
	}
}

void CMapKeyFrame::UpdateDependencies( CMapWorld*, CMapClass* )
{
	m_bRebuildPath = true;
}

void CMapKeyFrame::Unlink()
{
	if ( m_pNextKeyFrame && m_pNextKeyFrame->m_pPreviousKeyFrame == this )
	{
		m_pNextKeyFrame->m_pPreviousKeyFrame = NULL;
		m_pNextKeyFrame->m_bRebuildPath = true;
	}
	if ( m_pPreviousKeyFrame && m_pPreviousKeyFrame->m_pNextKeyFrame == this )
	{
		m_pPreviousKeyFrame->m_pNextKeyFrame = NULL;
		m_pPreviousKeyFrame->m_bRebuildPath = true;
	}
	m_pNextKeyFrame = m_pPreviousKeyFrame = NULL;
}

void CMapKeyFrame::CalculateNextTime()
{
	if ( m_flMoveSpeed <= 0 || !m_pNextKeyFrame )
		return;

	Vector end;
	m_pNextKeyFrame->GetOrigin( end );
	float distance = ( end - m_Origin ).Length();
	if ( distance == 0 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			float angle = m_Angles[i] - m_pNextKeyFrame->m_Angles[i];
			if ( angle > 180 )
				angle -= 360;
			float amount = (float)abs( (int)angle );
			if ( amount > distance )
				distance = amount;
		}
	}
	m_flNextTime = distance / m_flMoveSpeed;
	// rebuild the path before we next render
	m_bRebuildPath = true;
}

void CMapKeyFrame::Interpolate( CMapKeyFrame* pFrame, float time, Vector& origin,
	float* quaternion, int positionInterpolator, int rotationInterpolator )
{
	float fraction = pFrame->m_flNextTime >= 0.01 ? time / pFrame->m_flNextTime : 1;
	CMapKeyFrame* frames[4] =
	{
		pFrame->GetPreviousKeyFrame(), pFrame, pFrame->GetNextKeyFrame(),
		pFrame->GetNextKeyFrame()->GetNextKeyFrame()
	};
	Vector points[4];
	for ( int i = 0; i < 4; i++ )
		frames[i]->GetOrigin( points[i] );

	if ( fraction < 0 || fraction > 1 )
		return;

	if ( positionInterpolator == 0 )
	{
		for ( int i = 0; i < 3; i++ )
			origin[i] = (float)( fraction * points[2][i] + ( 1.0 - fraction ) * points[1][i] );
	}
	else if ( positionInterpolator == 1 )
	{
		for ( int i = 0; i < 3; i++ )
		{
			double a = points[0][i];
			double b = points[1][i];
			double c = points[2][i];
			double d = points[3][i];
			origin[i] = (float)( ( ( a + a - b * 5.0 + c * 4.0 - d ) * ( fraction * fraction )
				+ ( b * 3.0 - a - c * 3.0 + d ) * ( fraction * ( fraction * fraction ) )
				+ ( -a + c ) * fraction + b + b ) * 0.5 );
		}
	}

	if ( rotationInterpolator == 0 )
	{
		float start[4], end[4];
		frames[1]->GetQuaternion( start );
		frames[2]->GetQuaternion( end );
		SlerpQuaternion( start, end, fraction, quaternion );
	}
}

void CMapKeyFrame::RebuildPath()
{
	m_bRebuildPath = false;
	CMapEntity* pEntity = dynamic_cast<CMapEntity*>( Parent );
	CMapWorld* pWorld = GetWorldObject( this );
	if ( !pEntity || !pWorld )
		return;

	if ( m_pNextKeyFrame )
	{
		m_pNextKeyFrame->m_pPreviousKeyFrame = NULL;
		m_pNextKeyFrame = NULL;
	}
	CMapEntity* pNextEntity = pWorld->FindChildByKeyValue( "targetname", pEntity->GetKeyValue( "NextKey" ) );
	if ( pNextEntity )
	{
		CMapKeyFrame* pNext = pNextEntity->GetChildOfType( (CMapKeyFrame*)NULL );
		if ( pNext != this )
		{
			m_pNextKeyFrame = pNext;
			if ( pNext )
				pNext->m_pPreviousKeyFrame = this;
		}
	}
	if ( m_pNextKeyFrame && m_pNextKeyFrame->m_bRebuildPath )
		m_pNextKeyFrame->RebuildPath();
	if ( m_pPreviousKeyFrame && m_pPreviousKeyFrame->m_bRebuildPath )
		m_pPreviousKeyFrame->RebuildPath();
	CalculateNextTime();
	CMapAnimator* pAnimator = GetAnimator();
	for ( int i = 0; i < 6; i++ )
	{
		float quaternion[4];
		float time = (float)( m_flNextTime * (double)( i + 1 ) * 0.16666667f );
		Interpolate( this, time, m_Path[i], quaternion,
			pAnimator ? pAnimator->m_nPositionInterpolator : 0,
			pAnimator ? pAnimator->m_nRotationInterpolator : 0 );
	}
}

void CMapKeyFrame::Render3D( CRender3D* pRender )
{
	if ( m_bRebuildPath )
		RebuildPath();
	if ( !m_pNextKeyFrame || m_flMoveSpeed <= 0 || pRender->GetRenderFrame() == (float)m_nRenderFrame )
		return;

	m_nRenderFrame = (int)pRender->GetRenderFrame();
	Vector origin;
	GetOrigin( origin );
	pRender->SetRenderMode( RENDER_MODE_WIREFRAME );
	pRender->BindTexture( NULL );
	glBegin( GL_LINE_STRIP );
	glColor3f( 0, 1, 0 );
	glVertex3fv( &origin[0] );
	for ( int i = 0; i < 6; i++ )
	{
		float red = (float)( (double)( i + 1 ) * 0.16666667f );
		glColor3f( red, 1.0f - red, 0 );
		glVertex3fv( &m_Path[i][0] );
	}
	glEnd();
	pRender->SetRenderMode( RENDER_MODE_DEFAULT );
	if ( m_pPreviousKeyFrame )
		m_pPreviousKeyFrame->Render3D( pRender );
}

//-----------------------------------------------------------------------------
// Purpose: notifies the keyframe that it has been cloned
//			inserts the clone into the correct place in the keyframe list
// Input  : *pClone - 
//-----------------------------------------------------------------------------
void CMapKeyFrame::OnClone( CMapClass* pClone, CMapWorld* pWorld,
	CMapObjectList& OriginalList, CMapObjectList& NewList )
{
	CMapClass::OnClone( pClone, pWorld, OriginalList, NewList );
	CMapKeyFrame* pFrame = dynamic_cast<CMapKeyFrame*>( pClone );
	CMapEntity* pEntity = dynamic_cast<CMapEntity*>( Parent );
	CMapEntity* pNewEntity = pFrame ? dynamic_cast<CMapEntity*>( pFrame->Parent ) : NULL;
	if ( !pEntity || !pNewEntity )
		return;

	GetHistory()->Keep( this );
	// insert the newly created keyframe into the sequence

	// point the clone's next at what we were pointing at
	const char* next = pEntity->GetKeyValue( "NextKey" );
	if ( next )
		pNewEntity->SetKeyValue( "NextKey", next );
	const char* name = pEntity->GetKeyValue( "targetname" );
	// create a new targetname for the clone
	char newName[128];
	CMapEntity::GenerateNewTargetname( name && *name ? name : "keyframe", newName, sizeof( newName ) );
	pNewEntity->SetKeyValue( "targetname", newName );
	// point the current keyframe at the clone
	pEntity->SetKeyValue( "NextKey", newName );
}

void CMapKeyFrame::OnRemoveFromWorld( CMapWorld* pWorld, bool bNotifyChildren )
{
	CMapClass::OnRemoveFromWorld( pWorld, bNotifyChildren );
	CMapEntity* pEntity = dynamic_cast<CMapEntity*>( Parent );
	if ( pEntity && m_pPreviousKeyFrame && m_pNextKeyFrame )
	{
		CMapEntity* pPrevious = dynamic_cast<CMapEntity*>( m_pPreviousKeyFrame->Parent );
		if ( pPrevious )
		{
			GetHistory()->Keep( pPrevious );
			const char* next = pEntity->GetKeyValue( "NextKey" );
			pPrevious->SetKeyValue( "NextKey", next ? next : "" );
		}
	}
	Unlink();
}
