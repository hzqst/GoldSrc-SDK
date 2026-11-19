"""Exercise the production VGUI2 TextImage constructors and SetText with long wide strings, and the unlocalized symbol SetText keeps for GetUnlocalizedText.

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

// tokens are registered without the leading '#'
struct Localize {
 std::vector<std::pair<std::string, std::wstring>> strings;
 StringIndex_t FindIndex(const char* name) {
  for (size_t i = 0; i < strings.size(); ++i)
   if (strings[i].first == name) return (StringIndex_t)i;
  return INVALID_STRING_INDEX;
 }
 const char* GetNameByIndex(StringIndex_t index) { return strings[index].first.c_str(); }
 const wchar_t* GetValueByIndex(StringIndex_t index) { return strings[index].second.c_str(); }
 // ASCII only, cut and terminated like the production conversions
 int ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, int bytes) {
  int capacity = bytes / (int)sizeof(wchar_t), written = 0;
  while (ansi[written] && written < capacity - 1) { unicode[written] = (wchar_t)ansi[written]; ++written; }
  unicode[written] = 0;
  return written + 1;
 }
 int ConvertUnicodeToANSI(const wchar_t* unicode, char* ansi, int bytes) {
  int written = 0;
  while (unicode[written] && written < bytes - 1) { ansi[written] = (char)unicode[written]; ++written; }
  ansi[written] = 0;
  return written + 1;
 }
};
static Localize table;
Localize* localize() { return &table; }

template <class T> struct CUtlVector : std::vector<T> { void RemoveAll() { this->clear(); } };

// the members and functions SetText and the constructors use, with the production member declarations
class TextImage {
public:
 TextImage(const char* text);
 TextImage(const wchar_t* text);
 ~TextImage();
 void SetText(const char* text);
 void SetText(const wchar_t* text, bool bClearUnlocalizedSymbol = false);
 void GetText(char* buffer, int bufferSize);
 void GetUnlocalizedText(char* buffer, int bufferSize);
 StringIndex_t GetUnlocalizedTextSymbol();
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

static std::string Unlocalized(TextImage& image) {
 char buffer[256];
 image.GetUnlocalizedText(buffer, sizeof(buffer));
 return buffer;
}

static void PlainTextClearsTheLocalizedSymbol() {
 vgui2::table.strings = {{"Token", L"localized"}, {"Other", L"other"}};
 TextImage image("#Token");
 assert(0 == image.GetUnlocalizedTextSymbol());
 AssertText(image, L"localized");
 assert("#Token" == Unlocalized(image));
 image.SetText("plain");
 assert(vgui2::INVALID_STRING_INDEX == image.GetUnlocalizedTextSymbol());
 AssertText(image, L"plain");
 assert("plain" == Unlocalized(image));
 image.SetText("#Other");
 assert(1 == image.GetUnlocalizedTextSymbol());
 assert("#Other" == Unlocalized(image));
 image.SetText("#Missing");
 assert(vgui2::INVALID_STRING_INDEX == image.GetUnlocalizedTextSymbol());
 AssertText(image, L"#Missing");
 assert("#Missing" == Unlocalized(image));
 image.SetText("#Token");
 image.SetText((const char*)nullptr);
 assert(vgui2::INVALID_STRING_INDEX == image.GetUnlocalizedTextSymbol());
 assert("" == Unlocalized(image));
 puts("PASS: plain, missing and null narrow texts clear the symbol a #token left, a found #token sets it");
}

// Label and Frame OnDialogVariablesChanged rebuild the text from the symbol and set it as wide text
static void WideTextKeepsTheLocalizedSymbolByDefault() {
 vgui2::table.strings = {{"Token", L"localized"}};
 TextImage image("#Token");
 image.SetText(L"rebuilt");
 assert(0 == image.GetUnlocalizedTextSymbol());
 AssertText(image, L"rebuilt");
 assert("#Token" == Unlocalized(image));
 puts("PASS: wide text keeps the symbol that OnDialogVariablesChanged rebuilds from by default");
}

static void WideTextCanClearTheLocalizedSymbol() {
 vgui2::table.strings = {{"Token", L"localized"}};
 TextImage image("#Token");
 image.SetText(L"custom", true);
 assert(vgui2::INVALID_STRING_INDEX == image.GetUnlocalizedTextSymbol());
 AssertText(image, L"custom");
 assert("custom" == Unlocalized(image));
 image.SetText("#Token");
 image.SetText((const wchar_t*)nullptr, true);
 assert(vgui2::INVALID_STRING_INDEX == image.GetUnlocalizedTextSymbol());
 AssertText(image, L"");
 puts("PASS: wide text set with bClearUnlocalizedSymbol clears the symbol a #token left");
}

int main() {
 LongTextsGetABufferThatFits();
 ConstructingWithALongTextAllocates();
 ReassigningGrowsAndReusesTheBuffer();
 PlainTextClearsTheLocalizedSymbol();
 WideTextKeepsTheLocalizedSymbolByDefault();
 WideTextCanClearTheLocalizedSymbol();
}
'''


def main():
    folder_path = ROOT / 'vgui2/controls'
    source = (folder_path / 'TextImage.cpp').read_text(encoding='utf-8')
    header = (folder_path / 'TextImage.h').read_text(encoding='utf-8')
    code = STUBS.replace('MEMBERS', members(header))
    code += ''.join(function(source, signature) + '\n' for signature in (
        'TextImage::TextImage( const char* text )', 'TextImage::TextImage( const wchar_t* text )', 'TextImage::~TextImage()',
        'void TextImage::SetText( const char* text )', 'void TextImage::SetText( const wchar_t* text, bool bClearUnlocalizedSymbol )',
        'void TextImage::GetText( char* buffer, int bufferSize )', 'void TextImage::GetUnlocalizedText( char* buffer, int bufferSize )',
        'StringIndex_t TextImage::GetUnlocalizedTextSymbol()'))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-textimage-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
