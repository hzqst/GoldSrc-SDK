"""Compile the production VGUI1 Win32 Surface::setFullscreenMode (#290).

Run: python tests/vgui1_surface_fullscreen_tests.py
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it.

The SurfaceBase mode-info methods and the Win32 Surface::getModeInfoCount /
setFullscreenMode are extracted as-is and run against a Surface that keeps the
shadowing _unusedModeInfoDar member and stub Win32 display calls.

- listed-mode: 640x480x16 and 800x600x16 switch the display and record the
  fullscreen mode.
- unlisted-mode: a mode that getModeInfoCount does not list is rejected without
  touching the display.
- already-fullscreen: asking for the current fullscreen mode does nothing.
- change-fails: a refused display change returns false and stays windowed.
- shadow-unused: the shadowing Surface list is never filled.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


PRELUDE = r'''
#include "VGUI_Dar.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace vgui;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef void* HWND;

struct DEVMODEA
{
 WORD dmSize;
 DWORD dmFields;
 DWORD dmBitsPerPel;
 DWORD dmPelsWidth;
 DWORD dmPelsHeight;
 DWORD dmPanningWidth;
};

static const DWORD DM_BITSPERPEL = 0x40000;
static const DWORD DM_PELSWIDTH = 0x80000;
static const DWORD DM_PELSHEIGHT = 0x100000;
static const DWORD CDS_FULLSCREEN = 4;
static const long DISP_CHANGE_SUCCESSFUL = 0;
static const long DISP_CHANGE_FAILED = -1;
static HWND const HWND_TOPMOST = reinterpret_cast<HWND>(-1);
static const unsigned SWP_NOMOVE = 2;
static const unsigned SWP_NOSIZE = 1;

static int displayChanges = 0;
static long displayChangeResult = DISP_CHANGE_SUCCESSFUL;
static DEVMODEA lastMode;

static long ChangeDisplaySettingsA(DEVMODEA* mode, DWORD flags)
{
 Check(mode != NULL && flags == CDS_FULLSCREEN, "fullscreen change requested");
 ++displayChanges;
 lastMode = *mode;
 return displayChangeResult;
}

static int SetWindowPos(HWND, HWND, int, int, int, int, unsigned) { return 1; }

class Panel
{
public:
 void getBounds(int& x, int& y, int& wide, int& tall) { x = 0; y = 0; wide = 320; tall = 240; }
 void setBounds(int, int, int, int) {}
 void repaint() {}
};

struct SurfacePlat
{
 HWND window;
 bool isFullscreen;
 int fullscreenInfo[3];
 int restoreInfo[4];
};

class App
{
public:
 static App* getInstance() { static App app; return &app; }
 void surfaceBaseDeleted(class SurfaceBase*) {}
};

class SurfaceBase
{
public:
 virtual ~SurfaceBase();
 Panel* getPanel() { return &_panel; }
 virtual void resetModeInfo();
 virtual int getModeInfoCount();
 virtual bool getModeInfo(int index, int& wide, int& tall, int& bpp);
protected:
 virtual void addModeInfo(int wide, int tall, int bpp);
 Dar<char*> _modeInfoDar;
 Panel _panel;
};

class Surface : public SurfaceBase
{
public:
 Surface() { _plat = &_platData; _platData = SurfacePlat(); }
 virtual int getModeInfoCount();
 virtual bool setFullscreenMode(int wide, int tall, int bpp);
 void applyChanges() {}
 SurfacePlat* plat() { return _plat; }
 int unusedCount() { return _unusedModeInfoDar.getCount(); }
protected:
 SurfacePlat* _plat;
 SurfacePlat _platData;
 Dar<char*> _unusedModeInfoDar;
};
'''

TESTS = r'''
static const int LISTED_MODES[][3] = { { 640, 480, 16 }, { 800, 600, 16 } };
static const int UNLISTED_WIDE = 1024;
static const int UNLISTED_TALL = 768;
static const int UNLISTED_BPP = 32;

static void CheckFullscreen(Surface& surface, int wide, int tall, int bpp)
{
 Check(surface.plat()->isFullscreen, "surface is fullscreen");
 Check(surface.plat()->fullscreenInfo[0] == wide && surface.plat()->fullscreenInfo[1] == tall
  && surface.plat()->fullscreenInfo[2] == bpp, "fullscreen mode recorded");
 Check(static_cast<int>(lastMode.dmPelsWidth) == wide && static_cast<int>(lastMode.dmPelsHeight) == tall
  && static_cast<int>(lastMode.dmBitsPerPel) == bpp, "requested display mode");
}

static void Run(const std::string& test)
{
 Surface surface;
 if (test == "listed-mode")
 {
  for (const int* mode : LISTED_MODES)
  {
   const int changesBefore = displayChanges;
   Check(surface.setFullscreenMode(mode[0], mode[1], mode[2]), "listed mode accepted");
   Check(displayChanges == changesBefore + 1, "display changed once");
   CheckFullscreen(surface, mode[0], mode[1], mode[2]);
  }
 }
 else if (test == "unlisted-mode")
 {
  Check(!surface.setFullscreenMode(UNLISTED_WIDE, UNLISTED_TALL, UNLISTED_BPP), "unlisted mode rejected");
  Check(displayChanges == 0, "display untouched");
  Check(!surface.plat()->isFullscreen, "still windowed");
 }
 else if (test == "already-fullscreen")
 {
  Check(surface.setFullscreenMode(640, 480, 16), "first switch");
  Check(surface.setFullscreenMode(640, 480, 16), "same mode accepted");
  Check(displayChanges == 1, "no second display change");
 }
 else if (test == "change-fails")
 {
  displayChangeResult = DISP_CHANGE_FAILED;
  Check(!surface.setFullscreenMode(640, 480, 16), "refused change returns false");
  Check(displayChanges == 1, "display change attempted");
  Check(!surface.plat()->isFullscreen, "still windowed");
 }
 else if (test == "shadow-unused")
 {
  surface.setFullscreenMode(800, 600, 16);
  Check(surface.unusedCount() == 0, "shadowing list stays empty");
  Check(surface.SurfaceBase::getModeInfoCount() == 2, "modes listed in SurfaceBase");
 }
 else Check(false, "unknown scenario");
}

int main(int argc, char** argv)
{
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


def methods(source, signatures):
    return '\n'.join(block(source, signature, '\n}\n') for signature in signatures)


def supports_asan(compiler, folder):
    probe, output = Path(folder) / 'probe.cpp', Path(folder) / 'probe.exe'
    probe.write_text('int main() { return 0; }\n')
    result = subprocess.run(compiler + ['-fsanitize=address', str(probe), '-o', str(output)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode == 0


def main():
    surface_base = (VGUI / 'SurfaceBase.cpp').read_text()
    surface = (VGUI / 'win32/Surface.cpp').read_text()
    program = '\n'.join([
        PRELUDE,
        methods(surface_base, (
            'void freeModeInfo(',
            'SurfaceBase::~SurfaceBase()',
            'void SurfaceBase::resetModeInfo()',
            'int SurfaceBase::getModeInfoCount()',
            'bool SurfaceBase::getModeInfo(',
            'void SurfaceBase::addModeInfo(')),
        methods(surface, (
            'int Surface::getModeInfoCount()',
            'bool Surface::setFullscreenMode(')),
        TESTS,
    ])

    scenarios = ['listed-mode', 'unlisted-mode', 'already-fullscreen', 'change-fails', 'shadow-unused']
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-fullscreen-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(program)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        flags = ['-std=c++17', '-g', f'-I{VGUI / "include"}']
        if supports_asan(compiler, folder):
            flags.append('-fsanitize=address')
        subprocess.run(compiler + flags + [str(source_file), '-o', str(executable)], check=True)
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
