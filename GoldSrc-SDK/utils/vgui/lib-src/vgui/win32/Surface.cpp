#include "VGUI_Surface.h"

#include "VGUI_App.h"
#include "VGUI_Bitmap.h"
#include "VGUI_Cursor.h"
#include "VGUI_Font.h"
#include "VGUI_ImagePanel.h"
#include "VGUI_Panel.h"
#include "vgui_win32.h"

#include <windowsx.h>
#include <stdio.h>

#define VIRTUAL_KEY_COUNT	256
#define BITMAP_SPARE_PIXELS	100

using namespace vgui;

namespace
{

const char surfaceClassName[] = "Surface";

struct Texture
{
	int		id;
	HBITMAP	bitmap;
	int		wide;
	int		tall;
	uchar*	pixels;
};

Texture textures[VGUI_TEXTURE_CAPACITY];
int textureCount;
Texture* currentTexture;
HCURSOR defaultCursors[Cursor::dc_last];
HCURSOR currentCursor;
KeyCode keyCodes[VIRTUAL_KEY_COUNT];

Texture* FindTexture( int id )
{
	if ( currentTexture != NULL && currentTexture->id == id )
	{
		return currentTexture;
	}
	for ( int i = 0; i < textureCount; ++i )
	{
		if ( textures[i].id == id )
		{
			return &textures[i];
		}
	}
	return NULL;
}

void InitializeInputTables()
{
	defaultCursors[Cursor::dc_none] = NULL;
	const LPCSTR cursorResources[] = { IDC_ARROW, IDC_IBEAM, IDC_WAIT, IDC_CROSS,
		IDC_UPARROW, IDC_SIZENWSE, IDC_SIZENESW, IDC_SIZEWE, IDC_SIZENS,
		IDC_SIZEALL, IDC_NO, IDC_HAND };
	for ( int i = 0; i < Cursor::dc_last - Cursor::dc_arrow; ++i )
	{
		defaultCursors[Cursor::dc_arrow + i] = LoadCursorA( NULL, cursorResources[i] );
	}
	for ( int i = 0; i < 10; ++i )
	{
		keyCodes['0' + i] = (KeyCode)( KEY_0 + i );
		keyCodes[VK_NUMPAD0 + i] = (KeyCode)( KEY_PAD_0 + i );
	}
	for ( int i = 0; i < 26; ++i )
	{
		keyCodes['A' + i] = (KeyCode)( KEY_A + i );
	}
	for ( int i = 0; i < 12; ++i )
	{
		keyCodes[VK_F1 + i] = (KeyCode)( KEY_F1 + i );
	}

	struct KeyMapping
	{
		int		native;
		KeyCode	code;
	};
	const KeyMapping mappings[] =
	{
		{ VK_DIVIDE, KEY_PAD_DIVIDE }, { VK_MULTIPLY, KEY_PAD_MULTIPLY },
		{ VK_SUBTRACT, KEY_PAD_MINUS }, { VK_ADD, KEY_PAD_PLUS }, { VK_DECIMAL, KEY_PAD_DECIMAL },
		{ VK_OEM_4, KEY_LBRACKET }, { VK_OEM_6, KEY_RBRACKET }, { VK_OEM_1, KEY_SEMICOLON },
		{ VK_OEM_7, KEY_APOSTROPHE }, { VK_OEM_3, KEY_BACKQUOTE }, { VK_OEM_COMMA, KEY_COMMA },
		{ VK_OEM_PERIOD, KEY_PERIOD }, { VK_OEM_2, KEY_SLASH }, { VK_OEM_5, KEY_BACKSLASH },
		{ VK_OEM_MINUS, KEY_MINUS }, { VK_OEM_PLUS, KEY_EQUAL }, { VK_RETURN, KEY_ENTER },
		{ VK_SPACE, KEY_SPACE }, { VK_BACK, KEY_BACKSPACE }, { VK_TAB, KEY_TAB },
		{ VK_CAPITAL, KEY_CAPSLOCK }, { VK_NUMLOCK, KEY_NUMLOCK }, { VK_ESCAPE, KEY_ESCAPE },
		{ VK_SCROLL, KEY_SCROLLLOCK }, { VK_INSERT, KEY_INSERT }, { VK_DELETE, KEY_DELETE },
		{ VK_HOME, KEY_HOME }, { VK_END, KEY_END }, { VK_PRIOR, KEY_PAGEUP },
		{ VK_NEXT, KEY_PAGEDOWN }, { VK_PAUSE, KEY_BREAK }, { VK_SHIFT, KEY_RSHIFT },
		{ VK_MENU, KEY_RALT }, { VK_CONTROL, KEY_RCONTROL }, { VK_LWIN, KEY_LWIN },
		{ VK_RWIN, KEY_RWIN }, { VK_APPS, KEY_APP }, { VK_UP, KEY_UP }, { VK_LEFT, KEY_LEFT },
		{ VK_DOWN, KEY_DOWN }, { VK_RIGHT, KEY_RIGHT }
	};
	for ( const KeyMapping& mapping : mappings )
	{
		keyCodes[mapping.native] = mapping.code;
	}
}

LRESULT CALLBACK SurfaceWindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
{
	Surface* surface = (Surface*)GetWindowLongPtrA( window, GWLP_USERDATA );
	switch ( message )
	{
	case WM_SETFOCUS:
		surface->getPanel()->requestFocus();
		break;
	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		BeginPaint( window, &paint );
		surface->getPanel()->solveTraverse();
		surface->getPanel()->paintTraverse();
		EndPaint( window, &paint );
		ValidateRect( window, NULL );
		break;
	}
	case WM_ERASEBKGND:
		surface->getPanel()->repaint();
		break;
	case WM_SETCURSOR:
		::SetCursor( currentCursor );
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if ( ( HIWORD( lParam ) & KF_REPEAT ) == 0 )
		{
			surface->getApp()->internalKeyPressed( keyCodes[wParam], surface );
		}
		surface->getApp()->internalKeyTyped( keyCodes[wParam], surface );
		break;
	case WM_KEYUP:
	case WM_SYSKEYUP:
		surface->getApp()->internalKeyReleased( keyCodes[wParam], surface );
		break;
	case WM_MOUSEMOVE:
		surface->getApp()->internalCursorMoved( GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ), surface );
		break;
	case WM_LBUTTONDOWN:
		surface->getApp()->internalMousePressed( MOUSE_LEFT, surface );
		break;
	case WM_RBUTTONDOWN:
		surface->getApp()->internalMousePressed( MOUSE_RIGHT, surface );
		break;
	case WM_MBUTTONDOWN:
		surface->getApp()->internalMousePressed( MOUSE_MIDDLE, surface );
		break;
	case WM_LBUTTONUP:
		surface->getApp()->internalMouseReleased( MOUSE_LEFT, surface );
		break;
	case WM_RBUTTONUP:
		surface->getApp()->internalMouseReleased( MOUSE_RIGHT, surface );
		break;
	case WM_MBUTTONUP:
		surface->getApp()->internalMouseReleased( MOUSE_MIDDLE, surface );
		break;
	case WM_LBUTTONDBLCLK:
		surface->getApp()->internalMouseDoublePressed( MOUSE_LEFT, surface );
		break;
	case WM_RBUTTONDBLCLK:
		surface->getApp()->internalMouseDoublePressed( MOUSE_RIGHT, surface );
		break;
	case WM_MBUTTONDBLCLK:
		surface->getApp()->internalMouseDoublePressed( MOUSE_MIDDLE, surface );
		break;
	case WM_MOUSEWHEEL:
		surface->getApp()->internalMouseWheeled( GET_WHEEL_DELTA_WPARAM( wParam ) / WHEEL_DELTA, surface );
		break;
	default:
		return DefWindowProcA( window, message, wParam, lParam );
	}
	return 1;
}

}

Surface::~Surface()
{
}

bool Surface::createPlat()
{
	InitializeInputTables();
	WNDCLASSA windowClass = {};
	windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	windowClass.lpfnWndProc = SurfaceWindowProc;
	windowClass.hInstance = GetModuleHandleA( NULL );
	windowClass.lpszClassName = surfaceClassName;
	RegisterClassA( &windowClass );
	int x, y, wide, tall;
	getPanel()->getBounds( x, y, wide, tall );
	_plat = new SurfacePlat;
	_plat->window = CreateWindowExA( 0, surfaceClassName, "", WS_POPUP, x, y, wide, tall,
		NULL, NULL, GetModuleHandleA( NULL ), NULL );
	const int initialClipSize = 64;
	_plat->clip = CreateRectRgn( 0, 0, initialClipSize, initialClipSize );
	_plat->drawDC = CreateCompatibleDC( NULL );
	_plat->windowDC = GetDC( _plat->window );
	_plat->textureDC = NULL;
	_plat->bitmap = NULL;
	_plat->bitmapSize[0] = 0;
	_plat->bitmapSize[1] = 0;
	_plat->isFullscreen = false;
	SetBkMode( _plat->drawDC, TRANSPARENT );
	SetWindowLongPtrA( _plat->window, GWLP_USERDATA, (LONG_PTR)this );
	SetTextAlign( _plat->drawDC, TA_UPDATECP );
	return true;
}

bool Surface::recreateContext()
{
	int wide, tall;
	getPanel()->getSize( wide, tall );
	if ( wide > _plat->bitmapSize[0] || tall > _plat->bitmapSize[1]
		|| wide < _plat->bitmapSize[0] - 2 * BITMAP_SPARE_PIXELS
		|| tall < _plat->bitmapSize[1] - 2 * BITMAP_SPARE_PIXELS )
	{
		const int bitmapWide = wide + BITMAP_SPARE_PIXELS;
		const int bitmapTall = tall + BITMAP_SPARE_PIXELS;
		HBITMAP bitmap = CreateCompatibleBitmap( _plat->windowDC, bitmapWide, bitmapTall );
		if ( bitmap == NULL )
		{
			// Keep the old backing bitmap and size; the next resize retries.
			return false;
		}
		const HGDIOBJ previous = SelectObject( _plat->drawDC, bitmap );
		if ( previous == NULL || previous == HGDI_ERROR )
		{
			DeleteObject( bitmap );
			return false;
		}
		// Selecting the new bitmap deselected the old one, so it can be deleted now.
		if ( _plat->bitmap != NULL )
		{
			DeleteObject( _plat->bitmap );
		}
		_plat->bitmap = bitmap;
		_plat->bitmapSize[0] = bitmapWide;
		_plat->bitmapSize[1] = bitmapTall;
	}
	if ( _plat->textureDC == NULL )
	{
		_plat->textureDC = CreateCompatibleDC( _plat->drawDC );
	}
	return _plat->textureDC != NULL;
}

void Surface::applyChanges()
{
	RECT bounds;
	GetWindowRect( _plat->window, &bounds );
	const int oldWide = bounds.right - bounds.left;
	const int oldTall = bounds.bottom - bounds.top;
	int x, y, wide, tall;
	getPanel()->getBounds( x, y, wide, tall );
	if ( x != bounds.left || y != bounds.top || wide != oldWide || tall != oldWide )
	{
		SetWindowPos( _plat->window, NULL, x, y, wide, tall, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS );
	}
	const bool visible = ( GetWindowLongA( _plat->window, GWL_STYLE ) & WS_VISIBLE ) != 0;
	if ( visible && !getPanel()->isVisible() )
	{
		ShowWindow( _plat->window, SW_HIDE );
	}
	else if ( !visible && getPanel()->isVisible() )
	{
		ShowWindow( _plat->window, SW_SHOW );
	}
	if ( ( wide != oldWide || tall != oldTall ) && getPanel()->isVisible() )
	{
		recreateContext();
	}
}

void Surface::swapBuffers()
{
	int wide, tall;
	getPanel()->getSize( wide, tall );
	SetRectRgn( _plat->clip, 0, 0, wide, tall );
	SelectObject( _plat->drawDC, _plat->clip );
	SetViewportOrgEx( _plat->drawDC, 0, 0, NULL );
	BitBlt( _plat->windowDC, 0, 0, wide, tall, _plat->drawDC, 0, 0, SRCCOPY );
}

void Surface::drawSetColor( int r, int g, int b, int )
{
	SetBkColor( _plat->drawDC, RGB( r, g, b ) );
}

void Surface::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	RECT rectangle = { x0, y0, x1, y1 };
	ExtTextOutA( _plat->drawDC, 0, 0, ETO_OPAQUE, &rectangle, NULL, 0, NULL );
}

void Surface::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	drawFilledRect( x0, y0, x1, y0 + 1 );
	drawFilledRect( x0, y1 - 1, x1, y1 );
	drawFilledRect( x0, y0 + 1, x0 + 1, y1 - 1 );
	drawFilledRect( x1 - 1, y0 + 1, x1, y1 - 1 );
}

void Surface::drawSetTextColor( int r, int g, int b, int )
{
	SetTextColor( _plat->drawDC, RGB( r, g, b ) );
}

void Surface::drawSetTextPos( int x, int y )
{
	MoveToEx( _plat->drawDC, x, y, NULL );
}

void Surface::drawPrintText( const char* text, int textLen )
{
	if ( text != NULL && textLen > 0 )
	{
		ExtTextOutA( _plat->drawDC, 0, 0, 0, NULL, text, textLen, NULL );
	}
}

void Surface::drawSetTextFont( Font* font )
{
	if ( font != NULL )
	{
		font->getPlat()->drawSetTextFont( _plat );
	}
}

int Surface::createNewTextureID()
{
	const int firstTextureId = 2700;
	static int nextId = firstTextureId;
	return nextId++;
}

void Surface::drawSetTexture( int id )
{
	currentTexture = FindTexture( id );
}

void Surface::drawSetTextureRGBA( int id, const char* rgba, int wide, int tall )
{
	Texture* texture = FindTexture( id );
	if ( texture == NULL )
	{
		if ( textureCount == VGUI_TEXTURE_CAPACITY )
		{
			return;
		}
		texture = &textures[textureCount++];
		texture->id = id;
	}
	BITMAPINFO info = {};
	info.bmiHeader.biSize = sizeof( info.bmiHeader );
	info.bmiHeader.biWidth = wide;
	info.bmiHeader.biHeight = -tall;
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = VGUI_RGBA_BYTES * 8;
	info.bmiHeader.biCompression = BI_RGB;
	void* pixels = NULL;
	HBITMAP bitmap = CreateDIBSection( _plat->drawDC, &info, DIB_RGB_COLORS, &pixels, NULL, 0 );
	if ( bitmap == NULL || pixels == NULL )
	{
		// Keep the old texture when the new bitmap cannot be created.
		if ( bitmap != NULL )
		{
			DeleteObject( bitmap );
		}
		return;
	}
	// drawTexturedRect deselects texture bitmaps after each blit, so the old one can be deleted.
	if ( texture->bitmap != NULL )
	{
		DeleteObject( texture->bitmap );
	}
	texture->bitmap = bitmap;
	texture->wide = wide;
	texture->tall = tall;
	texture->pixels = (uchar*)pixels;
	for ( int y = 0; y < tall; ++y )
	{
		for ( int x = 0; x < wide; ++x )
		{
			const int offset = VGUI_RGBA_BYTES * ( x + y * wide );
			texture->pixels[offset] = (uchar)rgba[offset + 2];
			texture->pixels[offset + 1] = (uchar)rgba[offset + 1];
			texture->pixels[offset + 2] = (uchar)rgba[offset];
			texture->pixels[offset + 3] = (uchar)rgba[offset + 3];
		}
	}
}

void Surface::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	if ( currentTexture == NULL || _plat->textureDC == NULL )
	{
		return;
	}
	// Textures are shared by every Surface and a bitmap can be selected into one DC at a time,
	// so restore the previous selection after the blit.
	const HGDIOBJ previous = SelectObject( _plat->textureDC, currentTexture->bitmap );
	if ( previous == NULL || previous == HGDI_ERROR )
	{
		return;
	}
	StretchBlt( _plat->drawDC, x0, y0, x1 - x0, y1 - y0, _plat->textureDC,
		0, 0, currentTexture->wide, currentTexture->tall, SRCCOPY );
	SelectObject( _plat->textureDC, previous );
}

void Surface::pushMakeCurrent( Panel* panel, bool useInsets )
{
	int left, top, right, bottom;
	panel->getInset( left, top, right, bottom );
	if ( !useInsets )
	{
		left = top = right = bottom = 0;
	}
	int rootX, rootY, rootRight, rootBottom;
	getPanel()->getAbsExtents( rootX, rootY, rootRight, rootBottom );
	int panelX, panelY, panelRight, panelBottom;
	panel->getAbsExtents( panelX, panelY, panelRight, panelBottom );
	int clipLeft, clipTop, clipRight, clipBottom;
	panel->getClipRect( clipLeft, clipTop, clipRight, clipBottom );
	if ( getPanel() == panel )
	{
		SetViewportOrgEx( _plat->drawDC, left, top, NULL );
	}
	else
	{
		SetViewportOrgEx( _plat->drawDC, left + panelX - rootX, top + panelY - rootY, NULL );
	}
	SetRectRgn( _plat->clip, clipLeft - rootX, clipTop - rootY, clipRight - rootX, clipBottom - rootY );
	SelectObject( _plat->drawDC, _plat->clip );
}

void Surface::popMakeCurrent( Panel* )
{
}

void Surface::enableMouseCapture( bool state )
{
	if ( state )
	{
		SetCapture( _plat->window );
	}
	else
	{
		ReleaseCapture();
	}
}

void Surface::setCursor( Cursor* cursor )
{
	_currentCursor = cursor;
	if ( cursor != NULL )
	{
		_emulatedCursor->setImage( cursor->getBitmap() );
		const Cursor::DefaultCursor type = cursor->getDefaultCursor();
		if ( type > Cursor::dc_user && type < Cursor::dc_last )
		{
			currentCursor = defaultCursors[cursor->getDefaultCursor()];
		}
	}
	else
	{
		currentCursor = NULL;
		_emulatedCursor->setImage( NULL );
	}
	::SetCursor( currentCursor );
}

void Surface::invalidate( Panel* )
{
	InvalidateRect( _plat->window, NULL, FALSE );
}

void Surface::setTitle( const char* title )
{
	SetWindowTextA( _plat->window, title );
}

bool Surface::hasFocus()
{
	return GetFocus() == _plat->window;
}

bool Surface::isWithin( int x, int y )
{
	POINT position = { x, y };
	return WindowFromPoint( position ) == _plat->window;
}

int Surface::getModeInfoCount()
{
	resetModeInfo();
	addModeInfo( 640, 480, 16 );
	addModeInfo( 800, 600, 16 );
	return SurfaceBase::getModeInfoCount();
}

bool Surface::setFullscreenMode( int wide, int tall, int bpp )
{
	if ( _plat->isFullscreen && _plat->fullscreenInfo[0] == wide
		&& _plat->fullscreenInfo[1] == tall && _plat->fullscreenInfo[2] == bpp )
	{
		return true;
	}
	if ( SurfaceBase::getModeInfoCount() == 0 )
	{
		getModeInfoCount();
	}
	for ( int i = 0; i < SurfaceBase::getModeInfoCount(); ++i )
	{
		int modeWide, modeTall, modeBpp;
		if ( !getModeInfo( i, modeWide, modeTall, modeBpp )
			|| wide != modeWide || tall != modeTall || bpp != modeBpp )
		{
			continue;
		}
		DEVMODEA mode = {};
		mode.dmSize = (WORD)offsetof( DEVMODEA, dmPanningWidth );
		mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
		mode.dmBitsPerPel = bpp;
		mode.dmPelsWidth = wide;
		mode.dmPelsHeight = tall;
		if ( ChangeDisplaySettingsA( &mode, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
		{
			continue;
		}
		if ( !_plat->isFullscreen )
		{
			getPanel()->getBounds( _plat->restoreInfo[0], _plat->restoreInfo[1],
				_plat->restoreInfo[2], _plat->restoreInfo[3] );
			SetWindowPos( _plat->window, HWND_TOPMOST, -1, -1, -1, -1, SWP_NOMOVE | SWP_NOSIZE );
		}
		getPanel()->setBounds( 0, 0, wide, tall );
		applyChanges();
		_plat->isFullscreen = true;
		_plat->fullscreenInfo[0] = wide;
		_plat->fullscreenInfo[1] = tall;
		_plat->fullscreenInfo[2] = bpp;
		getPanel()->repaint();
		return true;
	}
	return false;
}

void Surface::setWindowedMode()
{
	if ( _plat->isFullscreen )
	{
		ChangeDisplaySettingsA( NULL, 0 );
		getPanel()->setBounds( _plat->restoreInfo[0], _plat->restoreInfo[1],
			_plat->restoreInfo[2], _plat->restoreInfo[3] );
		applyChanges();
		_plat->isFullscreen = false;
		getPanel()->repaint();
	}
}

void Surface::setAsTopMost( bool state )
{
	SetWindowPos( _plat->window, state ? HWND_TOPMOST : HWND_NOTOPMOST,
		0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );
}
