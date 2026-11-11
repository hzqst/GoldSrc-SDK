//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Panel.h"
#include "FileSystem.h"

class CPanelListPanel;

namespace vgui2
{
class Label;
}

enum objtype_t
{
	O_BADTYPE = 0,
	O_BOOL,
	O_NUMBER,
	O_LIST,
	O_STRING
};

typedef struct
{
	objtype_t	type;
	char		szDescription[32];
} objtypedesc_t;

class CScriptListItem
{
public:
	CScriptListItem();
	CScriptListItem( const char* item, const char* value );

	char				szItemText[128];
	char				szValue[256];
	CScriptListItem*	pNext;
};

class CScriptObject : public vgui2::Panel
{
public:
	CScriptObject();
	~CScriptObject() override;

	void AddItem( CScriptListItem* item );
	bool ReadFromBuffer( char** buffer );
	void WriteToConfig();
	void WriteToFile( FileHandle_t file );
	void WriteToScriptFile( FileHandle_t file );
	void SetCurValue( const char* value );
	objtype_t GetType( char* typeName );

	objtype_t			type;
	char				cvarname[64];
	char				prompt[256];
	CScriptListItem*	pListItems;
	float				fMin;
	float				fMax;
	char				defValue[128]; // Default value string
	float				fdefValue; // Float version of default value.
	char				curValue[128];
	float				fcurValue;
	bool				bSetInfo; // Prepend "Setinfo" to keyvalue pair in config?
	// Linked list of default list box items.
	CScriptObject*		pNext;
};

class CDescription
{
public:
	CDescription( CPanelListPanel* panel );
	~CDescription();

	bool ReadFromBuffer( char** buffer );
	virtual bool InitFromFile( char* filename );
	void TransferCurrentValues( const char* configFile );
	void AddObject( CScriptObject* object );
	void WriteToConfig();
	void WriteToFile( FileHandle_t file );
	void WriteToScriptFile( FileHandle_t file );
	virtual void WriteScriptHeader( FileHandle_t file ) = 0; // Clients must implement this.
	virtual void WriteFileHeader( FileHandle_t file ) = 0; // Clients must implement this.
	void setDescription( const char* description );
	void setHint( const char* hint );
	const char* GetDescription() { return m_pszDescriptionType; }
	const char* getHint() { return m_pszHintText; }

	CScriptObject*	pObjList;

private:
	char*				m_pszHintText;
	char*				m_pszDescriptionType;
	CPanelListPanel*	m_pListPanel;
};

class mpcontrol_t : public vgui2::Panel
{
public:
	mpcontrol_t( vgui2::Panel* parent, const char* panelName );
	~mpcontrol_t() override {}
	void OnSizeChanged( int wide, int tall ) override;

	objtype_t			type;
	vgui2::Panel*		pControl;
	vgui2::Label*		pPrompt;
	CScriptObject*		pScrObj;
	mpcontrol_t*		next;
};

void	StripFloatTrailingZeros( char* text );
void	UTIL_StripInvalidCharacters( char* text );
void	FixupString( char* text );
