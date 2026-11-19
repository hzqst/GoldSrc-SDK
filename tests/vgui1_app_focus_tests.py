"""Compile the VGUI1 App and SurfaceBase and check App's panel pointers (#219).

Run: python tests/vgui1_app_focus_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

App, SurfaceBase and the Panel sources from vgui1_panel_tests.py are built with
its platform stubs, whose internalSetMouseArena is replaced by one that counts
releases, and a TestSurface that counts mouse captures. They are built twice:
with VGUI_ABI_COMPAT_MODE=1 (the original vgui.dll layout, no destructors,
whose runs check memory errors only) and with VGUI_ABI_COMPAT_MODE=0.

- surface-deleted: deleting a surface makes App forget its key focus, wanted
  key focus, mouse capture and arena panel, and releases the cursor clip, so a
  tick with another surface left does not use the deleted one.
- capture-detached: releasing the capture of a panel whose ancestor left the
  surface releases it on the surface that captured; a panel on no surface gets
  the capture without a native one.
- capture-switch: capturing on another surface releases the first one.
- repaint-all-attached: repaintAll on a panel on a surface still invalidates
  the surface's top panel (#226).
- root-deleted-first: deleting a surface after its root, the way
  VGuiWrap_Shutdown does (with VGUI_ABI_COMPAT_MODE=1 the root is only freed),
  does not touch the root; otherwise the surface loses its root and App stops
  ticking it (#224).
- surface-copy-detached: a copied surface has no root or emulated cursor, App
  does not tick it, and deleting it keeps App's key focus and arena; the
  original keeps its root and emulated cursor (#437).
- surface-assign-keeps-root: an assigned surface keeps its own root and
  emulated cursor (#437).
With VGUI_ABI_COMPAT_MODE=0 only:
- captured-panel-deleted: deleting the capture and arena panel releases the
  native capture and the cursor clip.
- focus-deleted-in-callback: a panel that deletes itself when it loses or
  gets the key focus is not used afterwards.
- surface-deleted-first: a root that outlives its surface has its tree taken
  off the surface and loses the surface's emulated cursor (#224).
- moved-root: a surface's root moved onto another surface stays on it when
  the first surface is deleted, and a deleted moved root is forgotten by the
  first surface (#224).
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

ARENA_STUB = 'void App::internalSetMouseArena( int, int, int, int, bool ) {}'
COUNTING_ARENA = '''namespace vgui { extern int arenaReleases; }
void App::internalSetMouseArena( int, int, int, int, bool enabled )
{
 if ( !enabled )
  ++vgui::arenaReleases;
}'''

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "VGUI_App.h"
#include "VGUI_Panel.h"
#include "VGUI_SurfaceBase.h"

using namespace vgui;

namespace vgui
{
int arenaReleases;
}

class TestApp : public App
{
public:
 TestApp() : App( true ) {}
 virtual void main( int, char** ) override {}

 bool allClear()
 {
  return _keyFocus == NULL && _oldMouseFocus == NULL && _mouseFocus == NULL && _mouseCapture == NULL
   && _wantedKeyFocus == NULL && _mouseArenaPanel == NULL;
 }

 Panel* keyFocus() { return _keyFocus; }
 Panel* mouseCapture() { return _mouseCapture; }
 bool ticks( SurfaceBase* surfaceBase ) { return _surfaceBaseDar.hasElement( surfaceBase ); }
};

static TestApp* app;

// A surface with focus that counts its native mouse captures.
class TestSurface : public SurfaceBase
{
public:
 TestSurface( Panel* embeddedPanel ) : SurfaceBase( embeddedPanel ), captures( 0 ), releases( 0 ), invalidated( NULL ) {}
 ~TestSurface() {}

 virtual void setTitle( const char* ) override {}
 virtual bool setFullscreenMode( int, int, int ) override { return false; }
 virtual void setWindowedMode() override {}
 virtual void setAsTopMost( bool ) override {}
 virtual void createPopup( Panel* ) override {}
 virtual bool hasFocus() override { return true; }
 virtual bool isWithin( int, int ) override { return false; }
 virtual int createNewTextureID() override { return 0; }
 virtual void GetMousePos( int& x, int& y ) override { x = 0; y = 0; }
 virtual void drawSetColor( int, int, int, int ) override {}
 virtual void drawFilledRect( int, int, int, int ) override {}
 virtual void drawOutlinedRect( int, int, int, int ) override {}
 virtual void drawSetTextFont( Font* ) override {}
 virtual void drawSetTextColor( int, int, int, int ) override {}
 virtual void drawSetTextPos( int, int ) override {}
 virtual void drawPrintText( const char*, int ) override {}
 virtual void drawSetTextureRGBA( int, const char*, int, int ) override {}
 virtual void drawSetTexture( int ) override {}
 virtual void drawTexturedRect( int, int, int, int ) override {}
 virtual void invalidate( Panel* panel ) override { invalidated = panel; }
 virtual void enableMouseCapture( bool state ) override { ++( state ? captures : releases ); }
 virtual void setCursor( Cursor* ) override {}
 virtual void swapBuffers() override {}
 virtual void pushMakeCurrent( Panel*, bool ) override {}
 virtual void popMakeCurrent( Panel* ) override {}
 virtual void applyChanges() override {}

 int captures;
 int releases;
 Panel* invalidated;
};

// Deletes itself after it loses (or gets) the key focus.
class SelfDeletingPanel : public Panel
{
public:
 SelfDeletingPanel( bool onLost ) : Panel( 0, 0, 10, 10 ), _onLost( onLost ) {}

protected:
 virtual void internalFocusChanged( bool lost ) override
 {
  Panel::internalFocusChanged( lost );
#if VGUI_ABI_COMPAT_MODE != 1
  if ( lost == _onLost )
   delete this;
#endif
 }

private:
 bool _onLost;
};

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

// Gives a panel the key focus. Off Windows a tick first gives a surface's
// panel the focus when no panel has it.
static void Focus(Panel* panel)
{
 app->externalTick();
 panel->requestFocus();
 app->externalTick();
 Check(panel == app->keyFocus(), "key focus taken");
}

static void Run(const std::string& test)
{
 if (test == "surface-deleted")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* focused = new Panel( 0, 0, 10, 10 );
  Panel* wanted = new Panel( 0, 0, 10, 10 );
  focused->setParent( root );
  wanted->setParent( root );
  Panel* otherRoot = new Panel( 0, 0, 100, 100 );
  TestSurface* otherSurface = new TestSurface( otherRoot );
  Focus(focused);
  wanted->requestFocus();
  app->setMouseCapture( wanted );
  app->setMouseArena( wanted );
  delete surface;
  Check(app->allClear(), "App forgot the surface's panels");
  Check(1 == arenaReleases, "cursor clip released");
  // The other surface keeps App ticking with a focused surface.
  app->externalTick();
  Check(NULL == app->keyFocus() || otherRoot == app->keyFocus(), "no key focus from the deleted surface");
  delete otherSurface;
#if VGUI_ABI_COMPAT_MODE != 1
  delete root;
  delete otherRoot;
#endif
 }
 else if (test == "capture-detached")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* parent = new Panel( 0, 0, 50, 50 );
  Panel* child = new Panel( 0, 0, 10, 10 );
  parent->setParent( root );
  child->setParent( parent );
  app->setMouseCapture( child );
  Check(1 == surface->captures, "captured");
  root->removeChild( parent );
  app->setMouseCapture( NULL );
  Check(1 == surface->releases, "released on the capturing surface");
  app->setMouseCapture( child );
  Check(child == app->mouseCapture() && 1 == surface->captures, "no native capture off a surface");
  app->setMouseCapture( NULL );
  Check(1 == surface->releases && NULL == app->mouseCapture(), "nothing to release");
  delete surface;
#if VGUI_ABI_COMPAT_MODE != 1
  delete root;
  delete parent;
#endif
 }
 else if (test == "capture-switch")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* otherRoot = new Panel( 0, 0, 100, 100 );
  TestSurface* otherSurface = new TestSurface( otherRoot );
  app->setMouseCapture( root );
  app->setMouseCapture( root );
  Check(2 == surface->captures && 0 == surface->releases, "same surface kept");
  app->setMouseCapture( otherRoot );
  Check(1 == surface->releases && 1 == otherSurface->captures, "capture moved");
  app->setMouseCapture( NULL );
  Check(1 == otherSurface->releases && 1 == surface->releases, "released once");
  delete surface;
  delete otherSurface;
#if VGUI_ABI_COMPAT_MODE != 1
  delete root;
  delete otherRoot;
#endif
 }
 else if (test == "repaint-all-attached")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* parent = new Panel( 0, 0, 50, 50 );
  Panel* child = new Panel( 0, 0, 10, 10 );
  parent->setParent( root );
  child->setParent( parent );
  child->repaintAll();
  Check(root == surface->invalidated, "surface's top panel invalidated");
  delete surface;
#if VGUI_ABI_COMPAT_MODE != 1
  delete root;
#endif
 }
 else if (test == "root-deleted-first")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  ( new Panel( 0, 0, 10, 10 ) )->setParent( root );
#if VGUI_ABI_COMPAT_MODE != 1
  delete root;
  Check(NULL == surface->getPanel(), "surface lost its root");
  app->externalTick();
#else
  // As VGuiWrap_Shutdown does without destructors.
  ::operator delete( root );
#endif
  delete surface;
 }
 else if (test == "surface-copy-detached")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* focused = new Panel( 0, 0, 10, 10 );
  focused->setParent( root );
  Focus(focused);
  app->setMouseArena( focused );
  TestSurface* copy = new TestSurface( *surface );
  Check(NULL == copy->getPanel(), "the copy has no root");
  Check(!app->ticks( copy ), "App does not tick the copy");
  Check(surface == root->getSurfaceBase(), "the root stays on the original");
  copy->setEmulatedCursorVisible( true );
  copy->setEmulatedCursorPos( 1, 1 );
  app->externalTick();
  delete copy;
  Check(focused == app->keyFocus(), "key focus kept");
  Check(0 == arenaReleases, "arena kept");
  Check(app->ticks( surface ), "App still ticks the original");
#if VGUI_ABI_COMPAT_MODE != 1
  Check(2 == root->getChildCount(), "the original keeps its emulated cursor");
#endif
  surface->setEmulatedCursorPos( 1, 1 );
  delete surface;
#if VGUI_ABI_COMPAT_MODE != 1
  delete root;
#endif
 }
 else if (test == "surface-assign-keeps-root")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* otherRoot = new Panel( 0, 0, 100, 100 );
  TestSurface* otherSurface = new TestSurface( otherRoot );
  *otherSurface = *surface;
  Check(otherRoot == otherSurface->getPanel(), "the assigned surface keeps its root");
  Check(otherSurface == otherRoot->getSurfaceBase(), "its root stays on it");
  delete surface;
  Check(otherRoot == otherSurface->getPanel(), "deleting the source keeps the root");
#if VGUI_ABI_COMPAT_MODE != 1
  Check(1 == otherRoot->getChildCount(), "the assigned surface keeps its emulated cursor");
#endif
  otherSurface->setEmulatedCursorPos( 1, 1 );
  delete otherSurface;
#if VGUI_ABI_COMPAT_MODE != 1
  delete root;
  delete otherRoot;
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "surface-deleted-first")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* child = new Panel( 0, 0, 10, 10 );
  child->setParent( root );
  Check(2 == root->getChildCount(), "emulated cursor added");
  delete surface;
  Check(NULL == root->getSurfaceBase() && NULL == child->getSurfaceBase(), "tree off the surface");
  Check(1 == root->getChildCount() && child == root->getChild( 0 ), "emulated cursor deleted");
  child->repaint();
  child->repaintAll();
  delete root;
 }
 else if (test == "moved-root")
 {
  Panel* otherRoot = new Panel( 0, 0, 100, 100 );
  TestSurface* otherSurface = new TestSurface( otherRoot );
  // The surface is deleted first.
  Panel* root = new Panel( 0, 0, 50, 50 );
  TestSurface* surface = new TestSurface( root );
  root->setParent( otherRoot );
  delete surface;
  Check(otherSurface == root->getSurfaceBase(), "moved root kept the other surface");
  delete root;
  // The moved root is deleted first.
  root = new Panel( 0, 0, 50, 50 );
  surface = new TestSurface( root );
  root->setParent( otherRoot );
  delete root;
  Check(NULL == surface->getPanel(), "surface forgot the moved root");
  delete surface;
  delete otherSurface;
  delete otherRoot;
 }
 else if (test == "captured-panel-deleted")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* child = new Panel( 0, 0, 10, 10 );
  child->setParent( root );
  app->setMouseCapture( child );
  app->setMouseArena( child );
  delete child;
  Check(1 == surface->releases, "native capture released");
  Check(1 == arenaReleases, "cursor clip released");
  Check(app->allClear(), "App pointers cleared");
  delete surface;
  delete root;
 }
 else if (test == "focus-deleted-in-callback")
 {
  Panel* root = new Panel( 0, 0, 100, 100 );
  TestSurface* surface = new TestSurface( root );
  Panel* losing = new SelfDeletingPanel( true );
  Panel* plain = new Panel( 0, 0, 10, 10 );
  losing->setParent( root );
  plain->setParent( root );
  Focus(losing);
  plain->requestFocus();
  // losing deletes itself when it loses the focus.
  app->externalTick();
  Check(plain == app->keyFocus(), "focus moved");
  Panel* getting = new SelfDeletingPanel( false );
  getting->setParent( root );
  getting->requestFocus();
  // getting deletes itself when it gets the focus.
  app->externalTick();
  Check(NULL == app->keyFocus(), "deleted panel not focused");
  delete surface;
  delete root;
 }
#endif
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
 // Static so that the scheme App never frees stays reachable for leak checks.
 static TestApp testApp;
 app = &testApp;
 // The stub clock stands still, so every tick has to run.
 app->setMinimumTickMillisInterval( 0 );
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

SCENARIOS = ['surface-deleted', 'capture-detached', 'capture-switch', 'repaint-all-attached', 'root-deleted-first',
             'surface-copy-detached', 'surface-assign-keeps-root']
NONCOMPAT_SCENARIOS = ['captured-panel-deleted', 'focus-deleted-in-callback', 'surface-deleted-first',
                       'moved-root']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}
SOURCES = ['SurfaceBase.cpp', 'ImagePanel.cpp']


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-app-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS)
        sources = vgui1_sources(folder)
        stubs = sources[0].read_text()
        if ARENA_STUB not in stubs:
            raise SystemExit('platform stubs no longer match: ' + ARENA_STUB)
        sources[0].write_text(stubs.replace(ARENA_STUB, COUNTING_ARENA))
        sources = [test_file] + sources + [VGUI / name for name in SOURCES]
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
