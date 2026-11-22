#pragma once
#if USE_HLCLI

// halflife-cli (HLND2T-DiligentGraphics, -hlcli) reads controls through RequestInfo with "VGUI2.*" requests.
// The standard headers come first: basetypes.h, through KeyValues.h, defines a clamp macro that breaks std::clamp.
#include <algorithm>
#include <string>
#include <vector>
#include <string.h>
#include <wchar.h>
#include "Panel.h"
#include "controls.h"
#include "../../engine/controls.h"
#include "vgui/IVGui.h"
#include "vgui/KeyValues.h"
#include "VGUI2TextPage.h"

// Module-local helpers. No new virtual slots or cross-module Panel layout access.
namespace vgui2automation
{
constexpr int kTextPage = 4096;
constexpr int kLabelCapacity = 32768; // TextImage stores its length in a short.
constexpr int kItemTextLimit = 128;

inline bool IsRequest( KeyValues* data, const char* name )
{
	return !strcmp( data->GetName(), name );
}

inline bool Begin( vgui2::Panel* panel, KeyValues* data )
{
	const vgui2::HPanel handle = vgui2::ivgui()->PanelToHandle( panel->GetVPanel() );
	if ( data->GetInt( "target", -1 ) != static_cast<int>( handle ) )
		return false;
	data->SetInt( "vgui2_version", 1 );
	data->SetInt( "vgui2_target", static_cast<int>( handle ) );
	return true;
}

// Offsets/counts are Unicode scalar values, including on UTF-16 platforms.
inline void Text( KeyValues* data, const wchar_t* text, size_t length )
{
	TextPage page = SliceText( text, length, data->GetInt( "text_offset", 0 ),
		( std::min )( kTextPage, data->GetInt( "text_limit", 256 ) ) );
	data->SetWString( "text", page.text.c_str() );
	data->SetInt( "text_length", page.length );
	data->SetInt( "text_next", page.next );
}

template <class T> inline void LabelText( T* label, KeyValues* data )
{
	std::vector<wchar_t> text( kLabelCapacity, 0 );
	label->GetText( text.data(), static_cast<int>( text.size() ) ); // existing virtual
	const bool truncated = text.back() != 0;
	text.back() = 0;
	Text( data, text.data(), wcslen( text.data() ) );
	data->SetInt( "text_source_truncated", truncated );
}

inline void Item( KeyValues* data, int id, const wchar_t* text )
{
	KeyValues* item = new KeyValues( "item" );
	item->SetInt( "id", id );
	std::wstring summary( text ? text : L"" );
	item->SetInt( "text_truncated", summary.size() > kItemTextLimit );
	if ( summary.size() > kItemTextLimit )
		summary.resize( kItemTextLimit );
	if ( !summary.empty() && summary.back() >= 0xd800 && summary.back() <= 0xdbff )
		summary.pop_back();
	item->SetWString( "text", summary.c_str() );
	data->FindKey( "items", true )->AddSubKey( item );
}

inline std::wstring DisplayValue( KeyValues* data, const char* key )
{
	if ( !data )
		return L"";
	if ( data->GetDataType( key ) == KeyValues::TYPE_WSTRING )
		return data->GetWString( key );
	const char* value = data->GetString( key, "" );
	if ( value[0] == '#' )
	{
		if ( const wchar_t* localized = vgui2::localize()->Find( value ) )
			return localized;
	}
	std::vector<wchar_t> converted( strlen( value ) + 1, 0 );
	vgui2::localize()->ConvertANSIToUnicode( value, converted.data(),
		static_cast<int>( converted.size() * sizeof( wchar_t ) ) );
	return converted.data();
}
} // namespace vgui2automation
#endif
