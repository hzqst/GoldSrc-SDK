"""Exercise Host_Frame profiling when host_profile changes while the frame runs."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature, end='\n}'):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index(end, start) + len(end)]


STUBS = r'''
#include <cassert>
#include <csetjmp>
struct cvar_t { float value; };
struct client_t { int active; };
static cvar_t host_profile;
static std::jmp_buf host_abortserver;
static int giActive;
static int giStateInfo;
static int g_iQuitCommandIssued;
static client_t clients[2] = {{1}, {0}};
static struct { int maxclients; client_t* clients; } svs = {2, clients};
static float profileDuringFrame;
static int timeSamples;
static int profileReports;
static double Sys_FloatTime() { return ++timeSamples; }
static void _Host_Frame(float) { host_profile.value = profileDuringFrame; }
static void Cbuf_Execute() {}
static void Con_Printf(const char*, ...) { ++profileReports; }
'''

TESTS = r'''
static void RunFrames(float profileBefore, float profileDuring, int frames)
{
    for (int frame = 0; frame < frames; ++frame)
    {
        int stateInfo = -1;
        host_profile.value = profileBefore;
        profileDuringFrame = profileDuring;
        Host_Frame(0.0f, 1, &stateInfo);
        assert(0 == stateInfo);
    }
}

int main()
{
    constexpr int PROFILE_SAMPLE_FRAMES = 1000;

    RunFrames(0.0f, 1.0f, PROFILE_SAMPLE_FRAMES);
    assert(0 == timeSamples && 0 == profileReports);

    RunFrames(1.0f, 0.0f, PROFILE_SAMPLE_FRAMES - 1);
    assert(2 * (PROFILE_SAMPLE_FRAMES - 1) == timeSamples && 0 == profileReports);
    RunFrames(1.0f, 0.0f, 1);
    assert(2 * PROFILE_SAMPLE_FRAMES == timeSamples && 1 == profileReports);
}
'''


def main():
    production = extract('host.cpp', 'int Host_Frame( float time, int iState, int* stateInfo )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-host-profile-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
