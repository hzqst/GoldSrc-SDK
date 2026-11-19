"""Compile the production VGUI1 Win32 Surface GDI resource handling (#214, #241, #242, #295, #298).

Run: python tests/vgui1_surface_gdi_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it.

Surface::createPlat, ~Surface, recreateContext, popMakeCurrent, the texture
methods, and the copy constructor and operator= from Surface.cpp are extracted as-is and run against stub Win32 / GDI calls that track live windows,
objects and DC selections. As in GDI,
deleting an object that is still selected into a DC fails and leaves it alive,
and a bitmap cannot be selected into a second DC.

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
- texture-reupload: re-uploading a drawn texture deletes the old bitmap, keeps
  one texture bitmap alive, and converts RGBA to BGRA.
- texture-selection-restored: drawing leaves the texture DC's previous
  bitmap selected.
- texture-two-surfaces: a texture drawn on one surface can be drawn on another.
- texture-create-fails: a refused re-upload keeps the old texture drawable, and
  a refused first upload draws nothing.
- destroy-releases: destroying a drawn surface releases its window, window DC,
  DCs, backing bitmap, clip region and SurfacePlat, detaching the window first.
- destroy-before-context: destroying a surface without a context releases
  everything it created.
- destroy-fullscreen: destroying a fullscreen surface restores the display mode.
- copy-owns-nothing: a copied surface has no window, and destroying it leaves
  the original's window and GDI objects alone (#437).
- assign-keeps-window: an assigned surface keeps its own window (#437).
- pop-deselects-font: popMakeCurrent selects the stock font into the draw DC,
  so the font a paint selected can be deleted.
- create-window-fails, create-region-fails, create-dc-fails, get-dc-fails: a
  refused window, clip region, memory DC or window DC makes createPlat return
  false without taking the screen DC, and destroying the surface releases what
  was created.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]
VGUI = ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


PRELUDE = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned char uchar;
typedef void* HANDLE;
typedef std::intptr_t LONG_PTR;
typedef struct HWND__* HWND;
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HRGN__* HRGN;
typedef struct HDC__* HDC;
typedef struct HBITMAP__* HBITMAP;
typedef void* HGDIOBJ;
static HGDIOBJ const HGDI_ERROR = reinterpret_cast<HGDIOBJ>(static_cast<std::intptr_t>(-1));
static const DWORD BI_RGB = 0;
static const UINT DIB_RGB_COLORS = 0;
static const DWORD SRCCOPY = 0x00CC0020;
static const DWORD WS_POPUP = 0x80000000;
static const UINT CS_HREDRAW = 2;
static const UINT CS_VREDRAW = 1;
static const UINT CS_DBLCLKS = 8;
static const int GWLP_USERDATA = -21;
static const int TRANSPARENT = 1;
static const UINT TA_UPDATECP = 1;
static const int SYSTEM_FONT = 13;

struct BITMAPINFOHEADER
{
 DWORD biSize;
 long biWidth;
 long biHeight;
 unsigned short biPlanes;
 unsigned short biBitCount;
 DWORD biCompression;
};

struct BITMAPINFO
{
 BITMAPINFOHEADER bmiHeader;
};

enum GdiKind { GDI_DC, GDI_BITMAP, GDI_REGION, GDI_WINDOW, GDI_FONT };

struct GdiObject
{
 GdiKind kind;
 bool alive;
 bool stock;
 int wide;
 int tall;
 std::size_t selected;
 uchar* bits;
 LONG_PTR userData;
 std::size_t font;
};

static std::vector<GdiObject> objects;
static int failedDeletes = 0;
static bool failCreateBitmap = false;
static bool failCreateDC = false;
static bool failSelect = false;
static bool failCreateDIB = false;
static bool failCreateWindow = false;
static bool failCreateRegion = false;
static bool failGetDC = false;
static int blits = 0;
static std::size_t lastBlitSource = 0;
static int displayRestores = 0;
static int livePlats = 0;
static int destroyedAttachedWindows = 0;

static std::size_t Add(GdiKind kind, bool stock, int wide, int tall)
{
 GdiObject object = { kind, true, stock, wide, tall, 0, NULL, 0, 0 };
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

static bool IsSelected(std::size_t bitmap, const GdiObject* except = NULL)
{
 for (const GdiObject& object : objects)
 {
  if (&object != except && object.alive && object.kind == GDI_DC && object.selected == bitmap) return true;
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

static HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits, HANDLE, DWORD)
{
 Check(Find(dc, GDI_DC) != NULL && info != NULL && usage == DIB_RGB_COLORS && bits != NULL, "DIB arguments");
 *bits = NULL;
 if (failCreateDIB) return NULL;
 const int wide = info->bmiHeader.biWidth;
 const int tall = -info->bmiHeader.biHeight;
 Check(info->bmiHeader.biBitCount == VGUI_RGBA_BYTES * 8 && tall > 0, "top-down 32-bit DIB");
 const std::size_t handle = Add(GDI_BITMAP, false, wide, tall);
 objects[handle - 1].bits = new uchar[VGUI_RGBA_BYTES * wide * tall];
 *bits = objects[handle - 1].bits;
 return reinterpret_cast<HBITMAP>(handle);
}

static bool IsFontSelected(std::size_t font)
{
 for (const GdiObject& object : objects)
 {
  if (object.alive && object.kind == GDI_DC && object.font == font) return true;
 }
 return false;
}

static HGDIOBJ GetStockObject(int kind)
{
 Check(kind == SYSTEM_FONT, "stock system font");
 static std::size_t systemFont = 0;
 if (systemFont == 0) systemFont = Add(GDI_FONT, true, 0, 0);
 return reinterpret_cast<HGDIOBJ>(systemFont);
}

static HGDIOBJ SelectObject(HDC dc, HGDIOBJ object)
{
 GdiObject* target = Find(dc, GDI_DC);
 if (Find(object, GDI_FONT) != NULL)
 {
  if (target == NULL) return NULL;
  const std::size_t previous = target->font;
  target->font = reinterpret_cast<std::size_t>(object);
  return reinterpret_cast<HGDIOBJ>(previous);
 }
 GdiObject* bitmap = Find(object, GDI_BITMAP);
 if (failSelect || target == NULL || bitmap == NULL) return NULL;
 // a bitmap can be selected into only one DC at a time
 if (!bitmap->stock && IsSelected(reinterpret_cast<std::size_t>(object), target)) return NULL;
 const std::size_t previous = target->selected;
 target->selected = reinterpret_cast<std::size_t>(object);
 return reinterpret_cast<HGDIOBJ>(previous);
}

static BOOL DeleteObject(HGDIOBJ handle)
{
 if (GdiObject* font = Find(handle, GDI_FONT))
 {
  if (IsFontSelected(reinterpret_cast<std::size_t>(handle)))
  {
   ++failedDeletes;
   return 0;
  }
  if (!font->stock) font->alive = false;
  return 1;
 }
 GdiObject* object = Find(handle, GDI_BITMAP);
 if (GdiObject* region = Find(handle, GDI_REGION))
 {
  region->alive = false;
  return 1;
 }
 if (object == NULL || IsSelected(reinterpret_cast<std::size_t>(handle)))
 {
  ++failedDeletes;
  return 0;
 }
 if (!object->stock)
 {
  object->alive = false;
  delete[] object->bits;
  object->bits = NULL;
 }
 return 1;
}

static BOOL StretchBlt(HDC dest, int, int, int, int, HDC source, int, int, int sourceWide, int sourceTall, DWORD rop)
{
 GdiObject* target = Find(dest, GDI_DC);
 GdiObject* from = Find(source, GDI_DC);
 Check(target != NULL && from != NULL && rop == SRCCOPY, "blit between live DCs");
 GdiObject* bitmap = Find(reinterpret_cast<HGDIOBJ>(from->selected), GDI_BITMAP);
 Check(bitmap != NULL && !bitmap->stock, "blit source is a texture bitmap");
 Check(bitmap->wide == sourceWide && bitmap->tall == sourceTall, "blit uses the texture size");
 ++blits;
 lastBlitSource = from->selected;
 return 1;
}

// Deleting a DC deselects whatever it holds.
static BOOL DeleteDC(HDC dc)
{
 GdiObject* object = Find(dc, GDI_DC);
 Check(object != NULL, "DC deleted once");
 object->alive = false;
 return 1;
}

static HRGN CreateRectRgn(int, int, int, int)
{
 if (failCreateRegion) return NULL;
 return reinterpret_cast<HRGN>(Add(GDI_REGION, false, 0, 0));
}

struct WNDCLASSA
{
 UINT style;
 LONG_PTR (*lpfnWndProc)(HWND, UINT, LONG_PTR, LONG_PTR);
 int cbClsExtra;
 int cbWndExtra;
 HINSTANCE hInstance;
 void* hIcon;
 void* hCursor;
 void* hbrBackground;
 const char* lpszMenuName;
 const char* lpszClassName;
};

static LONG_PTR SurfaceWindowProc(HWND, UINT, LONG_PTR, LONG_PTR) { return 0; }
static void InitializeInputTables() {}
static HINSTANCE GetModuleHandleA(const char*) { return NULL; }
static unsigned short RegisterClassA(const WNDCLASSA* windowClass)
{
 Check(windowClass->lpfnWndProc == SurfaceWindowProc, "window procedure registered");
 return 1;
}

static HWND CreateWindowExA(DWORD, const char*, const char*, DWORD, int, int, int wide, int tall, HWND, void*,
 HINSTANCE, void*)
{
 if (failCreateWindow) return NULL;
 return reinterpret_cast<HWND>(Add(GDI_WINDOW, false, wide, tall));
}

static LONG_PTR SetWindowLongPtrA(HWND window, int index, LONG_PTR value)
{
 GdiObject* object = Find(window, GDI_WINDOW);
 Check(object != NULL && index == GWLP_USERDATA, "user data on a live window");
 const LONG_PTR previous = object->userData;
 object->userData = value;
 return previous;
}

static BOOL DestroyWindow(HWND window)
{
 GdiObject* object = Find(window, GDI_WINDOW);
 Check(object != NULL, "window destroyed once");
 if (object->userData != 0) ++destroyedAttachedWindows;
 object->alive = false;
 return 1;
}

// The window DC is a DC object until it is released.
static HDC GetDC(HWND window)
{
 Check(Find(window, GDI_WINDOW) != NULL, "DC of a live window");
 if (failGetDC) return NULL;
 return CreateCompatibleDC(NULL);
}

static int ReleaseDC(HWND window, HDC dc)
{
 Check(Find(window, GDI_WINDOW) != NULL, "DC released before its window is destroyed");
 return DeleteDC(dc);
}

static int SetBkMode(HDC, int) { return 0; }
static UINT SetTextAlign(HDC, UINT) { return 0; }

static long ChangeDisplaySettingsA(void* mode, DWORD flags)
{
 Check(mode == NULL && flags == 0, "display mode restored");
 ++displayRestores;
 return 0;
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
 void getBounds(int& x, int& y, int& wide, int& tall) { x = 0; y = 0; getSize(wide, tall); }
 int _wide;
 int _tall;
};

struct SurfacePlat
{
 SurfacePlat() { ++livePlats; }
 ~SurfacePlat() { --livePlats; }
 HWND window;
 HDC drawDC;
 HDC windowDC;
 HDC textureDC;
 HRGN clip;
 HBITMAP bitmap;
 int bitmapSize[2];
 bool isFullscreen;
};

// The SurfaceBase copy is covered by vgui1_surface_mode_info_tests.py and
// vgui1_app_focus_tests.py.
class SurfaceBase
{
public:
 SurfaceBase() {}
 SurfaceBase(const SurfaceBase&) {}
 SurfaceBase& operator=(const SurfaceBase&) { return *this; }
};

class Surface : public SurfaceBase
{
public:
 // Surface::Surface calls createPlat; the tests call recreateContext themselves.
 Surface()
 {
  _panel._wide = 0;
  _panel._tall = 0;
  _created = createPlat();
 }
 Surface(const Surface& other);
 Surface& operator=(const Surface& other);
 ~Surface();
 bool createPlat();
 Panel* getPanel() { return &_panel; }
 void setSize(int wide, int tall) { _panel._wide = wide; _panel._tall = tall; }
 SurfacePlat* plat() { return _plat; }
 bool created() const { return _created; }
 bool recreateContext();
 void popMakeCurrent(Panel* panel);
 void drawSetTexture(int id);
 void drawSetTextureRGBA(int id, const char* rgba, int wide, int tall);
 void drawTexturedRect(int x0, int y0, int x1, int y1);
protected:
 Panel _panel;
 SurfacePlat* _plat;
 bool _created;
};
'''

TESTS = r'''
static const int RESIZE_CYCLES = 32;
static const int SMALL_SIZE[2] = { 160, 120 };
static const int LARGE_SIZE[2] = { 1024, 768 };
static const int MINIMUM_SIZE = 1;
// windowDC and drawDC are created by createPlat
static const int FIXTURE_DCS = 2;
static const int TEXTURE_ID = 2700;
static const int OTHER_TEXTURE_ID = 2701;
static const int TEXTURE_WIDE = 2;
static const int TEXTURE_TALL = 2;
static const int TEXTURE_UPLOADS = 8;
static const char TEXTURE_RGBA[VGUI_RGBA_BYTES * TEXTURE_WIDE * TEXTURE_TALL] = {
 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };

static void UploadAndDraw(Surface& surface, int id, int wide, int tall)
{
 surface.drawSetTextureRGBA(id, TEXTURE_RGBA, wide, tall);
 surface.drawSetTexture(id);
 surface.drawTexturedRect(0, 0, wide, tall);
}

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

// Only the fixture's own surface may still be alive; shared textures are not owned by a Surface.
static void CheckReleased(int sharedTextures)
{
 Check(LiveCount(GDI_WINDOW) == 1, "window destroyed");
 Check(LiveCount(GDI_DC) == FIXTURE_DCS, "window DC released and DCs deleted");
 Check(LiveCount(GDI_REGION) == 1, "clip region deleted");
 Check(LiveCount(GDI_BITMAP) == sharedTextures, "backing bitmap deleted");
 Check(livePlats == 1, "SurfacePlat deleted");
 Check(destroyedAttachedWindows == 0, "window detached before it is destroyed");
 Check(failedDeletes == 0, "every delete succeeded");
}

// Creates and destroys a surface while flag refuses one createPlat resource.
static void CheckCreateFails(bool& flag)
{
 flag = true;
 Surface* failed = new Surface();
 flag = false;
 Check(!failed->created(), "createPlat reports the failure");
 if (failed->plat()->window == NULL) Check(failed->plat()->windowDC == NULL, "no screen DC without a window");
 delete failed;
 CheckReleased(0);
}

static void Run(const std::string& test)
{
 Surface surface;
 SurfacePlat* plat = surface.plat();
 if (test == "pop-deselects-font")
 {
  Surface surface;
  HDC drawDC = surface.plat()->drawDC;
  HGDIOBJ font = reinterpret_cast<HGDIOBJ>(Add(GDI_FONT, false, 0, 0));
  SelectObject(drawDC, font);
  Check(!DeleteObject(font), "a selected font cannot be deleted");
  failedDeletes = 0;
  surface.popMakeCurrent(NULL);
  Check(Find(drawDC, GDI_DC)->font == reinterpret_cast<std::size_t>(GetStockObject(SYSTEM_FONT)),
        "the stock font is selected");
  Check(DeleteObject(font) && LiveCount(GDI_FONT) == 0, "the font is deleted after the paint");
 }
 else if (test == "resize-cycle")
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
 else if (test == "texture-reupload")
 {
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(surface.recreateContext(), "context");
  for (int i = 0; i < TEXTURE_UPLOADS; ++i)
  {
   UploadAndDraw(surface, TEXTURE_ID, TEXTURE_WIDE, TEXTURE_TALL);
   Check(blits == i + 1, "texture drawn");
   // backing bitmap + current texture bitmap
   Check(LiveCount(GDI_BITMAP) == 2, "old texture bitmap deleted");
   Check(failedDeletes == 0, "every delete succeeded");
  }
  const uchar* bits = Find(reinterpret_cast<HGDIOBJ>(lastBlitSource), GDI_BITMAP)->bits;
  for (int i = 0; i < TEXTURE_WIDE * TEXTURE_TALL; ++i)
  {
   const uchar* source = reinterpret_cast<const uchar*>(TEXTURE_RGBA) + VGUI_RGBA_BYTES * i;
   const uchar* pixel = bits + VGUI_RGBA_BYTES * i;
   Check(pixel[0] == source[2] && pixel[1] == source[1] && pixel[2] == source[0] && pixel[3] == source[3],
    "RGBA converted to BGRA");
  }
 }
 else if (test == "texture-selection-restored")
 {
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(surface.recreateContext(), "context");
  const HBITMAP defaultBitmap = Selected(plat->textureDC);
  UploadAndDraw(surface, TEXTURE_ID, TEXTURE_WIDE, TEXTURE_TALL);
  Check(blits == 1, "texture drawn");
  Check(Selected(plat->textureDC) == defaultBitmap, "previous selection restored");
 }
 else if (test == "texture-two-surfaces")
 {
  Surface popup;
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  popup.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(surface.recreateContext() && popup.recreateContext(), "contexts");
  UploadAndDraw(surface, TEXTURE_ID, TEXTURE_WIDE, TEXTURE_TALL);
  popup.drawSetTexture(TEXTURE_ID);
  popup.drawTexturedRect(0, 0, TEXTURE_WIDE, TEXTURE_TALL);
  Check(blits == 2, "texture drawn on both surfaces");
 }
 else if (test == "texture-create-fails")
 {
  surface.setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(surface.recreateContext(), "context");
  UploadAndDraw(surface, TEXTURE_ID, TEXTURE_WIDE, TEXTURE_TALL);
  const std::size_t oldBitmap = lastBlitSource;
  failCreateDIB = true;
  surface.drawSetTextureRGBA(TEXTURE_ID, TEXTURE_RGBA, TEXTURE_WIDE * 2, TEXTURE_TALL * 2);
  surface.drawSetTextureRGBA(OTHER_TEXTURE_ID, TEXTURE_RGBA, TEXTURE_WIDE, TEXTURE_TALL);
  failCreateDIB = false;
  surface.drawSetTexture(TEXTURE_ID);
  surface.drawTexturedRect(0, 0, TEXTURE_WIDE, TEXTURE_TALL);
  Check(blits == 2 && lastBlitSource == oldBitmap, "old texture kept and drawable");
  surface.drawSetTexture(OTHER_TEXTURE_ID);
  surface.drawTexturedRect(0, 0, TEXTURE_WIDE, TEXTURE_TALL);
  Check(blits == 2, "failed first upload draws nothing");
  Check(failedDeletes == 0, "every delete succeeded");
 }
 else if (test == "destroy-releases")
 {
  Surface* destroyed = new Surface();
  destroyed->setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(destroyed->recreateContext(), "context");
  UploadAndDraw(*destroyed, TEXTURE_ID, TEXTURE_WIDE, TEXTURE_TALL);
  destroyed->setSize(LARGE_SIZE[0], LARGE_SIZE[1]);
  Check(destroyed->recreateContext(), "context recreated");
  const int sharedTextures = LiveCount(GDI_BITMAP) - 1;
  delete destroyed;
  CheckReleased(sharedTextures);
 }
 else if (test == "destroy-before-context")
 {
  Surface* destroyed = new Surface();
  delete destroyed;
  CheckReleased(0);
 }
 else if (test == "destroy-fullscreen")
 {
  Surface* destroyed = new Surface();
  destroyed->setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(destroyed->recreateContext(), "context");
  destroyed->plat()->isFullscreen = true;
  delete destroyed;
  Check(displayRestores == 1, "display mode restored once");
  CheckReleased(0);
 }
 else if (test == "copy-owns-nothing")
 {
  Surface* original = new Surface();
  original->setSize(SMALL_SIZE[0], SMALL_SIZE[1]);
  Check(original->recreateContext(), "context");
  Surface* copy = new Surface(*original);
  Check(copy->plat() == NULL, "the copy has no window");
  delete copy;
  Check(LiveCount(GDI_WINDOW) == 2 && livePlats == 2, "the original keeps its window");
  delete original;
  CheckReleased(0);
 }
 else if (test == "assign-keeps-window")
 {
  Surface* source = new Surface();
  Surface* target = new Surface();
  SurfacePlat* plat = target->plat();
  *target = *source;
  Check(target->plat() == plat, "the assigned surface keeps its window");
  delete source;
  delete target;
  CheckReleased(0);
 }
 else if (test == "create-window-fails") CheckCreateFails(failCreateWindow);
 else if (test == "create-region-fails") CheckCreateFails(failCreateRegion);
 else if (test == "create-dc-fails") CheckCreateFails(failCreateDC);
 else if (test == "get-dc-fails") CheckCreateFails(failGetDC);
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


def define(source, name):
    return re.search(rf'^#define {name}\b.*$', source, re.M).group(0)


def methods(source, signatures):
    return '\n'.join(block(source, signature, '\n}\n') for signature in signatures)


def main():
    surface = (VGUI / 'win32/Surface.cpp').read_text()
    common_surface = (VGUI / 'Surface.cpp').read_text()
    program = '\n'.join([
        define(surface, 'BITMAP_SPARE_PIXELS'),
        define((VGUI / 'include/VGUI.h').read_text(), 'VGUI_RGBA_BYTES'),
        define((VGUI / 'include/VGUI_Surface.h').read_text(), 'VGUI_TEXTURE_CAPACITY'),
        PRELUDE,
        re.search(r'^const char surfaceClassName\[\].*$', surface, re.M).group(0),
        block(surface, 'struct Texture', 'Texture* currentTexture;'),
        methods(surface, (
            'Texture* FindTexture(',
            'Surface::~Surface()',
            'bool Surface::createPlat()',
            'bool Surface::recreateContext()',
            'void Surface::popMakeCurrent(',
            'void Surface::drawSetTexture(',
            'void Surface::drawSetTextureRGBA(',
            'void Surface::drawTexturedRect(')),
        methods(common_surface, (
            'Surface::Surface( const Surface& other )',
            'Surface& Surface::operator=(')),
        TESTS,
    ])

    scenarios = ['pop-deselects-font', 'resize-cycle', 'texture-dc-reused', 'within-spare', 'minimum-size',
                 'bitmap-create-fails', 'select-fails', 'texture-dc-fails', 'texture-reupload',
                 'texture-selection-restored', 'texture-two-surfaces', 'texture-create-fails',
                 'destroy-releases', 'destroy-before-context', 'destroy-fullscreen', 'copy-owns-nothing',
                 'assign-keeps-window',
                 'create-window-fails', 'create-region-fails', 'create-dc-fails', 'get-dc-fails']
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-gdi-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(program)
        executable = compile_program([source_file], executable, debug=True, sanitizers=['address'])
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
