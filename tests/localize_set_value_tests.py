"""Exercise the production SetValueByIndex and AddString against shared and self-referencing value storage."""
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
#ifdef _MSC_VER
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

typedef unsigned long StringIndex_t;
#define INVALID_LOCALIZE_STRING_INDEX ((StringIndex_t)-1)

// every growth moves the storage, so a stale pointer into the old buffer is caught by ASan
template <class T>
struct CUtlVector {
 std::vector<T> items;
 int Count() const { return (int)items.size(); }
 T& operator[](int i) { return items[i]; }
 T* Base() { return items.data(); }
 int AddMultipleToTail(int num) {
  int first = Count();
  std::vector<T> grown(items.size() + num);
  std::copy(items.begin(), items.end(), grown.begin());
  items.swap(grown);
  return first;
 }
};

struct CUtlSymbol {
 std::string text;
 CUtlSymbol() {}
 CUtlSymbol(const char* value) : text(value) {}
};

struct ISurface {
 void GetTextSize(int, const wchar_t*, int&, int&) { assert(false); }
};
static ISurface* g_pSurface = nullptr;

struct localizedstring_t {
 StringIndex_t nameIndex;
 StringIndex_t valueIndex;
 CUtlSymbol filename;
};

struct CUtlRBTree {
 std::vector<localizedstring_t> items;
 localizedstring_t& operator[](StringIndex_t i) { return items[i]; }
 StringIndex_t Insert(const localizedstring_t& item) { items.push_back(item); return items.size() - 1; }
};

struct CLocalizedStringTable {
 bool m_bUseOnlyLongestLanguageString = false;
 CUtlRBTree m_Lookup;
 CUtlVector<char> m_Names;
 CUtlVector<wchar_t> m_Values;
 CUtlSymbol m_CurrentFile;

 // outside of file loading there is no fast value lookup, so nothing is deduplicated
 int FindExistingValueIndex(const wchar_t*) { return -1; }
 StringIndex_t FindIndex(const char* name) {
  for (StringIndex_t i = 0; i < m_Lookup.items.size(); i++)
   if (!strcasecmp(&m_Names[m_Lookup.items[i].nameIndex], name))
    return i;
  return INVALID_LOCALIZE_STRING_INDEX;
 }
 const char* GetNameByIndex(StringIndex_t index) { return &m_Names[m_Lookup[index].nameIndex]; }
 wchar_t* GetValueByIndex(StringIndex_t index) { return &m_Values[m_Lookup[index].valueIndex]; }
 std::wstring Value(const char* name) { return GetValueByIndex(FindIndex(name)); }

 StringIndex_t AddShared(const wchar_t* value, int count);
 int AddValue(const wchar_t* value);
 void SetValueByIndex(StringIndex_t index, wchar_t* newValue);
 void AddString(const char* pString, wchar_t* pValue, const char* fileName);
};

// adds count tokens that all reference the same value slot, as AddString does for equal text while loading
StringIndex_t CLocalizedStringTable::AddShared(const wchar_t* value, int count) {
 int valueIndex = AddValue(value);
 StringIndex_t first = (StringIndex_t)m_Lookup.items.size();
 for (int i = 0; i < count; i++)
  m_Lookup.items.push_back({0, (StringIndex_t)valueIndex, CUtlSymbol()});
 return first;
}
'''
    code += function(table, 'int CLocalizedStringTable::AddValue(') + '\n'
    code += function(table, 'void CLocalizedStringTable::SetValueByIndex(') + '\n'
    code += function(table, 'void CLocalizedStringTable::AddString(') + '\n'
    code += r'''
static std::wstring value_of(CLocalizedStringTable& table, StringIndex_t index) {
 return table.GetValueByIndex(index);
}

static void check_edit(const wchar_t* replacement) {
 CLocalizedStringTable table;
 table.AddShared(L"Other", 1);
 StringIndex_t a = table.AddShared(L"Same", 2);
 StringIndex_t b = a + 1;
 std::wstring text(replacement);
 table.SetValueByIndex(a, &text[0]);
 assert(std::wstring(replacement) == value_of(table, a));
 assert(std::wstring(L"Same") == value_of(table, b));
 assert(std::wstring(L"Other") == value_of(table, 0));
}

int main() {
 // shorter, equal-length and longer replacements only change the edited token
 check_edit(L"New");
 check_edit(L"Abcd");
 check_edit(L"Longer");
 check_edit(L"");

 // a replacement that points into the value storage survives the growth of that storage
 CLocalizedStringTable table;
 StringIndex_t a = table.AddShared(L"Same", 1);
 StringIndex_t b = table.AddShared(L"Source value", 1);
 table.SetValueByIndex(a, table.GetValueByIndex(b));
 assert(std::wstring(L"Source value") == value_of(table, a));
 assert(std::wstring(L"Source value") == value_of(table, b));

 // editing a token again keeps its siblings intact
 StringIndex_t c = table.AddShared(L"Shared", 2);
 std::wstring first(L"First");
 std::wstring second(L"Second edit");
 table.SetValueByIndex(c, &first[0]);
 table.SetValueByIndex(c, &second[0]);
 assert(std::wstring(L"Second edit") == value_of(table, c));
 assert(std::wstring(L"Shared") == value_of(table, c + 1));

 // AddString copies a value taken from the table itself, both for a new token and an existing one
 CLocalizedStringTable strings;
 std::wstring settings(L"Settings");
 std::wstring other(L"Other");
 strings.AddString("Base", &settings[0], "base.txt");
 strings.AddString("Other", &other[0], "base.txt");
 strings.AddString("Copy", strings.GetValueByIndex(strings.FindIndex("Base")), NULL);
 assert(std::wstring(L"Settings") == strings.Value("Copy"));
 strings.AddString("Other", strings.GetValueByIndex(strings.FindIndex("Copy")), NULL);
 assert(std::wstring(L"Settings") == strings.Value("Other"));
 assert(std::wstring(L"Settings") == strings.Value("Base"));
 assert(3 == (int)strings.m_Lookup.items.size());
}
'''
    with tempfile.TemporaryDirectory(prefix='goldsrc-localize-set-value-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: SetValueByIndex gives the edited token its own value storage, '
          'and AddString copies values that point into the table')


if __name__ == '__main__':
    main()
