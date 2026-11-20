"""Exercise production team selection and MOTD handling with small UI stubs."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def source(name):
    return (ROOT / 'vgui2/game_controls' / name).read_text(encoding='utf-8')


STUBS = r'''
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <strings.h>
#define _strnicmp strncasecmp
#define _snprintf snprintf
#endif
namespace vgui2 {
struct Panel {};
struct HTML {
 HTML(Panel*, const char*, bool, bool) {}
 virtual ~HTML() = default;
 virtual bool OnStartRequest(const char*, const char*, const char*, bool) { return true; }
 std::vector<std::string> urls;
 void OpenURL(const char* url, const char*, bool) { urls.emplace_back(url); }
};
struct Label { void SetText(const wchar_t*) {} };
struct Frame {
 bool visible = false;
 void Activate() {}
 void SetTitle(const wchar_t*, bool) {}
 void SetVisible(bool value) { visible = value; }
};
typedef void* FileHandle_t;
struct FileSystem {
 std::string written;
 bool exists = false;
 bool FileExists(const char*) { return exists; }
 void RemoveFile(const char*, const char*) { exists = false; }
 FileHandle_t Open(const char*, const char*, const char*) { written.clear(); return this; }
 void Write(const void* data, int size, FileHandle_t) { written.append((const char*)data, size); exists = true; }
 void Close(FileHandle_t) {}
 void GetLocalPath(const char* name, char* path, int size) { std::snprintf(path, size, "%s", name); }
} files;
FileSystem* filesystem() { return &files; }
struct Localize {
 int ConvertUnicodeToANSI(const wchar_t* text, char* ansi, int size) {
  int i = 0;
  for (; text[i] && i < size - 1; ++i)
   ansi[i] = (char)text[i];
  ansi[i] = 0;
  return i;
 }
} localization;
Localize* localize() { return &localization; }
}
using vgui2::FileHandle_t;
std::vector<std::string> commands;
int hints = 0;
void SendCommand(const char* text) { commands.emplace_back(text); }
struct Callbacks { void (*CallEnghudClientCmd)(const char*) = SendCommand; } callbacks;
struct Viewport {
 int hides = 0;
 void HideBackGround() { ++hides; }
 Callbacks* GetClientDllInterface() { return &callbacks; }
} viewport, *gViewPortInterface = &viewport;
void DuckMessage(const char*) { ++hints; }
struct CTeamMenu {
 int m_iNumTeams = 2;
 int closes = 0;
 bool visible = true;
 void SetVisible(bool value) { visible = value; }
 void OnClose() { ++closes; }
 void OnTeamButton(int team);
};
struct CClientMOTD : vgui2::Frame {
 vgui2::HTML* m_pMessage;
 vgui2::Label* m_pServerName;
 char m_szTempFileName[64] = "motd_temp.html";
 bool IsURL(const char* text);
 bool ShowMessage(const char* message);
 void Activate(const wchar_t* title, const wchar_t* message);
};
'''

TESTS = r'''
int main() {
 CTeamMenu menu;
 menu.OnTeamButton(0);
 menu.OnTeamButton(1);
 menu.OnTeamButton(2);
 assert(3 == commands.size());
 assert(0 == std::strcmp("jointeam 1", commands[0].c_str()));
 assert(0 == std::strcmp("jointeam 2", commands[1].c_str()));
 assert(0 == std::strcmp("jointeam 5", commands[2].c_str()));
 commands.clear();
 menu.OnTeamButton(3);
 assert(commands.empty());
 assert(1 == hints && 1 == viewport.hides);
 assert(!menu.visible && 4 == menu.closes);
 ClientMOTDHTML html(nullptr, "test");
 vgui2::HTML* browser = &html;
 assert(!browser->OnStartRequest("steam://run/10", "", "", true));
 assert(!browser->OnStartRequest("StEaM://connect/localhost", "", "", false));
 assert(browser->OnStartRequest("https://example.invalid/motd", "", "", true));
 assert(browser->OnStartRequest("file:///motd_temp.html", "", "", false));
 assert(browser->OnStartRequest("", "", "", true));
 vgui2::Label server;
 CClientMOTD motd;
 motd.m_pMessage = &html;
 motd.m_pServerName = &server;
 motd.Activate(L"server", L"<b>welcome</b>");
 assert(motd.visible);
 assert(std::string("<b>welcome</b>") == vgui2::files.written);
 assert(1 == html.urls.size() && std::string("file:///motd_temp.html") == html.urls[0]);
 motd.visible = false;
 motd.Activate(L"server", L"<img src=\"view-source:x\">");
 assert(!motd.visible && !vgui2::files.exists && 1 == html.urls.size());
 motd.Activate(L"server", L"http://example.invalid/motd");
 assert(motd.visible && 2 == html.urls.size() && std::string("http://example.invalid/motd") == html.urls[1]);
 motd.visible = false;
 motd.Activate(L"server", L"https://example.invalid/motd");
 assert(motd.visible && !vgui2::files.exists && 3 == html.urls.size());
 assert(std::string("https://example.invalid/motd") == html.urls[2]);
 motd.Activate(L"server", L"HTTPS://example.invalid/motd");
 assert(!vgui2::files.exists && 4 == html.urls.size() && std::string("HTTPS://example.invalid/motd") == html.urls[3]);
 motd.Activate(L"server", L"https://");
 assert(vgui2::files.exists && std::string("https://") == vgui2::files.written);
 assert(5 == html.urls.size() && std::string("file:///motd_temp.html") == html.urls[4]);
}
'''


def function(text, signature):
    start = text.index(signature)
    return text[start:text.index('\n}', start) + 2] + '\n'


def main():
    team = function(source('teammenu.cpp'), 'void CTeamMenu::OnTeamButton(')
    motd = source('clientmotd.cpp')
    html = re.search(r'class ClientMOTDHTML\b.*?\n\};', motd, re.S).group(0)
    limit = re.search(r'static const size_t MaxMultiByteCharLength = \d+;', motd).group(0)
    motd_functions = ''.join(function(motd, signature) for signature in (
        'bool CClientMOTD::IsURL(',
        'bool CClientMOTD::ShowMessage(',
        'void CClientMOTD::Activate( const wchar_t*'))
    with tempfile.TemporaryDirectory(prefix='goldsrc-game-controls-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text('\n'.join((STUBS, team, html, limit, motd_functions, TESTS)))
        executable = compile_program([path / 'test.cpp'], path / 'test')
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
