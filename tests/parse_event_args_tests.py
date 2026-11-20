"""Exercise production CL_ParseEvent on events sent with and without arguments in one message."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstring>
#include <deque>
#include <vector>
typedef unsigned char byte;
typedef float vec3_t[3];
enum { PITCH, YAW, ROLL };
#define EVENT_COUNT_BITS	5
#define EVENT_INDEX_BITS	10
#define EVENT_ENTITY_BITS	11
#define EVENT_DELAY_BITS	16
struct event_args_t
{
	int flags, entindex;
	vec3_t origin, angles, velocity;
	int ducking;
	float fparam1, fparam2;
	int iparam1, iparam2, bparam1, bparam2;
};
struct entity_state_t { int number; vec3_t origin, angles; int usehull; };
struct packet_entities_t { int num_entities; entity_state_t* entities; };
struct frame_t { packet_entities_t packet_entities; };
struct delta_t {};
struct sizebuf_t {};
static struct { frame_t frames[1]; int parsecountmod; int maxclients; } cl;
static sizebuf_t net_message;
static delta_t eventDelta;
static delta_t* eventDeltaPointer = &eventDelta;
static std::deque<unsigned> bits;
static std::deque<event_args_t> sentArgs;
static std::vector<event_args_t> queued;
static void Q_memset( void* d, int c, size_t n ) {std::memset(d,c,n);}
static delta_t** DELTA_LookupRegistration( const char* ) {return &eventDeltaPointer;}
static void MSG_StartBitReading( sizebuf_t* ) {}
static void MSG_EndBitReading( sizebuf_t* ) {}
static unsigned MSG_ReadBits( int ) {unsigned v=bits.front();bits.pop_front();return v;}
// Like the real delta parser: start from the baseline, then apply the fields that were sent
static void DELTA_ParseDelta( byte* from, byte* to, delta_t*, int size ) {std::memcpy(to,from,size);*(event_args_t*)to=sentArgs.front();sentArgs.pop_front();}
static bool VectorCompare( const float* a, const float* b ) {return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2];}
static void VectorCopy( const float* a, float* b ) {b[0]=a[0];b[1]=a[1];b[2]=a[2];}
static vec3_t vec3_origin;
static void COM_NormalizeAngles( float* ) {}
static void Con_DPrintf( const char*, ... ) {}
static void CL_CalcPlayerVelocity( int, float* velocity ) {velocity[0]=velocity[1]=velocity[2]=9.0f;}
static void CL_QueueEvent( int, int, float, event_args_t* args ) {queued.push_back(*args);}
'''

TESTS = r'''
// One event in the message: index, packet index, optional arguments, no delay
static void Event( int packetIndex, const event_args_t* args )
{
	bits.insert(bits.end(),{1u,1u,(unsigned)packetIndex,args?1u:0u});
	if(args) sentArgs.push_back(*args);
	bits.push_back(0);
}

int main() {
	entity_state_t entities[2] = {};
	entities[0].number=1;
	entities[1].number=5;entities[1].origin[0]=10;entities[1].origin[1]=20;entities[1].origin[2]=30;
	cl.frames[0].packet_entities={2,entities};
	cl.maxclients=1;

	event_args_t player = {};
	player.origin[0]=1;player.origin[1]=2;player.origin[2]=3;player.angles[PITCH]=4;player.fparam1=7;player.iparam1=8;

	// A player event with arguments, then an event of entity 5 that the server sent without arguments
	bits.push_back(2);
	Event(0,&player);
	Event(1,nullptr);
	CL_ParseEvent();
	assert(bits.empty());assert(2==queued.size());

	assert(1==queued[0].entindex);assert(7==queued[0].fparam1);assert(9==queued[0].velocity[0]);

	// The second event takes its entity's origin and none of the first event's arguments
	const event_args_t& second=queued[1];
	assert(5==second.entindex);
	assert(10==second.origin[0]);assert(20==second.origin[1]);assert(30==second.origin[2]);
	assert(0==second.angles[PITCH]);
	assert(0==second.fparam1);assert(0==second.iparam1);
	assert(0==second.velocity[0]);assert(0==second.ducking);
}
'''


def main():
    source = (ROOT / 'cl_parsefn.cpp').read_text()
    code = STUBS + function(source, 'void CL_ParseEvent()') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-parse-event-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)
    print('PASS: CL_ParseEvent does not carry arguments to an event sent without them')


if __name__ == '__main__':
    main()
