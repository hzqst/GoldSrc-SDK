"""Exercise production localization writers, AddFile read-back and the SharedFindString fallback buffers."""
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
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
typedef void* FileHandle_t;
typedef unsigned short ucs2;
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
 unsigned int Size(FileHandle_t) { return (unsigned int)data.size(); }
 int Read(void* buffer, int size, FileHandle_t) { std::memcpy(buffer, data.data(), size); return size; }
 void Close(FileHandle_t) { ++closes; }
};

struct CUtlSymbol {
 std::string text;
 CUtlSymbol() {}
 CUtlSymbol(const char* value) : text(value) {}
 bool operator==(const CUtlSymbol& other) const { return text == other.text; }
};
static const CUtlSymbol UTL_INVAL_SYMBOL;

struct SymbolList {
 std::vector<CUtlSymbol> items;
 int Count() const { return (int)items.size(); }
 CUtlSymbol& operator[](int index) { return items[index]; }
 void Remove(int index) { items.erase(items.begin() + index); }
 void AddToTail(const CUtlSymbol& symbol) { items.push_back(symbol); }
};

struct ISystem {
 bool CommandLineParamExists(const char*) { return false; }
 bool GetRegistryString(const char*, char*, int) { return false; }
};
struct ISurface { const char* GetLanguage() { return ""; } };
struct IVGui { void DPrintf(const char*, ...) {} };
static ISystem systemStub;
static ISurface surfaceStub;
static IVGui vguiStub;
static ISystem* g_pSystem = &systemStub;
static ISurface* g_pSurface = &surfaceStub;
static IVGui* g_pIVgui = &vguiStub;
static const char* s_LanguageToken = "%language%";
static const char* s_English = "english";

// the keys in these tests are ASCII, so the platform UCS-2 to UTF-8 conversion is not needed
static int Q_UCS2ToUTF8(const ucs2* pUCS2, int, char* pUTF8, int cubDestSizeInBytes) {
 int i = 0;
 for (; pUCS2[i] && i < cubDestSizeInBytes - 1; ++i) {
  assert(pUCS2[i] < 0x80);
  pUTF8[i] = (char)pUCS2[i];
 }
 pUTF8[i] = 0;
 return i;
}

struct localizedstring_t { CUtlSymbol filename; const char* name; const wchar_t* value; };

static bool failConversion;

struct CLocalizedStringTable {
 std::vector<localizedstring_t> m_Lookup;
 std::vector<std::pair<std::string, std::wstring>> added;
 SymbolList m_LocalizationFiles;
 CUtlSymbol m_CurrentFile;
 char m_szLanguage[64] = {};
 bool m_bUseOnlyLongestLanguageString = false;
 bool AddAllLanguageFiles(IFileSystem*, const char*) { assert(false); return false; }
 void BuildFastValueLookup() {}
 void DiscardFastValueLookup() {}
 void AddString(const char* name, wchar_t* value, const char*) { added.push_back({name, value}); }
 bool AddFile(IFileSystem*, const char*);
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
    for name in ('char* Q_strncat(', 'static int DecodeUTF8CodePoint(', 'static int EncodeWideCodePoint(',
                 'static int DecodeWideCodePoint(', 'static void WriteUTF16LE('):
        code += function(table, name) + '\n'
    helpers = (ROOT / 'GoldSrc-SDK/vgui2/src/public/UnicodeFileHelpers.cpp').read_text(encoding='utf-8')
    for name in ('ucs2* AdvanceOverWhitespace(', 'ucs2* ReadUnicodeToken(', 'ucs2* ReadToEndOfLine('):
        code += function(helpers, name) + '\n'
    code += function(table, 'int CLocalizedStringTable::ConvertANSIToUnicode(').replace(
        'CLocalizedStringTable::ConvertANSIToUnicode(', 'CLocalizedStringTable::RealConvertANSIToUnicode(', 1) + '\n'
    code += function(table, 'bool CLocalizedStringTable::SaveToFile(') + '\n'
    code += function(table, 'bool CLocalizedStringTable::AddFile(') + '\n'

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
// the file is UTF-16LE whatever the size of wchar_t; the expected text here is ASCII
static void append(std::vector<unsigned char>& out, const char* text) {
 for (; *text; ++text) {
  out.push_back((unsigned char)*text);
  out.push_back(0);
 }
}

static void append(std::vector<unsigned char>& out, std::initializer_list<unsigned char> bytes) {
 out.insert(out.end(), bytes);
}

int main() {
 // the file ends with the end string and no terminator; entries from other files are skipped
 // U+4E2D U+6587 and U+1F600 are the same UTF-16LE bytes whether wchar_t is 16 or 32 bits
 const wchar_t* chinese = L"\u4E2D\u6587";
 const wchar_t* emoji = L"A\U0001F600B";
 CLocalizedStringTable table;
 table.m_Lookup.push_back({CUtlSymbol("resource/a.txt"), "Token_A", L"Value A"});
 table.m_Lookup.push_back({CUtlSymbol("resource/b.txt"), "Token_B", L"Value B"});
 table.m_Lookup.push_back({CUtlSymbol("resource/a.txt"), "Token_C", chinese});
 table.m_Lookup.push_back({CUtlSymbol("resource/a.txt"), "Token_D", emoji});
 IFileSystem file;
 assert(table.SaveToFile(&file, "resource/a.txt"));
 assert(1 == file.opens);
 assert(1 == file.closes);

 std::vector<unsigned char> expected = {0xFF, 0xFE};
 append(expected, "\"lang\"\r\n{\r\n\"Language\" \"English\"\r\n\"Tokens\"\r\n{\r\n");
 append(expected, "\t\"Token_A\"\t\t\"Value A\"\r\n");
 append(expected, "\t\"Token_C\"\t\t\"");
 append(expected, {0x2D, 0x4E, 0x87, 0x65});
 append(expected, "\"\r\n");
 append(expected, "\t\"Token_D\"\t\t\"A");
 append(expected, {0x3D, 0xD8, 0x00, 0xDE});
 append(expected, "B\"\r\n");
 append(expected, "}\r\n}\r\n");
 assert(expected == file.data);

 // AddFile reads the saved file back to the same wide strings
 CLocalizedStringTable reader;
 assert(reader.AddFile(&file, "resource/a.txt"));
 assert(0 == std::strcmp("English", reader.m_szLanguage));
 assert(3 == (int)reader.added.size());
 assert(std::string("Token_A") == reader.added[0].first);
 assert(std::wstring(L"Value A") == reader.added[0].second);
 assert(std::string("Token_C") == reader.added[1].first);
 assert(std::wstring(chinese) == reader.added[1].second);
 assert(std::string("Token_D") == reader.added[2].first);
 assert(std::wstring(emoji) == reader.added[2].second);
 assert(2 == file.opens);
 assert(2 == file.closes);

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
    print('PASS: SaveToFile UTF-16LE output and handles, AddFile read-back, SharedFindString fallback capacity')


if __name__ == '__main__':
    main()
