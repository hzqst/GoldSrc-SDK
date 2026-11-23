"""Exercise NET_ParseServerInfoResponse matching SVC_Info wire layout."""
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
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using byte = unsigned char;
constexpr int NETAPI_RESPONSE_SIZE = 2048;

static std::vector<byte> g_pkt;
static size_t g_pos = 0;

static void MSG_BeginReading() { g_pos = 0; }
static int MSG_ReadByte() { return g_pos < g_pkt.size() ? g_pkt[g_pos++] : -1; }
static int MSG_ReadLong()
{
    if (g_pos + 4 > g_pkt.size()) return -1;
    unsigned v = static_cast<unsigned>(g_pkt[g_pos])
        | (static_cast<unsigned>(g_pkt[g_pos + 1]) << 8)
        | (static_cast<unsigned>(g_pkt[g_pos + 2]) << 16)
        | (static_cast<unsigned>(g_pkt[g_pos + 3]) << 24);
    g_pos += 4;
    return static_cast<int>(v);
}
static char* MSG_ReadString()
{
    static char buf[256];
    int i = 0;
    while (g_pos < g_pkt.size() && g_pkt[g_pos] != 0)
    {
        if (i < 255) buf[i++] = static_cast<char>(g_pkt[g_pos]);
        ++g_pos;
    }
    if (g_pos < g_pkt.size()) ++g_pos;
    buf[i] = 0;
    return buf;
}

static char* va(const char* fmt, ...)
{
    static char buf[4][128];
    static int idx = 0;
    idx = (idx + 1) & 3;
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf[idx], sizeof(buf[idx]), fmt, args);
    va_end(args);
    return buf[idx];
}

static std::map<std::string, std::string> g_kv;
static void Info_SetValueForKey(char*, const char* k, const char* v, int)
{
    g_kv[k] = v;
}
'''

TESTS = r'''
static void WriteByte(std::vector<byte>& out, int v) { out.push_back(static_cast<byte>(v & 0xFF)); }
static void WriteLong(std::vector<byte>& out, int v)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<byte>((v >> (i * 8)) & 0xFF));
}
static void WriteString(std::vector<byte>& out, const char* s)
{
    const size_t n = std::strlen(s) + 1;
    out.insert(out.end(), reinterpret_cast<const byte*>(s), reinterpret_cast<const byte*>(s) + n);
}

int main()
{
    // 1. Detailed 'm' reply with mod block matching SVC_Info wire layout
    {
        g_pkt.clear();
        g_kv.clear();
        WriteLong(g_pkt, -1);
        WriteByte(g_pkt, 'm');
        WriteString(g_pkt, "127.0.0.1:27015");
        WriteString(g_pkt, "My Server");
        WriteString(g_pkt, "de_dust2");
        WriteString(g_pkt, "cstrike");
        WriteString(g_pkt, "Counter-Strike");
        WriteByte(g_pkt, 12);  // current players (1 byte)
        WriteByte(g_pkt, 32);  // max players (1 byte)
        WriteByte(g_pkt, 48);  // protocol
        WriteByte(g_pkt, 'd'); // dedicated
        WriteByte(g_pkt, 'l'); // linux
        WriteByte(g_pkt, 1);   // password
        WriteByte(g_pkt, 1);   // is mod
        WriteString(g_pkt, "https://example.com");
        WriteString(g_pkt, "https://example.com/dl");
        WriteString(g_pkt, "");
        WriteLong(g_pkt, 100);
        WriteLong(g_pkt, 65536);
        WriteByte(g_pkt, 0);   // svonly
        WriteByte(g_pkt, 1);   // cldll
        WriteByte(g_pkt, 1);   // secure (after mod block!)
        WriteByte(g_pkt, 2);   // bots

        char out[NETAPI_RESPONSE_SIZE];
        NET_ParseServerInfoResponse(out);

        assert("My Server" == g_kv["hostname"]);
        assert("de_dust2" == g_kv["map"]);
        assert("cstrike" == g_kv["gamedir"]);
        assert("Counter-Strike" == g_kv["gamedesc"]);
        assert("12" == g_kv["current"]);
        assert("32" == g_kv["max"]);
        assert("48" == g_kv["protocol"]);
        assert("d" == g_kv["type"]);
        assert("l" == g_kv["os"]);
        assert("Y" == g_kv["pw"]);
        assert("Y" == g_kv["mod"]);
        assert("https://example.com" == g_kv["mod_url"]);
        assert("https://example.com/dl" == g_kv["mod_dl"]);
        assert("100" == g_kv["mod_ver"]);
        assert("65536" == g_kv["mod_size"]);
        assert("N" == g_kv["svonly"]);
        assert("Y" == g_kv["cldll"]);
        assert("1" == g_kv["secure"]);
    }

    // 2. Non-mod detailed 'm' reply
    {
        g_pkt.clear();
        g_kv.clear();
        WriteLong(g_pkt, -1);
        WriteByte(g_pkt, 'm');
        WriteString(g_pkt, "LOOPBACK");
        WriteString(g_pkt, "Half-Life");
        WriteString(g_pkt, "crossfire");
        WriteString(g_pkt, "valve");
        WriteString(g_pkt, "Half-Life");
        WriteByte(g_pkt, 3);
        WriteByte(g_pkt, 16);
        WriteByte(g_pkt, 48);
        WriteByte(g_pkt, 'l');
        WriteByte(g_pkt, 'w');
        WriteByte(g_pkt, 0);
        WriteByte(g_pkt, 0); // not a mod
        WriteByte(g_pkt, 0); // not secure

        char out[NETAPI_RESPONSE_SIZE];
        NET_ParseServerInfoResponse(out);

        assert("3" == g_kv["current"]);
        assert("16" == g_kv["max"]);
        assert("N" == g_kv["pw"]);
        assert("N" == g_kv["mod"]);
        assert("0" == g_kv["secure"]);
    }
}
'''


def main():
    production = extract('net_api.cpp', 'static void NET_ParseServerInfoResponse( char* returninfo )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-netapi-serverinfo-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: NET_ParseServerInfoResponse matches SVC_Info wire layout')


if __name__ == '__main__':
    main()
