"""Exercise the production Team Fortress Classic text windows with a tracked COM_LoadFile and COM_FreeFile.

Run: python tests/tfc_text_window_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = re.search(re.escape(signature) + r'[^\n;]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#define _TFC
enum { SHOW_MAPBRIEFING = 1, SHOW_CLASSDESC, SHOW_MOTD, SHOW_SPECHELP };
enum { PC_UNDEFINED, PC_SCOUT, PC_SNIPER, PC_SOLDIER, PC_DEMOMAN, PC_MEDIC, PC_HVYWEAP, PC_PYRO, PC_SPY, PC_ENGINEER, PC_RANDOM, PC_CIVILIAN };
static const char* sTFClassSelection[] = {"undefined", "scout", "sniper", "soldier", "demoman", "medic", "hwguy", "pyro", "spy", "engineer", "randompc", "civilian"};
static int g_iPlayerClass = PC_UNDEFINED;
static int ScreenWidth = 640, ScreenHeight = 480;
static std::map<std::string, std::string> files;
static std::set<char*> loadedFiles;
static int wrongFrees = 0;
static struct {
 void* COM_LoadFile(const char* name, int, int*) {
  auto file = files.find(name);
  if (file == files.end()) return nullptr;
  char* buffer = new char[file->second.size() + 1];
  memcpy(buffer, file->second.c_str(), file->second.size() + 1);
  loadedFiles.insert(buffer);
  return buffer;
 }
 // frees only the buffers COM_LoadFile returned, and counts any other pointer instead of corrupting the heap
 void COM_FreeFile(void* file) {
  if (!file) return;
  if (!loadedFiles.erase((char*)file)) { ++wrongFrees; return; }
  delete[] (char*)file;
 }
 const char* pfnGetLevelName() { return "maps/2fort.bsp"; }
} gEngfuncs;
struct CHudTextMessage {
 static char* LocaliseTextString(const char* text, char* buffer, int size) {
  snprintf(buffer, size, "%s", text);
  return buffer;
 }
 static char* BufferedLocaliseTextString(const char* text) {
  static char buffer[1024];
  return LocaliseTextString(text, buffer, sizeof(buffer));
 }
};
struct Panel { virtual ~Panel() {} };
// keeps its own copy of the text like the production message window
struct CMenuPanel : Panel {
 std::string text, title;
 void setParent(Panel*) {}
};
static CMenuPanel* CMessageWindowPanel_Create(const char* text, const char* title, int, int, int, int, int, int) {
 CMenuPanel* panel = new CMenuPanel;
 panel->text = text;
 panel->title = title;
 return panel;
}
// only the members CreateTextWindow uses
class ViewportStub : public Panel {
public:
 char m_szServerName[64] = "Server";
 char m_szMOTD[64] = "Welcome";
 char m_sMapName[64] = "";
};
'''

TESTS = r'''
namespace NAME {
static void Run() {
 files = {{"classes/long_scout.txt", "Scout long help"}, {"maps/2fort.txt", "Capture the flag"}};
 TeamFortressViewport viewport;
 g_iPlayerClass = PC_SCOUT;
 CMenuPanel* panel = viewport.CreateTextWindow(SHOW_CLASSDESC);
 assert(std::string("Scout long help") == panel->text);
 delete panel;
 assert(loadedFiles.empty() && 0 == wrongFrees);
 // the short localised help is shown when there is no long class description
 g_iPlayerClass = PC_MEDIC;
 panel = viewport.CreateTextWindow(SHOW_CLASSDESC);
 assert(std::string("#Help_medic") == panel->text);
 delete panel;
 panel = viewport.CreateTextWindow(SHOW_MAPBRIEFING);
 assert(std::string("Capture the flag") == panel->text);
 delete panel;
 assert(loadedFiles.empty() && 0 == wrongFrees);
 puts("PASS: the LABEL class description frees the long class description file it loads");
}
}
'''

VIEWPORTS = {'tfc': ('tfc/cl_dll/vgui_TeamFortressViewport.cpp', 'Team Fortress Classic'),
             'valve': ('cl_dll/vgui_TeamFortressViewport.cpp', 'Half-Life viewport built with _TFC')}


def main():
    code = STUBS
    for name, (path, label) in VIEWPORTS.items():
        source = (ROOT / path).read_text(encoding='latin-1')
        code += f'namespace {name} {{\nclass TeamFortressViewport : public ViewportStub {{\npublic:\n CMenuPanel* CreateTextWindow(int iTextToShow);\n}};\n'
        code += function(source, 'CMenuPanel* TeamFortressViewport::CreateTextWindow(') + '\n}\n'
        code += TESTS.replace('NAME', name).replace('LABEL', label)
    code += '\nint main() {\n' + ''.join(f' {name}::Run();\n' for name in VIEWPORTS) + '}\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-tfc-text-window-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
