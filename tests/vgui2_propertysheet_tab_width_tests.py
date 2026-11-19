"""Exercise the production VGUI2 PropertySheet tab width with the production PageTab, and check that the options and
add server dialogs set their tab width, in their proportional scale, before they add their pages.

Run: python tests/vgui2_propertysheet_tab_width_tests.py
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
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def declaration(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n};', start) + 3]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <vector>

namespace vgui2 {
typedef int MouseCode;
typedef int KeyCode;
struct Color {};
struct IScheme {};
struct IBorder {};

// the proportional scale of the dialogs, the screen height over the base height
static int scale = 2;

class KeyValues {
public:
 KeyValues(const char*) {}
 KeyValues(const char*, const char*, int) {}
};

template <class T> class Dar {
public:
 int GetCount() { return (int)items.size(); }
 bool HasElement(T item) { for (T i : items) if (i == item) return true; return false; }
 void AddElement(T item) { items.push_back(item); }
 T& operator[](int i) { return items[i]; }
 std::vector<T> items;
};

// only the members the tab width uses
class Panel {
public:
 virtual ~Panel() {}
 void SetParent(Panel*) {}
 void AddActionSignalTarget(Panel*) {}
 void PostMessage(Panel*, KeyValues* message, float) { delete message; }
 void SetVisible(bool) {}
 void InvalidateLayout(bool, bool) {}
 void Repaint() {}
 void GetSize(int& wide, int& tall) { wide = _wide; tall = _tall; }
 void SetSize(int wide, int tall) { _wide = wide; _tall = tall; }
 int GetWide() { return _wide; }
 int GetProportionalScaledValue(int value) { return value * scale; }
 Color GetSchemeColor(const char*, Color color, IScheme*) { return color; }
 Color GetFgColor() { return Color(); }
 virtual void OnKeyCodeTyped(KeyCode) {}
 int _wide = 64, _tall = 24;
};

// a tab whose title is contentWide pixels wide
static int contentWide = 20;

class Button : public Panel {
public:
 Button(Panel*, const char*, const char*) {}
 void SetCommand(KeyValues* command) { delete command; }
 virtual Color GetButtonFgColor() { return Color(); }
 virtual bool CanBeDefaultButton() { return true; }
 virtual IBorder* GetBorder(bool, bool, bool, bool) { return nullptr; }
 virtual void OnMouseReleased(MouseCode) {}
 virtual void OnMousePressed(MouseCode) {}
 virtual void ApplySchemeSettings(IScheme*) {}
 void GetContentSize(int& wide, int& tall) { wide = contentWide; tall = 10; }
};

class ComboBox {
public:
 void AddItem(const char*, KeyValues*) {}
};

class PageTab;

// only the members the tab width uses
class PropertySheet : public Panel {
public:
 PropertySheet() { _tabWidth = GetProportionalScaledValue( 64 ); }
 void AddPage(Panel* page, const char* title);
 void SetTabWidth(int pixels);
 Dar<Panel*> _pages;
 Dar<PageTab*> _pageTabs;
 int _tabWidth;
 bool _showTabs = true;
 ComboBox* _combo = nullptr;
};
'''

TESTS = r'''
// the tab overrides draw, click or move between tabs and are not under test
Color PageTab::GetButtonFgColor() { return Color(); }
void PageTab::SetActive(bool) {}
bool PageTab::CanBeDefaultButton() { return false; }
IBorder* PageTab::GetBorder(bool, bool, bool, bool) { return nullptr; }
void PageTab::OnMouseReleased(MouseCode) {}
void PageTab::OnMousePressed(MouseCode) {}
void PageTab::OnKeyCodeTyped(KeyCode) {}
}
using namespace vgui2;

// adds pages, then gives the tabs their scheme as the first solve does, and returns the first tab's width
static int TabWide(bool widthFirst, int width) {
 PropertySheet sheet;
 Panel pages[3];
 if (widthFirst) sheet.SetTabWidth(width);
 for (Panel& page : pages) sheet.AddPage(&page, "title");
 if (!widthFirst) sheet.SetTabWidth(width);
 IScheme scheme;
 for (PageTab* tab : sheet._pageTabs.items) static_cast<Button*>(tab)->ApplySchemeSettings(&scheme);
 int wide = sheet._pageTabs[0]->GetWide();
 for (PageTab* tab : sheet._pageTabs.items) { assert(wide == tab->GetWide()); delete tab; }
 return wide;
}

static void ATabWidthSetBeforeThePagesIsTheMinimum() {
 scale = 2;
 contentWide = 20;
 assert(72 * scale == TabWide(true, 72 * scale));
 // set after the pages, the tabs keep the default width
 assert(64 * scale == TabWide(false, 72 * scale));
 puts("PASS: a tab width set before the pages is the tabs' minimum width, one set after them leaves the default");
}

static void ALongTitleWidensItsTab() {
 scale = 2;
 contentWide = 200;
 assert(200 + 10 * scale == TabWide(true, 72 * scale));
 contentWide = 20;
 puts("PASS: a tab is still as wide as its title and margin when that passes the minimum width");
}

static void AnUnscaledWidthIsNarrowerThanTheDefault() {
 scale = 2;
 contentWide = 20;
 // the add server dialog used 72 pixels, below the scaled 64 its sheet starts with
 assert(72 == TabWide(true, 72) && 72 < 64 * scale);
 scale = 1;
 assert(72 == TabWide(true, 72));
 puts("PASS: an unscaled 72 pixel width stays 72 at any scale, so past a scale of 1.125 it is narrower than the scaled default");
}

int main() {
 setvbuf(stdout, nullptr, _IONBF, 0);
 ATabWidthSetBeforeThePagesIsTheMinimum();
 ALongTitleWidensItsTab();
 AnUnscaledWidthIsNarrowerThanTheDefault();
}
'''


def constructor_calls(source, signature):
    """Returns the constructor body and the offsets of its first AddPage and of its SetTabWidth call."""
    body = function(source, signature)
    add_page = re.search(r'\bAddPage\(', body)
    tab_width = re.search(r'SetTabWidth\(([^;]*)\);', body)
    assert add_page and tab_width, signature
    return add_page.start(), tab_width


def check_dialogs():
    for path, signature in (('GameUI/OptionsDialog.cpp', 'COptionsDialog::COptionsDialog( vgui2::Panel* parent )'),
                            ('serverbrowser/DialogAddServer.cpp', 'CDialogAddServer::CDialogAddServer(')):
        source = (ROOT / path).read_text(encoding='utf-8')
        add_page, tab_width = constructor_calls(source, signature)
        if tab_width.start() > add_page:
            raise SystemExit(f'FAIL: {path} sets the tab width after it adds a page, so its tabs keep the default')
        if 'GetProportionalScaledValue(' not in tab_width.group(1):
            raise SystemExit(f'FAIL: {path} sets an unscaled tab width while its sheet starts with a scaled one')
    print('PASS: the options and add server dialogs set a scaled tab width before they add their pages')


def main():
    check_dialogs()
    sheet = (ROOT / 'vgui2/controls/PropertySheet.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += declaration(sheet, 'class PageTab : public Button') + '\n'
    code += function(sheet, 'void PageTab::ApplySchemeSettings( IScheme* scheme )') + '\n'
    code += function(sheet, 'void PropertySheet::AddPage( Panel* page, const char* title )') + '\n'
    code += function(sheet, 'void PropertySheet::SetTabWidth( int pixels )') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-propertysheet-tab-width-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
