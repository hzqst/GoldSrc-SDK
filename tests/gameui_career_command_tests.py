"""Exercise the production career bot, map and task commands with stubs.

Run: python tests/gameui_career_command_tests.py
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


STUBS = r'''
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "StringUtils.h"

struct BotProfile {
 const char* GetName() const { return name.c_str(); }
 std::string name;
};
enum BotProfileTeamType { BOT_TEAM_T, BOT_TEAM_CT, BOT_TEAM_ANY };
struct BotProfileManager {
 const BotProfile* GetProfile(const char* name, BotProfileTeamType) {
  auto it = profiles.find(name);
  return it == profiles.end() ? nullptr : &it->second;
 }
 std::map<std::string, BotProfile> profiles;
};

class CCareerCharacter {
public:
 explicit CCareerCharacter(const BotProfile* profile) : m_pProfile(profile) {}
 const char* GetName() const { return m_pProfile->GetName(); }
 const BotProfile* m_pProfile;
};
typedef std::vector<CCareerCharacter*> CharacterVec;

struct ICareerTask {
 virtual ~ICareerTask() {}
 virtual const char* GetTaskName() = 0;
 virtual const char* GetWeaponName() const = 0;
};
typedef std::vector<ICareerTask*> ITaskVec;
struct Task : ICareerTask {
 Task(const char* name, const char* weapon) : name(name), weapon(weapon) {}
 const char* GetTaskName() override { return name.c_str(); }
 const char* GetWeaponName() const override { return weapon.c_str(); }
 std::string name, weapon;
};
'''

TESTS = r'''
static void BotNamesThatCouldCloseTheirQuotesAreNotSent() {
 char buffer[1024];
 assert(FormatAddBotCommand(buffer, sizeof(buffer), true, "Frank"));
 assert(std::string("wait\nwait\nbot_add_ct \"Frank\"\n") == buffer);
 assert(FormatAddBotCommand(buffer, sizeof(buffer), false, "Mr; Ed"));
 assert(std::string("wait\nwait\nbot_add_t \"Mr; Ed\"\n") == buffer);
 for (const char* name : {"a\";quit;\"", "a\nquit", "a\rquit"})
  assert(!FormatAddBotCommand(buffer, sizeof(buffer), true, name));
 const std::string longName(1100, 'n');
 assert(!FormatAddBotCommand(buffer, sizeof(buffer), true, longName.c_str()));
 puts("PASS: bot names that could close their quotes or do not fit are not sent");
}

static void CareerBotsThatCouldAddCommandsAreLeftOut() {
 BotProfileManager manager;
 for (const char* name : {"Frank", "Bad\"Bot", "Line\nBot", "Semi;Colon"})
  manager.profiles[name].name = name;
 // the career files quote bot names with ', so a name can hold a " or a line break
 CharacterVec bots = ParseProfilesFromString(&manager, "Frank 'Bad\"Bot' 'Line\nBot' 'Semi;Colon' Missing");
 assert(2 == bots.size());
 assert(std::string("Frank") == bots[0]->GetName());
 assert(std::string("Semi;Colon") == bots[1]->GetName());
 for (CCareerCharacter* bot : bots) delete bot;
 puts("PASS: career bots whose name could close the bot_add quotes are left out");
}

static void TaskCommandsCannotBeSplit() {
 char buffer[1024];
 assert(FormatAddTaskCommand(buffer, sizeof(buffer), "killwith", "ak47", 3, true, false, false));
 assert(std::string("wait\ncareer_add_task killwith \"ak47\" 3 1 0 0\n") == buffer);
 assert(FormatAddTaskCommand(buffer, sizeof(buffer), "winfast", "", 0, false, false, true));
 assert(std::string("wait\ncareer_add_task winfast \"\" 0 0 0 1\n") == buffer);
 assert(!FormatAddTaskCommand(buffer, sizeof(buffer), "kill;quit", "ak47", 1, false, false, false));
 assert(!FormatAddTaskCommand(buffer, sizeof(buffer), "kill\"", "ak47", 1, false, false, false));
 assert(!FormatAddTaskCommand(buffer, sizeof(buffer), "killwith", "a\";quit;\"", 1, false, false, false));
 assert(!FormatAddTaskCommand(buffer, sizeof(buffer), "killwith", "a\nquit", 1, false, false, false));
 puts("PASS: task names and weapons that could split career_add_task are not sent");
}

static void MapsThatCouldAddCommandsAreNotSendable() {
 Task good("killwith", "deagle"), badName("kill;quit", ""), badWeapon("killwith", "x\"y");
 ITaskVec tasks = {&good};
 assert(IsSendableCareerMap("de_dust2_cz", tasks));
 ITaskVec none;
 assert(IsSendableCareerMap("cs_italy_cz", none));
 assert(!IsSendableCareerMap("de_dust2_cz;quit", tasks));
 assert(!IsSendableCareerMap("de\"dust", tasks));
 ITaskVec withBadName = {&good, &badName};
 assert(!IsSendableCareerMap("de_dust2_cz", withBadName));
 ITaskVec withBadWeapon = {&badWeapon};
 assert(!IsSendableCareerMap("de_dust2_cz", withBadWeapon));
 puts("PASS: maps whose name or tasks could add commands are not sendable");
}
'''


def main():
    gameui = ROOT / 'GameUI'
    definitions = (gameui / 'CareerDefinitions.cpp').read_text(encoding='utf-8')
    game = (gameui / 'CareerGame.cpp').read_text(encoding='utf-8')
    shared = (ROOT / 'game_shared/shared_util.cpp').read_text(encoding='utf-8')
    shared_header = (ROOT / 'game_shared/shared_util.h').read_text(encoding='utf-8')
    code = STUBS
    code += ''.join(re.findall(r'^static char s_shared_\w+.*\n', shared, re.M))
    code += function(shared_header, 'inline char *CloneString(')
    code += ''.join(function(shared, signature) for signature in (
        'char *SharedGetToken(', 'void SharedSetQuoteChar(', 'const char *SharedParse(', 'bool SharedTokenWaiting('))
    code += function(definitions, 'static bool IsSendableCareerMap(') + function(definitions, 'static CharacterVec ParseProfilesFromString(')
    code += function(game, 'static bool FormatAddBotCommand(') + function(game, 'static bool FormatAddTaskCommand(')
    code += TESTS
    code += ('\nint main() { BotNamesThatCouldCloseTheirQuotesAreNotSent(); CareerBotsThatCouldAddCommandsAreLeftOut(); '
             'TaskCommandsCannotBeSplit(); MapsThatCouldAddCommandsAreNotSendable(); }\n')
    with tempfile.TemporaryDirectory(prefix='goldsrc-gameui-career-command-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([path / 'test.cpp', gameui / 'StringUtils.cpp'], path / 'test',
                                     includes=[gameui], sanitizers=['address', 'undefined'],
                                     defines=['_CRT_SECURE_NO_WARNINGS'])
        subprocess.run([str(executable)], check=True, cwd=path)


if __name__ == '__main__':
    main()
