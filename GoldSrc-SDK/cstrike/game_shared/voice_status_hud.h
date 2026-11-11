//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VOICE_STATUS_HUD_H
#define VOICE_STATUS_HUD_H

#include "hud.h"
#include "voice_status.h"
#include "cl_entity.h"
#include "../../engine/utlvector.h"
#include "../../vgui2/controls/Label.h"
#include "../../vgui2/controls/ImagePanel.h"


//-----------------------------------------------------------------------------
// Purpose: a label which displays a name and a speaking icon
//-----------------------------------------------------------------------------
class CVoiceLabel
{
public:
	CVoiceLabel();
	~CVoiceLabel();
	void SetFgColor( Color color ) { m_pLabel->SetFgColor( color ); }
	void SetBgColor( Color color ) { m_pLabel->SetBgColor( color ); }
	void SetVisible( bool visible ) { m_pLabel->SetVisible( visible ); }
	bool GetVisible() { return m_pLabel->IsVisible(); }
	void GetContentSize( int& width, int& height );
	void SetBounds( int x, int y );
	void SetClientIndex( int index ) { m_clientindex = index; }
	int GetClientIndex() { return m_clientindex; }
	void SetLocation( const char* location );
	void SetPlayerName( const char* name );

private:

	//-----------------------------------------------------------------------------
	// Purpose: inner class that overrides ApplySchemeSettings() for the label so an image can be loaded, also saves colors away
	//   so ApplySchemeSettings() doesn't override them
	//-----------------------------------------------------------------------------
	class VoiceVGUILabel : public vgui2::Label
	{
	public:
		VoiceVGUILabel( vgui2::Panel* parent, const char* name, const char* text )
			: Label( parent, name, text )
		{
		}
	private:
		void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	};
	void RebuildLabelText();
	VoiceVGUILabel* m_pLabel; // the label with the user name and icon
	int m_clientindex;	// Client index of the speaker. -1 if this label isn't being used.
	wchar_t* m_locationString; // localized location string.  NULL if the location is "".
	char* m_playerName;
};



//-----------------------------------------------------------------------------
// Purpose: Handles the displaying of labels on the hud and icons above players in game when they talk
//-----------------------------------------------------------------------------
class CVoiceStatusHud : public IVoiceHud, public CHudBase
{
public:
	CVoiceStatusHud();
	virtual ~CVoiceStatusHud();
	int Init( IVoiceStatusHelper* helper, IVoiceStatus* status ) override;
	int VidInit() override;
	void CreateEntities() override;
	void UpdateLocation( int player, const char* location ) override;
	void UpdateSpeakerStatus( int player, bool talking ) override;
	CVoiceLabel* FindVoiceLabel( int player );
														// Returns NULL if none.
														// entindex can be -1 if you want a currently-unused voice label.
	CVoiceLabel* GetFreeVoiceLabel();				// Get an unused voice label. Returns NULL if none.
	void RepositionLabels() override;

private:
	cl_entity_s m_VoiceHeadModels[VOICE_MAX_PLAYERS];			// These aren't necessarily in the order of players. They are just
																	// a place for it to put data in during CreateEntities.
	HSPRITE m_VoiceHeadModel;		// Voice head model (goes above players who are speaking).
	float m_VoiceHeadModelHeight;	// Height above their head to place the model.
	IVoiceStatusHelper* m_pHelper;
	IVoiceStatus* m_pStatus;

	// Labels telling who is speaking.
	CUtlVector<CVoiceLabel*> m_Labels;
	vgui2::ImagePanel* m_pLocalPlayerTalkIcon;
};

#endif
