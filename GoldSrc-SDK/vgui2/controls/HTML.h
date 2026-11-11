//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: Creates a HTML control
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "EditablePanel.h"
#include "PHandle.h"
#include "html/ichromehtmlwrapper.h"
#include "utlstring.h"
#include "utlvector.h"

class HTMLComboBoxHost;

class IHTMLResponses_HL1
{
public:
	virtual void BrowserReady() = 0;
	virtual void BrowserNeedsPaint( int textureid, int wide, int tall, const unsigned char* rgba, int updatex,
		int updatey, int updatewide, int updatetall, int combobox_wide, int combobox_tall,
		const unsigned char* combobox_rgba ) = 0;

	virtual void BrowserStartRequest( const char* url, const char* target, const char* postdata, bool isredirect ) = 0;
	virtual void BrowserURLChanged( const char* url, const char* postdata, bool isredirect ) = 0;
	virtual void BrowserFinishedRequest( const char* url, const char* pagetitle ) = 0;

	virtual void BrowserShowPopup() = 0;
	virtual void BrowserHidePopup() = 0;
	virtual void BrowserSizePopup( int x, int y, int wide, int tall ) = 0;

	virtual void BrowserHorizontalScrollBarSizeResponse( int x, int y, int wide, int tall, int scroll, int scroll_max,
		float zoom ) = 0;
	virtual void BrowserVerticalScrollBarSizeResponse( int x, int y, int wide, int tall, int scroll, int scroll_max,
		float zoom ) = 0;
	virtual void BrowserGetZoomResponse( float zoom ) = 0;
	virtual void BrowserCanGoBackandForward( bool back, bool forward ) = 0;

	virtual void BrowserJSAlert( const char* message ) = 0;
	virtual void BrowserJSConfirm( const char* message ) = 0;

	virtual void BrowserPopupHTMLWindow( const char* url, int wide, int tall, int x, int y ) = 0;
	virtual void BrowserSetHTMLTitle( const char* title ) = 0;
	virtual void BrowserLoadingResource() = 0;
	virtual void BrowserStatusText( const char* text ) = 0;
	virtual void BrowserSetCursor( int cursor ) = 0;
	virtual void BrowserFileLoadDialog() = 0;

	virtual void BrowserShowToolTip( const char* text ) = 0;
	virtual void BrowserUpdateToolTip( const char* text ) = 0;
	virtual void BrowserHideToolTip() = 0;

	virtual void BrowserClose() = 0;
	virtual void BrowserLinkAtPositionResponse( const char* url, int x, int y ) = 0;
};

#if defined( BSHIFT_CLIENT ) || defined( _WIN32 )
class IHTMLSerializer
{
public:
	virtual IHTMLResponses* GetResponseTarget() = 0;
	virtual int BrowserGetIndex() = 0;
	virtual void BrowserPosition( int x, int y ) = 0;
	virtual void PostURL( const char* url, const char* postData ) = 0;
	virtual void BrowserErrorStrings( const char* title, const char* connectionProblem, const char* retry,
		const char* unknown, const char* cacheMiss, const char* badURL, const char* detailBadURL ) = 0;
	virtual void StopLoad() = 0;
	virtual void Reload() = 0;
	virtual void GoBack() = 0;
	virtual void GoForward() = 0;
	virtual void MouseDown( int code ) = 0;
	virtual void MouseUp( int code ) = 0;
	virtual void MouseWheel( int delta ) = 0;
	virtual void MouseMove( int x, int y ) = 0;
	virtual void MouseDoubleClick( int code ) = 0;
	virtual void BrowserSize( int wide, int tall ) = 0;
	virtual void KeyUp( int virtualKey, int modifiers ) = 0;
	virtual void KeyDown( int virtualKey, int modifiers ) = 0;
	virtual void KeyChar( int unichar ) = 0;
	virtual void RunJavaScript( const char* script ) = 0;
	virtual void SetHorizontalScroll( int value ) = 0;
	virtual void SetVerticalScroll( int value ) = 0;
	virtual void SetFocus( bool state ) = 0;
	virtual void AddHeader( const char* header, const char* value ) = 0;
	virtual void NeedsPaintResponse( int textureID ) = 0;
	virtual void StartRequestResponse( bool allow ) = 0;
	virtual void RequestBrowserSizes() = 0;
	virtual void JSDialogResponse( int result ) = 0;
	virtual void GetLinkAtPosition( int x, int y ) = 0;

	std::uintptr_t GetBrowserHandle() { return (std::uintptr_t)GetResponseTarget(); }
	int GetBrowserIndex() { return BrowserGetIndex(); }
	void ExecuteJavascript( const char* script ) { RunJavaScript( script ); }
	void FullRepaint() { RequestBrowserSizes(); }
	void LinkAtPosition( int x, int y ) { GetLinkAtPosition( x, y ); }
};
#else
class IHTMLSerializer
{
public:
	virtual ~IHTMLSerializer()
	{
	}

	virtual void MouseMove( int x, int y ) = 0;
	virtual std::uintptr_t GetBrowserHandle() = 0;

	virtual void StopLoad() = 0;
	virtual void Reload() = 0;
	virtual void GoBack() = 0;
	virtual void GoForward() = 0;
	virtual void ExecuteJavascript( const char* script ) = 0;

	virtual void MouseDoubleClick( int code ) = 0;
	virtual void KeyChar( wchar_t unichar ) = 0;
	virtual void MouseWheel( int delta ) = 0;
	virtual void StartRequestResponse( bool allow ) = 0;
	virtual void FullRepaint() = 0;
	virtual void JSDialogResponse( int result ) = 0;
	virtual void NeedsPaintResponse( int textureId ) = 0;
	virtual void SetFocus( bool state ) = 0;

	virtual void SetHorizontalScroll( int value ) = 0;
	virtual void SetVerticalScroll( int value ) = 0;
	virtual void BrowserSize( int wide, int tall ) = 0;
	virtual void BrowserPosition( int x, int y ) = 0;

	virtual void LinkAtPosition( int x, int y ) = 0;
	virtual void MouseDown( int code ) = 0;
	virtual void MouseUp( int code ) = 0;
	virtual int GetBrowserIndex() = 0;
	virtual void PostURL( const char* url, const char* postData ) = 0;
	virtual void KeyUp( int virtualKey, int modifiers ) = 0;
	virtual void KeyDown( int virtualKey, int modifiers ) = 0;
	virtual void AddHeader( const char* header, const char* value ) = 0;
};
#endif

namespace vgui2
{
class Label;
class Menu;
class ScrollBar;
class TextEntry;

class HTML : public Panel, public IHTMLResponses_HL1
{
	DECLARE_CLASS_SIMPLE( HTML, Panel );

public:
	struct ScrollData_t
	{
		bool	m_bVisible; // is the scroll bar visible
		int		m_nX;
		int		m_nY;
		int		m_nWide;
		int		m_nTall;
		int		m_nMax; // most amount of pixels we can scroll
		int		m_nScroll; // currently scrolled amount of pixels
		float	m_flZoom; // zoom level this scroll bar is for
	};

	struct LinkAtPos_t
	{
		unsigned int	m_nX;
		unsigned int	m_nY;
		CUtlString		m_sURL;
	};

	struct CustomURLHandler_t
	{
		PHandle	hPanel;
		char	url[32];
	};

	class CHTMLFindBar : public EditablePanel
	{
		DECLARE_CLASS_SIMPLE( CHTMLFindBar, EditablePanel );

	public:
		explicit CHTMLFindBar( HTML* parent );
		~CHTMLFindBar() override;

		void OnCommand( const char* pchCmd ) override;

		TextEntry*	m_pFindBar;
		HTML*		m_pParent;
		Label*		m_pFindCountLabel;
		bool		m_bHidden;
	};

	HTML( Panel* parent, const char* name, bool allowJavaScript, bool bPopupWindow );
	~HTML() override;

	void OnSizeChanged( int wide, int tall ) override;
	void OnMove() override;
	void OnSetFocus() override;
	void OnKillFocus() override;
	void OnCursorMoved( int x, int y ) override;
	void OnMousePressed( MouseCode code ) override;
	void OnMouseDoublePressed( MouseCode code ) override;
	void OnMouseReleased( MouseCode code ) override;
	void OnMouseWheeled( int delta ) override;
	void OnKeyCodeTyped( KeyCode code ) override;
	void OnKeyTyped( wchar_t unichar ) override;
	void OnKeyCodeReleased( KeyCode code ) override;
	void Paint() override;
	void PostChildPaint() override;
	void PerformLayout() override;
	void OnCommand( const char* command ) override;
	void ApplySchemeSettings( IScheme* scheme ) override;

	// IHTML pass through functions
	virtual void OpenURL( const char* URL, const char* postData, bool force );
	virtual bool StopLoading();
	virtual bool Refresh();
	virtual void RunJavascript( const char* pchScript );
	virtual void GoBack();
	virtual void GoForward();
	virtual bool BCanGoBack();
	virtual bool BCanGoFoward();

	// event functions you can override and specialize behavior of
	virtual bool OnStartRequest( const char* url, const char* target, const char* pchPostData, bool bIsRedirect );
	virtual void OnFinishRequest( const char* url, const char* pageTitle );
	virtual void OnSetHTMLTitle( const char* title );
	virtual void OnLinkAtPosition( const char* url );
	virtual void OnURLChanged( const char* url, const char* postData, bool isRedirect );
	// events callbacks
	// Called when the control wishes to go to a new URL (either through the OpenURL() method or internally triggered).
	// Return TRUE to allow it to continue and FALSE to stop it loading the URL.
	virtual bool OnOpenNewTab( const char* url, bool foreground );

	// configuration
	virtual void SetScrollbarsEnabled( bool state );
	virtual void SetContextMenuEnabled( bool state );
	virtual void SetViewSourceEnabled( bool state );
	virtual void NewWindowsOnly( bool state );
	virtual void AddCustomURLHandler( const char* customProtocolName, Panel* target );

	MESSAGE_FUNC_INT( OnSetCursorVGUI, "SetCursor", cursor );

	virtual void BrowserReady();
	virtual void BrowserNeedsPaint( int textureid, int wide, int tall, const unsigned char* rgba, int updatex,
		int updatey, int updatewide, int updatetall, int combobox_wide, int combobox_tall,
		const unsigned char* combobox_rgba );
	virtual void BrowserStartRequest( const char* url, const char* target, const char* postdata, bool isredirect );
	virtual void BrowserURLChanged( const char* url, const char* postdata, bool isredirect );
	virtual void BrowserFinishedRequest( const char* url, const char* pagetitle );
	virtual void BrowserShowPopup();
	virtual void BrowserHidePopup();
	virtual void BrowserSizePopup( int x, int y, int wide, int tall );
	virtual void BrowserHorizontalScrollBarSizeResponse( int x, int y, int wide, int tall, int scroll, int scroll_max,
		float zoom );
	virtual void BrowserVerticalScrollBarSizeResponse( int x, int y, int wide, int tall, int scroll, int scroll_max,
		float zoom );
	virtual void BrowserGetZoomResponse( float flZoom );
	virtual void BrowserCanGoBackandForward( bool bgoback, bool bgoforward );
	virtual void BrowserJSAlert( const char* message );
	virtual void BrowserJSConfirm( const char* message );
	virtual void BrowserPopupHTMLWindow( const char* url, int wide, int tall, int x, int y );
	virtual void BrowserSetHTMLTitle( const char* title );
	virtual void BrowserLoadingResource();
	virtual void BrowserStatusText( const char* text );
	virtual void BrowserSetCursor( int in_cursor );
	virtual void BrowserFileLoadDialog();
	virtual void BrowserShowToolTip( const char* text );
	virtual void BrowserUpdateToolTip( const char* text );
	virtual void BrowserHideToolTip();
	virtual void BrowserClose();
	virtual void BrowserLinkAtPositionResponse( const char* url, int x, int y );

	virtual void BrowserResize();
	virtual void CalcScrollBars( int w, int h );

	void PostURL( const char* URL, const char* pchPostData, bool force );
	void GetLinkAtPosition( int x, int y );
	void UpdateSizeAndScrollBars();

	void ShowFindDialog();
	void HideFindDialog();
	bool FindDialogVisible();
	void Find( const char* pchSubStr );
	void FindNext();
	void StopFind();

	bool IsScrolledToBottom();
	bool IsScrollbarVisible();
	void AddHeader( const char* pchHeader, const char* pchValue );
	void PaintComboBox();

private:
	void UpdateCachedHTMLValues();

	IHTMLSerializer*	m_Serializer;
	Menu*				m_pContextMenu;

public:
	MESSAGE_FUNC( OnSliderMoved, "ScrollBarSliderMoved" );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
	MESSAGE_FUNC( OnFileSelectionCancelled, "FileSelectionCancelled" );
	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC_PTR( OnEditNewLine, "TextNewLine", panel );
	MESSAGE_FUNC_INT( DismissJSDialog, "DismissJSDialog", result );

private:
	Panel*							m_pInteriorPanel;
	ScrollBar*						_hbar;
	ScrollBar*						_vbar;
	PHandle							m_hFileOpenDialog;
	CHTMLFindBar*					m_pFindBar;
	::HTMLComboBoxHost*				m_pComboBoxHost;

	int								m_iMouseX;
	int								m_iMouseY;
	int								m_iScrollBorderX;
	int								m_iScrollBorderY;
	int								m_iWideLastHTMLSize;
	int								m_iTalLastHTMLSize;
	int								m_iCopyLinkMenuItemID;

	bool							m_bScrollBarEnabled;
	bool							m_bContextMenuEnabled;
	int								m_iScrollbarSize;
	bool							m_bNewWindowsOnly;
	int								m_nViewSourceAllowedIndex;

	CUtlString						m_sDragURL;
	int								m_iDragStartX;
	int								m_iDragStartY;

	CUtlVector<CustomURLHandler_t>	m_CustomURLHandlers;

	int								m_iBrowser;
	int								m_iHTMLTextureID; // vgui texture id
	// Track the texture width and height requested so we can tell
	// when the size has changed and reallocate the texture.
	int								m_allocedTextureWidth;
	int								m_allocedTextureHeight;
	int								m_iComboBoxTextureID;
	int								m_allocedComboBoxWidth;
	int								m_allocedComboBoxHeight;

	CUtlString						m_sCurrentURL; // the url of our current page
	// find in page state
	bool							m_bInFind;
	CUtlString						m_sLastSearchString;
	bool							m_bCanGoBack; // cache of forward and back state
	bool							m_bCanGoForward;

	LinkAtPos_t						m_LinkAtPos; // cache for link at pos requests, because the request is async
	bool							m_bRequestingDragURL; // true if we need a response for a drag url loc
	bool							m_bRequestingCopyLink; // true if we wanted to copy the link under the cursor

	ScrollData_t					m_scrollHorizontal; // details of horizontal scroll bar
	ScrollData_t					m_scrollVertical; // details of vertical scroll bar
	float							m_flZoom; // current page zoom level

	CUtlString						m_sPendingURLLoad; // cache of url to load if we get a PostURL before the cef object is mage
	CUtlString						m_sPendingPostData; // cache of the post data for above

	friend class ::HTMLComboBoxHost;
};

inline void HTML::OnFinishRequest( const char*, const char* )
{
}

inline void HTML::OnSetHTMLTitle( const char* )
{
}

inline void HTML::OnLinkAtPosition( const char* )
{
}

inline void HTML::OnURLChanged( const char*, const char*, bool )
{
}

inline bool HTML::OnOpenNewTab( const char*, bool )
{
	return false;
}
}

class HTMLInterior : public vgui2::Panel
{
	DECLARE_CLASS_SIMPLE( HTMLInterior, Panel );

public:
	explicit HTMLInterior( vgui2::HTML* parent );
	~HTMLInterior() override;

private:
	vgui2::HTML* m_pHTML;
};

class HTMLComboBoxHost : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE( HTMLComboBoxHost, EditablePanel );

public:
	explicit HTMLComboBoxHost( vgui2::HTML* parent );
	~HTMLComboBoxHost() override;

	void PerformLayout() override;
	void PaintBackground() override;
	void OnKillFocus() override;

	void OnCursorMoved( int x, int y ) override;
	void OnMousePressed( vgui2::MouseCode code ) override;
	void OnMouseDoublePressed( vgui2::MouseCode code ) override;
	void OnMouseReleased( vgui2::MouseCode code ) override;
	void OnMouseWheeled( int delta ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnKeyTyped( wchar_t unichar ) override;
	void OnKeyCodeReleased( vgui2::KeyCode code ) override;

private:
	vgui2::HTML* m_pParent;
};
