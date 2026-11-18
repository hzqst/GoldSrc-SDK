"""Compile the production WAV resampler and exercise guarded cache buffers.

Run: python tests/sound_resample_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.

Each case sizes the cache with S_GetResampledBytes, exactly as S_LoadSound does,
and places both the input and the cache immediately before a no-access page, so
a read past the samples or a write past the cache faults (Windows only; other
hosts fall back to unguarded buffers).
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]

HELPERS = (
    'static bool S_KeepHighRateSounds()',
    'static float S_GetResampleStep( int inrate )',
    'static int S_GetResampledLength( int samples, float stepscale )',
    'static int S_GetResampledBytes( int samples, int width, float stepscale )',
)
RESAMPLE = 'void ResampleSfx( sfx_t* sfx, int inrate, int inwidth, byte* data, int datasize )'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif
using byte = unsigned char;
struct cache_user_t { void* data; };
struct sfx_t { char name[64]; cache_user_t cache; };
struct sfxcache_t { int length, loopstart, speed, width, stereo; byte data[1]; };
struct dma_t { int speed; };
struct cvar_t { float value; };
static dma_t s_Device{};
static volatile dma_t* shm = &s_Device;
static cvar_t hisound{};
static cvar_t loadas8bit{};
static void* Cache_Check(cache_user_t* user) { return user->data; }
static short LittleShort(short value) { return value; }
static void Con_DPrintf(const char*, ...) {}
'''

TESTS = r'''
static constexpr int OutputRate = 22050;
static constexpr int HighRate = OutputRate * 2;
static constexpr int LowRate = OutputRate / 2;
static constexpr int BadRate = 1;
static constexpr int ShortCount = 1000;
// Upsampling this many samples passes INT_MAX in a 32-bit samplefrac.
static constexpr int LongCount = 9000000;
// Float cannot represent this count, it rounds up to 2^24 + 4.
static constexpr int FloatInexactCount = (1 << 24) + 3;
static constexpr int SampleBias = 128;
static constexpr int ByteMask = 0xff;

static void Check(bool condition)
{
 if (!condition) throw std::runtime_error("resample assertion failed");
}

// Places bytes immediately before a no-access page so any overrun faults.
class CGuardedBuffer
{
public:
 explicit CGuardedBuffer(std::size_t bytes)
 {
  const std::size_t size = bytes + 1;
#if defined(_WIN32)
  SYSTEM_INFO system{};
  GetSystemInfo(&system);
  const std::size_t page = system.dwPageSize;
  const std::size_t pages = (size + page - 1) / page + 1;
  m_pMemory = static_cast<byte*>(VirtualAlloc(nullptr, pages * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  Check(m_pMemory != nullptr);
  DWORD oldProtection;
  Check(VirtualProtect(m_pMemory + (pages - 1) * page, page, PAGE_NOACCESS, &oldProtection) != 0);
  m_pData = m_pMemory + (pages - 1) * page - size;
#else
  m_pMemory = static_cast<byte*>(std::malloc(size));
  Check(m_pMemory != nullptr);
  m_pData = m_pMemory;
#endif
 }
 ~CGuardedBuffer()
 {
#if defined(_WIN32)
  VirtualFree(m_pMemory, 0, MEM_RELEASE);
#else
  std::free(m_pMemory);
#endif
 }
 CGuardedBuffer(const CGuardedBuffer&) = delete;
 CGuardedBuffer& operator=(const CGuardedBuffer&) = delete;
 byte* Data() const { return m_pData; }
private:
 byte* m_pMemory{};
 byte* m_pData{};
};

struct ResampleResult
{
 int length;
 int speed;
 std::vector<int> samples;
};

// Allocates the cache the way S_LoadSound does, then lets ResampleSfx fill it.
static ResampleResult Resample(int rate, int width, const std::vector<byte>& input)
{
 const int insamples = static_cast<int>(input.size()) / width;
 const int bytes = S_GetResampledBytes(insamples, width, S_GetResampleStep(rate));
 Check(bytes >= 0);
 CGuardedBuffer source(input.size());
 std::memcpy(source.Data(), input.data(), input.size());
 CGuardedBuffer cache(offsetof(sfxcache_t, data) + bytes);
 auto* pCache = reinterpret_cast<sfxcache_t*>(cache.Data());
 pCache->length = insamples;
 pCache->loopstart = -1;
 pCache->speed = rate;
 pCache->width = width;
 sfx_t sound{"test.wav", {pCache}};
 ResampleSfx(&sound, rate, width, source.Data(), static_cast<int>(input.size()));
 ResampleResult result{pCache->length, pCache->speed, {}};
 for (int i = 0; i < pCache->length; ++i)
 {
  if (pCache->width == 2)
   result.samples.push_back(reinterpret_cast<short*>(pCache->data)[i]);
  else
   result.samples.push_back(reinterpret_cast<signed char*>(pCache->data)[i]);
 }
 return result;
}

static std::vector<byte> Ramp(int count, int width)
{
 std::vector<byte> bytes(static_cast<std::size_t>(count) * width);
 for (int i = 0; i < count; ++i)
 {
  if (width == 2)
   reinterpret_cast<short*>(bytes.data())[i] = static_cast<short>(i);
  else
   bytes[i] = static_cast<byte>(i);
 }
 return bytes;
}

int main(int argc, char** argv)
{
 try
 {
  Check(argc == 2);
  const std::string test = argv[1];
  s_Device.speed = OutputRate;
  hisound.value = 1;
  if (test == "hisound")
  {
   // The cache must be sized and filled with the same step for every hisound value.
   for (float value : {1.0f, 0.0f, -1.0f})
   {
    hisound.value = value;
    const bool highRate = value > 0;
    const auto result = Resample(HighRate, 2, Ramp(ShortCount, 2));
    const int outcount = highRate ? ShortCount : ShortCount / 2;
    const int outputRate = highRate ? HighRate : OutputRate;
    const int lastSample = highRate ? ShortCount - 1 : ShortCount - 2;
    Check(outcount == result.length && outputRate == result.speed);
    Check(lastSample == result.samples.back());
   }
  }
  else if (test == "long")
  {
   const auto result = Resample(LowRate, 1, Ramp(LongCount, 1));
   Check(LongCount * 2 == result.length);
   const int lastSample = ((LongCount - 1) & ByteMask) - SampleBias;
   Check(lastSample == result.samples.back());
  }
  else if (test == "exact")
  {
   // A unit step reads 16-bit samples through the general path.
   const auto result = Resample(OutputRate, 2, Ramp(FloatInexactCount, 2));
   Check(FloatInexactCount == result.length);
   Check(static_cast<short>(FloatInexactCount - 1) == result.samples.back());
  }
  else if (test == "short")
  {
   // A chunk shorter than the declared length must not be read past its end.
   const std::vector<byte> input = Ramp(ShortCount, 2);
   const int bytes = S_GetResampledBytes(ShortCount, 2, S_GetResampleStep(HighRate));
   Check(bytes >= 0);
   CGuardedBuffer source(input.size());
   std::memcpy(source.Data(), input.data(), input.size());
   CGuardedBuffer cache(offsetof(sfxcache_t, data) + bytes);
   auto* pCache = reinterpret_cast<sfxcache_t*>(cache.Data());
   pCache->length = ShortCount;
   pCache->loopstart = -1;
   pCache->speed = HighRate;
   pCache->width = 2;
   sfx_t sound{"short.wav", {pCache}};
   ResampleSfx(&sound, HighRate, 2, source.Data(), ShortCount * 2 - 6);
   Check(ShortCount - 4 == reinterpret_cast<short*>(pCache->data)[ShortCount - 1]);
  }
  else if (test == "limits")
  {
   Check(ShortCount * 2 == S_GetResampledBytes(ShortCount, 2, 1));
   Check(ShortCount * 2 * 2 == S_GetResampledBytes(ShortCount, 2, 0.5f));
   Check(-1 == S_GetResampledBytes(ShortCount * 100, 2, S_GetResampleStep(BadRate)));
   Check(-1 == S_GetResampledBytes(INT_MAX, 2, 1));
   Check(-1 == S_GetResampledBytes(INT_MAX, 1, 1));
   Check(-1 == S_GetResampledLength(INT_MAX, 0.5f));
   Check(INT_MAX == S_GetResampledLength(INT_MAX, 1));
  }
  else
  {
   Check(false);
  }
 }
 catch (const std::exception& error)
 {
  std::fprintf(stderr, "%s\n", error.what());
  return EXIT_FAILURE;
 }
 std::printf("PASS: %s\n", argv[1]);
 return EXIT_SUCCESS;
}
'''


def main():
    source = (ROOT / 'GoldSrc-SDK/engine/snd_mem.cpp').read_text()
    functions = [function(source, signature) for signature in HELPERS]
    functions.append(function(source, RESAMPLE))
    scenarios = ['hisound', 'long', 'exact', 'short', 'limits']
    with tempfile.TemporaryDirectory(prefix='goldsrc-resample-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(STUBS + '\n'.join(functions) + TESTS)
        executable = compile_program([source_file], executable)
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
