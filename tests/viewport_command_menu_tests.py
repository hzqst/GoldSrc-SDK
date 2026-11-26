"""Exercise the production VGUI1 command menu loaders of Half-Life, Team Fortress Classic, Ricochet, Deathmatch Classic
and Opposing Force with counted menus, buttons and a tracked menu file.

Run: python tests/viewport_command_menu_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


# the loaders close a try block at column 0 on Windows, so the function ends at the brace after m_iInitialized = true
def command_menu(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', source.index('m_iInitialized = true;', start)) + 2]


STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#define MAX_MENUS 3
#define MAX_BUTTONS 4
#define MAX_MAPNAME 64
#define MAX_BUTTON_SIZE 32
#define BUTTON_SIZE_Y 30
#define CMENU_SIZE_X 160
#define CMENU_TOP 32
#define PC_ENGINEER 9
static const char* sTFClasses[] = {"", "SCOUT", "SNIPER", "SOLDIER", "DEMOMAN", "MEDIC", "HWGUY", "PYRO", "SPY", "ENGINEER"};
#ifdef _WIN32
class CException {};
#endif
namespace Scheme { enum { sf_primary3 }; }
template <class T> T* VGuiCompat_OwnActionSignal(T* signal) { return signal; }
template <class T> T* VGuiCompat_OwnInputSignal(T* signal) { return signal; }
static int liveMenus = 0, liveButtons = 0, liveSignals = 0;
struct Signal {
 Signal() { ++liveSignals; }
 virtual ~Signal() { --liveSignals; }
};
struct CMenuHandler_StringCommand : Signal { template <class... A> CMenuHandler_StringCommand(A&&...) {} };
struct CMenuHandler_ToggleCvar : Signal { template <class... A> CMenuHandler_ToggleCvar(A&&...) {} };
struct CMenuHandler_PopupSubMenuInput : Signal { template <class... A> CMenuHandler_PopupSubMenuInput(A&&...) {} };
struct Panel { virtual ~Panel() {} };
class CCommandMenu;
// owns the signals added to it like the VGUI1 panel tree does
class CommandButton {
public:
 CommandButton() { ++liveButtons; }
 CommandButton(int, const char* text, int, int, float, float, bool = false) : m_text(text ? text : "") { ++liveButtons; }
 CommandButton(int, char* text, int, int, float, float, bool = false) : m_text(text ? text : "") { ++liveButtons; }
 CommandButton(const char* text) : m_text(text ? text : "") { ++liveButtons; }
 template <class... A> CommandButton(A&&...) { ++liveButtons; }
 virtual ~CommandButton() { --liveButtons; }
 void setBoundKey(char key) { m_key = key; }
 void setParentMenu(CCommandMenu* menu) { m_parentMenu = menu; }
 void setFont(int) {}
 void addActionSignal(Signal* signal) { m_signals.emplace_back(signal); }
 void addInputSignal(Signal* signal) { m_signals.emplace_back(signal); }
 std::string m_text;
 char m_key = 0;
 CCommandMenu* m_parentMenu = nullptr;
private:
 std::vector<std::unique_ptr<Signal>> m_signals;
};
struct MapButton : CommandButton {
 MapButton(const char*, const char* text, int, int, float, float) : CommandButton(text) {}
 MapButton(char*, char* text, int, int, float, float) : CommandButton(text) {}
 template <class... A> MapButton(A&&... args) : CommandButton(args...) {}
};
struct TeamOnlyCommandButton : CommandButton {
 TeamOnlyCommandButton(int, const char* text, int, int, float, float, bool = false) : CommandButton(text) {}
 TeamOnlyCommandButton(int, char* text, int, int, float, float, bool = false) : CommandButton(text) {}
 template <class... A> TeamOnlyCommandButton(A&&... args) : CommandButton(args...) {}
};
struct ToggleCommandButton : CommandButton {
 ToggleCommandButton(const char*, const char* text, int, int, float, float, bool = false) : CommandButton(text) {}
 ToggleCommandButton(char*, char* text, int, int, float, float, bool = false) : CommandButton(text) {}
 template <class... A> ToggleCommandButton(A&&... args) : CommandButton(args...) {}
};
struct SpectToggleButton : CommandButton {
 SpectToggleButton(const char*, const char* text, int, int, float, float, bool = false) : CommandButton(text) {}
 SpectToggleButton(char*, char* text, int, int, float, float, bool = false) : CommandButton(text) {}
 template <class... A> SpectToggleButton(A&&... args) : CommandButton(args...) {}
};
struct SpectButton : CommandButton {
 SpectButton(int, const char* text, int, int, float, float) : CommandButton(text) {}
 SpectButton(int, char* text, int, int, float, float) : CommandButton(text) {}
 template <class... A> SpectButton(A&&... args) : CommandButton(args...) {}
};
// owns its buttons but not its submenus, which the viewport keeps in m_pCommandMenus
class CCommandMenu : public Panel {
public:
 template <class... A> CCommandMenu(CCommandMenu* parent, A&&...) : m_parent(parent) { ++liveMenus; }
 ~CCommandMenu() { --liveMenus; }
 void setParent(Panel*) {}
 void setVisible(bool) {}
 bool AddButton(CommandButton* button) {
  if ((int)m_buttons.size() >= MAX_BUTTONS) return false;
  m_buttons.emplace_back(button);
  return true;
 }
 int GetNumButtons() { return (int)m_buttons.size(); }
 CCommandMenu* GetParentMenu() { return m_parent; }
 float m_flButtonSizeY = 0;
 int m_iSpectCmdMenu = 0;
 std::vector<std::unique_ptr<CommandButton>> m_buttons;
private:
 CCommandMenu* m_parent;
};
struct CHudTextMessage {
 static char* LocaliseTextString(const char* text, char* buffer, int size) {
  snprintf(buffer, size, "%s", text);
  return buffer;
 }
};
static struct { CHudTextMessage m_TextMessage; } gHUD;
static std::string menuText;
static bool menuExists = true;
static char* loadedFile = nullptr;
static int wrongFrees = 0;
static struct {
 void* COM_LoadFile(const char*, int, int* length) {
  if (!menuExists) return nullptr;
	  if (length) *length = (int)menuText.size();
  loadedFile = new char[menuText.size() + 1];
  memcpy(loadedFile, menuText.c_str(), menuText.size() + 1);
  return loadedFile;
 }
 // frees only the buffer COM_LoadFile returned, and counts any other pointer instead of corrupting the heap
 void COM_FreeFile(void* file) {
  if (!file) return;
  if (file != loadedFile) { ++wrongFrees; return; }
  delete[] loadedFile;
  loadedFile = nullptr;
 }
 void Con_Printf(const char*, ...) {}
 void Con_DPrintf(const char*, ...) {}
 // mimics engine COM_ParseFile: comments, standalone braces, quoted strings, and end+1 on unclosed quote
 char* COM_ParseFile(char* data, char* token) {
  token[0] = 0;
  if (!data) return nullptr;
  for (;;) {
   while (*data && isspace((unsigned char)*data)) ++data;
   if (!*data) return nullptr;
   if (data[0] == '/' && data[1] == '/') {
    while (*data && *data != '\n') ++data;
    continue;
   }
   break;
  }
  int length = 0;
  if (*data == '"') {
   for (++data; *data && *data != '"'; ++data) token[length++] = *data;
   ++data; // intentionally steps past NUL on unclosed quote like engine COM_Parse
  } else if (*data == '{' || *data == '}') {
   token[length++] = *data++;
  } else {
   for (; *data && !isspace((unsigned char)*data) && *data != '{' && *data != '}'; ++data) token[length++] = *data;
  }
  token[length] = 0;
  return data;
 }
} gEngfuncs;
// only the members the command menu loaders use
class ViewportStub : public Panel {
public:
 ~ViewportStub() { for (int i = 0; i < m_iNumMenus; ++i) delete m_pCommandMenus[i]; }
 template <class T, class... A> CommandButton* CreateCustomButton(const T* text, A&&...) { return new CommandButton(text); }
 template <class... A> CCommandMenu* CreateSubMenu(CommandButton*, CCommandMenu* parent, A&&...) { return new CCommandMenu(parent); }
 void SetCurrentMenu(void*) {}
 void SetCurrentCommandMenu(CCommandMenu*) {}
 CCommandMenu* m_pCommandMenus[MAX_MENUS] = {};
 CCommandMenu* m_pCurrentCommandMenu = nullptr;
 void* m_pCurrentMenu = nullptr;
 int m_iNumMenus = 0;
 bool m_iInitialized = false;
 int m_iCurrentTeamNumber = 0, m_iUser1 = 0, m_iUser2 = 0, m_iUser3 = 0;
 char m_sDetpackStrings[3][MAX_BUTTON_SIZE];
};
'''

VIEWPORTS = {
    'valve': ('valve/cl_dll/vgui_TeamFortressViewport.cpp', 'TeamFortressViewport',
              'int CreateCommandMenu(char* menuFile, int direction, int yOffset, bool flatDesign, float flButtonSizeX, float flButtonSizeY, int xOffset, int reservedMenus);',
              'int TeamFortressViewport::CreateCommandMenu(', 'viewport.CreateCommandMenu(menuFile, 0, 0, false, 160, 30, 0, 0)'),
    'tfc': ('tfc/cl_dll/vgui_TeamFortressViewport.cpp', 'TeamFortressViewport',
            'int CreateCommandMenu(char* menuFile, int direction, int yOffset, bool flatDesign, float flButtonSizeX, float flButtonSizeY, int xOffset);',
            'int TeamFortressViewport::CreateCommandMenu(', 'viewport.CreateCommandMenu(menuFile, 0, 0, false, 160, 30, 0)'),
    'ricochet': ('ricochet/cl_dll/vgui_TeamFortressViewport.cpp', 'TeamFortressViewport', 'void CreateCommandMenu(void);',
                 'void TeamFortressViewport::CreateCommandMenu(', 'viewport.CreateCommandMenu()'),
    'dmc': ('dmc/cl_dll/vgui_viewport.cpp', 'TeamFortressViewport', 'int CreateCommandMenu(char* menuFile, int direction, int yOffset);',
            'int TeamFortressViewport::CreateCommandMenu(', 'viewport.CreateCommandMenu(menuFile, 0, 0)'),
    'gearbox': ('gearbox/cl_dll/vgui_CTFViewport.cpp', 'CTFViewport', 'void CreateCommandMenu();',
                'void CTFViewport::CreateCommandMenu(', 'viewport.CreateCommandMenu()'),
}

TESTS = r'''
namespace NAME {
static void Build(const char* text) {
 menuText = text;
 char menuFile[] = "commandmenu.txt";
 VIEWPORT viewport;
 BUILD;
 assert(!loadedFile);
 assert(0 == wrongFrees);
}

static void Run() {
 // a submenu and its buttons, parsed until COM_ParseFile returns NULL at the end of the file
 Build("\"1\" \"Plain\" \"say plain\"\n\"2\" \"Sub\" \"{\"\n\"3\" \"Inner\" \"say inner\"\n\"}\"\n");
 // the last submenu reaches MAX_MENUS, so parsing stops in the middle of the file
 Build("\"1\" \"First\" \"{\"\n\"2\" \"A\" \"a\"\n\"}\"\n\"3\" \"Second\" \"{\"\n\"4\" \"B\" \"b\"\n\"}\"\n\"5\" \"After\" \"say after\"\n");
 // a stray brace closes the root menu, so the next button fails with no command menu
 Build("\"1\" \"Plain\" \"say plain\"\n\"}\"\n\"2\" \"Orphan\" \"say orphan\"\n");
 menuExists = false;
 Build("\"1\" \"Plain\" \"say plain\"\n");
 menuExists = true;
 assert(0 == liveMenus && 0 == liveButtons && 0 == liveSignals);
 puts("PASS: the LABEL command menu frees the loaded menu file, not the parse position, when parsing ends, stops at MAX_MENUS or fails");
}
}
'''

LABELS = {'valve': 'Half-Life', 'tfc': 'Team Fortress Classic', 'ricochet': 'Ricochet', 'dmc': 'Deathmatch Classic',
          'gearbox': 'Opposing Force'}

VALVE_TESTS = r'''
namespace valve {
static void RunRejectedSubmenu() {
 char filename[] = "commandmenu.txt";
 menuText = R"MENU(
  // kept submenu with tight braces
  "1" "Kept submenu"{
    "1" "Kept child" "say inside"
  }
  "2" "Rejected submenu" {
    MAP "crossfire" "1" "{" "say brace"
    TEAM1 "2" "}" "say brace"
    "" "Unbound inside rejected" "}"
    CUSTOM "4" "Custom leaf inside" "!CUSTOMLEAF"
    CUSTOM "5" "Custom sub inside" "!CUSTOMSUB" {
      "1" "Depth 2" {
        "1" "Depth 3" "say deep"
      }
      "2" "Empty nested" {}
    }
    "3" "Nested submenu" {
      TOGGLE "1" "" "hud_draw"
    }
  }
  "" "Unbound sibling" "}"
  CUSTOM "4" "Custom sibling" "!MYCUSTOM"
 )MENU";
 {
  TeamFortressViewport viewport;
  int root = viewport.CreateCommandMenu(filename, 0, 0, false, 160, 30, 0, 1);
  assert(viewport.m_iNumMenus == 2);
  auto& rootBtns = viewport.m_pCommandMenus[root]->m_buttons;
  assert(rootBtns.size() == 3);
  assert(rootBtns[0]->m_text == "Kept submenu" && rootBtns[0]->m_key == '1');
  assert(rootBtns[1]->m_text == "Unbound sibling" && rootBtns[1]->m_key == '\0');
  assert(rootBtns[2]->m_text == "Custom sibling" && rootBtns[2]->m_key == '4');
  assert(rootBtns[1]->m_parentMenu == viewport.m_pCommandMenus[root]);
  auto& subBtns = viewport.m_pCommandMenus[1]->m_buttons;
  assert(subBtns.size() == 1 && subBtns[0]->m_text == "Kept child");
  assert(!loadedFile);
 }
 Build("\"1\" \"Truncated");
 assert(0 == liveMenus && 0 == liveButtons && 0 == liveSignals);
 puts("PASS: Half-Life command menu parses empty keys, brace commands, CUSTOM items and deeply nested rejected submenus");
}
}
'''


def main():
    code = STUBS
    for name, (path, viewport, declaration, signature, build) in VIEWPORTS.items():
        source = (ROOT / path).read_text(encoding='latin-1')
        code += f'namespace {name} {{\nclass {viewport} : public ViewportStub {{\npublic:\n {declaration}\n}};\n'
        if name == 'valve':
            helpers = source.index('namespace\n{\nstruct MenuToken')
            code += source[helpers:source.index('//-----------------------------------------------------------------------------', helpers)]
        code += command_menu(source, signature) + '\n}\n'
        code += (TESTS.replace('NAME', name).replace('VIEWPORT', viewport).replace('BUILD', build)
                 .replace('LABEL', LABELS[name]))
    code += VALVE_TESTS
    code += '\nint main() {\n' + ''.join(f' {name}::Run();\n' for name in VIEWPORTS)
    code += ' valve::RunRejectedSubmenu();\n}\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-viewport-command-menu-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
