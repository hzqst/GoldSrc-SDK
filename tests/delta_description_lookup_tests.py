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

static void DELTA_ParseDelta( BitBuffer*, unsigned char* from, unsigned char* to, delta_t* meta )
{
	assert( meta == &g_MetaDelta );
	std::memcpy( to, from, sizeof( delta_description_t ) );
	reinterpret_cast<delta_description_t*>( to )->fieldOffset = 200 + g_ParseDeltaCount;
	++g_ParseDeltaCount;
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
}
} // namespace hltv_test

int main()
{
	engine_test::RunTests();
	hltv_test::RunTests();
	return 0;
}
'''


def main():
    engine_fn = extract('engine/cl_parse.cpp', 'void CL_Parse_DeltaDescription()\n{')
    hltv_fn = extract('HLTV/Core/World.cpp', 'void World::ParseDeltaDescription( BitBuffer* stream )\n{')
    with tempfile.TemporaryDirectory(prefix='goldsrc-delta-description-lookup-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text(
            '\n'.join([ENGINE_STUBS, engine_fn, ENGINE_CLOSE, HLTV_STUBS, hltv_fn, HLTV_CLOSE]),
            encoding='utf-8',
        )
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: delta description lookup rejects unregistered names in Engine and HLTV')


if __name__ == '__main__':
    main()
