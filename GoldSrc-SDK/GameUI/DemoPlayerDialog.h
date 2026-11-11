//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
//
// Purpose: DemoPlayerDialog.h: interface for the CDemoPlayerDialog class.
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "Frame.h"
#include "BaseSystemModule.h"
#include "PHandle.h"

class IBaseSystem;
class IDemoPlayer;
class IEngineWrapper;
class IWorld;

namespace vgui2
{
class Button;
class Label;
class Slider;
class ToggleButton;
}

namespace vgui = vgui2;

class CDemoPlayerDialog : public vgui::Frame, public BaseSystemModule
{
	DECLARE_CLASS_SIMPLE( CDemoPlayerDialog, vgui::Frame );

public:
	CDemoPlayerDialog( vgui2::Panel* parent );
	~CDemoPlayerDialog() override;
	MESSAGE_FUNC_CHARPTR( OnLoadDemo, "DemoSelected", demoname );

protected:
	void OnThink() override;
	void OnClose() override;
	void OnCommand( const char* command ) override;
	void ReceiveSignal( ISystemModule* module, unsigned int signal, void* data ) override;
	void Update();	// updates all visible data fields
	bool LoadModules();
	// main panel button events
	void OnPause();	// pause playback
	void OnPlay();	// rsume playback
	void OnStart();	// go to start
	void OnEnd();	// go to end
	void OnSlower();	// faster playback speed
	void OnFaster();	// slower playback speed
	void OnLoad();	// load new demo dialog
	void OnStop();	// stop demo playback completely
	void OnNextFrame( int direction ); // next/last frame
	void OnEvents(); // open demo events editor
	void OnSave();	// save demo file again
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;

	vgui2::Label*			m_pLableTimeCode;
	vgui2::Slider*			m_pTimeSlider;
	vgui2::ToggleButton*	m_MasterButton;
	vgui2::Button*			m_pButtonPlay;
	vgui2::Button*			m_pButtonStepF; // |>
	vgui2::Button*			m_pButtonFaster; // >>
	vgui2::Button*			m_pButtonSlower; // <<
	vgui2::Button*			m_pButtonStepB; // <|
	vgui2::Button*			m_pButtonPause; // ||
	vgui2::Button*			m_pButtonStart; // |<
	vgui2::Button*			m_pButtonEnd; // >|
	vgui2::Button*			m_pButtonLoad; // ^
	vgui2::Button*			m_pButtonStop; // x

	vgui2::DHANDLE<vgui2::Frame>	m_hDemoPlayerFileDialog;
	vgui2::DHANDLE<vgui2::Frame>	m_hDemoEventsDialog;

	IEngineWrapper*		m_Engine;
	IDemoPlayer*		m_DemoPlayer;
	IBaseSystem*		m_System;
	IWorld*				m_World;
	float				m_NextTimeScale;
	int					m_lastSliderTime;
};
