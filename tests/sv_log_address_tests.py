"""Exercise SV_AddLogAddress_f duplicate tail detection and SV_DelLogAddress_f head removal."""
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

struct netadr_t {
    unsigned char ip[4];
    unsigned short port;
};
struct netlog_t { netadr_t net_address; };
struct LOGLIST_T {
    netlog_t log;
    LOGLIST_T* next;
};

static LOGLIST_T* firstLog = nullptr;
static std::vector<const char*> g_cmd;
static int Cmd_Argc() { return static_cast<int>(g_cmd.size()); }
static const char* Cmd_Argv(int i) { return i < Cmd_Argc() ? g_cmd[i] : ""; }
static int Q_atoi(const char* s) { return std::atoi(s); }
static void Con_Printf(const char*, ...) {}

static bool NET_StringToAdr(const char* text, netadr_t* out)
{
    unsigned a, b, c, d, p;
    if (5 != std::sscanf(text, "%u.%u.%u.%u:%u", &a, &b, &c, &d, &p))
        return false;
    out->ip[0] = static_cast<unsigned char>(a);
    out->ip[1] = static_cast<unsigned char>(b);
    out->ip[2] = static_cast<unsigned char>(c);
    out->ip[3] = static_cast<unsigned char>(d);
    out->port = static_cast<unsigned short>(p);
    return true;
}

static const char* NET_AdrToString(netadr_t adr)
{
    static char buf[64];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u", adr.ip[0], adr.ip[1], adr.ip[2], adr.ip[3], adr.port);
    return buf;
}
'''

TESTS = r'''
static std::vector<std::string> ListAddresses()
{
    std::vector<std::string> out;
    for (LOGLIST_T* p = firstLog; p; p = p->next)
        out.emplace_back(NET_AdrToString(p->log.net_address));
    return out;
}

int main()
{
    // Add two distinct addresses
    g_cmd = {"logaddress_add", "10.0.0.1", "27500"};
    SV_AddLogAddress_f();
    g_cmd = {"logaddress_add", "10.0.0.2", "27501"};
    SV_AddLogAddress_f();
    assert((std::vector<std::string>{"10.0.0.1:27500", "10.0.0.2:27501"}) == ListAddresses());

    // Re-adding the tail node must be rejected as duplicate
    g_cmd = {"logaddress_add", "10.0.0.2", "27501"};
    SV_AddLogAddress_f();
    assert((std::vector<std::string>{"10.0.0.1:27500", "10.0.0.2:27501"}) == ListAddresses());

    // Add a third address and re-add the head
    g_cmd = {"logaddress_add", "10.0.0.3", "27502"};
    SV_AddLogAddress_f();
    g_cmd = {"logaddress_add", "10.0.0.1", "27500"};
    SV_AddLogAddress_f();
    assert((std::vector<std::string>{"10.0.0.1:27500", "10.0.0.2:27501", "10.0.0.3:27502"}) == ListAddresses());

    // Deleting the head node must preserve the remaining list (no drop or leak)
    g_cmd = {"logaddress_del", "10.0.0.1", "27500"};
    SV_DelLogAddress_f();
    assert((std::vector<std::string>{"10.0.0.2:27501", "10.0.0.3:27502"}) == ListAddresses());

    // Clean up remaining nodes via SV_DelLogAddress_f
    g_cmd = {"logaddress_del", "10.0.0.3", "27502"};
    SV_DelLogAddress_f();
    assert((std::vector<std::string>{"10.0.0.2:27501"}) == ListAddresses());

    g_cmd = {"logaddress_del", "10.0.0.2", "27501"};
    SV_DelLogAddress_f();
    assert(nullptr == firstLog);
}
'''


def main():
    production = [
        extract('sv_log.cpp', 'void SV_AddLogAddress_f()'),
        extract('sv_log.cpp', 'void SV_DelLogAddress_f()'),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-sv-log-address-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: logaddress_add checks tail node and logaddress_del preserves list on head removal')


if __name__ == '__main__':
    main()
