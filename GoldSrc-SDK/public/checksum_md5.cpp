//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================
#include "checksum_md5.h"

#include <string.h>

#define MD5_F1( x, y, z )	( ( z ) ^ ( ( x ) & ( ( y ) ^ ( z ) ) ) )
#define MD5_F2( x, y, z )	MD5_F1( z, x, y )
#define MD5_F3( x, y, z )	( ( x ) ^ ( y ) ^ ( z ) )
#define MD5_F4( x, y, z )	( ( y ) ^ ( ( x ) | ~( z ) ) )

#define MD5_STEP( function, w, x, y, z, data, shift ) \
	do \
	{ \
		( w ) += function( x, y, z ) + ( data ); \
		( w ) = ( ( w ) << ( shift ) ) | ( ( w ) >> ( 32 - ( shift ) ) ); \
		( w ) += ( x ); \
	} while ( 0 )

/*
==================
MD5Init

Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious initialization constants.
==================
*/
void MD5Init( MD5Context_t* context )
{
	context->buf[0] = 0x67452301;
	context->buf[1] = 0xEFCDAB89;
	context->buf[2] = 0x98BADCFE;
	context->buf[3] = 0x10325476;
	context->bits[0] = 0;
	context->bits[1] = 0;
}

void MD5Update( MD5Context_t* context, const unsigned char* input, unsigned int length )
{
	unsigned int count = context->bits[0];
	context->bits[0] = count + ( length << 3 );
	if ( context->bits[0] < count )
		context->bits[1]++; /* Carry from low to high */
	context->bits[1] += length >> 29;

	count = ( count >> 3 ) & 63;
	if ( count )
	{
		unsigned char* tail = context->in + count;
		count = sizeof( context->in ) - count;
		if ( length < count )
		{
			memcpy( tail, input, length );
			return;
		}

		memcpy( tail, input, count );
		MD5Transform( context->buf, (const unsigned int*)context->in );
		input += count;
		length -= count;
	}

	while ( length >= sizeof( context->in ) )
	{
		memcpy( context->in, input, sizeof( context->in ) );
		MD5Transform( context->buf, (const unsigned int*)context->in );
		input += sizeof( context->in );
		length -= sizeof( context->in );
	}

	memcpy( context->in, input, length );
}

/*
===============
MD5Final


Final wrapup - pad to 64-byte boundary with the bit pattern 
1 0* (64-bit count of bits processed, MSB-first)
===============
*/
void MD5Final( unsigned char digest[16], MD5Context_t* context )
{
	unsigned int count = ( context->bits[0] >> 3 ) & 63;
	unsigned char* tail = context->in + count;
	*tail++ = 0x80;
	count = sizeof( context->in ) - 1 - count;
	if ( count < sizeof( context->bits ) )
	{
		/* Two lots of padding:  Pad the first block to 64 bytes */
		memset( tail, 0, count );
		MD5Transform( context->buf, (const unsigned int*)context->in );
		memset( context->in, 0, sizeof( context->in ) - sizeof( context->bits ) );
	}
	else
	{
		memset( tail, 0, count - sizeof( context->bits ) );
	}

	memcpy( context->in + sizeof( context->in ) - sizeof( context->bits ), context->bits, sizeof( context->bits ) );
	MD5Transform( context->buf, (const unsigned int*)context->in );
	memcpy( digest, context->buf, sizeof( context->buf ) );
	context->buf[0] = 0;
}

void MD5Transform( unsigned int state[4], const unsigned int block[16] )
{
	unsigned int a = state[0];
	unsigned int b = state[1];
	unsigned int c = state[2];
	unsigned int d = state[3];

	MD5_STEP( MD5_F1, a, b, c, d, block[0] + 0xD76AA478, 7 );
	MD5_STEP( MD5_F1, d, a, b, c, block[1] + 0xE8C7B756, 12 );
	MD5_STEP( MD5_F1, c, d, a, b, block[2] + 0x242070DB, 17 );
	MD5_STEP( MD5_F1, b, c, d, a, block[3] + 0xC1BDCEEE, 22 );
	MD5_STEP( MD5_F1, a, b, c, d, block[4] + 0xF57C0FAF, 7 );
	MD5_STEP( MD5_F1, d, a, b, c, block[5] + 0x4787C62A, 12 );
	MD5_STEP( MD5_F1, c, d, a, b, block[6] + 0xA8304613, 17 );
	MD5_STEP( MD5_F1, b, c, d, a, block[7] + 0xFD469501, 22 );
	MD5_STEP( MD5_F1, a, b, c, d, block[8] + 0x698098D8, 7 );
	MD5_STEP( MD5_F1, d, a, b, c, block[9] + 0x8B44F7AF, 12 );
	MD5_STEP( MD5_F1, c, d, a, b, block[10] + 0xFFFF5BB1, 17 );
	MD5_STEP( MD5_F1, b, c, d, a, block[11] + 0x895CD7BE, 22 );
	MD5_STEP( MD5_F1, a, b, c, d, block[12] + 0x6B901122, 7 );
	MD5_STEP( MD5_F1, d, a, b, c, block[13] + 0xFD987193, 12 );
	MD5_STEP( MD5_F1, c, d, a, b, block[14] + 0xA679438E, 17 );
	MD5_STEP( MD5_F1, b, c, d, a, block[15] + 0x49B40821, 22 );

	MD5_STEP( MD5_F2, a, b, c, d, block[1] + 0xF61E2562, 5 );
	MD5_STEP( MD5_F2, d, a, b, c, block[6] + 0xC040B340, 9 );
	MD5_STEP( MD5_F2, c, d, a, b, block[11] + 0x265E5A51, 14 );
	MD5_STEP( MD5_F2, b, c, d, a, block[0] + 0xE9B6C7AA, 20 );
	MD5_STEP( MD5_F2, a, b, c, d, block[5] + 0xD62F105D, 5 );
	MD5_STEP( MD5_F2, d, a, b, c, block[10] + 0x02441453, 9 );
	MD5_STEP( MD5_F2, c, d, a, b, block[15] + 0xD8A1E681, 14 );
	MD5_STEP( MD5_F2, b, c, d, a, block[4] + 0xE7D3FBC8, 20 );
	MD5_STEP( MD5_F2, a, b, c, d, block[9] + 0x21E1CDE6, 5 );
	MD5_STEP( MD5_F2, d, a, b, c, block[14] + 0xC33707D6, 9 );
	MD5_STEP( MD5_F2, c, d, a, b, block[3] + 0xF4D50D87, 14 );
	MD5_STEP( MD5_F2, b, c, d, a, block[8] + 0x455A14ED, 20 );
	MD5_STEP( MD5_F2, a, b, c, d, block[13] + 0xA9E3E905, 5 );
	MD5_STEP( MD5_F2, d, a, b, c, block[2] + 0xFCEFA3F8, 9 );
	MD5_STEP( MD5_F2, c, d, a, b, block[7] + 0x676F02D9, 14 );
	MD5_STEP( MD5_F2, b, c, d, a, block[12] + 0x8D2A4C8A, 20 );

	MD5_STEP( MD5_F3, a, b, c, d, block[5] + 0xFFFA3942, 4 );
	MD5_STEP( MD5_F3, d, a, b, c, block[8] + 0x8771F681, 11 );
	MD5_STEP( MD5_F3, c, d, a, b, block[11] + 0x6D9D6122, 16 );
	MD5_STEP( MD5_F3, b, c, d, a, block[14] + 0xFDE5380C, 23 );
	MD5_STEP( MD5_F3, a, b, c, d, block[1] + 0xA4BEEA44, 4 );
	MD5_STEP( MD5_F3, d, a, b, c, block[4] + 0x4BDECFA9, 11 );
	MD5_STEP( MD5_F3, c, d, a, b, block[7] + 0xF6BB4B60, 16 );
	MD5_STEP( MD5_F3, b, c, d, a, block[10] + 0xBEBFBC70, 23 );
	MD5_STEP( MD5_F3, a, b, c, d, block[13] + 0x289B7EC6, 4 );
	MD5_STEP( MD5_F3, d, a, b, c, block[0] + 0xEAA127FA, 11 );
	MD5_STEP( MD5_F3, c, d, a, b, block[3] + 0xD4EF3085, 16 );
	MD5_STEP( MD5_F3, b, c, d, a, block[6] + 0x04881D05, 23 );
	MD5_STEP( MD5_F3, a, b, c, d, block[9] + 0xD9D4D039, 4 );
	MD5_STEP( MD5_F3, d, a, b, c, block[12] + 0xE6DB99E5, 11 );
	MD5_STEP( MD5_F3, c, d, a, b, block[15] + 0x1FA27CF8, 16 );
	MD5_STEP( MD5_F3, b, c, d, a, block[2] + 0xC4AC5665, 23 );

	MD5_STEP( MD5_F4, a, b, c, d, block[0] + 0xF4292244, 6 );
	MD5_STEP( MD5_F4, d, a, b, c, block[7] + 0x432AFF97, 10 );
	MD5_STEP( MD5_F4, c, d, a, b, block[14] + 0xAB9423A7, 15 );
	MD5_STEP( MD5_F4, b, c, d, a, block[5] + 0xFC93A039, 21 );
	MD5_STEP( MD5_F4, a, b, c, d, block[12] + 0x655B59C3, 6 );
	MD5_STEP( MD5_F4, d, a, b, c, block[3] + 0x8F0CCC92, 10 );
	MD5_STEP( MD5_F4, c, d, a, b, block[10] + 0xFFEFF47D, 15 );
	MD5_STEP( MD5_F4, b, c, d, a, block[1] + 0x85845DD1, 21 );
	MD5_STEP( MD5_F4, a, b, c, d, block[8] + 0x6FA87E4F, 6 );
	MD5_STEP( MD5_F4, d, a, b, c, block[15] + 0xFE2CE6E0, 10 );
	MD5_STEP( MD5_F4, c, d, a, b, block[6] + 0xA3014314, 15 );
	MD5_STEP( MD5_F4, b, c, d, a, block[13] + 0x4E0811A1, 21 );
	MD5_STEP( MD5_F4, a, b, c, d, block[4] + 0xF7537E82, 6 );
	MD5_STEP( MD5_F4, d, a, b, c, block[11] + 0xBD3AF235, 10 );
	MD5_STEP( MD5_F4, c, d, a, b, block[2] + 0x2AD7D2BB, 15 );
	MD5_STEP( MD5_F4, b, c, d, a, block[9] + 0xEB86D391, 21 );

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

char* MD5_Print( unsigned char hash[16] )
{
	static char text[64];
	static const char digits[] = "0123456789abcdef";
	memset( text, 0, sizeof( text ) );
	for ( unsigned int i = 0; i < 16; i++ )
	{
		text[i * 2] = digits[hash[i] >> 4];
		text[i * 2 + 1] = digits[hash[i] & 0x0F];
	}

	return text;
}
