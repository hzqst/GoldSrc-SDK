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

#include "Panel.h"
#include "utlvector.h"

namespace vgui2
{
class IImage;
class TextImage;

//-----------------------------------------------------------------------------
// Purpose: Contains and displays a set of images
//			By default starts with one TextImage
//-----------------------------------------------------------------------------
class Label : public Panel
{
	DECLARE_CLASS_SIMPLE( Label, Panel );

public:
	// Constructors
	Label( Panel* parent, const char* name, const char* text );
	Label( Panel* parent, const char* name, const wchar_t* text );
	~Label() override;

	// Take the string and looks it up in the localization file to convert it to unicode
	virtual void SetText( const char* text );
	// Set unicode text directly
	virtual void SetText( const wchar_t* text );

	// Get the current text
	virtual void GetText( char* buffer, int bufferSize );
	virtual void GetText( wchar_t* buffer, int bufferSize );

	// Content alignment
	// Get the size of the content within the label
	virtual void GetContentSize( int& width, int& height );

	// Set how the content aligns itself within the label
	// alignment code, used to determine how the images are layed out within the Label
	enum Alignment
	{
		a_northwest = 0,
		a_north = 1,
		a_northeast = 2,
		a_west = 3,
		a_center = 4,
		a_east = 5,
		a_southwest = 6,
		a_south = 7,
		a_southeast = 8
	};

	enum EColorState
	{
		CS_NORMAL = 0,
		CS_DULL = 1,
		CS_BRIGHT = 2,
		CS_LIGHT = 3
	};

	virtual void SetContentAlignment( Alignment alignment );
	void SetEnabled( bool state ) override;

	// Additional offset at the Start of the text (from whichever sides it is aligned)
	virtual void SetTextInset( int xInset, int yInset );

	// Text colors
	void SetFgColor( Color color ) override;
	Color GetFgColor() override;
	// colors to use when the label is disabled
	virtual void SetDisabledFgColor1( Color color );
	virtual void SetDisabledFgColor2( Color color );
	virtual Color GetDisabledFgColor1();
	virtual Color GetDisabledFgColor2();
	virtual void SetTextColorState( EColorState state );

	// Font
	virtual void SetFont( HFont font );
	virtual HFont GetFont();

	// Hotkey
	Panel* HasHotkey( wchar_t key ) override;
	virtual void SetHotkey( wchar_t key );

	// Labels can be associated with controls, and alter behaviour based on the associates behaviour
	// If the associate is disabled, so are we
	// If the associate has focus, we may alter how we draw
	// If we get a hotkey press or focus message, we forward the focus to the associate
	virtual void SetAssociatedControl( Panel* control );

	// Multiple image handling
	// Images are drawn from left to right across the label, ordered by index
	// By default there is a TextImage in position 0 (see GetTextImage()/SetTextImageIndex())
	virtual int AddImage( IImage* image, int offset ); // Return the index the image was placed in
	virtual void SetImageAtIndex( int index, IImage* image, int offset );
	virtual void SetImagePreOffset( int index, int offset ); // Set the offset in pixels before the image
	virtual IImage* GetImageAtIndex( int index );
	virtual int GetImageCount();
	virtual void ClearImages();
	// fixes the layout bounds of the image within the label
	virtual void SetImageBounds( int index, int xPosition, int width );

	void SetProportionalScaleImages( bool state );

	// Teturns a pointer to the default text image
	virtual TextImage* GetTextImage();

	// Moves where the default text image is within the image array (it starts in position 0)
	// Setting it to -1 removes it from the image list
	// Returns the index the default text image was previously in
	virtual int SetTextImageIndex( int index );

	// Message handling
	// outputData - keyName is the name of the attribute requested.
	// for Labels "text" is an option that returns the default text image text
	// returns true on success in finding the requested value. false on failure.
	bool RequestInfo( KeyValues* output ) override;

	virtual void SizeToContents();

protected:
	void PerformLayout() override;
	virtual wchar_t CalculateHotkey( const char* text );
	virtual wchar_t CalculateHotkey( const wchar_t* text );
	virtual void ComputeAlignment( int& textWidth, int& textHeight, int& x, int& y );
	void Paint() override;
	MESSAGE_FUNC_PARAMS( OnSetText, "SetText", message );
	virtual void DrawDashedLine( int x0, int y0, int x1, int y1, int dashLength, int gapLength );
	void OnRequestFocus( VPANEL subFocus, VPANEL defaultPanel ) override;
	MESSAGE_FUNC( OnHotkeyPressed, "Hotkey" );
	void OnMousePressed( MouseCode code ) override;
	void OnSizeChanged( int width, int height ) override;

	// makes sure that the maxIndex will be a valid index
	virtual void EnsureImageCapacity( int count );

	// editing
	void ApplySchemeSettings( IScheme* scheme ) override;
	void GetSettings( KeyValues* settings ) override;
	void ApplySettings( KeyValues* settings ) override;
	const char* GetDescription() override;

	MESSAGE_FUNC_PARAMS( OnDialogVariablesChanged, "DialogVariables", variables );

private:
	void Init();
	void SetWrap( bool state );
	Alignment _contentAlignment;
	TextImage* _textImage; // this is the textImage, if the full text will not
							// fit we put as much as we can and add an elipsis (...)
	struct TImageInfo
	{
		IImage* image;
		short offset;
		short xpos;
		short width;
	};
	CUtlVector<TImageInfo> _imageDar;
	int _textInset[2];
	Color _disabledFgColor1;
	Color _disabledFgColor2;
	Color _associateColor;
	int _textImageIndex; // index in the image array that the default _textimage resides
	EColorState _textColorState;
	Panel* _associate;
	char* _associateName;
	char* _fontOverrideName;
	wchar_t _hotkey; // the hotkey contained in the text
	bool m_bWrap;
#ifndef BSHIFT_CLIENT
	bool m_bProportionalScaleImages;
#endif
};
} // namespace vgui
