"""Exercise the production POSIX VGUI2 CSystem::RunFrame and GetTimeSinceLastUse on a stub clock and cursor, so the idle time is reported in seconds.

Run: python tests/vgui2_time_since_last_use_tests.py
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
TIMEOUT_SECONDS = 60


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// the time GetCurrentTime returns, in seconds
static double now = 0.0;

struct CInput {
 void GetCursorPos(int& x, int& y) { x = cursorX; y = cursorY; }
 int cursorX = 0;
 int cursorY = 0;
};
static CInput input;
static CInput* g_pInput = &input;

class CSystem {
public:
 void RunFrame();
 double GetCurrentTime() { return now; }
 long GetTimeMillis();
 bool SetWatchForComputerUse(bool state);
 double GetTimeSinceLastUse();
 double m_flFrameTime = 0.0;
 bool m_bStaticWatchForComputerUse = false;
 double m_StaticLastComputerUseTime = 0.0;
 int m_iStaticMouseOldX = 0;
 int m_iStaticMouseOldY = 0;
};
'''

TESTS = r'''

// the cursor moves further than the fifty pixel slack, so RunFrame records a use
static const int FAR_CURSOR_MOVE = 100;
static const double TOLERANCE = 1e-6;

static void UseAt(CSystem& system, double seconds) {
 now = seconds;
 input.cursorX += FAR_CURSOR_MOVE;
 system.RunFrame();
}

static void AssertNear(double expected, double actual) {
 if (fabs(expected - actual) > TOLERANCE) {
  printf("FAIL: expected %f, got %f\n", expected, actual);
  exit(1);
 }
}

static void ReportsZeroWithoutWatch() {
 CSystem system;
 UseAt(system, 10.0);
 now = 15.0;
 AssertNear(0.0, system.GetTimeSinceLastUse());
 puts("PASS: GetTimeSinceLastUse reports zero while not watching");
}

static void ReportsZeroRightAfterUse() {
 CSystem system;
 bool supported = system.SetWatchForComputerUse(true);
 assert(supported);
 UseAt(system, 10.0);
 AssertNear(0.0, system.GetTimeSinceLastUse());
 puts("PASS: GetTimeSinceLastUse reports zero right after a use");
}

static void ReportsSecondsSinceUse() {
 CSystem system;
 bool supported = system.SetWatchForComputerUse(true);
 assert(supported);
 UseAt(system, 10.0);
 now = 15.0;
 AssertNear(5.0, system.GetTimeSinceLastUse());
 puts("PASS: GetTimeSinceLastUse reports the seconds since the last use");
}

static void KeepsFractionalUseTime() {
 CSystem system;
 bool supported = system.SetWatchForComputerUse(true);
 assert(supported);
 UseAt(system, 10.75);
 now = 15.0;
 AssertNear(4.25, system.GetTimeSinceLastUse());
 puts("PASS: GetTimeSinceLastUse keeps the fraction of the use time");
}

static void IgnoresSmallCursorMoves() {
 CSystem system;
 bool supported = system.SetWatchForComputerUse(true);
 assert(supported);
 UseAt(system, 10.0);
 now = 12.0;
 input.cursorX += 1;
 system.RunFrame();
 now = 15.0;
 AssertNear(5.0, system.GetTimeSinceLastUse());
 puts("PASS: RunFrame does not record cursor jitter as a use");
}

int main() {
 ReportsZeroWithoutWatch();
 ReportsZeroRightAfterUse();
 ReportsSecondsSinceUse();
 KeepsFractionalUseTime();
 IgnoresSmallCursorMoves();
 return 0;
}
'''


def main():
    posix = (ROOT / 'vgui2/src/system_posix.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += ''.join(function(posix, signature) + '\n' for signature in (
        'void CSystem::RunFrame()', 'long CSystem::GetTimeMillis()',
        'bool CSystem::SetWatchForComputerUse( bool state )', 'double CSystem::GetTimeSinceLastUse()'))
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-time-since-last-use-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
