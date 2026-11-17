"""Compile the production VGUI1 glyph lookups with deterministic stubs (#245).

Run: python tests/vgui_font_glyph_tests.py
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).
AddressSanitizer is enabled when the compiler supports it.

Text bytes reach the font platforms as sign-extended chars, and callers may pass
any int. Each scenario asserts the Win32 lookups stay inside the 256-glyph range:

- normalize: NormalizeGlyphIndex maps -128..-1 to 0x80..0xFF and clamps the rest
  into 0..255.
- bitmap-widths: FontPlat_Bitmap::getCharABCwide uses the same byte glyph.
- bitmap-rgba: FontPlat_Bitmap::getCharRGBA renders glyph 255 for 256 and -1
  instead of reading one glyph past the bitmap.
"""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


STUBS = r'''
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#define GLYPH_COUNT 256
#define VFONT_GLYPH_COUNT 256
#define VGUI_RGBA_BYTES 4

using uchar = unsigned char;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

namespace vgui
{
class SurfacePlat;
class FileImageStream;

class VFontData
{
public:
 int m_CharWidths[VFONT_GLYPH_COUNT];
 int m_BitmapCharWidth;
 int m_BitmapCharHeight;
 unsigned char* m_pBitmap;
};

class BaseFontPlat
{
public:
 virtual ~BaseFontPlat() {}
 virtual bool equals(const char*, int, int, float, int, bool, bool, bool, bool) = 0;
 virtual void getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba) = 0;
 virtual void getCharABCwide(int ch, int& a, int& b, int& c) = 0;
 virtual int getTall() = 0;
 virtual void drawSetTextFont(SurfacePlat* surface) = 0;
};

static VFontData* g_loadedFont;
static bool LoadVFontDataFrom32BitTGA(FileImageStream*, VFontData* data)
{
 *data = *g_loadedFont;
 return true;
}
static char* vgui_strdup(const char* src)
{
 char* copy = new char[std::strlen(src) + 1];
 std::strcpy(copy, src);
 return copy;
}
'''

TESTS = r'''
}

using namespace vgui;

constexpr int CHAR_WIDE = 2;
constexpr int CHAR_TALL = 3;
constexpr int LAST_GLYPH = VFONT_GLYPH_COUNT - 1;
constexpr uchar OPAQUE_BYTE = 255;

// Glyph 255 is fully set and every other glyph is empty, so rendering the wrong
// glyph (or the bytes past the bitmap) is visible in the RGBA output.
static FontPlat_Bitmap* CreateBitmapFont(std::vector<uchar>& bitmap, VFontData& data)
{
 bitmap.assign(VFONT_GLYPH_COUNT * CHAR_WIDE * CHAR_TALL, 0);
 for (int y = 0; y < CHAR_TALL; ++y)
 {
  for (int x = 0; x < CHAR_WIDE; ++x)
  {
   bitmap[(y * VFONT_GLYPH_COUNT + LAST_GLYPH) * CHAR_WIDE + x] = 1;
  }
 }
 for (int ch = 0; ch < VFONT_GLYPH_COUNT; ++ch)
 {
  data.m_CharWidths[ch] = ch;
 }
 data.m_BitmapCharWidth = CHAR_WIDE;
 data.m_BitmapCharHeight = CHAR_TALL;
 data.m_pBitmap = bitmap.data();
 g_loadedFont = &data;
 return FontPlat_Bitmap::Create("test", nullptr);
}

static int GlyphWidth(BaseFontPlat* font, int ch)
{
 int a = -1, b = -1, c = -1;
 font->getCharABCwide(ch, a, b, c);
 Check(0 == a && 0 == c, "bitmap glyphs have no A/C spacing");
 return b;
}

static bool RendersGlyph(BaseFontPlat* font, int ch, uchar expected)
{
 std::vector<uchar> rgba(CHAR_WIDE * CHAR_TALL * VGUI_RGBA_BYTES, 0x7f);
 font->getCharRGBA(ch, 0, 0, CHAR_WIDE, CHAR_TALL, rgba.data());
 for (uchar value : rgba)
 {
  if (value != expected) return false;
 }
 return true;
}

int main(int argc, char** argv)
{
 try
 {
  Check(argc == 2, "usage: test <scenario>");
  const std::string test = argv[1];
  // The heap copy is sized exactly, so ASan reports any read past the glyphs.
  std::vector<uchar> bitmap;
  VFontData data{};

  if (test == "normalize")
  {
   Check(0 == NormalizeGlyphIndex(0), "0");
   Check(127 == NormalizeGlyphIndex(127), "127");
   Check(128 == NormalizeGlyphIndex(128), "128");
   Check(255 == NormalizeGlyphIndex(255), "255");
   Check(255 == NormalizeGlyphIndex(-1), "-1 is byte 0xFF");
   Check(128 == NormalizeGlyphIndex(-128), "-128 is byte 0x80");
   Check(0x80 == NormalizeGlyphIndex(static_cast<signed char>(0x80)), "signed byte 0x80");
   Check(255 == NormalizeGlyphIndex(256), "256 clamps to 255");
   Check(255 == NormalizeGlyphIndex(INT_MAX), "INT_MAX clamps to 255");
   Check(0 == NormalizeGlyphIndex(-129), "-129 clamps to 0");
   Check(0 == NormalizeGlyphIndex(INT_MIN), "INT_MIN clamps to 0");
  }
  else if (test == "bitmap-widths")
  {
   FontPlat_Bitmap* font = CreateBitmapFont(bitmap, data);
   Check(1 == GlyphWidth(font, 0), "width 0");
   Check(128 == GlyphWidth(font, 127), "width 127");
   Check(129 == GlyphWidth(font, 128), "width 128");
   Check(256 == GlyphWidth(font, 255), "width 255");
   Check(256 == GlyphWidth(font, -1), "width -1 uses glyph 0xFF");
   Check(129 == GlyphWidth(font, -128), "width -128 uses glyph 0x80");
   Check(256 == GlyphWidth(font, 256), "width 256 uses glyph 255");
   delete font;
  }
  else if (test == "bitmap-rgba")
  {
   FontPlat_Bitmap* font = CreateBitmapFont(bitmap, data);
   Check(RendersGlyph(font, 0, 0), "glyph 0 is empty");
   Check(RendersGlyph(font, 128, 0), "glyph 128 is empty");
   Check(RendersGlyph(font, 255, OPAQUE_BYTE), "glyph 255 is set");
   Check(RendersGlyph(font, -1, OPAQUE_BYTE), "-1 renders glyph 0xFF");
   Check(RendersGlyph(font, 256, OPAQUE_BYTE), "256 renders glyph 255, not past the bitmap");
   delete font;
  }
  else
  {
   Check(false, "unknown scenario");
  }
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
    font = (ROOT / 'GoldSrc-SDK/utils/vgui/lib-src/vgui/win32/Font.cpp').read_text()
    functions = [
        block(font, 'int NormalizeGlyphIndex( int ch )', '\n}\n'),
        block(font, 'class FontPlat_Bitmap : public BaseFontPlat', '\n};\n'),
    ]

    scenarios = ['normalize', 'bitmap-widths', 'bitmap-rgba']
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui-font-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(STUBS + '\n'.join(functions) + TESTS)
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
