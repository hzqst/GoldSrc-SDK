"""Exercise CHudMessage::Draw END3 disconnect behavior."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'
CLIENTS = ['valve', 'tfc']


def extract(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

enum { maxHUDMessages = 16, HUD_ACTIVE = 1 };

struct wrect_t { int left, right, top, bottom; };

struct client_textmessage_t
{
    int effect;
    unsigned char r1, g1, b1, a1;
    unsigned char r2, g2, b2, a2;
    float x, y;
    float fadein, fadeout, holdtime, fxtime;
    const char *pName;
    const char *pMessage;
};

struct message_parms_t
{
    float time;
};

static struct
{
    float m_flTime;
    wrect_t rect;
    int GetSprite(int) { return 0; }
    wrect_t &GetSpriteRect(int) { return rect; }
} gHUD = { 0.0f, { 0, 0, 0, 0 } };

static std::vector<std::string> g_commands;

static struct
{
    int (*pfnClientCmd)(const char *szCmdString);
} gEngfuncs = {
    [](const char *cmd) -> int { g_commands.emplace_back(cmd); return 0; },
};

static void SPR_Set(int, int, int, int) {}
static void SPR_DrawAdditive(int, int, int, const wrect_t *) {}

class CHudMessage
{
public:
    int m_iFlags = HUD_ACTIVE;
    client_textmessage_t *m_pMessages[maxHUDMessages] = {};
    float m_startTime[maxHUDMessages] = {};
    message_parms_t m_parms = { 0.0f };
    float m_gameTitleTime = 0.0f;
    client_textmessage_t *m_pGameTitle = NULL;
    int m_HUD_title_life = 0;
    int m_HUD_title_half = 0;

    float FadeBlend(float, float, float, float) { return 1.0f; }
    int XPosition(float, int, int) { return 0; }
    int YPosition(float, int) { return 0; }
    void MessageDrawScan(client_textmessage_t *, float) {}
    int Draw(float fTime);
};
'''

TESTS = r'''
static client_textmessage_t MakeMessage(const char *name, const char *text, float hold)
{
    client_textmessage_t msg = {};
    msg.effect = 0;
    msg.fadein = 0.5f;
    msg.fadeout = 0.5f;
    msg.holdtime = hold;
    msg.pName = name;
    msg.pMessage = text;
    return msg;
}

int main()
{
    const std::string expectedDisconnect = "wait\nwait\nwait\nwait\nwait\nwait\nwait\ndisconnect\n";

    // Earlier message ending while END3 is still active must NOT disconnect;
    // only END3 ending triggers disconnect once.
    {
        g_commands.clear();
        CHudMessage hud;
        client_textmessage_t prevCredit = MakeMessage("END2", "Previous credit", 1.0f);
        client_textmessage_t end3 = MakeMessage("END3", "Final credit", 5.0f);
        client_textmessage_t after = MakeMessage("OTHER", "Still active later", 8.0f);
        hud.m_pMessages[0] = &prevCredit;
        hud.m_startTime[0] = 0.0f; // ends at 2.0
        hud.m_pMessages[1] = &end3;
        hud.m_startTime[1] = 0.0f; // ends at 6.0
        hud.m_pMessages[2] = &after;
        hud.m_startTime[2] = 0.0f; // ends at 9.0

        gHUD.m_flTime = 3.0f;
        hud.Draw(3.0f);
        assert(NULL == hud.m_pMessages[0]);
        assert(&end3 == hud.m_pMessages[1]);
        assert(g_commands.empty());

        gHUD.m_flTime = 7.0f;
        hud.Draw(7.0f);
        assert(NULL == hud.m_pMessages[1]);
        assert(1u == g_commands.size());
        assert(expectedDisconnect == g_commands[0]);

        // Subsequent messages ending must not fire another disconnect.
        gHUD.m_flTime = 10.0f;
        hud.Draw(10.0f);
        assert(NULL == hud.m_pMessages[2]);
        assert(1u == g_commands.size());
    }

    // Engine sequence messages have pName == NULL; ending one must not crash or disconnect.
    {
        g_commands.clear();
        CHudMessage hud;
        client_textmessage_t unnamed = MakeMessage(NULL, "Sequence message", 1.0f);
        client_textmessage_t end3 = MakeMessage("END3", "Final credit", 1.0f);
        hud.m_pMessages[0] = &unnamed;
        hud.m_startTime[0] = 0.0f;
        hud.m_pMessages[1] = &end3;
        hud.m_startTime[1] = 0.0f;

        gHUD.m_flTime = 5.0f;
        hud.Draw(5.0f);
        assert(NULL == hud.m_pMessages[0]);
        assert(NULL == hud.m_pMessages[1]);
        assert(1u == g_commands.size());
        assert(expectedDisconnect == g_commands[0]);
    }
}
'''


def main():
    for client in CLIENTS:
        hud_header = (ROOT / client / 'cl_dll/hud.h').read_text(encoding='utf-8')
        source = (ROOT / client / 'cl_dll/message.cpp').read_text(encoding='utf-8')
        assert 'm_bEndAfterMessage' not in hud_header, f'{client}/cl_dll/hud.h still has m_bEndAfterMessage'
        assert 'm_bEndAfterMessage' not in source, f'{client}/cl_dll/message.cpp still has m_bEndAfterMessage'
        production = extract(source, 'int CHudMessage::Draw( float fTime )')
        with tempfile.TemporaryDirectory(prefix='goldsrc-hud-message-end3-') as folder:
            work = Path(folder)
            test_source = work / 'test.cpp'
            test_source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
            executable = compile_program([test_source], work / 'test', sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], cwd=work, check=True)
        print(f'PASS: {client} CHudMessage::Draw disconnects only when END3 itself ends')


if __name__ == '__main__':
    main()
