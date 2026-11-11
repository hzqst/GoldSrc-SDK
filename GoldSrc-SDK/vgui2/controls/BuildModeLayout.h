#pragma once

#include "Label.h"
#include "PanelListPanel.h"
#include "ScrollBar.h"
#include "vgui/IScheme.h"
#include "vgui/ISurface.h"

#include <algorithm>

namespace vgui2
{
static int EditorControlHeight( Panel* panel )
{
	panel->MakeReadyForUse();
	HFont font = scheme()->GetIScheme( panel->GetScheme() )->GetFontEx(
		"Default", panel->IsProportional(), panel->IsHDProportional() );
	if ( Label* label = dynamic_cast<Label*>( panel ) )
		font = label->GetFont();
	return std::max( panel->GetProportionalScaledValue( 24 ),
		surface()->GetFontTall( font ) + panel->GetProportionalScaledValue( 6 ) );
}

class BuildModePropertyList : public PanelListPanel
{
public:
	BuildModePropertyList( Panel* parent ) : PanelListPanel( parent, "BuildModeControls" ) {}

	int computeVPixelsNeeded() override
	{
		int gap = GetProportionalScaledValue( 5 );
		int height = gap;
		for ( int i = 0; i < GetItemCount(); ++i )
		{
			Panel* label = GetItemLabel( i );
			Panel* editor = GetItemPanel( i );
			if ( label && editor )
				height += std::max( EditorControlHeight( label ), EditorControlHeight( editor ) ) + gap;
		}
		return height;
	}

	void PerformLayout() override
	{
		ScrollBar* scrollbar = dynamic_cast<ScrollBar*>( FindChildByName( "PanelListPanelVScroll", false ) );
		Panel* embedded = FindChildByName( "PanelListEmbedded", false );
		if ( !scrollbar || !embedded )
			return;
		int left, top, right, bottom;
		GetInset( left, top, right, bottom );
		int width = std::max( 0, GetWide() - left - right );
		int height = std::max( 1, GetTall() - top - bottom );
		int contentHeight = computeVPixelsNeeded();
		bool scrolling = contentHeight > height;
		int scrollbarWidth = scrolling ? GetProportionalScaledValue( 18 ) : 0;
		scrollbar->SetVisible( scrolling );
		scrollbar->SetEnabled( scrolling );
		scrollbar->SetRange( 0, contentHeight );
		scrollbar->SetRangeWindow( height );
		scrollbar->SetButtonPressedScrollValue( GetProportionalScaledValue( 24 ) );
		scrollbar->SetValue( scrolling ? scrollbar->GetValue() : 0 );
		scrollbar->SetBounds( left + width - scrollbarWidth, top, scrollbarWidth, height );
		int contentWidth = std::max( 0, width - scrollbarWidth );
		embedded->SetBounds( left, top - scrollbar->GetValue(), contentWidth, contentHeight );
		int gap = GetProportionalScaledValue( 5 );
		int labelWidth = std::min( GetProportionalScaledValue( 96 ), contentWidth / 2 );
		int editorX = std::min( contentWidth, labelWidth + gap );
		int y = gap;
		for ( int i = 0; i < GetItemCount(); ++i )
		{
			Panel* label = GetItemLabel( i );
			Panel* editor = GetItemPanel( i );
			if ( !label || !editor )
				continue;
			int rowHeight = std::max( EditorControlHeight( label ), EditorControlHeight( editor ) );
			label->SetBounds( 0, y, labelWidth, rowHeight );
			editor->SetBounds( editorX, y, contentWidth - editorX, rowHeight );
			y += rowHeight + gap;
		}
	}
};
}
