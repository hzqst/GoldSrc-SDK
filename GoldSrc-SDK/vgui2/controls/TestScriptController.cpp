#include "TestScriptController.h"

#include "controls.h"
#include "filesystem.h"
#include "characterset.h"
#include "filesystem_helpers.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_RUN_ID_KEY "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\TestSuite\\TestRunId"

using namespace vgui2;

TestScriptController::TestScriptController() :
	BaseClass( NULL, "TestScriptController" ), m_TargetSymbols( 0, 256, true )
{
	SetVisible( false );
	ClearScriptCommandBuffer();
	ivgui()->AddTickSignal( GetVPanel(), 50 );
	m_szUniqueTestString[0] = 0;
	m_bParsingErrorOccured = false;
	m_flTimeBetweenRunningEachScriptLine = 0.1f;
	m_ScriptFunctions.EnsureCapacity( 256 );
}

TestScriptController::~TestScriptController()
{
}

void TestScriptController::OnDelay( float time )
{
	if ( m_flNextScriptCommandExecTime != 0.0f )
	{
		float now = (float)system()->GetFrameTime();
		m_flNextScriptCommandExecTime = now + time;
	}
}

void TestScriptController::OnSetScriptRunRate( float time )
{
	m_flTimeBetweenRunningEachScriptLine = time < 0.1f ? 0.1f : time;
}

VPANEL TestScriptController::FindMessageTarget( const char* targetName )
{
	VPANEL context = input()->GetFocus();
	if ( !context )
		context = surface()->GetEmbeddedPanel();

	while ( context )
	{
		VPANEL target = FindMessageTargetInSubPanel( context, targetName );
		if ( target )
			return target;

		context = ipanel()->GetParent( context );
	}

	if ( !stricmp( "TestScriptController", targetName ) )
		return GetVPanel();

	return 0;
}

VPANEL TestScriptController::FindMessageTargetInSubPanel( VPANEL parent, const char* targetName )
{
	if ( !parent )
		return 0;

	char searchName[256];
	strcpy( searchName, targetName );
	char* separator = strchr( searchName, '.' );
	const char* nextTargetName = NULL;
	if ( separator )
	{
		*separator = 0;
		nextTargetName = strchr( targetName, '.' ) + 1;
	}

	if ( !stricmp( ipanel()->GetName( parent ), searchName ) )
	{
		if ( nextTargetName )
			return FindMessageTargetInSubPanel( parent, nextTargetName );

		return parent;
	}

	for ( int i = 0; i < ipanel()->GetChildCount( parent ); i++ )
	{
		VPANEL child = ipanel()->GetChild( parent, i );
		if ( !stricmp( ipanel()->GetName( child ), searchName ) )
		{
			if ( nextTargetName )
				return FindMessageTargetInSubPanel( child, nextTargetName );

			return child;
		}
	}

	return 0;
}

int TestScriptController::PrintMatchingPanels( VPANEL startPanel, const char* targetName )
{
	int count = 0;
	for ( int i = 0; i < ipanel()->GetChildCount( startPanel ); i++ )
	{
		VPANEL child = ipanel()->GetChild( startPanel, i );
		count += PrintMatchingPanels( child, targetName );
		if ( child && FindMessageTargetInSubPanel( child, targetName ) )
		{
			char name[256];
			count++;
			CalculateFullNameForPanel( child, name, sizeof( name ) );
			ivgui()->DPrintf( "\t%s\n", name );
		}
	}

	return count;
}

void TestScriptController::OnPanelNotFoundError( const char* targetName )
{
	char contextPanel[512];
	VPANEL focus = input()->GetFocus();
	if ( focus )
		CalculateFullNameForPanel( focus, contextPanel, sizeof( contextPanel ) );
	else
		strcpy( contextPanel, "< NULL >" );

	ivgui()->DPrintf( "TestScriptController: couldn't find target '%s' in context '%s'\n", targetName, contextPanel );
	ivgui()->DPrintf( "TestScriptController: couldn't find target '%s'\n", targetName );
	ivgui()->DPrintf( "Panels with matching names: \n" );
	if ( PrintMatchingPanels( surface()->GetEmbeddedPanel(), targetName ) <= 0 )
		ivgui()->DPrintf( "\t< no matching names >\n" );
}

void TestScriptController::ExecuteNextScriptCommand()
{
	if ( !m_CurrentScriptCommandIndex.Count() )
		return;

	ScriptProgramCounter_t& counter = m_CurrentScriptCommandIndex[m_CurrentScriptCommandIndex.Count() - 1];
	ScriptFunction_t& function = m_ScriptFunctions[counter.currentScriptFunctionIndex];
	counter.currentScriptCommandIndex++;
	if ( !function.scriptCommands.IsValidIndex( counter.currentScriptCommandIndex ) )
	{
		if ( !counter.currentScriptFunctionIndex )
		{
			ivgui()->DPrintf( "TestScriptController : script completed successfully\n" );
			m_flNextScriptCommandExecTime = 0.0f;
			counter.currentScriptCommandIndex = 0;
		}
		else
		{
			m_CurrentScriptCommandIndex.Remove( m_CurrentScriptCommandIndex.Count() - 1 );
			m_flNextScriptCommandExecTime = (float)system()->GetFrameTime();
		}
		return;
	}

	ScriptCommand_t& command = function.scriptCommands[counter.currentScriptCommandIndex];
	const char* targetName = m_TargetSymbols.String( command.target );
	VPANEL target = FindMessageTarget( targetName );
	switch ( command.cmd )
	{
	case SCRIPTCOMMAND_MESSAGE:
		if ( target )
			PostMessage( target, command.msg->MakeCopy(), 0.0f );
		else
			OnPanelNotFoundError( targetName );
		break;

	case SCRIPTCOMMAND_WAITUNTILVISIBLE:
		if ( !target || !ipanel()->IsVisible( target ) )
			counter.currentScriptCommandIndex--;
		break;

	case SCRIPTCOMMAND_WAITUNTILENABLED:
		if ( !target )
			OnPanelNotFoundError( targetName );
		else if ( !ipanel()->IsEnabled( target ) )
			counter.currentScriptCommandIndex--;
		break;

	case SCRIPTCOMMAND_REGISTRYSET:
		if ( !stricmp( command.msg->GetName(), "Set" ) )
			system()->SetRegistryString( command.msg->GetString( "key", "" ), command.msg->GetString( "value", "" ) );
		else if ( !stricmp( command.msg->GetName(), "Delete" ) )
			system()->DeleteRegistryKey( command.msg->GetString( "key", "" ) );

		m_flNextScriptCommandExecTime = (float)system()->GetFrameTime();
		return;

	case SCRIPTCOMMAND_EXEC:
		{
			int functionIndex = -1;
			for ( int i = 0; i < m_ScriptFunctions.Count(); i++ )
			{
				if ( m_ScriptFunctions[i].name == command.target )
				{
					functionIndex = i;
					break;
				}
			}

			if ( functionIndex < 0 )
				OnPanelNotFoundError( targetName );

			int index = m_CurrentScriptCommandIndex.AddToTail();
			m_CurrentScriptCommandIndex[index].currentScriptFunctionIndex = functionIndex;
			m_CurrentScriptCommandIndex[index].currentScriptCommandIndex = -1;
			m_flNextScriptCommandExecTime = (float)system()->GetFrameTime();
			return;
		}
	}

	float now = (float)system()->GetFrameTime();
	m_flNextScriptCommandExecTime = now + m_flTimeBetweenRunningEachScriptLine;
}

void TestScriptController::CalculateFullNameForPanel( VPANEL panel, char* name, int )
{
	if ( !panel )
	{
		strcpy( name, "< NULL >" );
		return;
	}

	name[0] = 0;
	VPANEL parent = ipanel()->GetParent( panel );
	if ( parent )
	{
		char buffer[256];
		CalculateFullNameForPanel( parent, buffer, sizeof( buffer ) );
		strcpy( name, buffer );
		strcat( name, "." );
	}

	const char* panelName = ipanel()->GetName( panel );
	if ( !panelName || !panelName[0] )
		panelName = "< unnamed >";

	strcat( name, panelName );
}

void TestScriptController::ClearScriptCommandBuffer()
{
	m_flNextScriptCommandExecTime = 0.0f;
	for ( int i = 0; i < m_ScriptFunctions.Count(); i++ )
	{
		ScriptFunction_t& function = m_ScriptFunctions[i];
		for ( int j = 0; j < function.scriptCommands.Count(); j++ )
		{
			if ( function.scriptCommands[j].msg )
				function.scriptCommands[j].msg->deleteThis();
		}
		function.scriptCommands.RemoveAll();
	}

	m_ScriptFunctions.RemoveAll();
	m_CurrentScriptCommandIndex.RemoveAll();
}

void TestScriptController::MakeValueString( const char* value, char* output, int )
{
	while ( *value )
	{
		if ( !strnicmp( value, "%unique%", 8 ) )
		{
			value += 8;
			strcpy( output, m_szUniqueTestString );
			output += strlen( m_szUniqueTestString );
		}
		else
		{
			*output++ = *value++;
		}
	}

	*output = 0;
}

void TestScriptController::AbortOnAnyErrorLogsPresent()
{
	FileFindHandle_t handle;
	const char* file = filesystem()->FindFirst( "errorlogs/*", &handle, NULL );
	if ( !file )
		return;

	while ( file && file[0] )
	{
		if ( file[0] != '.' )
		{
			ClearScriptCommandBuffer();
			PostMessage( surface()->GetEmbeddedPanel(), new KeyValues( "Quit" ), 0.0f );
			system()->ShellExecute( "open", "errorlogs\\errorlog00001.txt" );
		}
		file = filesystem()->FindNext( handle );
	}

	filesystem()->FindClose( handle );
}

void TestScriptController::OnTick()
{
	if ( m_flNextScriptCommandExecTime != 0.0f && system()->GetFrameTime() >= m_flNextScriptCommandExecTime )
	{
		AbortOnAnyErrorLogsPresent();
		ExecuteNextScriptCommand();
	}
}

void TestScriptController::CreateNewUniqueTestString()
{
	const char* computerName = getenv( "COMPUTERNAME" );
	int runID = 0;
	system()->GetRegistryInteger( TEST_RUN_ID_KEY, runID );
#ifdef _WIN32
	_snprintf( m_szUniqueTestString, sizeof( m_szUniqueTestString ), "%s%d", computerName, runID );
#else
	snprintf( m_szUniqueTestString, sizeof( m_szUniqueTestString ), "%s%d", computerName, runID );
#endif
	system()->SetRegistryInteger( TEST_RUN_ID_KEY, runID + 1 );
}

const char* TestScriptController::ParseFunction( ScriptFunction_t& function, const char* memory )
{
	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "{}()'=" );
	char token[512];
	memory = ParseFile( memory, token, NULL, &breakSet );
	while ( token[0] && token[0] != '}' )
	{
		if ( !stricmp( token, "#include" ) )
		{
			memory = ParseFile( memory, token, NULL, &breakSet );
			if ( !LoadScriptFile( token ) )
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}
		}
		else if ( !stricmp( token, "function" ) )
		{
			memory = ParseFile( memory, token, NULL, &breakSet );
			ScriptFunction_t& newFunction = m_ScriptFunctions[m_ScriptFunctions.AddToTail()];
			newFunction.name = m_TargetSymbols.AddString( token );
			memory = ParseFile( memory, token, NULL, &breakSet );
			memory = ParseFunction( newFunction, memory );
		}
		else
		{
			ScriptCommand_t& command = function.scriptCommands[function.scriptCommands.AddToTail()];
			const char* lineStart = memory - strlen( token );
			bool parsed = true;
			if ( !stricmp( token, "exec" ) )
			{
				command.cmd = SCRIPTCOMMAND_EXEC;
				command.msg = NULL;
				memory = ParseFile( memory, token, NULL, &breakSet );
				if ( memory )
					command.target = m_TargetSymbols.AddString( token );
				else
					parsed = false;
			}
			else if ( !stricmp( token, "wait" ) )
			{
				command.cmd = SCRIPTCOMMAND_WAITUNTILVISIBLE;
				command.msg = NULL;
				memory = ParseFile( memory, token, NULL, &breakSet );
				if ( !memory || stricmp( token, "until" ) )
					parsed = false;

				if ( parsed )
				{
					memory = ParseFile( memory, token, NULL, &breakSet );
					if ( memory )
						command.target = m_TargetSymbols.AddString( token );
					else
						parsed = false;
				}
				if ( parsed )
				{
					memory = ParseFile( memory, token, NULL, &breakSet );
					if ( !memory || stricmp( token, "is" ) )
						parsed = false;
				}
				if ( parsed )
				{
					memory = ParseFile( memory, token, NULL, &breakSet );
					if ( !memory )
						parsed = false;
					else if ( !stricmp( token, "visible" ) )
						command.cmd = SCRIPTCOMMAND_WAITUNTILVISIBLE;
					else if ( !stricmp( token, "enabled" ) )
						command.cmd = SCRIPTCOMMAND_WAITUNTILENABLED;
					else
						parsed = false;
				}
			}
			else
			{
				command.cmd = !stricmp( token, "Registry" ) ? SCRIPTCOMMAND_REGISTRYSET : SCRIPTCOMMAND_MESSAGE;
				command.target = m_TargetSymbols.AddString( token );
				memory = ParseFile( memory, token, NULL, &breakSet );
				if ( !memory )
				{
					parsed = false;
				}
				else
				{
					command.msg = new KeyValues( token );
					while ( parsed )
					{
						const char* next = memory;
						while ( *next && *next != '\n' && !( next[0] == '/' && next[1] == '/' ) && !isalnum( *next ) )
							next++;

						if ( !*next || *next == '\n' || ( next[0] == '/' && next[1] == '/' ) )
							break;

						char key[512];
						char value[512];
						memory = ParseFile( memory, token, NULL, &breakSet );
						if ( !memory )
						{
							parsed = false;
							break;
						}
						strncpy( key, token, sizeof( key ) - 1 );
						key[sizeof( key ) - 1] = 0;
						memory = ParseFile( memory, token, NULL, &breakSet );
						if ( !memory || token[0] != '=' )
						{
							parsed = false;
							break;
						}
						memory = ParseFile( memory, token, NULL, &breakSet );
						if ( !memory )
						{
							parsed = false;
							break;
						}
						MakeValueString( token, value, sizeof( value ) );
						command.msg->SetString( key, value );
					}
				}
			}

			if ( !parsed )
			{
				m_bParsingErrorOccured = true;
				return NULL;
			}

			size_t length = memory - lineStart;
			command.originalLine = new char[length + 1];
			strncpy( command.originalLine, lineStart, length );
			command.originalLine[length] = 0;
		}

		memory = ParseFile( memory, token, NULL, &breakSet );
	}

	return memory;
}

bool TestScriptController::ParseScriptFile( const char* memory, int )
{
	m_bParsingErrorOccured = false;
	ParseFunction( m_ScriptFunctions[0], memory );
	return !m_bParsingErrorOccured;
}

bool TestScriptController::LoadScriptFile( const char* scriptFile )
{
	char path[512];
#ifdef _WIN32
	_snprintf( path, sizeof( path ), "testscripts/%s", scriptFile );
#else
	snprintf( path, sizeof( path ), "testscripts/%s", scriptFile );
#endif
	FileHandle_t file = filesystem()->Open( path, "rt", NULL );
	if ( !file )
		file = filesystem()->Open( scriptFile, "rt", NULL );

	int size = 0;
	char* buffer = NULL;
	if ( file )
	{
		unsigned int fileSize = filesystem()->Size( file );
		if ( fileSize <= INT_MAX )
		{
			size = (int)fileSize;
			buffer = (char*)malloc( (size_t)size + 1 );
		}
		if ( !buffer )
			filesystem()->Close( file );
	}

	if ( !buffer )
	{
		ivgui()->DPrintf( "Couldn't find script file %s\n", path );
		m_CurrentScriptCommandIndex.RemoveAll();
		float now = (float)system()->GetFrameTime();
		m_flNextScriptCommandExecTime = now + m_flTimeBetweenRunningEachScriptLine;
		return false;
	}

	int length = filesystem()->Read( buffer, size, file );
	buffer[length] = 0;
	filesystem()->Close( file );
	bool parsed = ParseScriptFile( buffer, length );
	free( buffer );
	return parsed;
}

bool TestScriptController::ExecuteScript( const char* scriptFile )
{
	CreateNewUniqueTestString();
	ClearScriptCommandBuffer();
	m_ScriptFunctions.AddToTail();
	ScriptFunction_t& main = m_ScriptFunctions[0];
	main.name = m_TargetSymbols.AddString( "Main" );
	if ( !LoadScriptFile( scriptFile ) )
	{
		int count = main.scriptCommands.Count();
		if ( count <= 1 )
			ivgui()->DPrintf( "Failure parsing script file %s on first line.", scriptFile );
		else
			ivgui()->DPrintf( "Failure parsing script file %s after line:\n\t%s\n", scriptFile,
				main.scriptCommands[count - 2].originalLine );
	}

	m_CurrentScriptCommandIndex.AddToTail();
	m_CurrentScriptCommandIndex[0].currentScriptFunctionIndex = 0;
	m_CurrentScriptCommandIndex[0].currentScriptCommandIndex = -1;
	float now = (float)system()->GetFrameTime();
	m_flNextScriptCommandExecTime = now + m_flTimeBetweenRunningEachScriptLine;
	return true;
}
