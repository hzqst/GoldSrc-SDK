"""Exercise Windows waveOut submission using production shutdown/startup and device doubles."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
constexpr int WAV_BUFFERS = 64, WAV_MASK = WAV_BUFFERS - 1, WHDR_DONE = 1, MMSYSERR_NOERROR = 0;
struct WAVEHDR { int dwFlags; }; using LPWAVEHDR = WAVEHDR*;
static WAVEHDR headers[WAV_BUFFERS]; static LPWAVEHDR lpWaveHdr = headers;
static int hWaveOut, snd_completed, snd_sent, sample16;
static bool wav_init, fakedma, snd_initialized = true, failWrite, failRestart;
static int sound_started, shutdowns, startups, writes;
struct Dma { int gamealive; char* buffer; };
static Dma dma, *shm;
static void Con_DPrintf(const char*, ...) {}
static void Con_Printf(const char*, ...) {}
static void Con_SafePrintf(const char*, ...) {}
static void VOX_Shutdown() {}
static void FreeSound() { delete[] dma.buffer; dma.buffer = nullptr; wav_init = false; }
static void SNDDMA_Shutdown() {
 assert(nullptr == shm); assert(0 == sound_started); ++shutdowns; FreeSound();
}
static bool SNDDMA_Init() {
 ++startups; if(failRestart) return false;
 dma = {1, new char[128]}; shm = &dma; wav_init = true; snd_sent = snd_completed = 0;
 return true;
}
static int waveOutWrite(int, LPWAVEHDR, int) { ++writes; return failWrite ? -1 : MMSYSERR_NOERROR; }
'''

TESTS = r'''
int main() {
 for(bool restartFails : {false, true}) {
  shutdowns = startups = writes = 0; failWrite = true; failRestart = restartFails;
  dma = {1, new char[128]}; shm = &dma; sound_started = 1; wav_init = true;
  snd_completed = snd_sent = 0;
  SNDDMA_Submit();
  assert(1 == writes); assert(1 == shutdowns); assert(1 == startups);
  if(restartFails) {assert(nullptr == shm); assert(0 == sound_started); assert(!wav_init);}
  else {assert(&dma == shm); assert(nullptr != shm->buffer); assert(1 == sound_started); S_Shutdown();}
 }
 failWrite = failRestart = false; SNDDMA_Init(); sound_started = 1;
 writes = shutdowns = startups = 0; SNDDMA_Submit();
 assert(8 == writes); assert(0 == shutdowns); assert(0 == startups); S_Shutdown();
}
'''


def main():
    dma = (ROOT / 'snd_dma.cpp').read_text()
    win = (ROOT / 'snd_win.cpp').read_text()
    code = '#include <initializer_list>\n' + STUBS
    code += function(dma, 'extern "C" void S_Startup(')
    code += function(dma, 'extern "C" void S_Shutdown(')
    code += function(win, 'extern "C" void SNDDMA_Submit(') + TESTS
    with tempfile.TemporaryDirectory(prefix='goldsrc-waveout-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(code)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: waveOutWrite failure, restart success/failure, shutdown ordering and normal submission')


if __name__ == '__main__':
    main()
