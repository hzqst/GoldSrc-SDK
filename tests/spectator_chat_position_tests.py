"""Exercise production spectator chat input placement below the picture in picture inset."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'

GAME_CONTROLS_STUBS = r'''
namespace game_controls {
struct Client {
 bool spectating = false;
 bool spectateOnly = false;
 bool insetOff = false;
 int x = 4, y = 4, wide = 240, tall = 180;
 bool SpectatorMode() { return spectating; }
 bool IsSpectateOnly() { return spectateOnly; }
 bool PipInsetOff() { return insetOff; }
 void InsetValues(int& outX, int& outY, int& outWide, int& outTall) { outX = x; outY = y; outWide = wide; outTall = tall; }
};
struct SpectatorGUI { int GetTopBarHeight() { return 52; } };
struct TeamFortressViewport {
 Client* m_pClientDllInterface;
 SpectatorGUI* m_pSpectatorGUI;
 void ChatInputPosition(int* x, int* y);
};
'''

GAME_CONTROLS_TESTS = r'''
void Run() {
 Client client;
 SpectatorGUI gui;
 TeamFortressViewport viewport{&client, &gui};
 int x = 8, y = 77;
 viewport.ChatInputPosition(&x, &y);
 assert(77 == y);
 client.spectating = true;
 viewport.ChatInputPosition(&x, &y);
 assert(189 == y);
 client.y = 40;
 viewport.ChatInputPosition(&x, &y);
 assert(225 == y);
 client.spectating = false;
 client.spectateOnly = true;
 client.insetOff = true;
 viewport.ChatInputPosition(&x, &y);
 assert(57 == y && 8 == x);
}
}
'''

CZEROR_STUBS = r'''
namespace czeror {
struct CVar { float value = 1; };
struct OverviewData { int insetWindowY = 4; int insetWindowHeight = 180; };
struct Spectator { CVar* m_pip; OverviewData m_OverviewData; };
struct Hud { Spectator m_Spectator; } gHUD;
struct EngineFuncs { bool spectateOnly = false; bool IsSpectateOnly() { return spectateOnly; } } gEngfuncs;
int g_iUser1 = 0;
struct SpectatorGUI { int GetTopBarHeight() { return 52; } };
struct TeamFortressViewport {
 SpectatorGUI* m_pSpectatorGUI;
 void ChatInputPosition(int* x, int* y);
};
'''

CZEROR_TESTS = r'''
void Run() {
 CVar pip;
 gHUD.m_Spectator.m_pip = &pip;
 SpectatorGUI gui;
 TeamFortressViewport viewport{&gui};
 int x = 8, y = 77;
 viewport.ChatInputPosition(&x, &y);
 assert(77 == y);
 g_iUser1 = 1;
 viewport.ChatInputPosition(&x, &y);
 assert(189 == y);
 gHUD.m_Spectator.m_OverviewData.insetWindowY = 40;
 viewport.ChatInputPosition(&x, &y);
 assert(225 == y);
 g_iUser1 = 0;
 gEngfuncs.spectateOnly = true;
 pip.value = 0;
 viewport.ChatInputPosition(&x, &y);
 assert(57 == y && 8 == x);
}
}
'''

DOD_STUBS = r'''
namespace dod {
#define CL_DLLEXPORT
const int ScreenHeight = 960;
struct CVar { float value = 1; };
struct OverviewData { int insetWindowY = 4; int insetWindowHeight = 180; };
struct Rect { int top = 10; int bottom = 30; };
struct ObjectiveIcons { Rect* TimerHUDArea; };
struct Spectator { CVar* m_pip; OverviewData m_OverviewData; };
struct Hud { Spectator m_Spectator; ObjectiveIcons m_ObjectiveIcons; } gHUD;
struct EngineFuncs { bool spectateOnly = false; bool IsSpectateOnly() { return spectateOnly; } } gEngfuncs;
void EngineChatInputPosition(int**, int**) {}
struct ClientDstAddrs { void (*pChatInputPosition)(int**, int**) = EngineChatInputPosition; } addrs, *g_pcldstAddrs = &addrs;
int g_iUser1 = 0;
bool hudVisible = true;
int i_iObjBottomY = 100;
bool ShowHudElement(int) { return hudVisible; }
'''

DOD_TESTS = r'''
void Run() {
 CVar pip;
 Rect timer;
 gHUD.m_Spectator.m_pip = &pip;
 gHUD.m_ObjectiveIcons.TimerHUDArea = &timer;
 int x = 8, y = 77;
 HUD_ChatInputPosition(&x, &y);
 assert(103 == y);
 g_iUser1 = 1;
 HUD_ChatInputPosition(&x, &y);
 assert(378 == y);
 gHUD.m_Spectator.m_OverviewData.insetWindowY = 40;
 HUD_ChatInputPosition(&x, &y);
 assert(450 == y);
 pip.value = 0;
 HUD_ChatInputPosition(&x, &y);
 assert(104 == y && 8 == x);
}
}
'''

MAIN = r'''
int main() {
 game_controls::Run();
 czeror::Run();
 dod::Run();
}
'''


def function(path, signature):
    text = (ROOT / path).read_text(encoding='utf-8')
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def main():
    game_controls = function('vgui2/game_controls/vgui_TeamFortressViewport.cpp', 'void TeamFortressViewport::ChatInputPosition(')
    czeror = function('czeror/cl_dll/vgui/hl_base/vgui_TeamFortressViewport.cpp', 'void TeamFortressViewport::ChatInputPosition(')
    dod = function('dod/cl_dll/dod_objectives.cpp', 'void CL_DLLEXPORT HUD_ChatInputPosition(')
    with tempfile.TemporaryDirectory(prefix='goldsrc-spectator-chat-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text('\n'.join((
            '#include <cassert>',
            GAME_CONTROLS_STUBS, game_controls, GAME_CONTROLS_TESTS,
            CZEROR_STUBS, czeror, CZEROR_TESTS,
            DOD_STUBS, dod, DOD_TESTS,
            MAIN)))
        executable = compile_program([path / 'test.cpp'], path / 'test')
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
