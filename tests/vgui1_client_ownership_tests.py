"""Compile the shared VGUI1 client controls and check how they own their panels (#308).

Run: python tests/vgui1_client_ownership_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

game_shared's CListBox, ScrollBar2, Slider2, CGrid and CCheckButton2 are
built with the VGUI1 sources and platform stubs from vgui1_panel_tests.py,
twice: with VGUI_ABI_COMPAT_MODE=1 and with VGUI_ABI_COMPAT_MODE=0, where a
Panel owns its border, its input signals and its children. They are compiled
against the internal vgui headers rather than the public ones the client DLLs
use, which checks their logic but not the client ABI. vgui_LoadTGA is stubbed.
The compatible build frees no panel, so its runs check memory errors only.

- forwarder: CInputSignalForwarder passes every event to its target.
- check-button-toggles: clicking a CCheckButton2's label toggles it through the
  forwarder its label owns.
- scroll-bar2-fires-once: a ScrollBar2 calls each of its IntChangeSignals once
  per change, and with VGUI_ABI_COMPAT_MODE=0 deletes them.
With VGUI_ABI_COMPAT_MODE=0 only:
- list-box-tree: deleting a panel that holds a CListBox frees the list box,
  its scrollbar (with the default slider it replaced) and the signal it gave
  the scrollbar, its items and their entries.
- check-button-tree: deleting a panel that holds a CCheckButton2 frees its
  member label and image panel and their forwarders once.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program
from vgui1_panel_tests import VGUI, vgui1_sources

ROOT = Path(__file__).resolve().parents[1]
GAME_SHARED = ROOT / 'GoldSrc-SDK/game_shared'
CONTROLS = ['vgui_listbox.cpp', 'vgui_scrollbar2.cpp', 'vgui_slider2.cpp', 'vgui_grid.cpp', 'vgui_checkbutton2.cpp']
VGUI_SOURCES = ['Bitmap.cpp', 'BitmapTGA.cpp', 'DataInputStream.cpp', 'ImagePanel.cpp']
# The client controls use the public headers' null and the MSVC _vsnprintf,
# which only needs a replacement where stdio.h does not declare it.
CONTROL_DEFINES = ['null=0'] + ([] if os.name == 'nt' else ['_vsnprintf=vsnprintf'])

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "VGUI_ActionSignal.h"
#include "VGUI_App.h"
#include "VGUI_BitmapTGA.h"
#include "VGUI_Border.h"
#include "VGUI_Button.h"
#include "VGUI_InputStream.h"
#include "VGUI_IntChangeSignal.h"
#include "VGUI_Label.h"
#include "VGUI_Panel.h"
#include "vgui_checkbutton2.h"
#include "vgui_compat_cleanup.h"
#include "vgui_defaultinputsignal.h"
#include "vgui_grid.h"
#include "vgui_listbox.h"
#include "vgui_scrollbar2.h"

using namespace vgui;

class TinyTGAStream : public InputStream
{
public:
 TinyTGAStream() : pos_( 0 )
 {
  static const uchar kTga[22] = {
   0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   1, 0, 1, 0, 32, 0,
   0x11, 0x22, 0x33, 0xff
  };
  std::memcpy( data_, kTga, sizeof( kTga ) );
 }
 virtual void seekStart( bool& success ) override { pos_ = 0; success = true; }
 virtual void seekRelative( int count, bool& success ) override
 {
  int next = pos_ + count;
  success = next >= 0 && next <= (int)sizeof( data_ );
  if (success) pos_ = next;
 }
 virtual void seekEnd( bool& success ) override { pos_ = (int)sizeof( data_ ); success = true; }
 virtual int getAvailable( bool& success ) override { success = true; return (int)sizeof( data_ ) - pos_; }
 virtual uchar readUChar( bool& success ) override
 {
  if (pos_ >= (int)sizeof( data_ )) { success = false; return 0; }
  success = true;
  return data_[pos_++];
 }
 virtual void readUChar( uchar* buf, int count, bool& success ) override
 {
  if (count < 0 || pos_ + count > (int)sizeof( data_ )) { success = false; return; }
  std::memcpy( buf, data_ + pos_, (size_t)count );
  pos_ += count;
  success = true;
 }
 virtual void close( bool& success ) override { success = true; }

private:
 uchar data_[22];
 int pos_;
};

static bool loadRealTga = false;

BitmapTGA* vgui_LoadTGA( char const* )
{
 if (!loadRealTga)
  return NULL;
 TinyTGAStream stream;
 return new BitmapTGA( &stream, false );
}

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}
 Panel* mouseCapture() const { return _mouseCapture; }
 Panel* wantedKeyFocus() const { return _wantedKeyFocus; }
 Panel* mouseArenaPanel() const { return _mouseArenaPanel; }
};

static const int ITEM_COUNT = 3;
static const int ENTRY_COUNT = 2;
static const int SCROLL_MAX = 100;
static const int SCROLL_VALUE = 50;

static int deletedSignals;
static int deletedActions;
static int deletedBorders;
static int deletedChildren;

class CountingAction : public ActionSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingAction() override { ++deletedActions; }
#else
 ~CountingAction() { ++deletedActions; }
#endif
 virtual void actionPerformed( Panel* ) override {}
};

class CountingBorder : public Border
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingBorder() override { ++deletedBorders; }
#else
 ~CountingBorder() { ++deletedBorders; }
#endif
 virtual void paint( Panel* ) override {}
};

class CountingChildPanel : public Panel
{
public:
 CountingChildPanel() : Panel( 0, 0, 4, 4 ) {}
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingChildPanel() override { ++deletedChildren; }
#else
 ~CountingChildPanel()
 {
  ++deletedChildren;
  VGuiCompatAccess::cleanupPanelStorage( this );
 }
#endif
};

class CountingIntChange : public IntChangeSignal
{
public:
 CountingIntChange() : calls( 0 ) {}
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingIntChange() override { ++deletedSignals; }
#else
 ~CountingIntChange() { ++deletedSignals; }
#endif
 virtual void intChanged( int, Panel* ) override { ++calls; }

 int calls;
};

class CountingSignal : public CDefaultInputSignal
{
public:
 CountingSignal() : pressed( 0 ), entered( 0 ), typed( 0 ) {}
 virtual void mousePressed( MouseCode, Panel* ) override { ++pressed; }
 virtual void cursorEntered( Panel* ) override { ++entered; }
 virtual void keyTyped( KeyCode, Panel* ) override { ++typed; }

 int pressed;
 int entered;
 int typed;
};

// Clicks a panel the way App does, through its protected internalMousePressed.
class PanelInput : public Panel
{
public:
 static void press( Panel* panel )
 {
  void ( Panel::*pressed )( MouseCode ) = &PanelInput::internalMousePressed;
  ( panel->*pressed )( MOUSE_LEFT );
 }
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static void Run(const std::string& test)
{
 if (test == "forwarder")
 {
  CountingSignal target;
  CInputSignalForwarder forwarder( &target );
  Panel panel( 0, 0, 10, 10 );
  forwarder.mousePressed( MOUSE_LEFT, &panel );
  forwarder.cursorEntered( &panel );
  forwarder.keyTyped( KEY_A, &panel );
  Check(1 == target.pressed && 1 == target.entered && 1 == target.typed, "events forwarded");
 }
 else if (test == "check-button-toggles")
 {
  CCheckButton2* button = new CCheckButton2();
  Check(!button->IsChecked(), "starts unchecked");
  PanelInput::press( &button->m_Label );
  Check(button->IsChecked(), "label click toggles");
  PanelInput::press( &button->m_CheckboxPanel );
  Check(!button->IsChecked(), "box click toggles");
  delete button;
 }
 else if (test == "scroll-bar2-fires-once")
 {
  loadRealTga = true;
  ScrollBar2* scrollBar = new ScrollBar2( 0, 0, 20, 100, true );
  scrollBar->setRange( 0, SCROLL_MAX );
  CountingIntChange* first = new CountingIntChange();
  CountingIntChange* second = new CountingIntChange();
  scrollBar->addIntChangeSignal( VGuiCompat_OwnIntChangeSignal( first ) );
  scrollBar->addIntChangeSignal( VGuiCompat_OwnIntChangeSignal( second ) );
  scrollBar->setValue( SCROLL_VALUE );
  Check(1 == first->calls && 1 == second->calls, "each signal called once");
  delete scrollBar;
  loadRealTga = false;
  Check(2 == deletedSignals, "scroll bar deleted its signals");
 }
#if VGUI_ABI_COMPAT_MODE == 1
 else if (test == "compat-button-focus-and-owned")
 {
  TestApp* app = static_cast<TestApp*>( App::getInstance() );
  Button* button = new Button( "Spec", 0, 0, 40, 20 );
  CountingChildPanel* child = new CountingChildPanel();
  child->setParent( button );
  button->addActionSignal( VGuiCompat_OwnActionSignal( new CountingAction() ) );
  button->setBorder( VGuiCompat_OwnBorder( new CountingBorder() ) );
  app->setMouseCapture( button );
  app->requestFocus( button );
  app->setMouseArena( button );
  Check(app->mouseCapture() == button && app->wantedKeyFocus() == button && app->mouseArenaPanel() == button, "app refs set");
  int childCount = 0;
  Panel** children = VGuiCompatAccess::takeChildren( button, childCount );
  Check(childCount == 1 && children[0] == child, "child detached");
  delete static_cast<CountingChildPanel*>( children[0] );
  delete[] children;
  VGuiGameShared_CleanupButtonMembers( button );
  delete button;
  Check(app->mouseCapture() == NULL, "capture cleared");
  Check(app->getFocus() == NULL && app->wantedKeyFocus() == NULL, "focus cleared");
  Check(app->mouseArenaPanel() == NULL, "mouse arena cleared");
  app->externalTick();
  Check(deletedActions == 1 && deletedBorders == 1 && deletedChildren == 1, "owned members freed");
 }
#endif
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "list-box-tree")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  CListBox* list = new CListBox();
  list->setParent( root );
  for (int item = 0; item < ITEM_COUNT; ++item)
  {
   CGrid* grid = new CGrid();
   grid->SetDimensions( ENTRY_COUNT, 1 );
   for (int entry = 0; entry < ENTRY_COUNT; ++entry)
    grid->SetEntry( entry, 0, new Label( "entry" ) );
   list->AddItem( grid );
  }
  Check(ITEM_COUNT == list->GetNumItems(), "items added");
  delete root;
 }
 else if (test == "check-button-tree")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  CCheckButton2* button = new CCheckButton2();
  button->setParent( root );
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

SCENARIOS = ['forwarder', 'check-button-toggles', 'scroll-bar2-fires-once']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS + ['compat-button-focus-and-owned'], '0': SCENARIOS + ['list-box-tree', 'check-button-tree']}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-client-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        sources = ([test_file] + vgui1_sources(folder) + [VGUI / name for name in VGUI_SOURCES]
                   + [GAME_SHARED / name for name in CONTROLS])
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            # VGUI_STATIC keeps VGUIAPI from exporting on Windows: MSVC would emit the whole
            # vtable of an exported class in every file, also for sources the test leaves out.
            executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI, GAME_SHARED],
                                         defines=['VGUI_STATIC', f'VGUI_ABI_COMPAT_MODE={mode}'] + CONTROL_DEFINES,
                                         debug=True, sanitizers=['address'])
            # The forwarder fixture still embeds a raw compat Panel on the stack.
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
