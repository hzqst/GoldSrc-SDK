"""Compile the production VGUI1 Panel and check how it owns its tree (#308).

Run: python tests/vgui1_panel_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

Panel, App, Label, Button, ToggleButton, TextEntry and the classes they need
are built twice: with VGUI_ABI_COMPAT_MODE=1 (the original vgui.dll layout, no
destructors) and with VGUI_ABI_COMPAT_MODE=0 (a Panel owns its border, its
input signals and its children). The platform half of App, the platform Font
and Cursor constructors and Font_Reset are stubbed. TextImage.cpp is compiled
from its source text with the scheme font lookup replaced by NULL, and
vgui_strcpy is extracted from vgui.cpp, whose remaining code only builds for
32-bit targets.

- remove-all-detaches: removeAllChildren clears each child's parent and takes
  each child's whole tree off the surface, without calling a removeChild
  override; a detached tree takes the surface of the panel it is added to
  next (#223).
- remove-child-detaches: removeChild takes the child's whole tree off the
  surface and leaves the other children on it (#223).
- repaint-all-unattached: repaintAll on a panel on no surface marks the top of
  its own tree for repainting, also after the tree was removed from its
  parent (#226).
With VGUI_ABI_COMPAT_MODE=0 only:
- delete-tree: deleting a panel deletes its border, input signals and the
  whole child tree.
- delete-attached-child: deleting a child removes it from its parent, which
  can then be deleted.
- member-child: a child that is a member of its parent removes itself before
  the parent's ~Panel runs.
- removed-signal-survives: removeInputSignal and removeChild hand ownership
  back to the caller.
- app-pointers-cleared: deleting a panel clears App's focus and mouse pointers
  to it.
- text-entry: a TextEntry, its own input signal, is deleted once.
- button-controller: a Button deletes its controller, and a ToggleButton
  deletes the default controller it replaces.
- copy-no-ownership: a copied Panel, Label or Button shares no children,
  border, signals or controller with the original.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'
FONT_LOOKUP = 'return App::getInstance()->getScheme()->getFont( _schemeFont );'
SOURCES = ['Panel.cpp', 'App.cpp', 'Label.cpp', 'Image.cpp', 'Color.cpp', 'Button.cpp', 'ToggleButton.cpp',
           'TextEntry.cpp', 'Border.cpp', 'LineBorder.cpp', 'Scheme.cpp', 'Font.cpp', 'Cursor.cpp', 'FlowLayout.cpp',
           'Layout.cpp', 'TreeFolder.cpp']


# The platform half of App (win32/App.cpp), the platform Font and Cursor code
# and Font_Reset, for tests that build the VGUI1 sources without a platform.
PLATFORM_STUBS = r'''
#include "VGUI_App.h"
#include "VGUI_Cursor.h"
#include "VGUI_Font.h"

using namespace vgui;

#if defined( _WIN32 )
typedef bool RegistryResult;
#else
typedef int RegistryResult;
#endif

void App::platTick() {}
void App::setCursorPos( int, int ) {}
void App::getCursorPos( int& x, int& y ) { x = 0; y = 0; }
void App::internalSetMouseArena( int, int, int, int, bool ) {}
long App::getTimeMillis() { return 0; }
void App::setClipboardText( const char*, int ) {}
int App::getClipboardTextCount() { return 0; }
int App::getClipboardText( int, char*, int ) { return 0; }
RegistryResult App::setRegistryString( const char*, const char* ) { return false; }
RegistryResult App::getRegistryString( const char*, char*, int ) { return false; }
RegistryResult App::setRegistryInteger( const char*, int ) { return false; }
RegistryResult App::getRegistryInteger( const char*, int& ) { return false; }
App* App::getInstance() { return _instance; }
// A default cursor without a bitmap, which ~Cursor can free.
Cursor::Cursor( DefaultCursor dc ) { privateInit( NULL, 0, 0 ); _dc = dc; }
Font::Font( const char*, void*, int, int, int, float, int, bool, bool, bool, bool ) {}
void Font::init( const char*, void*, int, int, int, float, int, bool, bool, bool, bool ) {}
void Font::getCharRGBA( int, int, int, int, int, uchar* ) {}
void Font::getCharABCwide( int, int& a, int& b, int& c ) { a = b = c = 0; }
int Font::getTall() { return 0; }
#if !defined( _WIN32 )
int Font::getWide() { return 0; }
#endif
namespace vgui
{
void Font_Reset() {}
}
'''


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "VGUI_App.h"
#include "VGUI_Button.h"
#include "VGUI_Cursor.h"
#include "VGUI_Font.h"
#include "VGUI_InputSignal.h"
#include "VGUI_Label.h"
#include "VGUI_LineBorder.h"
#include "VGUI_Panel.h"
#include "VGUI_TextEntry.h"
#include "VGUI_ToggleButton.h"

using namespace vgui;

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}

 void pointAllAt( Panel* panel )
 {
  _keyFocus = panel;
  _oldMouseFocus = panel;
  _mouseFocus = panel;
  _mouseCapture = panel;
  _wantedKeyFocus = panel;
  _mouseArenaPanel = panel;
 }

 bool allClear()
 {
  return _keyFocus == NULL && _oldMouseFocus == NULL && _mouseFocus == NULL && _mouseCapture == NULL
   && _wantedKeyFocus == NULL && _mouseArenaPanel == NULL;
 }
};

// Static so that the scheme App never frees stays reachable for leak checks.
static TestApp* app;

static int deletedSignals;

class CountingSignal : public InputSignal
{
public:
#if VGUI_ABI_COMPAT_MODE != 1
 virtual ~CountingSignal() override { ++deletedSignals; }
#endif
 virtual void cursorMoved( int, int, Panel* ) override {}
 virtual void cursorEntered( Panel* ) override {}
 virtual void cursorExited( Panel* ) override {}
 virtual void mousePressed( MouseCode, Panel* ) override {}
 virtual void mouseDoublePressed( MouseCode, Panel* ) override {}
 virtual void mouseReleased( MouseCode, Panel* ) override {}
 virtual void mouseWheeled( int, Panel* ) override {}
 virtual void keyPressed( KeyCode, Panel* ) override {}
 virtual void keyTyped( KeyCode, Panel* ) override {}
 virtual void keyReleased( KeyCode, Panel* ) override {}
 virtual void keyFocusTicked( Panel* ) override {}
};

class MemberOwner : public Panel
{
public:
 MemberOwner() { _member.setParent( this ); }

 Panel _member;
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

// Puts a panel's tree on a surface through the protected setSurfaceBaseTraverse,
// and reads and clears the protected _needsRepaint.
class PanelSurface : public Panel
{
public:
 static void set( Panel* panel, SurfaceBase* surface )
 {
  void ( Panel::*traverse )( SurfaceBase* ) = &PanelSurface::setSurfaceBaseTraverse;
  ( panel->*traverse )( surface );
 }

 static bool& needsRepaint( Panel* panel )
 {
  bool Panel::*flag = &PanelSurface::_needsRepaint;
  return panel->*flag;
 }
};

// Counts the calls to its removeChild override.
class RemoveCounter : public Panel
{
public:
 RemoveCounter() : removes( 0 ) {}
 virtual void removeChild( Panel* child ) override
 {
  ++removes;
  Panel::removeChild( child );
 }

 int removes;
};

// Stand-ins for two surfaces; the scenarios only compare these pointers.
static char surfaceStorage[2];
static SurfaceBase* const SURFACE_A = reinterpret_cast<SurfaceBase*>( &surfaceStorage[0] );
static SurfaceBase* const SURFACE_B = reinterpret_cast<SurfaceBase*>( &surfaceStorage[1] );

static void Run(const std::string& test)
{
 if (test == "remove-all-detaches")
 {
  Panel parent;
  parent.removeAllChildren();
  Check(0 == parent.getChildCount(), "nothing to remove");
  PanelSurface::set( &parent, SURFACE_A );
  // Declared before the children it takes, so that they are destroyed first.
  Panel other;
  PanelSurface::set( &other, SURFACE_B );
  Panel child;
  Panel grandchild;
  Panel sibling;
  child.setParent( &parent );
  grandchild.setParent( &child );
  sibling.setParent( &parent );
  Check(SURFACE_A == grandchild.getSurfaceBase(), "tree on the surface");
  parent.removeAllChildren();
  Check(0 == parent.getChildCount(), "children removed");
  Check(NULL == child.getParent() && NULL == sibling.getParent(), "children detached");
  Check(NULL == child.getSurfaceBase() && NULL == sibling.getSurfaceBase(), "children off the surface");
  Check(&child == grandchild.getParent(), "grandchild kept");
  Check(NULL == grandchild.getSurfaceBase(), "grandchild off the surface");
  Check(SURFACE_A == parent.getSurfaceBase(), "parent kept its surface");
  child.setParent( &other );
  Check(SURFACE_B == child.getSurfaceBase() && SURFACE_B == grandchild.getSurfaceBase(), "tree on the new surface");
  RemoveCounter counter;
  Panel counted;
  counted.setParent( &counter );
  counter.removeAllChildren();
  Check(0 == counter.removes && NULL == counted.getParent(), "removeChild override not called");
 }
 else if (test == "repaint-all-unattached")
 {
  Panel lone;
  lone.repaintAll();
  Check(PanelSurface::needsRepaint( &lone ), "unattached panel marked");
  Panel root;
  PanelSurface::set( &root, SURFACE_A );
  Panel child;
  Panel grandchild;
  child.setParent( &root );
  grandchild.setParent( &child );
  root.removeChild( &child );
  PanelSurface::needsRepaint( &root ) = false;
  PanelSurface::needsRepaint( &child ) = false;
  PanelSurface::needsRepaint( &grandchild ) = false;
  grandchild.repaintAll();
  Check(PanelSurface::needsRepaint( &child ), "top of the detached tree marked");
  Check(!PanelSurface::needsRepaint( &root ), "former parent left alone");
 }
 else if (test == "remove-child-detaches")
 {
  Panel parent;
  PanelSurface::set( &parent, SURFACE_A );
  Panel child;
  Panel grandchild;
  Panel sibling;
  child.setParent( &parent );
  grandchild.setParent( &child );
  sibling.setParent( &parent );
  parent.removeChild( &child );
  Check(1 == parent.getChildCount() && &sibling == parent.getChild( 0 ), "sibling kept");
  Check(NULL == child.getParent(), "child detached");
  Check(NULL == child.getSurfaceBase() && NULL == grandchild.getSurfaceBase(), "child tree off the surface");
  Check(&child == grandchild.getParent(), "grandchild kept");
  Check(SURFACE_A == sibling.getSurfaceBase(), "sibling on the surface");
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "delete-tree")
 {
  Panel* root = new Panel( 0, 0, 10, 10 );
  Panel* child = new Panel( 0, 0, 10, 10 );
  child->setParent( root );
  Label* grandChild = new Label( "grand child" );
  grandChild->setParent( child );
  root->setBorder( new LineBorder() );
  child->addInputSignal( new CountingSignal() );
  root->addInputSignal( new CountingSignal() );
  delete root;
  Check(2 == deletedSignals, "input signals deleted");
 }
 else if (test == "delete-attached-child")
 {
  Panel* root = new Panel( 0, 0, 10, 10 );
  Panel* child = new Panel( 0, 0, 10, 10 );
  child->setParent( root );
  Panel* sibling = new Panel( 0, 0, 10, 10 );
  sibling->setParent( root );
  delete child;
  Check(1 == root->getChildCount(), "child removed from parent");
  Check(sibling == root->getChild( 0 ), "sibling kept");
  delete root;
 }
 else if (test == "member-child")
 {
  Panel* root = new Panel( 0, 0, 10, 10 );
  MemberOwner* owner = new MemberOwner();
  owner->setParent( root );
  delete root;
 }
 else if (test == "removed-signal-survives")
 {
  Panel* root = new Panel( 0, 0, 10, 10 );
  Panel* child = new Panel( 0, 0, 10, 10 );
  CountingSignal* signal = new CountingSignal();
  child->setParent( root );
  root->addInputSignal( signal );
  root->removeInputSignal( signal );
  root->removeChild( child );
  delete root;
  Check(0 == deletedSignals, "removed signal kept");
  Check(NULL == child->getParent(), "removed child detached");
  delete signal;
  delete child;
 }
 else if (test == "app-pointers-cleared")
 {
  Panel* panel = new Panel( 0, 0, 10, 10 );
  app->pointAllAt( panel );
  delete panel;
  Check(app->allClear(), "App pointers cleared");
 }
 else if (test == "text-entry")
 {
  Panel* root = new Panel( 0, 0, 10, 10 );
  TextEntry* entry = new TextEntry( "text", 0, 0, 10, 10 );
  entry->setParent( root );
  delete root;
 }
 else if (test == "button-controller")
 {
  Panel* root = new Panel( 0, 0, 10, 10 );
  Button* button = new Button( "button", 0, 0 );
  button->setParent( root );
  ToggleButton* toggle = new ToggleButton( "toggle", 0, 0 );
  toggle->setParent( root );
  delete root;
 }
 else if (test == "copy-no-ownership")
 {
  Panel* source = new Panel( 1, 2, 30, 40 );
  Panel* child = new Panel( 0, 0, 10, 10 );
  child->setParent( source );
  source->setBorder( new LineBorder() );
  source->addInputSignal( new CountingSignal() );
  Panel* copy = new Panel( *source );
  int x, y, wide, tall;
  copy->getBounds( x, y, wide, tall );
  Check(1 == x && 2 == y && 30 == wide && 40 == tall, "copy has the bounds");
  Check(0 == copy->getChildCount(), "copy has no children");
  Check(NULL == copy->getParent(), "copy has no parent");
  delete copy;
  Check(0 == deletedSignals, "copy deleted no signal");
  Check(1 == source->getChildCount(), "source keeps its child");
  delete source;
  Check(1 == deletedSignals, "source deleted its signal");

  Label label( "label" );
  Label labelCopy( label );
  labelCopy = label;
  Button button( "button", 0, 0 );
  Button buttonCopy( button );
  buttonCopy = button;
 }
#endif
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
 static TestApp testApp;
 app = &testApp;
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

NONCOMPAT_SCENARIOS = ['delete-tree', 'delete-attached-child', 'member-child', 'removed-signal-survives',
                       'app-pointers-cleared', 'text-entry', 'button-controller', 'copy-no-ownership']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
SCENARIOS = ['remove-all-detaches', 'remove-child-detaches', 'repaint-all-unattached']
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}


def vgui1_sources(folder):
    """Write the platform stubs and the patched TextImage.cpp to folder and
    return them with SOURCES, ready for compile_program."""
    text_image = (VGUI / 'TextImage.cpp').read_text()
    if FONT_LOOKUP not in text_image:
        raise SystemExit('TextImage::getFont no longer matches: ' + FONT_LOOKUP)
    text_image = text_image.replace(FONT_LOOKUP, 'return NULL;')
    strcpy = function((VGUI / 'vgui.cpp').read_text(), 'void vgui_strcpy(')

    stubs_file = Path(folder) / 'vgui1_stubs.cpp'
    stubs_file.write_text(PLATFORM_STUBS + '\nnamespace vgui\n{\n' + strcpy + '\n}\n')
    text_image_file = Path(folder) / 'TextImage.cpp'
    text_image_file.write_text(text_image)
    return [stubs_file, text_image_file] + [VGUI / name for name in SOURCES]


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-panel-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        sources = [test_file] + vgui1_sources(folder)
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            # VGUI_EXPORTS matches the vgui target, so VGUIAPI is not dllimport on Windows.
            executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI],
                                         defines=['VGUI_EXPORTS', f'VGUI_ABI_COMPAT_MODE={mode}'], debug=True,
                                         sanitizers=['address'])
            for scenario in scenarios:
                result = subprocess.run([str(executable), scenario])
                if result.returncode:
                    failed.append(f'{scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
