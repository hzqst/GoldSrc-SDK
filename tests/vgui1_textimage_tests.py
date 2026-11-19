"""Compile the production VGUI1 TextImage and check how it owns its text (#308).

Run: python tests/vgui1_textimage_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
AddressSanitizer (including leak checks where supported) is enabled when the
compiler supports it; without it only the value checks run.

TextImage.cpp and Image.cpp are built twice: with VGUI_ABI_COMPAT_MODE=1 (the
original vgui.dll layout, no destructor) and with VGUI_ABI_COMPAT_MODE=0
(virtual destructors and Image::operator new / delete). TextImage.cpp is
compiled from its source text with the scheme font lookup replaced by NULL,
because App::getInstance() needs the whole VGUI1 App. vgui_strcpy is
extracted from vgui.cpp, whose remaining code only builds for 32-bit targets.

The test replaces the global array new / delete to count live text buffers, so
the compatible build, which cannot free the last buffer, still shows whether
setText or operator= lost an earlier one or freed it with the wrong delete.

- grow-frees-old: growing the text frees the previous buffer.
- shrink-reuses: shorter text reuses the buffer.
- self-text-grow / self-text-shrink: setText with a pointer into the current
  text, with and without growing, keeps that text.
- null-text: setText(NULL) gives an empty text.
- copy-independent: a copy-constructed TextImage has its own buffer.
- assign-independent: the same for operator=, which frees the buffer it replaces.
- self-assign: assigning a TextImage to itself keeps its text.
- destroy-frees (VGUI_ABI_COMPAT_MODE=0 only): deleting through Image* frees
  the text and the object.
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
FONT_LOOKUP = 'return App::getInstance()->getScheme()->getFont( _schemeFont );'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


TESTS = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

// The test reads the private text buffer.
#define private public
#include "VGUI_TextImage.h"
#undef private
#include "VGUI_App.h"

using namespace vgui;

// Only Color's scheme lookup reaches it, which these scenarios never do.
App* App::getInstance()
{
 return NULL;
}

static int liveArrays;
static int liveObjects;

void* operator new[](size_t size)
{
 void* memory = std::malloc(size);
 if (memory == NULL) throw std::bad_alloc();
 ++liveArrays;
 return memory;
}

void operator delete[](void* memory) noexcept
{
 if (memory != NULL) --liveArrays;
 std::free(memory);
}

void operator delete[](void* memory, size_t) noexcept
{
 operator delete[](memory);
}

void* operator new(size_t size)
{
 void* memory = std::malloc(size);
 if (memory == NULL) throw std::bad_alloc();
 ++liveObjects;
 return memory;
}

void operator delete(void* memory) noexcept
{
 if (memory != NULL) --liveObjects;
 std::free(memory);
}

void operator delete(void* memory, size_t) noexcept
{
 operator delete(memory);
}

static const int GROW_COUNT = 8;
static const int SELF_OFFSET = 2;
static const int GROW_EXTRA = 16;

static void Check(bool condition, const char* message)
{
 if (!condition) throw std::runtime_error(message);
}

static std::string Text(TextImage& image)
{
 return image._text != NULL ? image._text : "";
}

// The compatible build has no destructor; this frees the last buffer.
static void Release(TextImage& image)
{
#if VGUI_ABI_COMPAT_MODE == 1
 delete[] image._text;
 image._text = NULL;
 image._textBufferLen = 0;
#else
 (void)image;
#endif
}

static void Run(const std::string& test)
{
 if (test == "grow-frees-old")
 {
  TextImage image("a");
  std::string text = "a";
  for (int i = 0; i < GROW_COUNT; ++i)
  {
   text += "bc";
   image.setText(text.c_str());
   Check(1 == liveArrays, "one live buffer after growing");
  }
  Check(text == Text(image), "last text kept");
  Release(image);
 }
 else if (test == "shrink-reuses")
 {
  TextImage image("longer text");
  char* buffer = image._text;
  image.setText("short");
  Check(buffer == image._text, "buffer reused");
  Check(std::string("short") == Text(image), "short text");
  Release(image);
 }
 else if (test == "self-text-grow")
 {
  TextImage image("self text");
  int bufferLen = image._textBufferLen + GROW_EXTRA;
  image.setText(bufferLen, image._text);
  Check(bufferLen == image._textBufferLen, "buffer grown");
  Check(std::string("self text") == Text(image), "self text kept");
  Check(1 == liveArrays, "old buffer freed");
  Release(image);
 }
 else if (test == "self-text-shrink")
 {
  TextImage image("self text");
  image.setText(image._text + SELF_OFFSET);
  Check(std::string("lf text") == Text(image), "overlapping text copied");
  Release(image);
 }
 else if (test == "null-text")
 {
  TextImage image("text");
  image.setText(NULL);
  Check(std::string("") == Text(image), "null text is empty");
  Release(image);
 }
 else if (test == "copy-independent")
 {
  TextImage source("source");
  TextImage copy(source);
  Check(source._text != copy._text, "copy has its own buffer");
  Check(std::string("source") == Text(copy), "copy has the text");
  copy.setText("a longer copy text");
  Check(std::string("source") == Text(source), "source text unchanged");
  Check(2 == liveArrays, "two live buffers");
  Release(source);
  Release(copy);
 }
 else if (test == "assign-independent")
 {
  TextImage source("source");
  TextImage copy("replaced text");
  copy = source;
  Check(source._text != copy._text, "copy has its own buffer");
  Check(std::string("source") == Text(copy), "copy has the text");
  Check(2 == liveArrays, "replaced buffer freed");
  copy.setText("a longer copy text");
  Check(std::string("source") == Text(source), "source text unchanged");
  Release(source);
  Release(copy);
 }
 else if (test == "self-assign")
 {
  TextImage image("self");
  TextImage& alias = image;
  image = alias;
  Check(std::string("self") == Text(image), "text kept");
  Check(1 == liveArrays, "one live buffer");
  Release(image);
 }
#if VGUI_ABI_COMPAT_MODE != 1
 else if (test == "destroy-frees")
 {
  Image* image = new TextImage("destroyed");
  Check(1 == liveObjects && 1 == liveArrays, "object and text allocated");
  delete image;
  Check(0 == liveObjects, "object freed");
 }
#endif
 else Check(false, "unknown scenario");

 Check(0 == liveArrays, "no text buffer left");
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

SCENARIOS = ['grow-frees-old', 'shrink-reuses', 'self-text-grow', 'self-text-shrink', 'null-text', 'copy-independent',
             'assign-independent', 'self-assign']
# VGUI_ABI_COMPAT_MODE value -> scenarios run against that build.
MODES = {'1': SCENARIOS, '0': SCENARIOS + ['destroy-frees']}


def main():
    text_image = (VGUI / 'TextImage.cpp').read_text()
    if FONT_LOOKUP not in text_image:
        raise SystemExit('TextImage::getFont no longer matches: ' + FONT_LOOKUP)
    text_image = text_image.replace(FONT_LOOKUP, 'return NULL;')
    strcpy = function((VGUI / 'vgui.cpp').read_text(), 'void vgui_strcpy(')

    failed = []
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui1-textimage-') as folder:
        test_file = Path(folder) / 'test.cpp'
        test_file.write_text(TESTS + '\nnamespace vgui\n{\n' + strcpy + '\n}\n')
        text_image_file = Path(folder) / 'TextImage.cpp'
        text_image_file.write_text(text_image)
        for mode, scenarios in MODES.items():
            executable = Path(folder) / f'test-compat{mode}.exe'
            sources = [test_file, text_image_file, VGUI / 'Image.cpp', VGUI / 'Color.cpp']
            # VGUI_STATIC keeps VGUIAPI from exporting on Windows: MSVC would emit the whole
            # vtable of an exported class in every file, also for sources the test leaves out.
            executable = compile_program(sources, executable, includes=[VGUI / 'include'],
                                         defines=['VGUI_STATIC', f'VGUI_ABI_COMPAT_MODE={mode}'], debug=True,
                                         sanitizers=['address'])
            for scenario in scenarios:
                result = subprocess.run([str(executable), scenario])
                if result.returncode:
                    failed.append(f'{scenario} (VGUI_ABI_COMPAT_MODE={mode})')
    if failed:
        raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
