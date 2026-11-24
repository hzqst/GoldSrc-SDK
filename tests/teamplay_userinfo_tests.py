"""Exercise CHalfLifeTeamplay::ClientUserInfoChanged reading cl_autowepswitch."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/valve/dlls'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#ifdef _WIN32
#define stricmp _stricmp
#else
#include <strings.h>
#define stricmp strcasecmp
#endif

using BOOL = int;
constexpr BOOL TRUE = 1;
constexpr BOOL FALSE = 0;

static bool FStrEq(const char* a, const char* b) { return 0 == std::strcmp(a, b); }

struct edict_t {};
struct entvars_t { const char* netname = "player"; };
static const char* STRING(const char* s) { return s ? s : ""; }
static int GETPLAYERUSERID(edict_t*) { return 1; }
static const char* GETPLAYERAUTHID(edict_t*) { return "STEAM_0:0:1"; }

struct CBasePlayer {
    entvars_t ev;
    entvars_t* pev = &ev;
    char m_szTeamName[32] = "blue";
    int m_iAutoWepSwitch = 0;
    int entindex() { return 1; }
    edict_t* edict() { return nullptr; }
    void SetPrefsFromUserinfo(char* infobuffer);
};

static std::map<std::string, std::string> g_userinfo;

static struct {
    char* (*pfnInfoKeyValue)(char* infobuffer, const char* key);
    void (*pfnSetClientKeyValue)(int clientIndex, char* infobuffer, const char* key, const char* value);
    char* (*pfnGetInfoKeyBuffer)(edict_t* e);
} g_engfuncs = {
    [](char*, const char* key) -> char* {
        static std::string empty;
        auto it = g_userinfo.find(key);
        return it == g_userinfo.end() ? empty.data() : it->second.data();
    },
    [](int, char*, const char*, const char*) {},
    [](edict_t*) -> char* { return nullptr; },
};

static struct { float value = 0.0f; } defaultteam;
static void UTIL_SayText(const char*, CBasePlayer*) {}
static void UTIL_SayTextAll(const char*, CBasePlayer*) {}
static void UTIL_LogPrintf(const char*, ...) {}

struct CHalfLifeMultiplay {
    virtual ~CHalfLifeMultiplay() = default;
    virtual void ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer);
};

struct CHalfLifeTeamplay : CHalfLifeMultiplay {
    char m_szTeamList[64] = "blue;red";
    int teamChanges = 0;
    bool IsValidTeam(const char* team) { return 0 == stricmp(team, "blue") || 0 == stricmp(team, "red"); }
    void ChangePlayerTeam(CBasePlayer* pPlayer, const char* team, BOOL, BOOL)
    {
        std::strncpy(pPlayer->m_szTeamName, team, sizeof(pPlayer->m_szTeamName) - 1);
        pPlayer->m_szTeamName[sizeof(pPlayer->m_szTeamName) - 1] = '\0';
        ++teamChanges;
    }
    void RecountTeams(BOOL) {}
    void ClientUserInfoChanged(CBasePlayer* pPlayer, char* infobuffer) override;
};
'''

TESTS = r'''
int main()
{
    CHalfLifeTeamplay rules;
    CBasePlayer player;
    char dummy[4] = {};

    // Same team model (early return path): cl_autowepswitch must still be applied.
    g_userinfo = {{"model", "blue"}, {"cl_autowepswitch", "2"}};
    rules.ClientUserInfoChanged(&player, dummy);
    assert(2 == player.m_iAutoWepSwitch);
    assert(0 == rules.teamChanges);

    // Missing cl_autowepswitch defaults to 1.
    g_userinfo = {{"model", "blue"}};
    rules.ClientUserInfoChanged(&player, dummy);
    assert(1 == player.m_iAutoWepSwitch);

    // Explicit 0 disables autoswitch even during a team change.
    g_userinfo = {{"model", "red"}, {"cl_autowepswitch", "0"}};
    rules.ClientUserInfoChanged(&player, dummy);
    assert(0 == player.m_iAutoWepSwitch);
    assert(1 == rules.teamChanges);
    assert(0 == std::strcmp("red", player.m_szTeamName));
}
'''


def main():
    production = [
        extract('player.cpp', 'void CBasePlayer :: SetPrefsFromUserinfo( char * infobuffer )'),
        extract('multiplay_gamerules.cpp', 'void CHalfLifeMultiplay :: ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )'),
        extract('teamplay_gamerules.cpp', 'void CHalfLifeTeamplay::ClientUserInfoChanged( CBasePlayer *pPlayer, char *infobuffer )'),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-teamplay-userinfo-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: CHalfLifeTeamplay::ClientUserInfoChanged applies cl_autowepswitch')


if __name__ == '__main__':
    main()
