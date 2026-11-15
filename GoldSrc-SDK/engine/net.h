#pragma once

#include "common.h"
#include "cvar.h"
#include "../common/netadr.h"

#define MAX_MSGLEN				6000
#define PACKET_HEADER			10

#define MAX_STREAMS				2

#define FRAG_NORMAL_STREAM		0
#define FRAG_FILE_STREAM		1

#define MAX_ROUTEABLE_PACKET	1400
#define FRAGMENT_SIZE			MAX_ROUTEABLE_PACKET

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifdef _WIN32
typedef enum
{
	NS_CLIENT,
	NS_SERVER,
	NS_MULTICAST
} netsrc_t;
#else
// APIProxy.h declares netsrc_t from enums.h outside Windows.
#include "enums.h"
#endif

extern netadr_t net_local_adr;
extern netadr_t net_from; // address of who sent the packet
extern sizebuf_t net_message;

//============================================================================
#define MAX_FLOWS		2

#define FLOW_OUTGOING	0
#define FLOW_INCOMING	1

#define MAX_LATENT		32

typedef struct flowstats_s
{
	// Size of message sent/received
	int		size;
	// Time that message was sent/received
	double	time;
} flowstats_t;

typedef struct flow_s
{
	// Data for last MAX_LATENT messages
	flowstats_t	stats[MAX_LATENT];
	// Current message position
	int			current;
	// Time when we should recompute k/sec data
	double		nextcompute;

	// Average data
	float		kbytespersec;
	float		avgkbytespersec;
} flow_t;

// Generic fragment structure
typedef struct fragbuf_s
{
	// Next buffer in chain
	struct fragbuf_s*	next;
	// Id of this buffer
	int					bufferid;

	sizebuf_t			frag_message;
	// The actual data sits here
	byte				frag_message_buf[FRAGMENT_SIZE];

	// Is this a file buffer?
	qboolean			isfile;
	// Is this file buffer from memory ( custom decal, etc. ).
	qboolean			isbuffer;
	qboolean			iscompressed;

	// Name of the file to save out on remote host
	char				filename[MAX_PATH];
	// Offset in file from which to read data
	int					foffset;
	// Size of data to read at that offset
	int					size;
} fragbuf_t;

// Waiting list of fragbuf chains
typedef struct fragbufwaiting_s
{
	// Next chain in waiting list
	struct fragbufwaiting_s*	next;
	// Number of buffers in this chain
	int							fragbufcount;
	// The actual buffers
	fragbuf_t*					fragbufs;
} fragbufwaiting_t;

typedef struct netchan_s
{
	// NS_SERVER or NS_CLIENT, depending on channel.
	netsrc_t			sock;
	// Address this channel is talking to.
	netadr_t			remote_address;

	int					player_slot;
	// For timeouts.  Time last message was received.
	float				last_received;
	// Time when channel was connected.
	float				connect_time;

	// Bandwidth choke
	// Bytes per second
	double				rate;
	// If realtime > cleartime, free to send next packet
	double				cleartime;

	// Sequencing variables
	//
	// Increasing count of sequence numbers 
	int					incoming_sequence;
	// # of last outgoing message that has been ack'd.
	int					incoming_acknowledged;
	// Toggles T/F as reliable messages are received.
	int					incoming_reliable_acknowledged;
	// single bit, maintained local
	int					incoming_reliable_sequence;
	// Message we are sending to remote
	int					outgoing_sequence;
	// Whether the message contains reliable payload, single bit
	int					reliable_sequence;
	int					last_reliable_sequence;

	void*				connection_status;
	int					( *pfnNetchan_Blocksize )( void* status );

	sizebuf_t			message;
	byte				message_buf[MAX_MSGLEN - PACKET_HEADER];

	// Reliable message buffer.  We keep adding to it until reliable is acknowledged.  Then we clear it.
	int					reliable_length;
	byte				reliable_buf[MAX_MSGLEN - PACKET_HEADER];

	// Waiting list of buffered fragments to go onto queue.
	// Multiple outgoing buffers can be queued in succession
	fragbufwaiting_t*	waitlist[MAX_STREAMS];

	// Is reliable waiting buf a fragment?
	int					reliable_fragment[MAX_STREAMS];
	// Buffer id for each waiting fragment
	unsigned int		reliable_fragid[MAX_STREAMS];

	// The current fragment being set
	fragbuf_t*			fragbufs[MAX_STREAMS];
	// The total number of fragments in this stream
	int					fragbufcount[MAX_STREAMS];

	// Position in outgoing buffer where frag data starts
	short				frag_startpos[MAX_STREAMS];
	// Length of frag data in the buffer
	short				frag_length[MAX_STREAMS];

	// Incoming fragments are stored here
	fragbuf_t*			incomingbufs[MAX_STREAMS];
	// Set to true when incoming data is ready
	qboolean			incomingready[MAX_STREAMS];
	char				incomingfilename[MAX_PATH];

	void*				tempbuffer;
	int					tempbuffersize;

	// Incoming and outgoing flow metrics
	flow_t				flow[MAX_FLOWS];
} netchan_t;
#define DOWNLOAD_FILENAME_BYTES 256

extern char gDownloadFile[DOWNLOAD_FILENAME_BYTES];

extern cvar_t net_showpackets;

// Start up networking
void			NET_Init();
void			NET_Shutdown();
// Start up/shut down sockets layer
void			NET_Config( qboolean multiplayer );
int				NET_IsConfigured();
void			NET_OpenIP();
void			NET_GetLocalAddress();
void			NET_FlushSocket( netsrc_t source );
void			NET_ClearLagData( qboolean client, qboolean server );
// Retrieve packets from network layer
qboolean		NET_GetPacket( netsrc_t source );
// Send packet over network layer
void			NET_SendPacket( netsrc_t source, int length, void* data, netadr_t destination );
qboolean		NET_JoinGroup( netsrc_t source, netadr_t address );
qboolean		NET_LeaveGroup( netsrc_t source, netadr_t address );
void			NET_CheckCleanupFakeIPConnection( netsrc_t source, netadr_t* remoteAddress );

char*			NET_AdrToString( netadr_t address );
char*			NET_BaseAdrToString( netadr_t address );
qboolean		NET_StringToAdr( char* text, netadr_t* address );
// Compare addresses
qboolean		NET_CompareAdr( netadr_t first, netadr_t second );
qboolean		NET_CompareBaseAdr( netadr_t first, netadr_t second );
qboolean		NET_CompareClassBAdr( netadr_t first, netadr_t second );
qboolean		NET_IsLocalAddress( netadr_t address );
qboolean		NET_IsReservedAdr( netadr_t address );

unsigned int	NET_NetAdr_GetIPHostByteOrder( netadr_t* address );
unsigned int	NET_NetAdr_GetIPNetworkByteOrder( netadr_t* address );
unsigned short	NET_NetAdr_GetPort( netadr_t* address );
void			NET_NetAdr_SetIP( netadr_t* address, unsigned int ip );
void			NET_NetAdr_SetPort( netadr_t* address, unsigned short port );

// Initialize subsystem
void			Netchan_Init();
void			Netchan_Setup( netsrc_t source, netchan_t* channel, netadr_t address, int playerSlot,
					void* connectionStatus, int ( *fragmentSize )( void* ) );
void			Netchan_Clear( netchan_t* channel );
// Transmit reliable payload and unreliable data, if there's room
void			Netchan_Transmit( netchan_t* channel, int length, byte* data );
qboolean		Netchan_Process( netchan_t* channel );
// Is bandwidth choke active?
qboolean		Netchan_CanPacket( netchan_t* channel );
qboolean		Netchan_IncomingReady( netchan_t* channel );
void			Netchan_OutOfBandPrint( netsrc_t source, netadr_t address, char* format, ... );

void			Netchan_FragSend( netchan_t* channel );
// Take buffer/file/filebuffer and create fragments for it
void			Netchan_CreateFragments( qboolean server, netchan_t* channel, sizebuf_t* message );
void			Netchan_CreateFragments_( qboolean server, netchan_t* channel, sizebuf_t* message );
void			Netchan_CreateFileFragmentsFromBuffer( qboolean server, netchan_t* channel, char* filename,
					byte* uncompressed, int uncompressedSize );
void			Netchan_FlushIncoming( netchan_t* channel, int stream );
// Recreate buffer from incoming data fragments
qboolean		Netchan_CopyNormalFragments( netchan_t* channel );
qboolean		Netchan_CopyFileFragments( netchan_t* channel );
void			Netchan_UpdateProgress( netchan_t* channel );
