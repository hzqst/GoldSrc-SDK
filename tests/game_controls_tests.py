"""Exercise production team selection and MOTD navigation with small UI stubs."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def source(name):
    shared = ROOT / 'vgui2/game_controls' / name
    return (shared if shared.exists() else ROOT / 'cstrike/vgui2/game_controls' / name).read_text(encoding='utf-8')


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
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
};
}
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
}
'''


def main():
    team = source('teammenu.cpp')
    start = team.index('void CTeamMenu::OnTeamButton(')
    function = team[start:team.index('\n}', start) + 2]
    html = re.search(r'class ClientMOTDHTML\b.*?\n\};', source('clientmotd.cpp'), re.S).group(0)
    with tempfile.TemporaryDirectory(prefix='goldsrc-game-controls-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(STUBS + function + html + TESTS)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        subprocess.run(compiler + ['-std=c++17', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True, cwd=path)


if __name__ == '__main__':
    main()
