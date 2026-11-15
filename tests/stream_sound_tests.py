"""Compile production stream functions with deterministic cache/file test doubles.

Run: python tests/stream_sound_tests.py [--revision <git revision>]
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
using byte = unsigned char;
using qboolean = int;
using FileHandle_t = void*;
constexpr int CHAN_STREAM = 5, CHAN_NETWORKVOICE_BASE = 7, CHAN_NETWORKVOICE_END = 500;
constexpr int SOUND_CALLBACK_CACHE_LENGTH = 0x40000000, SOUND_NAME_BYTES = 64;
constexpr int CHUNK_BYTES = 32768, SAMPLE_RATE = 11025, DATA_OFFSET = 44;
constexpr int OLD_LENGTH = 123, MIX_TIME = 500;
struct cache_user_t { void* data; };
struct sfxcache_t { int length, loopstart, speed, width, stereo; };
struct sfx_t { char name[SOUND_NAME_BYTES]; cache_user_t cache; };
struct channel_t { sfx_t* sfx; int entchannel, isentence, iword, pos, end; };
struct wavinfo_t { int channels, width, rate, samples, dataofs, loopstart; };
struct wavstream_t { int csamplesplayed, csamplesinmem; FileHandle_t hFile;
 wavinfo_t info; int lastposloaded; };
struct voxword_t { sfx_t* sfx; bool fKeepCached; int samplefrac; };
static channel_t channels[1];
static wavstream_t wavstreams[1];
static voxword_t rgrgvoxword[1][2];
static struct { bool fPrecaching; } cl;
static struct Dma { int speed = SAMPLE_RATE; } dma, *shm = &dma;
static sfxcache_t oldCache, movedCache;
static sfx_t sound;
static byte readBuffer[CHUNK_BYTES + 1];
static int relocation, resamples, frees, closes, allocations;
static bool allocationFails, readFails;
static FileHandle_t openFile = reinterpret_cast<void*>(1);
#define Q_memset std::memset
#define Q_strncpy std::strncpy
#define Q_strcpy std::strcpy
static void Con_DPrintf(const char*, ...) {}
static void* Cache_Check(cache_user_t* user) { return user->data; }
static void Cache_Free(cache_user_t* user) {
 assert(nullptr != user->data); ++frees; user->data = nullptr;
}
static void* Cache_Alloc(cache_user_t* user, int, const char*) {
 assert(nullptr == user->data); ++allocations;
 return user->data = allocationFails ? nullptr : &movedCache;
}
static void FS_Close(FileHandle_t file) { assert(openFile == file); ++closes; }
static void SND_CloseMouth(channel_t*) {}
static void VoiceSE_NotifyFreeChannel(int) { assert(false); }
static sfxcache_t* S_LoadSound(sfx_t*, channel_t*) { assert(false); return nullptr; }
static void VOX_TrimStartEndTimes(channel_t*, sfxcache_t*) { assert(false); }
static qboolean GetWavinfo(char*, byte*, int, wavinfo_t* info) {
 *info = {1, 1, SAMPLE_RATE, CHUNK_BYTES * 3, DATA_OFFSET, -1}; return 1;
}
static byte* COM_LoadFileLimit(char*, int, int, int* bytes, FileHandle_t* file) {
 *file = openFile; *bytes = CHUNK_BYTES;
 if (relocation) {
  movedCache = oldCache;
  sound.cache.data = relocation == 1 ? &movedCache : nullptr;
 }
 return readFails ? nullptr : readBuffer;
}
static void ResampleSfx(sfx_t* s, int rate, int width, byte* data, int bytes) {
 assert(nullptr != data); assert(bytes > 0);
 assert(SAMPLE_RATE == rate); assert(1 == width);
 assert(&oldCache != s->cache.data || relocation == 0);
 ++resamples;
}
'''

TESTS = r'''
static void Reset() {
 oldCache = {OLD_LENGTH, -1, SAMPLE_RATE, 1, 1}; movedCache = {};
 sound = {}; std::strcpy(sound.name, "*test.wav"); sound.cache.data = &oldCache;
 channels[0] = {&sound, CHAN_STREAM, -1, 0, 99, 0};
 wavstreams[0] = {0, 0, openFile, {1, 1, SAMPLE_RATE, CHUNK_BYTES * 3, DATA_OFFSET, -1}, 0};
 relocation = resamples = frees = closes = allocations = 0;
 allocationFails = readFails = false;
}
int main(int argc, char** argv) {
 assert(2 == argc); Reset(); std::string test = argv[1];
 if (test == "chunk-move" || test == "chunk-evict") {
  relocation = test == "chunk-move" ? 1 : 2;
  Wavstream_GetNextChunk(channels, &sound);
  assert(OLD_LENGTH == oldCache.length);
  assert((relocation == 1 ? 1 : 0) == resamples);
  if (relocation == 1) assert(CHUNK_BYTES == movedCache.length);
 } else if (test == "mixer-move" || test == "mixer-evict") {
  relocation = test == "mixer-move" ? 1 : 2;
  auto* cache = &oldCache;
  int result = S_CheckWavEnd(channels, &cache, MIX_TIME, 0);
  if (relocation == 1) {
   assert(0 == result); assert(&movedCache == cache);
   assert(CHUNK_BYTES + MIX_TIME == channels[0].end); assert(0 == channels[0].pos);
  } else {
   assert(1 == result); assert(nullptr == cache); assert(nullptr == channels[0].sfx);
   assert(1 == closes); assert(0 == frees); assert(0 == resamples);
  }
 } else if (test == "free-evicted" || test == "free-live") {
  if (test == "free-evicted") sound.cache.data = nullptr;
  S_FreeChannel(channels);
  assert((test == "free-live" ? 1 : 0) == frees);
  assert(1 == closes); assert(nullptr == channels[0].sfx);
 } else if (test == "load-move" || test == "load-evict" || test == "load-alloc-fail") {
  wavstreams[0].hFile = nullptr;
  relocation = test == "load-move" ? 1 : 2;
  allocationFails = test == "load-alloc-fail";
  auto* cache = S_LoadStreamSound(&sound, channels);
  assert(OLD_LENGTH == oldCache.length);
  if (allocationFails) {
   assert(nullptr == cache); assert(1 == closes); assert(nullptr == wavstreams[0].hFile);
  } else {
   assert(&movedCache == cache); assert(CHUNK_BYTES - DATA_OFFSET == cache->length);
   assert(1 == resamples); assert(0 == closes);
  }
  assert((relocation == 2 ? 1 : 0) == allocations);
 } else if (test == "normal" || test == "final-chunk") {
  if (test == "final-chunk") wavstreams[0].csamplesplayed = wavstreams[0].info.samples - OLD_LENGTH;
  auto* cache = &oldCache;
  assert(0 == S_CheckWavEnd(channels, &cache, MIX_TIME, 0));
  int length = test == "final-chunk" ? OLD_LENGTH : CHUNK_BYTES;
  assert(length == cache->length); assert(length + MIX_TIME == channels[0].end);
  assert(1 == resamples);
 } else if (test == "end") {
  wavstreams[0].csamplesplayed = wavstreams[0].info.samples;
  auto* cache = &oldCache;
  assert(1 == S_CheckWavEnd(channels, &cache, MIX_TIME, 0));
  assert(1 == closes); assert(1 == frees); assert(0 == resamples);
 } else { assert(false); }
 std::printf("PASS: %s\n", argv[1]);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision')
    args = parser.parse_args()

    def read(name):
        path = 'GoldSrc-SDK/engine/' + name
        if args.revision:
            return subprocess.check_output(['git', 'show', f'{args.revision}:{path}'], cwd=ROOT).decode()
        return (ROOT / path).read_text()

    mix, mem = read('snd_mix.cpp'), read('snd_mem.cpp')
    functions = [function(mix, signature) for signature in (
        'extern "C" void Wavstream_Close(', 'extern "C" void S_FreeChannel(',
        'void Wavstream_GetNextChunk(', 'qboolean S_CheckWavEnd(')]
    functions.append(function(mem, 'sfxcache_t* S_LoadStreamSound('))
    scenarios = ['chunk-move', 'chunk-evict', 'mixer-move', 'mixer-evict',
                 'free-evicted', 'free-live', 'load-move', 'load-evict',
                 'load-alloc-fail', 'normal', 'final-chunk', 'end']
    with tempfile.TemporaryDirectory(prefix='goldsrc-stream-') as folder:
        source, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source.write_text(STUBS + '\n'.join(functions) + TESTS)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        subprocess.run(compiler + ['-std=c++17', str(source), '-o', str(executable)], check=True)
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
