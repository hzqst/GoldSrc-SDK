"""Exercise StatusText, StatusValue, ParseStatusString, LocaliseTextString, and SendAudio bounds."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(relpath, signature):
    source = (ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


VALVE_HARNESS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#define _snprintf snprintf
#endif

constexpr int MAX_PLAYERS = 64;
constexpr int MAX_PLAYER_NAME_LENGTH = 32;
constexpr int HUD_ACTIVE = 1;
constexpr int TRUE = 1;

struct client_textmessage_t { const char* pMessage; };
struct hud_player_info_t { const char* name; };
struct extra_player_info_t { int health; float showhealth; int radarflashon; float radarflash; int radarflashes; };

static hud_player_info_t g_PlayerInfoList[MAX_PLAYERS + 1] = {};
static extra_player_info_t g_PlayerExtraInfo[MAX_PLAYERS + 1] = {};
static int g_getPlayerInfoCalls = 0;
static float g_color[3] = {1.0f, 1.0f, 1.0f};
static float* GetClientColor(int) { return g_color; }

static std::string g_longMsg(200, 'X');
static client_textmessage_t g_longEntry = { g_longMsg.c_str() };
static client_textmessage_t* TextMessageGet(const char* name)
{
    if (0 == std::strcmp(name, "LONG"))
        return &g_longEntry;
    return nullptr;
}

static struct {
    void (*pfnGetPlayerInfo)(int ent_num, hud_player_info_t* pinfo);
} gEngfuncs = {
    [](int ent_num, hud_player_info_t* pinfo) {
        ++g_getPlayerInfoCalls;
        assert(ent_num >= 1 && ent_num <= MAX_PLAYERS);
        assert(pinfo == &g_PlayerInfoList[ent_num]);
    },
};

struct MsgReader {
    const unsigned char* data = nullptr;
    int size = 0;
    int pos = 0;
} g_reader;

static void BEGIN_READ(void* buf, int size) { g_reader = { static_cast<const unsigned char*>(buf), size, 0 }; }
static int READ_BYTE() { return g_reader.pos < g_reader.size ? g_reader.data[g_reader.pos++] : 0; }
static int READ_SHORT()
{
    if (g_reader.pos + 2 > g_reader.size) return 0;
    int lo = g_reader.data[g_reader.pos++];
    int hi = g_reader.data[g_reader.pos++];
    return static_cast<short>(lo | (hi << 8));
}
static char* READ_STRING()
{
    static char buf[256];
    int i = 0;
    while (g_reader.pos < g_reader.size && g_reader.data[g_reader.pos] != 0)
    {
        if (i < 255) buf[i++] = static_cast<char>(g_reader.data[g_reader.pos]);
        ++g_reader.pos;
    }
    if (g_reader.pos < g_reader.size) ++g_reader.pos;
    buf[i] = 0;
    return buf;
}

namespace valve_ns {
struct CHudTextMessage {
    char* LocaliseTextString(const char* msg, char* dst_buffer, int buffer_size);
};
static struct { CHudTextMessage m_TextMessage; } gHUD;
struct CHudStatusBar {
    enum {
        MAX_STATUSTEXT_LENGTH = 128,
        MAX_STATUSBAR_VALUES = 8,
        MAX_STATUSBAR_LINES = 3,
    };
    int m_iFlags = 0;
    int m_bReparseString = 0;
    char m_szStatusText[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH] = {};
    int sentinel1 = 0x11223344;
    char m_szStatusBar[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH] = {};
    int sentinel2 = 0x55667788;
    int m_iStatusValues[MAX_STATUSBAR_VALUES] = {};
    int sentinel3 = 0x13572468;
    float* m_pflNameColors[MAX_STATUSBAR_LINES] = {};
    void ParseStatusString(int line_num);
    int MsgFunc_StatusText(const char* pszName, int iSize, void* pbuf);
    int MsgFunc_StatusValue(const char* pszName, int iSize, void* pbuf);
};
'''

CSTRIKE_HARNESS = r'''
} // namespace valve_ns

namespace cstrike_ns {
struct CHudTextMessage {
    char* LocaliseTextString(const char* message, char* buffer, int size);
};
struct CHudHealth { int m_iPlayerLastPointedAt = 0; };
static struct { CHudTextMessage m_TextMessage; CHudHealth m_Health; float m_flTime = 12.0f; } gHUD;
struct CHudStatusBar {
    char m_szStatusText[1][128] = {};
    char m_szStatusBar[1][128] = {};
    int sentinel = 0x24681357;
    int m_iStatusValues[8] = {};
    float* m_pflNameColors[1] = {};
    void ParseStatusString(int line);
};
static int g_findAudCalls = 0;
static void FindAudMessage(char*, int, int) { ++g_findAudCalls; }
struct CHudSayText {
    int MsgFunc_SendAudio(const char*, int size, void* buffer);
};
'''

TESTS = r'''
} // namespace cstrike_ns

int main()
{
    // 1. Valve StatusText / StatusValue off-by-one rejection
    {
        valve_ns::CHudStatusBar sb;
        unsigned char badText[] = { 3, 'H', 'i', 0 };
        sb.MsgFunc_StatusText("StatusText", sizeof(badText), badText);
        assert(0x11223344 == sb.sentinel1);

        unsigned char badVal[] = { 8, 42, 0 };
        sb.MsgFunc_StatusValue("StatusValue", sizeof(badVal), badVal);
        assert(0x13572468 == sb.sentinel3);

        // Valid index works
        unsigned char okVal[] = { 1, 99, 0 };
        sb.MsgFunc_StatusValue("StatusValue", sizeof(okVal), okVal);
        assert(99 == sb.m_iStatusValues[1]);
    }

    // 2. Valve ParseStatusString: out-of-range %p, 32-char player name, output truncation & terminator
    {
        valve_ns::CHudStatusBar sb;
        sb.m_iStatusValues[0] = 1;
        sb.m_iStatusValues[1] = 200; // out of range
        sb.m_iStatusValues[2] = -5;  // negative
        std::strcpy(sb.m_szStatusText[0], "0 %p1 %p2 %");
        g_getPlayerInfoCalls = 0;
        sb.ParseStatusString(0);
        assert(0 == g_getPlayerInfoCalls);
        assert(0 == std::strcmp(" ****** ****** ", sb.m_szStatusBar[0]));
        assert(0x55667788 == sb.sentinel2);

        // 32-byte name + repeated %p exceeding 128 bytes must stay null-terminated and bounded
        std::string fullName(32, 'A');
        g_PlayerInfoList[5].name = fullName.c_str();
        sb.m_iStatusValues[1] = 5;
        std::strcpy(sb.m_szStatusText[0], "0 %p1%p1%p1%p1%p1");
        sb.ParseStatusString(0);
        assert(0x55667788 == sb.sentinel2);
        assert(0 == sb.m_szStatusBar[0][127]);
        assert(127 == std::strlen(sb.m_szStatusBar[0]));
    }

    // 3. Valve & CStrike LocaliseTextString: long expansion, long #word, and zero-size buffer
    {
        char out[16];
        std::memset(out, 'Z', sizeof(out));
        valve_ns::gHUD.m_TextMessage.LocaliseTextString("#LONG", out, sizeof(out));
        assert(15 == std::strlen(out));
        assert(0 == out[15]);

        std::string longWord = "#" + std::string(300, 'a');
        valve_ns::gHUD.m_TextMessage.LocaliseTextString(longWord.c_str(), out, sizeof(out));
        assert(15 == std::strlen(out));
        assert('#' == out[0]);

        char untouched = 'Q';
        valve_ns::gHUD.m_TextMessage.LocaliseTextString("abc", &untouched, 0);
        assert('Q' == untouched);

        cstrike_ns::gHUD.m_TextMessage.LocaliseTextString("#LONG", out, sizeof(out));
        assert(15 == std::strlen(out));
        assert(0 == out[15]);
        cstrike_ns::gHUD.m_TextMessage.LocaliseTextString(longWord.c_str(), out, sizeof(out));
        assert(15 == std::strlen(out));
    }

    // 4. CStrike ParseStatusString: out-of-range %p, out-of-range m_iPlayerLastPointedAt in %i, output cap
    {
        cstrike_ns::CHudStatusBar sb;
        sb.m_iStatusValues[0] = 1;
        sb.m_iStatusValues[1] = 120;
        cstrike_ns::gHUD.m_Health.m_iPlayerLastPointedAt = 999;
        std::strcpy(sb.m_szStatusText[0], "0 %p1 %i1");
        g_getPlayerInfoCalls = 0;
        sb.ParseStatusString(0);
        assert(0 == g_getPlayerInfoCalls);
        assert(0 == std::strcmp(" ****** 120", sb.m_szStatusBar[0]));
        assert(0x24681357 == sb.sentinel);

        std::string fullName(128, 'B');
        g_PlayerInfoList[2].name = fullName.c_str();
        sb.m_iStatusValues[1] = 2;
        std::strcpy(sb.m_szStatusText[0], "0 %p1%p1");
        sb.ParseStatusString(0);
        assert(0 == sb.m_szStatusBar[0][127]);
        assert(127 == std::strlen(sb.m_szStatusBar[0]));
        assert(0x24681357 == sb.sentinel);
    }

    // 5. CStrike SendAudio: out-of-range client index must not write past g_PlayerExtraInfo
    {
        cstrike_ns::CHudSayText say;
        unsigned char badAudio[] = { 200, '%', '!', 'M', 'R', 'A', 'D', 0, 100, 0 };
        cstrike_ns::g_findAudCalls = 0;
        say.MsgFunc_SendAudio("SendAudio", sizeof(badAudio), badAudio);
        assert(1 == cstrike_ns::g_findAudCalls);

        unsigned char okAudio[] = { 3, 'h', 'i', 0, 100, 0 };
        say.MsgFunc_SendAudio("SendAudio", sizeof(okAudio), okAudio);
        assert(1 == g_PlayerExtraInfo[3].radarflashon);
        assert(22 == g_PlayerExtraInfo[3].radarflashes);
    }
}
'''


def main():
    valve_prod = [
        extract('cl_dll/text_message.cpp', 'char *CHudTextMessage::LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size )'),
        extract('cl_dll/statusbar.cpp', 'void CHudStatusBar :: ParseStatusString( int line_num )'),
        extract('cl_dll/statusbar.cpp', 'int CHudStatusBar :: MsgFunc_StatusText( const char *pszName, int iSize, void *pbuf )'),
        extract('cl_dll/statusbar.cpp', 'int CHudStatusBar :: MsgFunc_StatusValue( const char *pszName, int iSize, void *pbuf )'),
    ]
    cstrike_prod = [
        extract('cstrike/cl_dll/text_message.cpp', 'char* CHudTextMessage::LocaliseTextString( const char* message, char* buffer, int size )'),
        extract('cstrike/cl_dll/statusbar.cpp', 'void CHudStatusBar::ParseStatusString(int line)'),
        extract('cstrike/cl_dll/saytext.cpp', 'int CHudSayText::MsgFunc_SendAudio( const char*, int size, void* buffer )'),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-statusbar-localise-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([VALVE_HARNESS, *valve_prod, CSTRIKE_HARNESS, *cstrike_prod, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: status bar bounds, %p player index, LocaliseTextString, and SendAudio')


if __name__ == '__main__':
    main()
