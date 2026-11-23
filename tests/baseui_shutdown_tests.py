"""Exercise CBaseUI::Shutdown deleting staticPanel before unloading UI modules."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(relpath, signature):
    source = (ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#define Q_strcasecmp _stricmp
#else
#include <strings.h>
#define Q_strcasecmp strcasecmp
#endif

static std::vector<std::string> g_log;

namespace vgui2 {
struct IVGui {
    void RunFrame() { g_log.push_back("RunFrame"); }
};
struct ISystem {
    void SaveUserConfigFile() { g_log.push_back("SaveUserConfigFile"); }
};
static IVGui g_vgui;
static ISystem g_sys;
static IVGui* ivgui() { return &g_vgui; }
static ISystem* system() { return &g_sys; }
}

struct CStaticPanel {
    ~CStaticPanel() { g_log.push_back("~CStaticPanel"); }
};
struct CEnginePanel {};
struct IGameUI {
    void Shutdown() { g_log.push_back("GameUI::Shutdown"); }
};
struct IClientVGUI {
    void Shutdown() { g_log.push_back("ClientVGUI::Shutdown"); }
};
struct ICareerUI {};
struct IGameConsole {};
struct BaseUISurface {
    void Shutdown() { g_log.push_back("Surface::Shutdown"); }
};
struct CSysModule { const char* name; };

static CStaticPanel* staticPanel = nullptr;
static CEnginePanel* staticGameUIPanel = nullptr;
static CEnginePanel* staticClientDLLPanel = nullptr;
static IGameUI* staticGameUIFuncs = nullptr;
static IClientVGUI* staticClient = nullptr;
static ICareerUI* staticCareerUI = nullptr;
static IGameConsole* staticGameConsole = nullptr;
static BaseUISurface* staticSurface = nullptr;

static struct {
    const char* (*pfnGetGameDirectory)();
} gEngfuncs = {
    []() { return "cstrike"; },
};

static void Sys_UnloadModule(CSysModule* mod)
{
    if (mod)
        g_log.push_back(std::string("Unload:") + mod->name);
}

struct CBaseUI {
    CSysModule* m_hStaticGameUIModule = nullptr;
    CSysModule* m_hVGuiModule = nullptr;
    CSysModule* m_hChromeModule = nullptr;
    void Shutdown();
};
'''

TESTS = r'''
int main()
{
    IGameUI gameUI;
    IClientVGUI clientUI;
    BaseUISurface surface;
    CEnginePanel gameUIPanel;
    CEnginePanel clientPanel;
    CSysModule modGameUI{"gameui"};
    CSysModule modVGui{"vgui2"};
    CSysModule modChrome{"chromehtml"};

    staticPanel = new CStaticPanel();
    staticGameUIPanel = &gameUIPanel;
    staticClientDLLPanel = &clientPanel;
    staticGameUIFuncs = &gameUI;
    staticClient = &clientUI;
    staticSurface = &surface;

    CBaseUI baseUI;
    baseUI.m_hStaticGameUIModule = &modGameUI;
    baseUI.m_hVGuiModule = &modVGui;
    baseUI.m_hChromeModule = &modChrome;

    baseUI.Shutdown();

    assert(nullptr == staticPanel);
    assert(nullptr == staticGameUIPanel);
    assert(nullptr == staticClientDLLPanel);
    assert(nullptr == staticGameUIFuncs);
    assert(nullptr == baseUI.m_hStaticGameUIModule);
    assert(nullptr == baseUI.m_hVGuiModule);
    assert(nullptr == baseUI.m_hChromeModule);

    // Order must be: RunFrame -> ~CStaticPanel -> GameUI::Shutdown -> ClientVGUI::Shutdown -> Unload:gameui -> ...
    assert(g_log.size() == 9);
    assert("RunFrame" == g_log[0]);
    assert("~CStaticPanel" == g_log[1]);
    assert("GameUI::Shutdown" == g_log[2]);
    assert("ClientVGUI::Shutdown" == g_log[3]);
    assert("Unload:gameui" == g_log[4]);
    assert("SaveUserConfigFile" == g_log[5]);
    assert("Unload:vgui2" == g_log[6]);
    assert("Surface::Shutdown" == g_log[7]);
    assert("Unload:chromehtml" == g_log[8]);
}
'''


def main():
    production = extract('vgui2/BaseUI_Interface.cpp', 'void CBaseUI::Shutdown()')
    with tempfile.TemporaryDirectory(prefix='goldsrc-baseui-shutdown-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: CBaseUI::Shutdown deletes staticPanel before unloading UI modules')


if __name__ == '__main__':
    main()
