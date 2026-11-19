#ifndef VGUI_SURFACE_H
#define VGUI_SURFACE_H

#include "VGUI_SurfaceBase.h"

#define VGUI_TEXTURE_CAPACITY	128

namespace vgui
{

class SurfacePlat;

class VGUIAPI Surface : public SurfaceBase
{
public:
	Surface( Panel* embeddedPanel );
	Surface( const Surface& other );
	Surface& operator=( const Surface& other );
#if defined( _WIN32 )
	~Surface();
#endif

	virtual void setTitle( const char* title ) override;
	virtual bool setFullscreenMode( int wide, int tall, int bpp ) override;
	virtual void setWindowedMode() override;
	virtual void setAsTopMost( bool state ) override;
	virtual int getModeInfoCount() override;
	virtual void createPopup( Panel* embeddedPanel ) override;
	virtual bool hasFocus() override;
	virtual bool isWithin( int x, int y ) override;
	virtual void GetMousePos( int& x, int& y ) override;

protected:
	virtual int createNewTextureID() override;
	virtual void drawSetColor( int r, int g, int b, int a ) override;
	virtual void drawFilledRect( int x0, int y0, int x1, int y1 ) override;
	virtual void drawOutlinedRect( int x0, int y0, int x1, int y1 ) override;
	virtual void drawSetTextFont( Font* font ) override;
	virtual void drawSetTextColor( int r, int g, int b, int a ) override;
	virtual void drawSetTextPos( int x, int y ) override;
	virtual void drawPrintText( const char* text, int textLen ) override;
	virtual void drawSetTextureRGBA( int id, const char* rgba, int wide, int tall ) override;
	virtual void drawSetTexture( int id ) override;
	virtual void drawTexturedRect( int x0, int y0, int x1, int y1 ) override;
	virtual void invalidate( Panel* panel ) override;
	virtual bool createPlat();
	virtual bool recreateContext();
	virtual void enableMouseCapture( bool state ) override;
	virtual void setCursor( Cursor* cursor ) override;
	virtual void swapBuffers() override;
	virtual void pushMakeCurrent( Panel* panel, bool useInsets ) override;
	virtual void popMakeCurrent( Panel* panel ) override;
	virtual void applyChanges() override;

protected:
	SurfacePlat*	_plat;
	// _needsSwap, _embeddedPanel and _unusedModeInfoDar shadow the SurfaceBase
	// members of the same names and are only kept for the original Surface
	// layout. Use SurfaceBase's members (or getPanel() / the mode-info methods).
	bool			_needsSwap;
	Panel*			_embeddedPanel;
	Dar<char*>		_unusedModeInfoDar;
};

}

#endif
