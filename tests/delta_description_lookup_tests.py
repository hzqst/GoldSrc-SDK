"""Regression tests for unregistered delta description names in Engine and HLTV (#465)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


ENGINE_STUBS = r'''
#include <cassert>
#include <csetjmp>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace engine_test
{
typedef unsigned char byte;
constexpr int DELTA_DESCRIPTION_NAME_BYTES = 256;
constexpr int DELTA_DESCRIPTION_COUNT_BITS = 16;

struct delta_description_t
{
	int fieldType;
	char fieldName[32];
	int fieldOffset;
};

struct delta_t
{
	int dynamic;
	int fieldCount;
	delta_description_t* pdd;
};

struct sizebuf_t
{
	int dummy;
};

static sizebuf_t net_message;
static delta_t g_MetaDelta;
static delta_t* g_RegisteredDesc = nullptr;

static const char* g_NextName = nullptr;
static unsigned int g_NextFieldCount = 0;
static int g_StartBitCount = 0;
static int g_EndBitCount = 0;
static int g_AllocCount = 0;
static int g_FreeDescCount = 0;
static int g_ParseDeltaCount = 0;
static std::string g_LastHostError;
static std::jmp_buf g_HostErrorJmp;

static void ResetState()
{
	g_NextName = nullptr;
	g_NextFieldCount = 0;
	g_StartBitCount = 0;
	g_EndBitCount = 0;
	g_AllocCount = 0;
	g_FreeDescCount = 0;
	g_ParseDeltaCount = 0;
	g_LastHostError.clear();
}

static void Q_memset( void* dst, int value, size_t size )
{
	std::memset( dst, value, size );
}

static void Q_strncpy( char* dst, const char* src, int count )
{
	std::strncpy( dst, src, static_cast<size_t>( count ) );
}

static char* MSG_ReadString()
{
	return const_cast<char*>( g_NextName );
}

[[noreturn]] static void Host_Error( const char* fmt, ... )
{
	char buf[512];
	va_list args;
	va_start( args, fmt );
	std::vsnprintf( buf, sizeof( buf ), fmt, args );
	va_end( args );
	g_LastHostError = buf;
	std::longjmp( g_HostErrorJmp, 1 );
}

static delta_t** DELTA_LookupRegistration( const char* name )
{
	if ( name && std::strcmp( name, "entity_state_t" ) == 0 )
		return &g_RegisteredDesc;
	return nullptr;
}

static void DELTA_FreeDescription( delta_t** desc )
{
	++g_FreeDescCount;
	if ( desc && *desc )
	{
		if ( ( *desc )->dynamic )
			std::free( ( *desc )->pdd );
		std::free( *desc );
		*desc = nullptr;
	}
}

static void MSG_StartBitReading( sizebuf_t* )
{
	++g_StartBitCount;
}

static void MSG_EndBitReading( sizebuf_t* )
{
	++g_EndBitCount;
}

static unsigned int MSG_ReadBits( int bits )
{
	assert( DELTA_DESCRIPTION_COUNT_BITS == bits );
	return g_NextFieldCount;
}

static void* Mem_ZeroMalloc( size_t size )
{
	++g_AllocCount;
	return std::calloc( 1, size > 0 ? size : 1 );
}

static void DELTA_ParseDelta( byte* from, byte* to, delta_t* meta, int size )
{
	assert( meta == &g_MetaDelta );
	assert( static_cast<int>( sizeof( delta_description_t ) ) == size );
	std::memcpy( to, from, static_cast<size_t>( size ) );
	reinterpret_cast<delta_description_t*>( to )->fieldOffset = 100 + g_ParseDeltaCount;
	++g_ParseDeltaCount;
}
'''

ENGINE_CLOSE = r'''
static bool InvokeExpectingHostError()
{
	if ( setjmp( g_HostErrorJmp ) == 0 )
	{
		CL_Parse_DeltaDescription();
		return false;
	}
	return true;
}

static void RunTests()
{
	// 1. Empty description name triggers Host_Error before bit reading or allocation.
	ResetState();
	g_NextName = "";
	assert( InvokeExpectingHostError() );
	assert( g_LastHostError.find( "Illegible description name" ) != std::string::npos );
	assert( 0 == g_StartBitCount );
	assert( 0 == g_AllocCount );

	// 2. Unregistered description name triggers Host_Error instead of writing through NULL.
	ResetState();
	g_NextName = "bogus_delta_t";
	g_NextFieldCount = 3;
	assert( InvokeExpectingHostError() );
	assert( g_LastHostError.find( "bogus_delta_t" ) != std::string::npos );
	assert( 0 == g_StartBitCount );
	assert( 0 == g_EndBitCount );
	assert( 0 == g_AllocCount );
	assert( 0 == g_FreeDescCount );
	assert( 0 == g_ParseDeltaCount );
	assert( nullptr == g_RegisteredDesc );

	// 3. Registered description name allocates and populates fields normally.
	ResetState();
	g_NextName = "entity_state_t";
	g_NextFieldCount = 2;
	CL_Parse_DeltaDescription();
	assert( 1 == g_StartBitCount );
	assert( 1 == g_EndBitCount );
	assert( 2 == g_AllocCount );
	assert( 0 == g_FreeDescCount );
	assert( 2 == g_ParseDeltaCount );
	assert( nullptr != g_RegisteredDesc );
	assert( 1 == g_RegisteredDesc->dynamic );
	assert( 2 == g_RegisteredDesc->fieldCount );
	assert( 100 == g_RegisteredDesc->pdd[0].fieldOffset );
	assert( 101 == g_RegisteredDesc->pdd[1].fieldOffset );

	// 4. Re-sending a registered description frees the old one and replaces it.
	ResetState();
	g_NextName = "entity_state_t";
	g_NextFieldCount = 1;
	CL_Parse_DeltaDescription();
	assert( 1 == g_FreeDescCount );
	assert( 1 == g_ParseDeltaCount );
	assert( nullptr != g_RegisteredDesc );
	assert( 1 == g_RegisteredDesc->fieldCount );
	assert( 100 == g_RegisteredDesc->pdd[0].fieldOffset );

	DELTA_FreeDescription( &g_RegisteredDesc );
	assert( nullptr == g_RegisteredDesc );
}
} // namespace engine_test
'''

HLTV_STUBS = r'''
namespace hltv_test
{
using std::memset;
using std::strncpy;

struct delta_description_t
{
	int fieldType;
	char fieldName[32];
	int fieldOffset;
};

struct delta_t
{
	int dynamic;
	int fieldCount;
	delta_description_t* pdd;
};

static delta_t g_MetaDelta;
static delta_t* g_RegisteredDesc = nullptr;
static int g_AllocCount = 0;
static int g_FreeDescCount = 0;
static int g_ParseDeltaCount = 0;
static int g_UpdateDescriptionsCount = 0;
static bool g_TriggerSizeErrorOnParse = false;

struct BitBuffer
{
	const char* nextName = nullptr;
	unsigned int nextFieldCount = 0;
	bool sizeError = false;
	int startBitModeCount = 0;
	int endBitModeCount = 0;

	char* ReadString() { return const_cast<char*>( nextName ); }
	void StartBitMode() { ++startBitModeCount; }
	void EndBitMode() { ++endBitModeCount; }
	unsigned int ReadBits( int bits )
	{
		assert( 16 == bits );
		return nextFieldCount;
	}
};

struct SystemStub
{
	int errorCount = 0;
	int printfCount = 0;
	std::string lastError;
	std::string lastPrintf;

	void Errorf( const char* fmt, ... )
	{
		char buf[512];
		va_list args;
		va_start( args, fmt );
		std::vsnprintf( buf, sizeof( buf ), fmt, args );
		va_end( args );
		lastError = buf;
		++errorCount;
	}

	void Printf( const char* fmt, ... )
	{
		char buf[512];
		va_list args;
		va_start( args, fmt );
		std::vsnprintf( buf, sizeof( buf ), fmt, args );
		va_end( args );
		lastPrintf = buf;
		++printfCount;
	}

	void DPrintf( const char*, ... ) {}
};

static void ResetCounters()
{
	g_AllocCount = 0;
	g_FreeDescCount = 0;
	g_ParseDeltaCount = 0;
	g_UpdateDescriptionsCount = 0;
	g_TriggerSizeErrorOnParse = false;
}

static delta_t** DELTA_LookupRegistration( const char* name )
{
	if ( name && std::strcmp( name, "entity_state_t" ) == 0 )
		return &g_RegisteredDesc;
	return nullptr;
}

static void DELTA_FreeDescription( delta_t** desc )
{
	++g_FreeDescCount;
	if ( desc && *desc )
	{
		if ( ( *desc )->dynamic )
			std::free( ( *desc )->pdd );
		std::free( *desc );
		*desc = nullptr;
	}
}

static void* Mem_ZeroMalloc( size_t size )
{
	++g_AllocCount;
	return std::calloc( 1, size > 0 ? size : 1 );
}

static void DELTA_ParseDelta( BitBuffer* stream, unsigned char* from, unsigned char* to, delta_t* meta, int bufferSize )
{
	assert( meta == &g_MetaDelta );
	assert( static_cast<int>( sizeof( delta_description_t ) ) == bufferSize );
	std::memcpy( to, from, sizeof( delta_description_t ) );
	reinterpret_cast<delta_description_t*>( to )->fieldOffset = 200 + g_ParseDeltaCount;
	++g_ParseDeltaCount;
	if ( g_TriggerSizeErrorOnParse )
		stream->sizeError = true;
}

static void DELTA_UpdateDescriptions()
{
	++g_UpdateDescriptionsCount;
}

class World
{
public:
	SystemStub* m_System = nullptr;
	void ParseDeltaDescription( BitBuffer* stream );
};
'''

HLTV_CLOSE = r'''
static void RunTests()
{
	SystemStub sys;
	World world;
	world.m_System = &sys;

	// 1. Empty description name logs Errorf and marks stream->sizeError so ProcessMessage stops.
	ResetCounters();
	sys = SystemStub{};
	BitBuffer emptyStream;
	emptyStream.nextName = "";
	world.ParseDeltaDescription( &emptyStream );
	assert( 1 == sys.errorCount );
	assert( true == emptyStream.sizeError );
	assert( 0 == emptyStream.startBitModeCount );
	assert( 0 == g_AllocCount );
	assert( 0 == g_UpdateDescriptionsCount );

	// 2. NULL description name also marks stream->sizeError cleanly.
	ResetCounters();
	sys = SystemStub{};
	BitBuffer nullStream;
	nullStream.nextName = nullptr;
	world.ParseDeltaDescription( &nullStream );
	assert( 1 == sys.errorCount );
	assert( true == nullStream.sizeError );
	assert( 0 == nullStream.startBitModeCount );
	assert( 0 == g_AllocCount );

	// 3. Unregistered description name warns and sets stream->sizeError without crashing or stopping system.
	ResetCounters();
	sys = SystemStub{};
	BitBuffer bogusStream;
	bogusStream.nextName = "bogus_delta_t";
	bogusStream.nextFieldCount = 4;
	world.ParseDeltaDescription( &bogusStream );
	assert( 0 == sys.errorCount );
	assert( 1 == sys.printfCount );
	assert( sys.lastPrintf.find( "bogus_delta_t" ) != std::string::npos );
	assert( true == bogusStream.sizeError );
	assert( 0 == bogusStream.startBitModeCount );
	assert( 0 == bogusStream.endBitModeCount );
	assert( 0 == g_AllocCount );
	assert( 0 == g_FreeDescCount );
	assert( 0 == g_ParseDeltaCount );
	assert( 0 == g_UpdateDescriptionsCount );
	assert( nullptr == g_RegisteredDesc );

	// 4. Registered description name parses fields and updates cached descriptions normally.
	ResetCounters();
	sys = SystemStub{};
	BitBuffer validStream;
	validStream.nextName = "entity_state_t";
	validStream.nextFieldCount = 2;
	world.ParseDeltaDescription( &validStream );
	assert( 0 == sys.errorCount );
	assert( 0 == sys.printfCount );
	assert( false == validStream.sizeError );
	assert( 1 == validStream.startBitModeCount );
	assert( 1 == validStream.endBitModeCount );
	assert( 2 == g_AllocCount );
	assert( 0 == g_FreeDescCount );
	assert( 2 == g_ParseDeltaCount );
	assert( 1 == g_UpdateDescriptionsCount );
	assert( nullptr != g_RegisteredDesc );
	assert( 1 == g_RegisteredDesc->dynamic );
	assert( 2 == g_RegisteredDesc->fieldCount );
	assert( 200 == g_RegisteredDesc->pdd[0].fieldOffset );
	assert( 201 == g_RegisteredDesc->pdd[1].fieldOffset );

	// 5. Re-sending a registered description frees the previous entry first.
	ResetCounters();
	BitBuffer replaceStream;
	replaceStream.nextName = "entity_state_t";
	replaceStream.nextFieldCount = 1;
	world.ParseDeltaDescription( &replaceStream );
	assert( false == replaceStream.sizeError );
	assert( 1 == g_FreeDescCount );
	assert( 1 == g_ParseDeltaCount );
	assert( 1 == g_UpdateDescriptionsCount );
	assert( 1 == g_RegisteredDesc->fieldCount );
	assert( 200 == g_RegisteredDesc->pdd[0].fieldOffset );

	DELTA_FreeDescription( &g_RegisteredDesc );
	assert( nullptr == g_RegisteredDesc );

	// 6. Stream sizeError mid-loop stops parsing remaining fields early.
	ResetCounters();
	g_TriggerSizeErrorOnParse = true;
	BitBuffer truncatedStream;
	truncatedStream.nextName = "entity_state_t";
	truncatedStream.nextFieldCount = 500;
	world.ParseDeltaDescription( &truncatedStream );
	assert( true == truncatedStream.sizeError );
	assert( 1 == g_ParseDeltaCount );

	DELTA_FreeDescription( &g_RegisteredDesc );
	assert( nullptr == g_RegisteredDesc );
}
} // namespace hltv_test

namespace hltv_parse_delta_test
{
#define DT_BYTE				1
#define DT_SHORT			2
#define DT_FLOAT			4
#define DT_INTEGER			8
#define DT_ANGLE			16
#define DT_TIMEWINDOW_8		32
#define DT_TIMEWINDOW_BIG	64
#define DT_STRING			128
#define DT_SIGNED			( 1u << 31 )

struct delta_stats_t
{
	int sendcount;
	int receivedcount;
};

struct delta_description_t
{
	int fieldType;
	char fieldName[32];
	int fieldOffset;
	short fieldSize;
	int significant_bits;
	float premultiply;
	float postmultiply;
	short flags;
	delta_stats_t stats;
};

struct delta_s
{
	int dynamic;
	int fieldCount;
	char conditionalencodename[32];
	void* conditionalencode;
	delta_description_t* pdd;
};

static bool g_large_Time_Buffers = false;
static double g_delta_Time = 0.0;

static bool DELTA_ScaleNearOne( float scale )
{
	return scale >= 0.9999f && scale <= 1.0001f;
}

class BitBuffer
{
public:
	unsigned char* data = nullptr;
	int maxSize = 0;
	int bitPos = 0;
	bool sizeError = false;

	BitBuffer( unsigned char* buf, int size ) : data( buf ), maxSize( size ) {}

	int CurrentBit() const { return bitPos; }

	unsigned int ReadBits( int n )
	{
		if ( sizeError || ( bitPos / 8 ) >= maxSize )
		{
			sizeError = true;
			return static_cast<unsigned int>( -1 );
		}
		unsigned int result = 0;
		for ( int i = 0; i < n; ++i )
		{
			int byteIdx = bitPos / 8;
			int bitIdx = bitPos % 8;
			if ( byteIdx >= maxSize )
			{
				sizeError = true;
				return static_cast<unsigned int>( -1 );
			}
			unsigned int bit = ( data[byteIdx] >> bitIdx ) & 1u;
			result |= ( bit << i );
			++bitPos;
		}
		return result;
	}

	int ReadSBits( int n )
	{
		unsigned int sign = ReadBits( 1 );
		int val = static_cast<int>( ReadBits( n - 1 ) );
		return sign ? -val : val;
	}

	float ReadBitAngle( int n )
	{
		unsigned int ui = ReadBits( n );
		return static_cast<float>( ui ) * ( 360.0f / static_cast<float>( 1 << n ) );
	}

	float ReadFloat()
	{
		unsigned int raw = ReadBits( 32 );
		float f = 0.0f;
		std::memcpy( &f, &raw, sizeof( f ) );
		return f;
	}
};

struct BitWriter
{
	unsigned char buf[128] = {};
	int bitPos = 0;

	void WriteBits( unsigned int value, int n )
	{
		for ( int i = 0; i < n; ++i )
		{
			if ( ( value >> i ) & 1u )
				buf[bitPos / 8] |= static_cast<unsigned char>( 1u << ( bitPos % 8 ) );
			++bitPos;
		}
	}

	int ByteCount() const { return ( bitPos + 7 ) / 8; }
};
'''

HLTV_PARSE_DELTA_CLOSE = r'''
static void RunTests()
{
	// 1. Truncated DT_STRING (hits sizeError mid-string): must terminate and NUL-terminate without OOB/infinite loop.
	{
		BitWriter w;
		w.WriteBits( 1, 3 );    // byteCount = 1
		w.WriteBits( 0x01, 8 ); // field 0 marked
		w.WriteBits( 'A', 8 );
		w.WriteBits( 'B', 8 );  // truncated before NUL!

		BitBuffer stream( w.buf, w.ByteCount() );
		delta_description_t desc[1] = {};
		desc[0].fieldType = DT_STRING;
		desc[0].fieldOffset = 0;
		delta_s fields = { 0, 1, "", nullptr, desc };

		unsigned char from[8] = {};
		unsigned char* to = static_cast<unsigned char*>( std::malloc( 8 ) );
		std::memset( to, 0xCC, 8 );
		DELTA_ParseDelta( &stream, from, to, &fields, 8 );
		assert( true == stream.sizeError );
		assert( 'A' == to[0] );
		assert( 'B' == to[1] );
		assert( '\0' == to[2] );
		std::free( to );
	}

	// 2. Overlong DT_STRING: clamps writes to bufferSize-1 + NUL and drains remaining wire bytes so field 1 stays aligned.
	{
		BitWriter w;
		w.WriteBits( 1, 3 );    // byteCount = 1
		w.WriteBits( 0x03, 8 ); // fields 0 and 1 marked
		const char* longStr = "0123456789ABCDEF";
		for ( const char* p = longStr; ; ++p )
		{
			w.WriteBits( static_cast<unsigned char>( *p ), 8 );
			if ( *p == '\0' )
				break;
		}
		w.WriteBits( 0x5A, 8 ); // field 1 (DT_BYTE)

		BitBuffer stream( w.buf, w.ByteCount() );
		delta_description_t desc[2] = {};
		desc[0].fieldType = DT_STRING;
		desc[0].fieldOffset = 2; // only 6 bytes remain in 8-byte buffer
		desc[1].fieldType = DT_BYTE;
		desc[1].fieldOffset = 0;
		desc[1].significant_bits = 8;
		desc[1].premultiply = 1.0f;
		delta_s fields = { 0, 2, "", nullptr, desc };

		unsigned char from[8] = {};
		unsigned char* to = static_cast<unsigned char*>( std::calloc( 1, 8 ) );
		DELTA_ParseDelta( &stream, from, to, &fields, 8 );
		assert( false == stream.sizeError );
		assert( 0x5A == to[0] );
		assert( 0 == std::strcmp( reinterpret_cast<char*>( to + 2 ), "01234" ) );
		std::free( to );
	}

	// 3. Unmarked DT_STRING: stops at NUL even when non-zero bytes follow, and clamps non-NUL-terminated source.
	{
		BitWriter w;
		w.WriteBits( 0, 3 ); // byteCount = 0 (all unmarked)
		BitBuffer stream( w.buf, w.ByteCount() );

		delta_description_t desc[1] = {};
		desc[0].fieldType = DT_STRING;
		desc[0].fieldOffset = 0;
		delta_s fields = { 0, 1, "", nullptr, desc };

		unsigned char from1[6] = { '\0', 'X', 'Y', 'Z', 'W', '!' };
		unsigned char* to1 = static_cast<unsigned char*>( std::malloc( 6 ) );
		std::memset( to1, 0xAA, 6 );
		DELTA_ParseDelta( &stream, from1, to1, &fields, 6 );
		assert( '\0' == to1[0] );
		assert( 0xAA == to1[1] ); // must NOT read/copy past leading NUL
		std::free( to1 );

		BitBuffer stream2( w.buf, w.ByteCount() );
		unsigned char from2[4] = { 'A', 'B', 'C', 'D' }; // no NUL
		unsigned char* to2 = static_cast<unsigned char*>( std::malloc( 4 ) );
		std::memset( to2, 0xAA, 4 );
		DELTA_ParseDelta( &stream2, from2, to2, &fields, 4 );
		assert( 0 == std::strcmp( reinterpret_cast<char*>( to2 ), "ABC" ) );
		std::free( to2 );
	}

	// 4. Out-of-bounds fieldOffset (negative, >= bufferSize, or partial overflow) must not write outside bufferSize.
	{
		BitWriter w;
		w.WriteBits( 1, 3 );
		w.WriteBits( 0x0F, 8 ); // 4 fields marked
		BitBuffer stream( w.buf, w.ByteCount() );

		delta_description_t desc[4] = {};
		desc[0].fieldType = DT_BYTE;
		desc[0].fieldOffset = -1;
		desc[1].fieldType = DT_BYTE;
		desc[1].fieldOffset = 4;
		desc[2].fieldType = DT_SHORT;
		desc[2].fieldOffset = 3; // needs 2 bytes, only 1 left
		desc[3].fieldType = DT_INTEGER;
		desc[3].fieldOffset = 2; // needs 4 bytes, only 2 left
		delta_s fields = { 0, 4, "", nullptr, desc };

		unsigned char from[4] = {};
		unsigned char* to = static_cast<unsigned char*>( std::calloc( 1, 4 ) );
		DELTA_ParseDelta( &stream, from, to, &fields, 4 );
		for ( int i = 0; i < 4; ++i )
			assert( 0 == to[i] );
		std::free( to );
	}

	// 5. fieldCount > 64: index 64 must not alias bit 32 in words[1].
	{
		BitWriter w;
		w.WriteBits( 5, 3 ); // 5 mask bytes (bits 0..39)
		w.WriteBits( 0, 8 );
		w.WriteBits( 0, 8 );
		w.WriteBits( 0, 8 );
		w.WriteBits( 0, 8 );
		w.WriteBits( 0x01, 8 ); // bit 32 set!
		w.WriteBits( 0x77, 8 ); // value for field 32
		BitBuffer stream( w.buf, w.ByteCount() );

		delta_description_t desc[65] = {};
		for ( int i = 0; i < 65; ++i )
		{
			desc[i].fieldType = DT_BYTE;
			desc[i].fieldOffset = 2; // scratch offset
			desc[i].significant_bits = 8;
			desc[i].premultiply = 1.0f;
		}
		desc[32].fieldOffset = 0;
		desc[64].fieldOffset = 1;
		delta_s fields = { 0, 65, "", nullptr, desc };

		unsigned char from[4] = { 0x11, 0x22, 0x00, 0x00 };
		unsigned char to[4] = {};
		DELTA_ParseDelta( &stream, from, to, &fields, 4 );
		assert( false == stream.sizeError );
		assert( 0x77 == to[0] ); // field 32 read from stream
		assert( 0x22 == to[1] ); // field 64 copied from baseline, NOT read from stream
	}
}
} // namespace hltv_parse_delta_test

int main()
{
	engine_test::RunTests();
	hltv_test::RunTests();
	hltv_parse_delta_test::RunTests();
	return 0;
}
'''


def main():
    engine_fn = extract('engine/cl_parse.cpp', 'void CL_Parse_DeltaDescription()\n{')
    hltv_fn = extract('HLTV/Core/World.cpp', 'void World::ParseDeltaDescription( BitBuffer* stream )\n{')
    hltv_parse_delta_fn = extract(
        'HLTV/Core/delta.cpp',
        'int DELTA_ParseDelta( BitBuffer* stream, unsigned char* from, unsigned char* to, delta_s* pFields, int bufferSize )\n{',
    )
    with tempfile.TemporaryDirectory(prefix='goldsrc-delta-description-lookup-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text(
            '\n'.join([
                ENGINE_STUBS, engine_fn, ENGINE_CLOSE,
                HLTV_STUBS, hltv_fn, HLTV_CLOSE,
                hltv_parse_delta_fn, HLTV_PARSE_DELTA_CLOSE,
            ]),
            encoding='utf-8',
        )
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: delta description lookup and HLTV DELTA_ParseDelta bounds checks')


if __name__ == '__main__':
    main()
