//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "OptionsSubAudio.h"
#include "CvarSlider.h"
#include "LabeledCommandComboBox.h"
#include "GameUI_Interface.h"
#include "ModInfo.h"

#include "vgui/KeyValues.h"
#include "wrect.h"
#include "cl_dll.h"

COptionsSubAudio::COptionsSubAudio( vgui2::Panel* parent ) : vgui2::PropertyPage( parent, NULL, true )
{
	m_pSFXSlider = new CCvarSlider( this, "SFX Slider", "#GameUI_SoundEffectVolume", 0.0f, 2.0f, "volume", false );
	m_pHEVSlider = new CCvarSlider( this, "Suit Slider", "#GameUI_HEVSuitVolume", 0.0f, 2.0f, "suitvolume", false );
	m_pMP3Slider = new CCvarSlider( this, "MP3 Volume", "#GameUI_MP3Volume", 0.0f, 1.0f, "mp3volume", false );
	m_pSoundQualityCombo = new CLabeledCommandComboBox( this, "Sound Quality" );
	//!! bug no command given
	m_pSoundQualityCombo->AddItem( "#GameUI_High", "hisound 1" );
	m_pSoundQualityCombo->AddItem( "#GameUI_Low", "hisound 0" );
	m_pSoundQualityCombo->SetInitialItem( engine->pfnGetCvarFloat( "hisound" ) == 0.0f ? 1 : 0 );
	LoadControlSettings( "Resource\\OptionsSubAudio.res", NULL );

	// override, hide the HEV suit volume when not in half-life
	if ( ModInfo()->IsMultiplayerOnly() )
	{
		vgui2::Panel* label = FindChildByName( "suit label", false );
		if ( label )
			label->SetVisible( false );
		vgui2::Panel* slider = FindChildByName( "Suit Slider", false );
		if ( slider )
			slider->SetVisible( false );
	}
}

COptionsSubAudio::~COptionsSubAudio()
{
}

void COptionsSubAudio::OnResetData()
{
	m_pSFXSlider->Reset();
	m_pHEVSlider->Reset();
	m_pMP3Slider->Reset();
	m_pSoundQualityCombo->Reset();
}

void COptionsSubAudio::OnApplyChanges()
{
	m_pSFXSlider->ApplyChanges();
	m_pHEVSlider->ApplyChanges();
	m_pMP3Slider->ApplyChanges();
	m_pSoundQualityCombo->ApplyChanges();
}

void COptionsSubAudio::OnControlModified()
{
	PostActionSignal( new KeyValues( "ApplyButtonEnable" ) );
}
