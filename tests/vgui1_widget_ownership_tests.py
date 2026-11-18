"""Compile the VGUI1 widgets that own objects of their own and check them (#308).

Run: python tests/vgui1_widget_ownership_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

TextPanel, TextGrid, EditPanel, CheckButton, RadioButton, WizardPanel, Frame,
Menu, ListPanel, TabPanel and TablePanel are built with the VGUI1 sources and
platform stubs from vgui1_panel_tests.py, twice: with VGUI_ABI_COMPAT_MODE=1
(the original vgui.dll layout, no destructors) and with VGUI_ABI_COMPAT_MODE=0.
vgui_strdup is extracted from vgui.cpp like vgui_strcpy. The compatible build
frees no panel, so its runs check memory errors only.

- wizard-page-texts: setting a WizardPage text again frees the old one with
  delete[].
- frame-cursors: a Frame's grips use the scheme's cursors.
- shared-layouts: every Menu, ListPanel, TreeFolder and Label property panel
  shares one layout of its kind.
With VGUI_ABI_COMPAT_MODE=0 only:
- toggle-copy: a copied ToggleButton toggles through a controller of its own.
- text-panel: deleting a TextPanel frees its TextImage; a copy has its own.
- text-grid: deleting a TextGrid frees its grid; a copy has its own.
- edit-panel: deleting an EditPanel frees its lines; a copy has its own.
- check-button / radio-button: deleting the button frees the image it made,
  also after setImage replaced it; a copy has its own image.
- wizard-page: deleting a WizardPage frees its texts; a copy has its own.
- frame: deleting a Frame frees its title; a copy has its own.
- tab-panel: deleting a TabPanel frees its ButtonGroup; a copy has its own.
- table-panel: deleting a TablePanel frees its fake input panel; a copy has
  its own.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program
from vgui1_panel_tests import VGUI, function, vgui1_sources

WIDGETS = ['TextPanel.cpp', 'TextGrid.cpp', 'EditPanel.cpp', 'CheckButton.cpp', 'RadioButton.cpp',
           'WizardPanel.cpp', 'Frame.cpp', 'Menu.cpp', 'ListPanel.cpp', 'TabPanel.cpp', 'TablePanel.cpp',
           'StackLayout.cpp', 'ScrollBar.cpp', 'Slider.cpp', 'ButtonGroup.cpp', 'RaisedBorder.cpp', 'HeaderPanel.cpp']

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

// The test reads the objects the widgets own.
#define private public
#define protected public
#include "VGUI_App.h"
#include "VGUI_CheckButton.h"
#include "VGUI_EditPanel.h"
#include "VGUI_Frame.h"
#include "VGUI_Image.h"
#include "VGUI_Label.h"
#include "VGUI_ListPanel.h"
#include "VGUI_Menu.h"
#include "VGUI_RadioButton.h"
#include "VGUI_Scheme.h"
#include "VGUI_TabPanel.h"
#include "VGUI_TablePanel.h"
#include "VGUI_TextGrid.h"
#include "VGUI_TextImage.h"
#include "VGUI_TextPanel.h"
#include "VGUI_ToggleButton.h"
#include "VGUI_TreeFolder.h"
#include "VGUI_WizardPanel.h"
#undef protected
#undef private

using namespace vgui;

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}
};

static const int TEXT_BUFFER_SIZE = 64;
static const int GRID_WIDE = 4;
static const int GRID_TALL = 2;
// The capacity EditPanel gives a new line.
static const int LINE_CAPACITY = 4;

// Clicks a panel the way App does, through its protected internalMousePressed
// and internalMouseReleased.
class PanelInput : public Panel
{
public:
 static void click( Panel* panel )
 {
  void ( Panel::*pressed )( MouseCode ) = &PanelInput::internalMousePressed;
  void ( Panel::*released )( MouseCode ) = &PanelInput::internalMouseReleased;
  ( panel->*pressed )( MOUSE_LEFT );
  ( panel->*released )( MOUSE_LEFT );
 }
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

class TestTable : public TablePanel
{
public:
 TestTable() : TablePanel( 0, 0, 100, 100, 2 ) {}
 virtual int getRowCount() override { return 0; }
 virtual int getCellTall( int ) override { return 0; }
 virtual Panel* getCellRenderer( int, int, bool, bool, bool ) override { return NULL; }
 virtual Panel* startCellEditing( int, int ) override { return NULL; }
};

// Label::createPropertyPanel puts its FooDumb second in the "Label" folder.
static const int LABEL_FOLDER = 2;
static const int FOO_DUMB = 1;

static Panel* LabelPropertyEditor(Panel* properties)
{
 return properties->getChild( LABEL_FOLDER )->getChild( FOO_DUMB );
}

static std::string Line(EditPanel& panel, int line)
{
 char buffer[TEXT_BUFFER_SIZE];
 panel.getText( line, 0, buffer, TEXT_BUFFER_SIZE );
 return buffer;
}

static std::string Title(WizardPanel::WizardPage& page)
{
 char buffer[TEXT_BUFFER_SIZE];
 page.getTitle( buffer, TEXT_BUFFER_SIZE );
 return buffer;
}

#if VGUI_ABI_COMPAT_MODE != 1
template <typename T>
static void CheckButtonImage()
{
 Panel* root = new Panel( 0, 0, 100, 100 );
 T* button = new T( "button", 0, 0 );
 button->setParent( root );
 Check(button->_ownImage == button->_image, "button shows its own image");
 // Label does not own the image that replaces it.
 Image* other = new Image();
 button->setImage( other );
 T* copy = new T( *button );
 Check(copy->_ownImage != NULL && copy->_ownImage != button->_ownImage, "copy has its own image");
 Check(copy->_ownImage == copy->_image, "copy shows its own image");
 *copy = *button;
 Check(copy->_ownImage == copy->_image, "assigned copy keeps its own image");
 delete copy;
 delete root;
 delete other;
}
#endif

static void Run(const std::string& test)
{
 if (test == "wizard-page-texts")
 {
  WizardPanel::WizardPage* page = new WizardPanel::WizardPage();
  page->setTitle( "first title" );
  page->setBackButtonText( "back" );
  page->setNextButtonText( "next" );
  page->setFinishedButtonText( "finished" );
  page->setCancelButtonText( "cancel" );
  Check(std::string("first title") == Title(*page), "title replaced");
#if VGUI_ABI_COMPAT_MODE != 1
  delete page;
#endif
 }
 else if (test == "frame-cursors")
 {
  Frame* frame = new Frame( 0, 0, 100, 100 );
  Scheme* scheme = App::getInstance()->getScheme();
  Check(scheme->getCursor( Scheme::scu_sizens ) == frame->_topGrip->getCursor(), "top grip cursor");
  Check(scheme->getCursor( Scheme::scu_sizewe ) == frame->_rightGrip->getCursor(), "right grip cursor");
  Check(scheme->getCursor( Scheme::scu_sizenwse ) == frame->_topLeftGrip->getCursor(), "top left grip cursor");
  Check(scheme->getCursor( Scheme::scu_sizenesw ) == frame->_bottomLeftGrip->getCursor(), "bottom left grip cursor");
  Check(scheme->getCursor( Scheme::scu_sizeall ) == frame->_captionGrip->getCursor(), "caption grip cursor");
#if VGUI_ABI_COMPAT_MODE != 1
  delete frame;
#endif
 }
 else if (test == "shared-layouts")
 {
  Menu* menu = new Menu( 10, 10 );
  Menu* otherMenu = new Menu( 0, 0, 10, 10 );
  Check(menu->_layout != NULL && menu->_layout == otherMenu->_layout, "menus share a layout");
  ListPanel* list = new ListPanel( 0, 0, 100, 100 );
  ListPanel* otherList = new ListPanel( 0, 0, 100, 100 );
  Check(list->_vpanel->_layout != NULL && list->_vpanel->_layout == otherList->_vpanel->_layout,
   "list panels share a layout");
  TreeFolder* folder = new TreeFolder( "folder" );
  TreeFolder* otherFolder = new TreeFolder( "other folder" );
  Check(folder->_layout != NULL && folder->_layout == otherFolder->_layout, "tree folders share a layout");
  Label* label = new Label( "label" );
  Panel* properties = label->createPropertyPanel();
  Panel* otherProperties = label->createPropertyPanel();
  Check(LabelPropertyEditor(properties)->_layout != NULL
   && LabelPropertyEditor(properties)->_layout == LabelPropertyEditor(otherProperties)->_layout,
   "label property editors share a layout");
#if VGUI_ABI_COMPAT_MODE != 1
  delete menu;
  delete otherMenu;
  delete list;
  delete otherList;
  delete folder;
  delete otherFolder;
  delete label;
  delete properties;
  delete otherProperties;
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "toggle-copy")
 {
  ToggleButton* toggle = new ToggleButton( "toggle", 0, 0 );
  ToggleButton* copy = new ToggleButton( *toggle );
  Check(copy->_buttonController != NULL && copy->_buttonController != toggle->_buttonController,
   "copy has its own controller");
  // A plain Button controller would select the copy only while pressed.
  PanelInput::click( copy );
  Check(copy->isSelected(), "copy toggles");
  Check(!toggle->isSelected(), "source unchanged");
  delete copy;
  delete toggle;
 }
 else if (test == "text-panel")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TextPanel* panel = new TextPanel( "text", 0, 0, 10, 10 );
  panel->setParent( root );
  TextPanel* copy = new TextPanel( *panel );
  Check(copy->_textImage != panel->_textImage, "copy has its own text image");
  Check(std::string("text") == copy->_textImage->_text, "copy has the text");
  copy->setText( "a longer copy text" );
  *copy = *panel;
  Check(std::string("text") == copy->_textImage->_text, "assigned copy has the text");
  delete copy;
  delete root;
 }
 else if (test == "text-grid")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TextGrid* grid = new TextGrid( GRID_WIDE, GRID_TALL, 0, 0, 10, 10 );
  grid->setParent( root );
  grid->printf( "ab" );
  TextGrid* copy = new TextGrid( *grid );
  Check(copy->_grid != NULL && copy->_grid != grid->_grid, "copy has its own grid");
  Check('a' == copy->_grid[0], "copy has the cells");
  TextGrid* other = new TextGrid( GRID_TALL, GRID_WIDE, 0, 0, 10, 10 );
  *other = *grid;
  Check(other->_grid != grid->_grid && GRID_WIDE == other->_gridSize[0], "assigned grid has its own cells");
  delete other;
  delete copy;
  delete root;
 }
 else if (test == "edit-panel")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  EditPanel* panel = new EditPanel( 0, 0, 10, 10 );
  panel->setParent( root );
  // The stub font is 0 pixels tall, so the cursor cannot move between lines;
  // add the second line directly.
  panel->setChar( panel->getLine( 0 ), 0, 'a' );
  panel->_lineDarDar.addElement( new Dar<char>( LINE_CAPACITY ) );
  panel->setChar( panel->getLine( 1 ), 0, 'b' );
  EditPanel* copy = new EditPanel( *panel );
  Check(2 == copy->getLineCount(), "copy has the lines");
  Check(copy->getLine( 0 ) != panel->getLine( 0 ), "copy has its own lines");
  Check(std::string("a") == Line(*copy, 0) && std::string("b") == Line(*copy, 1), "copy has the text");
  copy->setChar( copy->getLine( 1 ), 0, 'c' );
  Check(std::string("b") == Line(*panel, 1), "source text unchanged");
  EditPanel* other = new EditPanel( 0, 0, 10, 10 );
  *other = *panel;
  Check(2 == other->getLineCount() && std::string("b") == Line(*other, 1), "assigned panel has the text");
  delete other;
  delete copy;
  delete root;
 }
 else if (test == "check-button")
 {
  CheckButtonImage<CheckButton>();
 }
 else if (test == "radio-button")
 {
  CheckButtonImage<RadioButton>();
 }
 else if (test == "wizard-page")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  WizardPanel::WizardPage* page = new WizardPanel::WizardPage();
  page->setParent( root );
  page->setTitle( "title" );
  WizardPanel::WizardPage* copy = new WizardPanel::WizardPage( *page );
  Check(copy->_title != page->_title, "copy has its own title");
  Check(std::string("title") == Title(*copy), "copy has the title");
  WizardPanel::WizardPage* other = new WizardPanel::WizardPage();
  *other = *page;
  Check(other->_title != page->_title && std::string("title") == Title(*other), "assigned page has the title");
  delete other;
  delete copy;
  delete root;
 }
 else if (test == "frame")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  Frame* frame = new Frame( 0, 0, 100, 100 );
  frame->setParent( root );
  frame->setTitle( "title" );
  Frame* copy = new Frame( *frame );
  Check(copy->_title != frame->_title && std::string("title") == copy->_title, "copy has its own title");
  Frame* other = new Frame( 0, 0, 100, 100 );
  *other = *frame;
  Check(other->_title != frame->_title && std::string("title") == other->_title, "assigned frame has the title");
  delete copy;
  delete root;
  delete other;
 }
 else if (test == "tab-panel")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TabPanel* tabs = new TabPanel( 0, 0, 100, 100 );
  tabs->setParent( root );
  tabs->addTab( "first" );
  tabs->addTab( "second" );
  TabPanel* copy = new TabPanel( *tabs );
  Check(copy->_buttonGroup != NULL && copy->_buttonGroup != tabs->_buttonGroup, "copy has its own button group");
  TabPanel* other = new TabPanel( 0, 0, 100, 100 );
  ButtonGroup* group = other->_buttonGroup;
  *other = *tabs;
  Check(group == other->_buttonGroup, "assigned panel keeps its button group");
  delete copy;
  delete root;
  delete other;
 }
 else if (test == "table-panel")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestTable* table = new TestTable();
  table->setParent( root );
  TestTable* copy = new TestTable( *table );
  Check(copy->_fakeInputPanel != NULL && copy->_fakeInputPanel != table->_fakeInputPanel,
   "copy has its own fake input panel");
  TestTable* other = new TestTable();
  Panel* fake = other->_fakeInputPanel;
  *other = *table;
  Check(fake == other->_fakeInputPanel, "assigned table keeps its fake input panel");
  delete copy;
  delete root;
  delete other;
 }
#endif
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
 // Static so that the scheme App never frees stays reachable for leak checks.
 static TestApp app;
 try
 {
  Check(argc == 2, "usage: test <scenario>");
  Run(argv[1]);
 }
 catch (const std::exception& error)
 {
  std::fprintf(stderr, "%s\n", error.what());
  return EXIT_FAILURE;
 }
 std::printf("PASS: %s\n", argv[1]);
 return EXIT_SUCCESS;
}
'''

SCENARIOS = ['wizard-page-texts', 'frame-cursors', 'shared-layouts']
NONCOMPAT_SCENARIOS = ['toggle-copy', 'text-panel', 'text-grid', 'edit-panel', 'check-button', 'radio-button',
                       'wizard-page', 'frame', 'tab-panel', 'table-panel']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-widget-') as folder:
        test_file = Path(folder) / 'test.cpp'
        strdup = function((VGUI / 'vgui.cpp').read_text(), 'char* vgui_strdup(')
        test_file.write_text(TESTS + '\nnamespace vgui\n{\n' + strdup + '\n}\n')
        sources = [test_file] + vgui1_sources(folder) + [VGUI / name for name in WIDGETS]
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            # VGUI_EXPORTS matches the vgui target, so VGUIAPI is not dllimport on Windows.
            executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI],
                                         defines=['VGUI_EXPORTS', f'VGUI_ABI_COMPAT_MODE={mode}'], debug=True,
                                         sanitizers=['address'])
            # Without destructors the compatible build frees no panel; only check
            # its memory errors.
            env = dict(os.environ)
            if mode == '1':
                env['ASAN_OPTIONS'] = 'detect_leaks=0:' + env.get('ASAN_OPTIONS', '')
            for scenario in scenarios:
                result = subprocess.run([str(executable), scenario], env=env)
                if result.returncode:
                    failed.append(f'{scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
