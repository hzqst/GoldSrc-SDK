//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef ADMINSERVER_H
#define ADMINSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "../common/IAdminServer.h"
#include "../common/IManageServer.h"
#include "../GameUI/common/IVGuiModule.h"

#include "EditablePanel.h"
#include "Frame.h"
#include "Image.h"
#include "Menu.h"
#include "PHandle.h"
#include "PropertyPage.h"
#include "TextEntry.h"
#include "utllinkedlist.h"
#include "utlsymbol.h"
#include "utlvector.h"

#define SERVERDATA_REQUESTVALUE 0
#define SERVERDATA_SETVALUE 1
#define SERVERDATA_EXECCOMMAND 2

#define SERVERDATA_RESPONSE_VALUE 0
#define SERVERDATA_UPDATE 1

#define GAMESERVERDATA_INTERFACE_VERSION "GameServerData001"

class IGameServerData : public IBaseInterface
{
public:
	virtual void WriteDataRequest( const void* buffer, int bufferSize ) = 0;
	virtual int ReadDataResponse( void* buffer, int bufferSize ) = 0;
};

namespace vgui2
{
class AnimatingImagePanel;
class Button;
class CheckButton;
class ComboBox;
class ImagePanel;
class Label;
class ListPanel;
class PropertySheet;
class QueryBox;
class RadioButton;
class RichText;
}

class IServerDataResponse
{
public:
	virtual void OnServerDataResponse( const char* value, const char* response ) = 0;
};

class CRemoteServer
{
public:
	CRemoteServer();
	~CRemoteServer();

	void Initialize();

	void SetValue( const char* variable, const char* value );
	void SendCommand( const char* commandString );
	void RequestValue( IServerDataResponse* requester, const char* variable );

	void AddServerMessageHandler( IServerDataResponse* handler, const char* update );
	void RemoveServerDataResponseTarget( IServerDataResponse* invalidTarget );

	bool ProcessServerResponse();

private:
	void WriteRequest( int requestType, const char* variable, int variableLength, const char* value,
		int valueLength );

	struct ResponseHandlerRecord_t
	{
		int						requestID;
		IServerDataResponse*	handler;
	};

	struct MessageHandlerRecord_t
	{
		char					messageName[32];
		IServerDataResponse*	handler;
	};

	int													m_iCurrentRequestID;
	bool												m_bInitialized;
	CUtlLinkedList<ResponseHandlerRecord_t, int>		m_ResponseHandlers;
	CUtlLinkedList<MessageHandlerRecord_t, int>			m_MessageHandlers;
};

CRemoteServer& RemoteServer();

class CPlayerContextMenu : public vgui2::Menu
{
public:
	CPlayerContextMenu( vgui2::Panel* parent );
	~CPlayerContextMenu() override;

private:
	vgui2::Panel*	parent;
};

class CBanContextMenu : public vgui2::Menu
{
public:
	CBanContextMenu( vgui2::Panel* parent );
	~CBanContextMenu() override;

private:
	vgui2::Panel*	parent;
};

class CVarEditDialog : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CVarEditDialog, vgui2::Frame, "vgui::Frame" );

public:
	CVarEditDialog( vgui2::Panel* parent, const char* name );
	~CVarEditDialog() override;

	void Activate( vgui2::Panel* actionSignalTarget, KeyValues* rules );

protected:
	void OnCommand( const char* command ) override;
	void OnClose() override;

private:
	void ApplyChanges();

	vgui2::Button*		m_pOKButton;
	vgui2::Button*		m_pCancelButton;
	vgui2::TextEntry*	m_pStringEdit;
	vgui2::ComboBox*	m_pComboEdit;
	KeyValues*			m_pRules;
};

class CMapCycleEditDialog : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CMapCycleEditDialog, vgui2::Frame, "vgui::Frame" );

public:
	CMapCycleEditDialog( vgui2::Panel* parent, const char* name );
	~CMapCycleEditDialog() override;

	virtual void Activate( vgui2::Panel* updateTarget, CUtlVector<CUtlSymbol>& availableMaps,
		CUtlVector<CUtlSymbol>& mapCycle );

protected:
	void PerformLayout() override;
	void OnCommand( const char* command ) override;
	MESSAGE_FUNC_PTR( OnItemSelected, "ItemSelected", panel );

private:
	vgui2::ListPanel*	m_pAvailableMapList;
	vgui2::ListPanel*	m_pMapCycleList;
	vgui2::Button*		m_RightArrow;
	vgui2::Button*		m_LeftArrow;
	vgui2::Button*		m_UpArrow;
	vgui2::Button*		m_DownArrow;
};

class CDialogAddBan : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CDialogAddBan, vgui2::Frame, "vgui::Frame" );

public:
	CDialogAddBan( vgui2::Panel* parent );
	~CDialogAddBan() override;

	void Activate( const char* type, const char* player, const char* authid );

protected:
	void PerformLayout() override;
	void OnCommand( const char* command ) override;
	MESSAGE_FUNC_PTR( OnButtonToggled, "RadioButtonChecked", panel );

private:
	bool IsIPCheck();

	vgui2::TextEntry*	m_pTimeTextEntry;
	vgui2::TextEntry*	m_pIDTextEntry;
	vgui2::Button*		m_pOkayButton;
	vgui2::ComboBox*	m_pTimeCombo;
	vgui2::RadioButton*	m_pPermBanRadio;
	vgui2::RadioButton*	m_pTempBanRadio;
	const char*			m_cType;
};

class CDialogCvarChange : public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE_NAMED( CDialogCvarChange, vgui2::Frame, "vgui::Frame" );

public:
	CDialogCvarChange( vgui2::Panel* parent );
	~CDialogCvarChange() override;

	void Activate( const char* cvarName, const char* curValue, const char* type, const char* question );

protected:
	void OnCommand( const char* command ) override;
	void OnClose() override;

private:
	vgui2::Label*		m_pInfoLabel;
	vgui2::Label*		m_pCvarLabel;
	vgui2::TextEntry*	m_pCvarEntry;
	vgui2::Button*		m_pOkayButton;
	bool				m_bAddCvarText;
	const char*			m_cType;
};

class CVarListPropertyPage : public vgui2::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE_NAMED( CVarListPropertyPage, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	CVarListPropertyPage( vgui2::Panel* parent, const char* name );
	~CVarListPropertyPage() override;

	bool LoadVarList( const char* varfile );

	void OnServerDataResponse( const char* value, const char* response ) override;

protected:
	virtual void RefreshVarList();
	virtual void OnEditVariable( KeyValues* rule );

	void SetVarString( const char* varName, const char* value );
	const char* GetVarString( const char* varName );
	void SetCustomStringList( const char* varName, const char* stringList );

	void PerformLayout() override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;

private:
	MESSAGE_FUNC_CHARPTR( OnVarChanged, "VarChanged", var );
	MESSAGE_FUNC( EditVariable, "EditVariable" );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );

	vgui2::ListPanel*	m_pRulesList;
	vgui2::Button*		m_pEditButton;
};

class CServerInfoPanel : public CVarListPropertyPage
{
	DECLARE_CLASS_SIMPLE( CServerInfoPanel, CVarListPropertyPage );

public:
	CServerInfoPanel( vgui2::Panel* parent, const char* name );
	~CServerInfoPanel() override;

	const char* GetHostname();

protected:
	void OnResetData() override;
	void OnThink() override;
	void OnEditVariable( KeyValues* rule ) override;
	void OnServerDataResponse( const char* value, const char* response ) override;

private:
	void ParseIntoMapList( const char* maplist, CUtlVector<CUtlSymbol>& mapArray );
	void UpdateMapCycleValue();

	float					m_flUpdateTime;
	int						m_iPlayerCount;
	int						m_iMaxPlayers;
	float					m_flLastUptimeReceiveTime;
	int						m_iLastUptimeReceived;
	int						m_iLastUptimeDisplayed;
	bool					m_bMapListRetrieved;
	CUtlVector<CUtlSymbol>	m_AvailableMaps;
	CUtlVector<CUtlSymbol>	m_MapCycle;
};

class CServerConfigPanel : public CVarListPropertyPage
{
	DECLARE_CLASS_SIMPLE( CServerConfigPanel, CVarListPropertyPage );

public:
	CServerConfigPanel( vgui2::Panel* parent, const char* name, const char* mod );
	~CServerConfigPanel() override;

protected:
	void OnResetData() override;
	void OnThink() override;

private:
	float	m_flUpdateTime;
};

class CPlayerPanel : public vgui2::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE_NAMED( CPlayerPanel, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	CPlayerPanel( vgui2::Panel* parent, const char* name );
	~CPlayerPanel() override;

	void OnServerDataResponse( const char* value, const char* response ) override;

protected:
	void OnResetData() override;
	void OnThink() override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;

	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	MESSAGE_FUNC( OnKickButtonPressed, "KickPlayer" );
	MESSAGE_FUNC( OnBanButtonPressed, "BanPlayer" );
	MESSAGE_FUNC( KickSelectedPlayers, "KickSelectedPlayers" );
	_MessageFuncCommon( AddBanByID, "AddBanValue", ( void ( vgui2::Panel::* )( const char*, const char* ) )&ThisClass::AddBanByID,
		2, vgui2::DATATYPE_CONSTCHARPTR, "id", vgui2::DATATYPE_CONSTCHARPTR, "time" );
	virtual void AddBanByID( const char* id, const char* newtime );

private:
	vgui2::ListPanel*	m_pPlayerListPanel;
	vgui2::Button*		m_pKickButton;
	vgui2::Button*		m_pBanButton;
	CPlayerContextMenu*	m_pPlayerContextMenu;
	float				m_flUpdateTime;
};

class CBanPanel : public vgui2::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE_NAMED( CBanPanel, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	CBanPanel( vgui2::Panel* parent, const char* name );
	~CBanPanel() override;

	void OnServerDataResponse( const char* value, const char* response ) override;

protected:
	void OnResetData() override;
	void OnPageShow() override;
	void OnThink() override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;

	MESSAGE_FUNC( OnAddBan, "addban" );
	MESSAGE_FUNC( OnRemoveBan, "removeban" );
	MESSAGE_FUNC( OnChangeBan, "changeban" );
	MESSAGE_FUNC_CHARPTR( RemoveBanByID, "removebanbyid", id );
	_MessageFuncCommon( AddBanByID, "AddBanValue", ( void ( vgui2::Panel::* )( const char*, const char* ) )&ThisClass::AddBanByID,
		2, vgui2::DATATYPE_CONSTCHARPTR, "id", vgui2::DATATYPE_CONSTCHARPTR, "time" );
	virtual void AddBanByID( const char* id, const char* newtime );
	MESSAGE_FUNC_PARAMS( OnCvarChangeValue, "CvarChangeValue", kv );
	MESSAGE_FUNC_INT( OnOpenContextMenu, "OpenContextMenu", itemID );
	MESSAGE_FUNC( OnItemSelected, "ItemSelected" );
	MESSAGE_FUNC( OnImportBanList, "importban" );
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

private:
	bool IsIPAddress( const char* text );

	vgui2::ListPanel*	m_pBanListPanel;
	vgui2::Button*		m_pAddButton;
	vgui2::Button*		m_pRemoveButton;
	vgui2::Button*		m_pChangeButton;
	vgui2::Button*		m_pImportButton;
	CBanContextMenu*	m_pBanContextMenu;
	float				m_flUpdateTime;
	bool				m_bPageViewed;
};

class CGraphPanel : public vgui2::PropertyPage, public IServerDataResponse
{
	DECLARE_CLASS_SIMPLE_NAMED( CGraphPanel, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	CGraphPanel( vgui2::Panel* parent, const char* name );
	~CGraphPanel() override;

	void OnServerDataResponse( const char* value, const char* response ) override;

protected:
	void OnPageShow() override;
	void OnPageHide() override;
	void PerformLayout() override;
	void ApplySchemeSettings( vgui2::IScheme* pScheme ) override;
	void OnTick() override;

	MESSAGE_FUNC( OnCheckButton, "CheckButtonChecked" );
	_MessageFuncCommon( OnTextChanged, "TextChanged", ( void ( vgui2::Panel::* )( vgui2::Panel*, const char* ) )&ThisClass::OnTextChanged,
		2, vgui2::DATATYPE_PTR, "panel", vgui2::DATATYPE_CONSTCHARPTR, "text" );
	virtual void OnTextChanged( vgui2::Panel* panel, const char* text );
	MESSAGE_FUNC( OnClearButton, "clear" );

private:
	typedef enum { MINUTES, HOURS, DAYS } intervals;

	struct Points_t
	{
		float	cpu;
		float	in;
		float	out;
		float	time;
		float	fps;
		float	ping;
		float	players;
	};

	class CGraphsImage : public vgui2::Image
	{
	public:
		CGraphsImage();
		using Image::DrawLine;
		using Image::SetPos;
		bool AddPoint( Points_t p );
		void RemovePoints() { points.RemoveAll(); }
		void SaveSize( int x1, int y1 ) { x = x1; y = y1; SetSize( x1, y1 ); }

		void SetDraw( bool cpu_in, bool fps_in, bool net_in, bool net_out, bool ping_in, bool players_in );
		void SetScale( intervals time );
		void SetBgColor( Color col ) { bgColor = col; }
		void SetAxisColor( Color col ) { lineColor = col; }

		void GetFPSLimits( float& max, float& min ) { min = minFPS; max = maxFPS; }
		void GetInLimits( float& max, float& min ) { min = minIn; max = maxIn; }
		void GetOutLimits( float& max, float& min ) { min = minOut; max = maxOut; }
		void GetPingLimits( float& max, float& min ) { min = minPing; max = maxPing; }
		void GetPlayerLimits( float& max, float& min ) { min = minPlayers; max = maxPlayers; }

		void Paint() override;

	private:
		void CheckBounds( Points_t p );

		CUtlVector<Points_t> points;

		int x, y;
		float maxIn, minIn, maxOut, minOut;
		float maxFPS, minFPS;
		float minPing, maxPing;
		float maxPlayers, minPlayers;

		bool cpu, fps, net_i, ping, net_o, players;
		intervals timeBetween;
		Points_t avgPoint;
		int numAvgs;
		Color bgColor;
		Color lineColor;
	};
	friend class CGraphsImage;

	void SetAxisLabels( Color c, const char* max, const char* mid, const char* min );

	CGraphsImage*			m_pGraphs;
	vgui2::ImagePanel*		m_pGraphsPanel;
	vgui2::CheckButton*		m_pInButton;
	vgui2::CheckButton*		m_pOutButton;
	vgui2::CheckButton*		m_pFPSButton;
	vgui2::CheckButton*		m_pCPUButton;
	vgui2::CheckButton*		m_pPINGButton;
	vgui2::CheckButton*		m_pPlayerButton;
	vgui2::ComboBox*		m_pTimeCombo;
	vgui2::ComboBox*		m_pVertCombo;
	float					m_flNextStatsUpdateTime;
};

class CNonFocusableMenu : public vgui2::Menu
{
	DECLARE_CLASS_SIMPLE_NAMED( CNonFocusableMenu, vgui2::Menu, "Menu" );

public:
	CNonFocusableMenu( vgui2::Panel* parent, const char* panelName );

	void SetFocusPanel( vgui2::Panel* panel ) { m_pFocus = panel; }
	vgui2::VPANEL GetCurrentKeyFocus() override;

private:
	vgui2::Panel*	m_pFocus;
};

class TabCatchingTextEntry : public vgui2::TextEntry
{
public:
	TabCatchingTextEntry( vgui2::Panel* parent, const char* name, vgui2::VPANEL comp );

	void OnKeyCodeTyped( vgui2::KeyCode code ) override;
	void OnKillFocus() override;

private:
	vgui2::VPANEL	m_pCompletionList;
};

class CConsolePanel : public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE_NAMED( CConsolePanel, vgui2::EditablePanel, "vgui::EditablePanel" );

public:
	CConsolePanel( vgui2::Panel* parent, const char* panelName, bool bStatusVersion );
	~CConsolePanel() override;

	void ColorPrint( const Color& clr, const char* msg );
	void Print( const char* msg );
	void DPrint( const char* msg );

protected:
	void OnThink() override;
	void PerformLayout() override;
	void ApplySchemeSettings( vgui2::IScheme* pScheme ) override;
	void OnCommand( const char* command ) override;
	void OnKeyCodeTyped( vgui2::KeyCode code ) override;

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );
	MESSAGE_FUNC( CloseCompletionList, "CloseCompletionList" );
	MESSAGE_FUNC_CHARPTR( OnMenuItemSelected, "CompletionCommand", command );

private:
	class CHistoryItem
	{
	public:
		CHistoryItem();
		CHistoryItem( const char* text, const char* extra );
		CHistoryItem( const CHistoryItem& src );
		CHistoryItem& operator=( const CHistoryItem& src );
		~CHistoryItem();

		const char* GetText() const;
		const char* GetExtra() const;
		void SetText( const char* text, const char* extra );
		bool HasExtra() const { return m_bHasExtra; }

	private:
		char*	m_text;
		char*	m_extraText;
		bool	m_bHasExtra;
	};

	class CompletionItem
	{
	public:
		CompletionItem();
		CompletionItem( const CompletionItem& src ) = delete;
		CompletionItem& operator=( const CompletionItem& src ) = delete;
		~CompletionItem();

		const char* GetItemText();

		bool			m_bIsCommand;
		CHistoryItem*	m_pText;
	};

	void OnAutoComplete( bool reverse );
	void RebuildCompletionList( const char* partialText );
	void UpdateCompletionListPosition();
	void ClearCompletionList();
	void AddToHistory( const char* commandText, const char* extraText );

	vgui2::RichText*				m_pHistory;
	TabCatchingTextEntry*			m_pEntry;
	vgui2::Button*					m_pSubmit;
	CNonFocusableMenu*				m_pCompletionList;
	Color							m_PrintColor;
	Color							m_DPrintColor;
	int								m_iNextCompletion;
	char							m_szPartialText[256];
	char							m_szPreviousPartialText[256];
	bool							m_bAutoCompleteMode;
	bool							m_bStatusVersion;
	CUtlVector<CompletionItem*>		m_CompletionList;
	CUtlVector<CHistoryItem>		m_CommandHistory;
};

class CRawLogPanel : public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE_NAMED( CRawLogPanel, vgui2::PropertyPage, "vgui::PropertyPage" );

public:
	CRawLogPanel( vgui2::Panel* parent, const char* name );
	~CRawLogPanel() override;

	void DoInsertString( const char* str );

protected:
	void OnPageShow() override;
	void OnPageHide() override;

	MESSAGE_FUNC_CHARPTR( OnCommandSubmitted, "CommandSubmitted", command );

private:
	CConsolePanel*	m_pServerLogPanel;
};

//-----------------------------------------------------------------------------
// Purpose: Dialog for displaying information about a game server
//-----------------------------------------------------------------------------
class CGamePanelInfo : public vgui2::Frame, public IManageServer
{
	DECLARE_CLASS_SIMPLE_NAMED( CGamePanelInfo, vgui2::Frame, "vgui::Frame" );

public:
	CGamePanelInfo( vgui2::Panel* parent, const char* name, const char* mod );
	~CGamePanelInfo() override;

	void ShowPage() override;
	void SetAsRemoteServer( bool remote ) override;
	void AddToConsole( const char* msg ) override;

protected:
	void OnTick() override;
	void OnCommand( const char* command ) override;
	void OnClose() override;
	void ActivateBuildMode() override;

	MESSAGE_FUNC( OnHelp, "Help" );
	MESSAGE_FUNC( OnRestartServer, "RestartServer" );
	MESSAGE_FUNC( OnUpdateTitle, "UpdateTitle" );

private:
	void ShowMasterOutOfDateMessage( const char* details );

	vgui2::AnimatingImagePanel*			m_pAnimImagePanel;
	vgui2::PropertySheet*				m_pDetailsSheet;
	CPlayerPanel*						m_pPlayerListPanel;
	CBanPanel*							m_pBanListPanel;
	CRawLogPanel*						m_pServerLogPanel;
	CServerConfigPanel*					m_pServerConfigPanel;
	CGraphPanel*						m_pGraphsPanel;
	CServerInfoPanel*					m_pServerInfoPanel;
	bool								m_bRemoteServer;
	bool								m_bShuttingDown;
	vgui2::DHANDLE<vgui2::QueryBox>		m_hRestartQueryBox;
	vgui2::DHANDLE<vgui2::QueryBox>		m_hOutOfDateQueryBox;
};

//-----------------------------------------------------------------------------
// Purpose: Handles the UI and pinging of a half-life game server list
//-----------------------------------------------------------------------------
class CAdminServer : public IAdminServer, public IVGuiModule
{
public:
	CAdminServer();
	~CAdminServer() override;

	// IVGui module implementation
	bool Initialize( CreateInterfaceFn* factorylist, int numFactories ) override;
	bool PostInitialize( CreateInterfaceFn* modules, int factoryCount ) override;
	vgui2::VPANEL GetPanel() override;
	bool Activate() override;
	bool IsValid() override;
	void Shutdown() override;
	void Deactivate() override;
	void Reactivate() override;
	void SetParent( vgui2::VPANEL parent ) override;
	void RunFrame() override;

	// IAdminServer implementation
	ManageServerUIHandle_t OpenManageServerDialog( const char* serverName, const char* gameDir ) override;
	ManageServerUIHandle_t OpenManageServerDialog( unsigned int gameIP, unsigned int gamePort,
		const char* password ) override;
	void CloseManageServerDialog( ManageServerUIHandle_t gameDialog ) override;
	IManageServer* GetManageServerInterface( ManageServerUIHandle_t handle ) override;

private:
	struct OpenedManageDialog_t
	{
		unsigned long	handle;
		IManageServer*	manageInterface;
	};

	CUtlVector<OpenedManageDialog_t>	m_OpenedManageDialog;
	vgui2::VPANEL						m_hParent;
};

extern IGameServerData* g_pGameServerData;

#endif // AdminServer_H
