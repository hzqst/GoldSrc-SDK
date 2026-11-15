#include "HLTV/common/DemoFile.h"
#include "HLTV/common/NetPacket.h"
#include "HLTV/common/common.h"
#include "HLTV/common/mem.h"
#include "common/IBaseSystem.h"
#include "public/FileSystem.h"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define MAX_POSSIBLE_MSG		( 1 << 16 )

#ifndef _WIN32
static void LowerCaseDemoName( char* str )
{
	size_t i;

	for ( i = 0; strlen( str ) > i; ++i )
	{
		str[i] = (char)tolower( (unsigned char)str[i] );
	}
}
#endif

DemoFile::DemoFile()
{
	m_FileSystem = NULL;
	m_FileHandle = NULL;
	demoChannel = NULL;
	entries = NULL;
	Reset();
}

DemoFile::~DemoFile()
{
}

void DemoFile::Reset()
{
	memset( fileName, 0, sizeof( fileName ) );
	memset( &zeroDemoInfo, 0, sizeof( zeroDemoInfo ) );
	CloseFile();
	continuous = true;
}

bool DemoFile::IsRecording()
{
	return state == 2;
}

void DemoFile::CloseFile()
{
	if ( m_FileHandle != NULL )
	{
		if ( m_FileSystem != NULL )
		{
			if ( state == 2 )
			{
				unsigned char c;
				float f;
				int i;
				int position;

				c = 5;
				m_FileSystem->Write( &c, 1, m_FileHandle );
				f = (float)( m_System->GetTime() - startTime );
				m_FileSystem->Write( &f, 4, m_FileHandle );
				i = frameCount;
				m_FileSystem->Write( &i, 4, m_FileHandle );
				position = m_FileSystem->Tell( m_FileHandle );
				gameEntry.nFileLength = position - gameEntry.nOffset;
				gameEntry.fTrackTime = (float)( m_System->GetTime() - startTime );
				i = 2;
				gameEntry.nFrames = frameCount;
				m_FileSystem->Write( &i, 4, m_FileHandle );
				m_FileSystem->Write( &loadEntry, sizeof( loadEntry ), m_FileHandle );
				m_FileSystem->Write( &gameEntry, sizeof( gameEntry ), m_FileHandle );
				demoheader.nDirectoryOffset = position;
				m_FileSystem->Seek( m_FileHandle, 0, FILESYSTEM_SEEK_HEAD );
				m_FileSystem->Write( &demoheader, sizeof( demoheader ), m_FileHandle );
				m_System->Printf( "Completed demo %s.\n", fileName );
			}

			m_FileSystem->Close( m_FileHandle );
		}
	}

	if ( entries != NULL )
	{
		free( entries );
		entries = NULL;
	}

	m_FileHandle = NULL;
	state = 0;
}

void DemoFile::WriteDemoInfo( demo_info_t* demoInfo )
{
	m_FileSystem->Write( demoInfo, sizeof( demo_info_t ), m_FileHandle );
}

void DemoFile::WriteSequenceInfo()
{
	m_FileSystem->Write( &demoChannel->outgoing_sequence, 4, m_FileHandle );
	m_FileSystem->Write( &demoChannel->incoming_sequence, 4, m_FileHandle );
	m_FileSystem->Write( &demoChannel->last_reliable_sequence, 4, m_FileHandle );
	m_FileSystem->Write( &demoChannel->reliable_sequence, 4, m_FileHandle );
	m_FileSystem->Write( &demoChannel->incoming_acknowledged, 4, m_FileHandle );
	m_FileSystem->Write( &demoChannel->incoming_reliable_sequence, 4, m_FileHandle );
	m_FileSystem->Write( &demoChannel->incoming_reliable_acknowledged, 4, m_FileHandle );
}

void DemoFile::WriteDemoStartup( BitBuffer* buffer )
{
	int size;

	size = buffer->CurrentSize();
	if ( size > 0 )
	{
		unsigned char c;
		float f;
		int i;

		c = 0;
		m_FileSystem->Write( &c, 1, m_FileHandle );
		f = (float)( m_System->GetTime() - startTime );
		m_FileSystem->Write( &f, 4, m_FileHandle );
		i = frameCount;
		m_FileSystem->Write( &i, 4, m_FileHandle );
		m_FileSystem->Write( &zeroDemoInfo, sizeof( zeroDemoInfo ), m_FileHandle );
		WriteSequenceInfo();
		i = size;
		m_FileSystem->Write( &i, 4, m_FileHandle );
		m_FileSystem->Write( buffer->data, size, m_FileHandle );
	}
}

void DemoFile::WriteDemoMessage( BitBuffer* unreliableData, BitBuffer* reliableData )
{
	int size;

	size = unreliableData->CurrentSize() + reliableData->CurrentSize();
	if ( size > 0 )
	{
		if ( m_FileHandle != NULL && m_FileSystem != NULL )
		{
			unsigned char c;
			float f;
			int i;

			++frameCount;
			c = 1;
			m_FileSystem->Write( &c, 1, m_FileHandle );
			f = (float)( m_System->GetTime() - startTime );
			m_FileSystem->Write( &f, 4, m_FileHandle );
			i = frameCount;
			m_FileSystem->Write( &i, 4, m_FileHandle );
			m_FileSystem->Write( &zeroDemoInfo, sizeof( zeroDemoInfo ), m_FileHandle );
			WriteSequenceInfo();
			i = size;
			m_FileSystem->Write( &i, 4, m_FileHandle );
			m_FileSystem->Write( unreliableData->data, unreliableData->CurrentSize(), m_FileHandle );
			m_FileSystem->Write( reliableData->data, reliableData->CurrentSize(), m_FileHandle );
		}
	}
}

void DemoFile::WriteUpdateClientData( client_data_t* cdata )
{
	if ( m_FileHandle != NULL )
	{
		if ( m_FileSystem != NULL )
		{
			unsigned char cmd;
			float f;
			int i;

			cmd = 4;
			m_FileSystem->Write( &cmd, 1, m_FileHandle );
			f = (float)( m_System->GetTime() - startTime );
			m_FileSystem->Write( &f, 4, m_FileHandle );
			i = frameCount;
			m_FileSystem->Write( &i, 4, m_FileHandle );
			m_FileSystem->Write( cdata, sizeof( client_data_t ), m_FileHandle );
		}
	}
}

float DemoFile::GetDemoTime()
{
	return (float)( m_System->GetTime() - startTime );
}

void DemoFile::WriteSignonData()
{
	BitBuffer buffer( 0x20000u );
	int i;

	m_World->WriteNewData( &buffer );
	m_World->WriteSigonData( &buffer );
	buffer.WriteByte( svc_time );
	buffer.WriteFloat( (float)( m_System->GetTime() - startTime ) );

	for ( i = 0; ; ++i )
	{
		if ( i >= m_World->GetMaxClients() )
		{
			break;
		}

		m_World->WriteClientUpdate( &buffer, i );
	}

	m_World->WriteLightStyles( &buffer );
	buffer.WriteByte( svc_signonnum );
	buffer.WriteByte( 1 );
	WriteDemoStartup( &buffer );
}

bool DemoFile::StartRecording( char* newName )
{
	if ( state == 1 )
	{
		return false;
	}

	if ( newName == NULL || m_FileSystem == NULL )
	{
		return false;
	}

	if ( m_FileHandle != NULL )
	{
		CloseFile();
	}

	strncpy( fileName, newName, sizeof( fileName ) - sizeof( ".dem" ) );
	fileName[sizeof( fileName ) - sizeof( ".dem" )] = 0;
	COM_DefaultExtension( fileName, ".dem" );

	if ( fileName[0] == '/'
		|| strstr( fileName, ":" ) != NULL
		|| strstr( fileName, ".." ) != NULL
		|| strstr( fileName, "\\" ) != NULL )
	{
		m_System->Printf( "WARNING! DemoFile::StartRecording: unable to open %s (contains illegal characters).\n",
				fileName );
	}
	else if ( strcmp( COM_LastFileExtension( fileName ), "dem" ) == 0 )
	{
		m_FileHandle = m_FileSystem->Open( fileName, "wb", NULL );
		if ( m_FileHandle != NULL )
		{
			unsigned char b;
			float f;
			int i;

			memset( &demoheader, 0, sizeof( demoheader ) );
#ifdef _WIN32
			memcpy( &demoheader, "HLDEMO", sizeof( "HLDEMO" ) );
#else
			memcpy( demoheader.szFileStamp, "HLDEMO", sizeof( "HLDEMO" ) );
#endif
			COM_FileBase( m_World->GetLevelName(), demoheader.szMapName );
			COM_FileBase( m_World->GetGameDir(), demoheader.szDllDir );
			demoheader.mapCRC = 0;
			demoheader.nDemoProtocol = 5;
			demoheader.nNetProtocolVersion = PROTOCOL_VERSION;
			demoheader.nDirectoryOffset = 0;
			m_FileSystem->Write( &demoheader, sizeof( demoheader ), m_FileHandle );

			memset( &loadEntry, 0, sizeof( loadEntry ) );
			strcpy( loadEntry.szDescription, "LOADING" );
			loadEntry.nEntryType = 0;
			loadEntry.nOffset = m_FileSystem->Tell( m_FileHandle );
			frameCount = 0;
			startTime = (float)m_System->GetTime();
			WriteSignonData();

			b = 5;
			m_FileSystem->Write( &b, 1, m_FileHandle );
			f = (float)( m_System->GetTime() - startTime );
			m_FileSystem->Write( &f, 4, m_FileHandle );
			i = frameCount;
			m_FileSystem->Write( &i, 4, m_FileHandle );
			loadEntry.nFileLength = m_FileSystem->Tell( m_FileHandle ) - loadEntry.nOffset;

			memset( &gameEntry, 0, sizeof( gameEntry ) );
#ifdef _WIN32
			_snprintf( gameEntry.szDescription, sizeof( gameEntry.szDescription ), "Playback" );
#else
			snprintf( gameEntry.szDescription, sizeof( gameEntry.szDescription ), "Playback" );
#endif
			gameEntry.nEntryType = 1;
			gameEntry.nOffset = m_FileSystem->Tell( m_FileHandle );

			b = 2;
			m_FileSystem->Write( &b, 1, m_FileHandle );
			f = (float)( m_System->GetTime() - startTime );
			m_FileSystem->Write( &f, 4, m_FileHandle );
			i = frameCount;
			m_FileSystem->Write( &i, 4, m_FileHandle );

			state = 2;
			m_System->Printf( "Start recording to %s.\n", fileName );
			return true;
		}
		else
		{
			m_System->Printf( "WARNING! DemoFile::StartRecording: couldn't open demo file %s.\n", fileName );
		}
	}
	else
	{
		m_System->Printf(
				"WARNING! DemoFile::StartRecording: unable to open %s (wrong file extension, must be .dem).\n",
				fileName );
	}

	return false;
}

bool DemoFile::IsPlaying()
{
	return state == 1;
}

bool DemoFile::LoadDemo( char* demoname )
{
	int filePos;

	if ( state == 2 )
	{
		m_System->Printf( "Cannot load demo, still recording.\n" );
		return false;
	}

	if ( m_FileSystem == NULL )
	{
		return false;
	}

	CloseFile();
	strncpy( fileName, demoname, sizeof( fileName ) - sizeof( ".dem" ) );
	fileName[sizeof( fileName ) - sizeof( ".dem" )] = 0;
#ifdef _WIN32
	_strlwr( fileName );
#else
	LowerCaseDemoName( fileName );
#endif

	if ( strstr( fileName, ".dem" ) == NULL )
	{
		strcpy( &fileName[strlen( fileName )], ".dem" );
	}

	m_FileHandle = m_FileSystem->Open( fileName, "rb", NULL );
	if ( m_FileHandle == NULL )
	{
		m_System->Printf( "Coudn't open demo file %s.\n", fileName );
		return false;
	}

	memset( &demoheader, 0, sizeof( demoheader ) );
	m_FileSystem->Read( &demoheader, sizeof( demoheader ), m_FileHandle );

	if ( strcmp( demoheader.szFileStamp, "HLDEMO" ) != 0 )
	{
		m_System->Printf( "%s is not a HL demo file.\n", fileName );
		m_FileSystem->Close( m_FileHandle );
		m_FileHandle = NULL;
		return false;
	}

	if ( demoheader.nNetProtocolVersion != PROTOCOL_VERSION || demoheader.nDemoProtocol != 5 )
	{
		m_System->Printf( "WARNING! %s has an outdated demo format.\n", fileName );
	}

	filePos = m_FileSystem->Tell( m_FileHandle );
	m_FileSystem->Seek( m_FileHandle, demoheader.nDirectoryOffset, FILESYSTEM_SEEK_HEAD );
	m_FileSystem->Read( &entryNumber, 4, m_FileHandle );
	currentEntry = 0;

	if ( (unsigned int)( entryNumber - 1 ) <= 0x3FFu )
	{
		entries = (demoentry_t*)Mem_ZeroMalloc( sizeof( demoentry_t ) * entryNumber );
		m_FileSystem->Read( entries, sizeof( demoentry_t ) * entryNumber, m_FileHandle );
		m_FileSystem->Seek( m_FileHandle, entries[currentEntry].nOffset, FILESYSTEM_SEEK_HEAD );
	}
	else
	{
		m_System->Printf( "WARNING! Demo had bogus number of directory entries!\n" );
		entryNumber = 2;
		entries = NULL;
		m_FileSystem->Seek( m_FileHandle, filePos, FILESYSTEM_SEEK_HEAD );
	}

	startTime = (float)m_System->GetTime();
	nextReadTime = startTime;
	continuous = true;
	memset( &serverInfo, 0, sizeof( serverInfo ) );
	strncpy( serverInfo.address, demoChannel->remote_address.ToBaseString(), 0x3Fu );
	serverInfo.address[63] = 0;
	strncpy( serverInfo.name, fileName, 0x4Fu );
	serverInfo.name[79] = 0;
	strncpy( serverInfo.map, demoheader.szMapName, 0xFu );
	serverInfo.map[15] = 0;
	strncpy( serverInfo.gamedir, demoheader.szDllDir, 0xFFu );
	serverInfo.gamedir[255] = 0;
	strncpy( serverInfo.description, "Demo Playback", 0xFFu );
	serverInfo.activePlayers = 0;
	serverInfo.maxPlayers = 32;
	serverInfo.type = 'd';
#ifdef _WIN32
	serverInfo.os = 'w';
#else
	serverInfo.os = 'l';
#endif
	serverInfo.protocol = (unsigned char)demoheader.nNetProtocolVersion;
	serverInfo.description[255] = 0;
	serverInfo.pw = 0;
	serverInfo.mod = false;
	state = 1;
	return true;
}

void DemoFile::StopPlayBack()
{
	if ( state != 1 )
	{
		return;
	}

	CloseFile();

	if ( m_Server != NULL )
	{
		m_Server->Disconnect();
	}
}

void DemoFile::ReadDemoPacket( BitBuffer* demoData, demo_info_t* demoInfo )
{
	int msglen;
	unsigned char msgbuf[MAX_POSSIBLE_MSG];

	msglen = 0;

	if ( m_FileHandle == NULL || ( nextReadTime > m_System->GetTime() && continuous ) )
	{
		return;
	}

	for ( ;; )
	{
		int position;
		unsigned char cmd;
		float time;
		int frame;
		int channel = 0;
		int sampleSize = 0;

		position = m_FileSystem->Tell( m_FileHandle );
		if ( m_FileSystem->Read( &cmd, 1, m_FileHandle ) != 1 )
		{
			if ( state == 1 )
			{
				CloseFile();
				if ( m_Server != NULL )
				{
					m_Server->Disconnect();
				}
			}

			return;
		}

		m_FileSystem->Read( &time, 4, m_FileHandle );
		m_FileSystem->Read( &frame, 4, m_FileHandle );

		if ( cmd != 0 && cmd != 5 )
		{
			nextReadTime = time + startTime;
		}

		if ( nextReadTime > m_System->GetTime() && continuous )
		{
			m_FileSystem->Seek( m_FileHandle, position, FILESYSTEM_SEEK_HEAD );
			return;
		}

		msglen = 0;

		switch ( cmd )
		{
		case 2:
			startTime = (float)m_System->GetTime();
			break;

		case 3:
			msglen = 64;
			break;

		case 4:
			msglen = 32;
			break;

		case 5:
			++currentEntry;
			if ( currentEntry >= entryNumber )
			{
				StopPlayBack();
				return;
			}

			if ( entries != NULL )
			{
				m_FileSystem->Seek( m_FileHandle, entries[currentEntry].nOffset, FILESYSTEM_SEEK_HEAD );
			}

			break;

		case 6:
			msglen = 84;
			break;

		case 7:
			msglen = 8;
			break;

		case 8:
			m_FileSystem->Read( &channel, 4, m_FileHandle );
			m_FileSystem->Read( &sampleSize, 4, m_FileHandle );
			msglen = sampleSize + 16;
			break;

		case 9:
			m_FileSystem->Read( &msglen, 4, m_FileHandle );
			break;

		default:
			m_FileSystem->Read( demoInfo, sizeof( demo_info_t ), m_FileHandle );
			demoInfo->rp.cmd = &demoInfo->cmd;
			demoInfo->rp.movevars = &demoInfo->movevars;
			ReadSequenceInfo();

			if ( m_FileSystem->Read( &msglen, 4, m_FileHandle ) != 4 )
			{
				m_System->DPrintf( "WARNING! DemoFile::ReadDemoPacket: Bad demo length.\n" );
				StopPlayBack();
				return;
			}

			if ( msglen < 0 )
			{
				m_System->Printf( "WARNING! DemoFile::ReadDemoPacket: Demo message length < 0.\n" );
				StopPlayBack();
				return;
			}

			if ( msglen > MAX_POSSIBLE_MSG )
			{
				m_System->Printf( "WARNING! DemoFile::ReadDemoPacket: message length > MAX_POSSIBLE_MSG\n" );
				StopPlayBack();
				return;
			}

			if ( msglen == 0 )
			{
				return;
			}

			if ( m_FileSystem->Read( msgbuf, msglen, m_FileHandle ) != msglen )
			{
				m_System->Printf( "WARNING! DemoFile::ReadDemoPacket: Error reading demo message data.\n" );
				StopPlayBack();
				return;
			}

			{
				NetPacket* packet;

				packet = new NetPacket;
				packet->connectionless = false;
				packet->time = m_System->GetTime();
				packet->seqnr = demoChannel->incoming_sequence;
				packet->data.Resize( (unsigned int)msglen );
				packet->data.WriteBuf( msgbuf, msglen );
				packet->data.Reset();
				demoChannel->incomingPackets.AddHead( packet );
			}

			return;
		}

		if ( msglen == 0 )
		{
			continue;
		}

		m_FileSystem->Read( msgbuf, msglen, m_FileHandle );
		demoData->WriteByte( cmd );

		if ( cmd == 8 )
		{
			demoData->WriteLong( (unsigned int)channel );
			demoData->WriteLong( (unsigned int)sampleSize );
		}
		else if ( cmd == 9 )
		{
			demoData->WriteLong( (unsigned int)msglen );
		}

		demoData->WriteBuf( msgbuf, msglen );
	}
}

void DemoFile::ReadDemoInfo( demo_info_t* demoInfo )
{
	m_FileSystem->Read( demoInfo, sizeof( demo_info_t ), m_FileHandle );
	demoInfo->rp.cmd = &demoInfo->cmd;
	demoInfo->rp.movevars = &demoInfo->movevars;
}

void DemoFile::ReadSequenceInfo()
{
	m_FileSystem->Read( &demoChannel->incoming_sequence, 4, m_FileHandle );
	m_FileSystem->Read( &demoChannel->incoming_acknowledged, 4, m_FileHandle );
	m_FileSystem->Read( &demoChannel->incoming_reliable_acknowledged, 4, m_FileHandle );
	m_FileSystem->Read( &demoChannel->incoming_reliable_sequence, 4, m_FileHandle );
	m_FileSystem->Read( &demoChannel->outgoing_sequence, 4, m_FileHandle );
	m_FileSystem->Read( &demoChannel->reliable_sequence, 4, m_FileHandle );
	m_FileSystem->Read( &demoChannel->last_reliable_sequence, 4, m_FileHandle );
}

char* DemoFile::GetFileName()
{
	return fileName;
}

bool DemoFile::IsContinuous()
{
	return continuous;
}

void DemoFile::SetContinuous( bool enabled )
{
	continuous = enabled;
}

void DemoFile::Init( IWorld* world, IServer* server, NetChannel* channel )
{
	m_World = world;
	m_Server = server;
	m_System = world->GetSystem();
	demoChannel = channel;
	m_FileSystem = m_System->GetFileSystem();
	entries = NULL;
	Reset();
}
