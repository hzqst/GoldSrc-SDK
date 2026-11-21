"""Check production IP rate limiter ordering across the sign bit."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature, occurrence=0, end='\n}'):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = -1
    for _ in range(occurrence + 1):
        start = source.index(signature, start + 1)
    return source[start:source.index(end, start) + len(end)].replace('struct iprate_s', 'struct CIPRateLimit::iprate_s')


STUBS = r'''
#include <cassert>
#include <cstring>
class CIPRateLimit
{
public:
    struct iprate_s;
    static bool LessIP(const iprate_s&, const iprate_s&);
};
'''

TESTS = r'''
int main()
{
    const unsigned highBitAddress = 0x80000001u;
    CIPRateLimit::iprate_s low{}, high{};
    low.ip = 1;
    std::memcpy(&high.ip, &highBitAddress, sizeof(high.ip));
    assert(CIPRateLimit::LessIP(high, low));
    assert(!CIPRateLimit::LessIP(low, high));
    assert(!CIPRateLimit::LessIP(high, high));
}
'''


def main():
    production = [
        extract("ipratelimit.h", "\tstruct iprate_s", 0, "\n\t};"),
        extract("ipratelimit.cpp", "bool CIPRateLimit::LessIP"),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-ip-rate-order-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
