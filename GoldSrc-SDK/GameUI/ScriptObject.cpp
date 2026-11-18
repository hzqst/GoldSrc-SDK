#include "ScriptObject.h"
#include "GameUI_Interface.h"
#include "StringUtils.h"

#include "Label.h"
#include "vgui_controls/Controls.h"
#include "wrect.h"
#include "cl_dll.h"

// CScriptObject and CDescription class definitions
// 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#ifndef _WIN32
#include <strings.h>
#define stricmp	strcasecmp
#endif

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"GameUI\\ScriptObject.cpp" )

static char token[1024];

/////////////////////
objtypedesc_t objtypes[] =
{
	{ O_BOOL, "BOOL" },
	{ O_NUMBER, "NUMBER" },
	{ O_LIST, "LIST" },
	{ O_STRING, "STRING" }
};

void StripFloatTrailingZeros( char* text )
{
	// scan for a '.'
	char* decimal = strchr( text, '.' );
	if ( !decimal )
		return;

	char* end = text + strlen( text ) - 1;
	while ( end > decimal && *end == '0' )
		*end-- = 0;
	if ( *end == '.' )
		*end = 0;
}

mpcontrol_t::mpcontrol_t( vgui2::Panel* parent, const char* panelName )
	: vgui2::Panel( parent, panelName )
{
	type = O_BADTYPE;
	pControl = NULL;
	pPrompt = NULL;
	pScrObj = NULL;
	next = NULL;
	SetPaintBackgroundEnabled( false );
}

void mpcontrol_t::OnSizeChanged( int wide, int tall )
{
	if ( pPrompt )
	{
		if ( pControl )
			pControl->SetBounds( wide / 2 + 20, 4, wide / 2 - 20, tall - 8 );
		pPrompt->SetBounds( 0, 4, wide / 2 + 20, tall - 8 );
	}
	else if ( pControl )
	{
		pControl->SetBounds( 0, 4, wide, tall - 8 );
	}
}

CScriptListItem::CScriptListItem()
{
	pNext = NULL;
	memset( szItemText, 0, sizeof( szItemText ) );
	memset( szValue, 0, sizeof( szValue ) );
}

CScriptListItem::CScriptListItem( const char* item, const char* value )
{
	pNext = NULL;
	strncpy( szItemText, item, sizeof( szItemText ) );
	szItemText[sizeof( szItemText ) - 1] = 0;
	strncpy( szValue, value, sizeof( szValue ) );
	szValue[sizeof( szValue ) - 1] = 0;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
CScriptObject::CScriptObject()
{
	type = O_BOOL;
	bSetInfo = false;  // Prepend "Setinfo" to keyvalue pair in config?
	pNext = NULL;
	pListItems = NULL;
	fMin = -1;
	fMax = -1;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CScriptObject::~CScriptObject()
{
	CScriptListItem* item = pListItems;
	while ( item )
	{
		CScriptListItem* next = item->pNext;
		delete item;
		item = next;
	}
	pListItems = NULL;
}

void CScriptObject::AddItem( CScriptListItem* item )
{
	if ( !pListItems )
	{
		pListItems = item;
	}
	else
	{
		CScriptListItem* tail = pListItems;
		while ( tail->pNext )
			tail = tail->pNext;
		tail->pNext = item;
	}
	item->pNext = NULL;
}

void CScriptObject::SetCurValue( const char* value )
{
	strncpy( curValue, value, sizeof( curValue ) );
	curValue[sizeof( curValue ) - 1] = 0;
	fcurValue = (float)atof( curValue );
	if ( type == O_BOOL || type == O_NUMBER )
		StripFloatTrailingZeros( curValue );
}

/*
===================
UTIL_StripInvalidCharacters

Removes any possible formatting codes, double quote characters and line breaks from the input string
===================
*/
void UTIL_StripInvalidCharacters( char* text )
{
	char output[4096];
	char* dest = output;
	output[0] = 0;
	for ( const char* src = text; *src; src++ )
	{
		if ( *src != '%' )
			*dest++ = *src;
	}
	*dest = 0;
	strcpy( text, output );
	UTIL_StripQuotedCommandBreaks( text );
}

void FixupString( char* text )
{
	char buffer[4096];
	strncpy( buffer, text, sizeof( buffer ) );
	buffer[sizeof( buffer ) - 1] = 0;
	UTIL_StripInvalidCharacters( buffer );
	strcpy( text, buffer );
}

void CScriptObject::WriteToScriptFile( FileHandle_t file )
{
	FixupString( cvarname );
	vgui2::filesystem()->FPrintf( file, "\t\"%s\"\r\n", cvarname );
	vgui2::filesystem()->FPrintf( file, "\t{\r\n" );
	FixupString( prompt );

	switch ( type )
	{
	case O_BOOL:
		vgui2::filesystem()->FPrintf( file, "\t\t\"%s\"\r\n", prompt );
		vgui2::filesystem()->FPrintf( file, "\t\t{ BOOL }\r\n" );
		vgui2::filesystem()->FPrintf( file, "\t\t{ \"%i\" }\r\n", (int)fcurValue != 0 );
		break;

	case O_NUMBER:
		vgui2::filesystem()->FPrintf( file, "\t\t\"%s\"\r\n", prompt );
		vgui2::filesystem()->FPrintf( file, "\t\t{ NUMBER %f %f }\r\n", fMin, fMax );
		vgui2::filesystem()->FPrintf( file, "\t\t{ \"%f\" }\r\n", fcurValue );
		break;

	case O_LIST:
		vgui2::filesystem()->FPrintf( file, "\t\t\"%s\"\r\n", prompt );
		vgui2::filesystem()->FPrintf( file, "\t\t{\r\n\t\t\tLIST\r\n" );
		for ( CScriptListItem* item = pListItems; item; item = item->pNext )
		{
			UTIL_StripInvalidCharacters( item->szItemText );
			UTIL_StripInvalidCharacters( item->szValue );
			vgui2::filesystem()->FPrintf( file, "\t\t\t\"%s\" \"%s\"\r\n", item->szItemText, item->szValue );
		}
		vgui2::filesystem()->FPrintf( file, "\t\t}\r\n" );
		vgui2::filesystem()->FPrintf( file, "\t\t{ \"%f\" }\r\n", fcurValue );
		break;

	case O_STRING:
		vgui2::filesystem()->FPrintf( file, "\t\t\"%s\"\r\n", prompt );
		vgui2::filesystem()->FPrintf( file, "\t\t{ STRING }\r\n" );
		FixupString( curValue );
		vgui2::filesystem()->FPrintf( file, "\t\t{ \"%s\" }\r\n", curValue );
		break;

	default:
		break;
	}

	if ( bSetInfo )
		vgui2::filesystem()->FPrintf( file, "\t\tSetInfo\r\n" );
	vgui2::filesystem()->FPrintf( file, "\t}\r\n\r\n" );
}

void CScriptObject::WriteToFile( FileHandle_t file )
{
	FixupString( cvarname );
	vgui2::filesystem()->FPrintf( file, "\"%s\"\t\t", cvarname );

	switch ( type )
	{
	case O_BOOL:
		vgui2::filesystem()->FPrintf( file, "\"%s\"\r\n", fcurValue ? "1" : "0" );
		break;

	case O_NUMBER:
	{
		float value = fcurValue;
#ifdef _WIN32
		if ( fMin != -1 )
			value = value > fMin ? value : fMin;
		if ( fMax != -1 )
			value = fMax > value ? value : fMax;
#else
		if ( fMin != -1 )
			value = fMin > value ? fMin : value;
		if ( fMax != -1 )
			value = value > fMax ? fMax : value;
#endif
		vgui2::filesystem()->FPrintf( file, "\"%f\"\r\n", value );
		break;
	}

	case O_LIST:
	{
		CScriptListItem* item = pListItems;
		for ( int i = 0; i < (int)fcurValue && item; i++ )
			item = item->pNext;
		if ( !item )
		{
			vgui2::filesystem()->FPrintf( file, "\"0.0\"\r\n" );
			break;
		}
		UTIL_StripInvalidCharacters( item->szValue );
		vgui2::filesystem()->FPrintf( file, "\"%s\"\r\n", item->szValue );
		break;
	}

	case O_STRING:
		FixupString( curValue );
		vgui2::filesystem()->FPrintf( file, "\"%s\"\r\n", curValue );
		break;

	default:
		break;
	}
}

void CScriptObject::WriteToConfig()
{
	char valueText[2048];
	switch ( type )
	{
	case O_BOOL:
		sprintf( valueText, "%s", fcurValue ? "1" : "0" );
		break;

	case O_NUMBER:
	{
		float value = fcurValue;
#ifdef _WIN32
		if ( fMin != -1 )
			value = value > fMin ? value : fMin;
		if ( fMax != -1 )
			value = fMax > value ? value : fMax;
#else
		if ( fMin != -1 )
			value = fMin > value ? fMin : value;
		if ( fMax != -1 )
			value = value > fMax ? fMax : value;
#endif
		sprintf( valueText, "%f", value );
		break;
	}

	case O_LIST:
	{
		CScriptListItem* item = pListItems;
		for ( int i = 0; i < (int)fcurValue && item; i++ )
			item = item->pNext;
		if ( item )
		{
			sprintf( valueText, "%s", item->szValue );
			UTIL_StripInvalidCharacters( valueText );
		}
		else  //Couln't find index
		{
			sprintf( valueText, "0.0" );
		}
		break;
	}

	case O_STRING:
		sprintf( valueText, "\"%s\"", curValue );
		UTIL_StripInvalidCharacters( valueText );
		break;

	default:
		break;
	}

	// The cvar name goes into the command unquoted; the value goes inside quotes
	// and UTIL_StripInvalidCharacters already removed its quotes and line breaks
	if ( !UTIL_IsUnquotedCommandArgument( cvarname ) )
		return;

	std::string command = bSetInfo ? "setinfo " : "";
	command += cvarname;
	command += " \"";
	command += valueText;
	command += "\"\n";
	engine->pfnClientCmd( (char*)command.c_str() );
}

objtype_t CScriptObject::GetType( char* typeName )
{
	for ( int i = 0; i < sizeof( objtypes ) / sizeof( objtypes[0] ); i++ )
	{
		if ( !stricmp( objtypes[i].szDescription, typeName ) )
			return objtypes[i].type;
	}
	return O_BADTYPE;
}

bool CScriptObject::ReadFromBuffer( char** buffer )
{
	// Get the first token.
	// The cvar we are setting
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( strlen( token ) < sizeof( cvarname ) )
		strcpy( cvarname, token );
	else
	{
		memcpy( cvarname, token, sizeof( cvarname ) - 1 );
		cvarname[sizeof( cvarname ) - 1] = 0;
	}

	// Parse the {
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( strcmp( token, "{" ) )
		goto unexpectedToken;

	// Parse the Prompt
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( strlen( token ) < sizeof( prompt ) )
		strcpy( prompt, token );
	else
	{
		memcpy( prompt, token, sizeof( prompt ) - 1 );
		prompt[sizeof( prompt ) - 1] = 0;
	}

	// Parse the next {
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( strcmp( token, "{" ) )
		goto unexpectedToken;

	// Now parse the type:
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	type = GetType( token );
	if ( type == O_BADTYPE )
	{
		engine->Con_Printf( "Type '%s' unknown", token );
		return false;
	}

	switch ( type )
	{
	case O_NUMBER:
		// Parse the Min
		*buffer = engine->COM_ParseFile( *buffer, token );
		if ( !token[0] )
			return false;
		fMin = (float)atof( token );
		// Parse the Min
		*buffer = engine->COM_ParseFile( *buffer, token );
		if ( !token[0] )
			return false;
		fMax = (float)atof( token );
		// Parse the next {
		*buffer = engine->COM_ParseFile( *buffer, token );
		if ( !token[0] )
			return false;
		if ( strcmp( token, "}" ) )
			goto unexpectedToken;
		break;

	case O_BOOL:
	case O_STRING:
		// Parse the next {
		*buffer = engine->COM_ParseFile( *buffer, token );
		if ( !token[0] )
			return false;
		if ( strcmp( token, "}" ) )
			goto unexpectedToken;
		break;

	case O_LIST:
		// Parse items until we get the }
		while ( true )
		{
			// Parse the next {
			*buffer = engine->COM_ParseFile( *buffer, token );
			if ( !token[0] )
				return false;
			// Done?
			if ( !strcmp( token, "}" ) )
				break;

			//
			// Add the item to a list somewhere
			// AddItem( token )
			char itemText[128];
			char itemValue[128];
			strncpy( itemText, token, sizeof( itemText ) );
			itemText[sizeof( itemText ) - 1] = 0;
			// Parse the value
			*buffer = engine->COM_ParseFile( *buffer, token );
			if ( !token[0] )
				return false;
			strncpy( itemValue, token, sizeof( itemValue ) );
			itemValue[sizeof( itemValue ) - 1] = 0;
			AddItem( new CScriptListItem( itemText, itemValue ) );
		}
		break;

	default:
		break;
	}

	//
	// Now read in the default value

	// Parse the {
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( strcmp( token, "{" ) )
		goto unexpectedToken;

	// Parse the default
	*buffer = engine->COM_ParseFile( *buffer, token );
	//if ( strlen( token ) <= 0 )
	//	return false;
	strncpy( defValue, token, sizeof( defValue ) );
	defValue[sizeof( defValue ) - 1] = 0;
	fdefValue = (float)atof( token );
	if ( type == O_NUMBER )
		StripFloatTrailingZeros( defValue );
	SetCurValue( defValue );

	// Parse the }
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( strcmp( token, "}" ) )
		goto unexpectedToken;

	// Parse the final }
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( !stricmp( token, "SetInfo" ) )
	{
		bSetInfo = true;
		// Parse the final }
		*buffer = engine->COM_ParseFile( *buffer, token );
		if ( !token[0] )
			return false;
	}
	if ( !strcmp( token, "}" ) )
		return true;

unexpectedToken:
	engine->Con_Printf( "Expecting '{', got '%s'", token );
	return false;
}

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
/////////////////////////
CDescription::CDescription( CPanelListPanel* panel )
{
	pObjList = NULL;
	m_pListPanel = panel;
}
#ifdef _MSC_VER
#pragma warning( pop )
#endif

CDescription::~CDescription()
{
	CScriptObject* object = pObjList;
	while ( object )
	{
		CScriptObject* next = object->pNext;
		object->pNext = NULL;
		delete object;
		object = next;
	}
	pObjList = NULL;
	if ( m_pszHintText )
		free( m_pszHintText );
	if ( m_pszDescriptionType )
		free( m_pszDescriptionType );
}

void CDescription::AddObject( CScriptObject* object )
{
	if ( !pObjList )
	{
		pObjList = object;
	}
	else
	{
		CScriptObject* tail = pObjList;
		while ( tail->pNext )
			tail = tail->pNext;
		tail->pNext = object;
	}
	object->pNext = NULL;
}

bool CDescription::ReadFromBuffer( char** buffer )
{
	// Get the first token.
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	// Read VERSION #
	if ( stricmp( token, "VERSION" ) )
	{
		engine->Con_Printf( "Expecting 'VERSION', got '%s'", token );
		return false;
	}

	// Parse in the version #
	// Get the first token.
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
	{
		engine->Con_Printf( "Expecting version #" );
		return false;
	}
	float version = (float)atof( token );
	if ( version != 1.0f )
	{
		engine->Con_Printf( "Version mismatch, expecting %f, got %f", 1.0, version );
		return false;
	}

	// Get the "DESCRIPTION"
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	// Read DESCRIPTION
	if ( stricmp( token, "DESCRIPTION" ) )
	{
		engine->Con_Printf( "Expecting 'DESCRIPTION', got '%s'", token );
		return false;
	}

	// Parse in the description type
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
	{
		engine->Con_Printf( "Expecting '%s'", m_pszDescriptionType );
		return false;
	}
	if ( stricmp( token, m_pszDescriptionType ) )
	{
		engine->Con_Printf( "Expecting %s, got %s", m_pszDescriptionType, token );
		return false;
	}

	// Parse the {
	*buffer = engine->COM_ParseFile( *buffer, token );
	if ( !token[0] )
		return false;
	if ( strcmp( token, "{" ) )
	{
		engine->Con_Printf( "Expecting '{', got '%s'", token );
		return false;
	}

	while ( true )
	{
		char* start = *buffer;
		// Get the first token.
		*buffer = engine->COM_ParseFile( *buffer, token );
		if ( !token[0] )
			return false;
		// Read "cvar name" or  } when done
		if ( !stricmp( token, "}" ) )
			return true;
		*buffer = start;

		CScriptObject* object = new CScriptObject;
		if ( !object )
		{
			engine->Con_Printf( "Couldn't create script object" );
			return false;
		}
		if ( !object->ReadFromBuffer( &start ) )
		{
			delete object;
			return false;
		}
		*buffer = start;
		AddObject( object );
	}
}

bool CDescription::InitFromFile( char* filename )
{
	FileHandle_t file = vgui2::filesystem()->Open( filename, "rb", "GAMECONFIG" );
	if ( !file )
		return false;

	int size = vgui2::filesystem()->Size( file );

	// read the file
	unsigned char* buffer = new unsigned char[size + 1];
	ASSERT_AT( buffer, 773 );
	vgui2::filesystem()->Read( buffer, size, file );
	vgui2::filesystem()->Close( file );
	buffer[size] = 0;

	char* data = (char*)buffer;
	ReadFromBuffer( &data );

	delete[] buffer;
	return true;
}

void CDescription::WriteToFile( FileHandle_t file )
{
	WriteFileHeader( file );
	for ( CScriptObject* object = pObjList; object; object = object->pNext )
		object->WriteToFile( file );
}

void CDescription::WriteToConfig()
{
	for ( CScriptObject* object = pObjList; object; object = object->pNext )
		object->WriteToConfig();
}

void CDescription::WriteToScriptFile( FileHandle_t file )
{
	WriteScriptHeader( file );
	for ( CScriptObject* object = pObjList; object; object = object->pNext )
		object->WriteToScriptFile( file );
	vgui2::filesystem()->FPrintf( file, "}\r\n" );
}

void CDescription::TransferCurrentValues( const char* )
{
	for ( CScriptObject* object = pObjList; object; object = object->pNext )
	{
		const char* value = object->bSetInfo ? engine->LocalPlayerInfo_ValueForKey( object->cvarname )
			: engine->pfnGetCvarString( object->cvarname );
		if ( !value || !value[0] )
			continue;

		char valueText[1024];
		strncpy( valueText, value, sizeof( valueText ) );
		valueText[sizeof( valueText ) - 1] = 0;
		strncpy( object->curValue, valueText, sizeof( object->curValue ) );
		object->curValue[sizeof( object->curValue ) - 1] = 0;
		object->fcurValue = (float)atof( valueText );
		strncpy( object->defValue, valueText, sizeof( object->defValue ) );
		object->defValue[sizeof( object->defValue ) - 1] = 0;
		object->fdefValue = (float)atof( valueText );
	}
}

void CDescription::setDescription( const char* description )
{
	m_pszDescriptionType = strdup( description );
}

void CDescription::setHint( const char* hint )
{
	m_pszHintText = strdup( hint );
}
