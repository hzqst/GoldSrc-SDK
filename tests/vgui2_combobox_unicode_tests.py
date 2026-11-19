"""Exercise the production VGUI2 ComboBox::AddItem( const wchar_t* ) with the production UTF-8 converter under the C and a UTF-8 LC_CTYPE.

Run: python tests/vgui2_combobox_unicode_tests.py
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
TIMEOUT_SECONDS = 60


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

struct CLocalizedStringTable {
 int ConvertUnicodeToANSI(const wchar_t*, char*, int);
};
'''

COMBOBOX = r'''
static CLocalizedStringTable table;

namespace vgui2 {
class Panel {};

class KeyValues {
public:
 KeyValues(const char*, const char*, const wchar_t* value) : text(value) {}
 std::wstring text;
};

struct ILocalize {
 int ConvertUnicodeToANSI(const wchar_t* unicode, char* ansi, int ansiBufferSize) {
  return table.ConvertUnicodeToANSI(unicode, ansi, ansiBufferSize);
 }
};

static ILocalize localizer;
ILocalize* localize() { return &localizer; }

struct AddedItem {
 std::string name;
 std::wstring text;
 std::unique_ptr<KeyValues> command;
};

// records each item, IDs count up from 0
class Menu {
public:
 int GetInvalidMenuID() { return -1; }
 int AddMenuItem(const char* itemName, const wchar_t* itemText, KeyValues* command, Panel*, const KeyValues*) {
  items.push_back({itemName, itemText, std::unique_ptr<KeyValues>(command)});
  return (int)items.size() - 1;
 }
 std::vector<AddedItem> items;
};

class ComboBox : public Panel {
public:
 int AddItem(const wchar_t* itemText, const KeyValues* userData);
 Menu* m_pDropDown = nullptr;
};
'''

TESTS = r'''
}
using vgui2::ComboBox;
using vgui2::Menu;

struct Fixture {
 Fixture() { box.m_pDropDown = &menu; }
 Menu menu;
 ComboBox box;
};

// adds the text and checks the ID, the UTF-8 menu name and the wide text sent on selection
static void CheckAdded(const wchar_t* text, const char* expectedName, const wchar_t* expectedText) {
 Fixture f;
 assert(0 == f.box.AddItem(text, nullptr));
 assert(1 == (int)f.menu.items.size());
 assert(std::string(expectedName) == f.menu.items[0].name);
 assert(std::wstring(text) == f.menu.items[0].text);
 assert(std::wstring(expectedText) == f.menu.items[0].command->text);
}

static void AddsCaptionsAsUtf8(const char* locale) {
 CheckAdded(L"\u4e2d\u6587", "\xe4\xb8\xad\xe6\x96\x87", L"\u4e2d\u6587");
 CheckAdded(L"de_dust2", "de_dust2", L"de_dust2");
 CheckAdded(L"", "", L"");
 CheckAdded(L"caf\u00e9 \u4e2d", "caf\xc3\xa9 \xe4\xb8\xad", L"caf\u00e9 \u4e2d");
 // U+1F600 is a surrogate pair where wchar_t is 16 bits
 CheckAdded(L"\U0001F600", "\xf0\x9f\x98\x80", L"\U0001F600");
 printf("PASS: AddItem adds UTF-8 captions with valid IDs under LC_CTYPE %s\n", locale);
}

static void ReplacesALoneSurrogate() {
 const wchar_t text[] = {L'a', (wchar_t)0xD800, L'b', 0};
 Fixture f;
 assert(0 == f.box.AddItem(text, nullptr));
 assert(std::string("a\xef\xbf\xbd" "b") == f.menu.items[0].name);
 assert(std::wstring(text) == f.menu.items[0].command->text);
 puts("PASS: a lone surrogate becomes U+FFFD in the menu name");
}

static void RejectsNull() {
 Fixture f;
 assert(-1 == f.box.AddItem(nullptr, nullptr));
 assert(f.menu.items.empty());
 puts("PASS: a NULL caption returns the invalid menu ID");
}

int main() {
 assert(setlocale(LC_CTYPE, "C"));
 AddsCaptionsAsUtf8("C");
 ReplacesALoneSurrogate();
 RejectsNull();
 const char* utf8Locales[] = {"C.UTF-8", "C.utf8", "en_US.UTF-8", ".UTF-8"};
 for (const char* locale : utf8Locales) {
  if (setlocale(LC_CTYPE, locale)) {
   AddsCaptionsAsUtf8(locale);
   return 0;
  }
 }
 puts("FAIL: no UTF-8 LC_CTYPE is available");
 return 1;
}
'''


def main():
    table = (ROOT / 'vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    combo = (ROOT / 'vgui2/controls/ComboBox.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += ''.join(function(table, signature) + '\n' for signature in (
        'static int DecodeWideCodePoint(', 'static int EncodeUTF8CodePoint(',
        'int CLocalizedStringTable::ConvertUnicodeToANSI('))
    code += COMBOBOX
    code += function(combo, 'int ComboBox::AddItem( const wchar_t* itemText, const KeyValues* userData )') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-combobox-unicode-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
