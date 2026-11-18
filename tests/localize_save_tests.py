"""Exercise production localization writers and the SharedFindString fallback buffers."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def main():
    table = (ROOT / 'GoldSrc-SDK/vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    code = r'''
#include <cassert>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
typedef void* FileHandle_t;
typedef unsigned long StringIndex_t;
constexpr StringIndex_t INVALID_LOCALIZE_STRING_INDEX = (StringIndex_t)-1;

struct IFileSystem {
 std::vector<unsigned char> data;
 int opens = 0, closes = 0;
 FileHandle_t Open(const char*, const char*, const char*) { ++opens; return this; }
 int Write(const void* buffer, size_t size, FileHandle_t) {
  const unsigned char* bytes = (const unsigned char*)buffer;
  data.insert(data.end(), bytes, bytes + size);
  return (int)size;
 }
 void Close(FileHandle_t) { ++closes; }
};

struct CUtlSymbol {
 std::string text;
 CUtlSymbol() {}
 CUtlSymbol(const char* value) : text(value) {}
 bool operator==(const CUtlSymbol& other) const { return text == other.text; }
};

struct localizedstring_t { CUtlSymbol filename; const char* name; const wchar_t* value; };

static bool failConversion;

struct CLocalizedStringTable {
 std::vector<localizedstring_t> m_Lookup;
 StringIndex_t GetFirstStringIndex() { return m_Lookup.empty() ? INVALID_LOCALIZE_STRING_INDEX : 0; }
 StringIndex_t GetNextStringIndex(StringIndex_t index) { return index + 1 < m_Lookup.size() ? index + 1 : INVALID_LOCALIZE_STRING_INDEX; }
 const char* GetNameByIndex(StringIndex_t index) { return m_Lookup[index].name; }
 wchar_t* GetValueByIndex(StringIndex_t index) { return const_cast<wchar_t*>(m_Lookup[index].value); }
 int ConvertANSIToUnicode(const char*, wchar_t*, int);
 int RealConvertANSIToUnicode(const char*, wchar_t*, int);
 bool SaveToFile(IFileSystem*, const char*);
};

int CLocalizedStringTable::ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, int size) {
 if (failConversion) { unicode[0] = 0; return 0; }
 return RealConvertANSIToUnicode(ansi, unicode, size);
}
'''
    for name in ('static int DecodeUTF8CodePoint(', 'static int EncodeWideCodePoint('):
        code += function(table, name) + '\n'
    code += function(table, 'int CLocalizedStringTable::ConvertANSIToUnicode(').replace(
        'CLocalizedStringTable::ConvertANSIToUnicode(', 'CLocalizedStringTable::RealConvertANSIToUnicode(', 1) + '\n'
    code += function(table, 'bool CLocalizedStringTable::SaveToFile(') + '\n'

    # both SharedFindString copies, each against a recording localize stub in its own namespace
    for path, namespace in (('GoldSrc-SDK/game_shared/shared_util.cpp', 'vgui'),
                            ('GoldSrc-SDK/game_shared/cstrike/shared_util.cpp', 'vgui2')):
        source = (ROOT / path).read_text(encoding='utf-8')
        code += r'''
namespace %(ns)s {
struct Localize {
 int size = 0;
 wchar_t* Find(const char*) { return nullptr; }
 int ConvertANSIToUnicode(const char*, wchar_t* unicode, int bytes) {
  size = bytes;
  std::memset(unicode, 0xAB, bytes);
  unicode[0] = 0;
  return 1;
 }
};
static Localize stub;
static Localize* localize() { return &stub; }
}
namespace %(ns)s_copy {
''' % {'ns': namespace} + function(source, 'wchar_t* SharedFindString(') + '\n}\n'
    code += r'''
// the writer emits wchar_t units in memory order, so build the expectation the same way
static void append(std::vector<unsigned char>& out, const wchar_t* text) {
 const unsigned char* bytes = (const unsigned char*)text;
 out.insert(out.end(), bytes, bytes + std::wcslen(text) * sizeof(wchar_t));
}

int main() {
 // the file ends with the end string and no terminator; entries from other files are skipped
 CLocalizedStringTable table;
 table.m_Lookup.push_back({CUtlSymbol("resource/a.txt"), "Token_A", L"Value A"});
 table.m_Lookup.push_back({CUtlSymbol("resource/b.txt"), "Token_B", L"Value B"});
 table.m_Lookup.push_back({CUtlSymbol("resource/a.txt"), "Token_C", L"Value C"});
 IFileSystem file;
 assert(table.SaveToFile(&file, "resource/a.txt"));
 assert(1 == file.opens);
 assert(1 == file.closes);

 std::vector<unsigned char> expected = {0xFF, 0xFE};
 append(expected, L"\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n");
 append(expected, L"\t\"Token_A\"\t\t\"Value A\"\r\n");
 append(expected, L"\t\"Token_C\"\t\t\"Value C\"\r\n");
 append(expected, L"}\r\n}\r\n");
 assert(expected == file.data);

 // a failed start string still closes the file
 failConversion = true;
 IFileSystem failed;
 assert(!table.SaveToFile(&failed, "resource/a.txt"));
 assert(1 == failed.opens);
 assert(1 == failed.closes);
 failConversion = false;

 // the fallback conversion is given the whole buffer, in bytes, whatever the wchar_t size
 char identifier[] = "#Missing_Token";
 assert(0 == std::wcscmp(L"", vgui_copy::SharedFindString(identifier)));
 assert((int)(1024 * sizeof(wchar_t)) == vgui::stub.size);
 assert(0 == std::wcscmp(L"", vgui2_copy::SharedFindString(identifier)));
 assert((int)(1024 * sizeof(wchar_t)) == vgui2::stub.size);
}
'''
    with tempfile.TemporaryDirectory(prefix='goldsrc-localize-save-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: SaveToFile output and handles, SharedFindString fallback capacity')


if __name__ == '__main__':
    main()
