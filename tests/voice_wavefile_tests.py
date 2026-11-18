"""Exercise production WAV reader/writer and recording admission with a memory filesystem."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'
STUBS = r'''
#include <cassert>
#include <cstring>
#include <vector>
#include <algorithm>
#include <climits>
using FileHandle_t = void*;
constexpr int FILESYSTEM_SEEK_HEAD = 0;
static std::vector<char> file;
static unsigned int position, reportedSize;
static int opens, closes, reads, shortRead = -1;
static bool openFails;
static FileHandle_t FS_Open(const char*, const char* mode) {
 if(openFails) return nullptr;
 ++opens; position = 0; reads = 0;
 if(*mode == 'w') file.clear();
 return &file;
}
static void FS_Close(FileHandle_t) { ++closes; }
static unsigned int FS_Size(FileHandle_t) { return reportedSize ? reportedSize : file.size(); }
static unsigned int FS_Tell(FileHandle_t) { return position; }
static void FS_Seek(FileHandle_t, int offset, int) { assert(offset >= 0); position = offset; }
static int FS_Read(void* output, int size, int, FileHandle_t) {
 assert(size >= 0); ++reads;
 int count = std::min<unsigned int>(size, position < file.size() ? file.size() - position : 0);
 if(reads == shortRead && count) --count;
 std::memcpy(output, file.data() + position, count); position += count; return count;
}
static void FS_Write(const void* input, int size, int, FileHandle_t) {
 if(position + size > file.size()) file.resize(position + size);
 std::memcpy(file.data() + position, input, size); position += size;
}
'''

RECORD_STUBS = r'''
using qboolean = int;
constexpr int TRUE = 1, FALSE = 0, VOICE_LOCAL_PLAYER = -1, MAX_WAVEFILEDATA_LEN = 64;
static char* g_pMicInputFileData;
static int g_nMicInputFileBytes, g_CurMicInputFileByte;
static double g_MicStartTime;
static char *g_pUncompressedFileData, *g_pDecompressedFileData;
static const char *g_pUncompressedDataFilename, *g_pDecompressedDataFilename;
static int g_nUncompressedDataBytes, g_nDecompressedDataBytes;
static bool g_bUsingSteamVoice, g_bVoiceRecording;
static int starts;
struct Codec { void ResetState() {} }; static Codec* g_pEncodeCodec;
struct Recorder { bool RecordStart() {++starts; return true;} }; static Recorder recorder, *g_pVoiceRecord = &recorder;
struct Steam { void StartVoiceRecording() {++starts;} }; static Steam* SteamUser() {return nullptr;}
static struct { int wBitsPerSample, nChannels, nSamplesPerSec; } g_VoiceSampleFormat = {16, 1, 11025};
static void Con_Printf(const char*, ...) {}
static double Sys_FloatTime() {return 0;}
static void ClientDLL_VoiceStatus(int, bool) {}
static void Voice_RecordStop() {
 delete[] g_pMicInputFileData; g_pMicInputFileData = nullptr;
 delete[] g_pUncompressedFileData; g_pUncompressedFileData = nullptr;
 delete[] g_pDecompressedFileData; g_pDecompressedFileData = nullptr;
 g_bVoiceRecording = false;
}
'''

TESTS = r'''
static void Set32(size_t offset, uint32_t value) {std::memcpy(file.data()+offset, &value, sizeof(value));}
static void Set16(size_t offset, uint16_t value) {std::memcpy(file.data()+offset, &value, sizeof(value));}
static void Check(bool expected) {
 char marker; char* data = &marker; int bytes = -1, bits = -2, channels = -3, rate = -4;
 int before = closes;
 assert(expected == ReadWaveFile("test.wav", data, bytes, bits, channels, rate));
 assert(before + !openFails == closes);
 if(expected) {
  assert(8 == bytes); assert(16 == bits); assert(1 == channels); assert(11025 == rate);
  for(int i=0;i<bytes;++i) assert(char(i+1) == data[i]);
  delete[] data;
 } else {assert(&marker == data); assert(-1 == bytes); assert(-2 == bits); assert(-3 == channels); assert(-4 == rate);}
}
int main() {
 const char samples[] = {1,2,3,4,5,6,7,8};
 assert(WriteWaveFile("test.wav", samples, sizeof(samples), 16, 1, 11025));
 assert(52 == file.size()); const auto valid = file; Check(true);
 // Every truncation must be rejected without changing any outputs.
 for(size_t size=0;size<valid.size();++size) {file.assign(valid.begin(), valid.begin()+size); Check(false);}
 for(int offset : {0, 8, 12, 36}) {file=valid; file[offset]='?'; Check(false);}
 file=valid; Set32(4, UINT32_MAX); Check(false);
 file=valid; Set32(4, 28); Check(false);
 file=valid; Set32(16, 15); Check(false);
 file=valid; Set32(40, UINT32_MAX); Check(false);
 file=valid; Set32(40, 0); Check(false);
 for(int offset : {20,22,34}) {file=valid; Set16(offset, 0); Check(false);}
 file=valid; Set16(20, 3); Check(false);
 file=valid; Set32(24, 0); Check(false);
 file=valid; Set32(24, UINT32_MAX); Check(false);
 // Simulate a short read at every operation, including the sample payload.
 file=valid; Check(true); int readCount=reads;
 for(shortRead=1;shortRead<=readCount;++shortRead) Check(false);
 shortRead=-1;
 file=valid; reportedSize=UINT_MAX; Check(false); reportedSize=0;
 openFails=true; Check(false); openFails=false;
 // Odd unknown chunks and extended fmt chunks must be skipped with padding.
 file=valid; const char junk[]={'J','U','N','K',1,0,0,0,'x',0};
 file.insert(file.begin()+36, junk, junk+sizeof(junk)); Set32(4,file.size()-8); Check(true);
 file=valid; file.insert(file.begin()+36,2,0); Set32(16,18); Set32(4,file.size()-8); Check(true);
 // data before fmt is rejected.
 file=valid; std::rotate(file.begin()+12,file.begin()+36,file.end()); Check(false);
 // Recording rejects malformed and incompatible files before starting a device.
 file=valid; assert(TRUE == Voice_RecordStart(nullptr,nullptr,"test.wav")); assert(1==starts); Voice_RecordStop();
 for(int field : {22,24,34}) {
  file=valid; if(field==24) Set32(field,22050); else Set16(field,field==22 ? 2 : 8);
  assert(FALSE==Voice_RecordStart(nullptr,nullptr,"test.wav")); assert(nullptr==g_pMicInputFileData); assert(1==starts);
 }
 file.clear(); assert(FALSE==Voice_RecordStart(nullptr,nullptr,"test.wav")); assert(1==starts);
 assert(opens==closes); Voice_RecordStop();
}
'''


def main():
    wave = (ROOT / 'voice_wavefile.cpp').read_text()
    wave = '\n'.join(line for line in wave.splitlines()
                     if line not in ('#include "voice_wavefile.h"', '#include "filesystem.h"'))
    voice = (ROOT / 'voice.cpp').read_text()
    start = voice.index('qboolean Voice_RecordStart(')
    record = voice[start:voice.index('\n}', start) + 2]
    with tempfile.TemporaryDirectory(prefix='goldsrc-wave-') as folder:
        path = Path(folder)
        (path / 'test.cpp').write_text(STUBS + wave + RECORD_STUBS + record + TESTS)
        executable = compile_program([path / 'test.cpp'], path / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], check=True)
    print('PASS: WAV round-trip, chunks, truncations, short reads, cleanup and recording format')


if __name__ == '__main__':
    main()
