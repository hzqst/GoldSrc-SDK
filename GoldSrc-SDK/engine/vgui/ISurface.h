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

#include "../interface.h"
#include "../color.h"
#include "../appframework/iappsystem.h"
#include "IScheme.h"
#include "VGuiVertex.h"

class IHTMLChromeController;
class IHTMLResponses;

namespace vgui2
{
typedef unsigned int VPANEL;
typedef unsigned long HCursor;

class IHTML;
class IHTMLEvents;

//-----------------------------------------------------------------------------
// Purpose: Wraps contextless windows system functions
//-----------------------------------------------------------------------------
class ISurface : public IBaseInterface, public IAppSystem
{
public:
	enum SurfaceFeature_e
	{
		ANTIALIASED_FONTS = 1,
		DROPSHADOW_FONTS,
		ESCAPE_KEY,
		OPENING_NEW_HTML_WINDOWS,
		FRAME_MINIMIZE_MAXIMIZE,
		DIRECT_HWND_RENDER
	};

	// call to Shutdown surface; surface can no longer be used after this is called
	virtual void Shutdown() = 0;
	// frame
	virtual void RunFrame() = 0;

	// hierarchy root
	virtual VPANEL GetEmbeddedPanel() = 0;
	virtual void SetEmbeddedPanel( VPANEL panel ) = 0;
	// drawing context
	virtual void PushMakeCurrent( VPANEL panel, bool useInsets ) = 0;
	virtual void PopMakeCurrent( VPANEL panel ) = 0;

	virtual void DrawSetColor( int red, int green, int blue, int alpha ) = 0;
	virtual void DrawSetColor( Color color ) = 0;
	virtual void DrawFilledRect( int x0, int y0, int x1, int y1 ) = 0;
	virtual void DrawOutlinedRect( int x0, int y0, int x1, int y1 ) = 0;
	virtual void DrawLine( int x0, int y0, int x1, int y1 ) = 0;
	virtual void DrawPolyLine( int* x, int* y, int count ) = 0;

	virtual void DrawSetTextFont( HFont font ) = 0;
	virtual void DrawSetTextColor( int red, int green, int blue, int alpha ) = 0;
	virtual void DrawSetTextColor( Color color ) = 0;
	virtual void DrawSetTextPos( int x, int y ) = 0;
	virtual void DrawGetTextPos( int& x, int& y ) = 0;
	virtual void DrawPrintText( const wchar_t* text, int length ) = 0;
	virtual void DrawUnicodeChar( wchar_t ch ) = 0;
	virtual void DrawUnicodeCharAdd( wchar_t ch ) = 0;
	virtual void DrawFlushText() = 0;		// flushes any buffered text (for rendering optimizations)

	virtual IHTML* CreateHTMLWindow( IHTMLEvents* events, VPANEL parent ) = 0;
	virtual void PaintHTMLWindow( IHTML* window ) = 0;
	virtual void DeleteHTMLWindow( IHTML* window ) = 0;

	virtual void DrawSetTextureFile( int texture, const char* filename, int hardwareFilter, bool forceReload ) = 0;
	virtual void DrawSetTextureRGBA( int texture, const unsigned char* rgba, int width, int height,
		int hardwareFilter, bool forceReload ) = 0;
	virtual void DrawSetTexture( int texture ) = 0;
	virtual void DrawGetTextureSize( int texture, int& width, int& height ) = 0;
	virtual void DrawTexturedRect( int x0, int y0, int x1, int y1 ) = 0;
	virtual bool IsTextureIDValid( int texture ) = 0;
	virtual int CreateNewTextureID( bool procedural ) = 0;

	virtual void GetScreenSize( int& width, int& height ) = 0;
	virtual void SetAsTopMost( VPANEL panel, bool state ) = 0;
	virtual void BringToFront( VPANEL panel ) = 0;
	virtual void SetForegroundWindow( VPANEL panel ) = 0;
	virtual void SetPanelVisible( VPANEL panel, bool state ) = 0;
	virtual void SetMinimized( VPANEL panel, bool state ) = 0;
	virtual bool IsMinimized( VPANEL panel ) = 0;
	virtual void FlashWindow( VPANEL panel, bool state ) = 0;
	virtual void SetTitle( VPANEL panel, const wchar_t* title ) = 0;
	virtual void SetAsToolBar( VPANEL panel, bool state ) = 0; // removes the window's task bar entry (for context menu's, etc.)
	virtual void CreatePopup( VPANEL panel, bool minimized, bool showTaskbarIcon, bool disabled, bool mouseInput,
		bool keyboardInput ) = 0;
	virtual void SwapBuffers( VPANEL panel ) = 0;
	virtual void Invalidate( VPANEL panel ) = 0;

	virtual void SetCursor( HCursor cursor ) = 0;
	virtual bool IsCursorVisible() = 0;

	virtual void ApplyChanges() = 0;
	virtual bool IsWithin( int x, int y ) = 0;
	virtual bool HasFocus() = 0;
	virtual bool SupportsFeature( SurfaceFeature_e feature ) = 0;

	virtual void RestrictPaintToSinglePanel( VPANEL panel ) = 0;
	virtual void SetModalPanel( VPANEL panel ) = 0;
	virtual VPANEL GetModalPanel() = 0;
	virtual void UnlockCursor() = 0;
	virtual void LockCursor() = 0;
	virtual void SetTranslateExtendedKeys( bool state ) = 0;
	virtual VPANEL GetTopmostPopup() = 0;
	virtual void SetTopLevelFocus( VPANEL panel ) = 0;

	// fonts
	virtual HFont CreateFont() = 0;
	virtual bool AddGlyphSetToFont( HFont font, const char* name, int tall, int weight, int blur, int scanlines,
		int flags, int rangeMin, int rangeMax ) = 0;
	// adds a custom font file (only supports true type font files (.ttf) for now)
	virtual bool AddCustomFontFile( const char* filename ) = 0;
	// returns the details about the font
	virtual int GetFontTall( HFont font ) = 0;
	virtual void GetCharABCwide( HFont font, int ch, int& a, int& b, int& c ) = 0;
	virtual int GetCharacterWidth( HFont font, int ch ) = 0;
	virtual void GetTextSize( HFont font, const wchar_t* text, int& width, int& height ) = 0;

	// notify icons?!?
	virtual VPANEL GetNotifyPanel() = 0;
	virtual void SetNotifyIcon( VPANEL panel, HTexture texture, VPANEL target, const char* text ) = 0;

	// plays a sound
	virtual void PlaySound( const char* filename ) = 0;

	//!! these functions should not be accessed directly, but only through other vgui items
	//!! need to move these to seperate interface
	virtual int GetPopupCount() = 0;
	virtual VPANEL GetPopup( int index ) = 0;
	virtual bool ShouldPaintChildPanel( VPANEL panel ) = 0;
	virtual bool RecreateContext( VPANEL panel ) = 0;
	virtual void AddPanel( VPANEL panel ) = 0;
	virtual void ReleasePanel( VPANEL panel ) = 0;
	virtual void MovePopupToFront( VPANEL panel ) = 0;
	virtual void MovePopupToBack( VPANEL panel ) = 0;

	virtual void SolveTraverse( VPANEL panel, bool forceApplyScheme ) = 0;
	virtual void PaintTraverse( VPANEL panel ) = 0;
	virtual void EnableMouseCapture( VPANEL panel, bool state ) = 0;

	// returns the size of the workspace
	virtual void GetWorkspaceBounds( int& x, int& y, int& width, int& height ) = 0;
	// gets the absolute coordinates of the screen (in windows space)
	virtual void GetAbsoluteWindowBounds( int& x, int& y, int& width, int& height ) = 0;
	// gets the base resolution used in proportional mode
	virtual void GetProportionalBase( int& width, int& height ) = 0;

	virtual void CalculateMouseVisible() = 0;
	virtual bool NeedKBInput() = 0;
	virtual bool HasCursorPosFunctions() = 0;
	virtual void SurfaceGetCursorPos( int& x, int& y ) = 0;
	virtual void SurfaceSetCursorPos( int x, int y ) = 0;

	virtual void DrawTexturedPolygon( VGuiVertex* vertices, int count ) = 0;
	virtual int GetFontAscent( HFont font, wchar_t ch ) = 0;

	// web browser
	virtual void SetAllowHTMLJavaScript( bool state ) = 0;
	virtual void SetLanguage( const char* language ) = 0;
	virtual const char* GetLanguage() = 0;

	virtual bool DeleteTextureByID( int texture ) = 0;
	virtual void DrawUpdateRegionTextureBGRA( int texture, int x, int y, const unsigned char* bgra, int width,
		int height ) = 0;
	virtual void DrawSetTextureBGRA( int texture, const unsigned char* bgra, int width, int height ) = 0;

	virtual void CreateBrowser( VPANEL panel, IHTMLResponses* responses, bool popup, const char* userAgent ) = 0;
	virtual void RemoveBrowser( VPANEL panel, IHTMLResponses* responses ) = 0;
	virtual IHTMLChromeController* AccessChromeHTMLController() = 0;

	virtual void DrawTexturedRectAdd( int x0, int y0, int x1, int y1 ) = 0;
	virtual void SetSupportsEsc( bool state ) = 0;
	virtual int GetFontBlur( HFont font ) = 0;
	virtual bool IsAdditive( HFont font ) = 0;

	virtual void SetProportionalBase( int width, int height ) = 0;
	virtual void GetHDProportionalBase( int& width, int& height ) = 0;
	virtual void SetHDProportionalBase( int width, int height ) = 0;
};

ISurface* surface();
}
