"""Exercise production server browser filter normalization and filter descriptions with stubs.

Run: python tests/serverbrowser_filter_string_tests.py
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
    start = re.search(re.escape(signature) + r'[^\n;]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <clocale>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <ctype.h>
#include <map>
#include <string>
#include <wchar.h>
namespace vgui2 {
 struct Localize {
  std::map<std::string, std::wstring> tokens;
  const wchar_t* Find(const char* token) { auto entry=tokens.find(token); return entry==tokens.end()?nullptr:entry->second.c_str(); }
  // like the production converter: stops at the last whole character that fits
  int ConvertANSIToUnicode(const char* ansi, wchar_t* unicode, int bytes) {
   int capacity=bytes/int(sizeof(wchar_t)), written=0;
   for (; ansi && *ansi && written<capacity-1; ++ansi) unicode[written++]=(unsigned char)*ansi;
   unicode[written]=0; return written;
  }
 } localizer;
 Localize* localize() { return &localizer; }
}
struct CGameID {};
struct CModList { const char* name="Half-Life"; const char* GetModNameForModDir(const CGameID&) { return name; } } modList;
CModList& ModList() { return modList; }
struct Label { std::wstring text; void SetText(const wchar_t* value) { text=value; } };
class CBaseGamesPage {
public:
 void RecalculateFilterString();
 char m_szGameFilter[32]={}; char m_szMapFilter[32]={};
 CGameID m_iLimitToAppID; int m_iSecureFilter=0, m_iPingFilter=0;
 bool m_bFilterNoFullServers=false, m_bFilterNoEmptyServers=false;
 bool m_bFilterNoPasswordedServers=false, m_bFilterValidSteamAccount=false;
 Label label, *m_pFilterString=&label;
};
'''

TESTS = r'''
static const char* TOKENS[]={"#ServerBrowser_FilterDescSecureOnly", "#ServerBrowser_FilterDescInsecureOnly",
 "#ServerBrowser_FilterDescLatency", "#ServerBrowser_FilterDescNotFull", "#ServerBrowser_FilterDescNotEmpty",
 "#ServerBrowser_FilterDescNoPassword", "#ServerBrowser_FilterDescValidSteamAccount"};

static void EnableAll(CBaseGamesPage& page) {
 strcpy(page.m_szGameFilter, "valve"); strcpy(page.m_szMapFilter, "de_dust");
 page.m_iSecureFilter=1; page.m_iPingFilter=50;
 page.m_bFilterNoFullServers=page.m_bFilterNoEmptyServers=true;
 page.m_bFilterNoPasswordedServers=page.m_bFilterValidSteamAccount=true;
}

static void DescriptionListsActiveFilters() {
 vgui2::localizer.tokens={{TOKENS[0], L"Secure"}, {TOKENS[2], L"Latency"}, {TOKENS[3], L"Not full"},
  {TOKENS[4], L"Not empty"}, {TOKENS[5], L"No password"}, {TOKENS[6], L"Valid Steam account"}};
 CBaseGamesPage page; EnableAll(page);
 page.RecalculateFilterString();
 assert(std::wstring(L"Half-Life; Secure; Latency < 50; Not full; Not empty; No password; Valid Steam account; de_dust")
  ==page.label.text);
 CBaseGamesPage empty; empty.RecalculateFilterString();
 assert(empty.label.text.empty());
 puts("PASS: the description lists the active filters");
}

static void MissingTokensShowTheirNames() {
 vgui2::localizer.tokens.clear();
 CBaseGamesPage page; EnableAll(page); page.m_iSecureFilter=2;
 page.RecalculateFilterString();
 assert(std::wstring(L"Half-Life; #ServerBrowser_FilterDescInsecureOnly; #ServerBrowser_FilterDescLatency < 50; "
  L"#ServerBrowser_FilterDescNotFull; #ServerBrowser_FilterDescNotEmpty; #ServerBrowser_FilterDescNoPassword; "
  L"#ServerBrowser_FilterDescValidSteamAccount; de_dust")==page.label.text);
 puts("PASS: missing localization tokens show the token name");
}

static void LongTranslationsAreTruncated() {
 vgui2::localizer.tokens.clear();
 for (const char* token : TOKENS) vgui2::localizer.tokens[token]=std::wstring(1500, L'x');
 CBaseGamesPage page; EnableAll(page);
 page.RecalculateFilterString();
 assert(2047==page.label.text.size());
 assert(std::wstring(L"Half-Life; ")+std::wstring(1500, L'x')+L"; "+std::wstring(2047-11-1500-2, L'x')==page.label.text);
 puts("PASS: descriptions longer than the buffer are truncated");
}

static void LowercaseKeepsUTF8Bytes() {
#if defined(_WIN32)
 // vgui2 switches LC_CTYPE to the user locale; code page 1252 lowercases 0xC0-0xDE
 bool latin1=setlocale(LC_CTYPE, ".1252")!=nullptr;
 if (latin1) assert(0xE3==tolower(0xC3));
#endif
 char text[32]="DE_Dust\xC3\x89Z";
 LowercaseASCII(text);
 assert(0==strcmp("de_dust\xC3\x89z", text));
 setlocale(LC_CTYPE, "C");
 puts("PASS: lowercasing only changes A-Z and keeps UTF-8 bytes");
}
'''


def main():
    source = (ROOT / 'serverbrowser/BaseGamesPage.cpp').read_text(encoding='utf-8')
    code = STUBS
    for signature in ['static void LowercaseASCII(', 'static void AppendFilterText(',
                      'static void AppendFilterANSI(', 'static void AppendFilterToken(',
                      'void CBaseGamesPage::RecalculateFilterString()']:
        code += function(source, signature)
    code += TESTS
    code += ('\nint main() { DescriptionListsActiveFilters(); MissingTokensShowTheirNames(); '
             'LongTranslationsAreTruncated(); LowercaseKeepsUTF8Bytes(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-serverbrowser-filter-string-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
