"""Check that the game_shared CPerformanceCounter starts its accumulated time at zero."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

HEADER = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/game_shared/perf_counter.h'

TESTS = r'''
#include <cassert>
#include <cmath>
#include <cstring>
#include <new>
#include "perf_counter.h"
int main() {
 // Build the counter over garbage, as an uninitialized stack instance would see it
 alignas(CPerformanceCounter) unsigned char storage[sizeof(CPerformanceCounter)];
 std::memset(storage,0xCD,sizeof(storage));
 CPerformanceCounter *counter=new(storage) CPerformanceCounter;
 assert(0.0==counter->m_flCurrentTime);
 assert(0.0==counter->m_flLastCurrentTime);

 const double start=counter->GetCurTime();
 const double end=counter->GetCurTime();
 assert(std::isfinite(start)&&std::isfinite(end));
 assert(start>=0.0&&end>=start);
 counter->~CPerformanceCounter();
}
'''


def main():
    # Expose the private members so the test can read them directly
    header = HEADER.read_text(encoding='latin-1').replace('private:', 'public:')
    with tempfile.TemporaryDirectory(prefix='goldsrc-perf-counter-') as folder:
        path = Path(folder)
        (path / 'perf_counter.h').write_text(header, encoding='latin-1')
        (path / 'test.cpp').write_text(TESTS)
        executable = compile_program([path / 'test.cpp'], path / 'test', includes=[path], debug=True,
                                     sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=path)
    print('PASS: CPerformanceCounter starts with zero accumulated time')


if __name__ == '__main__':
    main()
