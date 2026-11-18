"""Compile the VGUI1 Win32 and Linux Font and check what it owns (#213, #214).

Run: python tests/vgui1_font_ownership_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it.

win32/Font.cpp, Font.cpp, common/fileimage.cpp and common/vfontdata.cpp are
compiled as-is against a stub windows.h whose GDI calls track live fonts, DCs
and bitmaps and what each DC has selected. As in GDI, deleting an object that
is still selected into a live DC fails and leaves it alive; deleting a DC
deselects what it holds. linux/Font.cpp is built the same way with the
engine's utlrbtree.h, on hosts other than Windows; its runs check memory errors,
leaks and the shared platforms, not GDI objects. vgui_strdup and vgui_strcpy
are extracted from vgui.cpp. Each platform is built twice: with VGUI_ABI_COMPAT_MODE=1 (the original vgui.dll layout, no
destructors) and with VGUI_ABI_COMPAT_MODE=0, where a Font owns its name and
holds a reference to its platform font, as does the platform list until
Font_Reset. The compatible build frees nothing, so its runs check memory
errors only.

- reuse: fonts with the same parameters share one platform font and its id;
  other parameters get another.
With VGUI_ABI_COMPAT_MODE=0 only:
- reset-frees-unused: Font_Reset frees the platform fonts no Font uses, with
  their GDI font, DC and DIB, and every GDI delete succeeds.
- reset-keeps-used: a platform font still used survives Font_Reset; a font
  made after it gets a new platform and id, and each platform is freed with
  its last Font.
- shared-platform: a platform shared by two fonts is freed with the second.
- copy: a copied or assigned Font has its own name and shares the platform
  and id; assignment releases the platform it replaces, and each is freed
  once.
- bitmap-font: deleting a bitmap font and resetting frees its bitmap platform
  font.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program
from vgui1_panel_tests import ROOT, VGUI, function

# Only the declarations win32/Font.cpp and vgui_win32.h use.
WINDOWS_H = r'''
#ifndef TEST_WINDOWS_H
#define TEST_WINDOWS_H

#include <cstddef>
#include <cstdlib>
#include <cstring>
#if !defined( _MSC_VER )
#include <strings.h>
static inline char* _strdup( const char* text ) { return strdup( text ); }
static inline int _stricmp( const char* a, const char* b ) { return strcasecmp( a, b ); }
#endif

typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef DWORD COLORREF;
typedef void* HANDLE;
typedef void* HGDIOBJ;
typedef struct HWND__* HWND;
typedef struct HDC__* HDC;
typedef struct HGLRC__* HGLRC;
typedef struct HRGN__* HRGN;
typedef struct HBITMAP__* HBITMAP;
typedef struct HFONT__* HFONT;

#define RGB( r, g, b ) ( (COLORREF)( ( r ) | ( ( g ) << 8 ) | ( ( b ) << 16 ) ) )
static const DWORD ANSI_CHARSET = 0;
static const DWORD SYMBOL_CHARSET = 2;
static const DWORD OUT_DEFAULT_PRECIS = 0;
static const DWORD CLIP_DEFAULT_PRECIS = 0;
static const DWORD DEFAULT_QUALITY = 0;
static const DWORD DEFAULT_PITCH = 0;
static const UINT TA_UPDATECP = 1;
static const int TRANSPARENT = 1;
static const int OPAQUE = 2;
static const DWORD BI_RGB = 0;
static const UINT DIB_RGB_COLORS = 0;

struct TEXTMETRICA
{
 long tmHeight;
 long tmAscent;
 long tmDescent;
 long tmAveCharWidth;
 long tmMaxCharWidth;
};

struct ABC
{
 int abcA;
 UINT abcB;
 int abcC;
};

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

struct POINT;
struct RECT;

HFONT CreateFontA( int tall, int wide, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
 DWORD strikeout, DWORD charset, DWORD precision, DWORD clip, DWORD quality, DWORD pitch, const char* name );
HDC CreateCompatibleDC( HDC dc );
HGDIOBJ SelectObject( HDC dc, HGDIOBJ object );
UINT SetTextAlign( HDC dc, UINT align );
BOOL GetTextMetricsA( HDC dc, TEXTMETRICA* metrics );
HBITMAP CreateDIBSection( HDC dc, const BITMAPINFO* info, UINT usage, void** bits, HANDLE section, DWORD offset );
BOOL GetCharABCWidthsA( HDC dc, UINT first, UINT last, ABC* abc );
COLORREF SetBkColor( HDC dc, COLORREF color );
COLORREF SetTextColor( HDC dc, COLORREF color );
int SetBkMode( HDC dc, int mode );
BOOL MoveToEx( HDC dc, int x, int y, POINT* point );
BOOL ExtTextOutA( HDC dc, int x, int y, UINT options, const RECT* rect, const char* text, UINT count, const int* dx );
BOOL DeleteDC( HDC dc );
BOOL DeleteObject( HGDIOBJ object );

#endif
'''

TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#if TEST_WIN32
#include <windows.h>
#endif

// The test reads the name, platform and id a Font keeps.
#define private public
#define protected public
#include "VGUI_Font.h"
#undef protected
#undef private

using namespace vgui;

static void Check(bool condition, const char* message)
{
 if (!condition)
  throw std::runtime_error(message);
}

static const int FONT_TALL = 14;
static const int FONT_WEIGHT = 400;

#if TEST_WIN32
enum GdiKind { GDI_FONT, GDI_DC, GDI_BITMAP };

struct GdiObject
{
 GdiKind kind;
 bool alive;
 std::size_t font;
 std::size_t bitmap;
 unsigned char* bits;
};

static std::vector<GdiObject> objects;
static int failedDeletes;

static std::size_t Add(GdiKind kind)
{
 GdiObject object = { kind, true, 0, 0, NULL };
 objects.push_back(object);
 return objects.size();
}

static GdiObject* Find(const void* handle)
{
 const std::size_t index = reinterpret_cast<std::size_t>(handle);
 if (index == 0 || index > objects.size() || !objects[index - 1].alive)
  return NULL;
 return &objects[index - 1];
}

static bool IsSelected(std::size_t handle)
{
 for (const GdiObject& object : objects)
 {
  if (object.alive && object.kind == GDI_DC && (object.font == handle || object.bitmap == handle))
   return true;
 }
 return false;
}

static int LiveCount(GdiKind kind)
{
 int count = 0;
 for (const GdiObject& object : objects)
 {
  if (object.alive && object.kind == kind)
   ++count;
 }
 return count;
}

HFONT CreateFontA(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const char*)
{
 return reinterpret_cast<HFONT>(Add(GDI_FONT));
}

HDC CreateCompatibleDC(HDC)
{
 return reinterpret_cast<HDC>(Add(GDI_DC));
}

HGDIOBJ SelectObject(HDC dc, HGDIOBJ handle)
{
 GdiObject* target = Find(dc);
 GdiObject* object = Find(handle);
 Check(target != NULL && target->kind == GDI_DC && object != NULL && object->kind != GDI_DC, "select into a live DC");
 std::size_t& slot = object->kind == GDI_FONT ? target->font : target->bitmap;
 const std::size_t previous = slot;
 slot = reinterpret_cast<std::size_t>(handle);
 return reinterpret_cast<HGDIOBJ>(previous);
}

UINT SetTextAlign(HDC, UINT) { return 0; }

BOOL GetTextMetricsA(HDC, TEXTMETRICA* metrics)
{
 std::memset(metrics, 0, sizeof(*metrics));
 metrics->tmHeight = FONT_TALL;
 metrics->tmAscent = FONT_TALL;
 metrics->tmAveCharWidth = FONT_TALL / 2;
 metrics->tmMaxCharWidth = FONT_TALL;
 return 1;
}

HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO* info, UINT, void** bits, HANDLE, DWORD)
{
 Check(Find(dc) != NULL, "DIB for a live DC");
 const std::size_t handle = Add(GDI_BITMAP);
 objects[handle - 1].bits = new unsigned char[VGUI_RGBA_BYTES * info->bmiHeader.biWidth * -info->bmiHeader.biHeight];
 *bits = objects[handle - 1].bits;
 return reinterpret_cast<HBITMAP>(handle);
}

BOOL GetCharABCWidthsA(HDC, UINT, UINT, ABC*) { return 0; }
COLORREF SetBkColor(HDC, COLORREF) { return 0; }
COLORREF SetTextColor(HDC, COLORREF) { return 0; }
int SetBkMode(HDC, int) { return 0; }
BOOL MoveToEx(HDC, int, int, POINT*) { return 1; }
BOOL ExtTextOutA(HDC, int, int, UINT, const RECT*, const char*, UINT, const int*) { return 1; }

// Deleting a DC deselects whatever it holds.
BOOL DeleteDC(HDC dc)
{
 GdiObject* object = Find(dc);
 Check(object != NULL && object->kind == GDI_DC, "DC deleted once");
 object->alive = false;
 return 1;
}

BOOL DeleteObject(HGDIOBJ handle)
{
 GdiObject* object = Find(handle);
 if (object == NULL || object->kind == GDI_DC || IsSelected(reinterpret_cast<std::size_t>(handle)))
 {
  ++failedDeletes;
  return 0;
 }
 object->alive = false;
 delete[] object->bits;
 object->bits = NULL;
 return 1;
}

#if !defined( _WIN32 )
int Font::getWide() { return 0; }
#endif

// Each system platform font holds one GDI font, one DC and one DIB.
static void CheckLivePlatforms(int count, const char* message)
{
 Check(count == LiveCount(GDI_FONT) && count == LiveCount(GDI_DC) && count == LiveCount(GDI_BITMAP), message);
}

// Reads the platform font, so ASan reports one that was freed too early.
static void CheckTall(Font* font, const char* message)
{
 Check(FONT_TALL == font->getTall(), message);
}
#else
// The Linux platform fonts hold no system resources; ASan reports leaks.
static void CheckLivePlatforms(int, const char*) {}
static int failedDeletes;

// Reads the platform font, so ASan reports one that was freed too early.
static void CheckTall(Font* font, const char* message)
{
 Check(font->getTall() >= 0, message);
}
#endif

static const int TGA_HEADER_BYTES = 18;
static const int TGA_RLE_IMAGE = 10;
static const int TGA_PIXEL_BITS = 32;
static const int TGA_PACKET_PIXELS = 128;
static const int TGA_PIXEL_BYTES = 4;
static const int GLYPH_HEIGHT = 2;
static const int GLYPH_COUNT = 256;

// A 32-bit RLE TGA with one pixel column per glyph, written as raw packets.
static std::vector<unsigned char> BuildFontTGA()
{
 std::vector<unsigned char> tga(TGA_HEADER_BYTES, 0);
 tga[2] = TGA_RLE_IMAGE;
 tga[12] = GLYPH_COUNT & 0xff;
 tga[13] = GLYPH_COUNT >> 8;
 tga[14] = GLYPH_HEIGHT;
 tga[16] = TGA_PIXEL_BITS;
 for (int y = 0; y < GLYPH_HEIGHT; ++y)
 {
  for (int x = 0; x < GLYPH_COUNT; x += TGA_PACKET_PIXELS)
  {
   tga.push_back(TGA_PACKET_PIXELS - 1);
   for (int i = 0; i < TGA_PACKET_PIXELS * TGA_PIXEL_BYTES; ++i)
    tga.push_back(0xff);
  }
 }
 return tga;
}

static Font* MakeFont(const char* name, int tall = FONT_TALL)
{
 return new Font(name, NULL, 0, tall, 0, 0.0f, FONT_WEIGHT, false, false, false, false);
}

static void Run(const char* scenario)
{
 if (!std::strcmp(scenario, "reuse"))
 {
  Font* first = MakeFont("Arial");
  Font* second = MakeFont("Arial");
  Font* other = MakeFont("Arial", FONT_TALL * 2);
  Check(first->_plat == second->_plat && first->_id == second->_id, "same parameters share a platform");
  Check(first->_plat != other->_plat && first->_id != other->_id, "other parameters get another platform");
  Check(first->_name != second->_name && 0 == std::strcmp("Arial", second->_name), "each font has its name");
  CheckTall(first, "platform metrics");
  CheckLivePlatforms(2, "two platforms");
#if VGUI_ABI_COMPAT_MODE != 1
  delete first;
  delete second;
  delete other;
  Font_Reset();
  CheckLivePlatforms(0, "platforms freed");
#endif
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (!std::strcmp(scenario, "reset-frees-unused"))
 {
  delete MakeFont("Arial");
  CheckLivePlatforms(1, "the platform list keeps an unused platform");
  Font_Reset();
  CheckLivePlatforms(0, "Font_Reset frees the unused platform");
 }
 else if (!std::strcmp(scenario, "reset-keeps-used"))
 {
  Font* before = MakeFont("Arial");
  Font_Reset();
  CheckLivePlatforms(1, "a used platform survives Font_Reset");
  CheckTall(before, "the used platform still works");

  Font* after = MakeFont("Arial");
  Check(after->_plat != before->_plat && after->_id != before->_id, "a font after Font_Reset gets a new platform");
  CheckLivePlatforms(2, "old and new platforms");
  delete before;
  CheckLivePlatforms(1, "the old platform is freed with its last font");
  delete after;
  CheckLivePlatforms(1, "the platform list keeps the new platform");
  Font_Reset();
  CheckLivePlatforms(0, "all platforms freed");
 }
 else if (!std::strcmp(scenario, "shared-platform"))
 {
  Font* first = MakeFont("Arial");
  Font* second = MakeFont("Arial");
  Font_Reset();
  delete first;
  CheckLivePlatforms(1, "a shared platform survives its first font");
  CheckTall(second, "the second font still works");
  delete second;
  CheckLivePlatforms(0, "a shared platform is freed with its last font");
 }
 else if (!std::strcmp(scenario, "copy"))
 {
  Font* original = MakeFont("Arial");
  Font* copy = new Font(*original);
  Check(copy->_name != original->_name && 0 == std::strcmp("Arial", copy->_name), "copy has its own name");
  Check(copy->_plat == original->_plat && copy->_id == original->_id, "copy shares the platform and id");

  Font* other = MakeFont("Tahoma");
  Font_Reset();
  *other = *copy;
  CheckLivePlatforms(1, "assignment releases the platform it replaces");
  Check(other->_plat == original->_plat && 0 == std::strcmp("Arial", other->_name), "assigned font");

  Font& alias = *other;
  *other = alias;
  Check(other->_plat == original->_plat && 0 == std::strcmp("Arial", other->_name), "self assignment");

  delete original;
  CheckTall(copy, "copy outlives the original");
  delete copy;
  delete other;
  CheckLivePlatforms(0, "copied fonts free their platform once");
 }
 else if (!std::strcmp(scenario, "bitmap-font"))
 {
  std::vector<unsigned char> tga = BuildFontTGA();
  Font* font = new Font("Bitmap", tga.data(), static_cast<int>(tga.size()), FONT_TALL, 0, 0.0f, FONT_WEIGHT,
                        false, false, false, false);
  Check(GLYPH_HEIGHT == font->getTall(), "bitmap font loaded");
  CheckLivePlatforms(0, "a bitmap font uses no GDI objects");
  delete font;
  Font_Reset();
 }
#endif
 else Check(false, "unknown scenario");

 Check(0 == failedDeletes, "every GDI delete succeeds");
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

COMMON_SOURCES = ['Font.cpp', 'common/fileimage.cpp', 'common/vfontdata.cpp']
# Platform -> its Font source and include directories past the shared ones.
PLATFORMS = {'win32': ('win32/Font.cpp', [VGUI / 'win32']),
             'linux': ('linux/Font.cpp', [VGUI / 'linux', ROOT / 'GoldSrc-SDK/engine'])}
SCENARIOS = ['reuse']
NONCOMPAT_SCENARIOS = ['reset-frees-unused', 'reset-keeps-used', 'shared-platform', 'copy', 'bitmap-font']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + NONCOMPAT_SCENARIOS}


def main():
    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-font-') as folder:
        windows = Path(folder) / 'windows'
        windows.mkdir()
        (windows / 'windows.h').write_text(WINDOWS_H)
        test_file = Path(folder) / 'test.cpp'
        vgui = (VGUI / 'vgui.cpp').read_text()
        helpers = function(vgui, 'char* vgui_strdup(') + '\n' + function(vgui, 'void vgui_strcpy(')
        test_file.write_text(TESTS + '\nnamespace vgui\n{\n' + helpers + '\n}\n')
        # linux/Font.cpp uses POSIX string functions.
        platforms = ['win32'] if sys.platform == 'win32' else ['win32', 'linux']
        for platform in platforms:
            source, includes = PLATFORMS[platform]
            sources = [test_file, VGUI / source] + [VGUI / name for name in COMMON_SOURCES]
            for mode, scenarios in MODES.items():
                executable = Path(folder) / f'test-{platform}-compat{mode}.exe'
                # VGUI_EXPORTS matches the vgui target, so VGUIAPI is not dllimport on Windows.
                executable = compile_program(sources, executable,
                                             includes=[windows, VGUI / 'include', VGUI / 'common'] + includes,
                                             defines=['VGUI_EXPORTS', f'VGUI_ABI_COMPAT_MODE={mode}',
                                                      f'TEST_WIN32={int(platform == "win32")}'],
                                             debug=True, sanitizers=['address'])
                for scenario in scenarios:
                    # Without destructors the compatible build frees nothing;
                    # only check its memory errors.
                    env = dict(os.environ)
                    if mode == '1':
                        env['ASAN_OPTIONS'] = 'detect_leaks=0:' + env.get('ASAN_OPTIONS', '')
                    result = subprocess.run([str(executable), scenario], env=env)
                    if result.returncode:
                        failed.append(f'{platform} {scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
