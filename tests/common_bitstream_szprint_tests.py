"""Exercise MSG_ReadBitString bounds, SZ_Print on empty buffer, and MSG_WriteBitCoord fractions."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(filename, signature):
    source = (ROOT / filename).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using byte = unsigned char;
using qboolean = int;
constexpr std::uint16_t FSB_ALLOWOVERFLOW = 1;
constexpr std::uint16_t FSB_OVERFLOWED = 2;

struct sizebuf_t {
    const char* buffername;
    std::uint16_t flags;
    byte* data;
    int maxsize;
    int cursize;
};

static int Q_strlen(const char* s) { return static_cast<int>(std::strlen(s)); }
static void Q_memcpy(void* d, const void* s, int n) { std::memcpy(d, s, n); }
static void Con_Printf(const char*, ...) {}
static void Con_DPrintf(const char*, ...) {}
static void Sys_Error(const char*, ...) { assert(false && "Sys_Error called"); }

static void* SZ_GetSpace(sizebuf_t* buffer, int length)
{
    int prev = buffer->cursize;
    if (prev + length > buffer->maxsize)
    {
        buffer->flags |= FSB_OVERFLOWED;
        prev = 0;
    }
    buffer->cursize = prev + length;
    return buffer->data + prev;
}

static qboolean msg_badread = 0;
static std::vector<byte> g_readBytes;
static size_t g_readPos = 0;
static std::uint32_t MSG_ReadBits(int numBits)
{
    assert(8 == numBits);
    if (g_readPos >= g_readBytes.size())
    {
        msg_badread = 1;
        return 1;
    }
    return g_readBytes[g_readPos++];
}

struct WrittenBits {
    std::vector<int> bits;
    void WriteBit(int v) { bits.push_back(v ? 1 : 0); }
    void WriteBits(std::uint32_t v, int n)
    {
        for (int i = 0; i < n; ++i)
            bits.push_back((v >> i) & 1);
    }
    size_t readPos = 0;
    int ReadBit() { assert(readPos < bits.size()); return bits[readPos++]; }
    std::uint32_t ReadBits(int n)
    {
        std::uint32_t v = 0;
        for (int i = 0; i < n; ++i)
            v |= (static_cast<std::uint32_t>(ReadBit()) << i);
        return v;
    }
    float ReadBitCoord()
    {
        int intval = ReadBit();
        int fractval = ReadBit();
        float value = 0.0f;
        if (intval || fractval)
        {
            int sign = ReadBit();
            if (intval)
                intval = static_cast<int>(ReadBits(12));
            if (fractval)
                fractval = static_cast<int>(ReadBits(3));
            value = static_cast<float>(intval) + static_cast<float>(fractval) * 0.125f;
            if (sign)
                value = -value;
        }
        return value;
    }
} g_bitStream;

static void MSG_WriteOneBit(int v) { g_bitStream.WriteBit(v); }
static void MSG_WriteBits(std::uint32_t v, int n) { g_bitStream.WriteBits(v, n); }
'''

TESTS = r'''
int main()
{
    // 1. SZ_Print on an empty heap buffer: ASan catches any data[-1] read.
    {
        byte* storage = static_cast<byte*>(std::malloc(32));
        sizebuf_t sb{"test", FSB_ALLOWOVERFLOW, storage, 32, 0};
        SZ_Print(&sb, "hello");
        assert(6 == sb.cursize);
        assert(0 == std::strcmp("hello", reinterpret_cast<char*>(storage)));
        SZ_Print(&sb, " world");
        assert(12 == sb.cursize);
        assert(0 == std::strcmp("hello world", reinterpret_cast<char*>(storage)));
        std::free(storage);
    }

    // 2. MSG_ReadBitString over 8192 bytes: truncates at 8191, consumes up to NUL, leaves stream aligned.
    {
        g_readBytes.assign(9000, 'A');
        g_readBytes.push_back(0);
        g_readBytes.push_back(0x5A); // sentinel byte after the string
        g_readPos = 0;
        msg_badread = 0;
        char* result = MSG_ReadBitString();
        assert(0 == msg_badread);
        assert(8191 == std::strlen(result));
        assert(9001 == g_readPos);
        assert(0x5A == MSG_ReadBits(8));
    }

    // 3. MSG_ReadBitString on truncated stream (no NUL): stops at badread without storing garbage 0x01.
    {
        g_readBytes = {'a', 'b', 'c'};
        g_readPos = 0;
        msg_badread = 0;
        char* result = MSG_ReadBitString();
        assert(1 == msg_badread);
        assert(0 == std::strcmp("abc", result));
    }

    // 4. MSG_WriteBitCoord preserves 1/8 fractional increments for positive, negative, and sub-unit values.
    for (float sample : { 0.0f, 0.125f, -0.375f, 10.625f, -42.875f, 100.0f })
    {
        g_bitStream = {};
        MSG_WriteBitCoord(sample);
        float decoded = g_bitStream.ReadBitCoord();
        assert(std::fabs(decoded - sample) < 1e-5f);
    }
}
'''


def main():
    production = [
        extract('common.cpp', 'void SZ_Print( sizebuf_t* buffer, const char* text )'),
        extract('common.cpp', 'char* MSG_ReadBitString()'),
        extract('common.cpp', 'static std::uint32_t MSG_CoordinateMagnitude( int value )'),
        extract('common.cpp', 'void MSG_WriteBitCoord( float coordinate )'),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-common-bitstream-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: SZ_Print, MSG_ReadBitString, and MSG_WriteBitCoord behave correctly')


if __name__ == '__main__':
    main()
