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

#include "vgui/ISurface.h"
#include "vgui/IClientPanel.h"
#include "vgui/IInputInternal.h"
#include "vgui/IPanel.h"
#include "vgui/IScheme.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"
#include "vgui_internal.h"
#include "VPanel.h"
#include "KeyValues.h"
#include "FileSystem.h"
#include "utlrbtree.h"
#include "utlsymbol.h"
#include "utlvector.h"
#include "../vgui_surfacelib/FontManager.h"
#include "../vgui_surfacelib/LinuxFont.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ole2.h>
#include <shellapi.h>
#include <mmsystem.h>

#undef CreateFont
#undef PlaySound
#undef PostMessage
#undef SendMessage
#undef GetClassName

extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );

#define SURFACE_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\vgui2\\src\\Surface.cpp"

using namespace vgui2;

namespace vgui2
{

extern ISchemeManager* g_pScheme;

enum CursorCode
{
	dc_user,
	dc_none,
	dc_arrow,
	dc_ibeam,
	dc_hourglass,
	dc_waitarrow,
	dc_crosshair,
	dc_up,
	dc_sizenwse,
	dc_sizenesw,
	dc_sizewe,
	dc_sizens,
	dc_sizeall,
	dc_no,
	dc_hand,
	dc_blank,

	dc_last
};

class VGuiVertex
{
public:
	int		x;
	int		y;
	float	u;
	float	v;
};

class SurfacePlat
{
public:
	HWND	hwnd;
	HDC		hdc;
	HDC		hwndDC;
	HDC		textureDC;
	int		reserved0;
	HRGN	clipRgn;
	HBITMAP	bitmap;
	int		bitmapSize[2];
	int		reserved1[4];
	bool	reservedFlag;
	bool	disabled; // whether the window can take user input
	int		reserved2[3];
	VPANEL	panel;
	int		reserved3;
	HICON	notifyIcon;
	int		reserved4[2];
};

}

namespace
{

#pragma pack( push, 1 )
struct TGAHeader_t
{
	unsigned char	id_length;
	unsigned char	colormap_type;
	unsigned char	image_type;
	unsigned short	colormap_index;
	unsigned short	colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin;
	unsigned short	y_origin;
	unsigned short	width;
	unsigned short	height;
	unsigned char	pixel_size;
	unsigned char	attributes;
};
#pragma pack( pop )

struct Texture
{
	int			_id;
	HBITMAP		_bitmap;
	HBITMAP		_maskBitmap;
	bool		_masked;
	HICON		_icon;
	int			_wide;
	int			_tall;
	void*		_bits;
	void*		_maskBits;
	const char*	_filename;
};

class CPopupList : public Dar< VPANEL >
{
public:
	~CPopupList()
	{
		delete[] _data;
	}

	VPANEL Element( int index )
	{
		if ( index < 0 || index >= _count )
		{
			return NULL;
		}

		return _data[index];
	}

	int Find( VPANEL panel )
	{
		for ( int i = 0; i < _count; i++ )
		{
			if ( _data[i] == panel )
			{
				return i;
			}
		}

		return -1;
	}

	void AddElement( VPANEL panel )
	{
		EnsureCapacity( _count + 1 );
		_data[_count] = panel;
		_count++;
	}

	void RemoveElementAt( int index )
	{
		if ( index < 0 || index >= _count )
		{
			return;
		}

		for ( ; index < _count - 1; index++ )
		{
			_data[index] = _data[index + 1];
		}

		_count--;
	}

	void RemoveElement( VPANEL panel )
	{
		for ( int i = 0; i < _count; i++ )
		{
			if ( _data[i] == panel )
			{
				RemoveElementAt( i );
				break;
			}
		}
	}

	void MoveElementToEnd( VPANEL panel )
	{
		if ( _data[_count - 1] == panel )
		{
			return;
		}

		for ( int i = _count - 2; i >= 0; i-- )
		{
			if ( _data[i] == panel )
			{
				memmove( _data + i, _data + i + 1, sizeof( VPANEL ) * ( _count - ( i + 1 ) ) );
				_data[_count - 1] = panel;
				return;
			}
		}
	}

private:
	void EnsureCapacity( int wantedCapacity )
	{
		if ( wantedCapacity <= _capacity )
		{
			return;
		}

		int newCapacity = _capacity;
		if ( newCapacity == 0 )
		{
			newCapacity = 1;
		}

		while ( newCapacity < wantedCapacity )
		{
			newCapacity *= 2;
		}

		VPANEL* newData = new VPANEL[newCapacity];
		if ( !newData )
		{
			exit( 0 );
			return;
		}

		memset( newData, 0, sizeof( VPANEL ) * newCapacity );
		_capacity = newCapacity;

		for ( int i = 0; i < _count; i++ )
		{
			newData[i] = _data[i];
		}

		delete[] _data;
		_data = newData;
	}
};

}

class CSurfaceDragDropTarget : public IDropTarget
{
public:
	virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, void** ppvObject );
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	virtual HRESULT STDMETHODCALLTYPE DragEnter( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt,
		DWORD* pdwEffect );
	virtual HRESULT STDMETHODCALLTYPE DragOver( DWORD grfKeyState, POINTL pt, DWORD* pdwEffect );
	virtual HRESULT STDMETHODCALLTYPE DragLeave();
	virtual HRESULT STDMETHODCALLTYPE Drop( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect );

	virtual KeyValues* CreateDragData( IDataObject* pDataObj );

private:
	int			m_iRefCount;
	KeyValues*	m_pDragData;
};

//-----------------------------------------------------------------------------
// Purpose: Implementation of ISurface for use running under windows, renders using GDI
//			This is not used in the game, EngineSurface is used instead
//-----------------------------------------------------------------------------
class CWin32Surface : public ISurface
{
public:
	CWin32Surface();
	~CWin32Surface();

	virtual void Shutdown();
	virtual void RunFrame();

	virtual VPANEL GetEmbeddedPanel();
	virtual void SetEmbeddedPanel( VPANEL pEmbeddedPanel );
	virtual void PushMakeCurrent( VPANEL panel, bool useInsets );
	virtual void PopMakeCurrent( VPANEL panel );

	virtual void DrawSetColor( int r, int g, int b, int a );
	virtual void DrawSetColor( Color col );
	virtual void DrawFilledRect( int x0, int y0, int x1, int y1 );
	virtual void DrawOutlinedRect( int x0, int y0, int x1, int y1 );
	virtual void DrawLine( int x0, int y0, int x1, int y1 );
	virtual void DrawPolyLine( int* px, int* py, int numPoints );

	virtual void DrawSetTextFont( HFont font );
	virtual void DrawSetTextColor( int r, int g, int b, int a );
	virtual void DrawSetTextColor( Color col );
	virtual void DrawSetTextPos( int x, int y );
	virtual void DrawGetTextPos( int& x, int& y );
	virtual void DrawPrintText( const wchar_t* text, int textLen );
	virtual void DrawUnicodeChar( wchar_t wch );
	virtual void DrawUnicodeCharAdd( wchar_t wch );
	virtual void DrawFlushText();

	virtual IHTML* CreateHTMLWindow( IHTMLEvents* events, VPANEL context );
	virtual void PaintHTMLWindow( IHTML* htmlwin );
	virtual void DeleteHTMLWindow( IHTML* htmlwin );

	virtual void DrawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload );
	virtual void DrawSetTextureRGBA( int id, const unsigned char* rgba, int wide, int tall, int hardwareFilter,
		bool forceReload );
	virtual void DrawSetTexture( int id );
	virtual void DrawGetTextureSize( int id, int& wide, int& tall );
	virtual void DrawTexturedRect( int x0, int y0, int x1, int y1 );
	virtual bool IsTextureIDValid( int id );
	virtual int CreateNewTextureID( bool procedural );

	virtual void GetScreenSize( int& wide, int& tall );
	virtual void SetAsTopMost( VPANEL panel, bool state );
	virtual void BringToFront( VPANEL panel );
	virtual void SetForegroundWindow( VPANEL panel );
	virtual void SetPanelVisible( VPANEL panel, bool state );
	virtual void SetMinimized( VPANEL panel, bool state );
	virtual bool IsMinimized( VPANEL panel );
	virtual void FlashWindow( VPANEL panel, bool state );
	virtual void SetTitle( VPANEL panel, const wchar_t* title );
	virtual void SetAsToolBar( VPANEL panel, bool state );
	virtual void CreatePopup( VPANEL panel, bool minimized, bool showTaskbarIcon, bool disabled, bool mouseInput,
		bool kbInput );
	virtual void SwapBuffers( VPANEL panel );
	virtual void Invalidate( VPANEL panel );

	virtual void SetCursor( HCursor cursor );
	virtual bool IsCursorVisible();
	virtual void ApplyChanges();
	virtual bool IsWithin( int x, int y );
	virtual bool HasFocus();
	virtual bool SupportsFeature( SurfaceFeature_e feature );
	virtual void RestrictPaintToSinglePanel( VPANEL panel );
	virtual void SetModalPanel( VPANEL panel );
	virtual VPANEL GetModalPanel();
	virtual void UnlockCursor();
	virtual void LockCursor();
	virtual void SetTranslateExtendedKeys( bool state );
	virtual VPANEL GetTopmostPopup();
	virtual void SetTopLevelFocus( VPANEL panel );

	// fonts
	virtual HFont CreateFont();
	virtual bool AddGlyphSetToFont( HFont font, const char* windowsFontName, int tall, int weight, int blur,
		int scanlines, int flags, int rangeMin, int rangeMax );
	virtual bool AddCustomFontFile( const char* fontFileName );
	virtual int GetFontTall( HFont font );
	virtual void GetCharABCwide( HFont font, int ch, int& a, int& b, int& c );
	virtual int GetCharacterWidth( HFont font, int ch );
	virtual void GetTextSize( HFont font, const wchar_t* text, int& wide, int& tall );

	virtual VPANEL GetNotifyPanel();
	virtual void SetNotifyIcon( VPANEL context, HTexture icon, VPANEL panelToReceiveMessages, const char* text );
	virtual void PlaySound( const char* fileName );

	virtual int GetPopupCount();
	virtual VPANEL GetPopup( int index );
	virtual bool ShouldPaintChildPanel( VPANEL childPanel );
	virtual bool RecreateContext( VPANEL panel );
	virtual void AddPanel( VPANEL panel );
	virtual void ReleasePanel( VPANEL panel );
	virtual void MovePopupToFront( VPANEL panel );
	virtual void MovePopupToBack( VPANEL panel );
	virtual void SolveTraverse( VPANEL panel, bool forceApplySchemeSettings = false );
	virtual void PaintTraverse( VPANEL panel );
	virtual void EnableMouseCapture( VPANEL panel, bool state );

	virtual void GetWorkspaceBounds( int& x, int& y, int& wide, int& tall );
	virtual void GetAbsoluteWindowBounds( int& x, int& y, int& wide, int& tall );
	virtual void GetProportionalBase( int& wide, int& tall );
	virtual void CalculateMouseVisible();
	virtual bool NeedKBInput();
	virtual bool HasCursorPosFunctions();
	virtual void SurfaceGetCursorPos( int& x, int& y );
	virtual void SurfaceSetCursorPos( int x, int y );

	virtual void DrawTexturedPolygon( VGuiVertex* pVertices, int n );
	virtual int GetFontAscent( HFont font, wchar_t wch );
	virtual void SetAllowHTMLJavaScript( bool state );
	virtual void SetLanguage( const char* language );
	virtual const char* GetLanguage();
	virtual bool DeleteTextureByID( int id );
	virtual void DrawUpdateRegionTextureBGRA( int nTextureID, int x, int y, const unsigned char* pchData, int wide,
		int tall );
	virtual void DrawSetTextureBGRA( int id, const unsigned char* rgba, int wide, int tall );

	virtual void CreateBrowser( VPANEL panel, IHTMLResponses* pBrowser, bool bPopupWindow, const char* pchUserAgentIdentifier );
	virtual void RemoveBrowser( VPANEL panel, IHTMLResponses* pBrowser );
	virtual IHTMLChromeController* AccessChromeHTMLController();

	virtual void DrawTexturedRectAdd( int x0, int y0, int x1, int y1 );
	virtual void SetSupportsEsc( bool bSupportsEsc );
	virtual int GetFontBlur( HFont font );
	virtual bool IsAdditive( HFont font );
	virtual void SetProportionalBase( int width, int height );
	virtual void GetHDProportionalBase( int& wide, int& tall );
	virtual void SetHDProportionalBase( int width, int height );

	virtual void SetContextPanel( VPANEL panel );

	void ActivatePanel( VPANEL panel );

private:
	static bool TextureLessFunc( const Texture& lhs, const Texture& rhs );

	void InitStaticData();
	SurfacePlat* ContextPlat();
	void SetPenColor( Color col );

	Texture* GetTextureById( int id );
	Texture* AllocTextureForId( int id );
	bool LoadTGA( Texture* texture, const char* filename );
	bool LoadBMP( Texture* texture, const char* filename );

	VPANEL							_embeddedPanel; // main panel
	VPANEL							_notifyPanel;
	VPANEL							_currentContextPanel;
	HTexture						_notifyIcon;
	CPopupList						_popupList;
	HCURSOR							_currentCursor;
	OSVERSIONINFOA					_osVersion;
	bool							_unicodeSupported;
	CUtlVector<CUtlSymbol>			m_CustomFontFileNames;
	HFont							_currentFont;
	CLinuxFont*						_currentFontObject;
	HPEN							_currentPen;
	bool							_needKB;
	bool							_needMouse;
	int								_textPos[2];
	Texture*						_currentTexture;
	CUtlRBTree<Texture, int>		m_VGuiSurfaceTextures;
	bool							_allowJavaScript;
	char							_language[32];
};

static LRESULT CALLBACK staticProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

static WNDCLASSA		staticWndclass;
static int				staticSurfaceAvailable;
static HCURSOR			staticDefaultCursor[20];
static KeyCode			s_pVirtualKeyToVGUI[256];
static UINT				staticShutdownMessage;
static ATOM				staticWndClassAtom;
static KeyCode			staticLastKeyCode;
static UINT				staticTaskbarCreatedMessage;

extern CWin32Surface g_Surface;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CWin32Surface, ISurface, "VGUI_Surface026", g_Surface );

CWin32Surface g_Surface;
static CSurfaceDragDropTarget g_SurfaceDragDropTarget;

static int staticTextureCurrentId = 2700;
static bool staticPrimeSoundSystem = true;

static HRESULT staticOleInitialize = OleInitialize( NULL );

static void InternalSchemeSettingsTraverse( VPanel* panel, bool forceApplySchemeSettings )
{
	for ( int i = 0; i < panel->GetChildCount(); i++ )
	{
		VPanel* child = panel->GetChild( i );
		if ( forceApplySchemeSettings || child->IsVisible() )
		{
			InternalSchemeSettingsTraverse( child, forceApplySchemeSettings );
		}
	}

	panel->Client()->PerformApplySchemeSettings();
}

static void InternalSolveTraverse( VPanel* panel )
{
	panel->Solve();

	for ( int i = 0; i < panel->GetChildCount(); i++ )
	{
		VPanel* child = panel->GetChild( i );
		if ( child->IsVisible() )
		{
			InternalSolveTraverse( child );
		}
	}
}

static void InternalThinkTraverse( VPanel* panel )
{
	panel->Client()->Think();

	for ( int i = 0; i < panel->GetChildCount(); i++ )
	{
		VPanel* child = panel->GetChild( i );
		if ( child->IsVisible() )
		{
			InternalThinkTraverse( child );
		}
	}
}

static HBITMAP CreateDIB( int wide, int tall, HDC hdc, WORD bitCount, void** ppBits )
{
	BITMAPINFO bitmapInfo;

	bitmapInfo.bmiHeader.biWidth = wide;
	bitmapInfo.bmiHeader.biHeight = -tall;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = bitCount;
	bitmapInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;

	return CreateDIBSection( hdc, &bitmapInfo, DIB_RGB_COLORS, ppBits, NULL, 0 );
}

static void HandleNotifyIconMessage( HWND hwnd, WPARAM wparam, LPARAM lparam )
{
	KeyValues* msg;

	UNREFERENCED_PARAMETER( wparam );

	switch ( lparam )
	{
	case WM_LBUTTONDBLCLK:
		SetForegroundWindow( hwnd );
		if ( !g_pSurface->GetNotifyPanel() )
		{
			return;
		}

		msg = new KeyValues( "NotifyIconMsg", "msg", "LBUTTONDBLCLK" );
		break;

	case WM_RBUTTONUP:
	case WM_CONTEXTMENU:
		if ( !g_pSurface->GetNotifyPanel() )
		{
			return;
		}

		msg = new KeyValues( "NotifyIconMsg", "msg", "CONTEXTMENU" );
		break;

	case WM_LBUTTONDOWN:
		if ( !g_Surface.GetNotifyPanel() )
		{
			return;
		}

		msg = new KeyValues( "NotifyIconMsg", "msg", "WM_LBUTTONDOWN" );
		break;

	default:
		return;
	}

	g_pIVgui->PostMessage( g_pSurface->GetNotifyPanel(), msg, NULL );
}

static LRESULT CALLBACK staticProc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
	VPANEL panel = NULL;
	IClientPanel* client = NULL;

	if ( staticSurfaceAvailable )
	{
		panel = g_pIVgui->HandleToPanel( GetWindowLongA( hwnd, GWL_USERDATA ) );
		if ( panel )
		{
			client = ( (VPanel*)panel )->Client();
		}
	}

	if ( msg == staticShutdownMessage )
	{
		g_pIVgui->ShutdownMessage( (unsigned int)lparam );
		return DefWindowProcA( hwnd, msg, wparam, lparam );
	}

	if ( msg == WM_ENDSESSION && wparam == TRUE )
	{
		if ( g_pSurface->GetEmbeddedPanel() )
		{
			g_pIPanel->SendMessage( g_pSurface->GetEmbeddedPanel(), new KeyValues( "WindowsEndSession" ), NULL );
		}

		return 0;
	}

	if ( !panel )
	{
		return DefWindowProcA( hwnd, msg, wparam, lparam );
	}

	switch ( msg )
	{
	case WM_CREATE:
		staticTaskbarCreatedMessage = RegisterWindowMessageA( "TaskbarCreated" );
		break;

	case WM_SETFOCUS:
		g_Surface.ActivatePanel( panel );
		break;

	case WM_KILLFOCUS:
		break;

	case WM_PAINT:
	{
		bool visible = g_pIPanel->IsVisible( panel );
		VPANEL parent = g_pIPanel->GetParent( panel );
		while ( parent && visible )
		{
			if ( !g_pIPanel->IsVisible( parent ) )
			{
				visible = false;
				break;
			}

			parent = g_pIPanel->GetParent( parent );
		}

		if ( visible )
		{
			PAINTSTRUCT ps;
			BeginPaint( hwnd, &ps );
			g_Surface.SolveTraverse( panel, false );
			//			solveTime = system()->GetCurrentTime();
			g_Surface.PaintTraverse( panel );
			EndPaint( hwnd, &ps );
		}

		ValidateRect( hwnd, NULL );
		break;
	}

	case WM_CLOSE:
		g_pIVgui->PostMessage( panel, new KeyValues( "Close" ), NULL );
		return 0;

	case WM_ERASEBKGND:
		client->Repaint();
		break;

	case WM_SETCURSOR:
//!!			SetCursor(staticCurrentCursor);
		break;

	case WM_MOUSEMOVE:
	{
		POINT pt;
		pt.x = (short)LOWORD( lparam );
		pt.y = (short)HIWORD( lparam );

		bool rightDown = ( ( wparam >> 1 ) & 1 ) != 0;
		bool middleDown = ( ( wparam >> 4 ) & 1 ) != 0;
		if ( !( wparam & MK_LBUTTON ) && g_pInput->IsMouseDown( MOUSE_LEFT ) )
		{
			g_pInput->InternalMouseReleased( MOUSE_LEFT );
		}

		if ( !rightDown && g_pInput->IsMouseDown( MOUSE_RIGHT ) )
		{
			g_pInput->InternalMouseReleased( MOUSE_LEFT );
		}

		if ( !middleDown && g_pInput->IsMouseDown( MOUSE_MIDDLE ) )
		{
			g_pInput->InternalMouseReleased( MOUSE_MIDDLE );
		}

		ClientToScreen( hwnd, &pt );
		g_pInput->InternalCursorMoved( pt.x, pt.y );
		break;
	}

	case WM_LBUTTONDOWN:
		g_pInput->InternalMousePressed( MOUSE_LEFT );
		break;

	case WM_LBUTTONUP:
		g_pInput->InternalMouseReleased( MOUSE_LEFT );
		break;

	case WM_LBUTTONDBLCLK:
		g_pInput->InternalMouseDoublePressed( MOUSE_LEFT );
		break;

	case WM_RBUTTONDOWN:
		g_pInput->InternalMousePressed( MOUSE_RIGHT );
		break;

	case WM_RBUTTONUP:
		g_pInput->InternalMouseReleased( MOUSE_RIGHT );
		break;

	case WM_RBUTTONDBLCLK:
		g_pInput->InternalMouseDoublePressed( MOUSE_RIGHT );
		break;

	case WM_MBUTTONDOWN:
		g_pInput->InternalMousePressed( MOUSE_MIDDLE );
		break;

	case WM_MBUTTONUP:
		g_pInput->InternalMouseReleased( MOUSE_MIDDLE );
		break;

	case WM_MBUTTONDBLCLK:
		g_pInput->InternalMouseDoublePressed( MOUSE_MIDDLE );
		break;

	case WM_MOUSEWHEEL:
		g_pInput->InternalMouseWheeled( ( (short)HIWORD( wparam ) ) / WHEEL_DELTA );
		break;

	case WM_XBUTTONDOWN:
		if ( HIWORD( wparam ) == XBUTTON1 )
		{
			g_pInput->InternalMousePressed( MOUSE_4 );
		}
		else
		{
			g_pInput->InternalMousePressed( MOUSE_5 );
		}
		break;

	case WM_XBUTTONUP:
		if ( HIWORD( wparam ) == XBUTTON1 )
		{
			g_pInput->InternalMouseReleased( MOUSE_4 );
		}
		else
		{
			g_pInput->InternalMouseReleased( MOUSE_5 );
		}
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		KeyCode code = s_pVirtualKeyToVGUI[wparam];
		staticLastKeyCode = code;

		if ( lparam & ( 1 << 30 ) )
		{
			g_pInput->InternalKeyCodeTyped( code );
		}
		else
		{
			g_pInput->InternalKeyCodePressed( code );
			g_pInput->InternalKeyCodeTyped( s_pVirtualKeyToVGUI[wparam] );
		}

		return DefWindowProcA( hwnd, msg, wparam, lparam );
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
		g_pInput->InternalKeyCodeReleased( s_pVirtualKeyToVGUI[wparam] );
		break;

	case WM_CHAR:
	case WM_SYSCHAR:
		g_pInput->InternalKeyTyped( (wchar_t)wparam );
		break;

	case WM_USER + 1:
		HandleNotifyIconMessage( hwnd, wparam, lparam );
		break;

	case WM_APP:
		break;

	default:
		if ( msg == staticTaskbarCreatedMessage )
		{
			g_pIVgui->PostMessage( panel, new KeyValues( "TaskbarRestart" ), NULL );
		}
		break;
	}

	return DefWindowProcA( hwnd, msg, wparam, lparam );
}

HRESULT STDMETHODCALLTYPE CSurfaceDragDropTarget::QueryInterface( REFIID riid, void** ppvObject )
{
	if ( riid == IID_IDropTarget )
	{
		*ppvObject = this;
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE CSurfaceDragDropTarget::AddRef()
{
	return ++m_iRefCount;
}

ULONG STDMETHODCALLTYPE CSurfaceDragDropTarget::Release()
{
	return --m_iRefCount;
}

HRESULT STDMETHODCALLTYPE CSurfaceDragDropTarget::DragEnter( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt,
	DWORD* pdwEffect )
{
	if ( m_pDragData )
	{
		m_pDragData->deleteThis();
	}

	m_pDragData = CreateDragData( pDataObj );
	return DragOver( grfKeyState, pt, pdwEffect );
}

HRESULT STDMETHODCALLTYPE CSurfaceDragDropTarget::DragOver( DWORD grfKeyState, POINTL pt, DWORD* pdwEffect )
{
	UNREFERENCED_PARAMETER( grfKeyState );
	UNREFERENCED_PARAMETER( pt );

	*pdwEffect = DROPEFFECT_NONE;

	if ( m_pDragData && g_pIVgui->IsRunning() )
	{
		VPANEL panel = g_pInput->GetMouseOver();
		if ( panel )
		{
			IClientPanel* client = ( (VPanel*)panel )->Client();
			if ( client->RequestInfo( m_pDragData ) )
			{
				*pdwEffect = DROPEFFECT_COPY;
			}
		}
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CSurfaceDragDropTarget::DragLeave()
{
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CSurfaceDragDropTarget::Drop( IDataObject* pDataObj, DWORD grfKeyState, POINTL pt,
	DWORD* pdwEffect )
{
	UNREFERENCED_PARAMETER( pDataObj );
	UNREFERENCED_PARAMETER( grfKeyState );
	UNREFERENCED_PARAMETER( pt );

	*pdwEffect = DROPEFFECT_NONE;

	if ( m_pDragData && g_pIVgui->IsRunning() )
	{
		VPANEL target = (VPANEL)m_pDragData->GetPtr( "AcceptPanel", NULL );
		if ( target && m_pDragData )
		{
			g_pIVgui->PostMessage( target, m_pDragData, NULL );
			m_pDragData = NULL;
		}

		if ( m_pDragData )
		{
			m_pDragData->deleteThis();
		}

		m_pDragData = NULL;
	}

	return S_OK;
}

KeyValues* CSurfaceDragDropTarget::CreateDragData( IDataObject* pDataObj )
{
	KeyValues* msg = NULL;
	FORMATETC formatEtc;
	STGMEDIUM medium;

	formatEtc.cfFormat = CF_TEXT;
	formatEtc.ptd = NULL;
	formatEtc.dwAspect = DVASPECT_CONTENT;
	formatEtc.lindex = -1;
	formatEtc.tymed = TYMED_HGLOBAL;

	if ( pDataObj->GetData( &formatEtc, &medium ) == S_OK )
	{
		if ( medium.tymed == TYMED_HGLOBAL )
		{
			const char* text = (const char*)GlobalLock( medium.hGlobal );
			msg = new KeyValues( "DragDrop", "type", "text", "text", text );
			GlobalUnlock( medium.hGlobal );
		}

		ReleaseStgMedium( &medium );
		return msg;
	}

	formatEtc.cfFormat = CF_HDROP;
	if ( pDataObj->GetData( &formatEtc, &medium ) != S_OK || medium.tymed != TYMED_HGLOBAL )
	{
		return msg;
	}

	msg = new KeyValues( "DragDrop", "type", "files" );
	KeyValues* fileList = msg->FindKey( "list", true );

	char fileName[512];
	HDROP hDrop = (HDROP)GlobalLock( medium.hGlobal );
	int fileCount = DragQueryFileA( hDrop, 0xFFFFFFFF, fileName, sizeof( fileName ) - 1 );
	for ( int i = 0; i < fileCount; i++ )
	{
		char key[32];
		sprintf( key, "%d", i );
		DragQueryFileA( hDrop, i, fileName, sizeof( fileName ) - 1 );
		fileList->SetString( key, fileName );
	}

	GlobalUnlock( medium.hGlobal );
	return msg;
}

CWin32Surface::CWin32Surface() :
	m_VGuiSurfaceTextures( 0, 128, TextureLessFunc )
{
	_currentCursor = NULL;
	_currentTexture = NULL;
	_language[0] = 0;

	InitStaticData();

	staticSurfaceAvailable = 0;
	_allowJavaScript = true;
	_currentFont = 0;
	_currentPen = NULL;
	_needKB = true;
	_needMouse = true;

	CoInitialize( NULL );

	_osVersion.dwOSVersionInfoSize = sizeof( _osVersion );
#pragma warning( push )
#pragma warning( disable : 4996 )
	GetVersionExA( &_osVersion );
#pragma warning( pop )
	_unicodeSupported = _osVersion.dwMajorVersion >= 5;

	_textPos[1] = 0;
	_textPos[0] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CWin32Surface::~CWin32Surface()
{
	staticSurfaceAvailable = 0;

	for ( int i = 0; i < m_VGuiSurfaceTextures.MaxElement(); i++ )
	{
		if ( !m_VGuiSurfaceTextures.IsValidIndex( i ) )
		{
			continue;
		}

		Texture& texture = m_VGuiSurfaceTextures[i];
		if ( texture._bitmap )
		{
			DeleteObject( texture._bitmap );
		}

		if ( texture._maskBitmap )
		{
			DeleteObject( texture._maskBitmap );
		}

		if ( texture._icon )
		{
			DestroyIcon( texture._icon );
		}
	}

	CoUninitialize();
}

bool CWin32Surface::TextureLessFunc( const Texture& lhs, const Texture& rhs )
{
	return lhs._id < rhs._id;
}

void CWin32Surface::InitStaticData()
{
	//load up all default cursors, this gets called everytime a Surface is created, but
	//who cares
	staticDefaultCursor[dc_none] = NULL;
	staticDefaultCursor[dc_arrow] = LoadCursorA( NULL, IDC_ARROW );
	staticDefaultCursor[dc_ibeam] = LoadCursorA( NULL, IDC_IBEAM );
	staticDefaultCursor[dc_hourglass] = LoadCursorA( NULL, IDC_WAIT );
	staticDefaultCursor[dc_waitarrow] = LoadCursorA( NULL, IDC_APPSTARTING );
	staticDefaultCursor[dc_crosshair] = LoadCursorA( NULL, IDC_CROSS );
	staticDefaultCursor[dc_up] = LoadCursorA( NULL, IDC_UPARROW );
	staticDefaultCursor[dc_sizenwse] = LoadCursorA( NULL, IDC_SIZENWSE );
	staticDefaultCursor[dc_sizenesw] = LoadCursorA( NULL, IDC_SIZENESW );
	staticDefaultCursor[dc_sizewe] = LoadCursorA( NULL, IDC_SIZEWE );
	staticDefaultCursor[dc_sizens] = LoadCursorA( NULL, IDC_SIZENS );
	staticDefaultCursor[dc_sizeall] = LoadCursorA( NULL, IDC_SIZEALL );
	staticDefaultCursor[dc_no] = LoadCursorA( NULL, IDC_NO );
	staticDefaultCursor[dc_hand] = LoadCursorA( NULL, IDC_HAND );

	int i;
	for ( i = 0; i < 10; i++ )
	{
		s_pVirtualKeyToVGUI['0' + i] = (KeyCode)( KEY_0 + i );
	}

	for ( i = 0; i < 26; i++ )
	{
		s_pVirtualKeyToVGUI['A' + i] = (KeyCode)( KEY_A + i );
	}

	for ( i = 0; i < 10; i++ )
	{
		s_pVirtualKeyToVGUI[VK_NUMPAD0 + i] = (KeyCode)( KEY_PAD_0 + i );
	}

	s_pVirtualKeyToVGUI[VK_DIVIDE] = KEY_PAD_DIVIDE;
	s_pVirtualKeyToVGUI[VK_MULTIPLY] = KEY_PAD_MULTIPLY;
	s_pVirtualKeyToVGUI[VK_SUBTRACT] = KEY_PAD_MINUS;
	s_pVirtualKeyToVGUI[VK_ADD] = KEY_PAD_PLUS;
	s_pVirtualKeyToVGUI[VK_DECIMAL] = KEY_PAD_DECIMAL;
	s_pVirtualKeyToVGUI[VK_OEM_4] = KEY_LBRACKET;
	s_pVirtualKeyToVGUI[VK_OEM_6] = KEY_RBRACKET;
	s_pVirtualKeyToVGUI[VK_OEM_1] = KEY_SEMICOLON;
	s_pVirtualKeyToVGUI[VK_OEM_7] = KEY_APOSTROPHE;
	s_pVirtualKeyToVGUI[VK_OEM_3] = KEY_BACKQUOTE;
	s_pVirtualKeyToVGUI[VK_OEM_COMMA] = KEY_COMMA;
	s_pVirtualKeyToVGUI[VK_OEM_PERIOD] = KEY_PERIOD;
	s_pVirtualKeyToVGUI[VK_OEM_2] = KEY_SLASH;
	s_pVirtualKeyToVGUI[VK_OEM_5] = KEY_BACKSLASH;
	s_pVirtualKeyToVGUI[VK_OEM_MINUS] = KEY_MINUS;
	s_pVirtualKeyToVGUI[VK_OEM_PLUS] = KEY_EQUAL;
	s_pVirtualKeyToVGUI[VK_RETURN] = KEY_ENTER;
	s_pVirtualKeyToVGUI[VK_SPACE] = KEY_SPACE;
	s_pVirtualKeyToVGUI[VK_BACK] = KEY_BACKSPACE;
	s_pVirtualKeyToVGUI[VK_TAB] = KEY_TAB;
	s_pVirtualKeyToVGUI[VK_CAPITAL] = KEY_CAPSLOCK;
	s_pVirtualKeyToVGUI[VK_NUMLOCK] = KEY_NUMLOCK;
	s_pVirtualKeyToVGUI[VK_ESCAPE] = KEY_ESCAPE;
	s_pVirtualKeyToVGUI[VK_SCROLL] = KEY_SCROLLLOCK;
	s_pVirtualKeyToVGUI[VK_INSERT] = KEY_INSERT;
	s_pVirtualKeyToVGUI[VK_DELETE] = KEY_DELETE;
	s_pVirtualKeyToVGUI[VK_HOME] = KEY_HOME;
	s_pVirtualKeyToVGUI[VK_END] = KEY_END;
	s_pVirtualKeyToVGUI[VK_PRIOR] = KEY_PAGEUP;
	s_pVirtualKeyToVGUI[VK_NEXT] = KEY_PAGEDOWN;
	s_pVirtualKeyToVGUI[VK_PAUSE] = KEY_BREAK;
	s_pVirtualKeyToVGUI[VK_SHIFT] = KEY_RSHIFT;
	s_pVirtualKeyToVGUI[VK_MENU] = KEY_RALT;
	s_pVirtualKeyToVGUI[VK_CONTROL] = KEY_RCONTROL;
	s_pVirtualKeyToVGUI[VK_LWIN] = KEY_LWIN;
	s_pVirtualKeyToVGUI[VK_RWIN] = KEY_RWIN;
	s_pVirtualKeyToVGUI[VK_APPS] = KEY_APP;
	s_pVirtualKeyToVGUI[VK_UP] = KEY_UP;
	s_pVirtualKeyToVGUI[VK_LEFT] = KEY_LEFT;
	s_pVirtualKeyToVGUI[VK_DOWN] = KEY_DOWN;
	s_pVirtualKeyToVGUI[VK_RIGHT] = KEY_RIGHT;

	for ( i = 0; i < 12; i++ )
	{
		s_pVirtualKeyToVGUI[VK_F1 + i] = (KeyCode)( KEY_F1 + i );
	}

	// make and register a very simple Window Class
	memset( &staticWndclass, 0, sizeof( staticWndclass ) );
	staticWndclass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	staticWndclass.lpfnWndProc = staticProc;
	staticWndclass.hInstance = GetModuleHandleA( NULL );

	WORD iconId = 101;
	const char* bootstrapperIcon = getenv( "__STEAM_BOOTSTRAPPER_ICON_ID__" );
	if ( bootstrapperIcon )
	{
		unsigned int requestedId = 0;
		if ( sscanf( bootstrapperIcon, "%u", &requestedId ) == 1 && requestedId > 101 )
		{
			iconId = (WORD)requestedId;
		}
	}

	staticWndclass.hIcon = LoadIconA( staticWndclass.hInstance, MAKEINTRESOURCEA( iconId ) );
	staticWndclass.lpszClassName = "Surface";
	staticWndClassAtom = RegisterClassA( &staticWndclass );

	staticShutdownMessage = RegisterWindowMessageA( "ShutdownValvePlatform" );
}

SurfacePlat* CWin32Surface::ContextPlat()
{
	return ( (VPanel*)_currentContextPanel )->Plat();
}

void CWin32Surface::SetPenColor( Color col )
{
	HPEN oldPen = _currentPen;

	_currentPen = CreatePen( PS_SOLID, 0, RGB( col[0], col[1], col[2] ) );
	if ( _currentPen )
	{
		SelectObject( ContextPlat()->hdc, _currentPen );
	}

	if ( oldPen )
	{
		DeleteObject( oldPen );
	}
}

void CWin32Surface::Shutdown()
{
	g_pSystem->SaveUserConfigFile();
	g_pSystem->Shutdown();
	g_pScheme->Shutdown( true );
	g_pIVgui->Shutdown();

	FontManager().ClearAllFonts();

	for ( int i = 0; i < m_CustomFontFileNames.Count(); i++ )
	{
		RemoveFontResourceExA( m_CustomFontFileNames[i].String(), FR_PRIVATE, NULL );
	}

	m_CustomFontFileNames.RemoveAll();

	staticSurfaceAvailable = 0;

	// release any panels still with surfaces
	while ( GetPopupCount() )
	{
		ReleasePanel( GetPopup( 0 ) );
	}

	UnregisterClassA( "Surface", GetModuleHandleA( NULL ) );
	staticWndClassAtom = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Handles windows message pump
//-----------------------------------------------------------------------------
void CWin32Surface::RunFrame()
{
	MSG msg;

	if ( g_pIVgui->GetShouldVGuiControlSleep() || PeekMessageA( &msg, NULL, 0, 0, PM_NOREMOVE ) )
	{
		do
		{
			int result = GetMessageA( &msg, NULL, 0, 0 );
			if ( result == 0 )
			{
				break;
			}

			if ( result == -1 )
			{
				g_pIVgui->Stop();
				return;
			}

			TranslateMessage( &msg );
			DispatchMessageA( &msg );
		} while ( PeekMessageA( &msg, NULL, 0, 0, PM_NOREMOVE ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CWin32Surface::GetEmbeddedPanel()
{
	return _embeddedPanel;
}

void CWin32Surface::SetEmbeddedPanel( VPANEL pEmbeddedPanel )
{
	staticSurfaceAvailable = 1;
	_embeddedPanel = pEmbeddedPanel;
	SetContextPanel( pEmbeddedPanel );
	CreatePopup( pEmbeddedPanel, false, true, false, true, true );

	SetTimer( ContextPlat()->hwnd, 0, 50, NULL );

	const char* language = _language;
	char registryLanguage[64];
	if ( !language[0] )
	{
		language = registryLanguage;
		if ( !g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", registryLanguage,
			sizeof( registryLanguage ) - 1 ) )
		{
			language = "english";
		}
	}

	FontManager().SetLanguage( language );
}

void CWin32Surface::PushMakeCurrent( VPANEL pPanel, bool useInsets )
{
	VPanel* panel = (VPanel*)pPanel;

	VPanel* window = panel;
	while ( window && !window->Plat() )
	{
		window = window->GetParent();
	}

	SetContextPanel( (VPANEL)window );
	_currentFontObject = NULL;

	if ( !window )
	{
		g_pIVgui->DPrintf( "Warning: VPanel with no drawing context\n" );
	}

	int inset[4];
	panel->Client()->GetInset( inset[0], inset[1], inset[2], inset[3] );
	if ( !useInsets )
	{
		inset[0] = 0;
		inset[1] = 0;
		inset[2] = 0;
		inset[3] = 0;
	}

	VPanel* context = (VPanel*)_currentContextPanel;
	int contextX, contextY, contextWide, contextTall;
	context->GetAbsPos( contextX, contextY );
	context->GetSize( contextWide, contextTall );
	contextWide += contextX;
	contextTall += contextY;

	int absX, absY, wide, tall;
	panel->GetAbsPos( absX, absY );
	panel->GetSize( wide, tall );
	wide += absX;
	tall += absY;

	int clipRect[4];
	panel->Client()->GetClipRect( clipRect[0], clipRect[1], clipRect[2], clipRect[3] );

	int originX, originY;
	if ( _currentContextPanel == pPanel )
	{
		originX = inset[0];
		originY = inset[1];
	}
	else
	{
		originX = absX + inset[0] - contextX;
		originY = absY + inset[1] - contextY;
	}

	SetViewportOrgEx( ContextPlat()->hdc, originX, originY, NULL );
	SetRectRgn( ContextPlat()->clipRgn, clipRect[0] - contextX, clipRect[1] - contextY,
		clipRect[2] - inset[2] - contextX, clipRect[3] - inset[3] - contextY );
	SelectObject( ContextPlat()->hdc, ContextPlat()->clipRgn );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::PopMakeCurrent( VPANEL panel )
{
	if ( panel == _currentContextPanel )
	{
		SetContextPanel( _embeddedPanel );
	}
}

void CWin32Surface::DrawSetColor( int r, int g, int b, int a )
{
	UNREFERENCED_PARAMETER( a );

	SetBkColor( ContextPlat()->hdc, RGB( r, g, b ) );
}

void CWin32Surface::DrawSetColor( Color col )
{
	DrawSetColor( col[0], col[1], col[2], col[3] );
}

void CWin32Surface::DrawFilledRect( int x0, int y0, int x1, int y1 )
{
	RECT rect;

	rect.left = x0;
	rect.top = y0;
	rect.right = x1;
	rect.bottom = y1;
	ExtTextOutA( ContextPlat()->hdc, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL );
}

void CWin32Surface::DrawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	DrawFilledRect( x0, y0, x1, y0 + 1 ); // top
	DrawFilledRect( x0, y1 - 1, x1, y1 ); // bottom
	DrawFilledRect( x0, y0 + 1, x0 + 1, y1 - 1 ); // left
	DrawFilledRect( x1 - 1, y0 + 1, x1, y1 - 1 ); // right
}

void CWin32Surface::DrawLine( int x0, int y0, int x1, int y1 )
{
	POINT points[2];

	points[0].x = x0;
	points[0].y = y0;
	points[1].x = x1;
	points[1].y = y1;
	Polyline( ContextPlat()->hdc, points, 2 );
}

void CWin32Surface::DrawPolyLine( int* px, int* py, int numPoints )
{
	POINT* points = (POINT*)malloc( sizeof( POINT ) * numPoints );
	if ( !points )
	{
		return;
	}

	for ( int i = 0; i < numPoints; i++ )
	{
		points[i].x = px[i];
		points[i].y = py[i];
	}

	Polyline( ContextPlat()->hdc, points, numPoints );
	free( points );
}

void CWin32Surface::DrawSetTextFont( HFont font )
{
	_currentFont = font;
}

void CWin32Surface::DrawSetTextColor( int r, int g, int b, int a )
{
	SetPenColor( Color( r, g, b, a ) );
	SetTextColor( ContextPlat()->hdc, RGB( r, g, b ) );
}

void CWin32Surface::DrawSetTextColor( Color col )
{
	SetPenColor( col );
	// end then for text
	DrawSetTextColor( col[0], col[1], col[2], col[3] );
}

void CWin32Surface::DrawSetTextPos( int x, int y )
{
	MoveToEx( ContextPlat()->hdc, x, y, NULL );
	_textPos[1] = y;
	_textPos[0] = x;
}

void CWin32Surface::DrawGetTextPos( int& x, int& y )
{
	x = _textPos[0];
	y = _textPos[1];
}

void CWin32Surface::DrawPrintText( const wchar_t* text, int textLen )
{
	if ( !text )
	{
		_wassert( L"text", SURFACE_SOURCE_PATH, 1145 );
	}

	for ( int i = 0; i < textLen; i++ )
	{
		DrawUnicodeChar( text[i] );
	}
//	ExtTextOut(_currentContextPanel->Plat()->hdc, 0, 0, 0, NULL, text, textLen, NULL);
}

void CWin32Surface::DrawUnicodeChar( wchar_t wch )
{
	CLinuxFont* font = FontManager().GetFontForChar( _currentFont, wch );
	if ( !font )
	{
		return;
	}

	if ( _currentFontObject != font )
	{
		font->SetAsActiveFont( ContextPlat()->hdc );
		_currentFontObject = font;
	}

	if ( _unicodeSupported )
	{
		ExtTextOutW( ContextPlat()->hdc, 0, 0, 0, NULL, &wch, 1, NULL );
	}
	else
	{
		char mbcs[6] = { 0 };
		WideCharToMultiByte( CP_ACP, 0, &wch, 1, mbcs, sizeof( mbcs ), NULL, NULL );
		ExtTextOutA( ContextPlat()->hdc, 0, 0, 0, NULL, mbcs, (UINT)strlen( mbcs ), NULL );
	}
}

void CWin32Surface::DrawUnicodeCharAdd( wchar_t wch )
{
	CLinuxFont* font = FontManager().GetFontForChar( _currentFont, wch );
	if ( !font )
	{
		return;
	}

	if ( _currentFontObject != font )
	{
		font->SetAsActiveFont( ContextPlat()->hdc );
		_currentFontObject = font;
	}

	ExtTextOutW( ContextPlat()->hdc, 0, 0, 0, NULL, &wch, 1, NULL );
}

//-----------------------------------------------------------------------------
// Purpose: does nothing, since we don't need this optimization in win32
//-----------------------------------------------------------------------------
void CWin32Surface::DrawFlushText()
{
}

IHTML* CWin32Surface::CreateHTMLWindow( IHTMLEvents* events, VPANEL context )
{
	UNREFERENCED_PARAMETER( events );
	UNREFERENCED_PARAMETER( context );

	return NULL;
}

void CWin32Surface::PaintHTMLWindow( IHTML* htmlwin )
{
	UNREFERENCED_PARAMETER( htmlwin );
}

void CWin32Surface::DeleteHTMLWindow( IHTML* htmlwin )
{
	UNREFERENCED_PARAMETER( htmlwin );
}

Texture* CWin32Surface::GetTextureById( int id )
{
	Texture findTexture;

	memset( &findTexture, 0, sizeof( findTexture ) );
	findTexture._id = id;

	int index = m_VGuiSurfaceTextures.Find( findTexture );
	if ( !m_VGuiSurfaceTextures.IsValidIndex( index ) )
	{
		return NULL;
	}

	return &m_VGuiSurfaceTextures[index];
}

Texture* CWin32Surface::AllocTextureForId( int id )
{
	Texture newTexture;

	memset( &newTexture, 0, sizeof( newTexture ) );
	newTexture._id = id;

	int index = m_VGuiSurfaceTextures.Insert( newTexture );
	return &m_VGuiSurfaceTextures[index];
}

//-----------------------------------------------------------------------------
// Purpose: Maps a texture file to an id, and makes it the current drawing texture
//			tries to load as a .tga first, and if not found as a .bmp
//-----------------------------------------------------------------------------
void CWin32Surface::DrawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload )
{
	UNREFERENCED_PARAMETER( hardwareFilter );

	Texture* texture = GetTextureById( id );
	if ( texture )
	{
		if ( !_stricmp( filename, texture->_filename ) && !forceReload )
		{
			_currentTexture = texture;
			return;
		}

		if ( texture->_bitmap )
		{
			DeleteObject( texture->_bitmap );
		}

		if ( texture->_maskBitmap )
		{
			DeleteObject( texture->_maskBitmap );
		}
	}
	else
	{
		texture = AllocTextureForId( id );
		memset( texture, 0, sizeof( Texture ) );
	}

	texture->_id = id;
	texture->_filename = filename;

	if ( LoadTGA( texture, filename ) )
	{
		_currentTexture = texture;
	}
	else
	{
		g_pIVgui->DPrintf2( "Error: texture file '%s' does not exist or is invalid\n", filename );
	}
}

void CWin32Surface::DrawSetTextureRGBA( int id, const unsigned char* rgba, int wide, int tall, int hardwareFilter,
	bool forceReload )
{
	UNREFERENCED_PARAMETER( hardwareFilter );
	UNREFERENCED_PARAMETER( forceReload );

	Texture* texture = GetTextureById( id );
	if ( texture )
	{
		if ( texture->_bitmap )
		{
			DeleteObject( texture->_bitmap );
		}

		if ( texture->_maskBitmap )
		{
			DeleteObject( texture->_maskBitmap );
		}
	}
	else
	{
		texture = AllocTextureForId( id );
		texture->_bitmap = NULL;
		texture->_maskBitmap = NULL;
		texture->_masked = false;
		texture->_maskBits = NULL;
	}

	// no texture or forced  load the new texture
	texture->_id = id;
	texture->_filename = NULL;
	texture->_wide = wide;
	texture->_tall = tall;
	texture->_icon = NULL;
	texture->_bits = NULL;

	BITMAPINFO bitmapInfo;
	HDC hdc = ContextPlat()->hdc;
	bitmapInfo.bmiHeader.biWidth = texture->_wide;
	bitmapInfo.bmiHeader.biHeight = -texture->_tall;
	bitmapInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;
	texture->_bitmap = CreateDIBSection( hdc, &bitmapInfo, DIB_RGB_COLORS, &texture->_bits, NULL, 0 );

	// copy over the texture data
	memcpy( texture->_bits, rgba, wide * tall * 4 );
	_currentTexture = texture;
}

void CWin32Surface::DrawSetTexture( int id )
{
	_currentTexture = GetTextureById( id );
}

//-----------------------------------------------------------------------------
// Purpose: Called by vgui to get texture dimensions
//-----------------------------------------------------------------------------
void CWin32Surface::DrawGetTextureSize( int id, int& wide, int& tall )
{
	Texture* texture = GetTextureById( id );
	if ( texture )
	{
		wide = texture->_wide;
		tall = texture->_tall;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::DrawTexturedRect( int x0, int y0, int x1, int y1 )
{
	if ( !_currentTexture || !ContextPlat()->textureDC )
	{
		return;
	}

	Texture* texture = _currentTexture;
	int srcWide = texture->_wide;
	int srcTall = texture->_tall;
	HBITMAP bitmap = texture->_bitmap;
	int wide = x1 - x0;
	int tall = y1 - y0;

	HGDIOBJ oldBitmap;
	if ( texture->_masked )
	{
		oldBitmap = SelectObject( ContextPlat()->textureDC, texture->_maskBitmap );
		StretchBlt( ContextPlat()->hdc, x0, y0, wide, tall, ContextPlat()->textureDC, 0, 0, srcWide, srcTall, SRCAND );
		SelectObject( ContextPlat()->textureDC, bitmap );
		StretchBlt( ContextPlat()->hdc, x0, y0, wide, tall, ContextPlat()->textureDC, 0, 0, srcWide, srcTall,
			SRCPAINT );
	}
	else
	{
		oldBitmap = SelectObject( ContextPlat()->textureDC, bitmap );
		StretchBlt( ContextPlat()->hdc, x0, y0, wide, tall, ContextPlat()->textureDC, 0, 0, srcWide, srcTall, SRCCOPY );
	}

	SelectObject( ContextPlat()->textureDC, oldBitmap );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the texture id has a valid texture bound to it
//-----------------------------------------------------------------------------
bool CWin32Surface::IsTextureIDValid( int id )
{
	return GetTextureById( id ) != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: allocates a new texture id
//-----------------------------------------------------------------------------
int CWin32Surface::CreateNewTextureID( bool procedural )
{
	UNREFERENCED_PARAMETER( procedural );

	return staticTextureCurrentId++;
}

#pragma warning( push )
#pragma warning( disable : 4701 )
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWin32Surface::LoadTGA( Texture* texture, const char* filename )
{
	char path[1024];
	_snprintf( path, sizeof( path ), "%s.tga", filename );

	FileHandle_t file = g_pFileSystem->Open( path, "rb", "SKIN" );
	if ( !file )
	{
		file = g_pFileSystem->Open( path, "rb", NULL );
		if ( !file )
		{
			return LoadBMP( texture, filename );
		}
	}

	TGAHeader_t header;
	g_pFileSystem->Read( &header, sizeof( header ), file );

	if ( header.image_type != 2 && header.image_type != 10 )
	{
		g_pIVgui->DPrintf2( "Error: texture file '%s' has invalid image_type %d\n", filename, header.image_type );
		return false;
	}

	if ( header.colormap_type != 0 || ( header.pixel_size != 24 && header.pixel_size != 32 ) )
	{
		return false;
	}

	if ( header.id_length != 0 )
	{
		g_pFileSystem->Seek( file, header.id_length, FILESYSTEM_SEEK_CURRENT );
	}

	bool hasAlpha = false;
	int columns = header.width;
	int rows = header.height;
	unsigned char* rgba = (unsigned char*)malloc( columns * rows * 4 );

	if ( header.image_type == 2 )
	{
		for ( int row = rows - 1; row >= 0; row-- )
		{
			unsigned char* pixbuf = rgba + row * columns * 4;
			for ( int column = 0; column < columns; column++ )
			{
				switch ( header.pixel_size )
				{
				case 24:
					g_pFileSystem->Read( pixbuf + 2, 1, file );
					g_pFileSystem->Read( pixbuf + 1, 1, file );
					g_pFileSystem->Read( pixbuf + 0, 1, file );
					pixbuf[3] = 255;
					pixbuf += 4;
					break;

				case 32:
					g_pFileSystem->Read( pixbuf + 2, 1, file );
					g_pFileSystem->Read( pixbuf + 1, 1, file );
					g_pFileSystem->Read( pixbuf + 0, 1, file );
					g_pFileSystem->Read( pixbuf + 3, 1, file );

					pixbuf[3] = ( pixbuf[3] == 0 ) ? 255 : 0;
					hasAlpha = true;
					pixbuf += 4;
					break;
				}
			}
		}
	}
	else
	{
		unsigned char red, green, blue, alpha;

		for ( int row = rows - 1; row >= 0; row-- )
		{
			unsigned char* pixbuf = rgba + row * columns * 4;
			for ( int column = 0; column < columns; )
			{
				unsigned char packetHeader;
				g_pFileSystem->Read( &packetHeader, 1, file );
				unsigned char packetSize = (unsigned char)( 1 + ( packetHeader & 0x7f ) );

				if ( packetHeader & 0x80 )
				{
					switch ( header.pixel_size )
					{
					case 24:
						g_pFileSystem->Read( &blue, 1, file );
						g_pFileSystem->Read( &green, 1, file );
						g_pFileSystem->Read( &red, 1, file );
						alpha = 255;
						break;

					case 32:
						g_pFileSystem->Read( &blue, 1, file );
						g_pFileSystem->Read( &green, 1, file );
						g_pFileSystem->Read( &red, 1, file );
						g_pFileSystem->Read( &alpha, 1, file );
						hasAlpha = true;
						break;
					}

					for ( unsigned char j = 0; j < packetSize; j++ )
					{
						pixbuf[0] = red;
						pixbuf[1] = green;
						pixbuf[2] = blue;
						pixbuf[3] = alpha;
						pixbuf += 4;

						column++;
						if ( column == columns )
						{
							column = 0;
							if ( row > 0 )
							{
								row--;
							}
							else
							{
								goto breakOut;
							}

							pixbuf = rgba + row * columns * 4;
						}
					}
				}
				else
				{
					for ( unsigned char j = 0; j < packetSize; j++ )
					{
						switch ( header.pixel_size )
						{
						case 24:
							g_pFileSystem->Read( pixbuf + 2, 1, file );
							g_pFileSystem->Read( pixbuf + 1, 1, file );
							g_pFileSystem->Read( pixbuf + 0, 1, file );
							pixbuf[3] = 255;
							pixbuf += 4;
							break;

						case 32:
							g_pFileSystem->Read( pixbuf + 2, 1, file );
							g_pFileSystem->Read( pixbuf + 1, 1, file );
							g_pFileSystem->Read( pixbuf + 0, 1, file );
							g_pFileSystem->Read( pixbuf + 3, 1, file );
							pixbuf += 4;
							break;
						}

						column++;
						if ( column == columns )
						{
							column = 0;
							if ( row > 0 )
							{
								row--;
							}
							else
							{
								goto breakOut;
							}

							pixbuf = rgba + row * columns * 4;
						}
					}
				}
			}
		}

	breakOut:;
	}

	g_pFileSystem->Close( file );

	texture->_wide = columns;
	texture->_tall = rows;
	texture->_icon = NULL;

	BITMAPINFO bitmapInfo;
	HDC hdc = ContextPlat()->hdc;
	bitmapInfo.bmiHeader.biWidth = columns;
	bitmapInfo.bmiHeader.biHeight = -rows;
	bitmapInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;
	texture->_bitmap = CreateDIBSection( hdc, &bitmapInfo, DIB_RGB_COLORS, &texture->_bits, NULL, 0 );

	texture->_masked = hasAlpha;
	if ( hasAlpha )
	{
		hdc = ContextPlat()->hdc;
		bitmapInfo.bmiHeader.biWidth = columns;
		bitmapInfo.bmiHeader.biHeight = -rows;
		bitmapInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;
		bitmapInfo.bmiHeader.biSizeImage = 0;
		bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
		bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
		bitmapInfo.bmiHeader.biClrUsed = 0;
		bitmapInfo.bmiHeader.biClrImportant = 0;
		texture->_maskBitmap = CreateDIBSection( hdc, &bitmapInfo, DIB_RGB_COLORS, &texture->_maskBits, NULL, 0 );
	}
	else
	{
		texture->_maskBitmap = NULL;
	}

	for ( int y = 0; y < texture->_tall; y++ )
	{
		for ( int x = 0; x < texture->_wide; x++ )
		{
			int offset = ( x + y * texture->_wide ) * 4;
			unsigned char* dest = (unsigned char*)texture->_bits + offset;
			unsigned char* src = rgba + offset;

			if ( hasAlpha )
			{
				unsigned int* mask = (unsigned int*)( (unsigned char*)texture->_maskBits + offset );
				if ( src[3] )
				{
					*mask = 0xFFFFFFFF;
					dest[0] = 0;
					dest[1] = 0;
					dest[2] = 0;
					dest[3] = 0;
				}
				else
				{
					*mask = 0;
					dest[0] = src[2];
					dest[1] = src[1];
					dest[2] = src[0];
					dest[3] = src[3];
				}
			}
			else
			{
				dest[0] = src[2];
				dest[1] = src[1];
				dest[2] = src[0];
				dest[3] = src[3];
			}
		}
	}

	free( rgba );
	return true;
}
#pragma warning( pop )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWin32Surface::LoadBMP( Texture* texture, const char* filename )
{
	char path[1024];
	_snprintf( path, sizeof( path ), "%s.bmp", filename );

	FileHandle_t file = g_pFileSystem->Open( path, "rb", "SKIN" );
	if ( !file )
	{
		file = g_pFileSystem->Open( path, "rb", NULL );
		if ( !file )
		{
			return false;
		}
	}

	bool success = false;
	unsigned int fileSize = g_pFileSystem->Size( file );

	BITMAPFILEHEADER fileHeader;
	g_pFileSystem->Read( &fileHeader, sizeof( fileHeader ), file );
	if ( fileHeader.bfType == 0x4D42 )
	{
		SIZE_T dataSize = fileSize - sizeof( fileHeader );
		HGLOBAL hMem = GlobalAlloc( GHND, dataSize );
		BITMAPINFO* bitmapInfo = (BITMAPINFO*)GlobalLock( hMem );
		g_pFileSystem->Read( bitmapInfo, (int)dataSize, file );

		// we now have a block of memory, rgba
		// throw a bitmap header on it and register it in windows
		texture->_wide = bitmapInfo->bmiHeader.biWidth;
		texture->_tall = bitmapInfo->bmiHeader.biHeight;
		texture->_icon = NULL;
		texture->_masked = false;
		texture->_bitmap = CreateDIB( texture->_wide, texture->_tall, ContextPlat()->hdc, 32, &texture->_bits );

		unsigned char* pixels = (unsigned char*)bitmapInfo + sizeof( BITMAPINFOHEADER ) + 256 * sizeof( RGBQUAD );
		for ( int y = 0; y < texture->_tall; y++ )
		{
			for ( int x = 0; x < texture->_wide; x++ )
			{
				unsigned char index = pixels[x + texture->_wide * ( texture->_tall - y - 1 )];
				unsigned char* dest = (unsigned char*)texture->_bits + ( x + y * texture->_wide ) * 4;
				dest[0] = bitmapInfo->bmiColors[index].rgbRed;
				dest[1] = bitmapInfo->bmiColors[index].rgbGreen;
				dest[2] = bitmapInfo->bmiColors[index].rgbBlue;
				dest[3] = 255;
			}
		}

		success = true;
		GlobalUnlock( hMem );
		GlobalFree( hMem );
	}

	g_pFileSystem->Close( file );
	return success;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::GetScreenSize( int& wide, int& tall )
{
	VPanel* embedded = (VPanel*)_embeddedPanel;
	if ( !embedded )
	{
		return;
	}

	wide = GetDeviceCaps( embedded->Plat()->hdc, HORZRES );
	tall = GetDeviceCaps( embedded->Plat()->hdc, VERTRES );
}

//-----------------------------------------------------------------------------
// Purpose: 
// HWND_TOPMOST - Places the window above all non-topmost windows. 
// The window maintains its topmost position even when it is deactivated.
// HWND_NOTOPMOST - Places the window above all non-topmost windows (that is, behind 
// all topmost windows). This flag has no effect if the window is already a non-topmost window.
//-----------------------------------------------------------------------------
void CWin32Surface::SetAsTopMost( VPANEL panel, bool state )
{
	VPanel* window;
	for ( window = (VPanel*)panel; window; window = window->GetParent() )
	{
		if ( window->Plat() )
		{
			break;
		}
	}

	UINT flags = SWP_NOMOVE | SWP_NOSIZE;
	if ( window->Plat()->disabled )
	{
		flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;
	}

	if ( !state )
	{
		SetWindowPos( window->Plat()->hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, flags );
		return;
	}

	SetFocus( window->Plat()->hwnd );
	SetWindowPos( window->Plat()->hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags );
}

void CWin32Surface::BringToFront( VPANEL panel )
{
	VPanel* window = (VPanel*)panel;
	if ( !window )
	{
		return;
	}

	while ( !window->Plat() )
	{
		window = window->GetParent();
		if ( !window )
		{
			return;
		}
	}

	if ( window->Plat() )
	{
		SetActiveWindow( window->Plat()->hwnd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: puts the thread that created the specified window into the foreground 
//          and activates the window.
//-----------------------------------------------------------------------------
void CWin32Surface::SetForegroundWindow( VPANEL panel )
{
	VPanel* window = (VPanel*)panel;
	if ( window && window->Plat() )
	{
		::SetForegroundWindow( window->Plat()->hwnd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetPanelVisible( VPANEL panel, bool state )
{
	VPanel* window = (VPanel*)panel;
	if ( !window->Plat() )
	{
		return;
	}

	if ( state )
	{
		ShowWindow( window->Plat()->hwnd, SW_SHOWNA );
	}
	else
	{
		ShowWindow( window->Plat()->hwnd, SW_HIDE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::SetMinimized( VPANEL panel, bool state )
{
	VPanel* window = (VPanel*)panel;
	if ( !window->Plat() )
	{
		return;
	}

	if ( state )
	{
		ShowWindow( window->Plat()->hwnd, SW_MINIMIZE );
	}
	else
	{
		ShowWindow( window->Plat()->hwnd, SW_SHOWNORMAL );
	}
}

bool CWin32Surface::IsMinimized( VPANEL panel )
{
	VPanel* window = (VPanel*)panel;
	if ( !window->Plat()->hwnd )
	{
		return false;
	}

	return IsIconic( window->Plat()->hwnd ) != 0;
}

void CWin32Surface::FlashWindow( VPANEL panel, bool state )
{
	VPanel* window = (VPanel*)panel;
	if ( window->Plat() )
	{
		::FlashWindow( window->Plat()->hwnd, state );
	}
}

void CWin32Surface::SetTitle( VPANEL panel, const wchar_t* title )
{
	VPanel* window = (VPanel*)panel;
	if ( !window )
	{
		return;
	}

	while ( !window->Plat() )
	{
		window = window->GetParent();
		if ( !window )
		{
			return;
		}
	}

	if ( _unicodeSupported )
	{
		SetWindowTextW( window->Plat()->hwnd, title );
	}
	else
	{
		char ansiTitle[512];
		WideCharToMultiByte( CP_ACP, 0, title, -1, ansiTitle, sizeof( ansiTitle ), NULL, NULL );
		SetWindowTextA( window->Plat()->hwnd, ansiTitle );
	}
}

void CWin32Surface::SetAsToolBar( VPANEL panel, bool state )
{
	VPanel* window = (VPanel*)panel;
	if ( !window )
	{
		return;
	}

	while ( !window->Plat() )
	{
		window = window->GetParent();
		if ( !window )
		{
			return;
		}
	}

	if ( window->Plat() )
	{
		HWND hwnd = window->Plat()->hwnd;
		LONG exStyle = GetWindowLongA( hwnd, GWL_EXSTYLE );
		if ( state )
		{
			SetWindowLongA( window->Plat()->hwnd, GWL_EXSTYLE, exStyle | WS_EX_TOOLWINDOW );
		}
		else
		{
			SetWindowLongA( window->Plat()->hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TOOLWINDOW );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::CreatePopup( VPANEL panel, bool minimized, bool showTaskbarIcon, bool disabled, bool mouseInput,
	bool kbInput )
{
	VPanel* popup = (VPanel*)panel;
	if ( popup->IsPopup() && popup->Plat() )
	{
		return;
	}

	if ( !popup->GetParent() && panel != _embeddedPanel )
	{
		popup->SetParent( (VPanel*)_embeddedPanel );
	}

	int x, y, wide, tall;
	popup->GetPos( x, y );
	popup->GetSize( wide, tall );
	popup->SetPopup( true );
	popup->SetKeyBoardInputEnabled( kbInput );
	popup->SetMouseInputEnabled( mouseInput );

	HWND parentHwnd = NULL;
	VPanel* parent = popup;
	while ( !parent->Plat() )
	{
		parent = parent->GetParent();
		if ( !parent )
		{
			break;
		}
	}

	if ( parent && parent != (VPanel*)_embeddedPanel )
	{
		parentHwnd = parent->Plat()->hwnd;
	}

	//create the window and initialize platform specific data
	//window is initial a popup and not visible
	//when ApplyChanges is called the window will be shown unless
	//it SetVisible(false) is called
	SurfacePlat* plat = new SurfacePlat();
	popup->SetPlat( plat );

	DWORD style;
	DWORD exStyle = 0;
	if ( showTaskbarIcon )
	{
		style = WS_POPUP | WS_CLIPCHILDREN | WS_SYSMENU | WS_MINIMIZEBOX;
	}
	else
	{
		style = WS_POPUP | WS_CLIPCHILDREN;
		exStyle = WS_EX_TOOLWINDOW;
		if ( !parentHwnd )
		{
			parentHwnd = ( (VPanel*)_embeddedPanel )->Plat()->hwnd;
		}
	}

	if ( panel != _embeddedPanel && popup->IsVisible() )
	{
		style |= WS_VISIBLE;
	}

	if ( disabled )
	{
		style |= WS_DISABLED;
	}

	if ( minimized )
	{
		style |= WS_MINIMIZE;
	}

	plat->hwnd = CreateWindowExA( exStyle, "Surface", "", style, x, y, wide, tall, parentHwnd, NULL,
		GetModuleHandleA( NULL ), NULL );
	plat->clipRgn = CreateRectRgn( 0, 0, 64, 64 );
	plat->hdc = CreateCompatibleDC( NULL );
	plat->hwndDC = NULL;
	plat->bitmap = NULL;
	plat->bitmapSize[0] = 0;
	plat->bitmapSize[1] = 0;
	plat->reservedFlag = false;
	plat->panel = panel;
	plat->disabled = disabled;
	plat->notifyIcon = NULL;
	plat->textureDC = NULL;

	SetBkMode( plat->hdc, TRANSPARENT );
	SetWindowLongA( plat->hwnd, GWL_USERDATA, (LONG)g_pIVgui->PanelToHandle( panel ) );
	SetTextAlign( plat->hdc, TA_UPDATECP );

	if ( !popup->IsVisible() || panel == _embeddedPanel )
	{
		SetPanelVisible( panel, false );
	}

	// create the context
	RecreateContext( panel );
	RegisterDragDrop( plat->hwnd, &g_SurfaceDragDropTarget );

	if ( _popupList.Find( panel ) < 0 )
	{
		_popupList.AddElement( panel );
	}
	else
	{
		__debugbreak();
	}

	if ( staticPrimeSoundSystem )
	{
		::PlaySoundA( "", NULL, SND_FILENAME | SND_NOWAIT | SND_NOSTOP | SND_NODEFAULT | SND_ASYNC );
		staticPrimeSoundSystem = false;
	}

	SetContextPanel( _embeddedPanel );
}

//-----------------------------------------------------------------------------
// Purpose: Called after a Paint to display the new buffer
//-----------------------------------------------------------------------------
void CWin32Surface::SwapBuffers( VPANEL panel )
{
	VPanel* window = (VPanel*)panel;
	if ( !window->Plat() )
	{
		return;
	}

	SurfacePlat* plat = window->Plat();

	int wide, tall;
	window->GetSize( wide, tall );

	plat->hwndDC = GetDC( plat->hwnd );
	SetRectRgn( plat->clipRgn, 0, 0, wide, tall );
	SelectObject( plat->hdc, plat->clipRgn );
	SetViewportOrgEx( plat->hdc, 0, 0, NULL );
	BitBlt( plat->hwndDC, 0, 0, wide, tall, plat->hdc, 0, 0, SRCCOPY );
	ReleaseDC( plat->hwnd, plat->hwndDC );
	plat->hwndDC = NULL;
}

void CWin32Surface::Invalidate( VPANEL panel )
{
	VPanel* window = (VPanel*)panel;
	if ( !window )
	{
		return;
	}

	while ( !window->Plat() )
	{
		window = window->GetParent();
		if ( !window )
		{
			return;
		}
	}

	if ( window->Plat() && window->IsVisible() )
	{
		InvalidateRect( window->Plat()->hwnd, NULL, FALSE );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the current cursor
//-----------------------------------------------------------------------------
void CWin32Surface::SetCursor( HCursor cursor )
{
	switch ( cursor )
	{
	case dc_none:
	case dc_arrow:
	case dc_ibeam:
	case dc_hourglass:
	case dc_waitarrow:
	case dc_crosshair:
	case dc_up:
	case dc_sizenwse:
	case dc_sizenesw:
	case dc_sizewe:
	case dc_sizens:
	case dc_sizeall:
	case dc_no:
	case dc_hand:
		_currentCursor = staticDefaultCursor[cursor];
		break;

	case dc_blank:
		return;

	default:
		break;
	}

	::SetCursor( _currentCursor );
}

bool CWin32Surface::IsCursorVisible()
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called every frame to change to window state if necessary
//-----------------------------------------------------------------------------
void CWin32Surface::ApplyChanges()
{
	for ( int i = 0; i < GetPopupCount(); i++ )
	{
		VPANEL panel = GetPopup( i );
		VPanel* popup = (VPanel*)panel;
		if ( !popup->Plat() )
		{
			continue;
		}

		SurfacePlat* plat = popup->Plat();
		// force the main panel to be invisible
		if ( panel == GetEmbeddedPanel() )
		{
			ShowWindow( plat->hwnd, SW_HIDE );
			continue;
		}

		if ( !popup->IsVisible() )
		{
			ShowWindow( plat->hwnd, SW_HIDE );
			continue;
		}

		RECT rect;
		GetWindowRect( plat->hwnd, &rect );
		int windowWide = rect.right - rect.left;
		int windowTall = rect.bottom - rect.top;

		int x, y, wide, tall;
		popup->GetPos( x, y );
		popup->GetSize( wide, tall );

		if ( x != rect.left || y != rect.top || wide != windowWide || tall != windowTall )
		{
			SetWindowPos( plat->hwnd, NULL, x, y, wide, tall, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS );

			if ( rect.left > 0 || rect.top > 0 )
			{
				g_pIVgui->PostMessage( panel, new KeyValues( "Move" ), NULL );
			}
		}

		bool windowVisible = ( GetWindowLongA( plat->hwnd, GWL_STYLE ) & WS_VISIBLE ) != 0;
		if ( !windowVisible )
		{
			if ( popup->IsVisible() )
			{
				ShowWindow( plat->hwnd, SW_SHOWNA );
			}
		}
		else if ( !popup->IsVisible() )
		{
			ShowWindow( plat->hwnd, SW_HIDE );
		}

		if ( ( wide != windowWide || tall != windowTall ) && popup->IsVisible() )
		{
			RecreateContext( panel );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the cursor is over this surface
//			Uses the windows call to do this, instead of doing it procedurally
//-----------------------------------------------------------------------------
bool CWin32Surface::IsWithin( int x, int y )
{
	POINT pt;
	pt.x = x;
	pt.y = y;

	HWND hwnd = WindowFromPoint( pt );
	for ( int i = 0; i < GetPopupCount(); i++ )
	{
		VPanel* popup = (VPanel*)GetPopup( i );
		if ( popup->Plat()->hwnd == hwnd )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if any of the windows have focus
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWin32Surface::HasFocus()
{
	HWND focus = GetFocus();
	if ( !focus )
	{
		return false;
	}

	// see if any of the windows on the surface have the focus
	for ( int i = 0; i < GetPopupCount(); i++ )
	{
		VPanel* popup = (VPanel*)GetPopup( i );
		if ( focus == popup->Plat()->hwnd )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: cap bits
//-----------------------------------------------------------------------------
bool CWin32Surface::SupportsFeature( SurfaceFeature_e feature )
{
	switch ( feature )
	{
	case ANTIALIASED_FONTS:
	case ESCAPE_KEY:
	case OPENING_NEW_HTML_WINDOWS:
	case FRAME_MINIMIZE_MAXIMIZE:
	case DIRECT_HWND_RENDER:
		return true;

	default:
		return false;
	}
}

void CWin32Surface::RestrictPaintToSinglePanel( VPANEL panel )
{
	UNREFERENCED_PARAMETER( panel );
}

void CWin32Surface::SetModalPanel( VPANEL panel )
{
	UNREFERENCED_PARAMETER( panel );
}

VPANEL CWin32Surface::GetModalPanel()
{
	return NULL;
}

void CWin32Surface::UnlockCursor()
{
}

void CWin32Surface::LockCursor()
{
}

void CWin32Surface::SetTranslateExtendedKeys( bool state )
{
	UNREFERENCED_PARAMETER( state );
}

VPANEL CWin32Surface::GetTopmostPopup()
{
	return NULL;
}

void CWin32Surface::SetTopLevelFocus( VPANEL panel )
{
	UNREFERENCED_PARAMETER( panel );
}

//-----------------------------------------------------------------------------
// Purpose: creates a new empty font
//-----------------------------------------------------------------------------
HFont CWin32Surface::CreateFont()
{
	return FontManager().CreateFont();
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CWin32Surface::AddGlyphSetToFont( HFont font, const char* windowsFontName, int tall, int weight, int blur,
	int scanlines, int flags, int rangeMin, int rangeMax )
{
	UNREFERENCED_PARAMETER( rangeMin );
	UNREFERENCED_PARAMETER( rangeMax );

	return FontManager().AddGlyphSetToFont( font, windowsFontName, tall, weight, blur, scanlines, flags );
}

bool CWin32Surface::AddCustomFontFile( const char* fontFileName )
{
	char fullPath[MAX_PATH];
	g_pFileSystem->GetLocalPath( fontFileName, fullPath, sizeof( fullPath ) );
	m_CustomFontFileNames.AddToTail( CUtlSymbol( fontFileName ) );

	return AddFontResourceExA( fullPath, FR_PRIVATE, NULL ) > 0 || AddFontResourceA( fullPath ) > 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CWin32Surface::GetFontTall( HFont font )
{
	return FontManager().GetFontTall( font );
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
//-----------------------------------------------------------------------------
void CWin32Surface::GetCharABCwide( HFont font, int ch, int& a, int& b, int& c )
{
	FontManager().GetCharABCwide( font, ch, a, b, c );
}

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CWin32Surface::GetCharacterWidth( HFont font, int ch )
{
	return FontManager().GetCharacterWidth( font, ch );
}

//-----------------------------------------------------------------------------
// Purpose: returns the area of a text string, including newlines
//-----------------------------------------------------------------------------
void CWin32Surface::GetTextSize( HFont font, const wchar_t* text, int& wide, int& tall )
{
	FontManager().GetTextSize( font, text, wide, tall );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CWin32Surface::GetNotifyPanel()
{
	return _notifyPanel;
}

void CWin32Surface::SetNotifyIcon( VPANEL context, HTexture icon, VPANEL panelToReceiveMessages, const char* text )
{
	VPanel* window = (VPanel*)context;
	if ( !window )
	{
		return;
	}

	while ( !window->Plat() )
	{
		window = window->GetParent();
		if ( !window )
		{
			return;
		}
	}

	if ( !text )
	{
		text = "";
	}

	if ( _notifyIcon == icon && _notifyPanel == panelToReceiveMessages && window->Plat()->notifyIcon )
	{
		NOTIFYICONDATAA data;
		memset( &data, 0, NOTIFYICONDATAA_V2_SIZE );
		data.cbSize = NOTIFYICONDATAA_V2_SIZE;
		data.hWnd = window->Plat()->hwnd;
		data.uID = 1;
		data.uFlags = NIF_TIP;
		data.uCallbackMessage = WM_USER + 1;
		data.hIcon = window->Plat()->notifyIcon;
		strncpy( data.szTip, text, 63 );
		data.szTip[63] = 0;
		Shell_NotifyIconA( NIM_MODIFY, &data );
		return;
	}

	_notifyPanel = panelToReceiveMessages;
	_notifyIcon = icon;

	DWORD message;
	if ( icon )
	{
		message = window->Plat()->notifyIcon ? NIM_MODIFY : NIM_ADD;

		Texture* texture = GetTextureById( icon );
		if ( !texture->_icon )
		{
			ContextPlat();

			char iconFile[256];
			sprintf( iconFile, "%s.ico", texture->_filename );
			texture->_icon = (HICON)LoadImageA( NULL, iconFile, IMAGE_ICON, 16, 16, LR_LOADFROMFILE | LR_DEFAULTSIZE );
			if ( !texture->_icon && texture->_wide <= 32 && texture->_tall <= 32 )
			{
				unsigned char maskBits[128];
				memset( maskBits, 0, sizeof( maskBits ) );
				HBITMAP mask = CreateBitmap( texture->_wide, texture->_tall, 1, 1, maskBits );

				ICONINFO iconInfo;
				iconInfo.fIcon = TRUE;
				iconInfo.xHotspot = 8;
				iconInfo.yHotspot = 8;
				iconInfo.hbmMask = mask;
				iconInfo.hbmColor = texture->_bitmap;
				texture->_icon = CreateIconIndirect( &iconInfo );
				DeleteObject( mask );
			}
		}

		window->Plat()->notifyIcon = texture->_icon;
	}
	else
	{
		message = NIM_DELETE;
		window->Plat()->notifyIcon = NULL;
	}

	NOTIFYICONDATAA data;
	memset( &data, 0, NOTIFYICONDATAA_V2_SIZE );
	data.cbSize = NOTIFYICONDATAA_V2_SIZE;
	data.hWnd = window->Plat()->hwnd;
	data.uID = 1;
	data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	data.uCallbackMessage = WM_USER + 1;
	data.hIcon = window->Plat()->notifyIcon;
	strncpy( data.szTip, text, 63 );
	data.szTip[63] = 0;

	if ( !Shell_NotifyIconA( message, &data ) && icon )
	{
		g_pIVgui->DPrintf2( "error: SetNotifyIcon(%d) failed\n", GetLastError() );
	}
}

void CWin32Surface::PlaySound( const char* fileName )
{
	char localPath[MAX_PATH];
	if ( g_pFileSystem->GetLocalPath( fileName, localPath, sizeof( localPath ) ) )
	{
		g_pFileSystem->GetLocalCopy( localPath );
		::PlaySoundA( localPath, NULL, SND_FILENAME | SND_NOWAIT | SND_NOSTOP | SND_NODEFAULT | SND_ASYNC );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CWin32Surface::GetPopupCount()
{
	return _popupList._count;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPANEL CWin32Surface::GetPopup( int index )
{
	return _popupList.Element( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWin32Surface::ShouldPaintChildPanel( VPANEL childPanel )
{
	// don't Paint children as part of the normal process, handle them in with WM_PAINT messages instead
	return !( (VPanel*)childPanel )->IsPopup();
}

//-----------------------------------------------------------------------------
// Purpose: Applies any changes to the panel into the underline wnidow
//-----------------------------------------------------------------------------
bool CWin32Surface::RecreateContext( VPANEL panel )
{
	VPanel* window = (VPanel*)panel;
	if ( !window || !window->Plat() )
	{
		return true;
	}

	SurfacePlat* plat = window->Plat();

	int wide, tall;
	window->GetSize( wide, tall );

	if ( wide > plat->bitmapSize[0] || tall > plat->bitmapSize[1] || wide < plat->bitmapSize[0] - 200
		|| tall < plat->bitmapSize[1] - 200 )
	{
		if ( plat->bitmap )
		{
			DeleteObject( plat->bitmap );
		}

		plat->hwndDC = GetDC( plat->hwnd );
		plat->bitmap = CreateCompatibleBitmap( plat->hwndDC, wide + 100, tall + 100 );
		plat->bitmapSize[0] = wide + 100;
		plat->bitmapSize[1] = tall + 100;

		if ( plat->textureDC )
		{
			DeleteDC( plat->textureDC );
		}

		SelectObject( plat->hdc, plat->bitmap );
		plat->textureDC = CreateCompatibleDC( plat->hdc );
		ReleaseDC( plat->hwnd, plat->hwndDC );
		plat->hwndDC = NULL;
	}

	return true;
}

void CWin32Surface::AddPanel( VPANEL panel )
{
	UNREFERENCED_PARAMETER( panel );
}

//-----------------------------------------------------------------------------
// Purpose: Called when a panel gets deleted
//-----------------------------------------------------------------------------
void CWin32Surface::ReleasePanel( VPANEL panel )
{
	_popupList.RemoveElement( panel );

	if ( panel == _currentContextPanel )
	{
		_currentContextPanel = _embeddedPanel;
	}

	VPanel* window = (VPanel*)panel;
	if ( !window->Plat() )
	{
		return;
	}

	SurfacePlat* plat = window->Plat();
	RevokeDragDrop( plat->hwnd );

	// remove notify icons
	if ( plat->notifyIcon )
	{
		SetNotifyIcon( panel, 0, NULL, NULL );
	}

	// hide the panel
	SetPanelVisible( panel, false );
	SetWindowLongA( plat->hwnd, GWL_USERDATA, -1 );
	SetWindowPos( plat->hwnd, HWND_BOTTOM, 0, 0, 1, 1, SWP_NOREDRAW | SWP_HIDEWINDOW );

	// free the window context
	if ( plat->bitmap )
	{
		DeleteObject( plat->bitmap );
	}

	if ( plat->textureDC )
	{
		DeleteDC( plat->textureDC );
	}

	if ( plat->hwndDC )
	{
		ReleaseDC( plat->hwnd, plat->hwndDC );
	}

	DeleteDC( plat->hdc );
	DestroyWindow( plat->hwnd );
	DeleteObject( plat->clipRgn );

	delete plat;
	window->SetPlat( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::MovePopupToFront( VPANEL panel )
{
	_popupList.MoveElementToEnd( panel );
}

void CWin32Surface::MovePopupToBack( VPANEL panel )
{
	UNREFERENCED_PARAMETER( panel );
}

//-----------------------------------------------------------------------------
// Purpose: Walks through the panel tree calling Solve() on them all, in order
//-----------------------------------------------------------------------------
void CWin32Surface::SolveTraverse( VPANEL panel, bool forceApplySchemeSettings )
{
	InternalSchemeSettingsTraverse( (VPanel*)panel, forceApplySchemeSettings );

	if ( ( (VPanel*)panel )->IsVisible() )
	{
		InternalThinkTraverse( (VPanel*)panel );
		InternalSolveTraverse( (VPanel*)panel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWin32Surface::PaintTraverse( VPANEL panel )
{
	( (VPanel*)panel )->Client()->PaintTraverse( false, true );
}

void CWin32Surface::EnableMouseCapture( VPANEL panel, bool state )
{
	VPanel* window;
	for ( window = (VPanel*)panel; window; window = window->GetParent() )
	{
		if ( window->Plat() )
		{
			break;
		}
	}

	if ( !state )
	{
		ReleaseCapture();
		return;
	}

	SetCapture( window->Plat()->hwnd );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the bounds of the usable workspace area
//-----------------------------------------------------------------------------
void CWin32Surface::GetWorkspaceBounds( int& x, int& y, int& wide, int& tall )
{
	RECT workArea;
	SystemParametersInfoA( SPI_GETWORKAREA, 0, &workArea, 0 );

	x = workArea.left;
	y = workArea.top;
	wide = workArea.right - x;
	tall = workArea.bottom - y;
}

//-----------------------------------------------------------------------------
// Purpose: gets the absolute coordinates of the screen (in screen space)
//-----------------------------------------------------------------------------
void CWin32Surface::GetAbsoluteWindowBounds( int& x, int& y, int& wide, int& tall )
{
	// always work in full window screen space
	x = 0;
	y = 0;
	GetScreenSize( wide, tall );
}

void CWin32Surface::GetProportionalBase( int& wide, int& tall )
{
	wide = 640;
	tall = 480;
}

void CWin32Surface::CalculateMouseVisible()
{
	_needKB = false;
	_needMouse = false;

	for ( int i = 0; i < g_pSurface->GetPopupCount(); i++ )
	{
		VPanel* popup = (VPanel*)g_pSurface->GetPopup( i );

		bool isVisible = popup->IsVisible();
		VPanel* parent = popup->GetParent();
		while ( parent && isVisible )
		{
			if ( !parent->IsVisible() )
			{
				isVisible = false;
				break;
			}

			parent = parent->GetParent();
		}

		if ( isVisible )
		{
			_needMouse |= popup->IsMouseInputEnabled();
			_needKB |= popup->IsKeyBoardInputEnabled();
		}
	}

	if ( _needMouse )
	{
		SetCursor( dc_arrow );
		UnlockCursor();
	}
}

bool CWin32Surface::NeedKBInput()
{
	return _needKB;
}

bool CWin32Surface::HasCursorPosFunctions()
{
	return false;
}

void CWin32Surface::SurfaceGetCursorPos( int& x, int& y )
{
	UNREFERENCED_PARAMETER( x );
	UNREFERENCED_PARAMETER( y );
}

void CWin32Surface::SurfaceSetCursorPos( int x, int y )
{
	UNREFERENCED_PARAMETER( x );
	UNREFERENCED_PARAMETER( y );
}

void CWin32Surface::DrawTexturedPolygon( VGuiVertex* pVertices, int n )
{
	HDC hdc = ContextPlat()->hdc;

	POINT* points = (POINT*)malloc( sizeof( POINT ) * n );
	if ( !points )
	{
		return;
	}

	for ( int i = 0; i < n; i++ )
	{
		points[i].x = pVertices[i].x;
		points[i].y = pVertices[i].y;
	}

	COLORREF textColor = GetTextColor( hdc );
	COLORREF backgroundColor = GetBkColor( hdc );
	DrawSetTextColor( GetRValue( backgroundColor ), GetGValue( backgroundColor ), GetBValue( backgroundColor ), 255 );

	HBRUSH brush = CreateSolidBrush( backgroundColor );
	HGDIOBJ oldBrush = SelectObject( hdc, brush );
	Polygon( hdc, points, n );
	SelectObject( hdc, oldBrush );
	DeleteObject( brush );
	free( points );

	DrawSetTextColor( GetRValue( textColor ), GetGValue( textColor ), GetBValue( textColor ), 255 );
}

int CWin32Surface::GetFontAscent( HFont font, wchar_t wch )
{
	return FontManager().GetFontAscent( font, wch );
}

void CWin32Surface::SetAllowHTMLJavaScript( bool state )
{
	_allowJavaScript = state;
}

void CWin32Surface::SetLanguage( const char* language )
{
	strncpy( _language, language, sizeof( _language ) );
	_language[sizeof( _language ) - 1] = 0;
}

const char* CWin32Surface::GetLanguage()
{
	return _language;
}

bool CWin32Surface::DeleteTextureByID( int id )
{
	UNREFERENCED_PARAMETER( id );

	return false;
}

void CWin32Surface::DrawUpdateRegionTextureBGRA( int nTextureID, int x, int y, const unsigned char* pchData, int wide,
	int tall )
{
	UNREFERENCED_PARAMETER( nTextureID );
	UNREFERENCED_PARAMETER( x );
	UNREFERENCED_PARAMETER( y );
	UNREFERENCED_PARAMETER( pchData );
	UNREFERENCED_PARAMETER( wide );
	UNREFERENCED_PARAMETER( tall );
}

void CWin32Surface::DrawSetTextureBGRA( int id, const unsigned char* rgba, int wide, int tall )
{
	UNREFERENCED_PARAMETER( id );
	UNREFERENCED_PARAMETER( rgba );
	UNREFERENCED_PARAMETER( wide );
	UNREFERENCED_PARAMETER( tall );
}

void CWin32Surface::CreateBrowser( VPANEL panel, IHTMLResponses* pBrowser, bool bPopupWindow,
	const char* pchUserAgentIdentifier )
{
	UNREFERENCED_PARAMETER( panel );
	UNREFERENCED_PARAMETER( pBrowser );
	UNREFERENCED_PARAMETER( bPopupWindow );
	UNREFERENCED_PARAMETER( pchUserAgentIdentifier );
}

void CWin32Surface::RemoveBrowser( VPANEL panel, IHTMLResponses* pBrowser )
{
	UNREFERENCED_PARAMETER( panel );
	UNREFERENCED_PARAMETER( pBrowser );
}

IHTMLChromeController* CWin32Surface::AccessChromeHTMLController()
{
	return NULL;
}

void CWin32Surface::DrawTexturedRectAdd( int x0, int y0, int x1, int y1 )
{
	DrawTexturedRect( x0, y0, x1, y1 );
}

void CWin32Surface::SetSupportsEsc( bool bSupportsEsc )
{
	UNREFERENCED_PARAMETER( bSupportsEsc );
}

int CWin32Surface::GetFontBlur( HFont font )
{
	return FontManager().GetFontBlur( font );
}

bool CWin32Surface::IsAdditive( HFont font )
{
	return FontManager().IsAdditive( font );
}

void CWin32Surface::SetProportionalBase( int width, int height )
{
	UNREFERENCED_PARAMETER( width );
	UNREFERENCED_PARAMETER( height );
}

void CWin32Surface::GetHDProportionalBase( int& wide, int& tall )
{
	wide = 1280;
	tall = 720;
}

void CWin32Surface::SetHDProportionalBase( int width, int height )
{
	UNREFERENCED_PARAMETER( width );
	UNREFERENCED_PARAMETER( height );
}

void CWin32Surface::SetContextPanel( VPANEL panel )
{
	_currentContextPanel = panel;
}

void CWin32Surface::ActivatePanel( VPANEL panel )
{
	VPanel* window = (VPanel*)panel;
	if ( !window )
	{
		return;
	}

	while ( !window->Plat() )
	{
		window = window->GetParent();
		if ( !window )
		{
			return;
		}
	}

	if ( window->Plat() && window->HasParent( window ) )
	{
		VPANEL modal = g_pInput->GetAppModalSurface();
		if ( !modal || modal == (VPANEL)window )
		{
			window->MoveToFront();
			return;
		}

		SetForegroundWindow( g_pInput->GetAppModalSurface() );
	}

	window->Client()->RequestFocus( 0 );
}
