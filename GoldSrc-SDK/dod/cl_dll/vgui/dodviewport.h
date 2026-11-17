#ifndef DODVIEWPORT_H
#define DODVIEWPORT_H

#include "game_controls/vgui_TeamFortressViewport.h"
#include "vgui/VGuiVertex.h"
#include "idodviewport.h"
#include "idodviewportmsgs.h"

class CDoDTeamMenu;
class CDoDClassMenu;
class CWidescreenPanel;
class CHintPanel;
class CDoDClientScoreBoardDialog;
class CDoDSpectatorGUI;
class CMortarPanel;

class DoDViewport : public TeamFortressViewport, public IDoDViewPort, public IDoDViewPortMsgs
{
public:
	DoDViewport();
	void Start() override;
	void SetParent( vgui2::VPANEL parent ) override;
	void HideAllVGUIMenu() override;
	int KeyInput( int down, int keynum, const char* binding ) override;
	void HideVGUIMenu( int menu ) override;
	void UpdateSpectatorPanel() override;
	void ShowSpectatorGUI() override;
	void DeactivateSpectatorGUI() override;
	void UpdateOnPlayerInfo();
	bool CanShowScoreBoard() override;
	int GetTeamScore( int team ) override;
	int GetForceCamera() override;
	void setWidescreenVisible( bool visible ) override;
	bool isWidescreenVisible() override;
	void setWidescreenSubtitle( int subtitle ) override;
	void updateTeamMenu() override;
	void ShowHintMessage( const char* message ) override;
	int MsgFunc_ForceCam( const char* name, int size, void* buffer ) override;
	int MsgFunc_ScoreInfo( const char* name, int size, void* buffer ) override;
	int MsgFunc_ScoreInfoLong( const char* name, int size, void* buffer ) override;
	int MsgFunc_TeamScore( const char* name, int size, void* buffer ) override;
	int MsgFunc_MapMarker( const char* name, int size, void* buffer ) override;
	int MsgFunc_WaveTime( const char* name, int size, void* buffer ) override;
	int MsgFunc_WaveStatus( const char* name, int size, void* buffer ) override;
	int MsgFunc_WideScreen( const char* name, int size, void* buffer ) override;
	int MsgFunc_ScoreShort( const char* name, int size, void* buffer ) override;
	int MsgFunc_Frags( const char* name, int size, void* buffer ) override;
	int MsgFunc_ObjScore( const char* name, int size, void* buffer ) override;
	int MsgFunc_PStatus( const char* name, int size, void* buffer ) override;
	int MsgFunc_PTeam( const char* name, int size, void* buffer ) override;
	int MsgFunc_PClass( const char* name, int size, void* buffer ) override;
	int MsgFunc_CurMarker( const char* name, int size, void* buffer ) override;
	int MsgFunc_VGUIMenu( const char* name, int size, void* buffer ) override;
	void SetMapMarker( int marker );
	HSPRITE GetMapMarkerSpr();
	void PaintBackgroundPanel() override;
	ExtraScoreBoardInfo GetExtraScoreBoardInfo( int team ) override;
	VGuiLibraryTeamInfo_t GetPlayerTeamInfo( int team ) override;
	void ShowMortarPanel() override;
	void HideMortarPanel() override;
	void PostMortarValue( float value ) override;
	bool IsVGUIMenuActive( int menu ) override;
	void DisplayVGUIMenu( int menu ) override;
	void OnTick() override;

private:
	CClientScoreBoardDialog* CreateScoreBoard() override;
	CClientMOTD* CreateMOTD() override;
	CSpectatorGUI* CreateSpectatorGUI() override;
	CTeamMenu* CreateTeamMenu() override;
	CClassMenu* CreateClassMenu() override;

	CDoDTeamMenu* m_pTeamMenu;
	CDoDClassMenu* m_pClassMenu;
	CWidescreenPanel* m_pWidescreenPanel;
	CHintPanel* m_pHintPanel;
	CDoDClientScoreBoardDialog* m_pPrivateClientScoreBoard;
	CDoDSpectatorGUI* m_pPrivateSpectatorGUI;
	CMortarPanel* m_pMortarPanel;
	int m_iForceCamera;
	int m_iForceChaseCam;
	int m_iFadeToBlack;
	int m_iMapMarker;
	float m_flMarkerDrawTime;
	int m_iBackgroundTexture;
	vgui2::VGuiVertex m_BackgroundTopVerts[4];
	vgui2::VGuiVertex m_BackgroundMainVerts[5];
	int m_iClassMask;
};

extern VGuiLibraryInterface_t clientInterface;

#endif
