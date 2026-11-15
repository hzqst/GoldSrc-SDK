#include "controls.h"
#include "vgui_intwrap2.h"
#include "vgui_int.h"
#include "vgui2/BaseUISurface.h"
#include "IBaseUI.h"
#include "IGameUI.h"
#include "IGameConsole.h"
#include "utlvector.h"
#include <cstring>
#include "iclientvgui.h"
#include "client.h"
#include "common.h"
#include "host.h"
#include "sys.h"
#include "ivideomode.h"
#include "glquake.h"
#include "../external/SDL2/SDL_video.h"

#include <cwchar>

#define MAX_TUTOR_MESSAGES 256

class ICareerUI;

extern IGameUI* staticGameUIFuncs;
extern IGameConsole* staticGameConsole;
extern ICareerUI* staticCareerUI;
extern IClientVGUI* staticClient;
extern SDL_Window** pmainwindow;
extern cl_enginefuncs_s cl_enginefuncs;

void SCR_UpdateScreen();

IBaseUI* staticUIFuncs;

static CUtlVector<char> g_TempConsoleBuffer( 0, 1024 );
static qboolean staticExclusiveInputShadow;

static int s_tutorMessageDecayData[MAX_TUTOR_MESSAGES];

int VGuiWrap2_GetLocalizedStringLength( const char* label )
{
	wchar_t* text;

	if ( !label )
		return 0;

	text = vgui2::localize()->Find( label );
	if ( !text )
		return 0;

	return (int)wcslen( text );
}

int VGuiWrap2_GameUIKeyPressed()
{
	if ( !staticGameUIFuncs )
		return 0;

	if ( staticGameUIFuncs->HasExclusiveInput() )
	{
		if ( cl.levelname[0] )
			staticUIFuncs->HideGameUI();
	}
	else
	{
		staticUIFuncs->ActivateGameUI();
	}
	return 1;
}

int VGuiWrap2_Key_Event( int down, int keynum, const char* currentBinding )
{
	return !staticUIFuncs || !staticUIFuncs->Key_Event( down, keynum, currentBinding );
}

void VGuiWrap2_HideGameUI()
{
	if ( staticUIFuncs )
		staticUIFuncs->HideGameUI();
}

void VGuiWrap2_HideConsole()
{
	if ( staticUIFuncs )
		staticUIFuncs->HideConsole();
}

int VGuiWrap2_IsConsoleVisible()
{
	if ( staticGameConsole )
		return staticGameConsole->IsConsoleVisible();
	return 0;
}

void VGuiWrap2_ShowConsole()
{
	if ( staticUIFuncs )
	{
		staticUIFuncs->ActivateGameUI();
		if ( staticUIFuncs )
			staticUIFuncs->ShowConsole();
	}
}

void VGuiWrap2_ShowDemoPlayer()
{
	if ( staticUIFuncs )
		staticUIFuncs->ActivateGameUI();
	if ( staticGameUIFuncs )
		staticGameUIFuncs->ActivateDemoUI();
}

void* VguiWrap2_GetCareerUI()
{
	return staticCareerUI;
}

int VGuiWrap2_IsInCareerMatch()
{
	return staticCareerUI ? g_careerState : CAREER_NONE;
}

void VGuiWrap2_ClearConsole()
{
	if ( staticGameConsole )
		staticGameConsole->Clear();
}

void VGuiWrap2_LoadingStarted( const char* resourceType, const char* resourceName )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->LoadingStarted( resourceType, resourceName );
}

void VGuiWrap2_LoadingFinished( const char* resourceType, const char* resourceName )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->LoadingFinished( resourceType, resourceName );
}

void VGuiWrap2_NotifyOfServerDisconnect()
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->DisconnectFromServer();
}

int VGuiWrap2_IsGameUIVisible()
{
	if ( staticGameUIFuncs )
		return staticGameUIFuncs->HasExclusiveInput();
	return 0;
}

void StopLoadingProgressBar()
{
	if ( cls.state == ca_active )
	{
		if ( staticUIFuncs )
			staticUIFuncs->HideGameUI();
	}
	else if ( staticUIFuncs )
	{
		if ( staticClient )
			staticClient->HideAllVGUIMenu();
		staticUIFuncs->ActivateGameUI();
	}

	if ( staticGameUIFuncs )
		staticGameUIFuncs->StopProgressBar( gfExtendedError != 0, gszDisconnectReason, gszExtendedDisconnectReason );

	gfExtendedError = 0;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;
}

void VGuiWrap2_NotifyOfServerConnect( const char* game, int IP, int port )
{
	if ( !staticGameUIFuncs )
		return;

	gfExtendedError = 0;
	gszDisconnectReason[0] = 0;
	gszExtendedDisconnectReason[0] = 0;
	StopLoadingProgressBar();
	staticGameUIFuncs->ConnectToServer( game, IP, port );
}

void StartLoadingProgressBar( const char* loadingType, int numProgressPoints )
{
	if ( Host_IsSinglePlayerGame() )
		return;

	if ( staticUIFuncs )
		staticUIFuncs->ActivateGameUI();

	if ( staticGameUIFuncs )
	{
		staticGameUIFuncs->StartProgressBar( loadingType, numProgressPoints );
		SCR_UpdateScreen();
	}
}

void ContinueLoadingProgressBar( const char*, int progressPoint, float progressFraction )
{
	if ( staticGameUIFuncs && staticGameUIFuncs->ContinueProgressBar( progressPoint, progressFraction ) )
		SCR_UpdateScreen();
}

void SetLoadingProgressBarStatusText( const char* statusText )
{
	if ( staticGameUIFuncs && staticGameUIFuncs->SetProgressBarStatusText( statusText ) )
		SCR_UpdateScreen();
}

void SetSecondaryProgressBar( float progress )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->SetSecondaryProgressBar( progress );
}

void SetSecondaryProgressBarText( const char* statusText )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->SetSecondaryProgressBarText( statusText );
}

void ValidateCDKey( int force, int inConnect )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->ValidateCDKey( force != 0, inConnect != 0 );
}

void VGUI2_OnDisconnectFromServer( int eLoginFailure )
{
	if ( staticGameUIFuncs )
		staticGameUIFuncs->OnDisconnectFromServer( eLoginFailure, com_empty_string );
}

void VGuiWrap2_Shutdown()
{
	if ( staticUIFuncs )
	{
		staticUIFuncs->Shutdown();
		staticUIFuncs = NULL;
	}
}

int VGuiWrap2_CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if ( staticUIFuncs )
		staticUIFuncs->CallEngineSurfaceAppHandler( event, userData );
	return 0;
}

void VGuiWrap2_Paint()
{
	int x = 0, y = 0;
	int wide, tall;
	int exclusiveInput;

	if ( !staticGameUIFuncs )
		return;

	if ( VideoMode_IsWindowed() )
	{
		SDL_GetWindowPosition( *pmainwindow, &x, &y );
		SDL_GetWindowSize( *pmainwindow, &wide, &tall );
	}
	else
	{
		VideoMode_GetCurrentVideoMode( &wide, &tall, NULL );
	}

	AllowFog( 0 );
	staticUIFuncs->Paint( x, y, wide, tall );

	if ( !staticClient || staticClient->UseVGUI1() )
	{
		exclusiveInput = staticGameUIFuncs->HasExclusiveInput();
		if ( exclusiveInput != staticExclusiveInputShadow )
		{
			if ( exclusiveInput )
			{
				VGuiWrap_ReleaseMouse();
			}
			else
			{
				VGuiWrap_GetMouse();
				ClearIOStates();
			}
		}
		staticExclusiveInputShadow = exclusiveInput;
	}
	AllowFog( 1 );
}

static void BufferConsoleMessage( const char* message )
{
	int len = (int)strlen( message );
	int start = g_TempConsoleBuffer.AddMultipleToTail( len );

	memcpy( &g_TempConsoleBuffer.Element( start ), message, len );
}

void VGuiWrap2_ConDPrintf( const char* message )
{
	if ( staticGameConsole )
		staticGameConsole->DPrintf( "%s", message );
	else
		BufferConsoleMessage( message );
}

void VGuiWrap2_ConPrintf( const char* message )
{
	if ( staticGameConsole )
		staticGameConsole->Printf( "%s", message );
	else
		BufferConsoleMessage( message );
}

void RegisterTutorMessageShown( int message )
{
	if ( (unsigned int)message >= MAX_TUTOR_MESSAGES )
		return;

	s_tutorMessageDecayData[message]++;
}

int GetTimesTutorMessageShown( int message )
{
	if ( (unsigned int)message >= MAX_TUTOR_MESSAGES )
		return -1;

	return s_tutorMessageDecayData[message];
}

void ProcessTutorMessageDecayBuffer( int* buffer, int bufferLength )
{
	int count;

	memset( s_tutorMessageDecayData, 0, sizeof( s_tutorMessageDecayData ) );

	count = bufferLength;
	if ( count > MAX_TUTOR_MESSAGES )
		count = MAX_TUTOR_MESSAGES;
	if ( count > 0 )
		memcpy( s_tutorMessageDecayData, buffer, count * sizeof( int ) );
}

void ConstructTutorMessageDecayBuffer( int* buffer, int bufferLength )
{
	int count;

	if ( !buffer )
		return;

	memset( buffer, 0, bufferLength * sizeof( int ) );

	count = bufferLength;
	if ( count > MAX_TUTOR_MESSAGES )
		count = MAX_TUTOR_MESSAGES;
	if ( count > 0 )
		memcpy( buffer, s_tutorMessageDecayData, count * sizeof( int ) );
}

void ResetTutorMessageDecayData()
{
	memset( s_tutorMessageDecayData, 0, sizeof( s_tutorMessageDecayData ) );
}

int VGuiWrap2_UseVGUI1()
{
	return staticClient ? staticClient->UseVGUI1() : 1;
}

void VguiWrap2_GetMouseDelta( int* x, int* y )
{
	g_BaseUISurface.GetMouseDelta( x, y );
}

void* VGuiWrap2_GetPanel()
{
	return NULL;
}

void VGuiWrap2_ReleaseMouse()
{
}

void VGuiWrap2_GetMouse()
{
}

void VGuiWrap2_SetVisible( int )
{
}

void VGuiWrap2_Startup()
{
	CreateInterfaceFn factories[2];

	if ( staticUIFuncs )
		return;

	factories[0] = Sys_GetFactoryThis();
	factories[1] = g_FileSystemFactory;

	staticUIFuncs = (IBaseUI*)Sys_GetFactoryThis()( "BaseUI001", NULL );
	staticUIFuncs->Initialize( factories, 2 );
	staticUIFuncs->Start( &cl_enginefuncs, CLDLL_INTERFACE_VERSION );

	g_TempConsoleBuffer.AddToTail( '\0' );
	VGuiWrap2_ConPrintf( g_TempConsoleBuffer.Base() );
	g_TempConsoleBuffer.Purge();
}
