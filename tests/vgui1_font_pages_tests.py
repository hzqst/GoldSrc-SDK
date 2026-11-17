"""Compile the production VGUI1 engine font-page packer with deterministic stubs.

Run: python tests/vgui1_font_pages_tests.py
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).

EngineSurfaceWrap::drawSetTextFont packs a font's glyphs into at most
FONT_PAGE_LIMIT 512x512 textures, and drawPrintText draws from them. Fresh heap
memory is filled with 0xCD so uninitialized FontInfoVGUI fields are visible.

- five-pages: every page from 0 to 4 gets its own new texture; bindIndex[4..7]
  are not left uninitialized.
- page-limit: a font needing more than 8 pages stops at 8 uploads, reports 8
  pages and does not write past bindIndex into texCoord[0].
- unpacked-skip: drawPrintText still advances over glyphs that were never
  packed, but does not draw them with stale coordinates.
- free-infos: FreeFontInfos deletes every FontInfoVGUI, and the surface
  constructor and destructor both call it.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'GoldSrc-SDK/engine/VGUI_EngineSurfaceWrap.cpp'


def block(source, start_marker, end_marker):
    start = source.index(start_marker)
    return source[start:source.index(end_marker, start) + len(end_marker)]


STUBS = r'''
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

constexpr unsigned char UNINITIALIZED_BYTE = 0xCD;
constexpr int MAX_TRACKED = 64;
static void* g_tracked[MAX_TRACKED];
static std::size_t g_trackedSize;

// FontInfoVGUI allocations are tracked so the tests can count live objects.
void* operator new(std::size_t size)
{
 void* memory = std::malloc(size ? size : 1);
 if (!memory) throw std::bad_alloc();
 std::memset(memory, UNINITIALIZED_BYTE, size);
 if (size == g_trackedSize)
 {
  for (void*& slot : g_tracked)
  {
   if (!slot) { slot = memory; break; }
  }
 }
 return memory;
}
void operator delete(void* memory) noexcept
{
 for (void*& slot : g_tracked)
 {
  if (slot == memory) slot = nullptr;
 }
 std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept { operator delete(memory); }

static int LiveTracked()
{
 int live = 0;
 for (void* slot : g_tracked) live += slot != nullptr;
 return live;
}

namespace vgui
{
template<class T> class Dar
{
public:
 int getCount() const { return static_cast<int>(_items.size()); }
 void setCount(int count) { _items.resize(count); }
 void addElement(const T& item) { _items.push_back(item); }
 T& operator[](int index) { return _items.at(index); }
private:
 std::vector<T> _items;
};

class Font
{
public:
 int id = 1;
 int wide = 8;
 int tall = 8;
 int getId() { return id; }
 int getWide() { return wide; }
 int getTall() { return tall; }
 void getCharABCwide(int, int& a, int& b, int& c) { a = 1; b = wide; c = 2; }
 void getCharRGBA(int, int, int, int, int, unsigned char*) {}
};
}
'''

STATICS = r'''
static vgui::Font* staticFont = nullptr;
static FontInfoVGUI* staticFontInfoVGUI = nullptr;
static vgui::Dar<FontInfoVGUI*> staticFontInfoVGUIDar;
static int staticContextCount = 0;
static unsigned char staticRGBA[FONT_TEXTURE_BYTES];
'''

SURFACE = r'''
class EngineSurfaceWrap
{
public:
 void drawSetTextFont(vgui::Font* font);
 void drawPrintText(const char* text, int textLen);

 std::vector<int> created, uploaded, bound;
 int printed = 0;
 int textX = 0, textY = 0;
 int nextTexture = 100;

 int createNewTextureID() { created.push_back(nextTexture); return nextTexture++; }
 void drawSetTextureRGBA(int id, const char*, int, int) { uploaded.push_back(id); }
 void drawSetTexture(int id) { bound.push_back(id); }
 void drawPrintChar(int, int, int, int, float, float, float, float) { ++printed; }
 void drawGetTextPos(int& x, int& y) { x = textX; y = textY; }
 void drawSetTextPos(int x, int y) { textX = x; textY = y; }
};
'''

TESTS = r'''
// Pages hold floor(512 / (tall + 1)) rows of floor(512 / (wide + 1)) glyphs.
// 150px glyphs fit 3x3 per page, so even the 95 printable ASCII glyphs packed
// on non-Windows builds need more than FONT_PAGE_LIMIT pages.
constexpr int HUGE_GLYPH = 150;
// 110px glyphs fit 4x4 per page, so both the 95 printable glyphs (6 pages) and
// the 256 glyphs packed on Windows (past the limit) reach pages 4..7.
constexpr int LARGE_GLYPH = 110;

static void PackFont(EngineSurfaceWrap& surface, vgui::Font& font)
{
 ++staticContextCount;
 surface.drawSetTextFont(&font);
 Check(staticFontInfoVGUI != nullptr, "font info created");
}

static bool CreatedTexture(const EngineSurfaceWrap& surface, int id)
{
 for (int texture : surface.created)
 {
  if (texture == id) return true;
 }
 return false;
}

static void CheckPagesUseCreatedTextures(const EngineSurfaceWrap& surface)
{
 const int pages = staticFontInfoVGUI->pageCount;
 Check(pages >= 1 && pages <= FONT_PAGE_LIMIT, "page count within the limit");
 Check(static_cast<int>(surface.created.size()) == pages, "one new texture per page");
 Check(static_cast<int>(surface.uploaded.size()) == pages, "one upload per page");
 for (int page = 0; page < pages; ++page)
 {
  Check(CreatedTexture(surface, staticFontInfoVGUI->bindIndex[page]), "page bound to a created texture");
  Check(surface.uploaded[page] == staticFontInfoVGUI->bindIndex[page], "page uploaded to its own texture");
 }
 for (int character = 0; character < FONT_GLYPH_COUNT; ++character)
 {
  const int page = staticFontInfoVGUI->pageForChar[character];
  Check(page == FONT_PAGE_UNPACKED || (page >= 0 && page < pages), "glyph page in range");
 }
}

int main(int argc, char** argv)
{
 g_trackedSize = sizeof(FontInfoVGUI);
 try
 {
  Check(argc == 2, "usage: test <scenario>");
  const std::string test = argv[1];
  EngineSurfaceWrap surface;
  vgui::Font font;
  if (test == "five-pages")
  {
   font.wide = font.tall = LARGE_GLYPH;
   PackFont(surface, font);
   CheckPagesUseCreatedTextures(surface);
  }
  else if (test == "page-limit")
  {
   font.wide = font.tall = HUGE_GLYPH;
   PackFont(surface, font);
   Check(FONT_PAGE_LIMIT == staticFontInfoVGUI->pageCount, "stops at the page limit");
   CheckPagesUseCreatedTextures(surface);
   // Glyph 0 is either unpacked (zeroed) or packed at the origin of page 0; a
   // texture id written through bindIndex[FONT_PAGE_LIMIT] would land in coord[0].
   const float* coord = staticFontInfoVGUI->texCoord[0];
   const bool packed = FONT_PAGE_UNPACKED != staticFontInfoVGUI->pageForChar[0];
   Check(0.0f == coord[0] && 0.0f == coord[1], "texCoord[0] origin untouched");
   Check((packed ? HUGE_GLYPH * FONT_TEXTURE_COORD_SCALE : 0.0f) == coord[2], "texCoord[0] right edge");
   Check((packed ? HUGE_GLYPH * FONT_TEXTURE_COORD_SCALE : 0.0f) == coord[3], "texCoord[0] bottom edge");
   for (int index = 4; index < 8; ++index)
   {
    Check(0.0f == coord[index], "unused texCoord slots zeroed");
   }
  }
  else if (test == "unpacked-skip")
  {
   font.wide = font.tall = HUGE_GLYPH;
   PackFont(surface, font);
   const char text[] = "~";
   Check(FONT_PAGE_UNPACKED == staticFontInfoVGUI->pageForChar[static_cast<unsigned char>(text[0])], "'~' did not fit");
   surface.drawPrintText(text, 1);
   Check(0 == surface.printed, "unpacked glyph not drawn");
   Check(surface.bound.empty(), "no texture bound for an unpacked glyph");
   Check(1 + HUGE_GLYPH + 2 == surface.textX, "pen still advances by a + b + c");

   // Non-Windows builds skip non-printable glyphs, so find the first glyph on page 0.
   int first = 0;
   while (0 != staticFontInfoVGUI->pageForChar[first]) ++first;
   const char packed[] = { static_cast<char>(first), 0 };
   surface.drawPrintText(packed, 1);
   Check(1 == surface.printed, "packed glyph drawn");
   Check(1 == static_cast<int>(surface.bound.size()) && surface.bound[0] == staticFontInfoVGUI->bindIndex[0], "page 0 bound");
  }
  else if (test == "free-infos")
  {
   for (int id = 1; id <= 3; ++id)
   {
    font.id = id;
    surface.drawSetTextFont(&font);
   }
   Check(3 == staticFontInfoVGUIDar.getCount(), "one info per font");
   Check(3 == LiveTracked(), "three infos allocated");
   FreeFontInfos();
   Check(0 == staticFontInfoVGUIDar.getCount(), "list cleared");
   Check(nullptr == staticFontInfoVGUI, "current info cleared");
   Check(0 == LiveTracked(), "every info deleted");
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


def check_free_calls(source):
    for signature in ('EngineSurfaceWrap::EngineSurfaceWrap(', 'EngineSurfaceWrap::~EngineSurfaceWrap()'):
        body = block(source, signature, '\n}\n')
        if not re.search(r'\bFreeFontInfos\(\s*\);', body):
            raise SystemExit(f'FAILED: {signature} does not call FreeFontInfos')


def main():
    source = SOURCE.read_text()
    check_free_calls(source)
    program = '\n'.join([
        STUBS,
        block(source, 'class FontInfoVGUI', '\n};\n'),
        block(source, 'constexpr int FONT_TEXTURE_SIZE', 'constexpr int FONT_PAGE_UNPACKED = -1;'),
        STATICS,
        block(source, 'static void FreeFontInfos()', '\n}\n'),
        SURFACE,
        block(source, 'void EngineSurfaceWrap::drawSetTextFont( vgui::Font* font )', '\n}\n'),
        block(source, 'void EngineSurfaceWrap::drawPrintText( const char* text, int textLen )', '\n}\n'),
        TESTS,
    ])

    scenarios = ['five-pages', 'page-limit', 'unpacked-skip', 'free-infos']
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-pages-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(program)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        subprocess.run(compiler + ['-std=c++17', '-g', str(source_file), '-o', str(executable)], check=True)
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
