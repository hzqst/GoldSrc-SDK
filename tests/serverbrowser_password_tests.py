"""Exercise the production server browser password handling with stubs.

Run: python tests/serverbrowser_password_tests.py
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
    start = re.search(re.escape(signature) + r'[^;{]*\n\{', source).start()
    return source[start:source.index('\n}', start) + 2]


def array_size(source, member):
    return re.search(r'\b' + re.escape(member) + r'\[(\w+)\]', source).group(1)


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "StringUtils.h"

namespace vgui2 {
struct ILocalize { const wchar_t* Find(const char*) { return nullptr; } };
static ILocalize g_localize;
ILocalize* localize() { return &g_localize; }
struct Panel {};
struct MessageBox {
 MessageBox(const wchar_t* title, const wchar_t* text, Panel*) : title(title), text(text) { shown.push_back(this); }
 void DoModal(Panel*) { modal = true; }
 std::wstring title, text;
 bool modal = false;
 static std::vector<MessageBox*> shown;
};
std::vector<MessageBox*> MessageBox::shown;
}

struct RunGameEngine {
 void AddTextCommand(const char* text) { commands.push_back(text); }
 std::vector<std::string> commands;
};
static RunGameEngine g_runGameEngine, *g_pRunGameEngine = &g_runGameEngine;

struct NetAdr { const char* GetConnectionAddressString() const { return "127.0.0.1:27015"; } };
struct gameserveritem_t { NetAdr m_NetAdr; };

class CDialogGameInfo : public vgui2::Panel {
public:
 void OnJoinServerWithPassword(const char* password);
 void ApplyConnectCommand(const gameserveritem_t& server);
 void ConstructConnectArgs(char* pchOptions, int cchOptions, const gameserveritem_t& server);
 void OnConnect() { connects++; }
 char m_szPassword[PASSWORD_SIZE] = "";
 bool m_bHLTVOnly = false;
 int connects = 0;
};
'''

TESTS = r'''
static const size_t PASSWORD_BUFFER = sizeof(((CDialogGameInfo*)nullptr)->m_szPassword);

static void Reset() {
 for (vgui2::MessageBox* box : vgui2::MessageBox::shown) delete box;
 vgui2::MessageBox::shown.clear();
 g_runGameEngine.commands.clear();
}

static void ValidPasswordsAreSentInsideQuotes() {
 Reset();
 const std::string longest(PASSWORD_BUFFER - 1, 'p');
 for (const std::string& password : {std::string("secret"), std::string("a;b c"), longest}) {
  CDialogGameInfo dialog;
  dialog.OnJoinServerWithPassword(password.c_str());
  assert(1 == dialog.connects);
  assert(vgui2::MessageBox::shown.empty());
  gameserveritem_t server;
  g_runGameEngine.commands.clear();
  dialog.ApplyConnectCommand(server);
  assert(2 == g_runGameEngine.commands.size());
  // ';' inside the quotes cannot split the command
  assert("password \"" + password + "\"\n" == g_runGameEngine.commands[0]);
  char options[512];
  dialog.ConstructConnectArgs(options, sizeof(options), server);
  assert(" +connect 127.0.0.1:27015 +password \"" + password + "\"" == std::string(options));
 }
 puts("PASS: passwords that fit and cannot close their quotes are sent inside quotes");
}

static void PasswordsThatCouldAddCommandsOrDoNotFitAreRefused() {
 const std::string tooLong(PASSWORD_BUFFER, 'p');
 for (const std::string& password : {std::string("a\";quit;\""), std::string("a\nquit"), std::string("a\rquit"), tooLong}) {
  Reset();
  CDialogGameInfo dialog;
  strcpy(dialog.m_szPassword, "old");
  dialog.OnJoinServerWithPassword(password.c_str());
  assert(0 == dialog.connects);
  assert(0 == dialog.m_szPassword[0]);
  assert(1 == vgui2::MessageBox::shown.size());
  assert(vgui2::MessageBox::shown[0]->modal);
  gameserveritem_t server;
  dialog.ApplyConnectCommand(server);
  // only the connect command is sent, without a password
  assert(1 == g_runGameEngine.commands.size());
  assert(std::string("connect 127.0.0.1:27015\n") == g_runGameEngine.commands[0]);
 }
 Reset();
 puts("PASS: passwords that could add commands or would be cut are refused with a message");
}
'''


def main():
    source = (ROOT / 'ServerBrowser/DialogGameInfo.cpp').read_text(encoding='utf-8')
    header = (ROOT / 'ServerBrowser/DialogGameInfo.h').read_text(encoding='utf-8')
    code = '#define PASSWORD_SIZE ' + array_size(header, 'm_szPassword') + '\n' + STUBS
    code += ''.join(function(source, signature) for signature in (
        'void CDialogGameInfo::OnJoinServerWithPassword(', 'void CDialogGameInfo::ApplyConnectCommand(',
        'void CDialogGameInfo::ConstructConnectArgs('))
    code += TESTS
    code += '\nint main() { ValidPasswordsAreSentInsideQuotes(); PasswordsThatCouldAddCommandsOrDoNotFitAreRefused(); }\n'
    with tempfile.TemporaryDirectory(prefix='goldsrc-serverbrowser-password-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'common/StringUtils.cpp'], path / 'test',
                                     includes=[ROOT / 'common'], sanitizers=['address', 'undefined'],
                                     defines=['_CRT_SECURE_NO_WARNINGS'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
