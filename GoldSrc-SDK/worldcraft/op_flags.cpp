//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements the spawnflags page of the Entity Properties dialog.
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "OP_Flags.h"
#include "GameData.h"
#include "ObjectProperties.h"

#define FLAG_CONTROL_COUNT	24
#define FLAG_EDIT_COUNT		12

static const int g_FlagControls[FLAG_CONTROL_COUNT] =
{
	IDC_FLAG0, IDC_FLAG1, IDC_FLAG2, IDC_FLAG3,
	IDC_FLAG4, IDC_FLAG5, IDC_FLAG6, IDC_FLAG7,
	IDC_FLAG8, IDC_FLAG9, IDC_FLAG10, IDC_FLAG11,
	IDC_FLAG12, IDC_FLAG13, IDC_FLAG14, IDC_FLAG15,
	IDC_FLAG16, IDC_FLAG17, IDC_FLAG18, IDC_FLAG19,
	IDC_FLAG20, IDC_FLAG21, IDC_FLAG22, IDC_FLAG23
};

IMPLEMENT_DYNCREATE( COP_Flags, CObjectPage )

BEGIN_MESSAGE_MAP( COP_Flags, CObjectPage )
	ON_BN_CLICKED( IDC_CLEARFLAGS, OnClearFlags )
END_MESSAGE_MAP()

COP_Flags::COP_Flags() : CObjectPage( IDD )
{
	m_pEditObjectRuntimeClass = RUNTIME_CLASS( editCEditGameClass );
	m_pObjectList = NULL;
}

void COP_Flags::UpdateData( int mode, PVOID data )
{
	if ( !IsWindow( m_hWnd ) || !data )
		return;

	CEditGameClass* object = (CEditGameClass*)data;
	if ( mode == SaveFlagsData )
	{
		int flags = object->GetSpawnFlags();
		for ( int i = 0; i < FLAG_EDIT_COUNT; i++ )
		{
			int state = (int)SendDlgItemMessage( g_FlagControls[i], BM_GETCHECK );
			if ( state == BST_UNCHECKED )
				flags &= ~( 1 << i );
			else if ( state == BST_CHECKED )
				flags |= 1 << i;
		}
		object->SetSpawnFlags( flags );
	}
	else if ( mode == LoadFirstData )
	{
		UpdateForClass( object->GetClassName() );
		int flags = object->GetSpawnFlags();
		for ( int i = 0; i < FLAG_EDIT_COUNT; i++ )
		{
			SendDlgItemMessage( g_FlagControls[i], BM_SETSTYLE, BS_AUTOCHECKBOX, TRUE );
			SendDlgItemMessage( g_FlagControls[i], BM_SETCHECK, ( flags & ( 1 << i ) ) != 0 );
		}
	}
	else if ( mode == LoadData )
	{
		int flags = object->GetSpawnFlags();
		for ( int i = 0; i < FLAG_EDIT_COUNT; i++ )
		{
			SendDlgItemMessage( g_FlagControls[i], BM_SETSTYLE, BS_AUTO3STATE, TRUE );
			int state = (int)SendDlgItemMessage( g_FlagControls[i], BM_GETCHECK );
			if ( ( ( flags & ( 1 << i ) ) != 0 ) != state )
				SendDlgItemMessage( g_FlagControls[i], BM_SETCHECK, BST_INDETERMINATE );
		}
	}
}

bool COP_Flags::SaveData()
{
	if ( !IsWindow( m_hWnd ) || !m_pObjectList )
		return false;

	//
	// Apply the dialog data to all the objects being edited.
	//
	POSITION pos = m_pObjectList->GetHeadPosition();
	while ( pos )
	{
		CEditGameClass* object = dynamic_cast<CEditGameClass*>( m_pObjectList->GetNext( pos ) );
		if ( object )
			UpdateData( SaveFlagsData, object );
	}
	return true;
}

void COP_Flags::UpdateForClass( LPCTSTR className )
{
	GDclass* objectClass = pGD->ClassForName( className );
	if ( !IsWindow( m_hWnd ) )
		return;

	GDinputvariable* flags = objectClass ? objectClass->VarForName( "spawnflags" ) : NULL;
	if ( flags )
	{
		for ( int bit = 0; bit < FLAG_CONTROL_COUNT; bit++ )
		{
			const char* caption = "";
			for ( int item = 0; item < flags->m_Items.GetSize(); item++ )
			{
				if ( flags->m_Items[item].iValue == ( 1 << bit ) )
				{
					caption = flags->m_Items[item].szCaption;
					break;
				}
			}
			SetDlgItemText( g_FlagControls[bit], caption );
		}
	}
	else
	{
		for ( int bit = 0; bit < FLAG_EDIT_COUNT; bit++ )
			SetDlgItemText( g_FlagControls[bit], "" );
	}
}

void COP_Flags::OnClearFlags()
{
	for ( int i = 0; i < FLAG_CONTROL_COUNT; i++ )
		SendDlgItemMessage( g_FlagControls[i], BM_SETCHECK, BST_UNCHECKED );
}
