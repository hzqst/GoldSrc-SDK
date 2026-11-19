"""Exercise the production VGUI2 controls that walk a parent or siblings, post to the parent or resolve a panel, with a parent, child, sibling or menu item of another VGUI module.

Run: python tests/vgui2_cross_module_parent_tests.py
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
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#ifndef _WIN32
#define _stricmp strcasecmp
#include <strings.h>
#endif

#define ASSERT_AT( expression, line )	assert( expression )

static void Require(bool condition, const char* message) {
 if (condition) return;
 printf("FAIL: %s\n", message);
 exit(1);
}

namespace vgui2 {
typedef uintptr_t VPANEL;
typedef int MouseCode;
enum KeyCode { KEY_B = 1, KEY_R, KEY_F4, KEY_ENTER, KEY_ESCAPE, KEY_LSHIFT, KEY_RSHIFT, KEY_LCONTROL, KEY_RCONTROL, KEY_LALT, KEY_RALT };
class Panel;

// only the members the controls use
class KeyValues {
public:
 KeyValues(const char* name) : name(name) {}
 KeyValues(const char* name, const char*, const char*) : name(name) {}
 KeyValues(const char* name, const char*, int) : name(name) {}
 const char* GetName() { return name.c_str(); }
 KeyValues* FindKey(const char*, bool) { return this; }
 void deleteThis() { delete this; }
 std::string name;
};

struct Node {
 const char* module;
 Panel* panel;
 VPANEL parent;
 std::vector<VPANEL> children;
};

// resolves a VPANEL the way VPanelWrapper::GetPanel does: the embedded panel or a panel of another module gives NULL
class IPanel {
public:
 Panel* GetPanel(VPANEL panel, const char* moduleName) {
  if (panel == embedded) return nullptr;
  return _stricmp(nodes[panel].module, moduleName) ? nullptr : nodes[panel].panel;
 }
 int GetChildCount(VPANEL panel) { return static_cast<int>(nodes[panel].children.size()); }
 VPANEL GetChild(VPANEL panel, int index) { return nodes[panel].children[index]; }
 bool IsVisible(VPANEL) { return true; }
 bool IsEnabled(VPANEL) { return true; }
 std::map<VPANEL, Node> nodes;
 VPANEL embedded = 0;
};
static IPanel panels;
static IPanel* ipanel() { return &panels; }
static char controlsModule[] = "GameUI";
static char* GetControlsModuleName() { return controlsModule; }

struct Post {
 VPANEL target;
 std::string name;
};

// a message to no panel is dropped like CVGui::PostMessage
class IVGui {
public:
 void PostMessage(VPANEL target, KeyValues* message, VPANEL, float) {
  if (target) posts.push_back(Post{ target, message->GetName() });
  message->deleteThis();
 }
 std::vector<Post> posts;
};
static IVGui gui;
static IVGui* ivgui() { return &gui; }

class ISurface {
public:
 enum SurfaceFeature_e { ESCAPE_KEY };
 VPANEL GetEmbeddedPanel() { return panels.embedded; }
 bool SupportsFeature(SurfaceFeature_e) { return false; }
};
static ISurface theSurface;
static ISurface* surface() { return &theSurface; }

class IInput {
public:
 bool IsKeyDown(KeyCode code) { return down.count(code) != 0; }
 VPANEL GetAppModalSurface() { return 0; }
 std::set<int> down;
};
static IInput theInput;
static IInput* input() { return &theInput; }

class ISchemeManager {
public:
 void ReloadSchemes() { ++reloads; }
 int reloads = 0;
};
static ISchemeManager theScheme;
static ISchemeManager* scheme() { return &theScheme; }

// only the members the controls use
class Panel {
public:
 Panel(const char* name, const char* module = controlsModule) : _panelName(name) {
  // a small id, so that a VPANEL survives the int Menu takes it as on a 64-bit build
  static VPANEL nextPanel = 1;
  _vpanel = nextPanel++;
  panels.nodes[_vpanel] = Node{ module, this, 0 };
 }
 virtual ~Panel() {}
 void AddChild(Panel* child) {
  panels.nodes[_vpanel].children.push_back(child->_vpanel);
  panels.nodes[child->_vpanel].parent = _vpanel;
 }
 VPANEL GetVPanel() { return _vpanel; }
 VPANEL GetVParent() { return panels.nodes[_vpanel].parent; }
 // resolves the parent in this module like Panel::GetParent
 Panel* GetParent() { return GetVParent() ? ipanel()->GetPanel(GetVParent(), GetControlsModuleName()) : nullptr; }
 const char* GetModuleName() { return GetControlsModuleName(); }
 const char* GetName() { return _panelName; }
 int GetChildCount() { return ipanel()->GetChildCount(_vpanel); }
 Panel* GetChild(int index) { return ipanel()->GetPanel(ipanel()->GetChild(_vpanel, index), GetControlsModuleName()); }
 Panel* FindSiblingByName(const char* siblingName);
 void SetProportional(bool state);
 bool IsProportional() { return m_bProportional; }
 virtual void InvalidateLayout(bool, bool reloadScheme) { ++layouts; if (reloadScheme) ++schemeReloads; }
 virtual void RequestFocus(int) { ++focusRequests; }
 virtual bool HasUserConfigSettings() { return userConfig; }
 virtual void ApplyUserConfigSettings(KeyValues*) { ++userConfigApplies; }
 virtual void GetUserConfigSettings(KeyValues*) { ++userConfigGets; }
 void Repaint() {}
 // dereferences the target like Panel::PostMessage
 void PostMessage(Panel* target, KeyValues* message, float delay) {
  Require(target != nullptr, "a message is posted to a NULL Panel");
  ivgui()->PostMessage(target->GetVPanel(), message, _vpanel, delay);
 }
 void PostMessage(VPANEL target, KeyValues* message, float delay) { ivgui()->PostMessage(target, message, _vpanel, delay); }
 VPANEL _vpanel;
 const char* _panelName;
 bool m_bProportional = false, userConfig = true;
 int layouts = 0, schemeReloads = 0, focusRequests = 0, userConfigApplies = 0, userConfigGets = 0;
};

class EditablePanel : public Panel {
public:
 using Panel::Panel;
 void ApplyUserConfigSettings(KeyValues* userConfig) override;
 void GetUserConfigSettings(KeyValues* userConfig) override;
};

class RadioButton : public Panel {
public:
 RadioButton(const char* name, int subTabPosition) : Panel(name), _subTabPosition(subTabPosition) {}
 RadioButton* FindBestRadioButton(int direction);
 int GetSubTabPosition();
 int GetRadioTabPosition();
 int _oldTabPosition = 1;
 int _subTabPosition;
};

class HTML : public Panel {
public:
 using Panel::Panel;
 void BrowserSetHTMLTitle(const char* title);
 void OnSetHTMLTitle(const char*) { ++titles; }
 int titles = 0;
};

class Dragger : public Panel {
public:
 Dragger() : Panel("dragger") {}
 void OnMouseDoublePressed(MouseCode code);
 int m_iDragger = 0;
 bool m_bMovable = true;
};

class MenuItem : public Panel {
public:
 using Panel::Panel;
 void ArmItem() { ++arms; }
 void DisarmItem() { ++disarms; }
 void OpenCascadeMenu() {}
 int arms = 0, disarms = 0;
};

class Menu : public Panel {
public:
 enum { MOUSE, KEYBOARD };
 Menu() : Panel("menu") {}
 void OnCursorEnteredMenuItem(int VPanel);
 void OnCursorExitedMenuItem(int VPanel);
 int GetMenuItemID(MenuItem*) { return 0; }
 void SetCurrentlySelectedItem(int) { ++selections; }
 int m_iInputMode = MOUSE;
 int selections = 0;
};

struct FocusNavGroup { VPANEL GetCurrentDefaultButton() { return 0; } };

class Frame : public EditablePanel {
public:
 Frame() : EditablePanel("frame") {}
 void OnKeyCodeTyped(KeyCode code);
 void ActivateBuildMode() {}
 void CloseModal() {}
 FocusNavGroup& GetFocusNavGroup() { return navGroup; }
 FocusNavGroup navGroup;
};
'''

TESTS = r'''
}
using namespace vgui2;

static void RadioButtonsMoveUnderAParentOfAnotherModule() {
 Panel parent{ "parent", "client" };
 Panel foreign{ "foreign", "client" };
 RadioButton first{ "first", 0 }, second{ "second", 1 }, third{ "third", 2 };
 parent.AddChild(&foreign);
 parent.AddChild(&first);
 parent.AddChild(&second);
 parent.AddChild(&third);
 assert(&second == first.FindBestRadioButton(1));
 assert(1 == second.focusRequests);
 assert(&second == third.FindBestRadioButton(-1));
 assert(&first == second.FindBestRadioButton(-1));
 puts("PASS: RadioButton moves to its sibling under a parent of another module and skips the sibling of another module");
}

static void SchemeReloadReachesTheTopLevelPanelsOfThisModule() {
 Panel embedded{ "embedded" };
 panels.embedded = embedded.GetVPanel();
 Frame frame;
 Panel foreign{ "foreign", "client" };
 embedded.AddChild(&foreign);
 embedded.AddChild(&frame);
 theInput.down = { KEY_LSHIFT, KEY_LCONTROL, KEY_LALT };
 frame.OnKeyCodeTyped(KEY_R);
 assert(1 == theScheme.reloads);
 assert(1 == frame.schemeReloads);
 assert(0 == foreign.schemeReloads);
 assert(0 == embedded.schemeReloads);
 theInput.down = { KEY_LSHIFT, KEY_LCONTROL };
 frame.OnKeyCodeTyped(KEY_R);
 assert(1 == theScheme.reloads);
 panels.embedded = 0;
 theInput.down.clear();
 puts("PASS: ctrl+shift+alt+R reloads the schemes and the top level panels of this module under the embedded panel");
}

static void HTMLAndDraggerPostToAParentOfAnotherModule() {
 Panel parent{ "parent", "client" };
 HTML html{ "html" };
 Dragger dragger;
 parent.AddChild(&html);
 parent.AddChild(&dragger);
 gui.posts.clear();
 html.BrowserSetHTMLTitle("title");
 dragger.OnMouseDoublePressed(0);
 assert(1 == html.titles);
 assert(2 == static_cast<int>(gui.posts.size()));
 assert(parent.GetVPanel() == gui.posts[0].target && "OnSetHTMLTitle" == gui.posts[0].name);
 assert(parent.GetVPanel() == gui.posts[1].target && "ResizeColumnToContents" == gui.posts[1].name);
 HTML orphan{ "orphan" };
 orphan.BrowserSetHTMLTitle("title");
 assert(1 == orphan.titles);
 assert(2 == static_cast<int>(gui.posts.size()));
 puts("PASS: HTML and the ListPanel dragger post to a parent of another module and HTML drops the message without a parent");
}

static void UserConfigSkipsTheChildOfAnotherModule() {
 EditablePanel page{ "page" };
 Panel foreign{ "foreign", "client" };
 Panel local{ "local" };
 Panel unnamed{ "" };
 page.AddChild(&foreign);
 page.AddChild(&local);
 page.AddChild(&unnamed);
 KeyValues config("config");
 page.ApplyUserConfigSettings(&config);
 page.GetUserConfigSettings(&config);
 assert(1 == local.userConfigApplies && 1 == local.userConfigGets);
 assert(0 == foreign.userConfigApplies && 0 == foreign.userConfigGets);
 assert(0 == unnamed.userConfigApplies && 0 == unnamed.userConfigGets);
 puts("PASS: EditablePanel user config settings skip the child of another module and reach the named child");
}

static void SetProportionalSkipsTheChildOfAnotherModule() {
 Panel page{ "page" };
 Panel foreign{ "foreign", "client" };
 Panel local{ "local" };
 page.AddChild(&foreign);
 page.AddChild(&local);
 page.SetProportional(true);
 assert(page.IsProportional() && local.IsProportional());
 assert(!foreign.IsProportional());
 puts("PASS: SetProportional skips the child of another module and reaches the next one");
}

static void FindSiblingByNameSkipsTheSiblingOfAnotherModule() {
 Panel parent{ "parent" };
 Panel foreign{ "foreign", "client" };
 Panel self{ "self" };
 Panel sibling{ "sibling" };
 parent.AddChild(&foreign);
 parent.AddChild(&self);
 parent.AddChild(&sibling);
 assert(&sibling == self.FindSiblingByName("SIBLING"));
 assert(nullptr == self.FindSiblingByName("foreign"));
 assert(nullptr == self.FindSiblingByName("missing"));
 puts("PASS: FindSiblingByName skips the sibling of another module and finds the next one");
}

static void MenuIgnoresAnItemOfAnotherModule() {
 Menu menu;
 MenuItem foreign{ "foreign", "client" };
 MenuItem local{ "local" };
 menu.OnCursorEnteredMenuItem(static_cast<int>(foreign.GetVPanel()));
 menu.OnCursorExitedMenuItem(static_cast<int>(foreign.GetVPanel()));
 assert(0 == foreign.arms && 0 == foreign.disarms && 0 == menu.selections);
 menu.OnCursorEnteredMenuItem(static_cast<int>(local.GetVPanel()));
 menu.OnCursorExitedMenuItem(static_cast<int>(local.GetVPanel()));
 assert(1 == local.arms && 1 == local.disarms && 1 == menu.selections);
 puts("PASS: Menu ignores the cursor on an item of another module and arms and disarms its own item");
}

int main() {
 setvbuf(stdout, nullptr, _IONBF, 0);
 RadioButtonsMoveUnderAParentOfAnotherModule();
 SchemeReloadReachesTheTopLevelPanelsOfThisModule();
 HTMLAndDraggerPostToAParentOfAnotherModule();
 UserConfigSkipsTheChildOfAnotherModule();
 SetProportionalSkipsTheChildOfAnotherModule();
 FindSiblingByNameSkipsTheSiblingOfAnotherModule();
 MenuIgnoresAnItemOfAnotherModule();
}
'''

SOURCES = {
    'RadioButton.cpp': ('RadioButton* RadioButton::FindBestRadioButton( int direction )', 'int RadioButton::GetSubTabPosition()',
                        'int RadioButton::GetRadioTabPosition()'),
    'Frame.cpp': ('void Frame::OnKeyCodeTyped( KeyCode code )',),
    'HTML.cpp': ('void HTML::BrowserSetHTMLTitle( const char* title )',),
    'ListPanel.cpp': ('void Dragger::OnMouseDoublePressed( MouseCode )',),
    'EditablePanel.cpp': ('void EditablePanel::ApplyUserConfigSettings( KeyValues* userConfig )',
                          'void EditablePanel::GetUserConfigSettings( KeyValues* userConfig )'),
    'Panel.cpp': ('void Panel::SetProportional( bool state )', 'Panel* Panel::FindSiblingByName( const char* siblingName )'),
    'Menu.cpp': ('void Menu::OnCursorEnteredMenuItem( int VPanel )', 'void Menu::OnCursorExitedMenuItem( int VPanel )'),
}


def main():
    controls = ROOT / 'vgui2/controls'
    code = STUBS
    for name, signatures in SOURCES.items():
        source = (controls / name).read_text(encoding='utf-8')
        code += ''.join(function(source, signature) + '\n' for signature in signatures)
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-cross-module-parent-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
