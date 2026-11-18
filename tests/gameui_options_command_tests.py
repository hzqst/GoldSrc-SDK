"""Exercise the production GameUI options commands (model, logo, name, script cvars, key bindings) with stubs.

Run: python tests/gameui_options_command_tests.py
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


def block(source, start_pattern):
    start = re.search(start_pattern, source, re.M).start()
    return source[start:source.index('\n};', start) + 3] + '\n'


def array_size(source, member):
    return re.search(r'\b' + re.escape(member) + r'\[(\w+)\]', source).group(1)


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#ifdef _WIN32
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif
#define override
#include "StringUtils.h"

typedef void* FileHandle_t;
typedef int FileFindHandle_t;

struct Engine {
 static int ClientCmd(char* command) { commands.push_back(command); return 0; }
 static const char* GetCvarString(char* name) { auto it = cvars.find(name); return it == cvars.end() ? "" : it->second.c_str(); }
 static void CvarSet(const char* name, const char* value) { cvars[name] = value; }
 int (*pfnClientCmd)(char*) = ClientCmd;
 const char* (*pfnGetCvarString)(char*) = GetCvarString;
 void (*Cvar_Set)(const char*, const char*) = CvarSet;
 static std::vector<std::string> commands;
 static std::map<std::string, std::string> cvars;
};
std::vector<std::string> Engine::commands;
std::map<std::string, std::string> Engine::cvars;
static Engine g_engine;
static Engine* engine = &g_engine;

// directory listings by FindFirst pattern; a trailing '/' marks a directory
namespace vgui2 {
struct IFileSystem {
 std::map<std::string, std::vector<std::string>> listings;
 const std::vector<std::string>* current = nullptr;
 size_t position = 0;
 void RemoveFile(const char*, const char*) {}
 const char* Name() {
  if (position >= current->size()) return nullptr;
  name = (*current)[position];
  if (!name.empty() && name.back() == '/') name.pop_back();
  return name.c_str();
 }
 const char* FindFirst(const char* pattern, FileFindHandle_t*, const char*) { current = &listings[pattern]; position = 0; return Name(); }
 const char* FindNext(FileFindHandle_t) { position++; return Name(); }
 bool FindIsDirectory(FileFindHandle_t) { return current->at(position).back() == '/'; }
 void FindClose(FileFindHandle_t) {}
 std::string name;
};
static IFileSystem g_fileSystem;
IFileSystem* filesystem() { return &g_fileSystem; }
}

struct CLabeledCommandComboBox {
 bool HasItem(const char* text) { for (auto& item : items) if (item.first == text) return true; return false; }
 void AddItem(const char* text, const char* command) { items.push_back({text, command}); }
 void SetInitialItem(int) {}
 std::vector<std::pair<std::string, std::string>> items;
};

struct CUtlSymbol {
 CUtlSymbol(const char* text) : text(text) {}
 const char* String() const { return text.c_str(); }
 std::string text;
};
static bool CaselessStringLessThan(const CUtlSymbol& lhs, const CUtlSymbol& rhs) { return stricmp(lhs.String(), rhs.String()) < 0; }
template <class T, class I> struct CUtlRBTree {
 CUtlRBTree(int, int, bool (*)(const T&, const T&)) {}
 I InvalidIndex() const { return -1; }
 I Find(const T& value) const { for (size_t i = 0; i < values.size(); i++) if (!stricmp(values[i].String(), value.String())) return (I)i; return -1; }
 void Insert(const T& value) { values.push_back(value); }
 I FirstInorder() const { return values.empty() ? -1 : 0; }
 I NextInorder(I index) const { return index + 1 < (I)values.size() ? index + 1 : -1; }
 const T& operator[](I index) const { return values[index]; }
 std::vector<T> values;
};

class COptionsSubMultiplayer {
public:
 void InitModelList(CLabeledCommandComboBox* combo);
 void InitLogoList(CLabeledCommandComboBox* combo);
 char m_LogoName[LOGO_NAME_SIZE];
};

class CCvarTextEntry {
public:
 void ApplyChanges(bool immediate);
 void GetText(char* buffer, int size) { snprintf(buffer, size, "%s", typed.c_str()); }
 char* m_pszCvarName = (char*)"name";
 char m_pszStartValue[START_VALUE_SIZE] = "";
 std::string typed;
};

class COptionsSubKeyboard {
public:
 void BindKey(const char* key, const char* binding);
 void UnbindKey(const char* key);
};
'''

TESTS = r'''
static void Clear() { Engine::commands.clear(); }

static void ModelDirectoriesThatCouldAddCommandsAreNotListed() {
 vgui2::g_fileSystem.listings["models/player/*.*"] = {"gign/", "a;quit/", "b\"c/", "file.mdl", std::string(250, 'm') + "/", "leet/"};
 CLabeledCommandComboBox combo;
 COptionsSubMultiplayer page;
 page.InitModelList(&combo);
 assert(2 == combo.items.size());
 assert(std::string("gign") == combo.items[0].first);
 assert(std::string("model gign\n") == combo.items[0].second);
 assert(std::string("model leet\n") == combo.items[1].second);
 puts("PASS: model directories that could add commands or do not fit in the command are not listed");
}

static void LogoNamesThatCouldAddCommandsOrBeCutAreNotListed() {
 const std::string longest(sizeof(((COptionsSubMultiplayer*)nullptr)->m_LogoName) - 1, 'l');
 vgui2::g_fileSystem.listings["logos/*.bmp"] = {"skull.bmp", "a;quit.bmp", "b\nc.bmp", longest + ".bmp", longest + "x.bmp", "remapped.bmp"};
 CLabeledCommandComboBox combo;
 COptionsSubMultiplayer page;
 page.InitLogoList(&combo);
 assert(2 == combo.items.size());
 assert(std::string("skull") == combo.items[0].first);
 assert(longest == combo.items[1].first);
 puts("PASS: logo names that could add commands or would be cut by m_LogoName are not listed");
}

static void NameTextCannotCloseItsQuotes() {
 CCvarTextEntry entry;
 entry.typed = "Player\";quit;\"\n";
 Clear();
 entry.ApplyChanges(false);
 assert(1 == Engine::commands.size());
 assert(std::string("name \"Player;quit;\"\n") == Engine::commands[0]);
 entry.typed = "\"\"";
 Clear();
 entry.ApplyChanges(false);
 assert(Engine::commands.empty());
 // the immediate path sets the cvar directly and never builds a command
 entry.typed = "a\"b";
 entry.ApplyChanges(true);
 assert(Engine::commands.empty());
 assert(std::string("a\"b") == Engine::cvars["name"]);
 puts("PASS: the name entry drops quotes and line breaks before it goes inside the quoted command");
}

static CScriptObject Script(const char* cvar, objtype_t type, const char* value) {
 CScriptObject object;
 strcpy(object.cvarname, cvar);
 object.type = type;
 strcpy(object.curValue, value);
 return object;
}

static void ScriptCvarCommandsCannotBeSplit() {
 CScriptObject text = Script("hostname", O_STRING, "My \"Server\";quit\n");
 Clear();
 text.WriteToConfig();
 assert(std::string("hostname \"My Server;quit\"\n") == Engine::commands.at(0));
 CScriptObject badName = Script("mp_a;quit", O_BOOL, "");
 Clear();
 badName.WriteToConfig();
 assert(Engine::commands.empty());
 puts("PASS: script cvar names that could add commands are skipped and values cannot close their quotes");
}

static void LongScriptCommandsAreNotCut() {
 // setinfo plus a 63 byte name and a 255 byte list value used to overflow char command[256]
 const std::string name(sizeof(((CScriptObject*)nullptr)->cvarname) - 1, 'n');
 const std::string value(sizeof(((CScriptListItem*)nullptr)->szValue) - 1, 'v');
 CScriptListItem item("item", value.c_str());
 CScriptObject object = Script(name.c_str(), O_LIST, "");
 object.pListItems = &item;
 object.fcurValue = 0;
 object.bSetInfo = true;
 Clear();
 object.WriteToConfig();
 assert("setinfo " + name + " \"" + value + "\"\n" == Engine::commands.at(0));
 puts("PASS: the longest setinfo command is sent whole");
}

static void BindingsCannotCloseTheirQuotes() {
 COptionsSubKeyboard keyboard;
 Clear();
 keyboard.BindKey("MOUSE1", "+attack;wait");
 keyboard.BindKey("w", "+forward\";quit;\"");
 keyboard.BindKey("a\"", "+moveleft");
 keyboard.BindKey("e", "+use\n");
 keyboard.UnbindKey("x");
 keyboard.UnbindKey("y\"");
 assert(2 == Engine::commands.size());
 assert(std::string("bind \"MOUSE1\" \"+attack;wait\"\n") == Engine::commands[0]);
 assert(std::string("unbind \"x\"\n") == Engine::commands[1]);
 const std::string longBinding(1500, 'b');
 Clear();
 keyboard.BindKey("k", longBinding.c_str());
 assert("bind \"k\" \"" + longBinding + "\"\n" == Engine::commands.at(0));
 puts("PASS: bindings keep ';' but cannot close their quotes, and long bindings are not cut");
}
'''


def main():
    gameui = ROOT / 'GameUI'
    multiplayer_header = (gameui / 'OptionsSubMultiplayer.h').read_text(encoding='utf-8')
    entry_header = (gameui / 'CvarTextEntry.h').read_text(encoding='utf-8')
    script_header = (gameui / 'ScriptObject.h').read_text(encoding='utf-8')
    multiplayer = (gameui / 'OptionsSubMultiplayer.cpp').read_text(encoding='utf-8')
    entry = (gameui / 'CvarTextEntry.cpp').read_text(encoding='utf-8')
    script = (gameui / 'ScriptObject.cpp').read_text(encoding='utf-8')
    keyboard = (gameui / 'OptionsSubKeyboard.cpp').read_text(encoding='utf-8')
    code = ('#define LOGO_NAME_SIZE ' + array_size(multiplayer_header, 'm_LogoName') + '\n'
            '#define START_VALUE_SIZE ' + array_size(entry_header, 'm_pszStartValue') + '\n' + STUBS)
    code += block(script_header, r'^enum objtype_t\b') + block(script_header, r'^class CScriptListItem\b')
    code += block(script_header, r'^class CScriptObject\b').replace(' : public vgui2::Panel', '')
    code += 'CScriptObject::~CScriptObject() {}\n'
    code += ''.join(function(script, signature) for signature in (
        'CScriptListItem::CScriptListItem( const char* item', 'CScriptObject::CScriptObject(',
        'void UTIL_StripInvalidCharacters(', 'void CScriptObject::WriteToConfig('))
    code += function(multiplayer, 'void COptionsSubMultiplayer::InitModelList(')
    code += function(multiplayer, 'void COptionsSubMultiplayer::InitLogoList(')
    code += function(entry, 'void CCvarTextEntry::ApplyChanges(')
    code += function(keyboard, 'void COptionsSubKeyboard::BindKey(') + function(keyboard, 'void COptionsSubKeyboard::UnbindKey(')
    code += TESTS
    code += ('\nint main() { ModelDirectoriesThatCouldAddCommandsAreNotListed(); LogoNamesThatCouldAddCommandsOrBeCutAreNotListed(); '
             'NameTextCannotCloseItsQuotes(); ScriptCvarCommandsCannotBeSplit(); LongScriptCommandsAreNotCut(); '
             'BindingsCannotCloseTheirQuotes(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-options-command-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', ROOT / 'common/StringUtils.cpp'], path / 'test',
                                     includes=[ROOT / 'common'], sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
