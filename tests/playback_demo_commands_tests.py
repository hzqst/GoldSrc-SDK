"""Exercise reconnect state changes across local, live and playback commands."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature, occurrence=0, end='\n}'):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = -1
    for _ in range(occurrence + 1):
        start = source.index(signature, start + 1)
    return source[start:source.index(end, start) + len(end)]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#define _snprintf std::snprintf
constexpr int ca_disconnected = 0, ca_connecting = 1, ca_connected = 2, ca_active = 3, ca_dedicated = 4;
constexpr int clc_stringcmd = 3, TRUE = 1;
constexpr int DEMO_HUD_COMMAND_LEN = 64, DEMO_HUD_COMMAND = 3, FALSE = 0;
static struct { int state, signon; bool demoplayback, passive; int connect_stream, demofile; struct { int message; } netchan; } cls;
static float g_demotimescale = 1.0f;
static bool privileged, plaque;
static bool Cmd_CurrentCommandIsPrivileged() { return privileged; }
static void Q_strncpy_s(char* out, const char* in, size_t n) { std::snprintf(out, n, "%s", in); }
static const char* NET_AdrToString(int) { return "unused"; }
static void Cbuf_AddText(const char*) {}
static void SCR_BeginLoadingPlaque(int) { plaque = true; }
static void Netchan_Clear(void*) {}
static void SZ_Clear(void*) {}
static void MSG_WriteByte(void*, int) {}
static void MSG_WriteString(void*, const char*) {}
static std::string queued;
static int bytesAvailable = DEMO_HUD_COMMAND_LEN;
static bool ended;
static int FS_Read(char* out, int count, int, int)
{
    assert(DEMO_HUD_COMMAND_LEN == count);
    std::memset(out, 'a', bytesAvailable);
    return bytesAvailable;
}
static void Host_EndGame(const char*) { ended = true; }
static bool ValidStuffText(const char* command) { return DEMO_HUD_COMMAND_LEN == std::strlen(command); }
static void Cbuf_AddTextToUntrustedBuffer(const char* text) { queued += text; }
'''

TESTS = r'''
int main()
{
    cls.state = ca_active;
    cls.signon = 4;
    cls.demoplayback = true;
    Host_Reconnect_f();
    assert(!plaque && ca_active == cls.state && 4 == cls.signon);
    privileged = true;
    Host_Reconnect_f();
    assert(plaque && ca_connected == cls.state && 0 == cls.signon);
    plaque = false;
    cls.state = ca_active;
    cls.demoplayback = false;
    privileged = false;
    Host_Reconnect_f();
    assert(plaque && ca_connected == cls.state);

    assert(ReadHudCommand());
    assert(std::string(DEMO_HUD_COMMAND_LEN, 'a') + "\n" == queued);
    bytesAvailable = DEMO_HUD_COMMAND_LEN - 1;
    queued.clear();
    assert(!ReadHudCommand());
    assert(ended && queued.empty());
}
'''


def main():
    demo = (ROOT / 'cl_demo.cpp').read_text(encoding='utf-8')
    declaration = next(line for line in demo.splitlines() if 'char text[DEMO_HUD_COMMAND_LEN' in line)
    start = demo.index('case DEMO_HUD_COMMAND:', demo.index('qboolean CL_ReadDemoMessage_OLD('))
    end = demo.index('case DEMO_CLIENT_DATA:', start)
    production = [
        extract("host_cmd.cpp", "void Host_Reconnect_f()\n{"),
        'bool ReadHudCommand() {\n'
        + declaration
        + '\nfor (int n = 0; n < 1; ++n) { switch (DEMO_HUD_COMMAND) {\n'
        + demo[start:end]
        + '\n}} return true; }',
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-playback-demo-commands-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
