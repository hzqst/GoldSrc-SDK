//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "stdafx.h"
#include "GameData.h"
#include "IEditorTexture.h"
#include "GlobalFunctions.h"
#include "worldcraft_mathlib.h"
#include "HelperFactory.h"
#include "MapAlignedBox.h"
#include "MapAnimator.h"
#include "MapDefs.h"
#include "MapDecal.h"
#include "History.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapSolid.h"
#include "MapStudioModel.h"
#include "MapWorld.h"
#include "Options.h"
#include "Render2D.h"
#include "VisGroup.h"

IMPLEMENT_MAPCLASS( CMapEntity )

bool CMapEntity::s_bShowEntityNames = true;
bool CMapEntity::s_bShowEntityConnections = false;

static CMapObjectList FoundEntities;

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pEntity -
//			pKV -
// Output :
//-----------------------------------------------------------------------------
static BOOL FindKeyValue(CMapEntity *pEntity, MDkeyvalue *pKV)
{
	LPCTSTR pszValue = pEntity->GetKeyValue(pKV->szKey);
	if (!pszValue || strcmpi(pszValue, pKV->szValue))
	{
		return TRUE;
	}

	FoundEntities.AddTail(pEntity);

	return TRUE;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapEntity::CMapEntity(void) : flags(0)
{
	m_pMoveParent = NULL;
	m_pAnimator = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapEntity::~CMapEntity(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: Adds a bounding box helper to this entity. If this entity's class
//			specifies a bounding box, it will be the correct size.
// Input  : pClass -
//-----------------------------------------------------------------------------
void CMapEntity::AddBoundBoxForClass(GDclass *pClass, bool bLoading)
{
	Vector Mins;
	Vector Maxs;

	//
	// If we have a class and it specifies a class, use that bounding box.
	//
	if ((pClass != NULL) && (pClass->HasBoundBox()))
	{
		pClass->GetBoundBox(Mins, Maxs);
	}
	//
	// Otherwise, use a default bounding box.
	//
	else
	{
		VectorFill(Mins, -8);
		VectorFill(Maxs, 8);
	}

	//
	// Create the box and add it as one of our children.
	//
	CMapAlignedBox *pBox = new CMapAlignedBox(Mins, Maxs);
	pBox->SetOrigin(m_Origin);

	pBox->SetShouldSerialize(FALSE);
	pBox->SetSelectionState(GetSelectionState());

	//
	// HACK: Make sure that the new child gets properly linked into the world.
	//		 This is not correct because it bypasses the doc's AddObjectToWorld code.
	//
	// Don't call AddObjectToWorld during VMF load because we don't want to call
	// OnAddToWorld during VMF load. We update our helpers during PostloadWorld.
	//
	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	if ((!bLoading) && (pWorld != NULL))
	{
		pWorld->AddObjectToWorld(pBox, this);
	}
	else
	{
		AddChild(pBox);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets our child's render color to our render color.
// Input  : pChild - Child object being added.
//-----------------------------------------------------------------------------
void CMapEntity::AddChild(CMapClass *pChild)
{
	//
	// If we have a game data class, set the child's render color to the color
	// dictated by the game data class.
	//
	if (IsClass())
	{
		GDclass *pClass = GetClass();
		color32 rgbColor = pClass->GetColor();
		pChild->SetRenderColor(rgbColor);
	}
	//
	// If no class, set the color to the default entity color (purple).
	//
	else
	{
		pChild->SetRenderColor(220, 0, 180);
	}

	CMapClass::AddChild(pChild);
	CMapAnimator* pAnimator = dynamic_cast<CMapAnimator*>( pChild );
	if ( pAnimator )
		m_pAnimator = pAnimator;

	//
	// Notify the new child of all our keys. Don't bother for solids.
	//
	if (dynamic_cast<CMapSolid*>(pChild) == NULL)
	{
		int nNumKeys = GetKeyValueCount();
		for (int i = 0; i < nNumKeys; i++)
		{
			MDkeyvalue KeyValue = m_KeyValues.GetKeyValue( i );
			pChild->OnParentKeyChanged( KeyValue.szKey, KeyValue.szValue );
		}
	}
}

void CMapEntity::RemoveChild( CMapClass* pChild, bool bUpdateBounds )
{
	if ( pChild == m_pAnimator )
		m_pAnimator = NULL;
	CMapClass::RemoveChild( pChild, bUpdateBounds );
}

void CMapEntity::RemoveAllChildren()
{
	m_pAnimator = NULL;
	CMapClass::RemoveAllChildren();
}

void CMapEntity::OnPreDelete()
{
	CMapWorld* pWorld = GetWorldObject( this );
	POSITION pos = Children.GetHeadPosition();
	while ( pos )
	{
		CMapDecal* pDecal = dynamic_cast<CMapDecal*>( Children.GetNext( pos ) );
		if ( pDecal )
			pDecal->OnRemoveFromWorld( pWorld, true );
	}
}

void CMapEntity::OnUndoRedo()
{
	UpdateHelpers( false );
	UpdateAllDependencies( this );
	CalcBounds( TRUE );
}

//-----------------------------------------------------------------------------
// Purpose: Adds a helper object as a child of this entity.
// Input  : pHelper - The helper object.
//			bLoading - True if this is being called from Postload, false otherwise.
//-----------------------------------------------------------------------------
void CMapEntity::AddHelper(CMapClass *pHelper, bool bLoading)
{
	if (!IsPlaceholder())
	{
		//
		// Solid entities have no origin, so place the helper at our center.
		//
		Vector vecCenter;
		m_Render2DBox.GetBoundsCenter(vecCenter);
		pHelper->SetOrigin(vecCenter);
	}
	else
	{
		pHelper->SetOrigin(m_Origin);
	}

	pHelper->SetShouldSerialize(FALSE);
	pHelper->SetSelectionState(GetSelectionState());

	//
	// HACK: Make sure that the new child gets properly linked into the world.
	//		 This is not correct because it bypasses the doc's AddObjectToWorld code.
	//
	// Don't call AddObjectToWorld during VMF load because we don't want to call
	// OnAddToWorld during VMF load. We update our helpers during PostloadWorld.
	//
	CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
	if ((!bLoading) && (pWorld != NULL))
	{
		pWorld->AddObjectToWorld(pHelper, this);
	}
	else
	{
		AddChild(pHelper);
	}

}

//-----------------------------------------------------------------------------
// Purpose: Creates all helper objects defined by the FGD and adds them as
//			children of this entity. Helper objects perform rendering, UI, and
//			bookkeeping functions for their parent entities. If the class
//			definition does not specify any helpers, or none of the helpers
//			could be added, a box helper is added so that the entity has some
//			visual representation.
// Inputs : pClass -
//			bLoading - True if this is being called from Postload, false otherwise.
//-----------------------------------------------------------------------------
void CMapEntity::AddHelpersForClass( GDclass* pClass, bool bLoading )
{
	bool bAddedVisual = false;
	if ( pClass )
	{
		POSITION pos;
		//
		// For every helper in the class definition...
		//
		CHelperInfo* pInfo = pClass->GetFirstHelper( pos );
		while ( pInfo )
		{
			//
			// Create the helper and attach it to this entity.
			//
			CMapClass* pHelper = CHelperFactory::CreateHelper( pInfo, this );
			if ( pHelper )
			{
				AddHelper( pHelper, bLoading );
				if ( pHelper->IsVisualElement() )
					bAddedVisual = true;
			}
			pInfo = pClass->GetNextHelper( pos );
		}
	}
	if ( !bAddedVisual )
		AddBoundBoxForClass( pClass, bLoading );
	CalcBounds( TRUE );
	PostUpdate( Notify_Changed );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a deep copy of this object.
// Output : Returns a pointer to the new allocated object.
//-----------------------------------------------------------------------------
CMapClass *CMapEntity::Copy(bool bUpdateDependencies)
{
	CMapEntity *pNew = new CMapEntity;
	pNew->CopyFrom(this, bUpdateDependencies);
	return pNew;
}

//-----------------------------------------------------------------------------
// Purpose: Performs a deep copy of a given object into this object.
// Input  : pobj - Object to copy from.
// Output : Returns a pointer to this object.
//-----------------------------------------------------------------------------
CMapClass *CMapEntity::CopyFrom(CMapClass *pobj, bool bUpdateDependencies)
{
	ASSERT(pobj->IsMapClass(MAPCLASS_TYPE(CMapEntity)));
	CMapEntity *pFrom = (CMapEntity*) pobj;

	flags = pFrom->flags;

	m_Origin = pFrom->m_Origin;

	CMapClass::CopyFrom(pobj, bUpdateDependencies);

	//
	// Copy our keys. If our targetname changed we must relink all targetname pointers.
	//
	const char *pszOldTargetName = CEditGameClass::GetKeyValue("targetname");
	char szOldTargetName[MAX_IO_NAME_LEN] = {};
	if (pszOldTargetName != NULL)
	{
		strcpy(szOldTargetName, pszOldTargetName);
	}

	CEditGameClass::CopyFrom(pFrom);
	const char *pszNewTargetName = CEditGameClass::GetKeyValue("targetname");

	if ((bUpdateDependencies) && (pszNewTargetName != NULL))
	{
		if (stricmp(szOldTargetName, pszNewTargetName) != 0)
		{
			UpdateAllDependencies(this);
		}
	}

	return(this);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : bFullUpdate -
//-----------------------------------------------------------------------------
void CMapEntity::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);
}

//-----------------------------------------------------------------------------
// Purpose: Debugging hook.
//-----------------------------------------------------------------------------
void CMapEntity::Debug(void)
{
	int nKeyValues = m_KeyValues.GetCount();
	for (int i = 0; i < nKeyValues; i++)
	{
		MDkeyvalue &KeyValue = m_KeyValues.GetKeyValue(i);
	}
}

//-----------------------------------------------------------------------------
// Purpose: If this entity has a name key, returns a string with "<name> <classname>"
//			in it. Otherwise returns a buffer with "<classname>" in it.
// Output : String description of the entity.
//-----------------------------------------------------------------------------
LPCTSTR CMapEntity::GetDescription(void)
{
	static char szBuf[128];
	const char *pszName = GetKeyValue("targetname");

	if (pszName != NULL)
	{
		sprintf(szBuf, "%s - %s", pszName, GetClassName());
	}
	else
	{
		strcpy(szBuf, GetClassName());
	}

	return(szBuf);
}

//-----------------------------------------------------------------------------
// Purpose: Returns the color that this entity should use for rendering.
//-----------------------------------------------------------------------------
void CMapEntity::GetRenderColor(unsigned char &red, unsigned char &green, unsigned char &blue)
{
	if (IsSelected())
	{
		red = GetRValue(Options.colors.clrSelection);
		green = GetGValue(Options.colors.clrSelection);
		blue = GetBValue(Options.colors.clrSelection);
	}
	else
	{
		GDclass *pClass = GetClass();
		if (pClass)
		{
			color32 rgbColor = pClass->GetColor();

			red = rgbColor.r;
			green = rgbColor.g;
			blue = rgbColor.b;
		}
		else
		{
			red = GetRValue(Options.colors.clrEntity);
			green = GetGValue(Options.colors.clrEntity);
			blue = GetBValue(Options.colors.clrEntity);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the color that this entity should use for rendering.
//-----------------------------------------------------------------------------
color32 CMapEntity::GetRenderColor(void)
{
	color32 clr;

	GetRenderColor(clr.r, clr.g, clr.b);
	return clr;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the size of this object.
// Output : Size, in bytes, of this object, not including any dynamically
//			allocated data members.
//-----------------------------------------------------------------------------
size_t CMapEntity::GetSize(void)
{
	return(sizeof(*this));
}

//-----------------------------------------------------------------------------
// Purpose: Sets this entity's origin and updates the bounding box.
// Input  : o - Origin to set.
//-----------------------------------------------------------------------------
void CMapEntity::SetOrigin(Vector& o)
{
	CMapClass::SetOrigin(o);

	// dvs: is this still necessary?
	if (!(flags & flagPlaceholder))
	{
		// not a placeholder.. no origin.
		return;
	}

	CalcBounds( TRUE );
	PostUpdate(Notify_Changed);
}

//-----------------------------------------------------------------------------
// Purpose: Removes all of this entity's helpers.
// Input  : bRemoveSolidChildren - Whether to also remove any solid children. This
//			is true when changing from a solid entity to a point entity.
//-----------------------------------------------------------------------------
void CMapEntity::RemoveHelpers(bool bRemoveSolids)
{
	m_pAnimator = NULL;
	OnPreDelete();
	POSITION pos = Children.GetHeadPosition();
	while (pos != NULL)
	{
		CMapClass *pChild = Children.GetNext(pos);
		if ( !pChild )
			continue;
		if (bRemoveSolids || ((dynamic_cast <CMapSolid *> (pChild)) == NULL))
		{
			bool retained = pChild->ShouldSerialize() && !pChild->IsTemporary() && !GetHistory()->IsPaused();
			if ( retained )
				GetHistory()->KeepForDestruction( pChild );
			RemoveChild( pChild, false );
			if ( !retained )
				delete pChild;
		}
	// LEAKLEAK: need to KeepForDestruction to avoid undo crashes, but how? where?
	//delete pChild;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *szOldName -
//			*szNewName -
//-----------------------------------------------------------------------------
void CMapEntity::ReplaceTargetname(const char *szOldName, const char *szNewName)
{
	//
	// Replace any keys whose value matches the old name.
	//
	int nNumKeys = GetKeyValueCount();
	for (int i = 0; i < nNumKeys; i++)
	{
		MDkeyvalue KeyValue = m_KeyValues.GetKeyValue(i);
		if (!stricmp(KeyValue.szValue, szOldName))
		{
			SetKeyValue(KeyValue.szKey, szNewName);
		}
	}

	CMapClass::ReplaceTargetname(szOldName, szNewName);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Inputs : pszClass -
//			bLoading - True if this is being called from Postload, false otherwise.
//-----------------------------------------------------------------------------
void CMapEntity::SetClass(LPCTSTR pszClass, bool bLoading)
{
	ASSERT(pszClass);
	KeepForUndo();

	if (!strcmp(pszClass, "infodecal") && !GetKeyValue("texture"))
	{
		SetKeyValue("texture", "clip");
	}

	//
	// If we are just setting to the same class, don't do anything.
	//
	if (IsClass(pszClass))
	{
		return;
	}

	if (IsPlaceholder())
	{
		RemoveHelpers(true);
	}

	//
	// Copy class name & resolve GDclass pointer.
	//
	CEditGameClass::SetClass(pszClass, bLoading);

	if (IsClass() && !IsSolidClass())
	{
		SetPlaceholder(TRUE);
		CalcBounds();
	}

	if (IsClass())
	{
		color32 rgbColor = m_pClass->GetColor();
		SetRenderColor(rgbColor);
	}

	GetDefaultKeys();

	if (IsPlaceholder())
	{
		AddHelpersForClass(m_pClass, bLoading);
	}
}

void CMapEntity::PostloadEntity(void)
{
	int nIndex;

	const char *pszValue = m_KeyValues.GetValue("origin", &nIndex);
	if (pszValue != NULL)
	{
		Vector Origin;
		sscanf(pszValue, "%f %f %f", &Origin[0], &Origin[1], &Origin[2]);
		SetOrigin(Origin);
		RemoveKey(nIndex);
	}

	pszValue = m_KeyValues.GetValue("classname", &nIndex);
	if (pszValue != NULL)
	{
		char szClassName[MAX_CLASS_NAME_LEN];
		lstrcpyn(szClassName, pszValue, sizeof(szClassName));
		SetClass(szClassName, true);

		pszValue = m_KeyValues.GetValue("classname", &nIndex);
		if (pszValue != NULL)
		{
			RemoveKey(nIndex);
		}
	}

	if (IsClass() && !IsSolidClass())
	{
		AddHelpersForClass(m_pClass, true);
	}
}

void CMapEntity::SetAnglesFromModel(void)
{
	POSITION pos = Children.GetHeadPosition();
	while (pos != NULL)
	{
		CMapStudioModel *pModel = dynamic_cast<CMapStudioModel *>(Children.GetNext(pos));
		if (pModel != NULL)
		{
			QAngle vecAngles;
			pModel->GetAngles(vecAngles);

			char szAngles[40];
			sprintf(szAngles, "%.0f %.0f %.0f", (double)vecAngles[0], (double)vecAngles[1], (double)vecAngles[2]);
			m_KeyValues.SetValue("angles", szAngles);
			return;
		}
	}
}

void CMapEntity::ImportAngle(int nAngle)
{
	double flPitch = 0;
	float flYaw = 0;

	if (nAngle == -1)
	{
		flPitch = -90;
	}
	else if (nAngle == -2)
	{
		flPitch = 90;
	}
	else
	{
		flYaw = (float)nAngle;
	}

	char szAngles[80];
	int nPitch = (int)flPitch;
	int nYaw = (int)flYaw;
	if ((nPitch == flPitch) && (nYaw == flYaw))
	{
		sprintf(szAngles, "%d %d %d", nPitch, nYaw, 0);
	}
	else
	{
		sprintf(szAngles, "%.2f %.2f %.2f", flPitch, (double)flYaw, 0.0);
	}

	SetKeyValue("angles", szAngles);
}

//-----------------------------------------------------------------------------
// Purpose: Called after the entire map has been loaded. This allows the object
//			to perform any linking with other map objects or to do other operations
//			that require all world objects to be present.
//-----------------------------------------------------------------------------
void CMapEntity::PostloadWorld(CMapWorld *pWorld)
{
	//
	// Call in all our children (some of which were created above).
	//
	CMapClass::PostloadWorld(pWorld);

	CalcBounds();
}

//-----------------------------------------------------------------------------
// Purpose: Insures that the entity has all the helpers that it needs (and no more
//			than it should) given its class.
//-----------------------------------------------------------------------------
void CMapEntity::UpdateHelpers(bool bLoading)
{
	//
	// If we have any helpers, delete them. Delete any solid children if we are
	// a point class.
	//
	RemoveHelpers(IsPlaceholder() == TRUE);

	//
	// Add the helpers appropriate for our current class.
	//
	AddHelpersForClass(GetClass(), bLoading);
}

//-----------------------------------------------------------------------------
// Purpose: increments the numerals at the end of a string
//			appends 0 if no numerals exist
// Input  : newName -
//-----------------------------------------------------------------------------
void IncrementStringName( CString &str )
{
	// walk backwards through the string looking for where the digits stop
	int pos = str.GetLength();
	while ( isdigit(str.GetAt(pos-1)) )
	{
		pos--;
	}

	// if no digits found, append a "0"
	if ( pos == str.GetLength() )
	{
		str += "0";
	}
	else
	{
		// get the number
		CString sNum = str.Right( str.GetLength() - pos );
		int iNum = atoi( sNum );

		// increment the number
		iNum++;

		// change the number in the string
		CString newStr;
		newStr.Format( "%s%d", (const char*)str.Left(pos), iNum );
		str = newStr;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Generates a new, unique targetname
//			a static function
// Input  : char *startName -
//			*outputName -
//			newNameBufferSize -
//-----------------------------------------------------------------------------
void CMapEntity::GenerateNewTargetname( const char *startName, char *outputName, int newNameBufferSize )
{
	outputName[0] = 0;
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	CMapWorld *pWorld = pDoc->GetMapWorld();
	if ( !pWorld )
		return;

	// find the start name
	CString newName( startName );
	if ( newName.IsEmpty() )
	{
		newName = "entity";
	}

	// try to find entities that match the name
	CMapEntity *pEnt = NULL;
	do
	{
		// increment the entity name
		IncrementStringName( newName );

		pEnt = pWorld->FindChildByKeyValue( "targetname", newName );
	}
	while ( pEnt );

	strncpy( outputName, newName, newNameBufferSize );
	outputName[newNameBufferSize-1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Allows the entity to update its key values based on a change in one
//			of its children. The child exposes the property as a key value pair.
// Input  : pChild - The child whose property changed.
//			szKey - The name of the property that changed.
//			szValue - The new value of the property.
//-----------------------------------------------------------------------------
void CMapEntity::NotifyChildKeyChanged(CMapClass *pChild, const char *szKey, const char *szValue)
{
	m_KeyValues.SetValue(szKey, szValue);

	//
	// Notify all our other non-solid children that a key has changed.
	//
	POSITION pos = Children.GetHeadPosition();
	while (pos != NULL)
	{
		CMapClass *pObject = Children.GetNext(pos);
		if ((pObject != pChild) && (pChild != NULL) && (dynamic_cast<CMapSolid *>(pObject) == NULL))
		{
			pObject->OnParentKeyChanged(szKey, szValue);
		}
	}

	CalcBounds();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapEntity::DeleteKeyValue(LPCSTR pszKey)
{
	char szOldValue[KEYVALUE_MAX_VALUE_LENGTH];
	const char *pszOld = GetKeyValue(pszKey);
	if (pszOld != NULL)
	{
		strcpy(szOldValue, pszOld);
	}
	else
	{
	  szOldValue[0] = '\0';
	}

	CEditGameClass::DeleteKeyValue(pszKey);

	OnKeyValueChanged(pszKey, szOldValue, "");
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapEntity::SetKeyValue(LPCSTR pszKey, LPCSTR pszValue)
{
	//
	// Get the current value so we can tell if it is changing.
	//
	char szOldValue[KEYVALUE_MAX_VALUE_LENGTH];
	const char *pszOld = GetKeyValue(pszKey);
	if (pszOld != NULL)
	{
		strcpy(szOldValue, pszOld);
	}
	else
	{
	  szOldValue[0] = '\0';
	}

	CEditGameClass::SetKeyValue(pszKey, pszValue);

	OnKeyValueChanged(pszKey, szOldValue, pszValue);
}

//-----------------------------------------------------------------------------
// Purpose: Notifies the entity that it has been cloned.
// Input  : pClone -
//-----------------------------------------------------------------------------
void CMapEntity::OnPreClone(CMapClass *pClone, CMapWorld *pWorld, CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	CMapClass::OnPreClone(pClone, pWorld, OriginalList, NewList);

	if (OriginalList.GetCount() == 1)
	{
		// dvs: TODO: make this FGD-driven instead of hardcoded, see also MapKeyFrame.cpp
		// dvs: TODO: use letters of the alphabet between adjacent numbers, ie path2a path2b, etc.
		if (!stricmp(GetClassName(), "path_corner") || !stricmp(GetClassName(), "path_track"))
		{
			//
			// Generate a new name for the clone.
			//
			CMapEntity *pNewEntity = dynamic_cast<CMapEntity*>(pClone);
			ASSERT(pNewEntity != NULL);
			if (!pNewEntity)
				return;

			// create a new targetname for the clone
			char newName[128];
			const char *oldName = GetKeyValue("targetname");
			if (!oldName || oldName[0] == 0)
				oldName = "path";

			CMapEntity::GenerateNewTargetname(oldName, newName, 127);
			pNewEntity->SetKeyValue("targetname", newName);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pClone -
//			pWorld -
//			OriginalList -
//			NewList -
//-----------------------------------------------------------------------------
void CMapEntity::OnClone(CMapClass *pClone, CMapWorld *pWorld, CMapObjectList &OriginalList, CMapObjectList &NewList)
{
	CMapClass::OnClone(pClone, pWorld, OriginalList, NewList);

	if (OriginalList.GetCount() == 1)
	{
		if (!stricmp(GetClassName(), "path_corner") || !stricmp(GetClassName(), "path_track"))
		{
			// dvs: TODO: make this FGD-driven instead of hardcoded, see also MapKeyFrame.cpp
			// dvs: TODO: use letters of the alphabet between adjacent numbers, ie path2a path2b, etc.
			CMapEntity *pNewEntity = dynamic_cast<CMapEntity*>(pClone);
			ASSERT(pNewEntity != NULL);
			if (!pNewEntity)
				return;

			// Point the clone at what we were pointing at.
			const char *pszNext = GetKeyValue("target");
			if (pszNext)
			{
				pNewEntity->SetKeyValue("target", pszNext);
			}

			// Point this path corner at the clone.
			SetKeyValue("target", pNewEntity->GetKeyValue("targetname"));
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pszKey -
//			pszOldValue -
//			pszValue -
//-----------------------------------------------------------------------------
void CMapEntity::OnKeyValueChanged(const char *pszKey, const char *pszOldValue, const char *pszValue)
{
	//
	// notify all our children that a key has changed
	//
	POSITION pos = Children.GetHeadPosition();
	while ( pos != NULL )
	{
		CMapClass *pChild = Children.GetNext( pos );
		if ( pChild != NULL )
		{
			pChild->OnParentKeyChanged( pszKey, pszValue );
		}
	}

	//
	// Changing our movement parent. Store a pointer to the movement parent
	// for when we're playing animations.
	//
	if ( !stricmp(pszKey, "parentname") )
	{
		CMapWorld *pWorld = (CMapWorld *)GetWorldObject( this );
		if (pWorld != NULL)
		{
			m_pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, pWorld->FindChildByKeyValue( "targetname", pszValue));
		}
	}
	//
	// Changing our model - rebuild the helpers from scratch.
	// dvs: this could probably go away - move support into the helper code.
	//
	else if (!stricmp(pszKey, "model"))
	{
		if (stricmp(pszOldValue, pszValue) != 0)
		{
			// We don't call SetKeyValue during VMF load.
			UpdateHelpers(false);
		}
	}
	//
	// If our targetname has changed, we have to relink EVERYTHING, not
	// just our dependents, because someone else may point to our new targetname.
	//
	else if (!stricmp(pszKey, "targetname") && (stricmp(pszOldValue, pszValue) != 0))
	{
		UpdateAllDependencies(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if this entity has any solid children. Entities of
//			classes that are not in the FGD are considered solid entities if
//			they have at least one solid child, point entities if not.
//-----------------------------------------------------------------------------
bool CMapEntity::HasSolidChildren(void)
{
	POSITION pos = Children.GetHeadPosition();
	while (pos != NULL)
	{
		CMapClass *pChild = Children.GetNext(pos);
		if ((dynamic_cast <CMapSolid *> (pChild)) != NULL)
		{
			return(true);
		}
	}

	return(false);
}

//-----------------------------------------------------------------------------
// Purpose: Called after this object is added to the world.
//
//			NOTE: This function is NOT called during serialization. Use PostloadWorld
//				  to do similar bookkeeping after map load.
//
// Input  : pWorld - The world that we have been added to.
//-----------------------------------------------------------------------------
void CMapEntity::OnAddToWorld(CMapWorld *pWorld)
{
	CMapClass::OnAddToWorld(pWorld);

	//
	// If we have a targetname, relink all the targetname pointers in the world
	// because someone might be looking for our targetname.
	//
	if (GetKeyValue("targetname") != NULL)
	{
		UpdateAllDependencies(this);
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pObject - The object that changed.
//-----------------------------------------------------------------------------
void CMapEntity::OnNotifyDependent(CMapClass *pObject, Notify_Dependent_t eNotifyType)
{
	CMapClass::OnNotifyDependent(pObject, eNotifyType);

	if (eNotifyType == Notify_Removed)
	{
		//
		// Check for our move parent going away.
		//
		if (pObject == m_pMoveParent)
		{
			CMapWorld *pWorld = (CMapWorld *)GetWorldObject(this);
			const char *pszParentName = CEditGameClass::GetKeyValue("parentname");
			if ((pWorld != NULL) && (pszParentName != NULL))
			{
				m_pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, pWorld->FindChildByKeyValue( "targetname", pszParentName));
			}
			else
			{
				m_pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, NULL);
			}
		}
	}
}

void CMapEntity::OnRemoveFromWorld( CMapWorld* pWorld, bool bNotifyChildren )
{
	m_pMoveParent = (CMapEntity*)UpdateDependency( m_pMoveParent, NULL );
	CMapClass::OnRemoveFromWorld( pWorld, bNotifyChildren );
}

//-----------------------------------------------------------------------------
// Purpose: Iterates through an object, and all it's children, looking for an
//			entity with a matching key and value
// Input  : key -
//			value -
// Output : Returns a pointer to the entity found.
//-----------------------------------------------------------------------------
CMapEntity *CMapEntity::FindChildByKeyValue( LPCSTR key, LPCSTR value )
{
	if ((key == NULL) || (value == NULL))
	{
		return(NULL);
	}

	int index;
	LPCSTR val = CEditGameClass::GetKeyValue(key, &index);

	if ( val && value && !stricmp(value, val) )
	{
		return this;
	}

	return CMapClass::FindChildByKeyValue( key, value );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a coordinate frame to render in, if the entity is animating
// Input  : matrix -
// Output : returns true if a new matrix is returned, false if it is just the identity
//-----------------------------------------------------------------------------
bool CMapEntity::GetTransformMatrix( matrix4_t& matrix )
{
	bool gotMatrix = false;
	if ( m_pMoveParent )
		gotMatrix = m_pMoveParent->GetTransformMatrix( matrix );
	if ( !m_pAnimator )
		return gotMatrix;
	if ( !gotMatrix )
		return m_pAnimator->GetTransformMatrix( matrix );

	matrix4_t animation, combined;
	if ( m_pAnimator->GetTransformMatrix( animation ) )
	{
		MultiplyMatrix( matrix, animation, combined );
		matrix = combined;
	}
	return gotMatrix;
}

//-----------------------------------------------------------------------------
// Purpose: Overloaded to use the color from our FGD definition.
//-----------------------------------------------------------------------------
void CMapEntity::SetColorFromVisGroups(void)
{
	if (m_pVisGroup != NULL)
	{
		//
		// Set our color to our visgroup color.
		//
		color32 rgbColor = m_pVisGroup->GetColor();
		SetRenderColor(rgbColor);
	}
	else
	{
		//
		// Set our color based on our FGD definition.
		//
		GDclass *pClass = GetClass();
		if (IsPlaceholder() && pClass)
		{
			color32 rgbColor = pClass->GetColor();
			SetRenderColor(rgbColor);
		}
		else
		{
			SetRenderColor(220, 0, 180);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pWorld -
//			pObject -
//-----------------------------------------------------------------------------
void CMapEntity::UpdateDependencies(CMapWorld *pWorld, CMapClass *pObject)
{
	CMapClass::UpdateDependencies(pWorld, pObject);

	//
	// If we have a movement parent, relink to our movement parent.
	//
	const char *pszParentName = CEditGameClass::GetKeyValue("parentname");
	if (pszParentName != NULL)
	{
		m_pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, pWorld->FindChildByKeyValue( "targetname", pszParentName));
	}
	else
	{
		m_pMoveParent = (CMapEntity *)UpdateDependency(m_pMoveParent, NULL);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Places the entity properly on a plane surface, at a given location
// Input:	pos - position on the plane
//			plane - surface plane to align to
//			align - alignment type (top, bottom)
// Output:
//-----------------------------------------------------------------------------

#define	ALIGN_EPSILON	1	// World units

void CMapEntity::AlignOnPlane( Vector& pos, PLANE *plane, alignType_e align )
{
	float	fHeight = m_Render2DBox.bmaxs[2] - m_Render2DBox.bmins[2];
	float	fOffset;
	Vector	vecNewPos;

	//Depending on the alignment type, get the offset from the surface
	switch ( align )
	{
	case ALIGN_BOTTOM:
		fOffset = m_Origin[2] - m_Render2DBox.bmins[2];
		break;

	case ALIGN_TOP:
		fOffset = m_Render2DBox.bmaxs[2] - m_Origin[2];
		break;
	}

	//Push our point out and away from this surface
	VectorMA( pos, fOffset + ALIGN_EPSILON, plane->normal, vecNewPos );

	//Update the entity and children
	SetOrigin( vecNewPos );
}



//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the object that should be added to the selection
//			list because this object was clicked on with a given selection mode.
// Input  : eSelectMode -
//-----------------------------------------------------------------------------
CMapClass *CMapEntity::PrepareSelection(SelectMode_t eSelectMode)
{
	//
	// Select up the hierarchy when in Groups selection mode if we belong to a group.
	//
	if ((eSelectMode == selectGroups) && (Parent != NULL) && !Parent->IsWorldObject())
	{
		return Parent->PrepareSelection(eSelectMode);
	}

	//
	// Don't select solid entities when in Solids selection mode. We'll select
	// their solid children.
	//
	if ((eSelectMode == selectSolids) && !IsPlaceholder())
	{
		return NULL;
	}

	return this;
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : pRender -
//-----------------------------------------------------------------------------
void CMapEntity::Render2D(CRender2D *pRender)
{
	// FIXME: we should just render all our children like we do in the 3D view
	Vector vecMins;
	Vector vecMaxs;
	GetRender2DBox(vecMins, vecMaxs);

	CPoint pt;
	CPoint pt2;
	pRender->TransformPoint3D(pt, vecMins);
	pRender->TransformPoint3D(pt2, vecMaxs);

	color32 rgbColor = GetRenderColor();
	pRender->SetLineType(CRender2D::LINE_SOLID, CRender2D::LINE_THIN, rgbColor.r, rgbColor.g, rgbColor.b);

	if (IsPlaceholder())
	{
		//
		// Draw the entity box.
		//
		RECT re;
		re.left = min(pt.x, pt2.x);
		re.right = max(pt.x, pt2.x) + 1;
		re.top = min(pt.y, pt2.y);
		re.bottom = max(pt.y, pt2.y) + 1;

		pRender->Rectangle(re, false);

		//
		// Draw center handle.
		//
		POINT ptCenter;
		ptCenter.x = (pt.x + pt2.x) / 2;
		ptCenter.y = (pt.y + pt2.y) / 2;

		pRender->MoveTo(ptCenter.x - 3, ptCenter.y - 3);
		pRender->LineTo(ptCenter.x + 4, ptCenter.y + 4);
		pRender->MoveTo(ptCenter.x + 3, ptCenter.y - 3);
		pRender->LineTo(ptCenter.x - 4, ptCenter.y + 4);
	}

 	//
	// Render the entity's name and class name if enabled.
	//
	if (s_bShowEntityNames && (pRender->GetZoom() >= 1))
	{
		CPoint ptText(0, 0);

		pRender->SetTextColor(rgbColor.r, rgbColor.g, rgbColor.b, GetRValue(Options.colors.clrBackground), GetGValue(Options.colors.clrBackground), GetBValue(Options.colors.clrBackground));

		const char *pszTargetName = GetKeyValue("targetname");
		if (pszTargetName != NULL)
		{
			ptText.x = pt.x + 2;
			ptText.y = pt.y + 2;
			pRender->DrawText(pszTargetName, ptText, 0, 0, CRender2D::TEXT_JUSTIFY_LEFT | CRender2D::TEXT_SINGLELINE);
		}

		const char *pszClassName = GetClassName();
		if (pszClassName != NULL)
		{
			ptText.x = pt.x + 2;
			ptText.y = pt2.y + 1;
			pRender->DrawText(pszClassName, ptText, 0, 0, CRender2D::TEXT_JUSTIFY_LEFT | CRender2D::TEXT_SINGLELINE);
		}

	}

	//
	// Draw the connections between entities and their targets if enabled.
	//
	if (s_bShowEntityConnections)
	{
		LPCTSTR pszTarget = GetKeyValue("target");

		if (pszTarget != NULL)
		{
			CMapWorld *pWorld = GetWorldObject(this);
			MDkeyvalue kv("targetname", pszTarget);

			CMapObjectList FoundEntities;
			FoundEntities.RemoveAll();
			pWorld->EnumChildren((ENUMMAPCHILDRENPROC)FindKeyValue, (DWORD)&kv, MAPCLASS_TYPE(CMapEntity));

			Vector vec;
			GetBoundsCenter(vec);
			pRender->TransformPoint3D(pt, vec);

			POSITION p = FoundEntities.GetHeadPosition();
			while (p)
			{
				CMapClass *pEntity = (CMapEntity *)FoundEntities.GetNext(p);
				pEntity->GetBoundsCenter(vec);
				pRender->TransformPoint3D(pt2, vec);
				pRender->MoveTo(pt);
				pRender->LineTo(pt2);
			}
		}
	}

	if ((GetSelectionState() != SELECT_NONE) && (GetKeyValue("angles") != NULL))
	{
		Vector vecOrigin;
		GetOrigin(vecOrigin);

		QAngle vecAngles;
		GetAngles(vecAngles);
		Vector vecForward;
		AngleVectors(vecAngles, &vecForward);

		pRender->SetLineType(CRender2D::LINE_SOLID, CRender2D::LINE_THIN, 255, 255, 0);
		pRender->DrawLine(vecOrigin, vecOrigin + vecForward * 24);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether this entity snaps to half grid or not. Some entities,
//			such as hinges, need to snap to a 0.5 grid to center on geometry.
//-----------------------------------------------------------------------------
bool CMapEntity::ShouldSnapToHalfGrid()
{
	return (GetClass() && GetClass()->ShouldSnapToHalfGrid());
}
