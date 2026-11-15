#ifndef PROXY_H
#define PROXY_H

#include "HLTV/common/IProxy.h"
#include "HLTV/common/IWorld.h"
#include "HLTV/common/Bitbuffer.h"
#include "HLTV/common/NetAddress.h"
#include "common/BaseSystemModule.h"
#include "DemoClient.h"
#include "Status.h"
#include "Master.h"
#include "Director.h"
#include "DirectorNull.h"

class INetwork;
class InfoString;

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4250 )
#endif

class Proxy : public IProxy, public BaseSystemModule
{
	friend class Master;

public:
	Proxy();
	virtual ~Proxy() override;

	virtual bool Init( IBaseSystem* system, int serial, char* name ) override;
	virtual void RunFrame( double time ) override;
	virtual void ReceiveSignal( ISystemModule* module, unsigned int signal, void* data ) override;
	virtual void ExecuteCommand( int commandID, char* commandLine ) override;
	virtual char* GetStatusLine() override;
	virtual char* GetType() override;
	virtual void ShutDown() override;

	virtual void Reset() override;
	virtual void Broadcast( unsigned char* data, int length, int groupType, bool isReliable ) override;
	virtual void IncreaseCheering( int amount ) override;
	void ReconnectClients();
	virtual void ChatCommentator( char* name, char* text ) override;
	virtual void ChatSpectator( char* name, char* text ) override;
	virtual void CountLocalClients( int& spectators, int& proxies ) override;
	virtual void ParseStatusMsg( BitBuffer* stream ) override;
	virtual void ParseStatusReport( NetAddress* from, BitBuffer* stream ) override;
	virtual bool ProcessConnectionlessMessage( NetAddress* from, BitBuffer* stream ) override;
	virtual resource_s* AddResource( char* filename, int type, char* name ) override;

	virtual bool IsMaster() override;
	virtual bool IsLanOnly() override;
	virtual bool IsActive() override;
	virtual bool IsPublicGame() override;
	virtual bool IsPasswordProtected() override;
	virtual bool IsStressed() override;
	virtual void SetDelay( float delay ) override;
	virtual void SetClientTime( double time, bool relative ) override;
	virtual void SetClientTimeScale( float scale ) override;
	virtual void SetMaxRate( int rate ) override;
	virtual void SetMaxLoss( float loss ) override;
	virtual void SetMaxUpdateRate( int rate ) override;
	virtual bool SetMaxClients( int clients ) override;
	virtual void SetRegion( unsigned char region ) override;

	virtual float GetDelay() override;
	virtual double GetSpectatorTime() override;
	virtual double GetProxyTime() override;
	virtual IObjectContainer* GetClients() override;
	virtual IWorld* GetWorld() override;
	virtual IServer* GetServer() override;
	virtual IDirector* GetDirector() override;
	virtual INetSocket* GetSocket() override;
	virtual int GetChatMode() override;
	virtual void GetStatistics( int& proxies, int& slots, int& spectators ) override;
	virtual int GetMaxRate() override;
	virtual int GetMaxClients() override;
	virtual int GetMaxUpdateRate() override;
	virtual resource_s* GetResource( char* filename ) override;
	virtual int GetDispatchMode() override;
	virtual unsigned char GetRegion() override;
	virtual bool WriteSignonData( int type, BitBuffer* stream ) override;

protected:
	typedef struct loopcmd_s
	{
		int		id;
		float	interval;
		float	lastTime;
		char	command[255];
	} loopcmd_t;

	typedef struct textmessage_s
	{
		int				effect;
		unsigned char	r1, g1, b1, a1;
		unsigned char	r2, g2, b2, a2;
		float			x, y;
		float			fadein, fadeout, holdtime, fxtime;
		char			text[80];
	} textmessage_t;

	enum LocalCommandIDs
	{
		CMD_ID_RCON = 1,
		CMD_ID_RCONPASSWORD,
		CMD_ID_RCONADDRESS,
		CMD_ID_SAY,
		CMD_ID_MSG,
		CMD_ID_CLIENTS,
		CMD_ID_KICK,
		CMD_ID_CHATMODE,
		CMD_ID_PUBLICGAME,
		CMD_ID_OFFLINETEXT,
		CMD_ID_ADMINPASSWORD,
		CMD_ID_SIGNONCOMMANDS,
		CMD_ID_SPECTATORPASSWORD,
		CMD_ID_DISPATCHMODE,
		CMD_ID_CHEERINGTHRESHOLD,
		CMD_ID_INFORMPLAYERS,
		CMD_ID_PING,
		CMD_ID_PROXYPASSWORD,
		CMD_ID_MAXRATE,
		CMD_ID_LOOPCMD,
		CMD_ID_MAXCLIENTS,
		CMD_ID_DISCONNECT,
		CMD_ID_LOCALMSG,
		CMD_ID_CONNECT,
		CMD_ID_PLAYDEMO,
		CMD_ID_DELAY,
		CMD_ID_STOP,
		CMD_ID_RECORD,
		CMD_ID_STOPRECORDING,
		CMD_ID_SERVERCMD,
		CMD_ID_CLIENTCMD,
		CMD_ID_BLOCKVOICE,
		CMD_ID_NAME,
		CMD_ID_UPDATERATE,
		CMD_ID_RATE,
		CMD_ID_ADDRESOURCE,
		CMD_ID_RESOURCES,
		CMD_ID_BANNERFILE,
		CMD_ID_BANN,
		CMD_ID_ADDFAKECLIENTS,
		CMD_ID_RETRY,
		CMD_ID_PLAYERS,
		CMD_ID_AUTORETRY,
		CMD_ID_SERVERPASSWORD,
		CMD_ID_STATUS,
		CMD_ID_HOSTNAME,
		CMD_ID_MAXQUERIES,
		CMD_ID_CLEARBANNS,
		CMD_ID_MAXLOSS,
		CMD_ID_PROTOCOL,
		CMD_ID_REGION
	};

	void ExecuteRcon( NetAddress* from, char* command );
	void SendRcon( NetAddress* to, unsigned int challenge );
	void ReplyServiceChallenge( NetAddress* from, char* type );
	void ReplyListen( NetAddress* to );
	void RejectConnection( NetAddress* to, bool badPassword, const char* format, ... );
	void ReplyConnect( NetAddress* from, int protocol, int challenge, char* protinfo, char* userinfo );
	void ReplyRules( NetAddress* to );
	void ReplyPlayers( NetAddress* to );
	void ReplyInfo( NetAddress* to, bool details );
	void ReplyInfoString( NetAddress* to );
	void ReplyChallenge( NetAddress* to );
	void ReplyPing( NetAddress* to );

	void CMD_Rcon( char* cmdLine );
	void CMD_ServerCmd( char* cmdLine );
	void CMD_ClientCmd( char* cmdLine );
	void CMD_RconPassword( char* cmdLine );
	void CMD_RconAddress( char* cmdLine );
	void CMD_Say( char* cmdLine );
	void CMD_Msg( char* cmdLine );
	void CMD_Clients( char* cmdLine );
	void CMD_Kick( char* cmdLine );
	void CMD_ChatMode( char* cmdLine );
	void CMD_PublicGame( char* cmdLine );
	void CMD_OffLineText( char* cmdLine );
	void CMD_AdminPassword( char* cmdLine );
	void CMD_SignOnCommands( char* cmdLine );
	void CMD_SpectatorPassword( char* cmdLine );
	void CMD_DispatchMode( char* cmdLine );
	void CMD_CheeringThreshold( char* cmdLine );
	void CMD_InformPlayers( char* cmdLine );
	void CMD_Ping( char* cmdLine );
	void CMD_ProxyPassword( char* cmdLine );
	void CMD_MaxRate( char* cmdLine );
	void CMD_MaxUpdateRate( char* cmdLine );
	void CMD_LoopCmd( char* cmdLine );
	void CMD_MaxClients( char* cmdLine );
	void CMD_LocalMsg( char* cmdLine );
	void CMD_Connect( char* cmdLine );
	void CMD_Disconnect( char* cmdLine );
	void CMD_PlayDemo( char* cmdLine );
	void CMD_Delay( char* cmdLine );
	void CMD_Stop( char* cmdLine );
	void CMD_Record( char* cmdLine );
	void CMD_StopRecording( char* cmdLine );
	void CMD_BlockVoice( char* cmdLine );
	void CMD_Name( char* cmdLine );
	void CMD_Rate( char* cmdLine );
	void CMD_Updaterate( char* cmdLine );
	void CMD_HostName( char* cmdLine );
	void CMD_AddResource( char* cmdLine );
	void CMD_Resources( char* cmdLine );
	void CMD_BannerFile( char* cmdLine );
	void CMD_Bann( char* cmdLine );
	void CMD_AddFakeClients( char* cmdLine );
	void CMD_Retry( char* cmdLine );
	void CMD_AutoRetry( char* cmdLine );
	void CMD_ServerPassword( char* cmdLine );
	void CMD_Status( char* cmdLine );
	void CMD_MaxQueries( char* cmdLine );
	void CMD_Players( char* cmdLine );
	void CMD_ClearBanns( char* cmdLine );
	void CMD_MaxLoss( char* cmdLine );
	void CMD_Protocol( char* cmdLine );
	void CMD_Region( char* cmdLine );

	void UpdateStatusLine();
	void DispatchClient( NetAddress* client, NetAddress* proxy );
	bool IsValidPassword( int clientType, char* password );
	void WriteHUDMsg( textmessage_t* message, BitBuffer* stream );
	void ExecuteLoopCommands();
	unsigned int GetChallengeNumber( NetAddress* address );
	bool CheckChallenge( NetAddress* address, unsigned int challenge );
	void CreateServerInfoString( InfoString* info );
	bool CheckDirectorModule();
	void RunClocks();
	void NewGameStarted();
	void NewServerConnection();
	void BroadcastPaused( bool paused );
	void BroadcastRetryMessage();
	void StopBroadcast( char* message );
	char* GetModVersion( char* gameDir );
	void DisconnectClients( char* reason );
	void FreeResource( resource_t* resource );
	void ClearResources();
	resource_t* LoadResourceFromFile( char* filename, resourcetype_t type );
	bool IsBanned( NetAddress* address );
	void UpdateInfoMessages();

	INetwork*		m_Network;
	IWorld*			m_World;
	IDirector*		m_Director;
	IServer*		m_Server;
	ObjectList		m_Clients;
	DemoClient		m_DemoClient;
	Status			m_Status;
	Master			m_Master;
	Director		m_DefaultDirector;
	DirectorNull	m_NullDirector;
	INetSocket*		m_Socket;
	struct
	{
		NetAddress		adr;
		unsigned int	challenge;
		float			time;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
	} challenges[1024];
	bool			m_IsMaster;
	bool			finishingBroadcast;
	bool			reconnectRequested;
	int				maxRate;
	int				maxUpdateRate;
	int				m_ChatMode;
	int				maxclients;
	float			m_MaxQueries;
	int				m_MaxFrameQueries;
	int				m_MaxSeenClients;
	NetAddress		rconAddress;
	char			rconPassword[128];
	char			adminPassword[128];
	char			proxyPassword[128];
	char			spectatorPassword[128];
	char			lastrconcommand[1024];
	char			offLineText[128];
	char			signonCommands[256];
	ObjectList		m_LoopCommands;
	bool			m_PublicGame;
	int				m_DispatchMode;
	float			m_ClientDelay;
	float			clientTimeScale;
	double			clientWorldTime;
	double			clientProxyTime;
	double			m_LastClockUpdateTime;
	float			m_CheeringThreshold;
	float			m_LastCheeringUpdate;
	int				m_CheeringPlayers;
	resource_s*		m_BannerTGA;
	ObjectList		m_Resources;
	double			m_NextStatusUpdateTime;
	float			m_FPS;
	float			m_MaxLoss;
	float			m_CurrentLoss;
	ObjectList		m_BannList;
	unsigned char	m_iRegion;
	textmessage_t	localMessage;
	textmessage_t	commentatorMessage;
	double			m_NextInfoMessagesUpdate;
	BitBuffer		m_InfoRules;
	BitBuffer		m_InfoPlayers;
	BitBuffer		m_InfoDetails;
	BitBuffer		m_InfoInfo;
	BitBuffer		m_InfoString;
};

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#endif
