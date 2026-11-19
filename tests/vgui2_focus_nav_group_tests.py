"""Exercise the production VGUI2 focus restore of EditablePanel and WizardPanel through FocusNavGroup, with the focused child of this module, of another module and deleted.

Run: python tests/vgui2_focus_nav_group_tests.py
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
};

// forwards to the client panel and resolves a VPANEL the way VPanelWrapper does: a panel of another module gives NULL
class IPanel {
public:
 Panel* GetPanel(VPANEL panel, const char* moduleName) {
  return _stricmp(nodes[panel].module, moduleName) ? nullptr : nodes[panel].panel;
 }
 void RequestFocus(VPANEL panel, int direction);
 bool RequestFocusNext(VPANEL panel, VPANEL existing);
 VPANEL GetCurrentKeyFocus(VPANEL panel);
 bool IsPopup(VPANEL panel);
 bool IsEnabled(VPANEL panel);
 bool IsVisible(VPANEL panel);
 std::map<VPANEL, Node> nodes;
};
static IPanel panels;
static IPanel* ipanel() { return &panels; }
static char controlsModule[] = "GameUI";
static char* GetControlsModuleName() { return controlsModule; }

// a deleted panel leaves the map, then the handle gives NULL like VPanelHandle
class VPanelHandle {
public:
 void Set(VPANEL panel) { _panel = panel; }
 VPANEL Get() { return panels.nodes.count(_panel) ? _panel : 0; }
 VPANEL _panel = 0;
};

// the key focus Panel::GetCurrentKeyFocus gives, to tell it from a child's key focus
static const VPANEL BASE_KEY_FOCUS = 1;

// only the members the focus restore uses
class Panel {
public:
 Panel(const char* module = controlsModule, int tabPosition = 0) : tabPosition(tabPosition) {
  _vpanel = reinterpret_cast<VPANEL>(this);
  panels.nodes[_vpanel] = Node{ module, this };
 }
 virtual ~Panel() {}
 void AddChild(Panel* child) { children.push_back(child); }
 void Delete() { panels.nodes.erase(_vpanel); }
 virtual void RequestFocus(int) { ++focusRequests; }
 virtual bool RequestFocusNext(VPANEL) { ++nextRequests; return true; }
 virtual void OnSetFocus() { ++setFocusCalls; }
 virtual void OnKillFocus() {}
 virtual VPANEL GetCurrentKeyFocus() { return BASE_KEY_FOCUS; }
 virtual bool IsPopup() { return popup; }
 virtual bool IsEnabled() { return enabled; }
 virtual bool IsVisible() { return visible; }
 int GetTabPosition() { return tabPosition; }
 int GetChildCount() { return static_cast<int>(children.size()); }
 Panel* GetChild(int index) { return children[index]; }
 VPANEL GetVPanel() { return _vpanel; }
 VPANEL _vpanel;
 std::vector<Panel*> children;
 int tabPosition;
 bool popup = false, enabled = true, visible = true;
 int focusRequests = 0, nextRequests = 0, setFocusCalls = 0;
};

void IPanel::RequestFocus(VPANEL panel, int direction) { nodes[panel].panel->RequestFocus(direction); }
bool IPanel::RequestFocusNext(VPANEL panel, VPANEL existing) { return nodes[panel].panel->RequestFocusNext(existing); }
VPANEL IPanel::GetCurrentKeyFocus(VPANEL panel) { return nodes[panel].panel->GetCurrentKeyFocus(); }
bool IPanel::IsPopup(VPANEL panel) { return nodes[panel].panel->IsPopup(); }
bool IPanel::IsEnabled(VPANEL panel) { return nodes[panel].panel->IsEnabled(); }
bool IPanel::IsVisible(VPANEL panel) { return nodes[panel].panel->IsVisible(); }

// a child of another module with its own key focus, like a client EditablePanel
class KeyFocusPanel : public Panel {
public:
 using Panel::Panel;
 VPANEL GetCurrentKeyFocus() override { return keyFocus; }
 VPANEL keyFocus = 0;
};

// only the members the focus restore uses
class FocusNavGroup {
public:
 FocusNavGroup(Panel* mainPanel) : _mainPanel(mainPanel) {}
 Panel* GetDefaultPanel();
 Panel* GetCurrentFocus();
 VPANEL GetCurrentFocusVPanel();
 VPanelHandle _currentFocus;
 Panel* _mainPanel;
};

class EditablePanel : public Panel {
public:
 EditablePanel() : m_NavGroup(this) {}
 void OnSetFocus() override;
 VPANEL GetCurrentKeyFocus() override;
 FocusNavGroup& GetFocusNavGroup() { return m_NavGroup; }
 FocusNavGroup m_NavGroup;
};

class WizardPanel : public EditablePanel {
public:
 void ResetKeyFocus();
 void ResetDefaultButton() {}
};
'''

TESTS = r'''
}
using namespace vgui2;

// the default child is first in tab order, the focused child is this module's, another module's or the page itself
template <class Page> struct Scene {
 Page page;
 Panel first{ controlsModule, 1 };
 Panel local{ controlsModule, 2 };
 KeyFocusPanel foreign{ "client", 3 };
 KeyFocusPanel sub{ "client" };
 Scene() {
  page.AddChild(&first);
  page.AddChild(&local);
  page.AddChild(&foreign);
  foreign.AddChild(&sub);
 }
 void Focus(Panel& panel) { page.m_NavGroup._currentFocus.Set(panel.GetVPanel()); }
};

static void TheChildOfThisModuleGetsTheFocusBack() {
 Scene<EditablePanel> scene;
 scene.Focus(scene.local);
 assert(&scene.local == scene.page.m_NavGroup.GetCurrentFocus());
 scene.page.OnSetFocus();
 assert(1 == scene.local.focusRequests);
 assert(0 == scene.first.focusRequests);
 assert(0 == scene.first.setFocusCalls);
 assert(BASE_KEY_FOCUS == scene.page.GetCurrentKeyFocus());
 puts("PASS: EditablePanel gives the focus back to its child of this module and chains its key focus");
}

static void TheChildOfAnotherModuleGetsTheFocusBack() {
 Scene<EditablePanel> scene;
 scene.Focus(scene.foreign);
 assert(nullptr == scene.page.m_NavGroup.GetCurrentFocus());
 assert(scene.foreign.GetVPanel() == scene.page.m_NavGroup.GetCurrentFocusVPanel());
 scene.page.OnSetFocus();
 assert(1 == scene.foreign.focusRequests);
 assert(0 == scene.first.focusRequests);
 assert(0 == scene.first.setFocusCalls);
 assert(scene.foreign.GetVPanel() == scene.page.GetCurrentKeyFocus());
 scene.foreign.keyFocus = scene.sub.GetVPanel();
 assert(scene.sub.GetVPanel() == scene.page.GetCurrentKeyFocus());
 scene.foreign.popup = true;
 assert(BASE_KEY_FOCUS == scene.page.GetCurrentKeyFocus());
 puts("PASS: EditablePanel gives the focus back to its child of another module and chains its key focus, a popup excepted");
}

static void ADeletedChildFallsBackToTheDefaultPanel() {
 Scene<EditablePanel> scene;
 scene.Focus(scene.foreign);
 scene.foreign.Delete();
 assert(0 == scene.page.m_NavGroup.GetCurrentFocusVPanel());
 scene.page.OnSetFocus();
 assert(1 == scene.first.focusRequests);
 assert(1 == scene.first.setFocusCalls);
 assert(0 == scene.foreign.focusRequests);
 assert(BASE_KEY_FOCUS == scene.page.GetCurrentKeyFocus());
 puts("PASS: EditablePanel falls back to the default panel once the focused child is deleted");
}

static void ThePageItselfFallsBackToTheDefaultPanel() {
 Scene<EditablePanel> scene;
 scene.page.m_NavGroup._currentFocus.Set(scene.page.GetVPanel());
 scene.page.OnSetFocus();
 assert(0 == scene.page.focusRequests);
 assert(1 == scene.first.focusRequests);
 assert(1 == scene.first.setFocusCalls);
 assert(0 == scene.page.GetCurrentKeyFocus());
 puts("PASS: EditablePanel focused on itself falls back to the default panel and has no key focus");
}

static void WizardPanelResetsTheFocusOfAnotherModule() {
 Scene<WizardPanel> scene;
 scene.Focus(scene.foreign);
 scene.page.ResetKeyFocus();
 assert(1 == scene.foreign.focusRequests);
 assert(0 == scene.foreign.nextRequests);
 scene.foreign.enabled = false;
 scene.page.ResetKeyFocus();
 assert(1 == scene.foreign.focusRequests);
 assert(1 == scene.foreign.nextRequests);
 scene.foreign.Delete();
 scene.page.ResetKeyFocus();
 assert(1 == scene.foreign.focusRequests);
 assert(1 == scene.foreign.nextRequests);
 puts("PASS: WizardPanel resets the key focus of its child of another module, moves on when disabled and skips it once deleted");
}

int main() {
 TheChildOfThisModuleGetsTheFocusBack();
 TheChildOfAnotherModuleGetsTheFocusBack();
 ADeletedChildFallsBackToTheDefaultPanel();
 ThePageItselfFallsBackToTheDefaultPanel();
 WizardPanelResetsTheFocusOfAnotherModule();
}
'''


def main():
    controls = ROOT / 'vgui2/controls'
    sources = {
        'FocusNavGroup.cpp': ('Panel* FocusNavGroup::GetDefaultPanel()', 'Panel* FocusNavGroup::GetCurrentFocus()',
                              'VPANEL FocusNavGroup::GetCurrentFocusVPanel()'),
        'EditablePanel.cpp': ('void EditablePanel::OnSetFocus()', 'VPANEL EditablePanel::GetCurrentKeyFocus()'),
        'WizardPanel.cpp': ('void WizardPanel::ResetKeyFocus()',),
    }
    code = STUBS
    for name, signatures in sources.items():
        source = (controls / name).read_text(encoding='utf-8')
        code += ''.join(function(source, signature) + '\n' for signature in signatures)
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-focus-nav-group-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
