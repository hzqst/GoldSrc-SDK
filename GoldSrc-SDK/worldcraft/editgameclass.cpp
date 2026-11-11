//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: Implements a class that encapsulates much of the functionality
//			of entities. CMapWorld and CMapEntity are both derived from this
//			class.
//
//			CEditGameClass-derived objects have the following properties:
//
//			Key/value pairs - A list of string pairs that hold data properties
//				of the object. Each property has a unique name.
//
//			Connections - A list of outputs in this object that are connected to
//				inputs in another entity.
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "GameData.h"
#include "EditGameClass.h"
#include "Mathlib.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor. Initializes data members.
//-----------------------------------------------------------------------------
CEditGameClass::CEditGameClass(void)
{
	m_pClass = NULL;
	m_szClass[0] = '\0';
	m_nSpawnFlags = 0;
}

CEditGameClass::~CEditGameClass(void)
{
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pszClass -
//			bLoading -
//-----------------------------------------------------------------------------
void CEditGameClass::SetClass(LPCTSTR pszClass, bool bLoading)
{
	extern GameData *pGD;

	lstrcpyn(m_szClass, pszClass, sizeof(m_szClass));
	int iLen = strlen(m_szClass) - 1;

	while ((iLen >= 0) && (m_szClass[iLen] == ' '))
	{
		m_szClass[iLen--] = '\0';
	}

	if (pGD)
	{
		m_pClass = pGD->ClassForName(m_szClass);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copies the data from a given CEditGameClass object into this one.
// Input  : pFrom - Object to copy.
// Output : Returns a pointer to this.
//-----------------------------------------------------------------------------
CEditGameClass *CEditGameClass::CopyFrom(CEditGameClass *pFrom)
{
	m_nSpawnFlags = pFrom->m_nSpawnFlags;
	m_pClass = pFrom->m_pClass;
	strcpy( m_szClass, pFrom->m_szClass );

	//
	// Copy all the keys.
	//
	m_KeyValues.RemoveAll();
	int nKeyValues = pFrom->GetKeyValueCount();
	for (int i = 0; i < nKeyValues; i++)
	{
		m_KeyValues.SetValue(pFrom->GetKey(i), pFrom->GetKeyValue(i));
	}
	ASSERT(m_KeyValues.GetCount() == nKeyValues);

	return(this);
}

//-----------------------------------------------------------------------------
// Purpose: Applies the default keys for this object's game class. Called when
//			the entity's class is changed.
//-----------------------------------------------------------------------------
void CEditGameClass::GetDefaultKeys( void )
{
	if ( m_pClass != NULL )
	{
		//
		// For each variable from the base class...
		//
		int nVariableCount = m_pClass->GetVariableCount();
		for ( int i = 0; i < nVariableCount; i++ )
		{
			GDinputvariable *pVar = m_pClass->GetVariableAt(i);
			ASSERT(pVar != NULL);

			if (pVar != NULL)
			{
				int iIndex;
				LPCTSTR p = m_KeyValues.GetValue(pVar->GetName(), &iIndex);

				//
				// If the variable is not present in this object, set the default value.
				//
				if ( p == NULL )
				{
					MDkeyvalue tmpkv;
					pVar->ResetDefaults();
					pVar->ToKeyValue(&tmpkv);

					//
					// Only set the key value if it is non-zero.
					//
					if (stricmp(tmpkv.szKey, "spawnflags") && (tmpkv.szKey[0] != 0) && (tmpkv.szValue[0] != 0) &&
						(stricmp(tmpkv.szValue, "0")))
					{
						SetKeyValue(tmpkv.szKey, tmpkv.szValue);
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns this object's angles as a vector of the form:
//			[0] PITCH [1] YAW [2] ROLL
//-----------------------------------------------------------------------------
void CEditGameClass::GetAngles(QAngle &vecAngles)
{
	vecAngles = vec3_origin;
	const char *pszAngles = GetKeyValue("angles");
	if (pszAngles != NULL)
	{
		sscanf(pszAngles, "%f %f %f", &vecAngles[PITCH], &vecAngles[YAW], &vecAngles[ROLL]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets this object's angles as a vector of the form:
//			[0] PITCH [1] YAW [2] ROLL
//-----------------------------------------------------------------------------
void CEditGameClass::SetAngles(const QAngle &vecAngles)
{
	char szAngles[80];
	sprintf(szAngles, "%g %g %g", (double)vecAngles[PITCH], (double)vecAngles[YAW], (double)vecAngles[ROLL]);
	SetKeyValue("angles", szAngles);
}

//-----------------------------------------------------------------------------
// Purpose: Returns all the spawnflags.
//-----------------------------------------------------------------------------
int CEditGameClass::GetSpawnFlags(void)
{
	return(m_nSpawnFlags);
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if a given spawnflag (or flags) is set, false if not.
//-----------------------------------------------------------------------------
bool CEditGameClass::GetSpawnFlag(int nFlags)
{
	int nSpawnFlags = GetSpawnFlags();
	return((nSpawnFlags & nFlags) != 0);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the given spawnflag (or flags) to the given state.
// Input  : nFlag - Flag values  to set or clear (ie 1, 2, 4, 8, 16, etc.)
//			bSet - True to set the flags, false to clear them.
//-----------------------------------------------------------------------------
void CEditGameClass::SetSpawnFlag(int nFlags, bool bSet)
{
	int nSpawnFlags = GetSpawnFlags();

	if (bSet)
	{
		nSpawnFlags |= nFlags;
	}
	else
	{
		nSpawnFlags &= ~nFlags;
	}

	SetSpawnFlags(nSpawnFlags);
}

//-----------------------------------------------------------------------------
// Purpose: Sets all the spawnflags at once.
// Input  : nSpawnFlags - New value for spawnflags.
//-----------------------------------------------------------------------------
void CEditGameClass::SetSpawnFlags(int nSpawnFlags)
{
	m_nSpawnFlags = nSpawnFlags;
}
