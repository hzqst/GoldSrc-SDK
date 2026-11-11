//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "vgui/IVGui.h"

namespace vgui2
{
class Panel;

class PHandle
{
public:
	PHandle();

	Panel* Get();
	Panel* Set( Panel* panel );

	operator Panel*();
	Panel* operator->()
	{
		return Get();
	}
	Panel* operator=( Panel* panel );
	bool operator==( Panel* panel );
	operator bool()
	{
		return Get() != NULL;
	}

private:
	HPanel m_iPanelID;
};

//-----------------------------------------------------------------------------
// Purpose: DHANDLE is a templated version of PHandle
//-----------------------------------------------------------------------------
template <class T> class DHANDLE : public PHandle
{
public:
	T* Get() { return (T*)PHandle::Get(); }
	T* Set( T* panel ) { return (T*)PHandle::Set( panel ); }
	operator T*() { return Get(); }
	T* operator->() { return Get(); }
	T* operator=( T* panel ) { return Set( panel ); }
	bool operator==( T* panel ) { return Get() == panel; }
};

//-----------------------------------------------------------------------------
// Purpose: Safe pointer class to just convert between VPANEL's and PHandle
//-----------------------------------------------------------------------------
class VPanelHandle
{
public:
	VPanelHandle() : m_iPanelID( (HPanel)-1 )
	{
	}

	VPANEL Get();
	VPANEL Set( VPANEL panel );

	operator VPANEL();
	VPANEL operator=( VPANEL panel );
	bool operator==( VPANEL panel );
	operator bool();

private:
	HPanel m_iPanelID;
};
}
