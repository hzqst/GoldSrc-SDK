"""Exercise SV_SendServerinfo mapcycle truncation so client MSG_ReadString consumes the terminator."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#define _snprintf snprintf
#endif

using byte = unsigned char;
using CRC32_t = unsigned int;
constexpr int PROTOCOL_VERSION = 48;
constexpr int svc_print = 8;
constexpr int svc_serverinfo = 11;
constexpr int svc_sendextrainfo = 54;
constexpr int svc_cdtrack = 32;
constexpr int svc_setview = 5;

struct sizebuf_t {
    std::vector<byte> data;
};
struct edict_t {};
struct client_t {
    edict_t* edict = nullptr;
    int spawned = 0;
    int connected = 0;
    int fully_connected = 0;
};

static struct { float value = 0; } developer, coop, deathmatch, sv_cheats;
static struct { const char* string = "mapcycle.txt"; } mapcyclefile;
static struct { int maxclients = 16; int spawncount = 1; } svs;
static struct { CRC32_t worldmapCRC = 0x12345678; byte clientdllmd5[16] = {}; const char* modelname = "maps/c1a0.bsp"; } sv;
static struct { int cdAudioTrack = 0; } gGlobalVariables;
static char com_gamedir[64] = "valve";
static char com_clientfallback[64] = "valve";

static int build_number() { return 10210; }
static int NUM_FOR_EDICT(edict_t*) { return 1; }
static void COM_Munge3(byte*, int, byte) {}
static void COM_FileBase(const char* in, char* out) { std::strcpy(out, in); }
static const char* Cvar_VariableString(const char*) { return "TestServer"; }
static void SV_WriteDeltaDescriptionsToClient(sizebuf_t*) {}
static void SV_SetMoveVars() {}
static void SV_WriteMovevarsToClient(sizebuf_t*) {}

static std::string g_mapcycleContent;
static byte* COM_LoadFileForMe(const char*, int* length)
{
    *length = static_cast<int>(g_mapcycleContent.size());
    byte* buf = static_cast<byte*>(std::malloc(g_mapcycleContent.size() + 1));
    std::memcpy(buf, g_mapcycleContent.data(), g_mapcycleContent.size());
    buf[g_mapcycleContent.size()] = 0;
    return buf;
}
static void COM_FreeFile(void* p) { std::free(p); }

static void MSG_WriteByte(sizebuf_t* sb, int v) { sb->data.push_back(static_cast<byte>(v & 0xFF)); }
static void MSG_WriteShort(sizebuf_t* sb, int v)
{
    sb->data.push_back(static_cast<byte>(v & 0xFF));
    sb->data.push_back(static_cast<byte>((v >> 8) & 0xFF));
}
static void MSG_WriteLong(sizebuf_t* sb, int v)
{
    for (int i = 0; i < 4; ++i)
        sb->data.push_back(static_cast<byte>((v >> (i * 8)) & 0xFF));
}
static void MSG_WriteBuf(sizebuf_t* sb, int len, const void* src)
{
    const byte* p = static_cast<const byte*>(src);
    sb->data.insert(sb->data.end(), p, p + len);
}
static void MSG_WriteString(sizebuf_t* sb, const char* s)
{
    const size_t n = std::strlen(s) + 1;
    sb->data.insert(sb->data.end(), reinterpret_cast<const byte*>(s), reinterpret_cast<const byte*>(s) + n);
}

// Mirrors engine/common.cpp MSG_ReadString (8192-byte buffer, stops after 8191 chars without reading trailing 0).
struct Reader {
    const std::vector<byte>& buf;
    size_t pos = 0;
    int ReadByte() { return pos < buf.size() ? buf[pos++] : -1; }
    int ReadChar() { return pos < buf.size() ? static_cast<signed char>(buf[pos++]) : -1; }
    void Skip(size_t n) { pos += n; }
    std::string ReadString()
    {
        std::string out;
        while (out.size() < 8191)
        {
            int c = ReadChar();
            if (c == -1 || c == 0)
                break;
            out.push_back(static_cast<char>(c));
        }
        return out;
    }
};
'''

TESTS = r'''
static void VerifyMapcycleLength(size_t inputLen, size_t expectedLen)
{
    g_mapcycleContent.assign(inputLen, 'm');
    sizebuf_t msg;
    client_t client;
    SV_SendServerinfo(&msg, &client);

    Reader r{msg.data, 0};
    // svc_print banner because svs.maxclients > 1
    assert(svc_print == r.ReadByte());
    r.ReadString();

    assert(svc_serverinfo == r.ReadByte());
    r.Skip(4 + 4 + 4 + 16 + 1 + 1 + 1); // proto, spawncount, crc, md5, maxclients, playernum, dm
    assert("valve" == r.ReadString());
    assert("TestServer" == r.ReadString());
    assert("maps/c1a0.bsp" == r.ReadString());

    std::string mapcycle = r.ReadString();
    assert(expectedLen == mapcycle.size());

    // Immediately after mapcycle comes the trailing 0 byte (is_hltv/extra byte) and svc_sendextrainfo.
    // If mapcycle had been 8191 chars, the unconsumed NUL would be read here as 0, shifting svc_sendextrainfo!
    assert(0 == r.ReadByte());
    assert(svc_sendextrainfo == r.ReadByte());
    assert("valve" == r.ReadString());
}

int main()
{
    VerifyMapcycleLength(100, 100);
    VerifyMapcycleLength(8190, 8190);
    VerifyMapcycleLength(8191, 8190);
    VerifyMapcycleLength(8192, 8190);
    VerifyMapcycleLength(10000, 8190);
}
'''


def main():
    production = extract('sv_main.cpp', 'void SV_SendServerinfo( sizebuf_t* message, client_t* client )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-serverinfo-mapcycle-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: SV_SendServerinfo caps mapcycle at 8190 chars so MSG_ReadString stays aligned')


if __name__ == '__main__':
    main()
