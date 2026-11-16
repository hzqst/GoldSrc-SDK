"""Compile the production mixer with deterministic cache failures."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#define Q_memset std::memset
using byte = unsigned char;
constexpr int SOUND_CALLBACK_CACHE_LENGTH = 1 << 30, CHAN_VOICE = 2, CHAN_STREAM = 5;
struct sfx_t { bool fails; };
struct sfxcache_t { int length, loopstart, speed, width, stereo; byte data[128]; };
struct channel_t { sfx_t* sfx; int leftvol, rightvol, pitch, isentence, iword, end, entnum, entchannel, pos; };
struct portable_samplepair_t { int left, right; };
static channel_t channels[3];
static int total_channels = 3, paintedtime = 0, loads, mixes;
static sfxcache_t cache = {64, -1, 11025, 1, 0, {}};
static struct { int speed; } dma = {11025}, *shm = &dma;
struct voxword_t { int pitch, timecompress; };
static voxword_t rgrgvoxword[1][1];
static struct { int csamplesplayed; } wavstreams[3];
static portable_samplepair_t paintbuffer[1024];
static float g_SND_VoiceOverdrive = 1;
using soundDataCallback_t = int(*)(sfxcache_t*, char*, int, int, int);
static sfxcache_t* S_LoadSound(sfx_t* s, channel_t*) { ++loads; return s->fails ? nullptr : &cache; }
static void S_FreeChannel(channel_t*) { assert(false); }
static void SND_MoveMouth(channel_t*, sfxcache_t*, int) { assert(false); }
static void SND_PaintChannelFrom16Offs(portable_samplepair_t*, channel_t*, short*, int, int) { assert(false); }
static int VOX_FPaintPitchChannelFrom8Offs(portable_samplepair_t*, channel_t*, sfxcache_t*, int, int, int, int) { assert(false); return 0; }
static void SND_PaintChannelFrom8Offs(portable_samplepair_t*, channel_t*, sfxcache_t*, int, int) { assert(false); }
static void SND_PaintChannelFrom8(channel_t* ch, sfxcache_t*, int count) { ++mixes; ch->pos += count; }
static int S_CheckWavEnd(channel_t*, sfxcache_t**, int, int) { return 1; }
constexpr int MAX_RAW_SAMPLES = 1024;
static portable_samplepair_t s_rawsamples[MAX_RAW_SAMPLES];
static int s_rawend = -1;
static struct { bool paused; } cl;
static bool s_careerAudioPaused, deviceFails;
static struct { float value; } hisound;
static int transfers;
static void SX_RoomFX(int, int, int) {}
static void S_TransferPaintBuffer(int) {
 assert(nullptr != shm); ++transfers;
 if(deviceFails) shm = nullptr;
}
'''

TESTS = r'''
int main() {
 sfx_t failing = {true}, healthy = {false};
 channels[0] = {&failing, 100, 100, 100, -1, 0, 64, 0, 0, 0};
 channels[1] = {&healthy, 100, 100, 100, -1, 0, 64, 0, 0, 0};
 S_MixChannelsToPaintbuffer(32, 0, 0);
 assert(2 == loads); assert(1 == mixes); assert(32 == channels[1].pos);
 assert(&failing == channels[0].sfx); assert(0 == channels[0].pos);
 // A transient failure must be retried successfully on the next paint.
 failing.fails = false;
 S_MixChannelsToPaintbuffer(32, 0, 0);
 assert(4 == loads); assert(3 == mixes); assert(32 == channels[0].pos);
 // Multiple paint chunks must stop immediately if device restart fails.
 channels[0].sfx = channels[1].sfx = nullptr;
 deviceFails = true; paintedtime = transfers = 0;
 S_PaintChannels(1200); assert(1 == transfers); assert(nullptr == shm);
 // Healthy transfers must still process the full update.
 deviceFails = false; shm = &dma; paintedtime = transfers = 0;
 S_PaintChannels(1200); assert(3 == transfers); assert(1200 == paintedtime);
}
'''


def main():
    mix = (ROOT / 'GoldSrc-SDK/engine/snd_mix.cpp').read_text()
    code = (STUBS + function(mix, 'void S_MixChannelsToPaintbuffer(')
            + function(mix, 'void S_PaintChannels(') + TESTS)
    with tempfile.TemporaryDirectory(prefix='goldsrc-mixer-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        compiler = shlex.split(os.environ.get('CXX', 'c++'))
        subprocess.run(compiler + ['-std=c++17', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                                  str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
        subprocess.run([str(path / 'test')], check=True)
    print('PASS: cache reload failure, subsequent channel, retry and device loss during paint')


if __name__ == '__main__':
    main()
