"""Exercise the production SetValueByIndex against tokens that share one deduplicated value slot."""
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

typedef unsigned long StringIndex_t;

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

struct localizedstring_t {
 int valueIndex;
};

struct CUtlRBTree {
 std::vector<localizedstring_t> items;
 localizedstring_t& operator[](StringIndex_t i) { return items[i]; }
};

struct CLocalizedStringTable {
 CUtlRBTree m_Lookup;
 CUtlVector<wchar_t> m_Values;
 StringIndex_t AddShared(const wchar_t* value, int count);
 const wchar_t* Value(StringIndex_t index) { return &m_Values[m_Lookup[index].valueIndex]; }
 void SetValueByIndex(StringIndex_t index, wchar_t* newValue);
};

// adds count tokens that all reference the same value slot, as AddString does for equal text
StringIndex_t CLocalizedStringTable::AddShared(const wchar_t* value, int count) {
 int len = (int)wcslen(value) + 1;
 int valueIndex = m_Values.AddMultipleToTail(len);
 memcpy(&m_Values[valueIndex], value, len * sizeof(wchar_t));
 StringIndex_t first = (StringIndex_t)m_Lookup.items.size();
 for (int i = 0; i < count; i++)
  m_Lookup.items.push_back({valueIndex});
 return first;
}
'''
    code += function(table, 'void CLocalizedStringTable::SetValueByIndex(') + '\n'
    code += r'''
static void check_edit(const wchar_t* replacement) {
 CLocalizedStringTable table;
 table.AddShared(L"Other", 1);
 StringIndex_t a = table.AddShared(L"Same", 2);
 StringIndex_t b = a + 1;
 std::wstring text(replacement);
 table.SetValueByIndex(a, &text[0]);
 assert(std::wstring(replacement) == table.Value(a));
 assert(std::wstring(L"Same") == table.Value(b));
 assert(std::wstring(L"Other") == table.Value(0));
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
 table.SetValueByIndex(a, const_cast<wchar_t*>(table.Value(b)));
 assert(std::wstring(L"Source value") == table.Value(a));
 assert(std::wstring(L"Source value") == table.Value(b));

 // editing a token again keeps its siblings intact
 StringIndex_t c = table.AddShared(L"Shared", 2);
 std::wstring first(L"First");
 std::wstring second(L"Second edit");
 table.SetValueByIndex(c, &first[0]);
 table.SetValueByIndex(c, &second[0]);
 assert(std::wstring(L"Second edit") == table.Value(c));
 assert(std::wstring(L"Shared") == table.Value(c + 1));
}
'''
    with tempfile.TemporaryDirectory(prefix='goldsrc-localize-set-value-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: SetValueByIndex gives the edited token its own value storage')


if __name__ == '__main__':
    main()
