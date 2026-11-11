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

#include "Label.h"
#include "TextImage.h"
#include "controls.h"
#include "vgui/ISurface.h"
#include "vgui/IScheme.h"
#include "../../engine/controls.h"
#include "../../engine/common.h"
#include "vgui/KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/IPanel.h"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwctype>

#if defined( _WIN32 ) && !defined( NDEBUG )
#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"vgui2\\controls\\Label.cpp" )
#else
#define ASSERT_AT( expression, line )	assert( expression )
#endif

#if !defined( _WIN32 )
#define _snprintf snprintf
#endif

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color Label::GetDisabledFgColor1()
{
	return _disabledFgColor1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Color Label::GetDisabledFgColor2()
{
	return _disabledFgColor2;
}

void Label::SetHotkey( wchar_t key )
{
	_hotkey = static_cast<wchar_t>( towlower( key ) );
}

void Label::SetAssociatedControl( Panel* control )
{
	_associate = control == this ? NULL : control;
}

void Label::SetContentAlignment( Alignment alignment )
{
	_contentAlignment = alignment;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Set the foreground color 1 color of the Label
//-----------------------------------------------------------------------------
void Label::SetDisabledFgColor1( Color color )
{
	_disabledFgColor1 = color;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Label::SetDisabledFgColor2( Color color )
{
	_disabledFgColor2 = color;
}

wchar_t Label::CalculateHotkey( const char* text )
{
	while ( *text )
	{
		if ( *text == '&' )
		{
			++text;
			if ( *text != '&' )
			{
				if ( !*text )
					return 0;
				if ( isalnum( *text ) )
					return static_cast<wchar_t>( tolower( *text ) );
			}
		}
		++text;
	}
	return 0;
}

wchar_t Label::CalculateHotkey( const wchar_t* text )
{
	if ( !text )
		return 0;
	while ( *text )
	{
		if ( *text == L'&' )
		{
			++text;
			if ( *text != L'&' )
			{
				if ( !*text )
					return 0;
				if ( iswalnum( *text ) )
					return static_cast<wchar_t>( towlower( *text ) );
			}
		}
		++text;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Get an IImage in the IImage array.
//-----------------------------------------------------------------------------
IImage* Label::GetImageAtIndex( int index )
{
#ifdef _WIN32
	ASSERT_AT( _imageDar.IsValidIndex( index ), 845 );
#endif
//	Assert( _imageDar[index].image );
	return _imageDar[index].image;
}

//-----------------------------------------------------------------------------
// Purpose: removes all images from the list
//			user is responsible for the memory
//-----------------------------------------------------------------------------
void Label::ClearImages()
{
	_imageDar.RemoveAll();
	_textImageIndex = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
TextImage* Label::GetTextImage()
{
	return _textImage;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of images in the array.
//-----------------------------------------------------------------------------
int Label::GetImageCount()
{
	return _imageDar.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Check if this label has a hotkey that is the key passed in.
//-----------------------------------------------------------------------------
Panel* Label::HasHotkey( wchar_t key )
{
	if ( _hotkey == key )
		return this;
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: fixes the layout bounds of the image within the label
//-----------------------------------------------------------------------------
void Label::SetImageBounds( int index, int xPosition, int width )
{
	_imageDar[index].xpos = static_cast<short>( xPosition );
	_imageDar[index].width = static_cast<short>( width );
}

//-----------------------------------------------------------------------------
// Purpose: Set the offset in pixels before the image
//-----------------------------------------------------------------------------
void Label::SetImagePreOffset( int index, int offset )
{
	if ( !_imageDar.IsValidIndex( index ) )
		return;

	if ( _imageDar.Element( index ).offset != offset )
	{
		_imageDar.Element( index ).offset = static_cast<short>( offset );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle a hotkey by passing on focus to associate
//-----------------------------------------------------------------------------
void Label::OnHotkeyPressed()
{
	if ( _associate && !IsBuildModeActive() )
		_associate->RequestFocus( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Redirect mouse pressed to giving focus to associate
//-----------------------------------------------------------------------------
void Label::OnMousePressed( MouseCode )
{
	if ( _associate && !IsBuildModeActive() )
		_associate->RequestFocus( 0 );
}

void Label::DrawDashedLine( int x0, int y0, int x1, int y1, int dashLength, int gapLength )
{
	int wide = static_cast<int>( static_cast<std::uint32_t>( x1 ) - static_cast<std::uint32_t>( x0 ) );
	int tall = static_cast<int>( static_cast<std::uint32_t>( y1 ) - static_cast<std::uint32_t>( y0 ) );
	if ( wide <= tall )
	{
		int y = y0;
		do
		{
			int end = static_cast<int>( static_cast<std::uint32_t>( y ) + static_cast<std::uint32_t>( dashLength ) );
			int bottom = end <= y1 ? end : y1;
			// draw partial
			surface()->DrawFilledRect( x0, y, x1, bottom );
			y = static_cast<int>( static_cast<std::uint32_t>( end ) + static_cast<std::uint32_t>( gapLength ) );
		} while ( y <= y1 );
	}
	else
	{
		int x = x0;
		do
		{
			int end = static_cast<int>( static_cast<std::uint32_t>( x ) + static_cast<std::uint32_t>( dashLength ) );
			int right = end <= x1 ? end : x1;
			// draw partial
			surface()->DrawFilledRect( x, y0, right, y1 );
			x = static_cast<int>( static_cast<std::uint32_t>( end ) + static_cast<std::uint32_t>( gapLength ) );
		} while ( x <= x1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return the text in the label
//-----------------------------------------------------------------------------
void Label::GetText( char* buffer, int bufferSize )
{
	_textImage->GetText( buffer, bufferSize );
}

//-----------------------------------------------------------------------------
// Purpose: Return the text in the label
//-----------------------------------------------------------------------------
void Label::GetText( wchar_t* buffer, int bufferSize )
{
	_textImage->GetText( buffer, bufferSize );
}

//-----------------------------------------------------------------------------
// Purpose: Set the font the text is drawn in
//-----------------------------------------------------------------------------
void Label::SetFont( HFont font )
{
	_textImage->SetFont( font );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Get the font the textImage is drawn in.
//-----------------------------------------------------------------------------
HFont Label::GetFont()
{
	return _textImage->GetFont();
}

void Label::SetTextInset( int xInset, int yInset )
{
	_textInset[0] = xInset;
	_textInset[1] = yInset;
	int width;
	int height;
	GetSize( width, height );
	std::uint32_t drawWidth = static_cast<std::uint32_t>( width ) - static_cast<std::uint32_t>( _textInset[0] );
	_textImage->SetDrawWidth( static_cast<int>( drawWidth ) );
}

void Label::SetText( const char* text )
{
	if ( text == NULL )
		text = "";
	// let the text image do the translation itself
	_textImage->SetText( text );
	wchar_t hotkey;
	if ( *text == '#' )
	{
		const wchar_t* localized = localize()->Find( text );
		hotkey = CalculateHotkey( localized );
	}
	else
	{
		hotkey = CalculateHotkey( text );
	}
	SetHotkey( hotkey );
	InvalidateLayout( false, false );
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Set unicode text directly
//-----------------------------------------------------------------------------
void Label::SetText( const wchar_t* text )
{
	_textImage->SetText( text );
	wchar_t hotkey = CalculateHotkey( text );
	SetHotkey( hotkey );
	InvalidateLayout( false, false );
	Repaint();
}

void Label::GetContentSize( int& width, int& height )
{
	if ( !GetFont() ) // we haven't loaded our font yet, so load it now
	{
		ISchemeManager* manager = scheme();
		HScheme schemeHandle = GetScheme();
		IScheme* currentScheme = manager->GetIScheme( schemeHandle );
		if ( currentScheme )
		{
			bool hdProportional = IsHDProportional();
			bool proportional = IsProportional();
			HFont font = currentScheme->GetFontEx( "Default", proportional, hdProportional );
			SetFont( font );
		}
	}
	int left, top, right, bottom;
	ComputeAlignment( left, top, right, bottom );
	width = static_cast<int>( static_cast<unsigned int>( right ) + static_cast<unsigned int>( _textInset[0] )
							  - static_cast<unsigned int>( left ) );
	int imageWidth, imageHeight;
	_textImage->GetSize( imageWidth, imageHeight );
	width = static_cast<int>( static_cast<unsigned int>( width ) - static_cast<unsigned int>( imageWidth ) );
	_textImage->GetContentSize( imageWidth, imageHeight );
	width = static_cast<int>( static_cast<unsigned int>( width ) + static_cast<unsigned int>( imageWidth ) );
	for ( int index = 0; index < _imageDar.Count(); ++index )
	{
		int offset = _imageDar[index].offset;
		width = static_cast<int>( static_cast<unsigned int>( width ) + static_cast<unsigned int>( offset ) );
	}
	height = static_cast<int>( static_cast<unsigned int>( bottom ) + static_cast<unsigned int>( _textInset[1] )
							   - static_cast<unsigned int>( top ) );
}

//-----------------------------------------------------------------------------
// Purpose: Set whether the text is displayed bright or dull
//-----------------------------------------------------------------------------
void Label::SetTextColorState( EColorState state )
{
	if ( _textColorState != state )
	{
		_textColorState = state;
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set the enabled state
//-----------------------------------------------------------------------------
void Label::SetEnabled( bool state )
{
	Panel::SetEnabled( state );
}

void Label::SetFgColor( Color color )
{
	Color currentColor = GetFgColor();
	if ( currentColor.r() != color.r() || currentColor.g() != color.g() || currentColor.b() != color.b()
		 || currentColor.a() != color.a() )
	{
		Panel::SetFgColor( color );
		_textImage->SetColor( color );
		Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the foreground color of the Label
//-----------------------------------------------------------------------------
Color Label::GetFgColor()
{
	return Panel::GetFgColor();
}

void Label::ComputeAlignment( int& x0, int& y0, int& x1, int& y1 )
{
	int paintWidth;
	int paintHeight;
	GetPaintSize( paintWidth, paintHeight );
	x0 = 0;
	y0 = 0;
	int totalWidth = 0;
	int maxHeight = 0;
	Alignment horizontalAlignment = _contentAlignment;
	for ( int index = 0; index < _imageDar.Count(); ++index )
	{
		TImageInfo& info = _imageDar[index];
		IImage* image = info.image;
		if ( image )
		{
			int imageWidth;
			int imageHeight;
			image->GetSize( imageWidth, imageHeight );
#ifndef BSHIFT_CLIENT
			if ( m_bProportionalScaleImages && image != _textImage )
			{
				imageWidth = Panel::GetProportionalScaledValue( imageWidth );
				imageHeight = Panel::GetProportionalScaledValue( imageHeight );
			}
#endif
			if ( imageWidth > paintWidth )
				horizontalAlignment = a_west;
			if ( imageHeight > maxHeight )
				maxHeight = imageHeight;
			totalWidth =
				static_cast<int>( static_cast<std::uint32_t>( imageWidth ) + static_cast<std::uint32_t>( info.offset )
								  + static_cast<std::uint32_t>( totalWidth ) );
		}
	}

	switch ( horizontalAlignment )
	{
	case a_northwest:
	case a_west:
	case a_southwest:
		x0 = 0;
		break;
	case a_north:
	case a_center:
	case a_south:
		x0 =
			static_cast<int>( static_cast<std::uint32_t>( paintWidth ) - static_cast<std::uint32_t>( totalWidth ) ) / 2;
		break;
	case a_northeast:
	case a_east:
	case a_southeast:
		x0 = static_cast<int>( static_cast<std::uint32_t>( paintWidth ) - static_cast<std::uint32_t>( totalWidth ) );
		break;
	default:
		break;
	}
	// y align text
	switch ( _contentAlignment )
	{
	case a_northwest:
	case a_north:
	case a_northeast:
		y0 = 0;
		break;
	case a_west:
	case a_center:
	case a_east:
		y0 =
			static_cast<int>( static_cast<std::uint32_t>( paintHeight ) - static_cast<std::uint32_t>( maxHeight ) ) / 2;
		break;
	case a_southwest:
	case a_south:
	case a_southeast:
		y0 = static_cast<int>( static_cast<std::uint32_t>( paintHeight ) - static_cast<std::uint32_t>( maxHeight ) );
		break;
	default:
		break;
	}
	x1 = static_cast<int>( static_cast<std::uint32_t>( totalWidth ) + static_cast<std::uint32_t>( x0 ) );
	y1 = static_cast<int>( static_cast<std::uint32_t>( maxHeight ) + static_cast<std::uint32_t>( y0 ) );
}

void Label::PerformLayout()
{
	int wide, tall;
	Panel::GetSize( wide, tall );
	wide = static_cast<int>( static_cast<unsigned int>( wide ) - static_cast<unsigned int>( _textInset[0] ) );
	int imageCount = _imageDar.Count();
	if ( imageCount == 1 && _textImageIndex == 0 )
	{
		int textWidth, textHeight;
		if ( m_bWrap )
		{
			_textImage->GetContentSize( textWidth, textHeight );
			_textImage->SetSize( wide, textHeight );
		}
		else
		{
			_textImage->GetContentSize( textWidth, textHeight );
			_textImage->SetSize( wide >= textWidth ? textWidth : wide, textHeight );
		}
		return;
	}
	// assume the images in the dar cannot be resized, and if
	// the images + the textimage are too wide we shring the textimage part
	if ( _textImageIndex < 0 )
		return;

	// get the size of the images
	int usedWidth = 0;
	if ( imageCount > 0 )
	{
		int index = 0;
		do
		{
			IImage* image = _imageDar[index].image;
			if ( image && index != _textImageIndex )
			{
				int imageWidth, imageHeight;
				image->GetSize( imageWidth, imageHeight );
#ifndef BSHIFT_CLIENT
				if ( m_bProportionalScaleImages )
					imageWidth = GetProportionalScaledValue( imageWidth );
#endif
				usedWidth = static_cast<int>(
					static_cast<unsigned int>( usedWidth ) + static_cast<unsigned int>( imageWidth ) );
			}
			++index;
		} while ( index < _imageDar.Count() );
	}
	int textWidth = static_cast<int>( static_cast<unsigned int>( wide ) - static_cast<unsigned int>( usedWidth ) );
	if ( textWidth >= 0 )
	{
		int imageWidth, imageHeight;
		_textImage->GetSize( imageWidth, imageHeight );
		_textImage->SetSize( textWidth, imageHeight );
	}
}

void Label::EnsureImageCapacity( int count )
{
	while ( _imageDar.Count() <= count )
		AddImage( NULL, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Multiple image handling
//			Images are drawn from left to right across the label, ordered by index
//			By default there is a TextImage in position 0
//			Set the contents of an IImage in the IImage array.
//-----------------------------------------------------------------------------
void Label::SetImageAtIndex( int index, IImage* image, int offset )
{
	EnsureImageCapacity( index );
//	Assert( image );
	if ( _imageDar.Element( index ).image != image || _imageDar.Element( index ).offset != offset )
	{
		_imageDar.Element( index ).image = image;
		_imageDar.Element( index ).offset = static_cast<short>( offset );
		InvalidateLayout( false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add an image to the list
//			returns the index the image was placed in
//-----------------------------------------------------------------------------
int Label::AddImage( IImage* image, int offset )
{
	int index = _imageDar.AddToTail();
	_imageDar.Element( index ).image = image;
	_imageDar.Element( index ).offset = static_cast<short>( offset );
	_imageDar.Element( index ).xpos = -1;
	_imageDar.Element( index ).width = -1;
	InvalidateLayout( false, false );
	return index;
}

int Label::SetTextImageIndex( int index )
{
	if ( index == _textImageIndex )
		return _textImageIndex;

	EnsureImageCapacity( index );
	int oldIndex = _textImageIndex;
	_imageDar[oldIndex].image = NULL;
	if ( index >= 0 )
		_imageDar[index].image = _textImage;
	_textImageIndex = index;
	return oldIndex;
}

//-----------------------------------------------------------------------------
// Purpose: Size the width of the label to its contents - only works from in ApplySchemeSettings or PerformLayout()
//-----------------------------------------------------------------------------
void Label::SizeToContents()
{
	int width;
	int height;
	GetContentSize( width, height );
	Panel::SetSize( width, height );
}

void Label::Init()
{
	_textColorState = CS_NORMAL;
	_textInset[0] = 0;
	_textInset[1] = 0;
	_hotkey = 0;
	_associate = NULL;
	_contentAlignment = a_west;
	_associateName = NULL;
	_fontOverrideName = NULL;
	m_bWrap = false;
#ifndef BSHIFT_CLIENT
	m_bProportionalScaleImages = true;
#endif
}

#ifdef BSHIFT_CLIENT
void Label::SetProportionalScaleImages( bool )
{
}
#else
void Label::SetProportionalScaleImages( bool bScale )
{
	m_bProportionalScaleImages = bScale;
}
#endif

void Label::SetWrap( bool bWrap )
{
	m_bWrap = bWrap;
	_textImage->SetWrap( bWrap );
}

Label::Label( Panel* parent, const char* name, const char* text ) : Panel( parent, name )
{
	Init();
	_textImage = new TextImage( text );
	_textImage->SetColor( Color( 0, 0, 0, 0 ) );
	const char* displayText = text ? text : "";
	_textImage->SetText( displayText );
	if ( displayText[0] == '#' )
		SetHotkey( CalculateHotkey( localize()->Find( displayText ) ) );
	else
		SetHotkey( CalculateHotkey( displayText ) );
	InvalidateLayout( false, false );
	Repaint();
	_textImageIndex = AddImage( _textImage, 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
Label::Label( Panel* parent, const char* name, const wchar_t* text ) : Panel( parent, name )
{
	Init();
	_textImage = new TextImage( text );
	_textImage->SetColor( Color( 0, 0, 0, 0 ) );
	_textImage->SetText( text );
	wchar_t hotkey = CalculateHotkey( text );
	SetHotkey( hotkey );
	InvalidateLayout( false, false );
	Repaint();
	_textImageIndex = Label::AddImage( _textImage, 0 );
}

Label::~Label()
{
	delete _textImage;
	if ( _associateName != NULL )
	{
		delete[] _associateName;
		_associateName = NULL;
	}
	if ( _fontOverrideName != NULL )
	{
		delete[] _fontOverrideName;
		_fontOverrideName = NULL;
	}
}

void Label::OnSetText( KeyValues* params )
{
	KeyValues* text = params->FindKey( "text", false );
	if ( !text )
		return;
	if ( text->GetDataType( NULL ) == KeyValues::TYPE_STRING )
		SetText( text->GetString( NULL, "" ) );
	else if ( text->GetDataType( NULL ) == KeyValues::TYPE_WSTRING )
		SetText( text->GetWString( NULL, L"" ) );
}

void Label::OnDialogVariablesChanged( KeyValues* dialogVariables )
{
	constexpr int TEXT_BUFFER_CHARACTERS = 1024;
	constexpr int TEXT_BUFFER_BYTES = 2048;
	constexpr StringIndex_t INVALID_LOCALIZATION_SYMBOL = static_cast<StringIndex_t>( -1 );
	StringIndex_t symbol = _textImage->GetUnlocalizedTextSymbol();
	if ( symbol == INVALID_LOCALIZATION_SYMBOL )
		return;
	wchar_t text[TEXT_BUFFER_CHARACTERS];
	localize()->ConstructString( text, TEXT_BUFFER_BYTES, symbol, dialogVariables );
	SetText( text );
}

//-----------------------------------------------------------------------------
// Purpose: sets custom settings from the scheme file
//-----------------------------------------------------------------------------
void Label::ApplySchemeSettings( IScheme* scheme )
{
	Panel::ApplySchemeSettings( scheme );
	if ( _fontOverrideName != NULL )
	{
		bool hdProportional = IsHDProportional();
		bool proportional = IsProportional();
		HFont font = scheme->GetFontEx( _fontOverrideName, proportional, hdProportional );
		SetFont( font );
	}
	if ( GetFont() == 0 )
	{
		bool hdProportional = IsHDProportional();
		bool proportional = IsProportional();
		HFont font = scheme->GetFontEx( "Default", proportional, hdProportional );
		SetFont( font );
	}

	int wide;
	int tall;
	if ( m_bWrap )
	{
		GetSize( wide, tall );
		wide = static_cast<int>( static_cast<std::uint32_t>( wide ) - static_cast<std::uint32_t>( _textInset[0] ) );
		_textImage->SetSize( wide, tall );
		_textImage->RecalculateNewLinePositions();
	}
	else
	{
		_textImage->GetContentSize( wide, tall );
		_textImage->SetSize( wide, tall );
	}

	// clear out any the images, since they will have been invalidated
	for ( int index = 0; index < _imageDar.Count(); ++index )
	{
		if ( _imageDar[index].image != NULL && index != _textImageIndex )
			_imageDar[index].image = NULL;
	}

	SetDisabledFgColor1( GetSchemeColor( "DisabledFgColor1", scheme ) );
	SetDisabledFgColor2( GetSchemeColor( "DisabledFgColor2", scheme ) );
	SetBgColor( GetSchemeColor( "LabelBgColor", scheme ) );
	const char* textColor;
	switch ( _textColorState )
	{
	case CS_DULL:
		textColor = "LabelDimText";
		break;
	case CS_BRIGHT:
		textColor = "BrightControlText";
		break;
	case CS_LIGHT:
		textColor = "BrightBaseText";
		break;
	default:
		textColor = "BaseText";
		break;
	}
	SetFgColor( GetSchemeColor( textColor, scheme ) );
	_associateColor = GetSchemeColor( "BrightControlText", scheme );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a description of the label string
//-----------------------------------------------------------------------------
const char* Label::GetDescription()
{
	static char description[1024];
	const char* parentDescription = Panel::GetDescription();
	_snprintf( description, sizeof( description ),
		"%s, string labelText, string associate, alignment textAlignment, int wrap, int dulltext, int brighttext, string font",
		parentDescription );
	return description;
}

//-----------------------------------------------------------------------------
// Purpose: Resond to resizing of the panel
//-----------------------------------------------------------------------------
void Label::OnSizeChanged( int width, int height )
{
	InvalidateLayout( false, false );
	Panel::OnSizeChanged( width, height );
}

void Label::OnRequestFocus( VPANEL subFocus, VPANEL defaultPanel )
{
	if ( _associate != NULL && subFocus == GetVPanel() && !Panel::IsBuildModeActive() )
		_associate->RequestFocus( 0 );
	else
		Panel::OnRequestFocus( subFocus, defaultPanel );
}

static int LabelPaintAdd( int value, int offset )
{
	return static_cast<int>( static_cast<std::uint32_t>( value ) + static_cast<std::uint32_t>( offset ) );
}

static int LabelPaintSubtract( int value, int offset )
{
	return static_cast<int>( static_cast<std::uint32_t>( value ) - static_cast<std::uint32_t>( offset ) );
}

void Label::Paint()
{
#ifdef BSHIFT_CLIENT
	int left, top, right, bottom;
	ComputeAlignment( left, top, right, bottom );
	if ( _associateName )
	{
		SetAssociatedControl( FindSiblingByName( _associateName ) );
		delete[] _associateName;
		_associateName = NULL;
	}

	int labelWidth, labelHeight;
	Panel::GetSize( labelWidth, labelHeight );
	int x = left;
	int imageTop = LabelPaintAdd( top, _textInset[1] );
	// draw the set of images
	for ( int index = 0; index < _imageDar.Count(); ++index )
	{
		TImageInfo& info = _imageDar[index];
		IImage* image = info.image;
		if ( !image )
			continue; // skip over null images

		int imageX = LabelPaintAdd( x, info.offset );
		if ( index == _textImageIndex ) // if this is the text image then add its inset
			imageX = LabelPaintAdd( imageX, _textInset[0] );
		if ( info.xpos >= 0 )
			imageX = info.xpos;
		int imageY = imageTop;
		image->SetPos( imageX, imageY );
		if ( _contentAlignment == a_west || _contentAlignment == a_center || _contentAlignment == a_east )
		{
			int imageWidth, imageHeight;
			image->GetSize( imageWidth, imageHeight );
			if ( imageHeight < LabelPaintSubtract( bottom, top ) )
			{
				int unusedHeight = LabelPaintSubtract( LabelPaintSubtract( bottom, top ), imageHeight );
				imageY = LabelPaintAdd( imageTop, unusedHeight / 2 );
				image->SetPos( imageX, imageY );
			}
		}
		// don't resize the image unless its too big
		if ( info.width >= 0 )
		{
			int imageWidth, imageHeight;
			image->GetSize( imageWidth, imageHeight );
			if ( imageWidth > info.width )
				image->SetSize( info.width, imageHeight );
		}

		// if it's the basic text image then draw specially
		if ( image == _textImage )
		{
			if ( IsEnabled() )
			{
				bool associateFocused = false;
				if ( _associate )
				{
					IPanel* panels = ipanel();
					VPANEL associate = _associate->GetVPanel();
					VPANEL focus = input()->GetFocus();
					associateFocused = panels->HasParent( focus, associate );
				}
				if ( associateFocused )
					_textImage->SetColor( _associateColor );
				else
					_textImage->SetColor( GetFgColor() );
			}
			else
			{
				_textImage->SetPos( LabelPaintAdd( imageX, 1 ), LabelPaintAdd( imageY, 1 ) );
				_textImage->SetColor( _disabledFgColor1 );
				_textImage->Paint();
				surface()->DrawFlushText();
				_textImage->SetPos( imageX, imageY );
				_textImage->SetColor( _disabledFgColor2 );
			}
			_textImage->Paint();
		}
		else
		{
			image->Paint();
		}

		int imageWidth, imageHeight;
		image->GetSize( imageWidth, imageHeight );
		x = LabelPaintAdd( imageWidth, imageX );
	}
#else
	int left, top, right, bottom;
	ComputeAlignment( left, top, right, bottom );
	if ( _associateName )
	{
		Panel* associate = FindSiblingByName( _associateName );
		SetAssociatedControl( associate );
		delete[] _associateName;
		_associateName = NULL;
	}
	int labelWidth, labelHeight;
	Panel::GetSize( labelWidth, labelHeight );
	int x = left;
	int imageTop = LabelPaintAdd( top, _textInset[1] );
	for ( int index = 0; index < _imageDar.Count(); ++index )
	{
		TImageInfo& info = _imageDar[index];
		IImage* image = info.image;
		if ( !image )
			continue;

		int originalWidth, originalHeight;
		image->GetSize( originalWidth, originalHeight );
		int imageWidth, imageHeight;
		if ( image == _textImage || !m_bProportionalScaleImages )
		{
			imageWidth = originalWidth;
			imageHeight = originalHeight;
		}
		else
		{
			imageWidth = GetProportionalScaledValue( originalWidth );
			imageHeight = GetProportionalScaledValue( originalHeight );
			image->SetSize( imageWidth, imageHeight );
		}
		int imageX = LabelPaintAdd( x, info.offset );
		if ( index == _textImageIndex )
			imageX = LabelPaintAdd( imageX, _textInset[0] );
		if ( info.xpos >= 0 )
			imageX = info.xpos;
		int imageY = imageTop;
		image->SetPos( imageX, imageY );
		if ( ( _contentAlignment == a_west || _contentAlignment == a_center || _contentAlignment == a_east )
			 && imageHeight < LabelPaintSubtract( bottom, top ) )
		{
			int unusedHeight = LabelPaintSubtract( LabelPaintSubtract( bottom, imageHeight ), top );
			imageY = LabelPaintAdd( imageTop, unusedHeight / 2 );
			image->SetPos( imageX, imageY );
		}
		if ( info.width >= 0 && imageWidth > info.width )
			image->SetSize( info.width, imageHeight );

		if ( image == _textImage )
		{
			if ( IsEnabled() )
			{
				bool associateFocused = false;
				if ( _associate )
				{
					IInput* inputInterface = input();
					IPanel* panels = ipanel();
					VPANEL associate = _associate->GetVPanel();
					VPANEL focus = inputInterface->GetFocus();
					associateFocused = panels->HasParent( focus, associate );
				}
				if ( associateFocused )
					_textImage->SetColor( _associateColor );
				else
				{
					Color color = GetFgColor();
					_textImage->SetColor( color );
				}
				_textImage->Paint();
			}
			else
			{
				int yOffset = GetProportionalScaledValue( 1 );
				int xOffset = GetProportionalScaledValue( 1 );
				_textImage->SetPos( LabelPaintAdd( imageX, xOffset ), LabelPaintAdd( imageY, yOffset ) );
				_textImage->SetColor( _disabledFgColor1 );
				_textImage->Paint();
				surface()->DrawFlushText();
				_textImage->SetPos( imageX, imageY );
				_textImage->SetColor( _disabledFgColor2 );
				_textImage->Paint();
			}
		}
		else
		{
			image->Paint();
		}
		x = LabelPaintAdd( imageWidth, imageX );
		image->SetSize( originalWidth, originalHeight );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Label::ApplySettings( KeyValues* settings )
{
	Panel::ApplySettings( settings );
	// label settings
	const char* text = settings->GetString( "labelText", NULL );
	if ( text )
	{
		if ( text[0] == '%' && text[strlen( text ) - 1] == '%' )
		{
			// it's a variable, set it to be a special variable localized string
			wchar_t unicodeVariable[256];
			localize()->ConvertANSIToUnicode( text, unicodeVariable, sizeof( unicodeVariable ) );
			char token[256];
			_snprintf( token, sizeof( token ), "#var_%s", text );
			localize()->AddString( token + 1, unicodeVariable, "" );
			SetText( token );
		}
		else
		{
			SetText( text );
		}
	}

	// text alignment
	const char* alignment = settings->GetString( "textAlignment", "" );
	if ( _stricmp( alignment, "north-west" ) == 0 )
		SetContentAlignment( a_northwest );
	else if ( _stricmp( alignment, "north" ) == 0 )
		SetContentAlignment( a_north );
	else if ( _stricmp( alignment, "north-east" ) == 0 )
		SetContentAlignment( a_northeast );
	else if ( _stricmp( alignment, "west" ) == 0 )
		SetContentAlignment( a_west );
	else if ( _stricmp( alignment, "center" ) == 0 )
		SetContentAlignment( a_center );
	else if ( _stricmp( alignment, "east" ) == 0 )
		SetContentAlignment( a_east );
	else if ( _stricmp( alignment, "south-west" ) == 0 )
		SetContentAlignment( a_southwest );
	else if ( _stricmp( alignment, "south" ) == 0 )
		SetContentAlignment( a_south );
	else if ( _stricmp( alignment, "south-east" ) == 0 )
		SetContentAlignment( a_southeast );

	// the control we are to be associated with may not have been created yet,
	// so keep a pointer to it's name and calculate it when we can
	const char* associate = settings->GetString( "associate", "" );
	if ( associate[0] )
	{
		_associateName = new char[strlen( associate ) + 1];
		strcpy( _associateName, associate );
	}
	if ( settings->GetInt( "dulltext", 0 ) == 1 )
		SetTextColorState( CS_DULL );
	else if ( settings->GetInt( "brighttext", 0 ) == 1 )
		SetTextColorState( CS_BRIGHT );
	else
		SetTextColorState( CS_NORMAL );

	// font settings
	const char* fontName = settings->GetString( "font", "" );
	ISchemeManager* schemes = scheme();
	HScheme schemeHandle = GetScheme();
	IScheme* fonts = schemes->GetIScheme( schemeHandle );
	if ( fontName[0] )
	{
		delete[] _fontOverrideName;
		_fontOverrideName = new char[strlen( fontName ) + 1];
		strcpy( _fontOverrideName, fontName );
		bool hdProportional = IsHDProportional();
		bool proportional = IsProportional();
		SetFont( fonts->GetFontEx( _fontOverrideName, proportional, hdProportional ) );
	}
	else if ( _fontOverrideName )
	{
		delete[] _fontOverrideName;
		_fontOverrideName = NULL;
		bool hdProportional = IsHDProportional();
		bool proportional = IsProportional();
		SetFont( fonts->GetFontEx( "Default", proportional, hdProportional ) );
	}
	m_bWrap = settings->GetInt( "wrap", 0 ) > 0;
	_textImage->SetWrap( m_bWrap );
	InvalidateLayout( true, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Label::GetSettings( KeyValues* settings )
{
	// panel settings
	Panel::GetSettings( settings );
	// label settings
	char buffer[256];
	_textImage->GetUnlocalizedText( buffer, 255 );
	const char* text = _strnicmp( buffer, "#var_", 5 ) == 0 ? buffer + 5 : buffer;
	settings->SetString( "labelText", text );
	const char* alignment;
	switch ( _contentAlignment )
	{
	case a_northwest:
		alignment = "north-west";
		break;
	case a_north:
		alignment = "north";
		break;
	case a_northeast:
		alignment = "north-east";
		break;
	case a_center:
		alignment = "center";
		break;
	case a_east:
		alignment = "east";
		break;
	case a_southwest:
		alignment = "south-west";
		break;
	case a_south:
		alignment = "south";
		break;
	case a_southeast:
		alignment = "south-east";
		break;
	default:
		alignment = "west";
		break;
	}
	settings->SetString( "textAlignment", alignment );
	if ( _associate )
		settings->SetString( "associate", _associate->GetName() );
	settings->SetInt( "dulltext", _textColorState == CS_DULL );
	settings->SetInt( "brighttext", _textColorState == CS_BRIGHT );
	if ( _fontOverrideName )
		settings->SetString( "font", _fontOverrideName );
	settings->SetInt( "wrap", m_bWrap );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool Label::RequestInfo( KeyValues* output )
{
	if ( _stricmp( output->GetName(), "GetText" ) != 0 )
		return Panel::RequestInfo( output );

	wchar_t text[256];
	_textImage->GetText( text, 255 );
	output->SetWString( "text", text );
	return true;
}

}
