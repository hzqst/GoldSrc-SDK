"""Compile the VGUI1 signal holders and check how they own their signals (#308).

Run: python tests/vgui1_signal_ownership_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

The widgets from vgui1_widget_ownership_tests.py, ScrollPanel, MessageBox and
TaskBar are built with the VGUI1 sources and platform stubs from
vgui1_panel_tests.py, twice: with VGUI_ABI_COMPAT_MODE=1 (the original vgui.dll
layout, nothing owns a signal) and with VGUI_ABI_COMPAT_MODE=0, where Button
and TextEntry own their ActionSignals, Slider and ScrollBar their
IntChangeSignals and HeaderPanel its ChangeSignals. The compatible build frees
no panel, so its runs check memory errors only. TaskBar never frees the
handlers it gives its frames, so task-bar-click checks memory errors only.

- scroll-bar-fires-once: a ScrollBar calls each of its IntChangeSignals once
  per change.
- tab-select: clicking a TabPanel tab selects it through the action signal its
  button owns.
- label-editor: the Label property editor sets the label's text from its
  TextEntry's action.
- task-bar-click: clicking a TaskBar button shows its frame, and with
  VGUI_ABI_COMPAT_MODE=0 deleting the task bar removes its clock from the App's
  tick signals, and the frame can still signal closing and focus changes to
  the handler after the task bar and its buttons are deleted.
With VGUI_ABI_COMPAT_MODE=0 only:
- button-signals: deleting a Button deletes its action signals; a copy has
  none.
- holder-signals: deleting a TextEntry, Slider, ScrollBar or HeaderPanel
  deletes its signals; a copied or assigned Slider has none.
- internal-handlers: deleting a Frame, MessageBox, WizardPanel, ScrollPanel,
  ListPanel or a TablePanel and its HeaderPanel frees the handlers they add.
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
from vgui1_widget_ownership_tests import WIDGETS

HOLDERS = ['ScrollPanel.cpp', 'MessageBox.cpp', 'TaskBar.cpp', 'LoweredBorder.cpp']

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

// The test reads the panels and texts the widgets keep.
#define private public
#define protected public
#include "VGUI_ActionSignal.h"
#include "VGUI_App.h"
#include "VGUI_Button.h"
#include "VGUI_ChangeSignal.h"
#include "VGUI_FocusChangeSignal.h"
#include "VGUI_Frame.h"
#include "VGUI_HeaderPanel.h"
#include "VGUI_IntChangeSignal.h"
#include "VGUI_Label.h"
#include "VGUI_ListPanel.h"
#include "VGUI_MessageBox.h"
#include "VGUI_ScrollBar.h"
#include "VGUI_ScrollPanel.h"
#include "VGUI_Slider.h"
#include "VGUI_TabPanel.h"
#include "VGUI_TablePanel.h"
#include "VGUI_TaskBar.h"
#include "VGUI_TextEntry.h"
#include "VGUI_TextImage.h"
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

static const int SCROLL_MAX = 100;
static const int SCROLL_VALUE = 50;
static const int TEXT_BUFFER_SIZE = 64;

static int deletedSignals;

class CountingAction : public ActionSignal
{
public:
 CountingAction() : calls( 0 ) {}
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingAction() override { ++deletedSignals; }
#endif
 virtual void actionPerformed( Panel* ) override { ++calls; }

 int calls;
};

class CountingIntChange : public IntChangeSignal
{
public:
 CountingIntChange() : calls( 0 ) {}
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingIntChange() override { ++deletedSignals; }
#endif
 virtual void intChanged( int, Panel* ) override { ++calls; }

 int calls;
};

class CountingChange : public ChangeSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingChange() override { ++deletedSignals; }
#endif
 virtual void valueChanged( Panel* ) override {}
};

class TestTable : public TablePanel
{
public:
 TestTable() : TablePanel( 0, 0, 100, 100, 2 ) {}
 virtual int getRowCount() override { return 0; }
 virtual int getCellTall( int ) override { return 0; }
 virtual Panel* getCellRenderer( int, int, bool, bool, bool ) override { return NULL; }
 virtual Panel* startCellEditing( int, int ) override { return NULL; }
};

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

// Label::createPropertyPanel puts its FooDumb second in the "Label" folder, and
// FooDumb's TextEntry second in it.
static const int LABEL_FOLDER = 2;
static const int FOO_DUMB = 1;
static const int FOO_DUMB_ENTRY = 1;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static void Run(const std::string& test)
{
 if (test == "scroll-bar-fires-once")
 {
  ScrollBar* scrollBar = new ScrollBar( 0, 0, 20, 100, true );
  scrollBar->setRange( 0, SCROLL_MAX );
  CountingIntChange* first = new CountingIntChange();
  CountingIntChange* second = new CountingIntChange();
  scrollBar->addIntChangeSignal( first );
  scrollBar->addIntChangeSignal( second );
  scrollBar->setValue( SCROLL_VALUE );
  Check(1 == first->calls && 1 == second->calls, "each signal called once");
#if VGUI_ABI_COMPAT_MODE != 1
  delete scrollBar;
  Check(2 == deletedSignals, "scroll bar deleted its signals");
#endif
 }
 else if (test == "tab-select")
 {
  TabPanel* tabs = new TabPanel( 0, 0, 100, 100 );
  tabs->addTab( "first" );
  tabs->addTab( "second" );
  // addTab inserts each tab first.
  Panel* second = tabs->_tabArea->getChild( 0 );
  Check(second != tabs->_selectedTab, "first tab selected");
  PanelInput::click( second );
  Check(second == tabs->_selectedTab, "clicked tab selected");
#if VGUI_ABI_COMPAT_MODE != 1
  delete tabs;
#endif
 }
 else if (test == "label-editor")
 {
  Label* label = new Label( "label" );
  Panel* properties = label->createPropertyPanel();
  Panel* editor = properties->getChild( LABEL_FOLDER )->getChild( FOO_DUMB );
  TextEntry* entry = dynamic_cast<TextEntry*>( editor->getChild( FOO_DUMB_ENTRY ) );
  Check(entry != NULL, "editor text entry");
  entry->setText( "edited", (int)strlen( "edited" ) );
  entry->fireActionSignal();
  Check(std::string("edited") == label->_textImage->_text, "label text set");
#if VGUI_ABI_COMPAT_MODE != 1
  delete properties;
  delete label;
#endif
 }
 else if (test == "task-bar-click")
 {
  App* app = App::getInstance();
  const int tickSignals = app->_tickSignalDar.getCount();
  TaskBar* taskBar = new TaskBar( 0, 0, 200, 26 );
  Frame* frame = new Frame( 0, 0, 100, 100 );
  frame->setVisible( false );
  taskBar->addFrame( frame );
  PanelInput::click( taskBar->_taskButtonDar[0] );
  Check(frame->isVisible(), "frame shown");
#if VGUI_ABI_COMPAT_MODE != 1
  // The frame keeps the handler after the task bar and its buttons are gone.
  delete taskBar;
  Check(tickSignals == app->_tickSignalDar.getCount(), "clock no longer ticked");
  frame->fireClosingSignal();
  for ( int i = 0; i < frame->_focusChangeSignalDar.getCount(); ++i )
   frame->_focusChangeSignalDar[i]->focusChanged( true, frame );
  delete frame;
#else
  (void)tickSignals;
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "button-signals")
 {
  Button* button = new Button( "button", 0, 0 );
  CountingAction* signal = new CountingAction();
  button->addActionSignal( signal );
  button->addActionSignal( new CountingAction() );
  Button* copy = new Button( *button );
  copy->fireActionSignal();
  Check(0 == signal->calls, "copy has no signals");
  *copy = *button;
  copy->fireActionSignal();
  Check(0 == signal->calls, "assigned copy has no signals");
  delete copy;
  Check(0 == deletedSignals, "copy deleted no signal");
  button->fireActionSignal();
  Check(1 == signal->calls, "button fires its signal");
  delete button;
  Check(2 == deletedSignals, "button deleted its signals");
 }
 else if (test == "holder-signals")
 {
  TextEntry* entry = new TextEntry( "text", 0, 0, 10, 10 );
  entry->addActionSignal( new CountingAction() );
  delete entry;
  Check(1 == deletedSignals, "text entry deleted its signal");
  Slider* slider = new Slider( 0, 0, 10, 100, true );
  slider->addIntChangeSignal( new CountingIntChange() );
  // Slider's copy constructor and operator= are the implicit ones.
  Slider* copy = new Slider( *slider );
  Check(0 == copy->_intChangeSignalDar.getCount(), "copy has no signals");
  Slider* assigned = new Slider( 0, 0, 10, 100, true );
  *assigned = *slider;
  Check(0 == assigned->_intChangeSignalDar.getCount(), "assigned slider has no signals");
  delete assigned;
  delete copy;
  Check(1 == deletedSignals, "copies deleted no signal");
  delete slider;
  Check(2 == deletedSignals, "slider deleted its signal");
  ScrollBar* scrollBar = new ScrollBar( 0, 0, 20, 100, true );
  scrollBar->addIntChangeSignal( new CountingIntChange() );
  delete scrollBar;
  Check(3 == deletedSignals, "scroll bar deleted its signal");
  HeaderPanel* header = new HeaderPanel( 0, 0, 100, 20 );
  header->addChangeSignal( new CountingChange() );
  delete header;
  Check(4 == deletedSignals, "header panel deleted its signal");
 }
 else if (test == "internal-handlers")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  ( new Frame( 0, 0, 100, 100 ) )->setParent( root );
  ( new MessageBox( "title", "text", 0, 0 ) )->setParent( root );
  ( new WizardPanel( 0, 0, 100, 100 ) )->setParent( root );
  ( new ScrollPanel( 0, 0, 100, 100 ) )->setParent( root );
  ( new ListPanel( 0, 0, 100, 100 ) )->setParent( root );
  TestTable* table = new TestTable();
  table->setParent( root );
  HeaderPanel* header = new HeaderPanel( 0, 0, 100, 20 );
  header->setParent( root );
  table->setHeaderPanel( header );
  delete root;
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

SCENARIOS = ['scroll-bar-fires-once', 'tab-select', 'label-editor', 'task-bar-click']
NONCOMPAT_SCENARIOS = ['button-signals', 'holder-signals', 'internal-handlers']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}
# TaskBar never frees the handlers it gives its frames.
LEAKING_SCENARIOS = {'task-bar-click'}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-signal-') as folder:
        test_file = Path(folder) / 'test.cpp'
        strdup = function((VGUI / 'vgui.cpp').read_text(), 'char* vgui_strdup(')
        test_file.write_text(TESTS + '\nnamespace vgui\n{\n' + strdup + '\n}\n')
        sources = ([test_file] + vgui1_sources(folder) + [VGUI / name for name in WIDGETS]
                   + [VGUI / name for name in HOLDERS])
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            # VGUI_EXPORTS matches the vgui target, so VGUIAPI is not dllimport on Windows.
            executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI],
                                         defines=['VGUI_EXPORTS', f'VGUI_ABI_COMPAT_MODE={mode}'], debug=True,
                                         sanitizers=['address'])
            for scenario in scenarios:
                # Without destructors the compatible build frees no panel; only
                # check its memory errors.
                env = dict(os.environ)
                if mode == '1' or scenario in LEAKING_SCENARIOS:
                    env['ASAN_OPTIONS'] = 'detect_leaks=0:' + env.get('ASAN_OPTIONS', '')
                result = subprocess.run([str(executable), scenario], env=env)
                if result.returncode:
                    failed.append(f'{scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
