"""Exercise client controller hot plug: an unplugged controller releases its held keys and yields to another one."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'
CLIENTS = ['valve', 'dod', 'tfc']


def extract(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "keydefs.h"

typedef unsigned long DWORD;

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;
enum { SDL_CONTROLLER_BUTTON_MAX = 21 };

struct SDL_GameController
{
    bool attached;
    int openCount;
};

static std::vector<SDL_GameController *> g_connected;

int SDL_NumJoysticks() { return (int)g_connected.size(); }
SDL_bool SDL_IsGameController(int) { return SDL_TRUE; }
SDL_GameController *SDL_GameControllerOpen(int index) { g_connected[index]->openCount++; return g_connected[index]; }
void SDL_GameControllerClose(SDL_GameController *controller) { controller->openCount--; }
SDL_bool SDL_GameControllerGetAttached(SDL_GameController *controller) { return controller->attached ? SDL_TRUE : SDL_FALSE; }
const char *SDL_GameControllerName(SDL_GameController *) { return "stub"; }

static float g_now = 0.0f;
static std::vector<std::pair<int, int>> g_keyEvents;

struct
{
    int (*CheckParm)(const char *, char **);
    float (*GetAbsoluteTime)();
    void (*Key_Event)(int, int);
    void (*Con_Printf)(const char *, ...);
    void (*Con_DPrintf)(const char *, ...);
} gEngfuncs = {
    [](const char *, char **) { return 0; },
    []() { return g_now; },
    [](int key, int down) { g_keyEvents.emplace_back(key, down); },
    [](const char *, ...) {},
    [](const char *, ...) {},
};

DWORD joy_numbuttons;
DWORD joy_oldbuttonstate, joy_oldpovstate;
int joy_avail = 0, joy_advancedinit, joy_haspov;
SDL_GameController *s_pJoystick = NULL;
'''

TESTS = r'''
static void Rescan()
{
    // step past the one second rescan throttle
    g_now += 2.0f;
    IN_StartupJoystick();
}

static void Unplug(SDL_GameController *controller)
{
    controller->attached = false;
    for (size_t i = 0; i < g_connected.size(); ++i)
    {
        if (g_connected[i] == controller)
        {
            g_connected.erase(g_connected.begin() + i);
            break;
        }
    }
}

static void HoldKeys()
{
    joy_oldbuttonstate = (1 << 0) | (1 << 5);
    joy_oldpovstate = 1 << 2;
}

static void AssertHeldKeysReleased()
{
    const std::vector<std::pair<int, int>> expected = { { K_JOY1 + 0, 0 }, { K_AUX1 + 5, 0 }, { K_AUX29 + 2, 0 } };
    assert(expected == g_keyEvents);
    assert(0u == joy_oldbuttonstate);
    assert(0u == joy_oldpovstate);
    g_keyEvents.clear();
}

int main()
{
    SDL_GameController first = { true, 0 }, second = { true, 0 }, third = { true, 0 };
    g_connected = { &first, &second };

    Rescan();
    assert(&first == s_pJoystick);
    assert(1 == joy_avail);
    assert(1 == first.openCount);

    // an attached controller stays open across rescans
    Rescan();
    assert(&first == s_pJoystick);
    assert(1 == first.openCount);
    assert(g_keyEvents.empty());

    // unplugging the controller in use releases its keys and opens the remaining one in the same scan
    HoldKeys();
    Unplug(&first);
    Rescan();
    AssertHeldKeysReleased();
    assert(0 == first.openCount);
    assert(&second == s_pJoystick);
    assert(1 == second.openCount);
    assert(1 == joy_avail);

    // unplugging the last controller releases its keys and leaves no controller open
    HoldKeys();
    Unplug(&second);
    Rescan();
    AssertHeldKeysReleased();
    assert(0 == second.openCount);
    assert(NULL == s_pJoystick);
    assert(0 == joy_avail);

    // plugging a controller back in opens it
    g_connected = { &third };
    Rescan();
    assert(&third == s_pJoystick);
    assert(1 == joy_avail);

    // the controller is still reported attached while the device list is already empty
    HoldKeys();
    g_connected.clear();
    Rescan();
    AssertHeldKeysReleased();
    assert(0 == third.openCount);
    assert(NULL == s_pJoystick);
    assert(0 == joy_avail);
}
'''


def main():
    for client in CLIENTS:
        source = (ROOT / client / 'cl_dll/inputw32.cpp').read_text(encoding='utf-8')
        production = '\n'.join([
            extract(source, 'static void IN_ShutdownJoystick (void)'),
            extract(source, 'void IN_StartupJoystick (void)'),
        ])
        with tempfile.TemporaryDirectory(prefix='goldsrc-client-joystick-hotplug-') as folder:
            work = Path(folder)
            test_source = work / 'test.cpp'
            test_source.write_text('\n'.join([STUBS, production, TESTS]), encoding='utf-8')
            executable = compile_program([test_source], work / 'test', includes=[ROOT / 'public'],
                                         sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], cwd=work, check=True)
        print(f'PASS: {client} controller hot plug releases held keys and switches to another connected controller')


if __name__ == '__main__':
    main()
