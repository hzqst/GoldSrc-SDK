"""Exercise the production localized string construction with output that fits, fills or overflows the buffer.

Covers CLocalizedStringTable::ConstructString with KeyValues variables and ConstructStringFromArgs with %s parameters.

Run: python tests/localize_construct_tests.py
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


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#define LOCALIZEDSTRINGTABLE_SOURCE_PATH L""
// the engine is 32-bit, where a va_list walks the pushed parameters like this array
typedef wchar_t** WideParameters;
#define va_list WideParameters

typedef int StringIndex_t;

struct KeyValues {
 const wchar_t* GetWString(const char* name, const wchar_t* defaultValue) {
  auto found = values.find(name);
  return found == values.end() ? defaultValue : found->second.c_str();
 }
 std::map<std::string, std::wstring> values;
};

// the members ConstructString uses
struct CLocalizedStringTable {
 void ConstructString(wchar_t* unicodeOutput, int unicodeBufferSizeInBytes, StringIndex_t unlocalizedTextSymbol,
  KeyValues* localizationVariables);
 const wchar_t* GetValueByIndex(StringIndex_t index) { return index < 0 ? nullptr : values[index].c_str(); }
 std::vector<std::wstring> values;
};
'''

TESTS = r'''
// the output buffer is exactly capacity characters on the heap, so writing past it is reported
struct Output {
 explicit Output(int capacity) : capacity(capacity), text(new wchar_t[capacity > 0 ? capacity : 1]) {
  std::wmemset(text, L'x', capacity > 0 ? capacity : 1);
 }
 ~Output() { delete[] text; }
 int Bytes() const { return capacity * (int)sizeof(wchar_t); }
 int capacity;
 wchar_t* text;
};

static CLocalizedStringTable table;
static KeyValues variables;

static std::wstring Construct(const wchar_t* format, int capacity) {
 table.values.assign(1, format);
 Output output(capacity);
 table.ConstructString(output.text, output.Bytes(), 0, &variables);
 assert(std::wmemchr(output.text, 0, capacity));
 return output.text;
}

static void VariableStringsAreCutBeforeTheLastCharacter() {
 variables.values["name"] = L"abcdef";
 variables.values["pair"] = L"ab";
 assert(std::wstring(L"abc") == Construct(L"abc", 4));
 assert(std::wstring(L"abc") == Construct(L"abcdef", 4));
 assert(std::wstring(L"xab") == Construct(L"x%name%", 4));
 assert(std::wstring(L"xab") == Construct(L"x%pair%y", 4));
 assert(std::wstring(L"xaby") == Construct(L"x%pair%y", 5));
 assert(std::wstring(L"a%b") == Construct(L"a%%bc", 4));
 assert(std::wstring(L"") == Construct(L"abc", 1));
 puts("PASS: text, variables and escaped percents that fill or overflow the buffer are cut and terminated inside it");
}

static void UnknownStringsAreCutBeforeTheLastCharacter() {
 Output small(4), large(32);
 table.ConstructString(small.text, small.Bytes(), -1, &variables);
 table.ConstructString(large.text, large.Bytes(), -1, &variables);
 assert(std::wstring(L"[un") == small.text);
 assert(std::wstring(L"[unknown string]") == large.text);
 Output none(1);
 table.ConstructString(none.text, (int)sizeof(wchar_t) - 1, -1, &variables);
 assert(L'x' == none.text[0]);
 puts("PASS: an unknown string is cut and terminated, and a buffer smaller than a character is not touched");
}

static std::wstring ConstructWithArgs(const wchar_t* format, const wchar_t* parameter, int capacity) {
 std::wstring formatText = format, parameterText = parameter;
 wchar_t* parameters[] = {&parameterText[0]};
 Output output(capacity);
 ConstructStringFromArgs(output.text, output.Bytes(), &formatText[0], 1, parameters);
 assert(std::wmemchr(output.text, 0, capacity));
 return output.text;
}

static void ArgumentStringsAreCutBeforeTheLastCharacter() {
 assert(std::wstring(L"<ab>") == ConstructWithArgs(L"<%s1>", L"ab", 16));
 assert(std::wstring(L"abc") == ConstructWithArgs(L"%s1", L"abc", 4));
 assert(std::wstring(L"abc") == ConstructWithArgs(L"%s1", L"abcd", 4));
 assert(std::wstring(L"abc") == ConstructWithArgs(L"%s1", L"abcde", 4));
 assert(std::wstring(L"xab") == ConstructWithArgs(L"x%s1", L"abc", 4));
 assert(std::wstring(L"%s2") == ConstructWithArgs(L"%s2", L"abc", 4));
 std::wstring formatText = L"%s1";
 wchar_t* parameters[] = {&formatText[0]};
 Output none(1);
 ConstructStringFromArgs(none.text, 0, &formatText[0], 1, parameters);
 assert(L'x' == none.text[0]);
 puts("PASS: %s parameters that fill or overflow the buffer are cut and terminated inside it, and an empty buffer is not touched");
}

int main() {
 VariableStringsAreCutBeforeTheLastCharacter();
 UnknownStringsAreCutBeforeTheLastCharacter();
 ArgumentStringsAreCutBeforeTheLastCharacter();
}
'''


def main():
    source = (ROOT / 'vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += function(source, 'void CLocalizedStringTable::ConstructString( wchar_t* unicodeOutput, int unicodeBufferSizeInBytes,\n'
                             '\tStringIndex_t unlocalizedTextSymbol, KeyValues* localizationVariables )') + '\n'
    code += function(source, 'static void ConstructStringFromArgs(') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-localize-construct-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
