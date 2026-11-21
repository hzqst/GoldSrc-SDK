"""Exercise VGUI2 cursor conversion against an SDL window stub."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature, occurrence=0, end='\n}'):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = -1
    for _ in range(occurrence + 1):
        start = source.index(signature, start + 1)
    return source[start:source.index(end, start) + len(end)]


STUBS = r'''
#include <cassert>
#include <cstdlib>
struct SDL_Window {};
struct Game { SDL_Window* GetMainWindow() { return nullptr; } } instance, *game = &instance;
class BaseUISurface
{
public:
    void SurfaceGetCursorPos(int&, int&);
    void SurfaceSetCursorPos(int, int);
};
static bool windowed;
static int mouseX, mouseY;
static float xAspect = 1.0f;
static bool VideoMode_IsWindowed() { return windowed; }
static float GetXMouseAspectRatioAdjustment() { return xAspect; }
static float GetYMouseAspectRatioAdjustment() { return 1.0f; }
static void VideoMode_GetCurrentVideoMode(int* w, int* h, int*) { *w = 800; *h = 600; }
static void SDL_GetWindowSize(SDL_Window*, int* w, int* h) { *w = 1920; *h = 1080; }
static void SDL_GetMouseState(int* x, int* y) { *x = mouseX; *y = mouseY; }
static void SDL_WarpMouseInWindow(SDL_Window*, int x, int y) { mouseX = x; mouseY = y; }
'''

TESTS = r'''
int main()
{
    BaseUISurface surface;
    windowed = true;
    surface.SurfaceSetCursorPos(123, 456);
    assert(123 == mouseX && 456 == mouseY);
    windowed = false;
    surface.SurfaceSetCursorPos(400, 300);
    assert(960 == mouseX && 540 == mouseY);
    xAspect = 4.0f / 3.0f;
    surface.SurfaceSetCursorPos(144, 216);
    int x, y;
    surface.SurfaceGetCursorPos(x, y);
    assert(std::abs(x - 144) <= 2 && std::abs(y - 216) <= 2);
}
'''


def main():
    production = [
        extract("vgui2/BaseUISurface.cpp", "void BaseUISurface::SurfaceGetCursorPos"),
        extract("vgui2/BaseUISurface.cpp", "void BaseUISurface::SurfaceSetCursorPos"),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-cursor-coordinates-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
