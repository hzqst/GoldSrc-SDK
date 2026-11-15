#include "Proxy.h"

#include "ProxyClient.h"
#include "FakeClient.h"
#include "common/IBaseSystem.h"
#include "common/TokenLine.h"
#include "common/hltv.h"
#include "HLTV/common/IClient.h"
#include "HLTV/common/INetwork.h"
#include "HLTV/common/INetSocket.h"
#include "HLTV/common/InfoString.h"
#include "HLTV/common/NetPacket.h"
#include "HLTV/common/common.h"
#include "HLTV/common/mem.h"
#include "HLTV/common/random.h"
#include "HLTV/common/md5.h"
#include "HLTV/common/DirectorCmd.h"
#include "external/steamworks/isteamgameserver.h"
#include "external/steamworks/steam_gameserver.h"

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif
#include <stdarg.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf
#endif

#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
Proxy::Proxy()
{
}

Proxy::~Proxy()
{
}

IObjectContainer* Proxy::GetClients() { return &m_Clients; }
IWorld* Proxy::GetWorld() { return m_World; }
IServer* Proxy::GetServer() { return m_Server; }
IDirector* Proxy::GetDirector() { return m_Director; }
INetSocket* Proxy::GetSocket() { return m_Socket; }
int Proxy::GetChatMode() { return m_ChatMode; }
int Proxy::GetMaxRate() { return maxRate; }
int Proxy::GetMaxUpdateRate() { return maxUpdateRate; }
bool Proxy::IsPublicGame() { return m_PublicGame; }
bool Proxy::IsPasswordProtected() { return spectatorPassword[0] != 0; }
int Proxy::GetDispatchMode() { return m_DispatchMode; }
char* Proxy::GetType() { return "proxy001"; }
bool Proxy::IsLanOnly() { return m_Master.m_NoMaster; }
unsigned char Proxy::GetRegion() { return m_iRegion; }
int Proxy::GetMaxClients() { return maxclients; }
void Proxy::SetRegion( unsigned char region ) { m_iRegion = region; }
bool Proxy::IsMaster() { return m_IsMaster; }
float Proxy::GetDelay() { return m_IsMaster ? m_ClientDelay : 0.0f; }
double Proxy::GetSpectatorTime() { return clientWorldTime; }
double Proxy::GetProxyTime() { return clientProxyTime; }

bool Proxy::IsActive()
{
	return m_World && m_World->IsActive();
}

bool Proxy::IsStressed()
{
	return m_CurrentLoss > m_MaxLoss || m_FPS < 30.0f;
}

bool Proxy::SetMaxClients( int clients )
{
	if ( clients < 0 )
	{
		maxclients = 0;
		return false;
	}
	if ( clients > 255 )
	{
		maxclients = 255;
		return false;
	}
	maxclients = clients;
	return true;
}

void Proxy::SetMaxLoss( float loss )
{
	if ( loss > 1.0f )
		m_MaxLoss = 1.0f;
	else if ( loss < 0.0f )
		m_MaxLoss = 0.0f;
	else
		m_MaxLoss = loss;
}

void Proxy::SetMaxRate( int rate )
{
	if ( rate > 20000 )
		maxRate = 20000;
	else if ( rate < 1000 )
		maxRate = 1000;
	else
		maxRate = rate;
}

void Proxy::SetMaxUpdateRate( int rate )
{
	if ( rate > 40 )
		maxUpdateRate = 40;
	else if ( rate < 1 )
		maxUpdateRate = 1;
	else
		maxUpdateRate = rate;
}

void Proxy::SetClientTime( double time, bool relative )
{
	if ( relative )
		time += clientWorldTime;
	clientWorldTime = time;
}

void Proxy::GetStatistics( int& proxies, int& slots, int& spectators )
{
	m_Status.GetGlobalStats( proxies, slots, spectators );
}

void Proxy::SetDelay( float delay )
{
	m_ClientDelay = delay;
	if ( delay < 10.0f )
	{
		m_ClientDelay = 0.0f;
		m_World->SetBufferSize( 10.0f );
	}
	else
	{
		m_World->SetBufferSize( delay + delay );
		clientWorldTime = m_World->GetTime() - m_ClientDelay;
	}
	m_Server->SetUserInfo( "hdelay", COM_VarArgs( "%u", (int)m_ClientDelay ) );
}

void Proxy::SetClientTimeScale( float scale )
{
	if ( scale > 4.0f )
		clientTimeScale = 4.0f;
	else if ( scale < 0.05f )
		clientTimeScale = 0.05f;
	else
		clientTimeScale = scale;

	BitBuffer buffer( 32 );
	buffer.WriteByte( svc_timescale );
	buffer.WriteFloat( clientTimeScale );
	Broadcast( buffer.data, buffer.CurrentSize(), GROUP_CLIENT | GROUP_PROXY | GROUP_DEMO | GROUP_UNUSED, true );
}

void Proxy::Reset()
{
	m_Status.Reset();
	m_MaxSeenClients = 0;
	m_LastCheeringUpdate = 0.0f;
	m_CheeringPlayers = 0;
}

void Proxy::ParseStatusMsg( BitBuffer* stream )
{
	m_Status.ParseStatusMsg( stream );
}

void Proxy::ParseStatusReport( NetAddress* from, BitBuffer* stream )
{
	m_Status.ParseStatusReport( from, stream );
}

void Proxy::CountLocalClients( int& spectators, int& proxies )
{
	spectators = 0;
	proxies = 0;
	for ( IClient* client = (IClient*)m_Clients.GetFirst(); client; client = (IClient*)m_Clients.GetNext() )
	{
		if ( client->GetClientType() == TYPE_PROXY )
			proxies++;
		else
			spectators++;
	}
}

resource_s* Proxy::GetResource( char* filename )
{
	for ( resource_t* resource = (resource_t*)m_Resources.GetFirst(); resource;
		resource = (resource_t*)m_Resources.GetNext() )
	{
		if ( !strncasecmp( filename, resource->szFileName, sizeof( resource->szFileName ) ) )
			return resource;
	}
	return NULL;
}

char* Proxy::GetStatusLine()
{
	static char string[256];
	snprintf( string, sizeof( string ), "Proxy name \"%s\", %s, Port %i, Clients %i/%i.\n",
		m_World->GetName(), m_IsMaster ? "Master" : "Relay", m_Socket->GetPort(),
		m_Clients.CountElements(), maxclients );
	return string;
}

void Proxy::Broadcast( unsigned char* data, int length, int groupType, bool isReliable )
{
	for ( IClient* client = (IClient*)m_Clients.GetFirst(); client; client = (IClient*)m_Clients.GetNext() )
	{
		if ( !client->IsActive() )
			continue;
		if ( ( ( groupType & GROUP_CLIENT ) && client->GetClientType() == TYPE_CLIENT ) ||
			( ( groupType & GROUP_PROXY ) && client->GetClientType() == TYPE_PROXY ) ||
			( ( groupType & GROUP_VOICE ) && client->IsHearingVoices() ) ||
			( ( groupType & GROUP_CHAT ) && client->HasChatEnabled() ) )
			client->Send( data, length, isReliable );
	}
	if ( m_DemoClient.IsActive() && ( groupType & GROUP_DEMO ) )
		m_DemoClient.Send( data, length, isReliable );
}

void Proxy::ReconnectClients()
{
	for ( IClient* client = (IClient*)m_Clients.GetFirst(); client; client = (IClient*)m_Clients.GetNext() )
		client->Reconnect();
	m_DemoClient.Reconnect();
}

void Proxy::DisconnectClients( char* reason )
{
	for ( IClient* client = (IClient*)m_Clients.GetFirst(); client; client = (IClient*)m_Clients.GetNext() )
		client->Disconnect( reason );
	m_DemoClient.Disconnect( reason );
}

void Proxy::StopBroadcast( char* message )
{
	if ( message )
		DisconnectClients( COM_VarArgs( "%s\n", message ) );
	else
		DisconnectClients( "HLTV stopped.\n" );
	m_Server->Disconnect();
	m_Server->StopRetry();
	m_World->StopGame();
	m_Master.SendShutdown();
	m_System->SetTitle( "HLTV - offline" );
}

void Proxy::ShutDown()
{
	if ( m_State == MODULE_DISCONNECTED )
		return;
	StopBroadcast( "HLTV Shutdown." );
	m_Master.ShutDown();
	m_Status.ShutDown();
	m_DemoClient.ShutDown();
	if ( m_Director )
		m_Director->ShutDown();
	if ( m_World )
		m_World->ShutDown();
	if ( m_Server )
		m_Server->ShutDown();
	if ( m_Network )
		m_Network->ShutDown();
	m_LoopCommands.Clear( true );
	m_BannList.Clear( true );
	m_InfoRules.Free();
	m_InfoPlayers.Free();
	m_InfoDetails.Free();
	m_InfoInfo.Free();
	m_InfoString.Free();
	ClearResources();
	BaseSystemModule::ShutDown();
	m_System->Printf( "Proxy module shutdown.\n" );
}

void Proxy::FreeResource( resource_t* resource )
{
	if ( resource->data )
		m_System->FreeFile( resource->data );
	free( resource );
}

void Proxy::ClearResources()
{
	resource_t* resource;
	while ( ( resource = (resource_t*)m_Resources.RemoveHead() ) != NULL )
		FreeResource( resource );
}

resource_t* Proxy::LoadResourceFromFile( char* filename, resourcetype_t type )
{
	resource_t* resource = (resource_t*)Mem_ZeroMalloc( sizeof( resource_t ) );
	strncpy( resource->szFileName, filename, sizeof( resource->szFileName ) - 1 );
	resource->type = type;
	resource->data = m_System->LoadFile( resource->szFileName, &resource->nDownloadSize );
	if ( resource->data && resource->nDownloadSize >= 0 )
	{
		MD5_Hash_Mem( resource->rgucMD5_hash, resource->data, resource->nDownloadSize );
		return resource;
	}
	m_System->Printf( "WARNING! Failed to load resource file %s.\n", filename );
	free( resource );
	return NULL;
}

bool Proxy::IsValidPassword( int clientType, char* password )
{
	switch ( clientType )
	{
	case TYPE_CLIENT:
		return !spectatorPassword[0] || !strcmp( spectatorPassword, password );
	case TYPE_PROXY:
		return !proxyPassword[0] || !strcmp( proxyPassword, password );
	case TYPE_COMMENTATOR:
		return adminPassword[0] && !strcmp( adminPassword, password );
	default:
		return false;
	}
}

void Proxy::RunClocks()
{
	double elapsed = m_SystemTime - m_LastClockUpdateTime;
	m_LastClockUpdateTime = m_SystemTime;
	if ( m_World->IsPaused() )
		return;
	elapsed *= clientTimeScale;
	clientProxyTime += elapsed;
	clientWorldTime += elapsed;
	if ( finishingBroadcast )
		return;
	frame_t* first = m_World->GetFirstFrame();
	if ( !first )
		return;
	if ( clientWorldTime > m_World->GetTime() )
	{
		clientWorldTime = m_World->GetTime() - m_ClientDelay;
		return;
	}
	if ( first->time > clientWorldTime + m_ClientDelay )
	{
		m_System->DPrintf( "Proxy::RunClocks: forcing client delay (1).\n" );
		clientWorldTime = first->time - m_ClientDelay;
	}
	else if ( m_World->GetTime() - m_ClientDelay > clientWorldTime + 10.0 )
	{
		m_System->DPrintf( "Proxy::RunClocks: forcing client delay (2).\n" );
		clientWorldTime = m_World->GetTime() - m_ClientDelay;
	}
}

void Proxy::NewServerConnection()
{
	m_IsMaster = m_Server->IsGameServer();
	if ( m_IsMaster && m_ClientDelay > 0.0f )
	{
		m_World->SetBufferSize( m_ClientDelay + m_ClientDelay );
		m_Server->SetDelayReconnect( true );
	}
	else
	{
		m_World->SetBufferSize( 10.0f );
		m_Server->SetDelayReconnect( false );
	}
	for ( resource_t* resource = (resource_t*)m_Resources.GetFirst(); resource;
		resource = (resource_t*)m_Resources.GetNext() )
		m_World->AddResource( resource );
	reconnectRequested = false;
}

bool Proxy::CheckDirectorModule()
{
	if ( m_Director && !strcmp( m_Director->GetModName(), m_World->GetGameDir() ) )
		return true;
#ifdef _WIN32
	char filename[260];
#else
	char filename[4096];
#endif
	sprintf( filename, "%s/dlls/director", m_World->GetGameDir() );
	if ( m_Director )
		m_System->RemoveModule( m_Director );
	m_Director = dynamic_cast<IDirector*>( m_System->GetModule( "director001", filename, "director" ) );
	if ( m_Director )
	{
		m_System->DPrintf( "Using extern director module (%s).\n", filename );
		return true;
	}
	if ( m_System->AddModule( &m_DefaultDirector, "director" ) )
	{
		m_Director = &m_DefaultDirector;
		m_System->DPrintf( "Using internal default director.\n" );
	}
	else
	{
		m_System->Errorf( "Proxy::CheckDirectorModule: failed to add internal director module .\n" );
	}
	return m_Director != NULL;
}

void Proxy::NewGameStarted()
{
	clientWorldTime = 0.0;
	m_LastClockUpdateTime = m_SystemTime;
	clientProxyTime = 0.0;
	finishingBroadcast = false;
	clientTimeScale = 1.0f;
	m_CurrentLoss = 0.0f;
	if ( m_Server->IsDemoFile() )
	{
		if ( m_Director )
		{
			m_System->RemoveModule( m_Director );
			m_Director = NULL;
		}
	}
	else if ( GetDelay() > 0.0f )
	{
		if ( CheckDirectorModule() )
			m_Director->NewGame( m_World, this );
		clientWorldTime = m_World->GetTime() - m_ClientDelay;
	}
	else
	{
		if ( m_Director )
		{
			m_System->RemoveModule( m_Director );
			m_Director = NULL;
		}
		if ( m_IsMaster )
		{
			if ( m_System->AddModule( &m_NullDirector, "nulldirector" ) )
			{
				m_Director = &m_NullDirector;
				m_NullDirector.NewGame( m_World, this );
				m_System->DPrintf( "Using zero delay director.\n" );
			}
			else
			{
				m_System->Errorf( "Proxy::NewGameStarted: failed to add null director module .\n" );
			}
		}
	}
	m_Server->SetDirector( m_Director );
#ifdef _WIN32
	char title[260];
#else
	char title[4096];
#endif
	bool isDemo = m_Server->IsDemoFile();
	char* level = m_World->GetLevelName();
	char* source = isDemo ? m_Server->GetDemoFileName() : m_Server->GetAddress()->ToString();
	snprintf( title, sizeof( title ), "HLTV - %s %s", source, level );
	m_System->SetTitle( title );
}

void Proxy::ReceiveSignal( ISystemModule* module, unsigned int signal, void* )
{
	int serial = module->GetSerial();
	if ( serial == m_Server->GetSerial() )
	{
		switch ( signal )
		{
		case 4: NewServerConnection(); break;
		case 5:
		case 6: finishingBroadcast = true; break;
		case 7: BroadcastRetryMessage(); break;
		}
	}
	else if ( serial == m_World->GetSerial() )
	{
		switch ( signal )
		{
		case 2:
			NewGameStarted();
			ReconnectClients();
			break;
		case 5:
		case 6: BroadcastPaused( signal == 5 ); break;
		case 8: StopBroadcast( "HLTV shutddown." ); break;
		}
	}
}

void Proxy::ExecuteLoopCommands()
{
#ifdef _WIN32
	static float nextCheck = 0.0f;
#else
	static double nextCheck = 0.0;
#endif
	if ( nextCheck > m_SystemTime )
		return;
#ifdef _WIN32
	nextCheck = (float)( m_SystemTime + 0.5 );
#else
	nextCheck = m_SystemTime + 0.5;
#endif
	for ( loopcmd_t* command = (loopcmd_t*)m_LoopCommands.GetFirst(); command;
		command = (loopcmd_t*)m_LoopCommands.GetNext() )
	{
		if ( m_SystemTime > command->lastTime + command->interval )
		{
			m_System->ExecuteString( command->command );
			command->lastTime = (float)m_SystemTime;
		}
	}
}

void Proxy::RunFrame( double time )
{
	float frameTime = (float)( time - m_SystemTime );
	BaseSystemModule::RunFrame( time );
	if ( m_MaxQueries > 0.0f )
	{
		m_MaxFrameQueries = (int)( m_MaxQueries * frameTime );
		if ( m_MaxFrameQueries <= 0 )
			m_MaxFrameQueries = 1;
	}
	else
		m_MaxFrameQueries = 0;
	if ( frameTime > 0.0f )
		m_FPS = 0.99f * m_FPS + 0.01f / frameTime;
	if ( m_SystemTime > m_NextStatusUpdateTime )
		UpdateStatusLine();
	if ( m_SystemTime > m_NextInfoMessagesUpdate )
		UpdateInfoMessages();
	int processed = 0;
	do
	{
		NetPacket* packet = m_Socket->ReceivePacket();
		if ( !packet )
			break;
		if ( !packet->connectionless || packet->data.sizeError )
			m_System->DPrintf( "WARNING! Packet from %s with invalid sequence number.\n", packet->address.ToString() );
		else if ( ProcessConnectionlessMessage( &packet->address, &packet->data ) )
			processed++;
		m_Socket->FreePacket( packet );
	} while ( processed <= 32 );
	ExecuteLoopCommands();
	if ( !m_World->IsActive() )
		return;
	if ( GetDelay() > 0.0f )
	{
		RunClocks();
		if ( finishingBroadcast && clientWorldTime > m_World->GetTime() && !reconnectRequested )
		{
			if ( m_Server->IsConnected() )
				m_Server->Reconnect();
			reconnectRequested = true;
		}
	}
#ifdef _WIN32
	float cheeringDeadline = m_LastCheeringUpdate + 8.0f;
#else
	double cheeringDeadline = m_LastCheeringUpdate + 8.0;
#endif
	if ( m_SystemTime > cheeringDeadline )
	{
		m_CheeringPlayers /= 2;
		m_LastCheeringUpdate = (float)m_SystemTime;
	}
}

void Proxy::WriteHUDMsg( textmessage_t* message, BitBuffer* stream )
{
	DirectorCmd command;
	float position[3] = { message->x, message->y, 0.0f };
	command.SetMessageData( message->effect, COM_PackRGB( message->r1, message->g1, message->b1 ),
		position, message->fadein, message->fadeout, message->holdtime, message->fxtime, message->text );
	command.WriteToStream( stream );
}

bool Proxy::WriteSignonData( int type, BitBuffer* stream )
{
	if ( type != TYPE_CLIENT && type != TYPE_DEMO )
		return false;
	if ( signonCommands[0] )
	{
		stream->WriteByte( svc_stufftext );
		stream->WriteString( COM_VarArgs( "%s\n", signonCommands ) );
	}
	stream->WriteByte( svc_stufftext );
#ifdef _WIN32
	float interpolation = 1.0f / GetMaxUpdateRate() + 0.05f;
	stream->WriteString( COM_VarArgs( "ex_interp %.2f\n", interpolation ) );
#else
	stream->WriteString( COM_VarArgs( "ex_interp %.2f\n", 1.0 / GetMaxUpdateRate() + 0.05f ) );
#endif
	stream->WriteByte( svc_timescale );
	stream->WriteFloat( 1.0f );
	if ( m_BannerTGA )
	{
		DirectorCmd command;
		command.SetBannerData( m_BannerTGA->szFileName );
		command.WriteToStream( stream );
	}
	return true;
}

void Proxy::BroadcastPaused( bool paused )
{
	BitBuffer buffer( 32 );
	buffer.WriteByte( svc_centerprint );
	buffer.WriteString( paused ? "Game was paused" : "Game was unpaused" );
	buffer.WriteByte( svc_setpause );
	buffer.WriteByte( paused );
	Broadcast( buffer.data, buffer.CurrentSize(), GROUP_CLIENT | GROUP_PROXY | GROUP_DEMO | GROUP_UNUSED, true );
}

void Proxy::BroadcastRetryMessage()
{
	BitBuffer buffer( 32 );
	buffer.WriteByte( svc_centerprint );
	buffer.WriteString( "Retrying HLTV connection ..." );
	Broadcast( buffer.data, buffer.CurrentSize(), GROUP_CLIENT | GROUP_PROXY | GROUP_UNUSED, false );
}

void Proxy::ChatCommentator( char*, char* text )
{
	BitBuffer buffer( 144 );
	strncpy( commentatorMessage.text, text, sizeof( commentatorMessage.text ) - 1 );
	commentatorMessage.text[sizeof( commentatorMessage.text ) - 1] = 0;
	COM_RemoveEvilChars( commentatorMessage.text );
	WriteHUDMsg( &commentatorMessage, &buffer );
	Broadcast( buffer.data, buffer.CurrentSize(), GROUP_CLIENT | GROUP_PROXY | GROUP_DEMO | GROUP_UNUSED, true );
}

void Proxy::ChatSpectator( char* name, char* text )
{
	if ( !m_ChatMode || !text )
		return;
	int message = m_World->FindUserMsgByName( "SayText" );
	if ( !message )
		return;
	char string[512] = {};
	COM_RemoveEvilChars( text );
	int length = (int)strlen( text );
	if ( length <= 0 )
		return;
	if ( length > 64 )
		text[64] = 0;
	if ( m_ChatMode == 1 || m_IsMaster )
	{
		snprintf( string + 3, sizeof( string ) - 3, "<%s> %s", name, text );
		length = (int)strlen( string + 3 );
		if ( length > 189 )
			length = 189;
		string[length + 3] = '\n';
		string[length + 4] = 0;
		string[0] = (char)message;
		string[1] = (char)( length + 3 );
		string[2] = 0;
		Broadcast( (unsigned char*)string, length + 5,
			m_ChatMode == 1 ? GROUP_CHAT : GROUP_CHAT | GROUP_PROXY, false );
	}
	else
	{
		snprintf( string, sizeof( string ), "say \"%s\"", text );
		if ( m_Server->IsConnected() )
			m_Server->SendStringCommand( string );
	}
}

void Proxy::IncreaseCheering( int amount )
{
	m_CheeringPlayers += amount;
	float fraction = (float)m_CheeringPlayers / (float)m_Clients.CountElements();
	if ( fraction > 1.0f )
		fraction = 1.0f;
	if ( fraction > m_CheeringThreshold )
	{
		DirectorCmd command;
		BitBuffer buffer( 64 );
		command.SetSoundData( "ambience/goal_1.wav", fraction );
		command.WriteToStream( &buffer );
		Broadcast( buffer.data, buffer.CurrentSize(), GROUP_CLIENT | GROUP_DEMO, true );
		m_CheeringPlayers = 0;
	}
}

void Proxy::UpdateStatusLine()
{
	char activeTime[32];
	strncpy( activeTime, COM_FormatTime( (float)m_World->GetTime() ), sizeof( activeTime ) - 1 );
	activeTime[sizeof( activeTime ) - 1] = 0;
	float in, out;
	m_Network->GetFlowStats( &in, &out );
	m_CurrentLoss = m_Server->GetPacketLoss();
	char text[128];
	snprintf( text, sizeof( text ),
		"%s, Time %s, Delay %.0f, FPS %.0f, Clients %i, In %.1f, Out %.1f, Loss %.2f",
		IsMaster() ? "Master" : "Relay", activeTime, m_ClientDelay, m_FPS, m_Clients.CountElements(),
		in, out, m_CurrentLoss );
	m_System->SetStatusLine( text );
	m_NextStatusUpdateTime = m_SystemTime + 0.25;
}

bool Proxy::IsBanned( NetAddress* address )
{
	for ( NetAddress* banned = (NetAddress*)m_BannList.GetFirst(); banned;
		banned = (NetAddress*)m_BannList.GetNext() )
	{
		if ( address->EqualBase( banned ) )
			return true;
	}
	return false;
}

void Proxy::ReplyPing( NetAddress* to )
{
	m_Socket->OutOfBandPrintf( to, "%c", 'j' );
}

void Proxy::ReplyListen( NetAddress* to )
{
	m_Socket->OutOfBandPrintf( to, "%c 0.0.0.0\n", 'G' );
}

void Proxy::ReplyInfoString( NetAddress* to )
{
	m_System->DPrintf( "Info string request from %s.\n", to->ToString() );
	m_Socket->SendPacket( to, m_InfoString.data, m_InfoString.CurrentSize() );
}

void Proxy::ReplyInfo( NetAddress* to, bool details )
{
	m_System->DPrintf( "General information%srequest from %s.\n", details ? " (detail) " : " ", to->ToString() );
	if ( !m_World->IsActive() )
		return;
	BitBuffer* buffer = details ? &m_InfoDetails : &m_InfoInfo;
	m_Socket->SendPacket( to, buffer->data, buffer->CurrentSize() );
}

void Proxy::ReplyPlayers( NetAddress* to )
{
	m_System->DPrintf( "Player information request from %s.\n", to->ToString() );
	if ( m_World->IsActive() )
		m_Socket->SendPacket( to, m_InfoPlayers.data, m_InfoPlayers.CurrentSize() );
}

void Proxy::ReplyRules( NetAddress* to )
{
	m_System->DPrintf( "Rules information request from %s.\n", to->ToString() );
	if ( m_World->IsActive() )
		m_Socket->SendPacket( to, m_InfoRules.data, m_InfoRules.CurrentSize() );
}

void Proxy::RejectConnection( NetAddress* to, bool badPassword, const char* format, ... )
{
	char text[1024];
	va_list args;
	va_start( args, format );
	vsnprintf( text, sizeof( text ), format, args );
	va_end( args );
	if ( badPassword )
		m_Socket->OutOfBandPrintf( to, "%cBADPASSWORD", '8' );
	else
		m_Socket->OutOfBandPrintf( to, "%c%s", '9', text );
	m_System->DPrintf( "Rejected connection: %s (%s)\n", text, to->ToString() );
}

void Proxy::DispatchClient( NetAddress* client, NetAddress* proxy )
{
	BitBuffer buffer( 128 );
	buffer.WriteLong( 0xFFFFFFFF );
	buffer.WriteByte( 'L' );
	buffer.WriteString( proxy->ToString() );
	m_Socket->SendPacket( client, buffer.data, buffer.CurrentSize() );
	m_System->DPrintf( "Dispatched client %s to proxy %s.\n", client->ToString(), proxy->ToString() );
}

unsigned int Proxy::GetChallengeNumber( NetAddress* address )
{
	int index = 0;
	int oldest = 0;
	float oldestTime = 1.0e38f;
	for ( ; index < 1024; index++ )
	{
		if ( challenges[index].adr.EqualBase( address ) )
			break;
		if ( oldestTime > challenges[index].time )
		{
			oldestTime = challenges[index].time;
			oldest = index;
		}
	}
	if ( index == 1024 )
	{
		index = oldest;
		unsigned int high = RandomLong( 0, 0xFFFF );
		challenges[index].challenge = ( high << 16 ) | RandomLong( 0, 0xFFFF );
		challenges[index].adr.FromNetAddress( address );
		challenges[index].time = (float)m_SystemTime;
	}
#ifdef _WIN32
	float expiry = challenges[index].time + 40.0f;
#else
	double expiry = challenges[index].time + 40.0;
#endif
	if ( m_SystemTime > expiry )
	{
		unsigned int high = RandomLong( 0, 0xFFFF );
		challenges[index].challenge = ( high << 16 ) | RandomLong( 0, 0xFFFF );
		challenges[index].time = (float)m_SystemTime;
	}
	return challenges[index].challenge;
}

bool Proxy::CheckChallenge( NetAddress* address, unsigned int challenge )
{
	for ( int i = 0; i < 1024; i++ )
	{
		if ( address->EqualBase( &challenges[i].adr ) )
			return challenges[i].challenge == challenge && m_SystemTime - challenges[i].time <= 40.0;
	}
	return false;
}

void Proxy::ReplyChallenge( NetAddress* to )
{
	m_Socket->OutOfBandPrintf( to, "%c00000000 %u %i\n", 'A', GetChallengeNumber( to ), 2 );
	m_System->DPrintf( "Received challenge from %s.\n", to->ToString() );
}

void Proxy::ReplyServiceChallenge( NetAddress* to, char* type )
{
	m_Socket->OutOfBandPrintf( to, "challenge %s %u\n", type, GetChallengeNumber( to ) );
}

void Proxy::SendRcon( NetAddress* to, unsigned int challenge )
{
	if ( !lastrconcommand[0] )
	{
		m_System->Printf( "HLTV proxy::SendRcon: Empty rcon string\n" );
		return;
	}
	if ( to->Equal( &rconAddress ) )
		m_Socket->OutOfBandPrintf( to, "rcon %u \"%s\" %s", challenge, rconPassword, lastrconcommand );
	else
		m_System->Printf( "Unwanted rcon challenge reply from %s\n", to->ToString() );
}

void Proxy::ExecuteRcon( NetAddress* from, char* command )
{
	m_System->Printf( "Executing rcon \"%s\" from %s.\n", command, from->ToString() );
	char output[1024];
	m_System->RedirectOutput( output + 1, sizeof( output ) - 1 );
	m_System->ExecuteString( command );
	m_System->RedirectOutput( NULL, 0 );
	if ( output[1] )
	{
		output[0] = 'l';
		m_Socket->OutOfBandPrintf( from, "%s", output );
	}
}

void Proxy::CMD_Ping( char* cmdLine )
{
	NetAddress address;
	TokenLine params( cmdLine );
	if ( !m_Network->ResolveAddress( params.GetToken( 1 ), &address ) )
	{
		m_System->Printf( "Error! HLTV proxy::CMD_Ping: IP address not valid.\n" );
		return;
	}
	if ( !address.m_Port )
		address.SetPort_( 27015 );
	m_Socket->OutOfBandPrintf( &address, "ping" );
}

void Proxy::CMD_Say( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
	{
		m_System->Printf( "Syntax: say <text>\n" );
		return;
	}
	if ( m_Server->IsConnected() )
	{
		char string[1024];
		snprintf( string, sizeof( string ), "say \"%s\"", params.GetRestOfLine( 1 ) );
		m_Server->SendStringCommand( string );
	}
}

void Proxy::CMD_MaxClients( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		if ( !SetMaxClients( atoi( params.GetToken( 1 ) ) ) )
			m_System->Printf( "Allowed maximum number of local clients is %i.\n", 255 );
	}
	else
	{
		m_System->Printf( "Syntax: maxclients <number>\n" );
		m_System->Printf( "Current number of maximal clients is %i.\n", GetMaxClients() );
	}
}

void Proxy::CMD_Delay( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		SetDelay( (float)atof( params.GetToken( 1 ) ) );
	else
	{
		m_System->Printf( "Syntax: delay <seconds>\n" );
		m_System->Printf( "Current spectator delay is %.1f seconds.\n", m_ClientDelay );
	}
}

void Proxy::CMD_Connect( char* cmdLine )
{
	TokenLine params( cmdLine );
	NetAddress address;
	if ( !m_Network->ResolveAddress( params.GetToken( 1 ), &address ) )
	{
		m_System->Printf( "Error! HLTV proxy::ConnectToServer: couldn't resolve server address.\n" );
		return;
	}
	if ( !address.m_Port )
		address.SetPort_( 27015 );
	Reset();
	m_Server->Connect( m_World, &address, m_Socket );
}

void Proxy::CMD_Name( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
	{
		m_System->Printf( "Current name is \"%s\".\n", m_Server->GetPlayerName() );
		return;
	}
	if ( strlen( params.GetToken( 1 ) ) - 1 > 30 )
	{
		m_System->Printf( "Invalid name length.\n" );
		return;
	}
	char name[32];
	strncpy( name, params.GetToken( 1 ), sizeof( name ) - 1 );
	name[sizeof( name ) - 1] = 0;
	m_Server->SetPlayerName( name );
}

void Proxy::CMD_Protocol( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		if ( !m_Server->SetProtocol( atoi( params.GetToken( 1 ) ) ) )
			m_System->Printf( "Protocol version not supported!\n" );
	}
	else
	{
		m_System->Printf( "Syntax: protocol <46|47>\n" );
		m_System->Printf( "Current protcol version is %i\n", m_Server->GetProtocol() );
	}
}

void Proxy::CMD_MaxRate( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		SetMaxRate( atoi( params.GetToken( 1 ) ) );
	else
	{
		m_System->Printf( "Syntax: maxrate <n>\n" );
		m_System->Printf( "Current maximal client rate is %i bytes/sec.\n", maxRate );
	}
}

void Proxy::CMD_ServerCmd( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
		m_System->Printf( "Syntax: servercmd <command>\n" );
	else if ( m_Server->IsConnected() )
		m_Server->SendStringCommand( params.GetRestOfLine( 1 ) );
}

void Proxy::CMD_Rate( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		m_Server->SetRate( atoi( params.GetToken( 1 ) ) );
	else
	{
		m_System->Printf( "Syntax: rate <bytes per second>\n" );
		m_System->Printf( "Current maximal server to HLTV proxy rate is %i bytes/sec.\n", m_Server->GetRate() );
	}
}

void Proxy::CMD_HostName( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		m_World->SetHostName( !strcasecmp( "none", params.GetToken( 1 ) ) ? NULL : params.GetToken( 1 ) );
	else
		m_System->Printf( "Host name is \"%s\"\n", m_World->GetHostName() );
}

void Proxy::CMD_Updaterate( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		SetMaxUpdateRate( atoi( params.GetToken( 1 ) ) );
		m_Server->SetUpdateRate( maxUpdateRate );
	}
	else
	{
		m_System->Printf( "Syntax: updaterate <n>\n" );
		m_System->Printf( "Current update rate is %i packets/sec.\n", m_Server->GetUpdateRate() );
	}
}

void Proxy::CMD_BlockVoice( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		m_Server->SetVoiceBlocking( atoi( params.GetToken( 1 ) ) != 0 );
	else
	{
		m_System->Printf( "Syntax: blockvoice <0|1>\n" );
		m_System->Printf( "Currently voice data is %s.\n", m_Server->IsVoiceBlocking() ? "blocked" : "relayed" );
	}
}

void Proxy::CMD_RconAddress( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		m_Network->ResolveAddress( params.GetToken( 1 ), &rconAddress );
		if ( !rconAddress.m_Port )
			rconAddress.SetPort_( 27015 );
	}
	else
	{
		m_System->Printf( "Syntax: rconaddress <IP:Port>\n" );
		m_System->Printf( "Current remote console address: %s\n", rconAddress.ToString() );
	}
}

void Proxy::CMD_RconPassword( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		strncpy( rconPassword, params.GetToken( 1 ), sizeof( rconPassword ) - 1 );
		rconPassword[sizeof( rconPassword ) - 1] = 0;
	}
	else
		m_System->Printf( "Syntax: rconpassword <string>\n" );
}

void Proxy::CMD_Rcon( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
	{
		m_System->Printf( "Syntax: rcon <string>\n" );
		return;
	}
	if ( !rconPassword[0] )
	{
		m_System->Printf( "Set 'rconpassword' before issuing a rcon command.\n" );
		return;
	}
	if ( !rconAddress.IsValid() && m_Server->IsConnected() )
		rconAddress.FromNetAddress( m_Server->GetAddress() );
	if ( !rconAddress.IsValid() )
	{
		m_System->Printf( "rconaddress not valid.\n" );
		return;
	}
	strncpy( lastrconcommand, params.GetRestOfLine( 1 ), sizeof( lastrconcommand ) - 1 );
	lastrconcommand[sizeof( lastrconcommand ) - 1] = 0;
	m_Socket->OutOfBandPrintf( &rconAddress, "challenge rcon\n" );
}

void Proxy::CMD_ProxyPassword( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() != 2 )
	{
		m_System->Printf( "Syntax: proxypassword <password>\n" );
		return;
	}
	if ( !strcasecmp( params.GetToken( 1 ), "none" ) )
		proxyPassword[0] = 0;
	else
	{
		strncpy( proxyPassword, params.GetToken( 1 ), sizeof( proxyPassword ) - 1 );
		proxyPassword[sizeof( proxyPassword ) - 1] = 0;
	}
}

void Proxy::CMD_MaxQueries( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		m_MaxQueries = (float)atof( params.GetToken( 1 ) );
	else
	{
		m_System->Printf( "Syntax: maxqueries <n>\n" );
		m_System->Printf( "Currently maximum %.0f queries per seconds accepted.\n", m_MaxQueries );
	}
}

void Proxy::CMD_OffLineText( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		strncpy( offLineText, params.GetToken( 1 ), sizeof( offLineText ) - 1 );
		offLineText[sizeof( offLineText ) - 1] = 0;
		COM_RemoveEvilChars( offLineText );
	}
	else
	{
		m_System->Printf( "Syntax: offlinetext <text>\n" );
		m_System->Printf( "Offline info text is \"%s\"\n", offLineText );
	}
}

void Proxy::CMD_AddResource( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 2 )
	{
		m_System->Printf( "Syntax: addresource <filename> <type> [<alias>]\n" );
		return;
	}
	if ( !AddResource( params.GetToken( 1 ), atoi( params.GetToken( 2 ) ),
		params.CountToken() == 3 ? NULL : params.GetToken( 3 ) ) )
		m_System->Printf( "Error! Failed to load resource %s.\n", params.GetToken( 1 ) );
}

void Proxy::CMD_PublicGame( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		m_PublicGame = atoi( params.GetToken( 1 ) ) != 0;
	else
	{
		m_System->Printf( "Syntax: publicgame <0|1>\n" );
		m_System->Printf( "Joingame is %s.\n", m_PublicGame ? "enabled" : "disabled" );
	}
}

void Proxy::CMD_SpectatorPassword( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() != 2 )
	{
		m_System->Printf( "Syntax: spectatorpassword <password>\n" );
		return;
	}
	if ( !strcasecmp( params.GetToken( 1 ), "none" ) )
		spectatorPassword[0] = 0;
	else
	{
		strncpy( spectatorPassword, params.GetToken( 1 ), sizeof( spectatorPassword ) - 1 );
		spectatorPassword[sizeof( spectatorPassword ) - 1] = 0;
	}
}

void Proxy::CMD_PlayDemo( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
		m_System->Printf( "Sytax: playdemo <filename>\n" );
	else
	{
		m_Server->LoadDemo( m_World, params.GetToken( 1 ), true, true );
		m_World->SetBufferSize( 10.0f );
	}
}

void Proxy::CMD_SignOnCommands( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		if ( strlen( params.GetToken( 1 ) ) > 250 )
			m_System->Printf( "Error! String too long (>250)!\n" );
		else
			strncpy( signonCommands, params.GetToken( 1 ), sizeof( signonCommands ) - 1 );
	}
	else
	{
		m_System->Printf( "Syntax: signoncommands <string>\n" );
		m_System->Printf( "Current sign on commands: \"%s\"\n", signonCommands );
	}
}

void Proxy::CMD_AdminPassword( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() != 2 )
	{
		m_System->Printf( "Syntax: adminpassword <password>\n" );
		return;
	}
	if ( !strcasecmp( params.GetToken( 1 ), "none" ) )
		adminPassword[0] = 0;
	else
	{
		strncpy( adminPassword, params.GetToken( 1 ), sizeof( adminPassword ) - 1 );
		adminPassword[sizeof( adminPassword ) - 1] = 0;
	}
}

void Proxy::CMD_MaxLoss( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		SetMaxLoss( (float)atof( params.GetToken( 1 ) ) );
	else
	{
		m_System->Printf( "Syntax: maxloss <f>\n" );
		m_System->Printf( "Current server packet loss limit is %.2f.\n", m_MaxLoss );
	}
}

void Proxy::CMD_Region( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		SetRegion( (unsigned char)atoi( params.GetToken( 1 ) ) );
	else
	{
		m_System->Printf( "Syntax: region <f>\n" );
		m_System->Printf( "Current server region is %i.\n", m_iRegion );
	}
}

void Proxy::CMD_ServerPassword( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
		m_System->Printf( "Sytax: serverpassword <string>\n" );
	else
		m_Server->SetUserInfo( "password", params.GetToken( 1 ) );
}

void Proxy::CMD_Disconnect( char* )
{
	m_Server->Disconnect();
}

void Proxy::CMD_Retry( char* )
{
	m_System->Printf( "Retrying connection...\n" );
	m_Server->Retry();
}

void Proxy::CMD_AutoRetry( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		m_Server->SetAutoRetry( atoi( params.GetToken( 1 ) ) == 1 );
	else
	{
		m_System->Printf( "Syntax: autoretry <0|1>\n" );
		m_System->Printf( "Automatic connection retry is %s.\n", m_Server->GetAutoRetry() ? "enabled" : "disabled" );
	}
}

void Proxy::CMD_BannerFile( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		if ( !strcasecmp( "none", params.GetToken( 1 ) ) )
			m_BannerTGA = NULL;
		else
		{
			m_BannerTGA = AddResource( params.GetToken( 1 ), t_generic, NULL );
			if ( m_BannerTGA )
			{
				snprintf( m_BannerTGA->szFileName, 64, "gfx/temp/%s.tga", COM_BinPrintf( m_BannerTGA->rgucMD5_hash, 16 ) );
			}
			else
				m_System->Printf( "ERROR! Couldn't load banner file.\n" );
		}
	}
	else
	{
		m_System->Printf( "Syntax: bannerfile <file.tga>\n" );
		if ( m_BannerTGA )
			m_System->Printf( "Current HLTV banner file is %s\n", m_BannerTGA->szFileName );
		else
			m_System->Printf( "No HLTV banner file specified.\n" );
	}
}

void Proxy::CMD_CheeringThreshold( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
		m_CheeringThreshold = (float)atof( params.GetToken( 1 ) );
	else
	{
		m_System->Printf( "Syntax: cheeringthreshold <n>\n" );
		m_System->Printf( "Cheering threshold is %.2f.\n", m_CheeringThreshold );
	}
}

void Proxy::CMD_ClearBanns( char* )
{
	m_System->Printf( "Clearing IP bann list (%i entries).\n", m_BannList.CountElements() );
	m_BannList.Clear( true );
}

void Proxy::CMD_Clients( char* cmdLine )
{
	TokenLine params( cmdLine );
	int count = 0;
	for ( IClient* client = (IClient*)m_Clients.GetFirst(); client; client = (IClient*)m_Clients.GetNext() )
	{
		m_System->Printf( "%s", client->GetStatusLine() );
		count++;
	}
	m_System->Printf( "--- Total %i Clients ---\n", count );
}

void Proxy::CMD_Kick( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() != 2 )
	{
		m_System->Printf( "Syntax: kick <Client ID>\n" );
		return;
	}
	int id = atoi( params.GetToken( 1 ) );
	for ( IClient* client = (IClient*)m_Clients.GetFirst(); client; client = (IClient*)m_Clients.GetNext() )
	{
		if ( client->GetSerial() == id )
		{
			client->Disconnect( "You have been kicked.\n" );
			return;
		}
	}
	m_System->Printf( "%i is not a valid client ID.\n", id );
}

void Proxy::CMD_Resources( char* )
{
	for ( resource_t* resource = (resource_t*)m_Resources.GetFirst(); resource;
		resource = (resource_t*)m_Resources.GetNext() )
		m_System->Printf( "File: \"%s\", Size: %i bytes.\n", resource->szFileName, resource->nDownloadSize );
	m_System->Printf( "--- Total %i Resources ---\n", m_Resources.CountElements() );
}

void Proxy::CMD_Players( char* )
{
	InfoString info( 256 );
	int count = 0;
	for ( int i = 0; i < m_World->GetMaxClients(); i++ )
	{
		if ( m_World->GetPlayerInfoString( i, &info ) )
		{
			m_System->Printf( "#%2i \"%s\" %s\n", i, info.ValueForKey( "name" ), info.ValueForKey( "model" ) );
			count++;
		}
	}
	m_System->Printf( "--- Total %i Players ---\n", count );
}

void Proxy::CMD_Status( char* )
{
	float in, out;
	m_Network->GetFlowStats( &in, &out );
	float loss = m_Server->GetPacketLoss();
	m_System->Printf( "--- HLTV Status ---\n" );
#ifdef _WIN32
	char* os = "Win32";
#else
	char* os = "Linux";
#endif
	m_System->Printf( "Online %s, FPS %.1f, Version %i (%s)\n", COM_FormatTime( (float)m_System->GetTime() ),
		m_FPS, COM_BuildNumber(), os );
	m_System->Printf( "Local IP %s, Network In %.1f, Out %.1f, Loss %.2f\n",
		m_Network->GetLocalAddress()->ToString(), in, out, loss );
	int spectators, proxies, slots;
	CountLocalClients( spectators, proxies );
	slots = GetMaxClients();
	m_System->Printf( "Local Slots %i, Spectators %i (max %i), Proxies %i\n",
		slots, spectators, m_MaxSeenClients, proxies );
	if ( m_Server->IsConnected() )
	{
		m_Status.GetGlobalStats( proxies, slots, spectators );
		m_System->Printf( "Total Slots %i, Spectators %i (max %i), Proxies %i\n",
			slots, spectators, m_Status.GetMaxSpectatorNumber(), proxies );
		if ( m_Server->IsDemoFile() )
			m_System->Printf( "Playing Demo File \"%s\"\n", m_Server->GetDemoFileName() );
		else if ( m_Server->IsGameServer() )
		{
			m_System->Printf( "Connected to Game Server %s, Delay %.0f\n", m_Server->GetAddress()->ToString(), m_ClientDelay );
			m_System->Printf( "Server Name \"%s\"\n", m_Server->GetHostName() );
		}
		else if ( m_Server->IsRelayProxy() )
		{
			m_System->Printf( "Connected to HLTV Proxy %s\n", m_Server->GetAddress()->ToString() );
			m_System->Printf( "Proxy Name \"%s\"\n", m_Server->GetHostName() );
		}
		else
			m_System->Printf( "Not connected.\n" );
	}
	else
		m_System->Printf( "Not connected.\n" );
	if ( m_World->IsActive() )
	{
		char activeTime[32];
		strncpy( activeTime, COM_FormatTime( (float)m_World->GetTime() ), sizeof( activeTime ) - 1 );
		activeTime[sizeof( activeTime ) - 1] = 0;
		m_System->Printf( "Game Time %s, Mod \"%s\", Map \"%s\", Players %i\n", activeTime,
			m_World->GetGameDir(), m_World->GetLevelName() + 5, m_World->GetNumPlayers() );
	}
	if ( m_DemoClient.IsActive() )
	{
		DemoFile* demo = m_DemoClient.GetDemoFile();
		m_System->Printf( "Recording to %s, Length %.1f sec.\n", demo->GetFileName(), demo->GetDemoTime() );
	}
}

static void ScanMessageColor( char* text, unsigned char* color )
{
	unsigned int values[4];
	int converted = sscanf( text, "%2x%2x%2x%2x", &values[0], &values[1], &values[2], &values[3] );
	for ( int i = 0; i < converted; i++ )
		memcpy( color + i, &values[i], sizeof( values[i] ) );
}

void Proxy::CMD_Msg( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
	{
		m_System->Printf( "Syntax: msg <text> [<duration> <pos x> <pos y> <color hex rgba>]\n" );
		return;
	}
	strncpy( localMessage.text, params.GetToken( 1 ), sizeof( localMessage.text ) - 1 );
	localMessage.text[sizeof( localMessage.text ) - 1] = 0;
	if ( params.CountToken() == 6 )
	{
		localMessage.holdtime = (float)atof( params.GetToken( 2 ) );
		localMessage.x = (float)atof( params.GetToken( 3 ) );
		localMessage.y = (float)atof( params.GetToken( 4 ) );
		ScanMessageColor( params.GetToken( 5 ), (unsigned char*)&localMessage + offsetof( textmessage_t, r1 ) );
	}
	BitBuffer buffer( 144 );
	WriteHUDMsg( &localMessage, &buffer );
	Broadcast( buffer.data, buffer.CurrentSize(), GROUP_CLIENT | GROUP_PROXY | GROUP_DEMO | GROUP_UNUSED, false );
}

void Proxy::CMD_LocalMsg( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
	{
		m_System->Printf( "Syntax: localmsg <text> [<duration> <pos x> <pos y> <color hex rgba>]\n" );
		return;
	}
	strncpy( localMessage.text, params.GetToken( 1 ), sizeof( localMessage.text ) - 1 );
	localMessage.text[sizeof( localMessage.text ) - 1] = 0;
	if ( params.CountToken() == 6 )
	{
		localMessage.holdtime = (float)atof( params.GetToken( 2 ) );
		localMessage.x = (float)atof( params.GetToken( 3 ) );
		localMessage.y = (float)atof( params.GetToken( 4 ) );
		ScanMessageColor( params.GetToken( 5 ), (unsigned char*)&localMessage + offsetof( textmessage_t, r1 ) );
	}
	BitBuffer buffer( 144 );
	WriteHUDMsg( &localMessage, &buffer );
	Broadcast( buffer.data, buffer.CurrentSize(), GROUP_CLIENT | GROUP_DEMO, false );
}

void Proxy::CMD_Record( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		if ( m_DemoClient.Connect( NULL, NULL, "" ) )
			m_DemoClient.SetFileName( params.GetToken( 1 ) );
	}
	else
	{
		m_System->Printf( "Syntax: record <filename>\n" );
		m_System->Printf( "Output format is \"filename-date-map.dem\".\n" );
	}
}

void Proxy::CMD_StopRecording( char* )
{
	if ( m_DemoClient.IsActive() )
		m_DemoClient.Disconnect( "End of Record" );
	else
		m_System->Printf( "Not recording.\n" );
}

void Proxy::CMD_Stop( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 1 )
		StopBroadcast( "HLTV stopped." );
	else
		StopBroadcast( params.GetRestOfLine( 1 ) );
	m_System->DPrintf( "Disconnected and stoppted.\n" );
}

void Proxy::CMD_LoopCmd( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 3 )
	{
		m_System->Printf( "Syntax: loopcmd <id> <seconds> <commands>\n" );
		m_System->Printf( "Currently %i looping commands in list.\n", m_LoopCommands.CountElements() );
		return;
	}
	int id = atoi( params.GetToken( 1 ) );
	float seconds = (float)atof( params.GetToken( 2 ) );
	char* text = params.GetRestOfLine( 3 );
	loopcmd_t* command;
	for ( command = (loopcmd_t*)m_LoopCommands.GetFirst(); command;
		command = (loopcmd_t*)m_LoopCommands.GetNext() )
	{
		if ( command->id == id )
			break;
	}
	if ( !strcasecmp( text, "none" ) || seconds == 0.0f )
	{
		if ( command )
		{
			m_LoopCommands.Remove( command );
			free( command );
		}
		else
			m_System->Printf( "Couldn't remove loop command %i,\n", id );
		return;
	}
	if ( !command )
	{
		command = (loopcmd_t*)Mem_ZeroMalloc( sizeof( loopcmd_t ) );
		command->id = id;
		m_LoopCommands.Add( command );
	}
	command->interval = seconds;
	command->lastTime = 0.0f;
	strncpy( command->command, text, sizeof( command->command ) );
	command->command[sizeof( command->command ) - 1] = 0;
}

resource_s* Proxy::AddResource( char* filename, int, char* name )
{
	resource_t* resource = GetResource( filename );
	if ( resource )
	{
		FreeResource( resource );
		m_Resources.Remove( resource );
	}
	resource = LoadResourceFromFile( filename, t_generic );
	if ( !resource )
		return NULL;
	if ( name )
		strcpy( resource->szFileName, name );
	m_Resources.Add( resource );
	return resource;
}

void Proxy::ReplyConnect( NetAddress* to, int protocol, int challenge, char*, char* userinfo )
{
	static const char* clientTypeString[] =
	{
		"Spectator", "Relay Proxy", "Director", "Commentator", "Fake Client"
	};
	InfoString info( userinfo );
	int type = atoi( info.ValueForKey( "*hltv" ) );
	char* name = info.ValueForKey( "name" );
	NetAddress relayProxy;
	if ( protocol != PROTOCOL_VERSION )
	{
		RejectConnection( to, false, "This HLTV proxy is using protocol %i which is incompatible with yours (%i).\n",
			PROTOCOL_VERSION, protocol );
		return;
	}
	if ( !CheckChallenge( to, challenge ) )
	{
		RejectConnection( to, false, "Challenge number invalid.\n" );
		return;
	}
	if ( !m_World->IsActive() )
	{
		RejectConnection( to, false, "HLTV not started. %s\n", offLineText );
		return;
	}
	if ( maxclients <= 0 )
	{
		RejectConnection( to, false, "HLTV proxy disabled.\n" );
		return;
	}
	if ( m_DispatchMode && type == TYPE_CLIENT )
	{
		float ratio = m_Status.GetBestRelayProxy( &relayProxy );
		float localRatio = (float)m_Clients.CountElements() / (float)maxclients * 1.25f;
		if ( localRatio > 1.0f )
			localRatio = 1.0f;
		if ( ratio < 0.0f )
		{
			if ( m_DispatchMode == 2 )
			{
				RejectConnection( to, false, "HLTV network is full.\n" );
				return;
			}
		}
		else if ( localRatio >= ratio || m_DispatchMode == 2 )
		{
			DispatchClient( to, &relayProxy );
			return;
		}
	}
	if ( !IsValidPassword( type, info.ValueForKey( "password" ) ) )
	{
		RejectConnection( to, true, "Bad password" );
		return;
	}
	if ( IsStressed() && type == TYPE_CLIENT )
	{
		RejectConnection( to, false, "Workload limit exceeded." );
		return;
	}
	IClient* client;
	for ( client = (IClient*)m_Clients.GetFirst(); client; client = (IClient*)m_Clients.GetNext() )
	{
		if ( to->Equal( client->GetAddress() ) )
			break;
	}
	if ( client )
		m_System->Printf( "%s reconnected (%s at %s).\n", clientTypeString[type], name, to->ToString() );
	else
	{
		if ( m_Clients.CountElements() >= maxclients )
		{
			RejectConnection( to, false, maxclients ? "HLTV proxy is full.\n" : "HLTV proxy is disabled.\n" );
			return;
		}
		ProxyClient* proxyClient = new ProxyClient( this );
		if ( !m_System->AddModule( proxyClient, to->ToString() ) )
		{
			free( proxyClient );
			RejectConnection( to, false, "HLTV proxy is overloaded.\n" );
			return;
		}
		client = proxyClient;
		client->SetWorld( m_World );
		m_Clients.AddHead( client );
		m_System->Printf( "%s connected (%s at %s).\n", clientTypeString[type], name, to->ToString() );
	}
	if ( !client->Connect( m_Socket, to, userinfo ) )
	{
		RejectConnection( to, false, "Connection rejected.\n" );
		m_System->RemoveModule( client );
		m_System->Printf( "Refused director connection for %s.\n", to->ToString() );
		return;
	}
	int spectators, proxies;
	CountLocalClients( spectators, proxies );
	if ( m_MaxSeenClients < spectators )
		m_MaxSeenClients = spectators;
}

bool Proxy::ProcessConnectionlessMessage( NetAddress* from, BitBuffer* stream )
{
	TokenLine command;
	if ( IsBanned( from ) )
		return false;
	if ( !command.SetLine( stream->ReadStringLine() ) )
	{
		m_System->DPrintf( "WARNING! HLTV proxy::ProcessConnectionlessMessage: message too long.\n" );
		return false;
	}
	int count = command.CountToken();
	if ( !count )
	{
		m_System->DPrintf( "WARNING! Invalid packet from %s.\n", from->ToString() );
		return false;
	}
	if ( SteamGameServer() )
	{
		unsigned int header = 0xFFFFFFFF;
		char fullBuffer[sizeof( header ) + 4096];
		memcpy( fullBuffer, &header, sizeof( header ) );
		int length = stream->maxSize;
		if ( length > 4096 )
			length = 4096;
		memcpy( fullBuffer + sizeof( header ), stream->data, length );
		SteamGameServer()->HandleIncomingPacket( fullBuffer, length + sizeof( header ),
			ntohl( *(unsigned int*)from->m_IP ), htons( from->m_Port ) );
	}
	char* token = command.GetToken( 0 );
	if ( token[0] == 'A' )
	{
		m_System->DPrintf( "Unwanted challenge response from %s.\n", from->ToString() );
		return false;
	}
	if ( !strcmp( token, "challenge" ) )
	{
		char* type = command.GetToken( 1 );
		if ( count == 2 )
			ReplyServiceChallenge( from, type );
		else if ( count == 3 )
		{
			if ( type && type[0] && !strcasecmp( type, "rcon" ) )
				SendRcon( from, strtoul( command.GetToken( 2 ), NULL, 10 ) );
			else
			{
				m_System->Printf( "Invalid challenge type (%s) from: %s\n", type ? type : "NULL", from->ToString() );
				return false;
			}
		}
		else
		{
			m_System->Printf( "Invalid challenge request from: %s\n", from->ToString() );
			return false;
		}
		return true;
	}
	if ( !strcmp( token, "rcon" ) )
	{
		if ( count <= 3 )
			return false;
		unsigned int challenge = strtoul( command.GetToken( 1 ), NULL, 10 );
		char* password = command.GetToken( 2 );
		char* text = command.GetRestOfLine( 3 );
		if ( !CheckChallenge( from, challenge ) )
		{
			m_System->Printf( "Invalid rcon challenge from: %s\n", from->ToString() );
			return false;
		}
		if ( !adminPassword[0] || strcmp( adminPassword, password ) )
		{
			m_System->Printf( "Invalid rcon password from: %s\n", from->ToString() );
			return false;
		}
		ExecuteRcon( from, text );
		return true;
	}
	if ( !strcmp( token, "getchallenge" ) )
	{
		ReplyChallenge( from );
		return true;
	}
	if ( !strcmp( token, "connect" ) )
	{
		if ( count == 5 )
			ReplyConnect( from, atoi( command.GetToken( 1 ) ), atoi( command.GetToken( 2 ) ),
				command.GetToken( 3 ), command.GetToken( 4 ) );
		else
			RejectConnection( from, false, "Insufficient connection info\n" );
		return true;
	}
	switch ( token[0] )
	{
	case 'T': ReplyInfo( from, true ); return true;
	case 'U': ReplyPlayers( from ); return true;
	case 'V': ReplyRules( from ); return true;
	case 'j':
		if ( !token[1] || token[1] == '\n' )
		{
			m_System->DPrintf( "Acknowledgment from %s.\n", from->ToString() );
			return true;
		}
		break;
	}
	if ( !strcmp( token, "listen" ) )
	{
		ReplyListen( from );
		return true;
	}
	if ( m_MaxFrameQueries <= 0 )
		return false;
	if ( !strcmp( token, "ping" ) || ( token[0] == 'i' && ( !token[1] || token[1] == '\n' ) ) )
		ReplyPing( from );
	else if ( !strcasecmp( token, "infostring" ) )
		ReplyInfoString( from );
	else if ( !strcasecmp( token, "info" ) )
		ReplyInfo( from, false );
	else if ( !strcasecmp( token, "details" ) )
		ReplyInfo( from, true );
	else if ( !strcasecmp( token, "players" ) )
		ReplyPlayers( from );
	else if ( !strcasecmp( token, "rules" ) )
		ReplyRules( from );
	else if ( !strcasecmp( token, "log" ) )
	{
		m_System->Printf( "Ignoring log from %s.\n", from->ToString() );
		return false;
	}
	else if ( token[0] == 'l' )
	{
		stream->Reset();
		stream->SkipBytes( 1 );
		m_System->Printf( ">%s\n", stream->ReadString() );
	}
	else
	{
		m_System->DPrintf( "Proxy::ProcessConnectionlessMessage: unknown \"%s\" from %s \n", token, from->ToString() );
		return false;
	}
	m_MaxFrameQueries--;
	return true;
}

void Proxy::CreateServerInfoString( InfoString* info )
{
	int proxies, slots, spectators;
	m_Status.GetLocalStats( proxies, slots, spectators );
	char address[256];
	snprintf( address, sizeof( address ), "%s", m_Network->GetLocalAddress()->ToString() );
	info->SetValueForKey( "protocol", COM_VarArgs( "%i", PROTOCOL_VERSION ) );
	info->SetValueForKey( "address", address );
	info->SetValueForKey( "players", COM_VarArgs( "%i", spectators ) );
	info->SetValueForKey( "proxy", COM_VarArgs( "%i", 2 - IsMaster() ) );
	if ( IsPublicGame() )
		info->SetValueForKey( "proxyaddress", m_World->GetGameServerAddress()->ToString() );
	info->SetValueForKey( "lan", COM_VarArgs( "%i", IsLanOnly() ) );
	info->SetValueForKey( "max", COM_VarArgs( "%i", slots ) );
	if ( m_World->IsActive() )
	{
		char mapName[260], gameDir[260];
		COM_FileBase( m_World->GetLevelName(), mapName );
		COM_FileBase( m_World->GetGameDir(), gameDir );
		serverinfo_t* serverInfo = m_World->GetServerInfo();
		info->SetValueForKey( "gamedir", gameDir );
		info->SetValueForKey( "description", serverInfo->description );
		info->SetValueForKey( "hostname", m_World->GetHostName() );
		info->SetValueForKey( "map", mapName );
		if ( serverInfo->mod )
		{
			info->SetValueForKey( "mod", "1" );
			info->SetValueForKey( "modversion", COM_VarArgs( "%i", serverInfo->ver ) );
		}
	}
	else
	{
		info->SetValueForKey( "gamedir", "valve" );
		info->SetValueForKey( "description", "HLTV" );
		info->SetValueForKey( "hostname", offLineText );
		info->SetValueForKey( "map", "none" );
	}
	info->SetValueForKey( "type", "p" );
	info->SetValueForKey( "password", COM_VarArgs( "%i", IsPasswordProtected() ) );
#ifdef _WIN32
	info->SetValueForKey( "os", "w" );
#else
	info->SetValueForKey( "os", "l" );
#endif
	info->SetValueForKey( "secure", "0" );
}

void Proxy::UpdateInfoMessages()
{
	serverinfo_t* serverInfo = m_World->GetServerInfo();
	InfoString info( 2048 );
	int proxies, slots, spectators;
	m_Status.GetLocalStats( proxies, slots, spectators );
	char address[32];
	snprintf( address, sizeof( address ), "%s:%i", m_Network->GetLocalAddress()->ToBaseString(), m_Socket->GetPort() );
	m_InfoInfo.Clear();
	if ( m_World->IsActive() )
	{
		m_InfoInfo.WriteLong( 0xFFFFFFFF );
		m_InfoInfo.WriteByte( 'C' );
		m_InfoInfo.WriteString( address );
		m_InfoInfo.WriteString( m_World->GetHostName() );
		m_InfoInfo.WriteString( serverInfo->map );
		m_InfoInfo.WriteString( serverInfo->gamedir );
		m_InfoInfo.WriteString( serverInfo->description );
		m_InfoInfo.WriteByte( spectators <= 255 ? spectators : 255 );
		m_InfoInfo.WriteByte( slots <= 255 ? slots : 255 );
		m_InfoInfo.WriteByte( PROTOCOL_VERSION );
		if ( spectators > 254 || slots > 254 )
		{
			m_InfoInfo.WriteLong( spectators );
			m_InfoInfo.WriteLong( slots );
		}
	}
	m_InfoDetails.Clear();
	if ( m_World->IsActive() )
	{
		m_InfoDetails.WriteLong( 0xFFFFFFFF );
		m_InfoDetails.WriteByte( 'm' );
		m_InfoDetails.WriteString( address );
		m_InfoDetails.WriteString( m_World->GetHostName() );
		m_InfoDetails.WriteString( serverInfo->map );
		m_InfoDetails.WriteString( serverInfo->gamedir );
		m_InfoDetails.WriteString( serverInfo->description );
		m_InfoDetails.WriteByte( spectators <= 255 ? spectators : 255 );
		m_InfoDetails.WriteByte( slots <= 255 ? slots : 255 );
		m_InfoDetails.WriteByte( PROTOCOL_VERSION );
		m_InfoDetails.WriteByte( 'p' );
#ifdef _WIN32
		m_InfoDetails.WriteByte( 'w' );
#else
		m_InfoDetails.WriteByte( 'l' );
#endif
		m_InfoDetails.WriteByte( IsPasswordProtected() );
		m_InfoDetails.WriteByte( serverInfo->mod );
		if ( serverInfo->mod )
		{
			m_InfoDetails.WriteString( serverInfo->url_info );
			m_InfoDetails.WriteString( serverInfo->url_dl );
			m_InfoDetails.WriteString( serverInfo->hlversion );
			m_InfoDetails.WriteLong( serverInfo->ver );
			m_InfoDetails.WriteLong( serverInfo->size );
			m_InfoDetails.WriteByte( serverInfo->svonly );
			m_InfoDetails.WriteByte( serverInfo->cldll );
		}
		m_InfoDetails.WriteByte( 0 );
		if ( spectators > 254 || slots > 254 )
		{
			m_InfoDetails.WriteLong( spectators );
			m_InfoDetails.WriteLong( slots );
		}
	}
	m_InfoRules.Clear();
	if ( m_World->IsActive() )
	{
		m_InfoRules.WriteLong( 0xFFFFFFFF );
		m_InfoRules.WriteByte( 'E' );
		m_InfoRules.WriteShort( 5 );
		m_InfoRules.WriteString( "GameServer" );
		if ( m_Server->IsDemoFile() )
			m_InfoRules.WriteString( m_Server->GetDemoFileName() );
		else if ( IsPublicGame() )
			m_InfoRules.WriteString( m_World->GetGameServerAddress()->ToString() );
		else
			m_InfoRules.WriteString( "Private Server" );
		m_InfoRules.WriteString( "HLTVProxy" );
		m_InfoRules.WriteString( COM_VarArgs( "%i", m_IsMaster ? 1 : 2 ) );
		m_InfoRules.WriteString( "HLTVDelay" );
		m_InfoRules.WriteString( COM_VarArgs( "%.1f", m_ClientDelay ) );
		m_InfoRules.WriteString( "HLTVChat" );
		m_InfoRules.WriteString( COM_VarArgs( "%i", m_ChatMode ) );
		m_InfoRules.WriteString( "HLTVDemo" );
		m_InfoRules.WriteString( m_DemoClient.IsActive() ? m_DemoClient.GetDemoFile()->GetFileName() : "none" );
	}
	m_InfoPlayers.Clear();
	if ( m_World->GetNumPlayers() > 0 )
	{
		m_InfoPlayers.WriteLong( 0xFFFFFFFF );
		m_InfoPlayers.WriteByte( 'D' );
		m_InfoPlayers.WriteByte( 0 );
		int count = 0;
		for ( int i = 0; i < m_World->GetMaxClients(); i++ )
		{
			if ( m_World->GetPlayerInfoString( i, &info ) )
			{
				m_InfoPlayers.WriteByte( ++count );
				m_InfoPlayers.WriteString( info.ValueForKey( "name" ) );
				m_InfoPlayers.WriteLong( 0 );
				m_InfoPlayers.WriteFloat( 1.0f );
			}
		}
		m_InfoPlayers.data[5] = (unsigned char)count;
	}
	CreateServerInfoString( &info );
	m_InfoString.Clear();
	m_InfoString.WriteLong( 0xFFFFFFFF );
	m_InfoString.WriteString( "infostringresponse" );
	m_InfoString.WriteString( info.GetString() );
	m_NextInfoMessagesUpdate = m_SystemTime + 1.0;
}

bool Proxy::Init( IBaseSystem* system, int serial, char* name )
{
	BaseSystemModule::Init( system, serial, name );
	if ( name )
	{
		strncpy( m_Name, name, sizeof( m_Name ) - 1 );
		m_Name[sizeof( m_Name ) - 1] = 0;
	}
	else
		strcpy( m_Name, "proxy001" );
	maxRate = 20000;
	maxUpdateRate = 20;
	m_IsMaster = false;
	m_System->RegisterCommand( "ping", this, CMD_ID_PING );
	m_System->RegisterCommand( "clients", this, CMD_ID_CLIENTS );
	m_System->RegisterCommand( "kick", this, CMD_ID_KICK );
	m_System->RegisterCommand( "chatmode", this, CMD_ID_CHATMODE );
	m_System->RegisterCommand( "say", this, CMD_ID_SAY );
	m_System->RegisterCommand( "msg", this, CMD_ID_MSG );
	m_System->RegisterCommand( "maxrate", this, CMD_ID_MAXRATE );
	m_System->RegisterCommand( "rcon", this, CMD_ID_RCON );
	m_System->RegisterCommand( "rconaddress", this, CMD_ID_RCONADDRESS );
	m_System->RegisterCommand( "rconpassword", this, CMD_ID_RCONPASSWORD );
	m_System->RegisterCommand( "adminpassword", this, CMD_ID_ADMINPASSWORD );
	m_System->RegisterCommand( "proxypassword", this, CMD_ID_PROXYPASSWORD );
	m_System->RegisterCommand( "spectatorpassword", this, CMD_ID_SPECTATORPASSWORD );
	m_System->RegisterCommand( "signoncommands", this, CMD_ID_SIGNONCOMMANDS );
	m_System->RegisterCommand( "offlinetext", this, CMD_ID_OFFLINETEXT );
	m_System->RegisterCommand( "publicgame", this, CMD_ID_PUBLICGAME );
	m_System->RegisterCommand( "dispatchmode", this, CMD_ID_DISPATCHMODE );
	m_System->RegisterCommand( "cheeringthreshold", this, CMD_ID_CHEERINGTHRESHOLD );
	m_System->RegisterCommand( "localmsg", this, CMD_ID_LOCALMSG );
	m_System->RegisterCommand( "loopcmd", this, CMD_ID_LOOPCMD );
	m_System->RegisterCommand( "connect", this, CMD_ID_CONNECT );
	m_System->RegisterCommand( "disconnect", this, CMD_ID_DISCONNECT );
	m_System->RegisterCommand( "playdemo", this, CMD_ID_PLAYDEMO );
	m_System->RegisterCommand( "delay", this, CMD_ID_DELAY );
	m_System->RegisterCommand( "stop", this, CMD_ID_STOP );
	m_System->RegisterCommand( "record", this, CMD_ID_RECORD );
	m_System->RegisterCommand( "stoprecording", this, CMD_ID_STOPRECORDING );
	m_System->RegisterCommand( "servercmd", this, CMD_ID_SERVERCMD );
	m_System->RegisterCommand( "clientcmd", this, CMD_ID_CLIENTCMD );
	m_System->RegisterCommand( "blockvoice", this, CMD_ID_BLOCKVOICE );
	m_System->RegisterCommand( "name", this, CMD_ID_NAME );
	m_System->RegisterCommand( "rate", this, CMD_ID_RATE );
	m_System->RegisterCommand( "updaterate", this, CMD_ID_UPDATERATE );
	m_System->RegisterCommand( "addresource", this, CMD_ID_ADDRESOURCE );
	m_System->RegisterCommand( "resources", this, CMD_ID_RESOURCES );
	m_System->RegisterCommand( "bannerfile", this, CMD_ID_BANNERFILE );
	m_System->RegisterCommand( "bann", this, CMD_ID_BANN );
	m_System->RegisterCommand( "retry", this, CMD_ID_RETRY );
	m_System->RegisterCommand( "autoretry", this, CMD_ID_AUTORETRY );
	m_System->RegisterCommand( "serverpassword", this, CMD_ID_SERVERPASSWORD );
	m_System->RegisterCommand( "status", this, CMD_ID_STATUS );
	m_System->RegisterCommand( "hostname", this, CMD_ID_HOSTNAME );
	m_System->RegisterCommand( "maxqueries", this, CMD_ID_MAXQUERIES );
	m_System->RegisterCommand( "maxclients", this, CMD_ID_MAXCLIENTS );
	m_System->RegisterCommand( "players", this, CMD_ID_PLAYERS );
	m_System->RegisterCommand( "clearbanns", this, CMD_ID_CLEARBANNS );
	m_System->RegisterCommand( "maxloss", this, CMD_ID_MAXLOSS );
	m_System->RegisterCommand( "region", this, CMD_ID_REGION );
	m_Network = (INetwork*)m_System->GetModule( "network001", "core", NULL );
	if ( !m_Network )
	{
		m_System->Errorf( "Proxy::Init: couldn't load network module.\n" );
		return false;
	}
	char* portString = m_System->CheckParam( "-port" );
	int port = atoi( portString ? portString : "27020" );
	m_Socket = m_Network->CreateSocket( port, false, false );
	if ( !m_Socket )
	{
		m_System->Errorf( "Proxy::Init: Could not create proxy port %i.\n", port );
		return false;
	}
	m_Status.SetProxy( this );
	if ( !m_System->AddModule( &m_Status, "status" ) )
	{
		m_System->Errorf( "Proxy::Init: add status module.\n" );
		return false;
	}
	m_Master.m_Proxy = this;
	if ( !m_System->AddModule( &m_Master, "master" ) )
	{
		m_System->Errorf( "Proxy::Init: add master module.\n" );
		return false;
	}
	m_Server = (IServer*)m_System->GetModule( "server001", "core", NULL );
	if ( !m_Server )
	{
		m_System->Errorf( "Proxy::Init: couldn't load server module.\n" );
		return false;
	}
	m_Server->RegisterListener( this );
	m_Server->SetProxy( this );
	m_Server->SetUserInfo( "hspecs", "0" );
	m_Server->SetUserInfo( "hslots", "0" );
	m_Server->SetGameDirectory( "valve", NULL );
	m_World = (IWorld*)m_System->GetModule( "world001", "core", NULL );
	if ( !m_World )
	{
		m_System->Errorf( "Proxy::Init: couldn't load world module.\n" );
		return false;
	}
	m_World->RegisterListener( this );
	if ( !m_System->AddModule( &m_DemoClient, "demo" ) )
		m_System->Printf( "Proxy::Init: Couldn't create demo client.\n" );
	m_DemoClient.SetProxy( this );
	m_DemoClient.SetWorld( m_World );
	SetDelay( 30.0f );
	SetClientTimeScale( 1.0f );
	m_Director = NULL;
	m_DispatchMode = 1;
	m_ChatMode = 0;
	m_LastClockUpdateTime = 0.0;
	m_CheeringPlayers = 0;
	m_BannerTGA = NULL;
	m_NextStatusUpdateTime = 0.0;
	m_PublicGame = true;
	reconnectRequested = false;
	finishingBroadcast = false;
	m_LastCheeringUpdate = 0.0f;
	m_CheeringThreshold = 0.25f;
	m_MaxLoss = 0.05f;
	m_CurrentLoss = 0.0f;
	m_FPS = 1.0f;
	m_MaxSeenClients = 0;
	maxclients = 128;
	m_MaxQueries = 100.0f;
	m_iRegion = 255;
	m_InfoRules.Resize( 1400 );
	m_InfoPlayers.Resize( 1400 );
	m_InfoDetails.Resize( 1400 );
	m_InfoInfo.Resize( 1400 );
	m_InfoString.Resize( 2080 );
	m_NextInfoMessagesUpdate = 0.0;
	rconAddress.Clear();
	memset( rconPassword, 0, sizeof( rconPassword ) );
	memset( proxyPassword, 0, sizeof( proxyPassword ) );
	memset( spectatorPassword, 0, sizeof( spectatorPassword ) );
	memset( adminPassword, 0, sizeof( adminPassword ) );
	for ( unsigned int i = 0; i < sizeof( challenges ) / sizeof( challenges[0] ); i++ )
	{
		challenges[i].adr.Clear();
		challenges[i].challenge = 0;
		challenges[i].time = 0.0f;
	}
	memset( lastrconcommand, 0, sizeof( lastrconcommand ) );
	memset( signonCommands, 0, sizeof( signonCommands ) );
	m_LoopCommands.Init();
	m_BannList.Init();
	m_Resources.Init();
	memset( &localMessage, 0, sizeof( localMessage ) );
	localMessage.x = -1.0f;
	localMessage.y = -1.0f;
	localMessage.fadein = 0.5f;
	localMessage.fadeout = 2.0f;
	localMessage.r1 = 255;
	localMessage.r2 = 255;
	localMessage.g1 = 160;
	localMessage.g2 = 255;
	localMessage.b1 = 0;
	localMessage.b2 = 255;
	localMessage.a1 = 255;
	localMessage.a2 = 255;
	localMessage.holdtime = 5.0f;
	localMessage.fxtime = 0.0f;
	commentatorMessage = localMessage;
	commentatorMessage.fadein = 0.3f;
	commentatorMessage.fadeout = 1.0f;
	commentatorMessage.r2 = 255;
	commentatorMessage.g2 = 160;
	commentatorMessage.b2 = 0;
	commentatorMessage.a2 = 255;
	strcpy( offLineText, "Game is delayed. Please try again later." );
	m_System->SetTitle( "HLTV - offline" );
	m_System->ExecuteFile( "hltv.cfg" );
	m_State = MODULE_RUNNING;
	m_System->Printf( "Proxy module initialized.\n" );
	return true;
}

void Proxy::ExecuteCommand( int commandID, char* commandLine )
{
	switch ( commandID )
	{
	case CMD_ID_RCON: CMD_Rcon( commandLine ); break;
	case CMD_ID_RCONPASSWORD: CMD_RconPassword( commandLine ); break;
	case CMD_ID_RCONADDRESS: CMD_RconAddress( commandLine ); break;
	case CMD_ID_SAY: CMD_Say( commandLine ); break;
	case CMD_ID_MSG: CMD_Msg( commandLine ); break;
	case CMD_ID_CLIENTS: CMD_Clients( commandLine ); break;
	case CMD_ID_KICK: CMD_Kick( commandLine ); break;
	case CMD_ID_CHATMODE: CMD_ChatMode( commandLine ); break;
	case CMD_ID_PUBLICGAME: CMD_PublicGame( commandLine ); break;
	case CMD_ID_OFFLINETEXT: CMD_OffLineText( commandLine ); break;
	case CMD_ID_ADMINPASSWORD: CMD_AdminPassword( commandLine ); break;
	case CMD_ID_SIGNONCOMMANDS: CMD_SignOnCommands( commandLine ); break;
	case CMD_ID_SPECTATORPASSWORD: CMD_SpectatorPassword( commandLine ); break;
	case CMD_ID_DISPATCHMODE: CMD_DispatchMode( commandLine ); break;
	case CMD_ID_CHEERINGTHRESHOLD: CMD_CheeringThreshold( commandLine ); break;
	case CMD_ID_PING: CMD_Ping( commandLine ); break;
	case CMD_ID_PROXYPASSWORD: CMD_ProxyPassword( commandLine ); break;
	case CMD_ID_MAXRATE: CMD_MaxRate( commandLine ); break;
	case CMD_ID_LOOPCMD: CMD_LoopCmd( commandLine ); break;
	case CMD_ID_MAXCLIENTS: CMD_MaxClients( commandLine ); break;
	case CMD_ID_DISCONNECT: CMD_Disconnect( commandLine ); break;
	case CMD_ID_LOCALMSG: CMD_LocalMsg( commandLine ); break;
	case CMD_ID_CONNECT: CMD_Connect( commandLine ); break;
	case CMD_ID_PLAYDEMO: CMD_PlayDemo( commandLine ); break;
	case CMD_ID_DELAY: CMD_Delay( commandLine ); break;
	case CMD_ID_STOP: CMD_Stop( commandLine ); break;
	case CMD_ID_RECORD: CMD_Record( commandLine ); break;
	case CMD_ID_STOPRECORDING: CMD_StopRecording( commandLine ); break;
	case CMD_ID_SERVERCMD: CMD_ServerCmd( commandLine ); break;
	case CMD_ID_CLIENTCMD: CMD_ClientCmd( commandLine ); break;
	case CMD_ID_BLOCKVOICE: CMD_BlockVoice( commandLine ); break;
	case CMD_ID_NAME: CMD_Name( commandLine ); break;
	case CMD_ID_UPDATERATE: CMD_Updaterate( commandLine ); break;
	case CMD_ID_RATE: CMD_Rate( commandLine ); break;
	case CMD_ID_ADDRESOURCE: CMD_AddResource( commandLine ); break;
	case CMD_ID_RESOURCES: CMD_Resources( commandLine ); break;
	case CMD_ID_BANNERFILE: CMD_BannerFile( commandLine ); break;
	case CMD_ID_BANN: CMD_Bann( commandLine ); break;
	case CMD_ID_ADDFAKECLIENTS: CMD_AddFakeClients( commandLine ); break;
	case CMD_ID_RETRY: CMD_Retry( commandLine ); break;
	case CMD_ID_PLAYERS: CMD_Players( commandLine ); break;
	case CMD_ID_AUTORETRY: CMD_AutoRetry( commandLine ); break;
	case CMD_ID_SERVERPASSWORD: CMD_ServerPassword( commandLine ); break;
	case CMD_ID_STATUS: CMD_Status( commandLine ); break;
	case CMD_ID_HOSTNAME: CMD_HostName( commandLine ); break;
	case CMD_ID_MAXQUERIES: CMD_MaxQueries( commandLine ); break;
	case CMD_ID_CLEARBANNS: CMD_ClearBanns( commandLine ); break;
	case CMD_ID_MAXLOSS: CMD_MaxLoss( commandLine ); break;
	case CMD_ID_PROTOCOL: CMD_Protocol( commandLine ); break;
	case CMD_ID_REGION: CMD_Region( commandLine ); break;
	default:
		m_System->Printf( "ERROR! Proxy::ExecuteCommand: unknown command ID %i.\n", commandID );
		break;
	}
}

void Proxy::CMD_ChatMode( char* cmdLine )
{
	static const char* chatModeString[] = { "OFF", "LOCAL", "GLOBAL" };
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		int mode = atoi( params.GetToken( 1 ) );
		if ( mode > 2 )
			mode = 2;
		else if ( mode < 0 )
			mode = 0;
		m_ChatMode = mode;
	}
	else
	{
		m_System->Printf( "Syntax: chatmode <0|1|2>\n" );
		m_System->Printf( "Currently chat mode is %s.\n", chatModeString[m_ChatMode] );
	}
}

void Proxy::CMD_DispatchMode( char* cmdLine )
{
	static const char* dispatchModeString[] = { "OFF", "AUTO", "ALWAYS" };
	TokenLine params( cmdLine );
	if ( params.CountToken() == 2 )
	{
		int mode = atoi( params.GetToken( 1 ) );
		if ( mode < 0 )
			mode = 0;
		else if ( mode > 2 )
			mode = 2;
		m_DispatchMode = mode;
	}
	else
	{
		m_System->Printf( "Syntax: dispatchmode <0|1|2>\n" );
		m_System->Printf( "Spectator dispatch mode is: %s.\n", dispatchModeString[m_DispatchMode] );
	}
}

void Proxy::CMD_ClientCmd( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() <= 2 )
	{
		m_System->Printf( "Syntax: clientcmd <group> <command>\n" );
		m_System->Printf( "groups: 1 = spectators only, 2 = proxies only, 3 = all\n" );
		return;
	}
	int selection = atoi( params.GetToken( 1 ) );
	char* text = params.GetRestOfLine( 2 );
	if ( strlen( text ) > 100 )
	{
		m_System->Printf( "ERROR! Command string too long.\n" );
		return;
	}
	int group = GROUP_CLIENT | GROUP_PROXY | GROUP_DEMO | GROUP_UNUSED;
	if ( selection == 1 )
		group = GROUP_CLIENT | GROUP_DEMO | GROUP_UNUSED;
	else if ( selection == 2 )
		group = GROUP_PROXY;
	BitBuffer buffer( 128 );
	buffer.WriteByte( svc_stufftext );
	buffer.WriteString( COM_VarArgs( "%s\n", text ) );
	Broadcast( buffer.data, buffer.CurrentSize(), group, true );
}

void Proxy::CMD_Bann( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() != 2 )
	{
		m_System->Printf( "Syntax: bann <IP>\n" );
		m_System->Printf( "Currently %i IPs banned\n", m_BannList.CountElements() );
		return;
	}
	NetAddress* address = new ( Mem_ZeroMalloc( sizeof( NetAddress ) ) ) NetAddress;
	if ( m_Network->ResolveAddress( params.GetToken( 1 ), address ) )
	{
		if ( !IsBanned( address ) )
		{
			m_BannList.Add( address );
			return;
		}
		m_System->Printf( "IP already banned.\n" );
	}
	else
		m_System->Printf( "Couldn't resolve IP \x02%s\"\n", params.GetToken( 1 ) );
	free( address );
}

void Proxy::CMD_AddFakeClients( char* cmdLine )
{
	TokenLine params( cmdLine );
	if ( params.CountToken() != 5 )
	{
		m_System->Printf( "Syntax: addfakeclients <number> <IP:port> <rate> <activity>\n" );
		return;
	}
	int count = atoi( params.GetToken( 1 ) );
	NetAddress address;
	m_Network->ResolveAddress( params.GetToken( 2 ), &address );
	int rate = atoi( params.GetToken( 3 ) );
	(void)atof( params.GetToken( 4 ) );
	for ( int i = 0; i < count; i++ )
	{
		FakeClient* client = new FakeClient;
		if ( !m_System->AddModule( client, "" ) )
		{
			m_System->Errorf( "AddFakeClients: failed to add module.\n" );
			break;
		}
		client->SetRate( rate );
		client->Connect( &address );
	}
}

char* Proxy::GetModVersion( char* )
{
	static char version[16] = "1.1.2.0";
	return version;
}
