#include "VGUI_Panel.h"

#include "VGUI_App.h"
#include "VGUI_Border.h"
#include "VGUI_BuildGroup.h"
#include "VGUI_Cursor.h"
#include "VGUI_FocusChangeSignal.h"
#include "VGUI_FocusNavGroup.h"
#include "VGUI_InputSignal.h"
#include "VGUI_Label.h"
#include "VGUI_Layout.h"
#include "VGUI_RepaintSignal.h"
#include "VGUI_SurfaceBase.h"
#include "VGUI_TreeFolder.h"

#include <stdio.h>
#include <string.h>

using namespace vgui;

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
Panel::Panel()
	: _childDar( 4 ),
	  _inputSignalDar( 4 ),
	  _repaintSignalDar( 4 ),
	  _focusChangeSignalDar( 4 )
{
	init( 0, 0, 64, 64 );
}

#if defined( _MSC_VER )
#pragma warning( suppress : 26495 )
#endif
Panel::Panel( int x, int y, int wide, int tall )
	: _childDar( 4 ),
	  _inputSignalDar( 4 ),
	  _repaintSignalDar( 4 ),
	  _focusChangeSignalDar( 4 )
{
	init( x, y, wide, tall );
}

#if VGUI_ABI_COMPAT_MODE != 1
Panel::Panel( const Panel& other )
	: _childDar( 4 ),
	  _inputSignalDar( 4 ),
	  _repaintSignalDar( 4 ),
	  _focusChangeSignalDar( 4 )
{
	init( other._pos[0], other._pos[1], other._size[0], other._size[1] );
	*this = other;
}

Panel::~Panel()
{
	App* app = App::getInstance();

	// A surface whose root is deleted loses its root and its emulated cursor,
	// a child deleted with this tree, and App stops ticking it. The surfaces
	// are found through App, as the root may have been moved off its surface.
	if ( app != NULL )
	{
		for ( int i = app->_surfaceBaseDar.getCount() - 1; i >= 0; --i )
		{
			SurfaceBase* surfaceBase = app->_surfaceBaseDar[i];
			if ( surfaceBase->_embeddedPanel == this )
			{
				surfaceBase->_embeddedPanel = NULL;
				surfaceBase->_emulatedCursor = NULL;
				app->_surfaceBaseDar.removeElementAt( i );
			}
		}
	}

	if ( _parent != NULL )
		_parent->removeChild( this );

	if ( app != NULL )
		app->panelDeleted( this );

	if ( _focusNavGroup != NULL )
		_focusNavGroup->removePanel( this );

	if ( _buildGroup != NULL )
		_buildGroup->panelRemoved( this );

	delete _border;
	_border = NULL;

	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		delete _inputSignalDar[i];
	_inputSignalDar.removeAll();

	// Detach every child first so that none of them removes itself from this
	// half-destroyed panel.
	Dar<Panel*> children( _childDar );
	_childDar.removeAll();
	for ( int i = 0; i < children.getCount(); ++i )
	{
		children[i]->_parent = NULL;
		children[i]->_surfaceBase = NULL;
	}
	for ( int i = 0; i < children.getCount(); ++i )
		delete children[i];
}

// Copies appearance and state only: the parent, children, border, signals and
// BuildGroup belong to one panel, so a copy is in no BuildGroup.
Panel& Panel::operator=( const Panel& other )
{
	if ( this == &other )
		return *this;

	memcpy( _pos, other._pos, sizeof( _pos ) );
	memcpy( _size, other._size, sizeof( _size ) );
	memcpy( _loc, other._loc, sizeof( _loc ) );
	memcpy( _minimumSize, other._minimumSize, sizeof( _minimumSize ) );
	memcpy( _preferredSize, other._preferredSize, sizeof( _preferredSize ) );
	memcpy( _clipRect, other._clipRect, sizeof( _clipRect ) );

	_cursor = other._cursor;
	_schemeCursor = other._schemeCursor;
	_visible = other._visible;
	_layout = other._layout;
	_needsLayout = other._needsLayout;
	setFocusNavGroup( other._focusNavGroup );
	_autoFocusNavEnabled = other._autoFocusNavEnabled;
	_needsRepaint = other._needsRepaint;
	_enabled = other._enabled;
	_fgColor = other._fgColor;
	_bgColor = other._bgColor;
	_layoutInfo = other._layoutInfo;
	_paintBorderEnabled = other._paintBorderEnabled;
	_paintBackgroundEnabled = other._paintBackgroundEnabled;
	_paintEnabled = other._paintEnabled;

	return *this;
}

void* Panel::operator new( size_t size )
{
	return ::operator new( size );
}

void Panel::operator delete( void* memory )
{
	::operator delete( memory );
}
#endif

void Panel::init( int x, int y, int wide, int tall )
{
	_pos[0] = x;
	_pos[1] = y;
	_size[0] = wide;
	_size[1] = tall;
	_loc[0] = 0;
	_loc[1] = 0;
	_minimumSize[0] = 0;
	_minimumSize[1] = 0;
	_needsRepaint = false;
	_parent = NULL;
	_surfaceBase = NULL;
	_visible = true;
	_cursor = NULL;
	_schemeCursor = Scheme::scu_arrow;
	_layout = NULL;
	_needsLayout = true;
	_focusNavGroup = NULL;
	_border = NULL;
	_enabled = true;
	_buildGroup = NULL;
	_layoutInfo = NULL;
	_paintBorderEnabled = true;
	_paintBackgroundEnabled = true;
	_paintEnabled = true;
	setFgColor( Scheme::sc_black );
	setBgColor( Scheme::sc_secondary3 );
	setAutoFocusNavEnabled( true );
}

void Panel::setPos( int x, int y )
{
	_pos[0] = x;
	_pos[1] = y;
}

void Panel::getPos( int& x, int& y )
{
	x = _pos[0];
	y = _pos[1];
}

void Panel::setSize( int wide, int tall )
{
	int wideClamped = _minimumSize[0];
	int tallClamped = _minimumSize[1];
	if ( wideClamped < wide )
		wideClamped = wide;
	if ( tallClamped < tall )
		tallClamped = tall;
	_size[0] = wideClamped;
	_size[1] = tallClamped;
}

void Panel::getSize( int& wide, int& tall )
{
	wide = _size[0];
	tall = _size[1];
}

void Panel::setBounds( int x, int y, int wide, int tall )
{
	setPos( x, y );
	setSize( wide, tall );
}

void Panel::getBounds( int& x, int& y, int& wide, int& tall )
{
	x = _pos[0];
	y = _pos[1];
	wide = _size[0];
	tall = _size[1];
}

int Panel::getWide()
{
	return _size[0];
}

int Panel::getTall()
{
	return _size[1];
}

Panel* Panel::getParent()
{
	return _parent;
}

void Panel::setVisible( bool state )
{
	_visible = state;
}

bool Panel::isVisible()
{
	return _visible;
}

bool Panel::isVisibleUp()
{
	Panel* panel = this;
	while ( panel != NULL )
	{
		if ( !panel->_visible )
			return false;
		panel = panel->getParent();
	}
	return true;
}

void Panel::repaint()
{
	_needsRepaint = true;
	if ( _surfaceBase != NULL )
	{
		_surfaceBase->_needsSwap = true;
		_surfaceBase->invalidate( this );
	}

#if defined( _WIN32 )
	for ( int i = 0; i < _repaintSignalDar.getCount(); ++i )
		_repaintSignalDar[i]->panelRepainted( this );
#endif
}

// A panel on no surface repaints the top of its own tree, which is painted in
// full when the tree is next on a surface.
void Panel::repaintAll()
{
	if ( _surfaceBase != NULL )
	{
		_surfaceBase->getPanel()->repaint();
		return;
	}

	Panel* root = this;
	while ( root->_parent != NULL )
		root = root->_parent;
	root->repaint();
}

void Panel::getAbsExtents( int& x0, int& y0, int& x1, int& y1 )
{
	x0 = _loc[0];
	y0 = _loc[1];
	x1 = x0 + _size[0];
	y1 = y0 + _size[1];
}

void Panel::getClipRect( int& x0, int& y0, int& x1, int& y1 )
{
	x0 = _clipRect[0];
	y0 = _clipRect[1];
	x1 = _clipRect[2];
	y1 = _clipRect[3];
}

void Panel::setParent( Panel* parent )
{
	if ( _parent != NULL )
		_parent->removeChild( this );
	if ( parent != NULL )
	{
		_parent = parent;
		parent->_childDar.putElement( this );
		setSurfaceBaseTraverse( _parent->_surfaceBase );
	}
}

void Panel::addChild( Panel* child )
{
	if ( child->_parent != NULL )
		child->_parent->removeChild( child );
	child->_parent = this;
	_childDar.putElement( child );
	child->setSurfaceBaseTraverse( _surfaceBase );
}

void Panel::insertChildAt( Panel* child, int index )
{
	if ( _childDar.hasElement( child ) )
		return;
	if ( child->_parent != NULL )
		child->_parent->removeChild( child );
	child->_parent = this;
	_childDar.insertElementAt( child, index );
	child->setSurfaceBaseTraverse( _surfaceBase );
}

// The child's whole tree leaves this panel's surface.
void Panel::removeChild( Panel* child )
{
	child->_parent = NULL;
	child->setSurfaceBaseTraverse( NULL );
	_childDar.removeElement( child );
}

bool Panel::wasMousePressed( MouseCode code )
{
	return getApp()->wasMousePressed( code, this );
}

bool Panel::wasMouseDoublePressed( MouseCode code )
{
	return getApp()->wasMouseDoublePressed( code, this );
}

bool Panel::isMouseDown( MouseCode code )
{
	return getApp()->isMouseDown( code, this );
}

bool Panel::wasMouseReleased( MouseCode code )
{
	return getApp()->wasMouseReleased( code, this );
}

bool Panel::wasKeyPressed( KeyCode code )
{
	return getApp()->wasKeyPressed( code, this );
}

bool Panel::isKeyDown( KeyCode code )
{
	return getApp()->isKeyDown( code, this );
}

bool Panel::wasKeyTyped( KeyCode code )
{
	return getApp()->wasKeyTyped( code, this );
}

bool Panel::wasKeyReleased( KeyCode code )
{
	return getApp()->wasKeyReleased( code, this );
}

void Panel::addInputSignal( InputSignal* s )
{
	_inputSignalDar.putElement( s );
}

void Panel::removeInputSignal( InputSignal* s )
{
	_inputSignalDar.removeElement( s );
}

void Panel::addRepaintSignal( RepaintSignal* s )
{
	_repaintSignalDar.putElement( s );
}

void Panel::removeRepaintSignal( RepaintSignal* s )
{
	_repaintSignalDar.removeElement( s );
}

bool Panel::isWithin( int x, int y )
{
	screenToLocal( x, y );
	if ( x < 0 )
		return false;
	if ( y >= 0 && _size[0] > x )
		return y < _size[1];
	return false;
}

Panel* Panel::isWithinTraverse( int x, int y )
{
	if ( !_visible )
		return NULL;
	if ( !isWithin( x, y ) )
		return NULL;

	for ( int i = _childDar.getCount() - 1; i >= 0; --i )
	{
		Panel* hit = _childDar[i]->isWithinTraverse( x, y );
		if ( hit != NULL )
			return hit;
	}
	return this;
}

void Panel::localToScreen( int& x, int& y )
{
	x += _loc[0];
	y += _loc[1];
}

void Panel::screenToLocal( int& x, int& y )
{
	x -= _loc[0];
	y -= _loc[1];
}

void Panel::setCursor( Cursor* cursor )
{
	_schemeCursor = Scheme::scu_user;
	_cursor = cursor;
}

void Panel::setCursor( Scheme::SchemeCursor scu )
{
	_cursor = NULL;
	_schemeCursor = scu;
}

Cursor* Panel::getCursor()
{
	if ( getApp()->getCursorOveride() != NULL )
		return getApp()->getCursorOveride();
	if ( _schemeCursor != Scheme::scu_user )
		return getApp()->getScheme()->getCursor( _schemeCursor );
	return _cursor;
}

void Panel::setMinimumSize( int wide, int tall )
{
	_minimumSize[0] = wide;
	_minimumSize[1] = tall;
}

void Panel::getMinimumSize( int& wide, int& tall )
{
	wide = _minimumSize[0];
	tall = _minimumSize[1];
}

void Panel::requestFocus()
{
	getApp()->requestFocus( this );
}

bool Panel::hasFocus()
{
	return getApp()->getFocus() == this;
}

int Panel::getChildCount()
{
	return _childDar.getCount();
}

Panel* Panel::getChild( int index )
{
	if ( index < 0 || index >= _childDar.getCount() )
		return NULL;
	return _childDar[index];
}

void Panel::setLayout( Layout* layout )
{
	_layout = layout;
	invalidateLayout( false );
}

void Panel::invalidateLayout( bool layoutNow )
{
	_needsLayout = true;
	if ( layoutNow )
		internalPerformLayout();
}

void Panel::setFocusNavGroup( FocusNavGroup* focusNavGroup )
{
#if VGUI_ABI_COMPAT_MODE != 1
	// With VGUI_ABI_COMPAT_MODE == 1 a deleted group does not clear
	// _focusNavGroup, so the old group may be gone and is not touched.
	if ( _focusNavGroup != NULL && _focusNavGroup != focusNavGroup )
		_focusNavGroup->removePanel( this );
#endif
	_focusNavGroup = focusNavGroup;
	if ( focusNavGroup != NULL )
		focusNavGroup->addPanel( this );
}

void Panel::requestFocusPrev()
{
	if ( _focusNavGroup != NULL )
		_focusNavGroup->requestFocusPrev();
}

void Panel::requestFocusNext()
{
	if ( _focusNavGroup != NULL )
		_focusNavGroup->requestFocusNext();
}

void Panel::addFocusChangeSignal( FocusChangeSignal* s )
{
	_focusChangeSignalDar.putElement( s );
}

bool Panel::isAutoFocusNavEnabled()
{
	return _autoFocusNavEnabled;
}

void Panel::setAutoFocusNavEnabled( bool state )
{
	_autoFocusNavEnabled = state;
}

void Panel::setBorder( Border* border )
{
	_border = border;
}

void Panel::setPaintBorderEnabled( bool state )
{
	_paintBorderEnabled = state;
}

void Panel::setPaintBackgroundEnabled( bool state )
{
	_paintBackgroundEnabled = state;
}

void Panel::setPaintEnabled( bool state )
{
	_paintEnabled = state;
}

void Panel::getInset( int& left, int& top, int& right, int& bottom )
{
	if ( _border != NULL )
		_border->getInset( left, top, right, bottom );
	else
	{
		left = 0;
		top = 0;
		right = 0;
		bottom = 0;
	}
}

void Panel::getPaintSize( int& wide, int& tall )
{
	if ( _border != NULL )
	{
		int left;
		int top;
		int right;
		int bottom;
		_border->getInset( left, top, right, bottom );
		wide = _size[0] - (left + right);
		tall = _size[1] - (top + bottom);
	}
	else
	{
		wide = _size[0];
		tall = _size[1];
	}
}

void Panel::setPreferredSize( int wide, int tall )
{
	_preferredSize[0] = wide;
	_preferredSize[1] = tall;
}

void Panel::getPreferredSize( int& wide, int& tall )
{
	wide = _preferredSize[0];
	tall = _preferredSize[1];
}

SurfaceBase* Panel::getSurfaceBase()
{
	return _surfaceBase;
}

bool Panel::isEnabled()
{
	return _enabled;
}

void Panel::setEnabled( bool state )
{
	_enabled = state;
}

void Panel::setBuildGroup( BuildGroup* buildGroup, const char* panelPersistanceName )
{
#if VGUI_ABI_COMPAT_MODE != 1
	// Also for the same group, which then lists the panel once, with the new
	// name. With VGUI_ABI_COMPAT_MODE == 1 a deleted group does not clear
	// _buildGroup, so the old group may be gone and is not touched.
	if ( _buildGroup != NULL )
		_buildGroup->panelRemoved( this );
#endif
	_buildGroup = buildGroup;
	if ( buildGroup != NULL )
		buildGroup->panelAdded( this, panelPersistanceName );
}

bool Panel::isBuildGroupEnabled()
{
	if ( _buildGroup == NULL )
		return false;
	return _buildGroup->isEnabled();
}

// Detaches every child as removeChild does, without deleting them. Like the
// original, it does not call a removeChild override.
void Panel::removeAllChildren()
{
	while ( _childDar.getCount() > 0 )
		Panel::removeChild( _childDar[_childDar.getCount() - 1] );
}

void Panel::repaintParent()
{
	if ( _parent != NULL )
		_parent->repaint();
}

Panel* Panel::createPropertyPanel()
{
	TreeFolder* root = new TreeFolder( "Properties" );
	TreeFolder* folder = new TreeFolder( "Panel" );
	folder->addChild( new Label( "setPos" ) );
	folder->addChild( new Label( "setSize" ) );
	folder->addChild( new Label( "setBorder" ) );
	folder->addChild( new Label( "setLayout" ) );
	root->addChild( folder );
	return root;
}

void Panel::getPersistanceText( char* buf, int bufLen )
{
	if ( buf == NULL || bufLen <= 0 )
	{
		return;
	}
	int x;
	int y;
	int wide;
	int tall;
	getBounds( x, y, wide, tall );
	snprintf( buf, (size_t)bufLen, "->setBounds(%d,%d,%d,%d);\n", x, y, wide, tall );
}

void Panel::applyPersistanceText( const char* )
{
}

void Panel::setFgColor( Scheme::SchemeColor sc )
{
	_fgColor.setColor( sc );
}

void Panel::setBgColor( Scheme::SchemeColor sc )
{
	_bgColor.setColor( sc );
}

void Panel::setFgColor( int r, int g, int b, int a )
{
	_fgColor.setColor( r, g, b, a );
}

void Panel::setBgColor( int r, int g, int b, int a )
{
	_bgColor.setColor( r, g, b, a );
}

void Panel::getFgColor( int& r, int& g, int& b, int& a )
{
	_fgColor.getColor( r, g, b, a );
}

void Panel::getBgColor( int& r, int& g, int& b, int& a )
{
	_bgColor.getColor( r, g, b, a );
}

void Panel::setBgColor( Color color )
{
	_bgColor = color;
}

void Panel::setFgColor( Color color )
{
	_fgColor = color;
}

void Panel::getBgColor( Color& color )
{
	color = _bgColor;
}

void Panel::getFgColor( Color& color )
{
	color = _fgColor;
}

void Panel::setAsMouseCapture( bool state )
{
	if ( state )
		getApp()->setMouseCapture( this );
	else
		getApp()->setMouseCapture( NULL );
}

void Panel::setAsMouseArena( bool )
{
	getApp()->setMouseArena( this );
}

App* Panel::getApp()
{
	return App::getInstance();
}

void Panel::getVirtualSize( int& wide, int& tall )
{
	getSize( wide, tall );
}

void Panel::setLayoutInfo( LayoutInfo* layoutInfo )
{
	_layoutInfo = layoutInfo;
}

LayoutInfo* Panel::getLayoutInfo()
{
	return _layoutInfo;
}

bool Panel::isCursorNone()
{
	Cursor* cursor = getCursor();
	if ( cursor == NULL )
		return true;
	return cursor->getDefaultCursor() == Cursor::dc_none;
}

void Panel::solveTraverse()
{
	if ( !_visible )
		return;
	solve();
	if ( _needsLayout )
		internalPerformLayout();
	solve();
	for ( int i = 0; i < _childDar.getCount(); ++i )
		_childDar[i]->solveTraverse();
}

void Panel::paintTraverse()
{
	paintTraverse( _needsRepaint );
}

void Panel::setSurfaceBaseTraverse( SurfaceBase* surfaceBase )
{
	_surfaceBase = surfaceBase;
	for ( int i = 0; i < _childDar.getCount(); ++i )
		_childDar[i]->setSurfaceBaseTraverse( surfaceBase );
}

void Panel::performLayout()
{
}

void Panel::internalPerformLayout()
{
	_needsLayout = false;
	if ( _layout != NULL )
		_layout->performLayout( this );
	else
		performLayout();
	repaint();
}

void Panel::drawSetColor( Scheme::SchemeColor sc )
{
	int r;
	int g;
	int b;
	int a;
	getApp()->getScheme()->getColor( sc, r, g, b, a );
	drawSetColor( r, g, b, a );
}

void Panel::drawSetColor( int r, int g, int b, int a )
{
	_surfaceBase->drawSetColor( r, g, b, a );
}

void Panel::drawFilledRect( int x0, int y0, int x1, int y1 )
{
	_surfaceBase->drawFilledRect( x0, y0, x1, y1 );
}

void Panel::drawOutlinedRect( int x0, int y0, int x1, int y1 )
{
	_surfaceBase->drawOutlinedRect( x0, y0, x1, y1 );
}

void Panel::drawSetTextFont( Scheme::SchemeFont sf )
{
	drawSetTextFont( getApp()->getScheme()->getFont( sf ) );
}

void Panel::drawSetTextFont( Font* font )
{
	_surfaceBase->drawSetTextFont( font );
}

void Panel::drawSetTextColor( Scheme::SchemeColor sc )
{
	int r;
	int g;
	int b;
	int a;
	getApp()->getScheme()->getColor( sc, r, g, b, a );
	drawSetTextColor( r, g, b, a );
}

void Panel::drawSetTextColor( int r, int g, int b, int a )
{
	_surfaceBase->drawSetTextColor( r, g, b, a );
}

void Panel::drawSetTextPos( int x, int y )
{
	_surfaceBase->drawSetTextPos( x, y );
}

void Panel::drawPrintText( const char* str, int strlen )
{
	_surfaceBase->drawPrintText( str, strlen );
}

void Panel::drawPrintText( int x, int y, const char* str, int strlen )
{
	_surfaceBase->drawSetTextPos( x, y );
	_surfaceBase->drawPrintText( str, strlen );
}

void Panel::drawPrintChar( char ch )
{
	char text[1];
	text[0] = ch;
	_surfaceBase->drawPrintText( text, 1 );
}

void Panel::drawPrintChar( int x, int y, char ch )
{
	char text[1];
	text[0] = ch;
	_surfaceBase->drawSetTextPos( x, y );
	_surfaceBase->drawPrintText( text, 1 );
}

void Panel::drawSetTextureRGBA( int id, const char* rgba, int wide, int tall )
{
	if ( _surfaceBase != NULL )
		_surfaceBase->drawSetTextureRGBA( id, rgba, wide, tall );
}

void Panel::drawSetTexture( int id )
{
	_surfaceBase->drawSetTexture( id );
}

void Panel::drawTexturedRect( int x0, int y0, int x1, int y1 )
{
	_surfaceBase->drawTexturedRect( x0, y0, x1, y1 );
}

void Panel::solve()
{
	int x = _pos[0];
	int y = _pos[1];
	_loc[0] = x;
	_loc[1] = y;
	if ( _parent == NULL )
	{
		_clipRect[0] = x;
		_clipRect[1] = y;
		_clipRect[2] = x + _size[0];
		_clipRect[3] = y + _size[1];
		return;
	}

	int left;
	int top;
	int right;
	int bottom;
	_parent->getInset( left, top, right, bottom );
	x = _loc[0];
	y = _loc[1];
	Panel* parent = _parent;
	int locX = parent->_loc[0] + left + x;
	int locY = parent->_loc[1] + top + y;
	_loc[0] = locX;
	_loc[1] = locY;
	_clipRect[0] = locX;
	_clipRect[1] = locY;
	int x1 = locX + _size[0];
	int y1 = locY + _size[1];
	_clipRect[2] = x1;
	_clipRect[3] = y1;
	if ( locX < parent->_clipRect[0] )
		_clipRect[0] = parent->_clipRect[0];
	if ( locY < parent->_clipRect[1] )
		_clipRect[1] = parent->_clipRect[1];
	if ( x1 > parent->_clipRect[2] )
		_clipRect[2] = parent->_clipRect[2];
	if ( y1 > parent->_clipRect[3] )
		_clipRect[3] = parent->_clipRect[3];
}

void Panel::paintTraverse( bool repaint )
{
	if ( !_visible )
		return;

	if ( _needsRepaint )
		repaint = true;
	_needsRepaint = false;

	if ( _clipRect[2] <= _clipRect[0] || _clipRect[3] <= _clipRect[1] )
		repaint = false;
	else if ( repaint && ( _paintBorderEnabled || _paintBackgroundEnabled || _paintEnabled ) )
	{
		_surfaceBase->pushMakeCurrent( this, true );
		if ( _border != NULL && _paintBorderEnabled )
			_border->doPaint( this );
		if ( _paintBackgroundEnabled )
			paintBackground();
		if ( _paintEnabled )
			paint();
		_surfaceBase->popMakeCurrent( this );
	}

	for ( int i = 0; i < _childDar.getCount(); ++i )
		_childDar[i]->paintTraverse( repaint );

	if ( repaint && isBuildGroupEnabled() && isEnabled() )
	{
		_surfaceBase->pushMakeCurrent( this, false );
		paintBuildOverlay();
		_surfaceBase->popMakeCurrent( this );
	}

	if ( _surfaceBase->_needsSwap && _surfaceBase->getPanel() == this )
	{
		_surfaceBase->_needsSwap = false;
		_surfaceBase->swapBuffers();
	}
}

void Panel::paintBackground()
{
	int wide;
	int tall;
	getPaintSize( wide, tall );
	int r;
	int g;
	int b;
	int a;
	getBgColor( r, g, b, a );
	drawSetColor( r, g, b, a );
	drawFilledRect( 0, 0, wide, tall );
}

void Panel::paint()
{
}

void Panel::paintBuildOverlay()
{
	int wide;
	int tall;
	getSize( wide, tall );
	drawSetColor( Scheme::sc_black );
	drawFilledRect( 0, 0, wide, 2 );
	drawFilledRect( 0, tall - 2, wide, tall );
	drawFilledRect( 0, 2, 2, tall - 2 );
	drawFilledRect( wide - 2, 2, wide, tall - 2 );
}

void Panel::internalCursorMoved( int x, int y )
{
	if ( isCursorNone() )
		return;
	if ( isBuildGroupEnabled() )
	{
		_buildGroup->cursorMoved( x, y, this );
		return;
	}
	screenToLocal( x, y );
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->cursorMoved( x, y, this );
}

void Panel::internalCursorEntered()
{
	if ( isCursorNone() )
		return;
	if ( isBuildGroupEnabled() )
		return;
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->cursorEntered( this );
}

void Panel::internalCursorExited()
{
	if ( isCursorNone() )
		return;
	if ( isBuildGroupEnabled() )
		return;
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->cursorExited( this );
}

void Panel::internalMousePressed( MouseCode code )
{
	if ( isCursorNone() )
		return;
	if ( isBuildGroupEnabled() )
	{
		_buildGroup->mousePressed( code, this );
		return;
	}
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->mousePressed( code, this );
}

void Panel::internalMouseDoublePressed( MouseCode code )
{
	if ( isCursorNone() )
		return;
	if ( isBuildGroupEnabled() )
	{
		_buildGroup->mouseDoublePressed( code, this );
		return;
	}
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->mouseDoublePressed( code, this );
}

void Panel::internalMouseReleased( MouseCode code )
{
	if ( isCursorNone() )
		return;
	if ( isBuildGroupEnabled() )
	{
		_buildGroup->mouseReleased( code, this );
		return;
	}
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->mouseReleased( code, this );
}

void Panel::internalMouseWheeled( int delta )
{
	if ( isBuildGroupEnabled() )
		return;
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->mouseWheeled( delta, this );
}

void Panel::internalKeyPressed( KeyCode code )
{
	if ( isBuildGroupEnabled() )
		return;
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->keyPressed( code, this );
}

void Panel::internalKeyTyped( KeyCode code )
{
	if ( isBuildGroupEnabled() )
	{
		_buildGroup->keyTyped( code, this );
		return;
	}

	if ( isAutoFocusNavEnabled() )
	{
		if ( code == KeyCode::KEY_TAB )
		{
			if ( isKeyDown( KeyCode::KEY_LSHIFT ) || isKeyDown( KeyCode::KEY_RSHIFT ) ||
				isKeyDown( KeyCode::KEY_LCONTROL ) )
				requestFocusPrev();
			else
				requestFocusNext();
		}
		else if ( code == KeyCode::KEY_UP )
			requestFocusPrev();
		else if ( code == KeyCode::KEY_DOWN )
			requestFocusNext();
	}

	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->keyTyped( code, this );
}

void Panel::internalKeyReleased( KeyCode code )
{
	if ( isBuildGroupEnabled() )
		return;
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->keyReleased( code, this );
}

void Panel::internalKeyFocusTicked()
{
	if ( isBuildGroupEnabled() )
		return;
	for ( int i = 0; i < _inputSignalDar.getCount(); ++i )
		_inputSignalDar[i]->keyFocusTicked( this );
}

void Panel::internalFocusChanged( bool lost )
{
	if ( !lost && _focusNavGroup != NULL )
		_focusNavGroup->setCurrentPanel( this );
	for ( int i = 0; i < _focusChangeSignalDar.getCount(); ++i )
		_focusChangeSignalDar[i]->focusChanged( lost, this );
}

void Panel::internalSetCursor()
{
	if ( isBuildGroupEnabled() )
		_buildGroup->getCursor( this );
	if ( _surfaceBase != NULL )
		_surfaceBase->setCursor( getCursor() );
}
