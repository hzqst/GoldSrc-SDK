#pragma once

#if defined( SW_RENDERER )
#include "igame.h"
#include "const.h"
#include "../external/SDL2/SDL.h"

class CGame : public IGame
{
public:
	CGame();
	~CGame() override;
	bool Init( void* instance ) override;
	bool Shutdown() override;
	bool CreateGameWindow() override;
	void SleepUntilInput( int milliseconds ) override;
	HWND GetMainWindow() override;
	HWND* GetMainWindowAddress() override;
	void SetWindowXY( int x, int y ) override;
	void SetWindowSize( int width, int height ) override;
	void GetWindowRect( int* x, int* y, int* width, int* height ) override;
	bool IsActiveApp() override;
	bool IsMultiplayer() override;
	void PlayStartupVideos() override;
	void PlayAVIAndWait( const char* filename ) override;
	void SetCursorVisible( bool visible ) override;
	bool OnlyLegacyRender() const override;

	void SetActiveApp( bool active );
	void ClearScreen();
	void AppActivate( bool active );

private:
	bool			m_bActiveApp;
	SDL_Window*		m_hSDLWindow;
	SDL_GLContext	m_hSDLGLContext;
	SDL_Renderer*	m_hSDLRenderer;
	bool			m_bCreatedRenderer;

	bool	m_bExpectSyntheticMouseMotion;
	int		m_nMouseTargetX;
	int		m_nMouseTargetY;
	int		m_nWarpDelta;
	bool	m_bCursorVisible;

	int		m_x;
	int		m_y;
	int		m_width;
	int		m_height;
	bool	m_bMultiplayer;
	bool	m_bLegacyRender;
};

extern CGame g_Game;
#else

#include "igame.h"
#include "const.h"
#include "../external/SDL2/SDL.h"

class CGame : public IGame
{
public:
	CGame();
	~CGame() override;

	bool Init( void* pvInstance ) override;
	bool Shutdown() override;
	bool CreateGameWindow() override;
	void SleepUntilInput( int time ) override;
	HWND GetMainWindow() override;
	HWND* GetMainWindowAddress() override;
	void SetWindowXY( int x, int y ) override;
	void SetWindowSize( int w, int h ) override;
	void GetWindowRect( int* x, int* y, int* w, int* h ) override;
	bool IsActiveApp() override;
	bool IsMultiplayer() override;
	void PlayStartupVideos() override;
	void PlayAVIAndWait( const char* fileName ) override;
	void SetCursorVisible( bool bState ) override;
	bool OnlyLegacyRender() const override;

	void SetActiveApp( bool active );
	void ClearScreen();
	void AppActivate( bool fActive );

private:
#if defined( _WIN32 )
	bool m_bActiveApp;
	SDL_Window* m_hSDLWindow;
	SDL_GLContext m_hSDLGLContext;
	SDL_Renderer* m_hSDLRenderer;
	bool m_bCreatedRenderer;

	bool m_bExpectSyntheticMouseMotion;
	int m_nMouseTargetX;
	int m_nMouseTargetY;
	int m_nWarpDelta;
	bool m_bCursorVisible;

	int m_x;
	int m_y;
	int m_width;
	int m_height;
	bool m_bMultiplayer;
	bool m_bLegacyRender;
#else
	int m_y;
	int m_x;
	int m_height;
	int m_width;

	qboolean m_bActiveApp;
	qboolean m_bMultiplayer;

	qboolean m_bExpectSyntheticMouseMotion;
	int m_nMouseTargetX;
	int m_nMouseTargetY;
	int m_nWarpDelta;

	qboolean m_bCursorVisible;
	qboolean m_bLegacyRender;

	SDL_Window* m_hSDLWindow;
	SDL_Renderer* m_hSDLRenderer;
	qboolean m_bCreatedRenderer;
	SDL_GLContext m_hSDLGLContext;
#endif
};

extern CGame g_Game;

#endif
