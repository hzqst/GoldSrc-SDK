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

#include "../utlvector.h"
#include <cstddef>

namespace vgui2
{
class Panel;

typedef void ( Panel::*MessageFunc_t )();

enum DataType_t : int
{
	DATATYPE_VOID,
	DATATYPE_CONSTCHARPTR,
	DATATYPE_INT,
	DATATYPE_FLOAT,
	DATATYPE_PTR,
	DATATYPE_BOOL,
	DATATYPE_KEYVALUES,
	DATATYPE_CONSTWCHARPTR,
	DATATYPE_UINT64
};

struct MessageMapItem_t
{
	const char*		name;
	alignas( 16 ) MessageFunc_t func;

	int				numParams;
	DataType_t		firstParamType;
	const char*		firstParamName;
	DataType_t		secondParamType;
	const char*		secondParamName;

	int				nameSymbol;
	int				firstParamSymbol;
	int				secondParamSymbol;
};


// mapping, one per class
struct PanelMessageMap
{
	CUtlVector<MessageMapItem_t>	entries;
	bool							processed;
	PanelMessageMap*				baseMap;
	const char*						( *pfnClassName )();
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
};

PanelMessageMap* FindOrAddPanelMessageMap( const char* name );

// if more parameters are needed, just use MAP_MESSAGE_PARAMS() and pass the keyvalue set into the function

//-----------------------------------------------------------------------------
// Purpose: stores the list of objects in the hierarchy
//			used to iterate through an object's message maps
//-----------------------------------------------------------------------------
struct PanelMap_t
{
	MessageMapItem_t*	dataDesc;
	int					dataNumFields;
	const char*			dataClassName;
	PanelMap_t*			baseMap;
	int					processed;
};
}
#define DECLARE_CLASS_SIMPLE( className, baseClassName )                                                               \
public:                                                                                                                \
	typedef baseClassName BaseClass;                                                                                   \
	typedef className ThisClass;                                                                                       \
                                                                                                                       \
	static void AddToMap( const char* scriptname, vgui2::MessageFunc_t function, int paramCount, int p1type,           \
		const char* p1name, int p2type, const char* p2name )                                                           \
	{                                                                                                                  \
		vgui2::PanelMessageMap* map = vgui2::FindOrAddPanelMessageMap( #className );                                   \
		vgui2::MessageMapItem_t entry = { scriptname, function, paramCount, (vgui2::DataType_t)p1type, p1name,         \
			(vgui2::DataType_t)p2type, p2name, 0, 0, 0 };                                                              \
		map->entries.AddToTail( entry );                                                                               \
	}                                                                                                                  \
                                                                                                                       \
	static void ChainToMap()                                                                                           \
	{                                                                                                                  \
		static bool chained = false;                                                                                   \
		if ( chained )                                                                                                 \
			return;                                                                                                    \
		chained = true;                                                                                                \
                                                                                                                       \
		vgui2::PanelMessageMap* map = vgui2::FindOrAddPanelMessageMap( #className );                                   \
		map->pfnClassName = &className::GetPanelClassName;                                                             \
		map->baseMap = vgui2::FindOrAddPanelMessageMap( #baseClassName );                                              \
	}                                                                                                                  \
                                                                                                                       \
	vgui2::PanelMessageMap* GetMessageMap() override                                                                   \
	{                                                                                                                  \
		static vgui2::PanelMessageMap* s_pMap = vgui2::FindOrAddPanelMessageMap( #className );                         \
		return s_pMap;                                                                                                 \
	}                                                                                                                  \
                                                                                                                       \
	static const char* GetPanelClassName()                                                                             \
	{                                                                                                                  \
		return #className;                                                                                             \
	}                                                                                                                  \
                                                                                                                       \
	static const char* GetPanelBaseClassName()                                                                         \
	{                                                                                                                  \
		return #baseClassName;                                                                                         \
	}                                                                                                                  \
                                                                                                                       \
private:                                                                                                               \
	class className##_RegisterMap                                                                                      \
	{                                                                                                                  \
	public:                                                                                                            \
		className##_RegisterMap()                                                                                      \
		{                                                                                                              \
			className::ChainToMap();                                                                                   \
		}                                                                                                              \
	};                                                                                                                 \
	className##_RegisterMap m_RegisterClass;                                                                           \
                                                                                                                       \
public:

#define DECLARE_CLASS_SIMPLE_NOBASE( className )                                                                       \
public:                                                                                                                \
	typedef className ThisClass;                                                                                       \
                                                                                                                       \
	static void AddToMap( const char* scriptname, vgui2::MessageFunc_t function, int paramCount, int p1type,           \
		const char* p1name, int p2type, const char* p2name )                                                           \
	{                                                                                                                  \
		vgui2::PanelMessageMap* map = vgui2::FindOrAddPanelMessageMap( #className );                                   \
		vgui2::MessageMapItem_t entry = { scriptname, function, paramCount, (vgui2::DataType_t)p1type, p1name,         \
			(vgui2::DataType_t)p2type, p2name, 0, 0, 0 };                                                              \
		map->entries.AddToTail( entry );                                                                               \
	}                                                                                                                  \
                                                                                                                       \
	static void ChainToMap()                                                                                           \
	{                                                                                                                  \
		static bool chained = false;                                                                                   \
		if ( chained )                                                                                                 \
			return;                                                                                                    \
		chained = true;                                                                                                \
                                                                                                                       \
		vgui2::PanelMessageMap* map = vgui2::FindOrAddPanelMessageMap( #className );                                   \
		map->pfnClassName = &className::GetPanelClassName;                                                             \
	}                                                                                                                  \
                                                                                                                       \
	virtual vgui2::PanelMessageMap* GetMessageMap()                                                                    \
	{                                                                                                                  \
		static vgui2::PanelMessageMap* s_pMap = vgui2::FindOrAddPanelMessageMap( #className );                         \
		return s_pMap;                                                                                                 \
	}                                                                                                                  \
                                                                                                                       \
	static const char* GetPanelClassName()                                                                             \
	{                                                                                                                  \
		return #className;                                                                                             \
	}                                                                                                                  \
                                                                                                                       \
private:                                                                                                               \
	class className##_RegisterMap                                                                                      \
	{                                                                                                                  \
	public:                                                                                                            \
		className##_RegisterMap()                                                                                      \
		{                                                                                                              \
			className::ChainToMap();                                                                                   \
		}                                                                                                              \
	};                                                                                                                 \
	className##_RegisterMap m_RegisterClass;                                                                           \
                                                                                                                       \
public:
#define _MessageFuncCommon( name, scriptname, function, paramCount, p1type, p1name, p2type, p2name )                 \
	class PanelMessageFunc_##name                                                                                      \
	{                                                                                                                  \
	public:                                                                                                            \
		static void InitVar()                                                                                          \
		{                                                                                                              \
			static bool bAdded = false;                                                                                \
			if ( bAdded )                                                                                              \
				return;                                                                                                \
			bAdded = true;                                                                                             \
			ThisClass::AddToMap( scriptname, (vgui2::MessageFunc_t)function, paramCount, p1type, p1name, p2type,      \
				p2name );                                                                                              \
		}                                                                                                              \
		PanelMessageFunc_##name()                                                                                      \
		{                                                                                                              \
			InitVar();                                                                                                 \
		}                                                                                                              \
	};                                                                                                                 \
	PanelMessageFunc_##name m_##name##_register

#define MESSAGE_FUNC( name, scriptname )                                                                               \
	_MessageFuncCommon( name, scriptname, &ThisClass::name, 0, vgui2::DATATYPE_VOID, NULL, vgui2::DATATYPE_VOID, NULL ); \
	virtual void name()

#define _MessageFuncOneParam( name, scriptname, type, datatype, p1 )                                                   \
	_MessageFuncCommon( name, scriptname, (void ( vgui2::Panel::* )( type ))&ThisClass::name, 1, datatype, #p1,      \
		vgui2::DATATYPE_VOID, NULL );                                                                                  \
	virtual void name( type p1 )

#define MESSAGE_FUNC_INT( name, scriptname, p1 ) _MessageFuncOneParam( name, scriptname, int, vgui2::DATATYPE_INT, p1 )
#define MESSAGE_FUNC_FLOAT( name, scriptname, p1 )                                                                     \
	_MessageFuncOneParam( name, scriptname, float, vgui2::DATATYPE_FLOAT, p1 )
#define MESSAGE_FUNC_PTR( name, scriptname, p1 )                                                                       \
	_MessageFuncOneParam( name, scriptname, vgui2::Panel*, vgui2::DATATYPE_PTR, p1 )
#define MESSAGE_FUNC_CHARPTR( name, scriptname, p1 )                                                                   \
	_MessageFuncOneParam( name, scriptname, const char*, vgui2::DATATYPE_CONSTCHARPTR, p1 )
#define MESSAGE_FUNC_WCHARPTR( name, scriptname, p1 )                                                                  \
	_MessageFuncOneParam( name, scriptname, const wchar_t*, vgui2::DATATYPE_CONSTWCHARPTR, p1 )

#define MESSAGE_FUNC_PARAMS( name, scriptname, p1 )                                                                    \
	_MessageFuncCommon( name, scriptname, (void ( vgui2::Panel::* )( KeyValues* ))&ThisClass::name, 1,                 \
		vgui2::DATATYPE_KEYVALUES, NULL, vgui2::DATATYPE_VOID, NULL );                                                 \
	virtual void name( KeyValues* p1 )

#define MESSAGE_FUNC_INT_INT( name, scriptname, p1, p2 )                                                               \
	_MessageFuncCommon( name, scriptname, (void ( vgui2::Panel::* )( int, int ))&ThisClass::name, 2,                   \
		vgui2::DATATYPE_INT, #p1, vgui2::DATATYPE_INT, #p2 );                                                          \
	virtual void name( int p1, int p2 )
