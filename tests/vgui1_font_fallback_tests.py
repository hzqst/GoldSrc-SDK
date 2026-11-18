"""Compile the production VGUI1 bitmap-font loader and Font::init fallback (#244).

Run: python tests/vgui1_font_fallback_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it.

common/fileimage.cpp and common/vfontdata.cpp are compiled as-is. Font::init and
the Font getters are extracted from the Win32 and Linux sources and run against
stub system/bitmap font platforms whose bitmap Create uses the real loader.

- loader-*: LoadVFontDataFrom32BitTGA accepts a valid 256-glyph RLE TGA and
  rejects truncated, header-only, empty, too narrow and wrong-depth data.
- {win32,linux}-valid: valid bitmap data creates a bitmap font.
- {win32,linux}-invalid / -empty: truncated or zero-length data falls back to a
  system font with the requested metrics instead of leaving _plat unset.
- {win32,linux}-null-data: no data creates a system font as before.
- {win32,linux}-reuse: two fallbacks with the same parameters share one system
  font platform and its id.
- {win32,linux}-shared-id: fonts reusing different platforms get those
  platforms' ids instead of all getting -1, so the engine's per-id glyph cache
  cannot mix them up.
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


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


PRELUDE = r'''
#include "fileimage.h"
#include "vfontdata.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#define VGUI_FIRST_FONT_ID 100

using uchar = unsigned char;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}
'''

# Shared by both platforms; DAR, IDS and NEXT_ID are replaced with each source's
# platform-list, platform-id-list and id-counter names.
PLATFORM_STUBS = r'''
template<class T> class Dar
{
public:
 int getCount() const { return static_cast<int>(_items.size()); }
 void addElement(const T& item) { _items.push_back(item); }
 T& operator[](int index) { return _items.at(index); }
private:
 std::vector<T> _items;
};

static char* CopyName(const char* name)
{
 char* copy = new char[std::strlen(name) + 1];
 std::strcpy(copy, name);
 return copy;
}
static char* _strdup(const char* name) { return CopyName(name); }
static char* strdup(const char* name) { return CopyName(name); }

class BaseFontPlat
{
public:
 virtual ~BaseFontPlat() {}
 virtual bool equals(const char* name, int tall, int wide, float rotation, int weight,
  bool italic, bool underline, bool strikeout, bool symbol) = 0;
 virtual void getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba) = 0;
 virtual void getCharABCwide(int ch, int& a, int& b, int& c) = 0;
 virtual int getTall() = 0;
};

class FontPlat_Bitmap : public BaseFontPlat
{
public:
 static FontPlat_Bitmap* Create(const char*, FileImageStream* stream)
 {
  FontPlat_Bitmap* font = new FontPlat_Bitmap();
  if (!LoadVFontDataFrom32BitTGA(stream, &font->data))
  {
   delete font;
   return NULL;
  }
  return font;
 }
 bool equals(const char*, int, int, float, int, bool, bool, bool, bool) override { return false; }
 void getCharRGBA(int, int, int, int, int, uchar*) override {}
 void getCharABCwide(int ch, int& a, int& b, int& c) override { a = c = 0; b = data.m_CharWidths[ch] + 1; }
 int getTall() override { return data.m_BitmapCharHeight; }
 VFontData data;
};

class FontPlat : public BaseFontPlat
{
public:
 FontPlat(const char* name, int tall, int wide, float rotation, int weight,
  bool italic, bool underline, bool strikeout, bool symbol)
  : _name(name), _tall(tall), _wide(wide), _rotation(rotation), _weight(weight),
    _italic(italic), _underline(underline), _strikeout(strikeout), _symbol(symbol)
 {
 }
 bool equals(const char* name, int tall, int wide, float rotation, int weight,
  bool italic, bool underline, bool strikeout, bool symbol) override
 {
  return _name == name && _tall == tall && _wide == wide && _rotation == rotation && _weight == weight
   && _italic == italic && _underline == underline && _strikeout == strikeout && _symbol == symbol;
 }
 void getCharRGBA(int, int, int, int, int, uchar*) override {}
 void getCharABCwide(int, int& a, int& b, int& c) override { a = c = 0; b = _tall / 2; }
 int getTall() override { return _tall; }
private:
 std::string _name;
 int _tall, _wide;
 float _rotation;
 int _weight;
 bool _italic, _underline, _strikeout, _symbol;
};

static Dar<BaseFontPlat*> DAR;
static Dar<int> IDS;
static int NEXT_ID = VGUI_FIRST_FONT_ID;

class Font
{
public:
 Font(const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
  bool italic, bool underline, bool strikeout, bool symbol)
 {
  init(name, data, length, tall, wide, rotation, weight, italic, underline, strikeout, symbol);
 }
 ~Font() { delete[] _name; }
 void init(const char* name, void* data, int length, int tall, int wide, float rotation, int weight,
  bool italic, bool underline, bool strikeout, bool symbol);
 void getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba);
 void getCharABCwide(int ch, int& a, int& b, int& c);
 int getTall();
 char* _name;
 BaseFontPlat* _plat;
 int _id;
};

// Font::init references the platforms it uses; FreePlatforms frees them.
static void AddReference(BaseFontPlat*) {}

static void FreePlatforms()
{
 for (int index = 0; index < DAR.getCount(); ++index) delete DAR[index];
}
'''

TESTS = r'''
constexpr int GLYPH_HEIGHT = 2;
constexpr int SYSTEM_TALL = 14;
constexpr int SYSTEM_WEIGHT = 400;
constexpr int TGA_HEADER_BYTES = 18;
constexpr int TGA_RLE_IMAGE = 10;
constexpr int TGA_PIXEL_BITS = 32;
constexpr int TGA_PACKET_PIXELS = 128;
constexpr int TGA_PIXEL_BYTES = 4;

// A valid 32-bit RLE TGA with one pixel column per glyph, written as raw packets.
static std::vector<uchar> BuildFontTGA(int width, int height, int pixelBits = TGA_PIXEL_BITS)
{
 std::vector<uchar> tga(TGA_HEADER_BYTES, 0);
 tga[2] = TGA_RLE_IMAGE;
 tga[12] = static_cast<uchar>(width & 0xff);
 tga[13] = static_cast<uchar>(width >> 8);
 tga[14] = static_cast<uchar>(height & 0xff);
 tga[15] = static_cast<uchar>(height >> 8);
 tga[16] = static_cast<uchar>(pixelBits);
 for (int y = 0; y < height; ++y)
 {
  for (int x = 0; x < width; x += TGA_PACKET_PIXELS)
  {
   const int count = width - x < TGA_PACKET_PIXELS ? width - x : TGA_PACKET_PIXELS;
   tga.push_back(static_cast<uchar>(count - 1));
   for (int i = 0; i < count * TGA_PIXEL_BYTES; ++i) tga.push_back(0xff);
  }
 }
 return tga;
}

static bool Loads(std::vector<uchar> bytes, int length)
{
 // Copy into an exactly sized heap block so ASan catches reads past the data.
 std::vector<uchar> copy(bytes.begin(), bytes.begin() + length);
 FileImageStream_Memory stream(length ? copy.data() : bytes.data(), length);
 VFontData data;
 return LoadVFontDataFrom32BitTGA(&stream, &data);
}

template<class FontType, class BitmapType, class SystemType>
static void CheckFont(const std::string& test)
{
 std::vector<uchar> tga = BuildFontTGA(VFONT_GLYPH_COUNT, GLYPH_HEIGHT);
 const int truncated = static_cast<int>(tga.size()) - TGA_PIXEL_BYTES;
 int a = -1, b = -1, c = -1;
 if (test == "valid")
 {
  FontType font("Arial", tga.data(), static_cast<int>(tga.size()), SYSTEM_TALL, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  Check(nullptr != dynamic_cast<BitmapType*>(font._plat), "valid data creates a bitmap font");
  Check(font._id >= VGUI_FIRST_FONT_ID, "bitmap font gets an id");
  Check(GLYPH_HEIGHT == font.getTall(), "bitmap tall");
 }
 else if (test == "invalid" || test == "empty")
 {
  const int length = test == "empty" ? 0 : truncated;
  FontType font("Arial", tga.data(), length, SYSTEM_TALL, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  Check(nullptr != dynamic_cast<SystemType*>(font._plat), "invalid data falls back to a system font");
  Check(font._id >= VGUI_FIRST_FONT_ID, "fallback font gets an id");
  Check(SYSTEM_TALL == font.getTall(), "fallback tall");
  font.getCharABCwide('A', a, b, c);
  Check(SYSTEM_TALL / 2 == b, "fallback metrics");
 }
 else if (test == "null-data")
 {
  FontType font("Arial", nullptr, 0, SYSTEM_TALL, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  Check(nullptr != dynamic_cast<SystemType*>(font._plat), "no data creates a system font");
  Check(SYSTEM_TALL == font.getTall(), "system tall");
 }
 else if (test == "reuse")
 {
  FontType first("Tahoma", tga.data(), truncated, SYSTEM_TALL, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  FontType second("Tahoma", tga.data(), truncated, SYSTEM_TALL, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  Check(nullptr != dynamic_cast<SystemType*>(first._plat), "first fallback is a system font");
  Check(first._plat == second._plat, "matching fallbacks share one platform");
  Check(first._id >= VGUI_FIRST_FONT_ID && second._id == first._id, "matching fallbacks share the platform id");
 }
 else if (test == "shared-id")
 {
  FontType small("Arial", nullptr, 0, SYSTEM_TALL, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  FontType large("Arial", nullptr, 0, SYSTEM_TALL * 2, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  FontType smallAgain("Arial", nullptr, 0, SYSTEM_TALL, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  FontType largeAgain("Arial", nullptr, 0, SYSTEM_TALL * 2, 0, 0, SYSTEM_WEIGHT, false, false, false, false);
  Check(small._id >= VGUI_FIRST_FONT_ID && large._id >= VGUI_FIRST_FONT_ID, "new platforms get ids");
  Check(small._id != large._id, "different platforms get different ids");
  Check(smallAgain._plat == small._plat && smallAgain._id == small._id, "reused small platform keeps its id");
  Check(largeAgain._plat == large._plat && largeAgain._id == large._id, "reused large platform keeps its id");
 }
 else
 {
  Check(false, "unknown font scenario");
 }
}

int main(int argc, char** argv)
{
 try
 {
  Check(argc == 2, "usage: test <scenario>");
  const std::string test = argv[1];
  std::vector<uchar> tga = BuildFontTGA(VFONT_GLYPH_COUNT, GLYPH_HEIGHT);
  const int size = static_cast<int>(tga.size());
  if (test == "loader-valid") Check(Loads(tga, size), "valid TGA loads");
  else if (test == "loader-truncated") Check(!Loads(tga, size - TGA_PIXEL_BYTES), "truncated pixels rejected");
  else if (test == "loader-header-only") Check(!Loads(tga, TGA_HEADER_BYTES), "header without pixels rejected");
  else if (test == "loader-empty") Check(!Loads(tga, 0), "empty data rejected");
  else if (test == "loader-narrow")
  {
   std::vector<uchar> narrow = BuildFontTGA(VFONT_GLYPH_COUNT / 2, GLYPH_HEIGHT);
   Check(!Loads(narrow, static_cast<int>(narrow.size())), "narrower than one column per glyph rejected");
  }
  else if (test == "loader-zero-height")
  {
   std::vector<uchar> flat = BuildFontTGA(VFONT_GLYPH_COUNT, 0);
   Check(!Loads(flat, static_cast<int>(flat.size())), "zero height rejected");
  }
  else if (test == "loader-bad-depth")
  {
   std::vector<uchar> rgb = BuildFontTGA(VFONT_GLYPH_COUNT, GLYPH_HEIGHT, 24);
   Check(!Loads(rgb, static_cast<int>(rgb.size())), "24-bit TGA rejected");
  }
  else if (test.rfind("win32-", 0) == 0)
  {
   CheckFont<win32::Font, win32::FontPlat_Bitmap, win32::FontPlat>(test.substr(6));
   win32::FreePlatforms();
  }
  else if (test.rfind("linux-", 0) == 0)
  {
   CheckFont<vgui_linux::Font, vgui_linux::FontPlat_Bitmap, vgui_linux::FontPlat>(test.substr(6));
   vgui_linux::FreePlatforms();
  }
  else Check(false, "unknown scenario");
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


def font_methods(source):
    return '\n'.join(block(source, signature, '\n}\n') for signature in (
        'void Font::init(',
        'void Font::getCharRGBA(',
        'void Font::getCharABCwide(',
        'int Font::getTall()'))


def platform(namespace, source, dar, ids, next_id):
    stubs = PLATFORM_STUBS.replace('DAR', dar).replace('IDS', ids).replace('NEXT_ID', next_id)
    return f'namespace {namespace}\n{{\n{stubs}\n{font_methods(source)}\n}}\n'


def main():
    win32 = (VGUI / 'win32/Font.cpp').read_text()
    linux = (VGUI / 'linux/Font.cpp').read_text()
    program = '\n'.join([
        PRELUDE,
        platform('win32', win32, 'fontPlatforms', 'fontPlatformIds', 'nextFontId'),
        platform('vgui_linux', linux, 'staticFontPlatDar', 'staticFontPlatIdDar', 'staticFontId'),
        TESTS,
    ])

    scenarios = ['loader-valid', 'loader-truncated', 'loader-header-only', 'loader-empty',
                 'loader-narrow', 'loader-zero-height', 'loader-bad-depth']
    for prefix in ('win32', 'linux'):
        scenarios += [f'{prefix}-{name}' for name in ('valid', 'invalid', 'empty', 'null-data', 'reuse', 'shared-id')]
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-fallback-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(program)
        sources = [source_file, VGUI / 'common/fileimage.cpp', VGUI / 'common/vfontdata.cpp']
        executable = compile_program(sources, executable, includes=[VGUI / 'include', VGUI / 'common'], debug=True,
                                     sanitizers=['address'])
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
