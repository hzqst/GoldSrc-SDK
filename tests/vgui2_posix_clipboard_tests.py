"""Exercise the production POSIX VGUI2 wide clipboard functions and TextEntry::CopySelected on a stub SDL clipboard, under the C and a UTF-8 LC_CTYPE.

Run: python tests/vgui2_posix_clipboard_tests.py
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
# more UTF-8 bytes than the 2048 byte buffers the clipboard used to go through
LONG_TEXT_CHARACTERS = 3000
TIMEOUT_SECONDS = 60


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

struct CLocalizedStringTable {
 int ConvertANSIToUnicode(const char*, wchar_t*, int);
 int ConvertUnicodeToANSI(const wchar_t*, char*, int);
};
static CLocalizedStringTable g_StringTable;

// the SDL clipboard holds UTF-8 text, empty means no text
static std::string clipboard;
static bool hasClipboard = false;
static int SDL_SetClipboardText(const char* text) { clipboard = text; hasClipboard = true; return 0; }
static char* SDL_GetClipboardText() {
 if (!hasClipboard) return nullptr;
 char* text = (char*)malloc(clipboard.size() + 1);
 memcpy(text, clipboard.c_str(), clipboard.size() + 1);
 return text;
}
static void SDL_free(void* p) { free(p); }

static void SetClipboard(const char* text) { clipboard = text; hasClipboard = true; }

class CSystem {
public:
 void SetClipboardText(const char* text, int textLen);
 void SetClipboardText(const wchar_t* text, int textLen);
 int GetClipboardTextCount();
 int GetClipboardText(int offset, wchar_t* buf, int bufLen);
};
static CSystem systemObject;

namespace vgui2 {
static CSystem* system() { return &systemObject; }

template <class T> class CUtlVector {
public:
 int Count() const { return (int)items.size(); }
 T& operator[](int i) { return items[i]; }
 T* Base() { return items.data(); }
 void AddToTail(T t) { items.push_back(t); }
 std::vector<T> items;
};

// only the members CopySelected uses
class TextEntry {
public:
 void CopySelected();
 bool GetSelectedRange(int& x0, int& x1) { x0 = select0; x1 = select1; return x0 != x1; }
 void RequestFocus(int) {}
 void FireActionSignal() {}
 CUtlVector<wchar_t> m_TextStream;
 bool _hideText = false;
 bool _dataChanged = false;
 int select0 = 0;
 int select1 = 0;
};
'''

TESTS = r'''

static std::wstring Repeat(const wchar_t* text, int count) {
 std::wstring result;
 for (int i = 0; i < count; ++i) result += text;
 return result;
}

static std::string Repeat(const char* text, int count) {
 std::string result;
 for (int i = 0; i < count; ++i) result += text;
 return result;
}

static void CopiesAsUtf8() {
 hasClipboard = false;
 systemObject.SetClipboardText(L"ab\u4e2dcd", 5);
 assert(std::string("ab\xe4\xb8\xad" "cd") == clipboard);
 systemObject.SetClipboardText(L"\U0001F600", (int)wcslen(L"\U0001F600"));
 assert(std::string("\xf0\x9f\x98\x80") == clipboard);
 const wchar_t loneSurrogate[] = {L'a', (wchar_t)0xD800, 0};
 systemObject.SetClipboardText(loneSurrogate, 2);
 assert(std::string("a\xef\xbf\xbd") == clipboard);
}

static void CopiesAtMostTextLen() {
 systemObject.SetClipboardText(L"ab\u4e2dcd", 3);
 assert(std::string("ab\xe4\xb8\xad") == clipboard);
 systemObject.SetClipboardText(L"ab", 100);
 assert(std::string("ab") == clipboard);
 SetClipboard("kept");
 systemObject.SetClipboardText(L"ab", 0);
 systemObject.SetClipboardText((const wchar_t*)nullptr, 2);
 assert(std::string("kept") == clipboard);
}

static void CopiesLongText() {
 std::wstring text = Repeat(L"\u4e2d", LONG_TEXT_CHARACTERS);
 systemObject.SetClipboardText(text.c_str(), (int)text.size());
 assert(Repeat("\xe4\xb8\xad", LONG_TEXT_CHARACTERS) == clipboard);
}

static void PastesUtf8() {
 wchar_t buf[64];
 SetClipboard("ab\xe4\xb8\xad" "cd");
 assert(6 == systemObject.GetClipboardText(0, buf, (int)sizeof(buf)));
 assert(std::wstring(L"ab\u4e2dcd") == buf);
 SetClipboard("\xf0\x9f\x98\x80");
 systemObject.GetClipboardText(0, buf, (int)sizeof(buf));
 assert(std::wstring(L"\U0001F600") == buf);
 SetClipboard("a\xff" "b");
 assert(4 == systemObject.GetClipboardText(0, buf, (int)sizeof(buf)));
 assert(std::wstring(L"a\ufffdb") == buf);
 SetClipboard("");
 assert(1 == systemObject.GetClipboardText(0, buf, (int)sizeof(buf)));
 assert(0 == buf[0]);
}

// TextEntry::Paste sizes the buffer with GetClipboardTextCount
static void PastesLongText() {
 SetClipboard(Repeat("\xe4\xb8\xad", LONG_TEXT_CHARACTERS).c_str());
 int bufferSize = systemObject.GetClipboardTextCount();
 std::vector<wchar_t> buf(bufferSize);
 assert(LONG_TEXT_CHARACTERS + 1 == systemObject.GetClipboardText(0, buf.data(), bufferSize * (int)sizeof(wchar_t)));
 assert(Repeat(L"\u4e2d", LONG_TEXT_CHARACTERS) == buf.data());
}

static void PastesWholeCharactersThatFit() {
 wchar_t buf[4];
 SetClipboard("ab\xe4\xb8\xad" "cd");
 assert(4 == systemObject.GetClipboardText(0, buf, (int)sizeof(buf)));
 assert(std::wstring(L"ab\u4e2d") == buf);
 // U+1F600 is two units where wchar_t is 16 bits, and is not split
 wchar_t one[2];
 SetClipboard("a\xf0\x9f\x98\x80");
 systemObject.GetClipboardText(0, one, (int)sizeof(one));
 assert(std::wstring(L"a") == one);
}

static void PasteRejectsNoBufferOrText() {
 wchar_t buf[2] = {L'x', L'x'};
 SetClipboard("ab");
 assert(0 == systemObject.GetClipboardText(0, (wchar_t*)nullptr, (int)sizeof(buf)));
 assert(0 == systemObject.GetClipboardText(0, buf, (int)sizeof(wchar_t) - 1));
 assert(L'x' == buf[0]);
 hasClipboard = false;
 assert(0 == systemObject.GetClipboardText(0, buf, (int)sizeof(buf)));
 assert(0 == buf[0]);
}

static void RunClipboard(const char* locale) {
 CopiesAsUtf8();
 CopiesAtMostTextLen();
 CopiesLongText();
 PastesUtf8();
 PastesLongText();
 PastesWholeCharactersThatFit();
 PasteRejectsNoBufferOrText();
 printf("PASS: the wide clipboard is UTF-8, copies textLen characters and pastes long or cut text under LC_CTYPE %s\n", locale);
}

static void TextEntryCopiesNewlinesWhole() {
 vgui2::TextEntry entry;
 for (const wchar_t* c = L"a\nb\nc"; *c; ++c) entry.m_TextStream.AddToTail(*c);
 entry.select0 = 0;
 entry.select1 = entry.m_TextStream.Count();
 entry.CopySelected();
 assert(std::string("a\r\nb\r\nc") == clipboard);
 puts("PASS: TextEntry::CopySelected counts the carriage returns it adds");
}

int main() {
 assert(setlocale(LC_CTYPE, "C"));
 RunClipboard("C");
 TextEntryCopiesNewlinesWhole();
 const char* utf8Locales[] = {"C.UTF-8", "C.utf8", "en_US.UTF-8", ".UTF-8"};
 for (const char* locale : utf8Locales) {
  if (setlocale(LC_CTYPE, locale)) {
   RunClipboard(locale);
   return 0;
  }
 }
 puts("FAIL: no UTF-8 LC_CTYPE is available");
 return 1;
}
'''


def main():
    table = (ROOT / 'vgui2/src/LocalizedStringTable.cpp').read_text(encoding='utf-8')
    posix = (ROOT / 'vgui2/src/system_posix.cpp').read_text(encoding='utf-8')
    text_entry = (ROOT / 'vgui2/controls/TextEntry.cpp').read_text(encoding='utf-8')
    code = f'#define LONG_TEXT_CHARACTERS {LONG_TEXT_CHARACTERS}\n' + STUBS
    code += ''.join(function(text_entry, signature) + '\n' for signature in ('void TextEntry::CopySelected()',))
    code += '}\n'
    code += ''.join(function(table, signature) + '\n' for signature in (
        'static int DecodeUTF8CodePoint(', 'static int EncodeWideCodePoint(',
        'int CLocalizedStringTable::ConvertANSIToUnicode(',
        'static int DecodeWideCodePoint(', 'static int EncodeUTF8CodePoint(',
        'int CLocalizedStringTable::ConvertUnicodeToANSI('))
    code += ''.join(function(posix, signature) + '\n' for signature in (
        'static int Q_UnicodeToUTF8(', 'static int Q_UTF8ToUnicode(',
        'void CSystem::SetClipboardText( const char* text, int )',
        'void CSystem::SetClipboardText( const wchar_t* text, int textLen )',
        'int CSystem::GetClipboardTextCount()',
        'int CSystem::GetClipboardText( int, wchar_t* buf, int bufLen )'))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-posix-clipboard-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
