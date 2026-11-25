"""Exercise the production VGUI2 CWin32Surface::PushMakeCurrent, PopMakeCurrent, SetAsTopMost and EnableMouseCapture with a panel that has no ancestor with a platform window, and with one that has (#484).

Run: python tests/vgui2_surface_window_tests.py
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

// only the Win32 calls the extracted functions make; each records what it was given
typedef void* HWND;
typedef void* HDC;
typedef void* HRGN;
typedef void* HGDIOBJ;
typedef void* LPPOINT;
typedef unsigned int UINT;
typedef int BOOL;
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOACTIVATE 0x0010
static char topMostTag, noTopMostTag;
#define HWND_TOPMOST ((HWND)&topMostTag)
#define HWND_NOTOPMOST ((HWND)&noTopMostTag)

struct Rect { int left, top, right, bottom; };
static Rect lastRgn;
static HRGN lastRgnHandle;
static HDC lastSelectedDC;
static HWND captured, focused, positioned, insertAfter;
static UINT positionFlags;
static int releases;

static BOOL SetViewportOrgEx(HDC, int, int, LPPOINT) { return 1; }
static BOOL SetRectRgn(HRGN rgn, int left, int top, int right, int bottom) {
 lastRgnHandle = rgn;
 lastRgn = Rect{left, top, right, bottom};
 return 1;
}
static HGDIOBJ SelectObject(HDC dc, HGDIOBJ object) { lastSelectedDC = dc; return object; }
static BOOL SetWindowPos(HWND hwnd, HWND after, int, int, int, int, UINT flags) {
 positioned = hwnd;
 insertAfter = after;
 positionFlags = flags;
 return 1;
}
static HWND SetFocus(HWND hwnd) { focused = hwnd; return nullptr; }
static HWND SetCapture(HWND hwnd) { captured = hwnd; return nullptr; }
static BOOL ReleaseCapture() { ++releases; return 1; }

namespace vgui2 {
typedef uintptr_t VPANEL;

// counts the warnings DPrintf reports
struct IVGui {
 void DPrintf(const char* format) { ++warnings; printf("%s", format); }
 int warnings = 0;
};
static IVGui gui;
static IVGui* g_pIVgui = &gui;

class SurfacePlat {
public:
 HWND hwnd;
 HDC hdc;
 HRGN clipRgn;
 bool disabled;
};

struct IClientPanel {
 void GetInset(int& left, int& top, int& right, int& bottom) { left = top = right = bottom = 1; }
 void GetClipRect(int& x0, int& y0, int& x1, int& y1) { x0 = y0 = 0; x1 = y1 = 100; }
};

// only the members the window lookup and the context use
class VPanel {
public:
 SurfacePlat* Plat() { return plat; }
 VPanel* GetParent() { return parent; }
 IClientPanel* Client() { return &client; }
 void GetAbsPos(int& x, int& y) { x = y = 0; }
 void GetSize(int& wide, int& tall) { wide = tall = 100; }
 SurfacePlat* plat = nullptr;
 VPanel* parent = nullptr;
 IClientPanel client;
};

struct CWin32Font;

// only the members the extracted functions use
class CWin32Surface {
public:
 void PushMakeCurrent(VPANEL panel, bool useInsets);
 void PopMakeCurrent(VPANEL panel);
 void SetAsTopMost(VPANEL panel, bool state);
 void EnableMouseCapture(VPANEL panel, bool state);
 void SetContextPanel(VPANEL panel);
 SurfacePlat* ContextPlat();
 VPANEL _embeddedPanel = 0;
 VPANEL _currentContextPanel = 0;
 CWin32Font* _currentFontObject = nullptr;
};
'''

TESTS = r'''
}
using namespace vgui2;

static char embeddedTag, windowTag;

// an embedded panel with its own window, a window, a child of it and a panel outside any window
struct Fixture {
 Fixture() {
  embeddedPlat = SurfacePlat{(HWND)&embeddedTag, (HDC)&embeddedTag, (HRGN)&embeddedTag, false};
  windowPlat = SurfacePlat{(HWND)&windowTag, (HDC)&windowTag, (HRGN)&windowTag, false};
  embedded.plat = &embeddedPlat;
  window.plat = &windowPlat;
  child.parent = &window;
  orphanChild.parent = &orphan;
  surface._embeddedPanel = (VPANEL)&embedded;
  surface.SetContextPanel(surface._embeddedPanel);
  captured = focused = positioned = insertAfter = nullptr;
  lastRgnHandle = nullptr;
  lastSelectedDC = nullptr;
  positionFlags = 0;
  releases = 0;
 }
 SurfacePlat embeddedPlat, windowPlat;
 VPanel embedded, window, child, orphan, orphanChild;
 CWin32Surface surface;
};

static void PushWithoutAWindowClipsEverythingAway() {
 Fixture f;
 int warnings = gui.warnings;
 f.surface.PushMakeCurrent((VPANEL)&f.orphanChild, true);
 assert(warnings + 1 == gui.warnings);
 assert(f.surface._embeddedPanel == f.surface._currentContextPanel);
 assert(&f.embeddedPlat == f.surface.ContextPlat());
 assert(f.embeddedPlat.clipRgn == lastRgnHandle);
 assert(0 == lastRgn.left && 0 == lastRgn.top && 0 == lastRgn.right && 0 == lastRgn.bottom);
 assert(f.embeddedPlat.hdc == lastSelectedDC);
 f.surface.PopMakeCurrent((VPANEL)&f.orphanChild);
 assert(f.surface._embeddedPanel == f.surface._currentContextPanel);
 puts("PASS: CWin32Surface::PushMakeCurrent with no window keeps the embedded panel as context with an empty clip region, and PopMakeCurrent keeps it");
}

static void PushWithAWindowUsesIt() {
 Fixture f;
 int warnings = gui.warnings;
 f.surface.PushMakeCurrent((VPANEL)&f.child, true);
 assert(warnings == gui.warnings);
 assert((VPANEL)&f.window == f.surface._currentContextPanel);
 assert(f.windowPlat.clipRgn == lastRgnHandle);
 assert(0 == lastRgn.left && 0 == lastRgn.top && 99 == lastRgn.right && 99 == lastRgn.bottom);
 f.surface.PopMakeCurrent((VPANEL)&f.window);
 assert(f.surface._embeddedPanel == f.surface._currentContextPanel);
 puts("PASS: CWin32Surface::PushMakeCurrent makes the nearest window the context and PopMakeCurrent restores the embedded panel");
}

static void TopMostWithoutAWindowDoesNothing() {
 Fixture f;
 f.surface.SetAsTopMost((VPANEL)&f.orphanChild, true);
 f.surface.SetAsTopMost((VPANEL)&f.orphanChild, false);
 assert(nullptr == positioned);
 assert(nullptr == focused);
 puts("PASS: CWin32Surface::SetAsTopMost with no window does nothing instead of dereferencing it");
}

static void TopMostWithAWindowPositionsIt() {
 Fixture f;
 f.surface.SetAsTopMost((VPANEL)&f.child, true);
 assert((HWND)&windowTag == positioned);
 assert((HWND)&windowTag == focused);
 assert(HWND_TOPMOST == insertAfter);
 assert((SWP_NOMOVE | SWP_NOSIZE) == positionFlags);
 f.windowPlat.disabled = true;
 f.surface.SetAsTopMost((VPANEL)&f.child, false);
 assert(HWND_NOTOPMOST == insertAfter);
 assert((SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) == positionFlags);
 puts("PASS: CWin32Surface::SetAsTopMost moves the nearest window in and out of the topmost band");
}

static void CaptureWithoutAWindowOnlyReleases() {
 Fixture f;
 f.surface.EnableMouseCapture((VPANEL)&f.orphanChild, true);
 assert(nullptr == captured);
 f.surface.EnableMouseCapture((VPANEL)&f.orphanChild, false);
 assert(1 == releases);
 puts("PASS: CWin32Surface::EnableMouseCapture with no window captures nothing and still releases the capture");
}

static void CaptureWithAWindowCapturesIt() {
 Fixture f;
 f.surface.EnableMouseCapture((VPANEL)&f.child, true);
 assert((HWND)&windowTag == captured);
 assert(0 == releases);
 puts("PASS: CWin32Surface::EnableMouseCapture captures the mouse to the nearest window");
}

int main() {
 PushWithoutAWindowClipsEverythingAway();
 PushWithAWindowUsesIt();
 TopMostWithoutAWindowDoesNothing();
 TopMostWithAWindowPositionsIt();
 CaptureWithoutAWindowOnlyReleases();
 CaptureWithAWindowCapturesIt();
}
'''


def main():
    source = (ROOT / 'vgui2/src/Surface.cpp').read_text(encoding='utf-8')
    signatures = ('SurfacePlat* CWin32Surface::ContextPlat()', 'void CWin32Surface::SetContextPanel( VPANEL panel )',
                  'void CWin32Surface::PushMakeCurrent( VPANEL pPanel, bool useInsets )', 'void CWin32Surface::PopMakeCurrent( VPANEL panel )',
                  'void CWin32Surface::SetAsTopMost( VPANEL panel, bool state )', 'void CWin32Surface::EnableMouseCapture( VPANEL panel, bool state )')
    code = STUBS + ''.join(function(source, signature) + '\n' for signature in signatures) + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-surface-window-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
