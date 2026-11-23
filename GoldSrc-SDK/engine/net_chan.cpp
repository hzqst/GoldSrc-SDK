#include "net.h"
#include "common.h"
#include "conprint.h"
#include "cvar.h"
#include "host.h"
#include "sys.h"
#include "mem.h"
#include "filesystem.h"
#include "bzlib.h"
#include "client.h"
#include "server.h"
#include "protocol.h"
#include "engine_apiproxy.h"
#include "cmd.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#define NET_MAX_MESSAGE 6037
#define UDP_HEADER_SIZE 28

#define NETCHAN_RELIABLE_BIT ( 1u << 31 )
#define NETCHAN_FRAGMENT_BIT ( 1u << 30 )

#define MAX_RELIABLE_FRAG_PAYLOAD 1200

#define MAX_FRAGMENTS 25000
#define MAX_FRAG_LENGTH 2048
#define MAX_FRAG_OFFSET 16384

extern cvar_t sv_lan;
extern cvar_t sv_lan_rate;

int net_drop;
char gDownloadFile[DOWNLOAD_FILENAME_BYTES];

cvar_t net_log = { "net_log", "0" };
cvar_t net_showpackets = { "net_showpackets", "0" };
cvar_t net_showdrop = { "net_showdrop", "0" };
cvar_t net_chokeloopback = { "net_chokeloop", "0" };
cvar_t net_drawslider = { "net_drawslider", "0" };
cvar_t sv_filetransfercompression = { "sv_filetransfercompression", "1" };
cvar_t sv_filetransfermaxsize = { "sv_filetransfermaxsize", "10485760" };

fragbuf_t* Netchan_FindBufferById( fragbuf_t** pplist, int id, qboolean allocate );
qboolean Netchan_Validate( netchan_t* chan, qboolean* frag_message, unsigned int* fragid, int* frag_offset,
	int* frag_length );
void Netchan_AddBufferToList( fragbuf_t** pplist, fragbuf_t* pbuf );

/*
===============
Netchan_CanPacket

Returns true if the bandwidth choke isn't active
================
*/
qboolean Netchan_CanPacket( netchan_t* chan )
{
	// Never choke loopback packets.
	if ( !net_chokeloopback.value && chan->remote_address.type == NA_LOOPBACK )
	{
		chan->cleartime = realtime;
		return TRUE;
	}

	return realtime > chan->cleartime;
}

/*
==============================
Netchan_IncomingReady

==============================
*/
qboolean Netchan_IncomingReady( netchan_t* chan )
{
	int i;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( chan->incomingready[i] )
			return TRUE;
	}

	return FALSE;
}

/*
==============================
Netchan_UnlinkFragment

==============================
*/
void Netchan_UnlinkFragment( fragbuf_t* buf, fragbuf_t** list )
{
	fragbuf_t* search;

	if ( !list )
	{
		Con_Printf( "Netchan_UnlinkFragment:  Asked to unlink fragment from empty list, ignored\n" );
		return;
	}

	// At head of list
	if ( buf == *list )
	{
		// Remove first element
		*list = buf->next;
		Mem_Free( buf );
		return;
	}

	search = *list;
	while ( search->next )
	{
		if ( search->next == buf )
		{
			search->next = buf->next;
			Mem_Free( buf );
			return;
		}
		search = search->next;
	}

	Con_Printf( "Netchan_UnlinkFragment:  Couldn't find fragment\n" );
}

/*
==============================
Netchan_UpdateFlow

==============================
*/
void Netchan_UpdateFlow( netchan_t* chan )
{
	int i, j;
	int start;
	int bytes;
	float faccumulatedtime;
	float kbps;
	flow_t* pflow;
	flowstats_t *pprev, *pstat;

	if ( !chan )
		return;

	bytes = 0;
	faccumulatedtime = 0.0f;

	for ( i = 0; i < MAX_FLOWS; i++ )
	{
		pflow = &chan->flow[i];
		if ( realtime - pflow->nextcompute < 0.1 )
			continue;

		pflow->nextcompute = realtime + 0.1;

		start = pflow->current - 1;
		pprev = &pflow->stats[start & ( MAX_LATENT - 1 )];
		for ( j = 0; j < MAX_LATENT / 2 - 1; j++ )
		{
			// Most recent message then backward from there
			pstat = &pflow->stats[( start - 1 - j ) & ( MAX_LATENT - 1 )];
			bytes += pstat->size;
			faccumulatedtime = (float)( faccumulatedtime + ( pprev->time - pstat->time ) );
			pprev = pstat;
		}

		kbps = 0.0f;
		if ( faccumulatedtime )
			kbps = (float)bytes / faccumulatedtime / 1024.0f;

		pflow->kbytespersec = kbps;
		pflow->avgkbytespersec = (float)( pflow->avgkbytespersec * ( 2.0 / 3.0 ) + kbps * ( 1.0 - 2.0 / 3.0 ) );
	}
}

/*
===============
Netchan_Transmit

tries to send an unreliable message to a connection, and handles the
transmition / retransmition of the reliable messages.

A 0 length will still generate a packet and deal with the reliable messages.
================
*/
void Netchan_Transmit( netchan_t* chan, int length, byte* data )
{
	sizebuf_t send;
	byte send_buf[NET_MAX_MESSAGE];
	qboolean send_reliable;
	qboolean send_resending;
	qboolean send_reliable_fragment;
	qboolean send_normal;
	qboolean send_frag[MAX_STREAMS];
	qboolean have_frag;
	unsigned int w1, w2;
	int i, j;
	int size;
	int maxsize;
	float fRate;
	float time;
	fragbuf_t* pbuf;
	FileHandle_t hfile;
	char compressedfilename[MAX_PATH];
	flow_t* pflow;
	flowstats_t* pstats;

	send.buffername = "Netchan_Transmit";
	send.data = send_buf;
	send.maxsize = sizeof( send_buf );
	send.cursize = 0;
	send.flags = 0;

	// check for message overflow
	if ( chan->message.flags & FSB_OVERFLOWED )
	{
		Con_Printf( "%s:Outgoing message overflow\n", NET_AdrToString( chan->remote_address ) );
		return;
	}

	// if the remote side dropped the last reliable message, resend it
	send_resending = FALSE;
	if ( chan->incoming_acknowledged > chan->last_reliable_sequence
		 && chan->incoming_reliable_acknowledged != chan->reliable_sequence )
		send_resending = TRUE;

	send_reliable = send_resending;

	// if the reliable transmit buffer is empty, copy the current message out
	if ( !chan->reliable_length )
	{
		Netchan_FragSend( chan );

		send_normal = chan->message.cursize != 0;
		for ( i = 0; i < MAX_STREAMS; i++ )
			send_frag[i] = chan->fragbufs[i] != NULL;

		// Stall reliable payloads if sending from frag buffer
		if ( send_normal && send_frag[FRAG_NORMAL_STREAM] )
		{
			send_normal = FALSE;

			// If the reliable buffer has gotten too big, queue it at the end of everything and clear out buffer
			if ( chan->message.cursize > MAX_RELIABLE_FRAG_PAYLOAD )
			{
				Netchan_CreateFragments( chan != &cls.netchan, chan, &chan->message );
				chan->message.cursize = 0;
			}
		}

		// Startpos will be zero if there is no regular payload
		have_frag = FALSE;
		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			chan->frag_startpos[i] = 0;
			chan->reliable_fragment[i] = FALSE;
			chan->reliable_fragid[i] = 0;
			chan->frag_length[i] = 0;

			if ( send_frag[i] )
				have_frag = TRUE;
		}

		if ( send_normal || have_frag )
		{
			chan->reliable_sequence ^= 1;
			send_reliable = TRUE;
		}

		if ( send_normal )
		{
			Q_memcpy( chan->reliable_buf, chan->message_buf, chan->message.cursize );
			chan->reliable_length = chan->message.cursize;
			chan->message.cursize = 0;

			for ( i = 0; i < MAX_STREAMS; i++ )
				chan->frag_startpos[i] = (short)chan->reliable_length;
		}

		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			pbuf = chan->fragbufs[i];
			if ( !pbuf || !send_frag[i] )
				continue;

			size = pbuf->frag_message.cursize;
			if ( pbuf->isfile && !pbuf->isbuffer )
				size = pbuf->size;

			if ( size + chan->reliable_length >= MAX_RELIABLE_FRAG_PAYLOAD )
				continue;

			chan->reliable_fragid[i] = ( (unsigned int)pbuf->bufferid << 16 ) | (unsigned short)chan->fragbufcount[i];

			if ( pbuf->isfile && !pbuf->isbuffer )
			{
				if ( pbuf->iscompressed )
				{
					snprintf( compressedfilename, sizeof( compressedfilename ), "%s.ztmp", pbuf->filename );
					hfile = FS_Open( compressedfilename, "rb" );
				}
				else
					hfile = FS_Open( pbuf->filename, "rb" );

				FS_Seek( hfile, pbuf->foffset, FILESYSTEM_SEEK_HEAD );
				FS_Read( pbuf->frag_message.data + pbuf->frag_message.cursize, pbuf->size, 1, hfile );
				pbuf->frag_message.cursize += pbuf->size;
				FS_Close( hfile );
			}

			Q_memcpy( chan->reliable_buf + chan->reliable_length, pbuf->frag_message.data,
				pbuf->frag_message.cursize );
			chan->reliable_length += pbuf->frag_message.cursize;
			chan->frag_length[i] = (short)pbuf->frag_message.cursize;

			Netchan_UnlinkFragment( pbuf, &chan->fragbufs[i] );
			chan->reliable_fragment[i] = TRUE;

			// Offset the rest of the starting positions
			for ( j = i + 1; j < MAX_STREAMS; j++ )
				chan->frag_startpos[j] += chan->frag_length[i];
		}
	}

	// Prepare the packet header
	w1 = chan->outgoing_sequence;
	if ( send_reliable )
		w1 |= NETCHAN_RELIABLE_BIT;

	w2 = chan->incoming_sequence | ( (unsigned int)chan->incoming_reliable_sequence << 31 );

	send_reliable_fragment = FALSE;
	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( chan->reliable_fragment[i] )
			send_reliable_fragment = TRUE;
	}

	if ( send_reliable && send_reliable_fragment )
		w1 |= NETCHAN_FRAGMENT_BIT;

	chan->outgoing_sequence++;

	MSG_WriteLong( &send, w1 );
	MSG_WriteLong( &send, w2 );

	// copy the reliable message to the packet first
	if ( send_reliable )
	{
		if ( send_reliable_fragment )
		{
			for ( i = 0; i < MAX_STREAMS; i++ )
			{
				if ( chan->reliable_fragment[i] )
				{
					MSG_WriteByte( &send, 1 );
					MSG_WriteLong( &send, chan->reliable_fragid[i] );
					MSG_WriteShort( &send, chan->frag_startpos[i] );
					MSG_WriteShort( &send, chan->frag_length[i] );
				}
				else
					MSG_WriteByte( &send, 0 );
			}
		}

		SZ_Write( &send, chan->reliable_buf, chan->reliable_length );
		chan->last_reliable_sequence = chan->outgoing_sequence - 1;
	}

	// Is there room for the unreliable payload?
	maxsize = send_resending ? FRAGMENT_SIZE : send.maxsize;
// add the unreliable part if space is available
	if ( maxsize - send.cursize >= length )
		SZ_Write( &send, data, length );
	else
		Con_DPrintf( "Netchan_Transmit:  Unreliable would overfow, ignoring\n" );

	// Deal with packets that are too small for some networks
	while ( send.cursize < 16 )
		MSG_WriteByte( &send, clc_nop );

	pflow = &chan->flow[FLOW_OUTGOING];
	pstats = &pflow->stats[pflow->current & ( MAX_LATENT - 1 )];
	pstats->size = send.cursize + UDP_HEADER_SIZE;
	pstats->time = realtime;
	pflow->current++;
	Netchan_UpdateFlow( chan );

	if ( !cls.demoplayback )
	{
		COM_Munge2( send.data + 8, send.cursize - 8, (byte)( chan->outgoing_sequence - 1 ) );

		if ( g_modfuncs.m_pfnProcessOutgoingNet )
			g_modfuncs.m_pfnProcessOutgoingNet( chan, &send );

		NET_SendPacket( chan->sock, send.cursize, send.data, chan->remote_address );
	}

	if ( sv.active && sv_lan.value && sv_lan_rate.value > 1000.0f )
		fRate = 1.0f / sv_lan_rate.value;
	else
		fRate = (float)( 1.0 / chan->rate );

	if ( realtime > chan->cleartime )
		chan->cleartime = realtime;

	chan->cleartime += (float)( send.cursize + UDP_HEADER_SIZE ) * fRate;

	if ( net_showpackets.value && net_showpackets.value != 2.0f )
	{
		time = (float)( chan == &cls.netchan ? cl.time : sv.time );
		Con_Printf( " %c --> sz=%i seq=%i ack=%i rel=%i tm=%f\n", chan == &cls.netchan ? 'c' : 's', send.cursize,
			chan->outgoing_sequence - 1, chan->incoming_sequence, send_reliable, time );
	}
}

/*
==============================
Netchan_ClearFragbufs

==============================
*/
void Netchan_ClearFragbufs( fragbuf_t** ppbuf )
{
	fragbuf_t *buf, *n;

	if ( !ppbuf )
		return;

	// Throw away any that are sitting around
	buf = *ppbuf;
	while ( buf )
	{
		n = buf->next;
		Mem_Free( buf );
		buf = n;
	}

	*ppbuf = NULL;
}

/*
==============================
Netchan_ClearFragments

==============================
*/
void Netchan_ClearFragments( netchan_t* chan )
{
	fragbufwaiting_t *wait, *next;
	int i;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		wait = chan->waitlist[i];
		while ( wait )
		{
			next = wait->next;
			Netchan_ClearFragbufs( &wait->fragbufs );
			Mem_Free( wait );
			wait = next;
		}
		chan->waitlist[i] = NULL;

		Netchan_ClearFragbufs( &chan->fragbufs[i] );
		Netchan_FlushIncoming( chan, i );
	}
}

/*
==============================
Netchan_Clear

==============================
*/
void Netchan_Clear( netchan_t* chan )
{
	int i;

	Netchan_ClearFragments( chan );

	if ( chan->reliable_length )
	{
		Con_DPrintf(
			"Netchan_Clear() : reliable length not 0, reliable_sequence: %d, incoming_reliable_acknowledged: %d\n",
			chan->reliable_length, chan->incoming_reliable_acknowledged );
		chan->reliable_length = 0;
		chan->reliable_sequence ^= 1;
	}

	chan->cleartime = 0.0;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		chan->reliable_fragid[i] = 0;
		chan->reliable_fragment[i] = 0;
		chan->fragbufcount[i] = 0;
		chan->frag_startpos[i] = 0;
		chan->frag_length[i] = 0;
		chan->incomingready[i] = FALSE;
	}

	if ( chan->tempbuffer )
	{
		Mem_Free( chan->tempbuffer );
		chan->tempbuffer = NULL;
	}
	chan->tempbuffersize = 0;
}

void Netchan_CheckForCompletion( netchan_t* chan, int index, int intotalbuffers )
{
	int c;
	fragbuf_t* p;
	char adr[128];
	char cmd[32];

	p = chan->incomingbufs[index];
	if ( !p )
		return;

	c = 0;
	while ( p )
	{
		c++;

		if ( ( (unsigned int)p->bufferid >> 16 ) != (unsigned int)c && chan == &cls.netchan )
		{
			if ( chan->sock == NS_MULTICAST )
			{
				Q_strncpy_s( adr, NET_AdrToString( chan->remote_address ), sizeof( adr ) );
				adr[strcspn( adr, ";\n" )] = 0;
				snprintf( cmd, sizeof( cmd ), "listen %s\n", adr );
				Cbuf_AddText( cmd );
				return;
			}

			Con_Printf( "Netchan_CheckForCompletion:  Lost/dropped fragment would cause stall, retrying connection\n" );
			Cbuf_AddText( "retry\n" );
		}

		p = p->next;
	}

	if ( c == intotalbuffers )
		chan->incomingready[index] = TRUE;
}

/*
=================
Netchan_Process

called when the current net_message is from remote_address
modifies net_message so that it points to the packet payload
=================
*/
qboolean Netchan_Process( netchan_t* chan )
{
	unsigned int sequence, sequence_ack;
	qboolean reliable_message, reliable_ack;
	qboolean fragmented;
	qboolean frag_message[MAX_STREAMS] = {};
	unsigned int fragid[MAX_STREAMS] = {};
	int frag_offset[MAX_STREAMS] = {};
	int frag_length[MAX_STREAMS] = {};
	int i, j;
	int wpos, rpos, len;
	float time;
	fragbuf_t* pbuf;
	flow_t* pflow;
	flowstats_t* pstats;

	if ( !cls.demoplayback && !cls.passive && !NET_CompareAdr( net_from, chan->remote_address ) )
		return FALSE;

	chan->last_received = (float)realtime;

	// get sequence numbers
	MSG_BeginReading();
	sequence = MSG_ReadLong();
	sequence_ack = MSG_ReadLong();

	if ( ( sequence_ack & NETCHAN_FRAGMENT_BIT ) && !g_modfuncs.m_pfnProcessIncomingNet )
		return FALSE;

	if ( g_modfuncs.m_pfnProcessIncomingNet && !g_modfuncs.m_pfnProcessIncomingNet( chan, &net_message ) )
		return FALSE;

	COM_UnMunge2( net_message.data + 8, net_message.cursize - 8, (byte)sequence );

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;
	fragmented = ( sequence & NETCHAN_FRAGMENT_BIT ) != 0;

	if ( fragmented )
	{
		for ( i = 0; i < MAX_STREAMS; i++ )
		{
			if ( MSG_ReadByte() )
			{
				frag_message[i] = TRUE;
				fragid[i] = MSG_ReadLong();
				frag_offset[i] = MSG_ReadShort();
				frag_length[i] = MSG_ReadShort();
			}
		}

		if ( !Netchan_Validate( chan, frag_message, fragid, frag_offset, frag_length ) )
			return FALSE;
	}

	sequence &= ~( NETCHAN_RELIABLE_BIT | NETCHAN_FRAGMENT_BIT );
	sequence_ack &= ~( NETCHAN_RELIABLE_BIT | NETCHAN_FRAGMENT_BIT );

	if ( net_showpackets.value && net_showpackets.value != 3.0f )
	{
		time = (float)( chan == &cls.netchan ? cl.time : sv.time );
		Con_Printf( " %c <-- sz=%i seq=%i ack=%i rel=%i tm=%f\n", chan == &cls.netchan ? 'c' : 's',
			net_message.cursize, sequence, sequence_ack, reliable_message, time );
	}

//
	// discard stale or duplicated packets
//
	if ( sequence <= (unsigned int)chan->incoming_sequence )
	{
		if ( net_showdrop.value )
		{
			if ( sequence == (unsigned int)chan->incoming_sequence )
				Con_Printf( "%s:duplicate packet %i at %i\n", NET_AdrToString( chan->remote_address ), sequence,
					chan->incoming_sequence );
			else
				Con_Printf( "%s:out of order packet %i at %i\n", NET_AdrToString( chan->remote_address ), sequence,
					chan->incoming_sequence );
		}
		return FALSE;
	}

//
	// dropped packets don't keep the message from being used
//
	net_drop = sequence - ( chan->incoming_sequence + 1 );
	if ( net_drop > 0 && net_showdrop.value )
		Con_Printf( "%s:Dropped %i packets at %i\n", NET_AdrToString( chan->remote_address ), net_drop, sequence );

	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	if ( reliable_ack == chan->reliable_sequence && chan->incoming_acknowledged + 1 >= chan->last_reliable_sequence )
		chan->reliable_length = 0; // it has been received

//
	// if this message contains a reliable message, bump incoming_reliable_sequence
//
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if ( reliable_message )
		chan->incoming_reliable_sequence ^= 1;

	// update statistics counters
	pflow = &chan->flow[FLOW_INCOMING];
	pstats = &pflow->stats[pflow->current & ( MAX_LATENT - 1 )];
	pstats->size = net_message.cursize + UDP_HEADER_SIZE;
	pstats->time = realtime;
	pflow->current++;
	Netchan_UpdateFlow( chan );

	if ( !fragmented )
		return TRUE;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( !frag_message[i] )
			continue;

		if ( fragid[i] )
		{
			pbuf = Netchan_FindBufferById( &chan->incomingbufs[i], fragid[i], TRUE );
			if ( pbuf )
			{
				SZ_Clear( &pbuf->frag_message );
				SZ_Write( &pbuf->frag_message, net_message.data + msg_readcount + frag_offset[i], frag_length[i] );
				if ( pbuf->frag_message.flags & FSB_OVERFLOWED )
				{
					Netchan_FlushIncoming( chan, i );
					if ( chan != &cls.netchan )
						chan->message.flags |= FSB_OVERFLOWED;
					return FALSE;
				}
			}
			else
				Con_Printf( "Netchan_Process:  Couldn't allocate or find buffer %i\n", fragid[i] >> 16 );

			// Count # of incoming bufs we've queued? are we done?
			Netchan_CheckForCompletion( chan, i, fragid[i] & 0xffff );
		}

		// Rearrange incoming data to not have the frag stuff in the middle of it
		wpos = msg_readcount + frag_offset[i];
		rpos = wpos + frag_length[i];
		len = net_message.cursize - rpos;
		for ( j = 0; j < len; j++ )
			net_message.data[wpos + j] = net_message.data[rpos + j];

		net_message.cursize -= frag_length[i];

		for ( j = i + 1; j < MAX_STREAMS; j++ )
			frag_offset[j] -= frag_length[i];
	}

	// Is there anything left to process?
	return net_message.cursize > 16;
}

/*
==============================
Netchan_AllocFragbuf

==============================
*/
fragbuf_t* Netchan_AllocFragbuf()
{
	fragbuf_t* buf;

	buf = (fragbuf_t*)Mem_ZeroMalloc( sizeof( fragbuf_t ) );
	buf->bufferid = 0;
	buf->frag_message.cursize = 0;
	buf->frag_message.data = buf->frag_message_buf;
	buf->frag_message.maxsize = FRAGMENT_SIZE;
	buf->frag_message.buffername = "Frag Buffer Alloc'd";
	buf->next = NULL;

	return buf;
}

/*
==============================
Netchan_AddFragbufToTail

==============================
*/
void Netchan_AddFragbufToTail( fragbufwaiting_t* wait, fragbuf_t* buf )
{
	fragbuf_t** p;

	buf->next = NULL;
	wait->fragbufcount++;

	p = &wait->fragbufs;
	while ( *p )
		p = &( *p )->next;
	*p = buf;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
void Netchan_CreateFragments_( qboolean, netchan_t* chan, sizebuf_t* msg )
{
	static const char hdr[] = "BZ2";
	char compressed[65536];
	unsigned int compressedSize;
	int chunksize;
	int remaining;
	int bytes;
	int bufferid;
	unsigned int pos;
	fragbuf_t* buf;
	fragbufwaiting_t *wait, **p;

	if ( !msg->cursize )
		return;

	if ( memcmp( msg->data, hdr, sizeof( hdr ) ) )
	{
		compressedSize = msg->cursize - (int)sizeof( hdr );
		if ( BZ2_bzBuffToBuffCompress( compressed, &compressedSize, (char*)msg->data, msg->cursize, 9, 0, 30 )
			 == BZ_OK )
		{
			Con_DPrintf( "Compressing split packet (%d -> %d bytes)\n", msg->cursize, (int)compressedSize );
			memcpy( msg->data, hdr, sizeof( hdr ) );
			memcpy( msg->data + sizeof( hdr ), compressed, compressedSize );
			msg->cursize = compressedSize + sizeof( hdr );
		}
	}

	chunksize = chan->pfnNetchan_Blocksize( chan->connection_status );

	wait = (fragbufwaiting_t*)Mem_ZeroMalloc( sizeof( fragbufwaiting_t ) );

	pos = 0;
	remaining = msg->cursize;
	bufferid = 1;
	while ( remaining > 0 )
	{
		bytes = remaining < chunksize ? remaining : chunksize;
		remaining -= bytes;

		buf = Netchan_AllocFragbuf();
		buf->bufferid = bufferid++;

		// Copy in data
		SZ_Clear( &buf->frag_message );
		SZ_Write( &buf->frag_message, msg->data + pos, bytes );
		pos += bytes;

		Netchan_AddFragbufToTail( wait, buf );
	}

	// Now add waiting list item to end of buffer queue
	p = &chan->waitlist[FRAG_NORMAL_STREAM];
	while ( *p )
		p = &( *p )->next;
	*p = wait;
}

/*
==============================
Netchan_CreateFragments

==============================
*/
void Netchan_CreateFragments( qboolean server, netchan_t* chan, sizebuf_t* msg )
{
	// Always queue any pending reliable data ahead of the fragmentation buffer
	if ( chan->message.cursize > 0 )
	{
		Netchan_CreateFragments_( server, chan, &chan->message );
		chan->message.cursize = 0;
	}

	Netchan_CreateFragments_( server, chan, msg );
}

/*
==============================
Netchan_FragSend

Fragmentation buffer is full and user is prepared to send
==============================
*/
void Netchan_FragSend( netchan_t* chan )
{
	fragbufwaiting_t* wait;
	int i;

	if ( !chan )
		return;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		// Already something queued up, just leave in waitlist
		if ( chan->fragbufs[i] )
			continue;

		// Nothing to queue?
		wait = chan->waitlist[i];
		if ( !wait )
			continue;

		chan->waitlist[i] = wait->next;
		wait->next = NULL;

		// Copy in to fragbuf
		chan->fragbufs[i] = wait->fragbufs;
		chan->fragbufcount[i] = wait->fragbufcount;

		// Throw away wait list
		Mem_Free( wait );
	}
}

/*
==============================
Netchan_FlushIncoming

==============================
*/
void Netchan_FlushIncoming( netchan_t* chan, int stream )
{
	fragbuf_t *p, *n;

	SZ_Clear( &net_message );
	msg_readcount = 0;

	p = chan->incomingbufs[stream];
	while ( p )
	{
		n = p->next;
		Mem_Free( p );
		p = n;
	}

	chan->incomingready[stream] = FALSE;
	chan->incomingbufs[stream] = NULL;
}

static void Netchan_DropCopiedFileFragments( netchan_t* chan )
{
	SZ_Clear( &net_message );
	msg_readcount = 0;

	chan->incomingbufs[FRAG_FILE_STREAM] = NULL;
	chan->incomingready[FRAG_FILE_STREAM] = FALSE;
}

qboolean Netchan_CopyFileFragments( netchan_t* chan )
{
	fragbuf_t *p, *n;
	char filename[MAX_PATH];
	char compressor[32];
	char filedir[MAX_PATH];
	char* pszFileName;
	qboolean bCompressed;
	unsigned int uncompressedSize;
	unsigned int filesize;
	int nsize;
	int pos;
	byte* data;
	char* buffer;
	char* uncompressed;
	FileHandle_t handle;

	if ( !chan->incomingready[FRAG_FILE_STREAM] )
		return FALSE;

	p = chan->incomingbufs[FRAG_FILE_STREAM];
	if ( !p )
	{
		Con_Printf( "Netchan_CopyFileFragments:  Called with no fragments readied\n" );
		chan->incomingready[FRAG_FILE_STREAM] = FALSE;
		return FALSE;
	}

	SZ_Clear( &net_message );
	MSG_BeginReading();
	SZ_Write( &net_message, p->frag_message.data, p->frag_message.cursize );

	Q_strncpy( filename, MSG_ReadString(), sizeof( filename ) - 1 );
	filename[sizeof( filename ) - 1] = 0;

	Q_strncpy( compressor, MSG_ReadString(), sizeof( compressor ) - 1 );
	compressor[sizeof( compressor ) - 1] = 0;

	bCompressed = _stricmp( compressor, "bz2" ) == 0;
	uncompressedSize = MSG_ReadLong();

	if ( !g_bIsDedicatedServer && !Host_IsServerActive() && !cl_allow_download.value )
	{
		Con_Printf( "File fragment received but cl_allow_download set to 0, ignoring\n" );
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return FALSE;
	}

	if ( Q_strlen( filename ) <= 0 )
	{
		Con_Printf( "File fragment received with no filename\nFlushing input queue\n" );
		// Clear out bufs
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return FALSE;
	}

	if ( Q_strstr( filename, ".." ) )
	{
		Con_Printf( "File fragment received with relative path, ignoring\n" );
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return FALSE;
	}

	if ( filename[0] != '!' && !IsSafeFileToDownload( filename ) )
	{
		Con_Printf( "File fragment received with bad path, ignoring\n" );
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return FALSE;
	}

	if ( Host_IsServerActive() && filename[0] != '!' )
	{
		Con_Printf( "File fragment received with bad path, ignoring (2)\n" );
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return FALSE;
	}

	Q_strncpy( chan->incomingfilename, filename, sizeof( chan->incomingfilename ) - 1 );
	chan->incomingfilename[sizeof( chan->incomingfilename ) - 1] = 0;

	if ( filename[0] != '!' && FS_FileExists( filename ) )
	{
		Con_Printf( "Can't download %s, already exists\n", filename );
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return TRUE;
	}

	// Create file from buffers
	nsize = 0;
	while ( p )
	{
		nsize += p->frag_message.cursize;
		if ( p == chan->incomingbufs[FRAG_FILE_STREAM] )
			nsize -= msg_readcount;
		p = p->next;
	}

	buffer = (char*)Mem_ZeroMalloc( nsize + 1 );
	if ( !buffer )
	{
		Con_Printf( "Buffer allocation failed on %i bytes\n", nsize + 1 );
		// Clear out bufs
		Netchan_FlushIncoming( chan, FRAG_FILE_STREAM );
		return FALSE;
	}

	p = chan->incomingbufs[FRAG_FILE_STREAM];
	pos = 0;
	while ( p )
	{
		n = p->next;

		data = p->frag_message.data;
		// First message has the file name, don't write that into the data stream, just write the rest of the actual data
		if ( p == chan->incomingbufs[FRAG_FILE_STREAM] )
		{
			p->frag_message.cursize -= msg_readcount;
			data += msg_readcount;
		}

		Q_memcpy( buffer + pos, data, p->frag_message.cursize );
		pos += p->frag_message.cursize;

		Mem_Free( p );
		p = n;
	}

	filesize = pos;

	if ( bCompressed )
	{
		uncompressed = (char*)Mem_Malloc( uncompressedSize );
		Con_DPrintf( "Decompressing file %s (%d -> %d)\n", filename, nsize, (int)uncompressedSize );
		BZ2_bzBuffToBuffDecompress( uncompressed, &uncompressedSize, buffer, nsize, 1, 0 );
		Mem_Free( buffer );
		filesize = uncompressedSize;
		buffer = uncompressed;
	}

	if ( filename[0] == '!' )
	{
		if ( chan->tempbuffer )
		{
			Con_DPrintf( "Netchan_CopyFragments:  Freeing holdover tempbuffer\n" );
			Mem_Free( chan->tempbuffer );
		}
		chan->tempbuffer = buffer;
		chan->tempbuffersize = filesize;
	}
	else
	{
		strncpy( filedir, filename, sizeof( filedir ) );
		COM_FixSlashes( filedir );
#if defined( _WIN32 )
		pszFileName = strrchr( filedir, '\\' );
#else
		pszFileName = strrchr( filedir, '/' );
#endif
		if ( pszFileName )
		{
			*pszFileName = 0;
			FS_CreateDirHierarchy( filedir, "GAMEDOWNLOAD" );
		}

		handle = FS_OpenPathID( filename, "wb", "GAMEDOWNLOAD" );
		if ( !handle )
		{
			Con_Printf( "File open failed %s\n", filename );
			Mem_Free( buffer );
			Netchan_DropCopiedFileFragments( chan );
			return FALSE;
		}

		Sys_Printf( "COM_WriteFile: %s\n", filename );
		FS_Write( buffer, filesize, 1, handle );
		FS_Close( handle );

		Mem_Free( buffer );
	}

	SZ_Clear( &net_message );
	msg_readcount = 0;

	chan->incomingbufs[FRAG_FILE_STREAM] = NULL;
	// Reset flag
	chan->incomingready[FRAG_FILE_STREAM] = FALSE;

	return TRUE;
}

#ifdef _MSC_VER
#pragma warning( suppress: 6262 )
#endif
qboolean Netchan_CopyNormalFragments( netchan_t* chan )
{
	static const char hdr[] = "BZ2";
	fragbuf_t *p, *n;
	qboolean success;
	char uncompressed[65536];
	unsigned int uncompressedSize;

	if ( !chan->incomingready[FRAG_NORMAL_STREAM] )
		return FALSE;

	p = chan->incomingbufs[FRAG_NORMAL_STREAM];
	if ( !p )
	{
		Con_Printf( "Netchan_CopyNormalFragments:  Called with no fragments readied\n" );
		chan->incomingready[FRAG_NORMAL_STREAM] = FALSE;
		return FALSE;
	}

	success = TRUE;

	SZ_Clear( &net_message );
	MSG_BeginReading();

	while ( p )
	{
		n = p->next;

		if ( success )
		{
			if ( SZ_HasSpace( &net_message, p->frag_message.cursize ) )
				SZ_Write( &net_message, p->frag_message.data, p->frag_message.cursize );
			else
			{
				SZ_Clear( &net_message );
				success = FALSE;
			}
		}

		Mem_Free( p );
		p = n;
	}

	if ( success && net_message.cursize >= (int)sizeof( hdr ) && !memcmp( net_message.data, hdr, sizeof( hdr ) ) )
	{
		uncompressedSize = sizeof( uncompressed );
		BZ2_bzBuffToBuffDecompress( uncompressed, &uncompressedSize, (char*)( net_message.data + sizeof( hdr ) ),
			net_message.cursize - sizeof( hdr ), 1, 0 );
		memcpy( net_message.data, uncompressed, uncompressedSize );
		net_message.cursize = uncompressedSize;
	}

	chan->incomingbufs[FRAG_NORMAL_STREAM] = NULL;
	// Reset flag
	chan->incomingready[FRAG_NORMAL_STREAM] = FALSE;

	return success;
}

int Netchan_CreateFileFragments( qboolean, netchan_t* chan, char* filename )
{
	int chunksize;
	int send;
	int remaining;
	int filesize;
	int pos;
	int bufferid;
	qboolean firstfragment;
	qboolean bCompressed;
	char compressedfilename[MAX_PATH];
	int32 compressedFileTime, fileTime;
	unsigned int compressedSize;
	char *uncompressed, *compressed;
	FileHandle_t hfile, handle;
	fragbuf_t* buf;
	fragbufwaiting_t *wait, **p;

	chunksize = chan->pfnNetchan_Blocksize( chan->connection_status );

	snprintf( compressedfilename, sizeof( compressedfilename ), "%s.ztmp", filename );

	bCompressed = FALSE;
	remaining = 0;

	compressedFileTime = FS_GetFileTime( compressedfilename );
	fileTime = FS_GetFileTime( filename );
	if ( compressedFileTime >= fileTime )
	{
		hfile = FS_Open( compressedfilename, "rb" );
		if ( hfile )
		{
			remaining = FS_Size( hfile );
			FS_Close( hfile );
			bCompressed = TRUE;
		}
	}

	handle = FS_Open( filename, "rb" );
	if ( !handle )
	{
		Con_Printf( "Warning:  Unable to open %s for transfer\n", filename );
		return 0;
	}

	filesize = FS_Size( handle );
	if ( (float)filesize > sv_filetransfermaxsize.value )
	{
		FS_Close( handle );
		Con_Printf( "Warning:  File %s is too big to transfer from host %s\n", filename,
			NET_AdrToString( chan->remote_address ) );
		return 0;
	}

	if ( !bCompressed )
	{
		remaining = filesize;

		if ( sv_filetransfercompression.value )
		{
			uncompressed = (char*)Mem_Malloc( remaining );
			compressed = (char*)Mem_Malloc( remaining );
			compressedSize = remaining;

			FS_Read( uncompressed, remaining, 1, handle );
			if ( BZ2_bzBuffToBuffCompress( compressed, &compressedSize, uncompressed, remaining, 9, 0, 30 )
				 == BZ_OK )
			{
				hfile = FS_Open( compressedfilename, "wb" );
				if ( hfile )
				{
					Con_DPrintf( "Creating compressed version of file %s (%d -> %d)\n", filename, remaining,
						(int)compressedSize );
					FS_Write( compressed, compressedSize, 1, hfile );
					FS_Close( hfile );

					remaining = compressedSize;
					bCompressed = TRUE;
				}
			}

			Mem_Free( uncompressed );
			Mem_Free( compressed );
		}
	}

	FS_Close( handle );

	wait = (fragbufwaiting_t*)Mem_ZeroMalloc( sizeof( fragbufwaiting_t ) );

	pos = 0;
	bufferid = 1;
	firstfragment = TRUE;
	while ( remaining > 0 )
	{
		send = remaining < chunksize ? remaining : chunksize;

		buf = Netchan_AllocFragbuf();
		buf->bufferid = bufferid++;

		SZ_Clear( &buf->frag_message );

		// 
		if ( firstfragment )
		{
			firstfragment = FALSE;

			MSG_WriteString( &buf->frag_message, filename );
			MSG_WriteString( &buf->frag_message, bCompressed ? "bz2" : "uncompressed" );
			MSG_WriteLong( &buf->frag_message, filesize );

			send -= buf->frag_message.cursize;
		}

		buf->iscompressed = bCompressed;
		buf->foffset = pos;
		buf->isfile = TRUE;
		buf->size = send;
		Q_strncpy( buf->filename, filename, sizeof( buf->filename ) - 1 );
		buf->filename[sizeof( buf->filename ) - 1] = 0;

		pos += send;
		remaining -= send;

		Netchan_AddFragbufToTail( wait, buf );
	}

	// Now add waiting list item to end of buffer queue
	p = &chan->waitlist[FRAG_FILE_STREAM];
	while ( *p )
		p = &( *p )->next;
	*p = wait;

	return 1;
}

/*
==============================
Netchan_CreateFileFragmentsFromBuffer

==============================
*/
void Netchan_CreateFileFragmentsFromBuffer(
	qboolean, netchan_t* chan, char* filename, byte* uncompressed_pbuf, int uncompressed_size )
{
	int chunksize;
	int send;
	int remaining;
	int pos;
	int bufferid;
	qboolean firstfragment;
	qboolean bCompressed;
	unsigned int size;
	char* pbuf;
	fragbuf_t* buf;
	fragbufwaiting_t *wait, **p;

	if ( !uncompressed_size )
		return;

	size = uncompressed_size;
	pbuf = (char*)Mem_Malloc( uncompressed_size );

	bCompressed = TRUE;
	if ( BZ2_bzBuffToBuffCompress( pbuf, &size, (char*)uncompressed_pbuf, uncompressed_size, 9, 0, 30 ) != BZ_OK )
	{
		bCompressed = FALSE;
		Mem_Free( pbuf );
		pbuf = (char*)uncompressed_pbuf;
		size = uncompressed_size;
	}
	else
		Con_DPrintf( "Compressed %s for transmission (%d -> %d)\n", filename, uncompressed_size, (int)size );

	chunksize = chan->pfnNetchan_Blocksize( chan->connection_status );

	wait = (fragbufwaiting_t*)Mem_ZeroMalloc( sizeof( fragbufwaiting_t ) );

	remaining = size;
	pos = 0;
	bufferid = 1;
	firstfragment = TRUE;
	while ( remaining > 0 )
	{
		send = remaining < chunksize ? remaining : chunksize;

		buf = Netchan_AllocFragbuf();
		buf->bufferid = bufferid++;

		SZ_Clear( &buf->frag_message );

		// 
		if ( firstfragment )
		{
			firstfragment = FALSE;

			MSG_WriteString( &buf->frag_message, filename );
			MSG_WriteString( &buf->frag_message, bCompressed ? "bz2" : "uncompressed" );
			MSG_WriteLong( &buf->frag_message, uncompressed_size );

			send -= buf->frag_message.cursize;
		}

		buf->foffset = pos;
		buf->isbuffer = TRUE;
		buf->isfile = TRUE;
		buf->size = send;

		MSG_WriteBuf( &buf->frag_message, send, pbuf + pos );

		pos += send;
		remaining -= send;

		Netchan_AddFragbufToTail( wait, buf );
	}

	// Now add waiting list item to end of buffer queue
	p = &chan->waitlist[FRAG_FILE_STREAM];
	while ( *p )
		p = &( *p )->next;
	*p = wait;
}

/*
===============
Netchan_OutOfBand

Sends an out-of-band datagram
================
*/
void Netchan_OutOfBand( netsrc_t sock, netadr_t adr, int length, byte* data )
{
	sizebuf_t send;
	byte send_buf[NET_MAX_MESSAGE];

	// write the packet header
	send.buffername = "Netchan_OutOfBand";
	send.maxsize = sizeof( send_buf );
	send.cursize = 0;
	send.data = send_buf;
	send.flags = FSB_ALLOWOVERFLOW;

	MSG_WriteLong( &send, -1 ); // -1 sequence means out of band
	SZ_Write( &send, data, length );

	// send the datagram
	if ( !cls.demoplayback )
		NET_SendPacket( sock, send.cursize, send.data, adr );
}

/*
===============
Netchan_OutOfBandPrint

Sends a text message in an out-of-band datagram
================
*/
void Netchan_OutOfBandPrint( netsrc_t sock, netadr_t adr, char* format, ... )
{
	va_list argptr;
	static char string[8192]; // ??? why static?

	va_start( argptr, format );
	vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	Netchan_OutOfBand( sock, adr, Q_strlen( string ) + 1, (byte*)string );
}

/*
==============
Netchan_Setup

called to open a channel to a remote system
==============
*/
void Netchan_Setup( netsrc_t socketnumber, netchan_t* chan, netadr_t adr, int player_slot, void* connection_status,
	int ( *pfnNetchan_Blocksize )( void* ) )
{
	Netchan_Clear( chan );

	Q_memset( chan, 0, sizeof( *chan ) );

	chan->player_slot = player_slot + 1;
	chan->sock = socketnumber;
	chan->remote_address = adr;
	chan->last_received = (float)realtime;
	chan->connect_time = (float)realtime;

	chan->message.data = chan->message_buf;
	chan->message.flags = FSB_ALLOWOVERFLOW;
	chan->outgoing_sequence = 1;
	chan->connection_status = connection_status;
	chan->message.maxsize = sizeof( chan->message_buf );
	chan->message.buffername = "netchan->message";
	chan->rate = 30000.0;
	chan->pfnNetchan_Blocksize = pfnNetchan_Blocksize;
}

/*
==============================
Netchan_UpdateProgress

==============================
*/
void Netchan_UpdateProgress( netchan_t* chan )
{
	fragbuf_t* p;
	int i, c;
	int len;
	unsigned int total;
	float percent, bestpercent;
	char sz[1024];

	scr_downloading.value = -1.0f;
	gDownloadFile[0] = 0;

	if ( net_drawslider.value != 1.0f && !chan->incomingbufs[FRAG_FILE_STREAM] )
		return;

	bestpercent = 0.0f;
	c = 0;
	for ( i = MAX_STREAMS - 1; i >= 0; i-- )
	{
		// Receiving data
		if ( chan->incomingbufs[i] )
		{
			for ( p = chan->incomingbufs[i]; p; p = p->next )
				c++;

			p = chan->incomingbufs[i];
			total = (unsigned short)p->bufferid;
			if ( total )
			{
				percent = (float)( (float)c * 100.0 / (float)total );
				if ( percent > bestpercent )
					bestpercent = percent;
			}

			if ( i == FRAG_FILE_STREAM )
			{
				len = 0;
				while ( len < 129 && p->frag_message.data[len] )
				{
					sz[len] = p->frag_message.data[len];
					len++;
				}
				sz[len] = 0;

				if ( Q_strlen( sz ) > 0 && ( sz[0] != '!' || !Q_strncmp( sz, "!ModuleC.dll", 11 ) ) )
				{
					Q_strncpy( gDownloadFile, sz, sizeof( gDownloadFile ) - 1 );
					gDownloadFile[sizeof( gDownloadFile ) - 1] = 0;
				}
			}
		}
		// Sending data
		else if ( chan->fragbufs[i] && chan->fragbufcount[i] )
		{
			percent = (float)( (float)chan->fragbufs[i]->bufferid * 100.0 / (float)chan->fragbufcount[i] );
			if ( percent > bestpercent )
				bestpercent = percent;
		}
	}

	scr_downloading.value = bestpercent;
}

/*
===============
Netchan_Init

===============
*/
void Netchan_Init()
{
	Cvar_RegisterVariable( &net_log );
	Cvar_RegisterVariable( &net_showpackets );
	Cvar_RegisterVariable( &net_showdrop );
	Cvar_RegisterVariable( &net_chokeloopback );
	Cvar_RegisterVariable( &net_drawslider );
	Cvar_RegisterVariable( &sv_filetransfercompression );
	Cvar_RegisterVariable( &sv_filetransfermaxsize );
}

/*
==============================
Netchan_FindBufferById

==============================
*/
fragbuf_t* Netchan_FindBufferById( fragbuf_t** pplist, int id, qboolean allocate )
{
	fragbuf_t* pbuf;

	for ( pbuf = *pplist; pbuf; pbuf = pbuf->next )
	{
		if ( pbuf->bufferid == id )
			return pbuf;
	}

	if ( !allocate )
		return NULL;

	// Create new entry
	pbuf = (fragbuf_t*)Mem_ZeroMalloc( sizeof( fragbuf_t ) );
	pbuf->bufferid = id;
	pbuf->frag_message.cursize = 0;
	pbuf->frag_message.data = pbuf->frag_message_buf;
	pbuf->frag_message.maxsize = FRAGMENT_SIZE;
	pbuf->frag_message.buffername = "Frag Buffer";
	pbuf->frag_message.flags = FSB_ALLOWOVERFLOW;
	pbuf->next = NULL;

	Netchan_AddBufferToList( pplist, pbuf );

	return pbuf;
}

qboolean Netchan_Validate( netchan_t*, qboolean* frag_message, unsigned int* fragid, int* frag_offset,
	int* frag_length )
{
	int i;

	for ( i = 0; i < MAX_STREAMS; i++ )
	{
		if ( !frag_message[i] )
			continue;

		if ( fragid[i] >> 16 > MAX_FRAGMENTS || (unsigned short)fragid[i] > MAX_FRAGMENTS )
			return FALSE;

		if ( (unsigned int)frag_length[i] > MAX_FRAG_LENGTH || (unsigned int)frag_offset[i] > MAX_FRAG_OFFSET )
			return FALSE;
	}

	return TRUE;
}

/*
==============================
Netchan_AddBufferToList

==============================
*/
void Netchan_AddBufferToList( fragbuf_t** pplist, fragbuf_t* pbuf )
{
	fragbuf_t *p, *n;
	unsigned short newId;

	pbuf->next = NULL;

	if ( !pplist )
		return;

	newId = (unsigned short)( (unsigned int)pbuf->bufferid >> 16 );
	if ( !*pplist || newId < (unsigned short)( (unsigned int)( *pplist )->bufferid >> 16 ) )
	{
		pbuf->next = *pplist;
		*pplist = pbuf;
		return;
	}

	p = *pplist;
	while ( ( n = p->next ) != NULL )
	{
		if ( newId < (unsigned short)( (unsigned int)n->bufferid >> 16 ) )
		{
			// Insert here
			pbuf->next = n;
			p->next = pbuf;
			return;
		}

		p = n;
	}

	p->next = pbuf;
}

/*
==============================
Netchan_IsSending

==============================
*/
qboolean Netchan_IsSending( netchan_t* chan )
{
	return chan->fragbufs[FRAG_NORMAL_STREAM] || chan->fragbufs[FRAG_FILE_STREAM];
}

/*
==============================
Netchan_IsReceiving

==============================
*/
qboolean Netchan_IsReceiving( netchan_t* chan )
{
	return chan->incomingbufs[FRAG_NORMAL_STREAM] || chan->incomingbufs[FRAG_FILE_STREAM];
}

int Netchan_CompressPacket()
{
	return 0;
}

qboolean Netchan_DecompressPacket( sizebuf_t* )
{
	return FALSE;
}
