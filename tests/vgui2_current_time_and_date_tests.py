"""Exercise the production POSIX VGUI2 CSystem::GetCurrentTimeAndDate on a stub clock, so it fills the requested fields like the Windows build.

Run: python tests/vgui2_current_time_and_date_tests.py
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
TIMEOUT_SECONDS = 60


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


def constant(source, name):
    return re.search(r'^static const int ' + name + r' = .*;$', source, re.MULTILINE).group(0)


STUBS = r'''
#include <cstdio>
#include <cstdlib>
#include <ctime>

// the clock and calendar conversion GetCurrentTimeAndDate reads
static bool clockFails = false;
static bool conversionFails = false;
static tm stubLocal = {};

static time_t StubTime(time_t* out) {
 time_t now = clockFails ? (time_t)-1 : (time_t)1;
 if (out) *out = now;
 return now;
}

static tm* StubLocaltime(const time_t*, tm* out) {
 if (conversionFails) return NULL;
 *out = stubLocal;
 return out;
}

#define time StubTime
#define localtime_r StubLocaltime

class CSystem {
public:
 bool GetCurrentTimeAndDate(int* year, int* month, int* dayOfWeek, int* day, int* hour, int* minute, int* second);
};
'''

TESTS = r'''
#undef time
#undef localtime_r

// 2026-11-20 21:44:51, a Friday
static const int TM_YEAR = 126;
static const int TM_MONTH = 10;
static const int WEEKDAY = 5;
static const int DAY = 20;
static const int HOUR = 21;
static const int MINUTE = 44;
static const int SECOND = 51;
static const int YEAR = 2026;
static const int MONTH = 11;
static const int UNSET = -1;

static void Reset() {
 clockFails = false;
 conversionFails = false;
 stubLocal = tm();
 stubLocal.tm_year = TM_YEAR;
 stubLocal.tm_mon = TM_MONTH;
 stubLocal.tm_wday = WEEKDAY;
 stubLocal.tm_mday = DAY;
 stubLocal.tm_hour = HOUR;
 stubLocal.tm_min = MINUTE;
 stubLocal.tm_sec = SECOND;
}

static void Expect(bool condition, const char* message) {
 if (!condition) {
  printf("FAIL: %s\n", message);
  exit(1);
 }
}

static void FillsOnlyYear() {
 Reset();
 CSystem system;
 int year = UNSET;
 Expect(system.GetCurrentTimeAndDate(&year, NULL, NULL, NULL, NULL, NULL, NULL), "a year only query succeeds");
 Expect(YEAR == year, "the year is the calendar year");
 puts("PASS: GetCurrentTimeAndDate fills a year only query");
}

static void FillsEveryField() {
 Reset();
 CSystem system;
 int year = UNSET, month = UNSET, dayOfWeek = UNSET, day = UNSET, hour = UNSET, minute = UNSET, second = UNSET;
 Expect(system.GetCurrentTimeAndDate(&year, &month, &dayOfWeek, &day, &hour, &minute, &second), "a full query succeeds");
 Expect(YEAR == year, "the year is the calendar year");
 Expect(MONTH == month, "the month counts from one");
 Expect(WEEKDAY == dayOfWeek, "the day of week counts from Sunday as zero");
 Expect(DAY == day, "the day of month is kept");
 Expect(HOUR == hour, "the hour is kept");
 Expect(MINUTE == minute, "the minute is kept");
 Expect(SECOND == second, "the second is kept");
 puts("PASS: GetCurrentTimeAndDate fills every requested field");
}

static void FailsWithoutClock() {
 Reset();
 clockFails = true;
 CSystem system;
 int year = UNSET;
 Expect(!system.GetCurrentTimeAndDate(&year, NULL, NULL, NULL, NULL, NULL, NULL), "a failed clock read reports failure");
 Expect(UNSET == year, "a failed clock read leaves the year untouched");
 puts("PASS: GetCurrentTimeAndDate fails when the clock cannot be read");
}

static void FailsWithoutConversion() {
 Reset();
 conversionFails = true;
 CSystem system;
 int year = UNSET;
 Expect(!system.GetCurrentTimeAndDate(&year, NULL, NULL, NULL, NULL, NULL, NULL), "a failed conversion reports failure");
 Expect(UNSET == year, "a failed conversion leaves the year untouched");
 puts("PASS: GetCurrentTimeAndDate fails when the local time cannot be converted");
}

int main() {
 FillsOnlyYear();
 FillsEveryField();
 FailsWithoutClock();
 FailsWithoutConversion();
 return 0;
}
'''


def main():
    posix = (ROOT / 'vgui2/src/system_posix.cpp').read_text(encoding='utf-8')
    code = STUBS
    code += ''.join(constant(posix, name) + '\n' for name in ('TM_YEAR_BASE', 'TM_MONTH_BASE'))
    code += function(posix, 'bool CSystem::GetCurrentTimeAndDate(') + '\n'
    code += TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-current-time-and-date-') as folder:
        folder = Path(folder)
        (folder / 'test.cpp').write_text(code, encoding='utf-8')
        executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True, cwd=folder, timeout=TIMEOUT_SECONDS)


if __name__ == '__main__':
    main()
