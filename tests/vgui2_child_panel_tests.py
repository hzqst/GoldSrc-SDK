"""Exercise the production VGUI2 child traversals (hotkeys, move, layout, find by name, keyboard input) on a page with a child from another VGUI module.

Run: python tests/vgui2_child_panel_tests.py
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


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>

#ifndef _WIN32
#define _stricmp strcasecmp
#include <strings.h>
#endif

namespace vgui2 {
typedef uintptr_t VPANEL;
class Panel;

struct Node {
 const char* module;
 Panel* panel;
 std::vector<VPANEL> children;
 bool keyboard = true;
};

// resolves a VPANEL the way VPanelWrapper::GetPanel does: a panel of another module gives NULL
class IPanel {
public:
 int GetChildCount(VPANEL panel) { return static_cast<int>(nodes[panel].children.size()); }
 VPANEL GetChild(VPANEL panel, int index) { return nodes[panel].children[index]; }
 Panel* GetPanel(VPANEL panel, const char* moduleName) {
  return _stricmp(nodes[panel].module, moduleName) ? nullptr : nodes[panel].panel;
 }
 void SetKeyBoardInputEnabled(VPANEL panel, bool state) { nodes[panel].keyboard = state; }
 std::map<VPANEL, Node> nodes;
};
static IPanel panels;
static IPanel* ipanel() { return &panels; }
static char controlsModule[] = "GameUI";
static char* GetControlsModuleName() { return controlsModule; }

// only the members the child traversals use
class Panel {
public:
 Panel(const char* name, const char* module = controlsModule, wchar_t hotkey = 0) : _panelName(name), hotkey(hotkey) {
  _vpanel = reinterpret_cast<VPANEL>(this);
  panels.nodes[_vpanel] = Node{ module, this };
 }
 virtual ~Panel() {}
 void AddChild(Panel* child) { panels.nodes[_vpanel].children.push_back(child->_vpanel); }
 virtual Panel* HasHotkey(wchar_t key) { return key == hotkey ? this : nullptr; }
 virtual void OnMove() { ++moves; }
 virtual bool IsVisible() { return true; }
 virtual bool IsEnabled() { return true; }
 virtual void InvalidateLayout(bool layoutNow, bool reloadScheme);
 void PerformApplySchemeSettings() { ++schemes; }
 void PerformLayout() {}
 void Repaint() {}
 void InternalMove();
 int GetChildCount();
 Panel* GetChild(int index);
 Panel* FindChildByName(const char* childName, bool recurseDown);
 void SetKeyBoardInputEnabled(bool state);
 VPANEL GetVPanel();
 const char* GetName() { return _panelName; }
 struct Tooltip { void InvalidateLayout() {} }* m_pTooltips = nullptr;
 bool _needsLayout = false, _needsSchemeUpdate = false, m_bInPerformLayout = false;
 VPANEL _vpanel;
 const char* _panelName;
 wchar_t hotkey;
 int moves = 0, schemes = 0;
};

class EditablePanel : public Panel {
public:
 using Panel::Panel;
 Panel* HasHotkey(wchar_t key) override;
};

class PropertySheet : public Panel {
public:
 using Panel::Panel;
 Panel* HasHotkey(wchar_t key) override;
 Panel* _activePage = nullptr;
};
'''

TESTS = r'''
}
using namespace vgui2;

// page children: a panel of another module first, then a panel of this module with its own child
struct Scene {
 PropertySheet sheet{ "sheet" };
 EditablePanel page{ "page" };
 Panel foreign{ "foreign", "client", L'a' };
 Panel button{ "button", controlsModule, L'a' };
 Panel deep{ "deep" };
 Scene() {
  sheet.AddChild(&page);
  sheet._activePage = &page;
  page.AddChild(&foreign);
  page.AddChild(&button);
  button.AddChild(&deep);
 }
};

static void HotkeysSkipTheUnresolvedChild() {
 Scene scene;
 assert(&scene.button == scene.sheet.HasHotkey(L'a'));
 assert(&scene.button == scene.page.HasHotkey(L'a'));
 assert(!scene.sheet.HasHotkey(L'b'));
 assert(!scene.page.HasHotkey(L'b'));
 puts("PASS: PropertySheet and EditablePanel hotkeys skip the child of another module and find the next one");
}

static void MoveAndLayoutSkipTheUnresolvedChild() {
 Scene scene;
 scene.page.InternalMove();
 assert(1 == scene.page.moves);
 assert(1 == scene.button.moves);
 assert(0 == scene.foreign.moves);
 scene.page.InvalidateLayout(false, true);
 assert(1 == scene.page.schemes);
 assert(1 == scene.button.schemes);
 assert(1 == scene.deep.schemes);
 assert(0 == scene.foreign.schemes);
 puts("PASS: InternalMove and InvalidateLayout skip the child of another module and reach the next ones");
}

static void FindChildByNameSkipsTheUnresolvedChild() {
 Scene scene;
 assert(&scene.button == scene.page.FindChildByName("BUTTON", false));
 assert(!scene.page.FindChildByName("deep", false));
 assert(&scene.deep == scene.page.FindChildByName("deep", true));
 assert(!scene.page.FindChildByName("foreign", true));
 assert(!scene.page.FindChildByName("missing", true));
 puts("PASS: FindChildByName skips the child of another module and finds direct and nested children");
}

static void KeyboardInputSkipsTheUnresolvedChild() {
 Scene scene;
 scene.page.SetKeyBoardInputEnabled(false);
 assert(!panels.nodes[scene.page._vpanel].keyboard);
 assert(!panels.nodes[scene.button._vpanel].keyboard);
 assert(!panels.nodes[scene.deep._vpanel].keyboard);
 assert(panels.nodes[scene.foreign._vpanel].keyboard);
 puts("PASS: SetKeyBoardInputEnabled skips the child of another module and reaches the next ones");
}

int main() {
 HotkeysSkipTheUnresolvedChild();
 MoveAndLayoutSkipTheUnresolvedChild();
 FindChildByNameSkipsTheUnresolvedChild();
 KeyboardInputSkipsTheUnresolvedChild();
}
'''


def main():
    controls = ROOT / 'vgui2/controls'
    sources = {
        'Panel.cpp': ('void Panel::InternalMove()', 'void Panel::InvalidateLayout( bool layoutNow, bool reloadScheme )',
                      'int Panel::GetChildCount()', 'Panel* Panel::FindChildByName( const char* childName, bool recurseDown )',
                      'VPANEL Panel::GetVPanel()', 'Panel* Panel::GetChild( int index )',
                      'void Panel::SetKeyBoardInputEnabled( bool state )'),
        'EditablePanel.cpp': ('Panel* EditablePanel::HasHotkey( wchar_t key )',),
        'PropertySheet.cpp': ('Panel* PropertySheet::HasHotkey( wchar_t key )',),
    }
    code = STUBS
    for name, signatures in sources.items():
        source = (controls / name).read_text(encoding='utf-8')
        code += ''.join(function(source, signature) + '\n' for signature in signatures)
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-child-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
