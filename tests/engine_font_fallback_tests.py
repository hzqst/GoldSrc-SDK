"""Compile the production engine VGUI1 font loader, EngineFont::Init and the
vgui2 TGA loader with deterministic stubs.

Run: python tests/engine_font_fallback_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it.

engine/vgui2/BaseUISurface_Font.h is included as-is; the stream, font platform,
EngineFont, loader and Font_Reset definitions are extracted verbatim from
BaseUISurface_Font.cpp. The vgui2 stream and Load32BitTGA are extracted from
vgui2/src/fileimage.cpp into their own namespace.

- engine-loader-*, vgui2-loader-*: a valid 256-glyph RLE TGA loads; truncated
  data is rejected (the memory streams pad reads with zeros), and the engine
  font loader also rejects images narrower than one column per glyph.
- engine-valid: valid bitmap data creates a bitmap font with a fresh id.
- engine-invalid / engine-empty: truncated or zero-length data falls back to a
  system font instead of leaving _plat uninitialized.
- engine-reuse: matching fallbacks share one platform and its id.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]
ENGINE_FONT = ROOT / 'GoldSrc-SDK/engine/vgui2/BaseUISurface_Font.cpp'
VGUI2_IMAGE = ROOT / 'GoldSrc-SDK/vgui2/src/fileimage.cpp'


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


PRELUDE = r'''
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <strings.h>
#endif

#include "BaseUISurface_Font.h"

#if !defined(_WIN32)
static int _stricmp(const char* left, const char* right) { return strcasecmp(left, right); }
#endif

using uchar = unsigned char;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

class FontPlatDar
{
public:
 int GetCount() const { return static_cast<int>(_items.size()); }
 void AddElement(BaseFontPlat* item) { _items.push_back(item); }
 void setCount(int count) { _items.resize(count); }
 BaseFontPlat*& operator[](int index) { return _items.at(index); }
private:
 std::vector<BaseFontPlat*> _items;
};

static int s_iFontCount;
static int staticFontId = 100;
static FontPlatDar staticFontPlatDar;
'''

VGUI2_PRELUDE = r'''
namespace vgui2_image
{
#pragma pack( push, 1 )
'''

TESTS = r'''
constexpr int FIRST_FONT_ID = 100;
constexpr int GLYPH_COUNT = 256;
constexpr int GLYPH_HEIGHT = 2;
constexpr int SYSTEM_TALL = 14;
constexpr int SYSTEM_WEIGHT = 400;
constexpr int TGA_HEADER_BYTES = 18;
constexpr int TGA_RLE_IMAGE = 10;
constexpr int TGA_PIXEL_BITS = 32;
constexpr int TGA_PACKET_PIXELS = 128;
constexpr int TGA_PIXEL_BYTES = 4;

// A valid 32-bit RLE TGA with one pixel column per glyph, written as raw packets.
static std::vector<uchar> BuildFontTGA(int width, int height)
{
 std::vector<uchar> tga(TGA_HEADER_BYTES, 0);
 tga[2] = TGA_RLE_IMAGE;
 tga[12] = static_cast<uchar>(width & 0xff);
 tga[13] = static_cast<uchar>(width >> 8);
 tga[14] = static_cast<uchar>(height & 0xff);
 tga[15] = static_cast<uchar>(height >> 8);
 tga[16] = TGA_PIXEL_BITS;
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

// Copy into an exactly sized heap block so ASan catches reads past the data.
static bool EngineLoads(const std::vector<uchar>& bytes, int length)
{
 std::vector<uchar> copy(bytes.begin(), bytes.begin() + length);
 FileImageStream_Memory stream(copy.data(), length);
 VFontData data;
 return LoadVFontDataFrom32BitTGA(&stream, &data);
}

static bool Vgui2Loads(const std::vector<uchar>& bytes, int length)
{
 std::vector<uchar> copy(bytes.begin(), bytes.begin() + length);
 vgui2_image::FileImageStream_Memory stream(copy.data(), length);
 vgui2_image::FileImage image;
 return vgui2_image::Load32BitTGA(&stream, &image);
}

static EngineFont* MakeFont(std::vector<uchar>& tga, int length, int tall = SYSTEM_TALL)
{
 return new EngineFont("Arial", length >= 0 ? tga.data() : nullptr, length, tall, 0, 0.0f, SYSTEM_WEIGHT,
  false, false, false, false);
}

int main(int argc, char** argv)
{
 try
 {
  Check(argc == 2, "usage: test <scenario>");
  const std::string test = argv[1];
  std::vector<uchar> tga = BuildFontTGA(GLYPH_COUNT, GLYPH_HEIGHT);
  const int size = static_cast<int>(tga.size());
  const int truncated = size - TGA_PIXEL_BYTES;
  if (test == "engine-loader-valid") Check(EngineLoads(tga, size), "valid TGA loads");
  else if (test == "engine-loader-truncated") Check(!EngineLoads(tga, truncated), "truncated pixels rejected");
  else if (test == "engine-loader-header-only") Check(!EngineLoads(tga, TGA_HEADER_BYTES), "header without pixels rejected");
  else if (test == "engine-loader-narrow")
  {
   std::vector<uchar> narrow = BuildFontTGA(GLYPH_COUNT / 2, GLYPH_HEIGHT);
   Check(!EngineLoads(narrow, static_cast<int>(narrow.size())), "narrower than one column per glyph rejected");
  }
  else if (test == "vgui2-loader-valid") Check(Vgui2Loads(tga, size), "valid TGA loads");
  else if (test == "vgui2-loader-truncated") Check(!Vgui2Loads(tga, truncated), "truncated pixels rejected");
  else if (test == "vgui2-loader-header-only") Check(!Vgui2Loads(tga, TGA_HEADER_BYTES), "header without pixels rejected");
  else
  {
   // Each EngineFont scenario deletes its fonts; the last one runs Font_Reset,
   // which deletes the platforms so leak checking covers them.
   if (test == "engine-valid")
   {
    EngineFont* font = MakeFont(tga, size);
    Check(nullptr != dynamic_cast<FontPlat_Bitmap*>(font->getPlat()), "valid data creates a bitmap font");
    Check(FIRST_FONT_ID == font->getId(), "bitmap font gets a fresh id");
    delete font;
   }
   else if (test == "engine-invalid" || test == "engine-empty")
   {
    EngineFont* font = MakeFont(tga, test == "engine-empty" ? 0 : truncated);
    Check(nullptr != dynamic_cast<FontPlat*>(font->getPlat()), "invalid data falls back to a system font");
    Check(FIRST_FONT_ID == font->getId(), "fallback font gets a fresh id");
    Check(font->getId() == font->getPlat()->getId(), "font id matches its platform");
    delete font;
   }
   else if (test == "engine-reuse")
   {
    EngineFont* first = MakeFont(tga, truncated);
    EngineFont* second = MakeFont(tga, truncated);
    EngineFont* other = MakeFont(tga, -1, SYSTEM_TALL * 2);
    Check(first->getPlat() == second->getPlat(), "matching fallbacks share one platform");
    Check(first->getId() == second->getId(), "matching fallbacks share the platform id");
    Check(other->getPlat() != first->getPlat() && other->getId() != first->getId(), "different parameters get a new platform");
    delete first;
    delete second;
    delete other;
   }
   else
   {
    Check(false, "unknown scenario");
   }
   Check(0 == staticFontPlatDar.GetCount(), "last font reset the platforms");
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


def main():
    engine = ENGINE_FONT.read_text()
    vgui2 = VGUI2_IMAGE.read_text()
    program = '\n'.join([
        PRELUDE,
        block(engine, 'static const int BITMAP_GLYPH_COUNT', '\n}\n'),
        block(engine, '#pragma pack( push, 1 )\nstruct TGAFileHeader', '#pragma pack( pop )'),
        # GetTextSize goes through the vgui2 localizer and font manager; it is
        # not on the paths tested here.
        block(engine, 'FileImageStream_Memory::FileImageStream_Memory(', 'void EngineFont::GetTextSize('),
        ' const char*, int& wide, int& tall ) { wide = tall = 0; }',
        block(engine, 'int EngineFont::GetTall()', 'BaseFontPlat* EngineFont::getPlat()\n{\n\treturn _plat;\n}\n'),
        block(engine, 'bool Load32BitTGA( FileImageStream* fp, FileImage* pImage )', '\n}\n'),
        block(engine, 'bool LoadVFontDataFrom32BitTGA( FileImageStream* fp, VFontData* pData )', '\n}\n'),
        block(engine, 'void Font_Reset()', '\n}\n'),
        VGUI2_PRELUDE,
        (ROOT / 'GoldSrc-SDK/vgui2/src/fileimage.h').read_text(),
        block(vgui2, 'class TGAFileHeader', '\n};\n'),
        '#pragma pack( pop )',
        block(vgui2, 'FileImageStream_Memory::FileImageStream_Memory(', 'return ret;\n}\n'),
        block(vgui2, 'bool Load32BitTGA( FileImageStream* fp, FileImage* pImage )', '\n}\n'),
        '}',
        TESTS,
    ])

    scenarios = ['engine-loader-valid', 'engine-loader-truncated', 'engine-loader-header-only',
                 'engine-loader-narrow', 'vgui2-loader-valid', 'vgui2-loader-truncated',
                 'vgui2-loader-header-only', 'engine-valid', 'engine-invalid', 'engine-empty',
                 'engine-reuse']
    with tempfile.TemporaryDirectory(prefix='goldsrc-engine-font-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(program)
        executable = compile_program([source_file], executable, includes=[ENGINE_FONT.parent], debug=True,
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
