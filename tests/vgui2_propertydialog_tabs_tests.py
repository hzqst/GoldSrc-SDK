"""Exercise the production VGUI2 PropertySheet tab width and tab layout with the production PropertyDialog layout and Frame client area, for tabs that fit, overflow or are hidden and dialogs near the workspace edge.

Run: python tests/vgui2_propertydialog_tabs_tests.py
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
#include <vector>

namespace vgui2 {
typedef int HFont;

// the title font is FONT_TALL, so the client area starts 5 + FONT_TALL + 8 below the frame top
static const int FONT_TALL = 10;

struct ISurface {
 int GetFontTall(HFont) { return FONT_TALL; }
 void GetWorkspaceBounds(int& x, int& y, int& wide, int& tall) { x = workspaceX; y = 0; wide = workspaceWide; tall = 768; }
 int workspaceX = 0;
 int workspaceWide = 1024;
};
static ISurface theSurface;
static ISurface* surface() { return &theSurface; }

template <class T> class CUtlVector {
public:
 int GetCount() { return (int)items.size(); }
 T& operator[](int i) { return items[i]; }
 std::vector<T> items;
};

// only the members the layouts use; proportional values are not scaled
class Panel {
public:
 virtual ~Panel() {}
 virtual void PerformLayout() {}
 void SetBounds(int x, int y, int wide, int tall) { _x = x; _y = y; _wide = wide; _tall = tall; }
 void GetBounds(int& x, int& y, int& wide, int& tall) { x = _x; y = _y; wide = _wide; tall = _tall; }
 void GetSize(int& wide, int& tall) { wide = _wide; tall = _tall; }
 int GetWide() { return _wide; }
 int GetTall() { return _tall; }
 void SetVisible(bool state) { _visible = state; }
 bool IsVisible() { return _visible; }
 void InvalidateLayout(bool, bool) {}
 void Repaint() {}
 void MoveToFront() {}
 void RequestFocus(int) {}
 int GetProportionalScaledValue(int value) { return value; }
 int _x = 0, _y = 0, _wide = 0, _tall = 0;
 bool _visible = true;
};

class Label : public Panel {
public:
 HFont GetFont() { return 0; }
};

class PageTab : public Panel {};

// only the members the tab width and the tab layout use
class PropertySheet : public Panel {
public:
 typedef Panel BaseClass;
 void PerformLayout() override;
 int GetTabsWide();
 void ChangeActiveTab(int) {}
 CUtlVector<PageTab*> _pageTabs;
 Panel* _activePage = nullptr;
 PageTab* _activeTab = nullptr;
 int _tabHeight = 28;
 bool _showTabs = true;
};

// only the members the client area uses; records the width it lays out for
class Frame : public Panel {
public:
 void PerformLayout() override { layoutWide = GetWide(); }
 void GetClientArea(int& x, int& y, int& width, int& height);
 Label* _title = nullptr;
 bool _drawTitleBar = true;
 int layoutWide = 0;
};

class Button : public Panel {};

class PropertyDialog : public Frame {
public:
 void PerformLayout() override;
 PropertySheet* _propertySheet = nullptr;
 Button* _okButton = nullptr;
 Button* _cancelButton = nullptr;
 Button* _applyButton = nullptr;
};
'''

TESTS = r'''
}
using namespace vgui2;

// the options dialog size, its client area is 522 - 5 - 5 = 512 wide
static const int DIALOG_WIDE = 522;
static const int CLIENT_WIDE = DIALOG_WIDE - 10;
static const int DIALOG_TALL = 444;
static const int TAB_COUNT = 7;

struct Fixture {
 Fixture() {
  theSurface.workspaceX = 0;
  theSurface.workspaceWide = 1024;
  dialog._title = &title;
  dialog._propertySheet = &sheet;
  dialog._okButton = &ok;
  dialog._cancelButton = &cancel;
  dialog._applyButton = &apply;
  for (PageTab& tab : tabs) sheet._pageTabs.items.push_back(&tab);
  sheet._activePage = &page;
  sheet._activeTab = &tabs[0];
  dialog.SetBounds(100, 50, DIALOG_WIDE, DIALOG_TALL);
 }
 // gives the tabs widths that take tabsWide side by side, 1 pixel apart
 void SetTabsWide(int tabsWide) {
  int each = (tabsWide - (TAB_COUNT - 1)) / TAB_COUNT;
  for (PageTab& tab : tabs) tab.SetBounds(0, 0, each, 25);
  tabs[TAB_COUNT - 1].SetBounds(0, 0, tabsWide - (TAB_COUNT - 1) - each * (TAB_COUNT - 1), 25);
  assert(tabsWide == sheet.GetTabsWide());
 }
 // lays out the dialog, then the sheet inside it as the solve traverse does
 void Layout() {
  dialog.PerformLayout();
  sheet.PerformLayout();
 }
 // the last tab ends inside the sheet, so it is drawn and clickable
 bool LastTabFits() {
  PageTab& last = tabs[TAB_COUNT - 1];
  return last._x + last._wide <= sheet._wide;
 }
 PropertyDialog dialog;
 PropertySheet sheet;
 Label title;
 Button ok, cancel, apply;
 Panel page;
 PageTab tabs[TAB_COUNT];
};

static void TabsThatFitKeepTheDialogWidth() {
 Fixture f;
 f.SetTabsWide(CLIENT_WIDE);
 f.Layout();
 assert(DIALOG_WIDE == f.dialog._wide && 100 == f.dialog._x);
 assert(CLIENT_WIDE == f.sheet._wide);
 assert(f.LastTabFits());
 assert(CLIENT_WIDE == f.tabs[TAB_COUNT - 1]._x + f.tabs[TAB_COUNT - 1]._wide);
 puts("PASS: tabs exactly as wide as the sheet keep the dialog width and the last tab ends at the sheet edge");
}

static void OverflowingTabsWidenTheDialog() {
 for (int extra : { 1, 10, 150 }) {
  Fixture f;
  f.SetTabsWide(CLIENT_WIDE + extra);
  f.Layout();
  assert(DIALOG_WIDE + extra == f.dialog._wide);
  assert(100 == f.dialog._x && 50 == f.dialog._y && DIALOG_TALL == f.dialog._tall);
  assert(DIALOG_WIDE + extra == f.dialog.layoutWide);
  assert(CLIENT_WIDE + extra == f.sheet._wide);
  assert(f.LastTabFits());
  // the buttons follow the new right edge
  assert(5 + CLIENT_WIDE + extra - 80 == f.apply._x);
  f.Layout();
  assert(DIALOG_WIDE + extra == f.dialog._wide && 100 == f.dialog._x);
 }
 puts("PASS: tabs wider than the sheet widen the dialog before the frame lays out, so the last tab fits, and a second layout keeps the width");
}

static void HiddenTabsKeepTheDialogWidth() {
 Fixture f;
 f.SetTabsWide(CLIENT_WIDE + 100);
 f.sheet._showTabs = false;
 assert(0 == f.sheet.GetTabsWide());
 f.Layout();
 assert(DIALOG_WIDE == f.dialog._wide);
 f.sheet._showTabs = true;
 f.sheet._pageTabs.items.clear();
 assert(0 == f.sheet.GetTabsWide());
 f.Layout();
 assert(DIALOG_WIDE == f.dialog._wide);
 puts("PASS: hidden tabs or no tab take no width and keep the dialog width");
}

static void AWidenedDialogStaysInTheWorkspace() {
 {
  // the right edge would pass the workspace, the dialog moves left
  Fixture f;
  f.dialog.SetBounds(600, 50, DIALOG_WIDE, DIALOG_TALL);
  f.SetTabsWide(CLIENT_WIDE + 50);
  f.Layout();
  assert(DIALOG_WIDE + 50 == f.dialog._wide);
  assert(1024 - (DIALOG_WIDE + 50) == f.dialog._x);
  assert(f.LastTabFits());
 }
 {
  // the workspace is narrower than the tabs need, the dialog takes the workspace width
  Fixture f;
  theSurface.workspaceX = 200;
  theSurface.workspaceWide = 600;
  f.dialog.SetBounds(300, 50, DIALOG_WIDE, DIALOG_TALL);
  f.SetTabsWide(CLIENT_WIDE + 300);
  f.Layout();
  assert(600 == f.dialog._wide && 200 == f.dialog._x);
  assert(590 == f.sheet._wide);
 }
 {
  // a dialog already wider than the workspace is not narrowed
  Fixture f;
  theSurface.workspaceWide = 500;
  f.SetTabsWide(CLIENT_WIDE + 50);
  f.Layout();
  assert(DIALOG_WIDE == f.dialog._wide && 100 == f.dialog._x);
 }
 puts("PASS: a widened dialog moves left to stay in the workspace, takes at most the workspace width and is never narrowed");
}

int main() {
 setvbuf(stdout, nullptr, _IONBF, 0);
 TabsThatFitKeepTheDialogWidth();
 OverflowingTabsWidenTheDialog();
 HiddenTabsKeepTheDialogWidth();
 AWidenedDialogStaysInTheWorkspace();
}
'''


def main():
    controls = ROOT / 'vgui2/controls'
    sheet = (controls / 'PropertySheet.cpp').read_text(encoding='utf-8')
    dialog = (controls / 'PropertyDialog.cpp').read_text(encoding='utf-8')
    frame = (controls / 'Frame.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += function(sheet, 'int PropertySheet::GetTabsWide()') + '\n'
    code += function(sheet, 'void PropertySheet::PerformLayout()') + '\n'
    code += function(frame, 'void Frame::GetClientArea( int& x, int& y, int& width, int& height )') + '\n'
    code += function(dialog, 'void PropertyDialog::PerformLayout()') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-propertydialog-tabs-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
