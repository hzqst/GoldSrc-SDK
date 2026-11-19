"""Exercise the production VGUI2 TextImage constructors and SetText with long wide strings.

Run: python tests/vgui2_textimage_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def members(header):
    start = header.index('\twchar_t*\t\t_utext;')
    end = header.index('CUtlVector<wchar_t*> m_LineBreaks;', start)
    return header[start:end]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace vgui2 {
typedef unsigned long HFont;
typedef int StringIndex_t;
static const StringIndex_t INVALID_STRING_INDEX = -1;

template <class T> struct CUtlVector : std::vector<T> { void RemoveAll() { this->clear(); } };

// the members and functions SetText and the constructors use, with the production member declarations
class TextImage {
public:
 TextImage(const char* text);
 TextImage(const wchar_t* text);
 ~TextImage();
 void SetText(const char*) {}
 void SetText(const wchar_t* text);
 void RecalculateNewLinePositions() {}
 MEMBERS
 CUtlVector<wchar_t*> m_LineBreaks;
};
'''

TESTS = r'''
}
using vgui2::TextImage;

static void AssertText(const TextImage& image, const std::wstring& text) {
 assert(text.size() == (size_t)image._textLen);
 assert(image._textLen < (size_t)image._textBufferLen);
 assert(text == image._utext);
}

static void LongTextsGetABufferThatFits() {
 for (size_t length : {32766, 32767, 32768, 65536}) {
  std::wstring text(length, L'A');
  TextImage image(L"");
  image.SetText(text.c_str());
  AssertText(image, text);
 }
 puts("PASS: texts of 32766, 32767, 32768 and 65536 characters are stored in a buffer that fits");
}

static void ConstructingWithALongTextAllocates() {
 std::wstring text(32768, L'B');
 TextImage image(text.c_str());
 AssertText(image, text);
 assert(!image.m_pwszEllipsesPosition);
 puts("PASS: constructing with a long text allocates it and clears the ellipses position");
}

static void ReassigningGrowsAndReusesTheBuffer() {
 TextImage image(L"short");
 AssertText(image, L"short");
 std::wstring text(40000, L'C');
 image.SetText(text.c_str());
 AssertText(image, text);
 wchar_t* buffer = image._utext;
 image.SetText(L"again");
 AssertText(image, L"again");
 assert(buffer == image._utext);
 image.SetText(text.c_str());
 AssertText(image, text);
 assert(buffer == image._utext);
 image.SetText((const wchar_t*)nullptr);
 AssertText(image, L"");
 puts("PASS: reassigning short, long and empty texts grows the buffer once and reuses it");
}

int main() {
 LongTextsGetABufferThatFits();
 ConstructingWithALongTextAllocates();
 ReassigningGrowsAndReusesTheBuffer();
}
'''


def main():
    folder_path = ROOT / 'vgui2/controls'
    source = (folder_path / 'TextImage.cpp').read_text(encoding='utf-8')
    header = (folder_path / 'TextImage.h').read_text(encoding='utf-8')
    code = STUBS.replace('MEMBERS', members(header))
    code += ''.join(function(source, signature) + '\n' for signature in (
        'TextImage::TextImage( const wchar_t* text )', 'TextImage::~TextImage()', 'void TextImage::SetText( const wchar_t* text )'))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-textimage-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
