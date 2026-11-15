#pragma once

#include "../vgui/ISurface.h"
#include "../IEngineSurface.h"
#include "imousecontrol.h"
#include "../utlsymbol.h"
#include "../utlvector.h"

#include <cstddef>

namespace vgui2
{
class IImage;
class Image;
}

class BaseUISurface : public vgui2::ISurface, public IMouseControl
{
public:
	typedef vgui2::VPANEL VPANEL;
	typedef vgui2::HCursor HCursor;
	typedef vgui2::HFont HFont;
	typedef vgui2::HTexture HTexture;
	typedef vgui2::IHTML IHTML;
	typedef vgui2::IHTMLEvents IHTMLEvents;
	typedef vgui2::VGuiVertex VGuiVertex;

	BaseUISurface( const BaseUISurface& other );
	BaseUISurface();
	~BaseUISurface();

	void Init( VPANEL panel, IEngineSurface* engineSurface, IHTMLChromeController* controller );
	void Shutdown() override;
	void AppHandler( void* window, void* event );

	bool IsMinimized( VPANEL panel ) override;
	void RestrictPaintToSinglePanel( VPANEL panel ) override;
	VPANEL GetEmbeddedPanel() override;
	void SetEmbeddedPanel( VPANEL panel ) override;
	void RunFrame() override;

	void GetScreenSize( int& width, int& height ) override;
	void SetAsTopMost( VPANEL panel, bool state ) override;
	void SetTitle( VPANEL panel, const wchar_t* title ) override;
	void SetAsToolBar( VPANEL panel, bool state ) override;
	VPANEL GetNotifyPanel() override;
	void SetNotifyIcon( VPANEL panel, HTexture texture, VPANEL target, const char* text ) override;
	bool RecreateContext( VPANEL panel ) override;
	void SetTopLevelFocus( VPANEL panel ) override;

	int GetPopupCount() override;
	VPANEL GetPopup( int index ) override;

	void SolveTraverse( VPANEL panel, bool forceApplyScheme ) override;
	void PaintTraverse( VPANEL panel ) override;

	void SetCursor( HCursor cursor ) override;
	bool IsCursorVisible() override;
	void LockCursor() override;
	void UnlockCursor() override;

	void PushMakeCurrent( VPANEL panel, bool useInsets ) override;
	void PopMakeCurrent( VPANEL panel ) override;

	virtual bool setFullscreenMode( int width, int height, int bitsPerPixel );
	virtual void setWindowedMode();
	virtual void SetAsTopMost( bool state );
	virtual void SetAsToolBar( bool state );

	bool SupportsFeature( SurfaceFeature_e feature ) override;
	bool HasFocus() override;
	bool IsWithin( int x, int y ) override;
	void CreatePopup( VPANEL panel, bool minimized, bool showTaskbarIcon, bool disabled, bool mouseInput,
		bool keyboardInput ) override;
	void Invalidate( VPANEL panel ) override;

	void DrawPrintText( const wchar_t* text, int length ) override;
	void DrawUnicodeChar( wchar_t ch ) override;
	void DrawUnicodeCharAdd( wchar_t ch ) override;
	void DrawSetTextFont( HFont font ) override;

	void AddPanel( VPANEL panel ) override;
	void ReleasePanel( VPANEL panel ) override;
	void MovePopupToFront( VPANEL panel ) override;
	void MovePopupToBack( VPANEL panel ) override;
	bool ShouldPaintChildPanel( VPANEL panel ) override;
	virtual void PanelRequestFocus( VPANEL panel );
	void SetPanelVisible( VPANEL panel, bool state ) override;
	virtual void EnableMouseCapture( bool state );
	void ApplyChanges() override;

	void DrawFilledRect( int x0, int y0, int x1, int y1 ) override;
	void DrawOutlinedRect( int x0, int y0, int x1, int y1 ) override;
	void DrawLine( int x0, int y0, int x1, int y1 ) override;
	void DrawPolyLine( int* x, int* y, int count ) override;
	void DrawTexturedPolygon( VGuiVertex* vertices, int count ) override;
	void DrawSetTextureRGBA(
		int texture, const unsigned char* rgba, int width, int height, int hardwareFilter, bool forceReload ) override;
	void DrawSetTexture( int texture ) override;
	void DrawTexturedRect( int x0, int y0, int x1, int y1 ) override;
	void DrawTexturedRectAdd( int x0, int y0, int x1, int y1 ) override;
	int CreateNewTextureID( bool procedural ) override;
	bool IsTextureIDValid( int texture ) override;

	void DrawSetColor( int red, int green, int blue, int alpha ) override;
	void DrawSetColor( Color color ) override;
	void DrawSetTextColor( int red, int green, int blue, int alpha ) override;
	void DrawSetTextColor( Color color ) override;
	void DrawSetTextPos( int x, int y ) override;
	void DrawGetTextPos( int& x, int& y ) override;
	virtual void DrawPrintChar( int x0, int y0, int x1, int y1, float s0, float t0, float s1, float t1 );
	void DrawFlushText() override;

	void CreateBrowser( VPANEL panel, IHTMLResponses* responses, bool popup, const char* userAgent ) override;
	void RemoveBrowser( VPANEL panel, IHTMLResponses* responses ) override;

	void DrawSetTextureFile( int texture, const char* filename, int hardwareFilter, bool forceReload ) override;
	void DrawGetTextureSize( int texture, int& width, int& height ) override;
	void EnableMouseCapture( VPANEL panel, bool state ) override;
	void SwapBuffers( VPANEL panel ) override;
	void PlaySound( const char* filename ) override;
	virtual void SetNotifyIcon( vgui2::Image* image, VPANEL target, const char* text );

	void SetMinimized( VPANEL panel, bool state ) override;
	void FlashWindow( VPANEL panel, bool state ) override;
	void BringToFront( VPANEL panel ) override;
	void SetForegroundWindow( VPANEL panel ) override;
	void SetModalPanel( VPANEL panel ) override;
	VPANEL GetModalPanel() override;

	virtual bool SetWatchForComputerUse( bool state );
	virtual double GetTimeSinceLastUse();
	void GetWorkspaceBounds( int& x, int& y, int& width, int& height ) override;

	// fonts
	HFont CreateFont() override;
	bool AddGlyphSetToFont( HFont font, const char* name, int tall, int weight, int blur, int scanlines, int flags,
		int rangeMin, int rangeMax ) override;
	int GetFontTall( HFont font ) override;
	int GetFontBlur( HFont font ) override;
	bool IsAdditive( HFont font ) override;
	int GetFontAscent( HFont font, wchar_t ch ) override;
	void GetCharABCwide( HFont font, int ch, int& a, int& b, int& c ) override;
	int GetCharacterWidth( HFont font, int ch ) override;
	void GetTextSize( HFont font, const wchar_t* text, int& width, int& height ) override;
	bool AddCustomFontFile( const char* filename ) override;

	void DrawSetSubTextureRGBA( int texture, int x, int y, const unsigned char* rgba, int width, int height );
	void SetScreenBounds( int x, int y, int width, int height );
	void GetAbsoluteWindowBounds( int& x, int& y, int& width, int& height ) override;
	VPANEL GetTopmostPopup() override;
	void SetTranslateExtendedKeys( bool state ) override;
	void GetProportionalBase( int& width, int& height ) override;
	void SetProportionalBase( int width, int height ) override;

	void CalculateMouseVisible() override;
	bool NeedKBInput() override;
	bool HasCursorPosFunctions() override;
	void SurfaceGetCursorPos( int& x, int& y ) override;
	void SurfaceSetCursorPos( int x, int y ) override;
	bool VGUI2MouseControl() override;
	void SetVGUI2MouseControl( bool state ) override;

	void SetAllowHTMLJavaScript( bool state ) override;
	void ResetCurrentTexture();
	void ResetViewPort();
	void DrawSetTextureRGBAWithAlphaChannel(
		int texture, const unsigned char* rgba, int width, int height, int hardwareFilter, bool forceReload );
	void SetLanguage( const char* language ) override;
	const char* GetLanguage() override;
	bool BIsFullScreen();
	void GetMouseDelta( int* x, int* y );
	void IgnoreMouseVisibility( bool state );

	IHTMLChromeController* AccessChromeHTMLController() override;
	void SetSupportsEsc( bool state ) override;
	bool DeleteTextureByID( int texture ) override;
	void DrawUpdateRegionTextureBGRA(
		int texture, int x, int y, const unsigned char* bgra, int width, int height ) override;
	void DrawSetTextureBGRA( int texture, const unsigned char* bgra, int width, int height ) override;

	IHTML* CreateHTMLWindow( IHTMLEvents* events, VPANEL parent ) override;
	void PaintHTMLWindow( IHTML* window ) override;
	void DeleteHTMLWindow( IHTML* window ) override;

	void GetHDProportionalBase( int& width, int& height ) override;
	void SetHDProportionalBase( int width, int height ) override;

	void InternalSolveTraverse( VPANEL panel );
	void InternalThinkTraverse( VPANEL panel );
	void InternalSchemeSettingsTraverse( VPANEL panel, bool forceApplyScheme );

	vgui2::IImage*			m_pEmulatedMouseImage;
	IEngineSurface*			_engineSurface;
	VPANEL					_embeddedPanel;
	HCursor					_currentCursor;
	VPANEL					_restrictedPanel;

	HFont					m_hCurrentFont;
	int						m_iCurrentTexture;
	int						m_iSurfaceBounds[4];
	CUtlVector<CUtlSymbol>	m_CustomFontFileNames;
	IHTMLChromeController*	m_pChromeController;
	bool					_cursorLocked;

	static bool m_bTranslateExtendedKeys;

	int						_proportionalBaseWidth;
	int						_proportionalBaseHeight;

	bool					_needMouse;
	bool					_needKB;
	bool					m_bVGUI2MouseControl;
	bool					m_bAllowJavaScript;
	char					m_szLanguage[32];
	bool					m_bFullScreen;
	bool					m_bSupportsEsc;

	int						m_deltax;
	int						m_deltay;
	bool					m_bIgnoreMouseVisCalc;

	unsigned long			m_MouseButtonDownTimeStamp;
	int						m_MouseButtonDownX;
	int						m_MouseButtonDownY;
	bool					m_bGotMouseButtonDown;

	int						_proportionalBaseWidth_HD;
	int						_proportionalBaseHeight_HD;
};

extern BaseUISurface g_BaseUISurface;
