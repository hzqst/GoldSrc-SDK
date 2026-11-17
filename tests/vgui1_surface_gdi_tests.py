"""Compile the production VGUI1 Win32 Surface GDI resource handling (#242).

Run: python tests/vgui1_surface_gdi_tests.py
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it.

Surface::recreateContext is extracted as-is and run against stub GDI calls that
track live objects and DC selections. As in GDI, deleting an object that is
still selected into a DC fails and leaves it alive.

- resize-cycle: alternately growing and shrinking the surface keeps one backing
  bitmap and one texture DC alive, and every delete succeeds.
- texture-dc-reused: the texture DC survives backing bitmap recreation.
- within-spare: a size inside the spare band does not recreate anything.
- minimum-size: the smallest surface that needs a context (1x1) gets a
  spare-sized backing bitmap.
- bitmap-create-fails: a refused bitmap keeps the old one selected and the old
  size, and the next resize retries.
- select-fails: a refused selection deletes the new bitmap and keeps the old.
- texture-dc-fails: a refused texture DC returns false and is retried.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


PRELUDE = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

typedef int BOOL;
typedef struct HDC__* HDC;
typedef struct HBITMAP__* HBITMAP;
typedef void* HGDIOBJ;
static HGDIOBJ const HGDI_ERROR = reinterpret_cast<HGDIOBJ>(static_cast<std::intptr_t>(-1));

enum GdiKind { GDI_DC, GDI_BITMAP };

struct GdiObject
{
 GdiKind kind;
 bool alive;
 bool stock;
 int wide;
 int tall;
 std::size_t selected;
};

static std::vector<GdiObject> objects;
static int failedDeletes = 0;
static bool failCreateBitmap = false;
static bool failCreateDC = false;
static bool failSelect = false;

static std::size_t Add(GdiKind kind, bool stock, int wide, int tall)
{
 GdiObject object = { kind, true, stock, wide, tall, 0 };
 objects.push_back(object);
 return objects.size();
}

static GdiObject* Find(const void* handle, GdiKind kind)
{
 const std::size_t index = reinterpret_cast<std::size_t>(handle);
 if (index == 0 || index > objects.size()) return NULL;
 GdiObject& object = objects[index - 1];
 return object.alive && object.kind == kind ? &object : NULL;
}

static bool IsSelected(std::size_t bitmap)
{
 for (const GdiObject& object : objects)
 {
  if (object.alive && object.kind == GDI_DC && object.selected == bitmap) return true;
 }
 return false;
}

static int LiveCount(GdiKind kind)
{
 int count = 0;
 for (const GdiObject& object : objects)
 {
  if (object.alive && !object.stock && object.kind == kind) ++count;
 }
 return count;
}

static HDC CreateCompatibleDC(HDC dc)
{
 if (failCreateDC || (dc != NULL && Find(dc, GDI_DC) == NULL)) return NULL;
 const std::size_t defaultBitmap = Add(GDI_BITMAP, true, 1, 1);
 const std::size_t handle = Add(GDI_DC, false, 0, 0);
 objects[handle - 1].selected = defaultBitmap;
 return reinterpret_cast<HDC>(handle);
}

static HBITMAP CreateCompatibleBitmap(HDC dc, int wide, int tall)
{
 Check(Find(dc, GDI_DC) != NULL, "bitmap created for a live DC");
 if (failCreateBitmap) return NULL;
 return reinterpret_cast<HBITMAP>(Add(GDI_BITMAP, false, wide, tall));
}

static HGDIOBJ SelectObject(HDC dc, HGDIOBJ object)
{
 GdiObject* target = Find(dc, GDI_DC);
 if (failSelect || target == NULL || Find(object, GDI_BITMAP) == NULL) return NULL;
 const std::size_t previous = target->selected;
 target->selected = reinterpret_cast<std::size_t>(object);
 return reinterpret_cast<HGDIOBJ>(previous);
}

static BOOL DeleteObject(HGDIOBJ handle)
{
 GdiObject* object = Find(handle, GDI_BITMAP);
 if (object == NULL || IsSelected(reinterpret_cast<std::size_t>(handle)))
 {
  ++failedDeletes;
  return 0;
 }
 if (!object->stock) object->alive = false;
 return 1;
}

static HBITMAP Selected(HDC dc)
{
 GdiObject* object = Find(dc, GDI_DC);
 return object != NULL ? reinterpret_cast<HBITMAP>(object->selected) : NULL;
}

class Panel
{
public:
 void getSize(int& wide, int& tall) { wide = _wide; tall = _tall; }
 int _wide;
 int _tall;
};

struct SurfacePlat
{
 HDC drawDC;
 HDC windowDC;
 HDC textureDC;
 HBITMAP bitmap;
 int bitmapSize[2];
};

class Surface
{
public:
 Surface()
 {
  _panel._wide = 0;
  _panel._tall = 0;
  _plat = &_platData;
  _platData = SurfacePlat();
  _platData.windowDC = CreateCompatibleDC(NULL);
  _platData.drawDC = CreateCompatibleDC(NULL);
  _platData.textureDC = NULL;
  _platData.bitmap = NULL;
 }
 Panel* getPanel() { return &_panel; }
 void setSize(int wide, int tall) { _panel._wide = wide; _panel._tall = tall; }
 SurfacePlat* plat() { return _plat; }
 bool recreateContext();
protected:
 Panel _panel;
 SurfacePlat* _plat;
 SurfacePlat _platData;
};
'''

TESTS = r'''
static const int RESIZE_CYCLES = 32;
static const int SMALL_SIZE[2] = { 160, 120 };
static const int LARGE_SIZE[2] = { 1024, 768 };
static const int MINIMUM_SIZE = 1;
// windowDC and drawDC are owned by the test fixture
static const int FIXTURE_DCS = 2;

static void CheckBacking(Surface& surface, int wide, int tall)
{
 SurfacePlat* plat = surface.plat();
 GdiObject* bitmap = Find(plat->bitmap, GDI_BITMAP);
 Check(bitmap != NULL, "backing bitmap alive");
 Check(Selected(plat->drawDC) == plat->bitmap, "backing bitmap selected into drawDC");
 Check(bitmap->wide == wide + BITMAP_SPARE_PIXELS && bitmap->tall == tall + BITMAP_SPARE_PIXELS,
  "backing bitmap has spare pixels");
 Check(plat->bitmapSize[0] == bitmap->wide && plat->bitmapSize[1] == bitmap->tall, "bitmap size recorded");
}

static void CheckOwnership(Surface& surface)
{
 Check(LiveCount(GDI_BITMAP) == 1, "one backing bitmap alive");
 Check(LiveCount(GDI_DC) == FIXTURE_DCS + (surface.plat()->textureDC != NULL ? 1 : 0), "no leaked DCs");
 Check(failedDeletes == 0, "every delete succeeded");
}

static void Run(const std::string& test)
{
 Surface surface;
 SurfacePlat* plat = surface.plat();
 if (test == "resize-cycle")
 {
  for (int i = 0; i < RESIZE_CYCLES; ++i)
  {
   const int* size = (i % 2) ? SMALL_SIZE : LARGE_SIZE;
   surface.setSize(size[0], size[1]);
   Check(surface.recreateContext(), "context recreated");
   CheckBacking(surface, size[0], size[1]);
   CheckOwnership(surface);
  }
 }
 else if (test == "texture-dc-reused")
 {
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(surface.recreateContext(), "first context");
  const HDC textureDC = plat->textureDC;
  Check(Find(textureDC, GDI_DC) != NULL, "texture DC created");
  surface.setSize(LARGE_SIZE[0], LARGE_SIZE[1]);
  Check(surface.recreateContext(), "second context");
  Check(plat->textureDC == textureDC, "texture DC reused");
  CheckOwnership(surface);
 }
 else if (test == "within-spare")
 {
  surface.setSize(LARGE_SIZE[0], LARGE_SIZE[1]);
  Check(surface.recreateContext(), "first context");
  const HBITMAP bitmap = plat->bitmap;
  const std::size_t objectCount = objects.size();
  surface.setSize(LARGE_SIZE[0] - BITMAP_SPARE_PIXELS / 2, LARGE_SIZE[1] + BITMAP_SPARE_PIXELS / 2);
  Check(surface.recreateContext(), "context kept");
  Check(plat->bitmap == bitmap && objects.size() == objectCount, "nothing recreated");
  CheckOwnership(surface);
 }
 else if (test == "minimum-size")
 {
  surface.setSize(MINIMUM_SIZE, MINIMUM_SIZE);
  Check(surface.recreateContext(), "context created");
  CheckBacking(surface, MINIMUM_SIZE, MINIMUM_SIZE);
  CheckOwnership(surface);
 }
 else if (test == "bitmap-create-fails")
 {
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(surface.recreateContext(), "first context");
  failCreateBitmap = true;
  surface.setSize(LARGE_SIZE[0], LARGE_SIZE[1]);
  Check(!surface.recreateContext(), "refused bitmap returns false");
  CheckBacking(surface, SMALL_SIZE[0], SMALL_SIZE[1]);
  CheckOwnership(surface);
  failCreateBitmap = false;
  Check(surface.recreateContext(), "retry succeeds");
  CheckBacking(surface, LARGE_SIZE[0], LARGE_SIZE[1]);
  CheckOwnership(surface);
 }
 else if (test == "select-fails")
 {
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(surface.recreateContext(), "first context");
  failSelect = true;
  surface.setSize(LARGE_SIZE[0], LARGE_SIZE[1]);
  Check(!surface.recreateContext(), "refused selection returns false");
  failSelect = false;
  CheckBacking(surface, SMALL_SIZE[0], SMALL_SIZE[1]);
  CheckOwnership(surface);
 }
 else if (test == "texture-dc-fails")
 {
  failCreateDC = true;
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(!surface.recreateContext(), "refused texture DC returns false");
  Check(plat->textureDC == NULL, "texture DC left empty");
  CheckBacking(surface, SMALL_SIZE[0], SMALL_SIZE[1]);
  failCreateDC = false;
  Check(surface.recreateContext(), "retry succeeds");
  Check(Find(plat->textureDC, GDI_DC) != NULL, "texture DC created");
  CheckOwnership(surface);
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


def supports_asan(compiler, folder):
    probe, output = Path(folder) / 'probe.cpp', Path(folder) / 'probe.exe'
    probe.write_text('int main() { return 0; }\n')
    result = subprocess.run(compiler + ['-fsanitize=address', str(probe), '-o', str(output)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode == 0


def main():
    surface = (VGUI / 'win32/Surface.cpp').read_text()
    program = '\n'.join([
        re.search(r'^#define BITMAP_SPARE_PIXELS.*$', surface, re.M).group(0),
        PRELUDE,
        block(surface, 'bool Surface::recreateContext()', '\n}\n'),
        TESTS,
    ])

    scenarios = ['resize-cycle', 'texture-dc-reused', 'within-spare', 'minimum-size',
                 'bitmap-create-fails', 'select-fails', 'texture-dc-fails']
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-gdi-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(program)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        flags = ['-std=c++17', '-g']
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
