"""Exercise the production condump with the production RichText reads and UTF-8 conversion.

Run: python tests/gameui_condump_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import re
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


def constants(source):
    return ''.join(re.findall(r'^static const int \w+ = .*;\n', source, re.M))


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

typedef void* FileHandle_t;

struct CLocalizedStringTable {
 int ConvertUnicodeToANSI(const wchar_t* unicode, char* ansi, int ansiBufferSize);
};
'''

FAKES = r'''
static CLocalizedStringTable g_table;
static int g_failedConversions = 0;

namespace vgui2 {
struct ILocalize {
 int ConvertUnicodeToANSI(const wchar_t* unicode, char* ansi, int ansiBufferSize) {
  int result = g_table.ConvertUnicodeToANSI(unicode, ansi, ansiBufferSize);
  if (!result) g_failedConversions++;
  return result;
 }
};
static ILocalize g_localize;
ILocalize* localize() { return &g_localize; }

struct IFileSystem {
 bool FileExists(const char*) { return false; }
 FileHandle_t Open(const char*, const char*, const char*) { opened = true; return this; }
 void Write(const void* data, int size, FileHandle_t) { output.append((const char*)data, size); }
 void Close(FileHandle_t) { closed = true; }
 std::string output;
 bool opened = false, closed = false;
};
static IFileSystem g_fileSystem;
IFileSystem* filesystem() { return &g_fileSystem; }
}

struct TextStream {
 int Count() const { return (int)text.size(); }
 wchar_t operator[](int index) const { return text[index]; }
 std::wstring text;
};

class RichText {
public:
 void GetText(int offset, wchar_t* buf, int bufLenInBytes);
 TextStream m_TextStream;
};

class CGameConsoleDialog {
public:
 void DumpConsoleTextToFile();
 void Print(const char* text) { printed += text; }
 RichText* m_pHistory;
 std::string printed;
};
'''

TESTS = r'''
// the history as condump should write it: UTF-8 with each LF written as CRLF
static std::string Expected(const std::wstring& history) {
 std::vector<char> utf8(history.size() * 4 + 1);
 assert(0 != g_table.ConvertUnicodeToANSI(history.c_str(), utf8.data(), (int)utf8.size()));
 std::string expected;
 for (const char* c = utf8.data(); *c; c++) {
  if (*c == '\n') expected += '\r';
  expected += *c;
 }
 return expected;
}

static void Dump(const std::wstring& history) {
 vgui2::g_fileSystem = vgui2::IFileSystem();
 g_failedConversions = 0;
 RichText richText;
 richText.m_TextStream.text = history;
 CGameConsoleDialog dialog;
 dialog.m_pHistory = &richText;
 dialog.DumpConsoleTextToFile();
 assert(vgui2::g_fileSystem.opened && vgui2::g_fileSystem.closed);
 assert(std::string("console dumped to condump000.txt\n") == dialog.printed);
 assert(0 == g_failedConversions);
 assert(Expected(history) == vgui2::g_fileSystem.output);
}

// ASCII where every character differs from its neighbours, with a line break now and then
static std::wstring Ascii(size_t length) {
 std::wstring text;
 for (size_t i = 0; i < length; i++)
  text += i % 61 == 60 ? L'\n' : (wchar_t)(L'!' + i % 90);
 return text;
}

static void EmptyHistoryWritesAnEmptyFile() {
 Dump(L"");
 assert(vgui2::g_fileSystem.output.empty());
 puts("PASS: an empty history writes an empty file");
}

static void AsciiAcrossChunkBoundariesIsWrittenWhole() {
 for (size_t length : {1, 510, 511, 512, 513, 1021, 1022, 1023, 1024, 1025, 5000})
  Dump(Ascii(length));
 puts("PASS: ASCII history across every chunk boundary is written without losing a character");
}

static void ChineseAndMixedTextIsWrittenWhole() {
 Dump(std::wstring(1500, L'\u4E2D'));
 std::wstring mixed;
 for (int i = 0; i < 700; i++)
  mixed += i % 3 ? L"\u4E2D\u6587a" : L"\u00E9\n";
 Dump(mixed);
 puts("PASS: Chinese and mixed history is written whole, with no conversion cut");
}

static void SupplementaryCharactersAcrossChunkBoundariesAreKeptWhole() {
 const std::wstring emoji = L"\U0001F600";
 // with UTF-16 the pair starts on the last character of a full chunk, 510 and 1020
 for (size_t before : {509, 510, 511, 1019, 1020}) {
  std::wstring text(before, L'a');
  text += emoji + L"b" + emoji;
  Dump(text);
 }
 std::wstring emojis;
 for (int i = 0; i < 1000; i++) emojis += emoji;
 Dump(emojis);
 puts("PASS: supplementary characters on a chunk boundary are written whole");
}
'''


def main():
    dialog = (ROOT / 'GameUI/GameConsoleDialog.cpp').read_text(encoding='utf-8')
    rich_text = (ROOT / 'vgui2/controls/RichText.cpp').read_text(encoding='utf-8')
    localize = (ROOT / 'vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += ''.join(function(localize, signature) + '\n' for signature in (
        'static int DecodeWideCodePoint(', 'static int EncodeUTF8CodePoint(',
        'int CLocalizedStringTable::ConvertUnicodeToANSI('))
    code += FAKES
    code += function(rich_text, 'void RichText::GetText( int offset, wchar_t* buf,') + '\n'
    code += constants(dialog)
    code += function(dialog, 'void CGameConsoleDialog::DumpConsoleTextToFile()') + '\n'
    code += TESTS
    code += ('\nint main() { EmptyHistoryWritesAnEmptyFile(); AsciiAcrossChunkBoundariesIsWrittenWhole();'
             ' ChineseAndMixedTextIsWrittenWhole(); SupplementaryCharactersAcrossChunkBoundariesAreKeptWhole(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-condump-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'],
                                     defines=['_CRT_SECURE_NO_WARNINGS'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
