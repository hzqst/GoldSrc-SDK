"""Exercise VGUI1 and VGUI2 CheckModState mapping KMOD_RSHIFT to KEY_RSHIFT."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(relpath, signature):
    source = (ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PREAMBLE = r'''
#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

using Uint32 = std::uint32_t;
enum SDL_Keymod : Uint32 {
    KMOD_NONE   = 0x0000,
    KMOD_LSHIFT = 0x0001,
    KMOD_RSHIFT = 0x0002,
    KMOD_LCTRL  = 0x0040,
    KMOD_RCTRL  = 0x0080,
    KMOD_LALT   = 0x0100,
    KMOD_RALT   = 0x0200,
    KMOD_LGUI   = 0x0400,
    KMOD_RGUI   = 0x0800,
    KMOD_CAPS   = 0x2000,
};

static Uint32 g_modState = KMOD_NONE;
static Uint32 SDL_GetModState() { return g_modState; }

namespace vgui2 {
enum KeyCode {
    KEY_LSHIFT = 1, KEY_RSHIFT, KEY_LALT, KEY_RALT,
    KEY_LCONTROL, KEY_RCONTROL, KEY_CAPSLOCK, KEY_LWIN, KEY_RWIN,
};
}

namespace vgui {
enum KeyCode {
    KEY_LSHIFT = 11, KEY_RSHIFT, KEY_LALT, KEY_RALT,
    KEY_LCONTROL, KEY_RCONTROL, KEY_CAPSLOCK, KEY_LWIN, KEY_RWIN,
};
struct SurfaceBase {};
struct App {
    std::vector<std::pair<char, KeyCode>> events;
    void internalKeyPressed(KeyCode code, SurfaceBase*) { events.emplace_back('P', code); }
    void internalKeyReleased(KeyCode code, SurfaceBase*) { events.emplace_back('R', code); }
};
}

struct FakeInputInternal {
    std::vector<std::pair<char, vgui2::KeyCode>> events;
    void InternalKeyCodePressed(vgui2::KeyCode code) { events.emplace_back('P', code); }
    void InternalKeyCodeTyped(vgui2::KeyCode code) { events.emplace_back('T', code); }
    void InternalKeyCodeReleased(vgui2::KeyCode code) { events.emplace_back('R', code); }
};
static FakeInputInternal g_input;
static FakeInputInternal* g_InputInternal = &g_input;

namespace vgui2_mod {
'''

MID = r'''
} // namespace vgui2_mod

namespace vgui1_mod {
'''

TESTS = r'''
} // namespace vgui1_mod

int main()
{
    vgui::App app;
    vgui::SurfaceBase surface;

    // Press RSHIFT alone -> must report KEY_RSHIFT (not KEY_LSHIFT)
    g_modState = KMOD_RSHIFT;
    vgui2_mod::CheckModState();
    vgui1_mod::CheckModState(&app, &surface);
    assert(2 == g_input.events.size());
    assert('P' == g_input.events[0].first && vgui2::KEY_RSHIFT == g_input.events[0].second);
    assert('T' == g_input.events[1].first && vgui2::KEY_RSHIFT == g_input.events[1].second);
    assert(1 == app.events.size());
    assert('P' == app.events[0].first && vgui::KEY_RSHIFT == app.events[0].second);

    // Hold LSHIFT while releasing RSHIFT -> must press LSHIFT and release RSHIFT independently
    g_input.events.clear();
    app.events.clear();
    g_modState = KMOD_LSHIFT;
    vgui2_mod::CheckModState();
    vgui1_mod::CheckModState(&app, &surface);
    assert(3 == g_input.events.size());
    assert('P' == g_input.events[0].first && vgui2::KEY_LSHIFT == g_input.events[0].second);
    assert('T' == g_input.events[1].first && vgui2::KEY_LSHIFT == g_input.events[1].second);
    assert('R' == g_input.events[2].first && vgui2::KEY_RSHIFT == g_input.events[2].second);
    assert(2 == app.events.size());
    assert('P' == app.events[0].first && vgui::KEY_LSHIFT == app.events[0].second);
    assert('R' == app.events[1].first && vgui::KEY_RSHIFT == app.events[1].second);

    // Release LSHIFT
    g_input.events.clear();
    app.events.clear();
    g_modState = KMOD_NONE;
    vgui2_mod::CheckModState();
    vgui1_mod::CheckModState(&app, &surface);
    assert(1 == g_input.events.size() && 'R' == g_input.events[0].first && vgui2::KEY_LSHIFT == g_input.events[0].second);
    assert(1 == app.events.size() && 'R' == app.events[0].first && vgui::KEY_LSHIFT == app.events[0].second);
}
'''


def main():
    v2 = extract('vgui2/BaseUISurface.cpp', 'static void CheckModState()')
    v1 = extract('VGUI_EngineSurfaceWrap.cpp', 'static void CheckModState( vgui::App* app, vgui::SurfaceBase* surface )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui-modifier-sync-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([PREAMBLE, v2, MID, v1, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: VGUI1 and VGUI2 CheckModState map KMOD_RSHIFT to KEY_RSHIFT')


if __name__ == '__main__':
    main()
