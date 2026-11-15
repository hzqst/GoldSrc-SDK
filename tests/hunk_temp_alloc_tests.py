"""Compile the production hunk-temp-buffer call sites with deterministic stubs.

Run: python tests/hunk_temp_alloc_tests.py
Requires a C++17 compiler (CXX, or clang++ on Windows / c++ elsewhere).

Hunk_TempAlloc returns NULL when the hunk is full. Each scenario forces that and
asserts the production code never reads or copies into the NULL buffer:

- sounds: S_LoadSound closes the file, falls through to its load failure and
  reports "Couldn't load" (#110).
- models: Mod_LoadModel closes the file and runs its existing load failure
  instead of reading into NULL (#111).
- exports: GetExportNames reports no exports instead of copying the export names
  into NULL, and treats OpenFile's HFILE_ERROR as failure (#111).
"""
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
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using byte = unsigned char;
using qboolean = int;
using FileHandle_t = void*;
using CRC32_t = unsigned int;

#if !defined(_WIN32)
constexpr int TRUE = 1; // windows.h defines it
#endif
constexpr int MAX_OSPATH = 260;
constexpr int MAX_KNOWN_MODELS = 1024;
constexpr char CHAR_STREAM = '*';
constexpr char CHAR_USERVOX = '?';
constexpr int IDSTUDIOHEADER = 0x54534449; // "IDST"
constexpr int IDPOLYHEADER = 0x4f504449;   // "IDPO"
constexpr int IDSPRITEHEADER = 0x50534449; // "IDSP"
#define Q_strcpy std::strcpy

struct cache_user_t { void* data; };
struct sfxcache_t { int length, loopstart, speed, width, stereo; byte data[1]; };
struct sfx_t { char name[64]; cache_user_t cache; };
struct channel_t { int unused; };
struct wavinfo_t { int rate, width, channels, loopstart, samples, dataofs; };
struct cvar_t { float value; };
struct model_t
{
 int type;
 cache_user_t cache;
 int needload;
 char name[MAX_OSPATH];
};
enum { mod_alias, mod_sprite, mod_studio, mod_brush };
enum { NL_PRESENT, NL_NEEDS_LOADED, NL_UNREFERENCED };
struct mod_known_info_t { int firstCRCDone; unsigned initialCRC; int shouldCRC; };
struct client_state_t { int state; };
enum { ca_dedicated };
struct dma_t { int speed; };

static cvar_t hisound{};
static cvar_t loadas8bit{};
static cvar_t fs_precache_timings{};
static cvar_t developer{};
static volatile dma_t* shm = nullptr;
static client_state_t cls{};
static model_t mod_known[MAX_KNOWN_MODELS]{};
static mod_known_info_t mod_known_info[MAX_KNOWN_MODELS]{};
static model_t* loadmodel{};
static char loadname[32]{};
static int g_bIsCStrike{};

static int reads, closes, loads, mappings, hunkCalls;
static bool readIntoNull, copyIntoNull, hunkFull;
static char printed[512];
static byte g_hunkBuffer[4096];

static void ResetCounters()
{
 reads = closes = loads = mappings = hunkCalls = 0;
 readIntoNull = copyIntoNull = false;
 printed[0] = 0;
}

static void Check(bool condition)
{
 if (!condition) throw std::runtime_error("hunk temp alloc assertion failed");
}

static void* Cache_Check(cache_user_t* user) { return user->data; }
static void Cache_Free(cache_user_t* user) { user->data = nullptr; }
static void* Cache_Alloc(cache_user_t*, int, const char*) { return nullptr; }
static short LittleShort(short value) { return value; }
static int LittleLong(int value) { return value; }

static void Con_Printf(const char* format, ...)
{
 va_list args;
 va_start(args, format);
 vsnprintf(printed, sizeof(printed), format, args);
 va_end(args);
}
static void Con_DPrintf(const char* format, ...)
{
 va_list args;
 va_start(args, format);
 vsnprintf(printed, sizeof(printed), format, args);
 va_end(args);
}
[[noreturn]] static void Sys_Error(const char* format, ...)
{
 throw std::runtime_error(format);
}
static double Sys_FloatTime() { return 0.0; }
static char* COM_CheckParm(const char*) { return nullptr; }
static char* COM_FileBase(const char* filename, char*) { return const_cast<char*>(filename); }
static void FS_GetLocalCopy(const char*) {}
static void FS_ReleaseReadBuffer(FileHandle_t, void*) {}
static void FS_Close(FileHandle_t file) { if (file) ++closes; }
static int FS_Read(void* output, int size, int count, FileHandle_t)
{
 if (!output) readIntoNull = true;
 ++reads;
 return size * count;
}
static void* Hunk_TempAlloc(int)
{
 ++hunkCalls;
 return hunkFull ? nullptr : g_hunkBuffer;
}

// S_LoadSound also needs these; the model/export scenarios never reach them.
static sfxcache_t* S_LoadStreamSound(sfx_t*, channel_t*) { return nullptr; }
static sfxcache_t* VoiceSE_GetSFXCache(sfx_t*) { return nullptr; }
static qboolean GetWavinfo(char*, byte*, int, wavinfo_t*) { return 0; }
static void ResampleSfx(sfx_t*, int, int, byte*, int) {}
static void Mod_LoadAliasModel(model_t*, void*) {}
static void Mod_LoadSpriteModel(model_t*, void*) {}
static void Mod_LoadStudioModel(model_t*, void*) {}
static void Mod_LoadBrushModel(model_t*, void*) {}
static void DT_LoadDetailMapFile(const char*) {}
static void CRC32_Init(CRC32_t*) {}
static void CRC32_ProcessBuffer(CRC32_t*, const void*, int) {}
static CRC32_t CRC32_Final(CRC32_t crc) { return crc; }
static void SetCStrikeFlags() {}
static int IsGameSubscribed(const char*) { return 0; }
static int IsCZPlayerModel(unsigned, const char*) { return 0; }
static void COM_ExplainDisconnection(qboolean, const char*, ...) {}
static void CL_Disconnect() {}

// File inputs. A read buffer is never offered, so both loaders take the
// Hunk_TempAlloc path.
static int g_fileSize = 4096;
static FileHandle_t FS_Open(const char*, const char*) { return reinterpret_cast<FileHandle_t>(1); }
static unsigned int FS_Size(FileHandle_t) { return static_cast<unsigned int>(g_fileSize); }
static void* FS_GetReadBuffer(FileHandle_t, int* size) { *size = 0; return nullptr; }
static byte* COM_LoadStackFile(char*, void*, int, int* length) { ++loads; *length = 0; return nullptr; }

#if defined(_WIN32)
// Route the export-name loader's Win32 file calls to stubs. These macros must
// precede the production functions that call them.
static HFILE g_openResult;
static void* g_mappedImage;

static HFILE StubOpenFile(const char*, OFSTRUCT*, UINT) { return g_openResult; }
static HANDLE StubCreateFileMappingA(HANDLE, LPSECURITY_ATTRIBUTES, DWORD, DWORD, DWORD, LPCSTR)
{
 ++mappings;
 return reinterpret_cast<HANDLE>(static_cast<INT_PTR>(2));
}
static void* StubMapViewOfFile(HANDLE, DWORD, DWORD, DWORD, SIZE_T) { return g_mappedImage; }
static BOOL StubUnmapViewOfFile(const void*) { return TRUE; }

#define OpenFile StubOpenFile
#define CreateFileMappingA StubCreateFileMappingA
#define MapViewOfFile StubMapViewOfFile
#define UnmapViewOfFile StubUnmapViewOfFile
#endif
'''

TESTS = r'''
// A minimal PE image whose only section is ".rdata" and whose export directory
// lists two name strings; GetExportNamesLength must report 24 bytes for it.
static void BuildSyntheticImage(std::vector<byte>& image)
{
 constexpr std::uint32_t rawBase = 0x400;
 constexpr std::uint32_t sectionRva = 0x1000;
 constexpr std::uint32_t exportRva = 0x1000;
 constexpr std::uint32_t namesRva = 0x1040;
 constexpr std::uint32_t firstStringRva = 0x1060;
 constexpr int nameBytes = 12;
 const char* names[2] = {"?Foo@@YAXXZ", "?Bar@@YAXXZ"};

 image.assign(0x1000, 0);
 auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());
 dos->e_lfanew = 0x40;
 auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image.data() + dos->e_lfanew);
 nt->FileHeader.NumberOfSections = 1;
 nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_DIRECTORY_ENTRY_EXPORT + 1;
 nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress = exportRva;

 auto* section = IMAGE_FIRST_SECTION(nt);
 std::memcpy(section->Name, ".rdata", 6);
 section->VirtualAddress = sectionRva;
 section->SizeOfRawData = 0x400;
 section->PointerToRawData = rawBase;

 auto* exports = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(image.data() + rawBase);
 exports->NumberOfNames = 2;
 exports->AddressOfNames = namesRva;

 auto* rvas = reinterpret_cast<DWORD*>(image.data() + rawBase + (namesRva - sectionRva));
 rvas[0] = firstStringRva;
 rvas[1] = firstStringRva + nameBytes;

 byte* strings = image.data() + rawBase + (firstStringRva - sectionRva);
 std::memcpy(strings, names[0], nameBytes);
 std::memcpy(strings + nameBytes, names[1], nameBytes);
}

int main(int argc, char** argv)
{
 try
 {
  Check(argc == 2);
  const std::string test = argv[1];
  ResetCounters();
  shm = nullptr; // no resampling in these scenarios
  hunkFull = false;

  sfx_t sound{};
  std::strcpy(sound.name, "buttons/blip1.wav");
  model_t model{};
  model.type = mod_brush;
  model.needload = NL_NEEDS_LOADED;
  std::strcpy(model.name, "maps/test.bsp");

#if defined(_WIN32)
  std::vector<byte> image;
  char* exportNames = reinterpret_cast<char*>(1);
  int exportLength = -1;
  g_openResult = 3;
#endif

  if (test == "sound-full-hunk")
  {
   hunkFull = true;
   Check(nullptr == S_LoadSound(&sound, nullptr));
   Check(0 == reads);
   Check(!readIntoNull);
   Check(1 == closes);
   Check(1 == loads);
   Check(std::string(printed).find("Couldn't load") != std::string::npos);
  }
  else if (test == "sound-with-buffer")
  {
   Check(nullptr == S_LoadSound(&sound, nullptr));
   Check(1 == reads);
   Check(!readIntoNull);
   Check(std::string(printed).find("Failed loading") != std::string::npos);
  }
  else if (test == "model-full-hunk")
  {
   hunkFull = true;
   Check(nullptr == Mod_LoadModel(&model, 0, 0));
   Check(0 == reads);
   Check(!readIntoNull);
   Check(1 == closes);
   Check(std::string(printed).find("could not load file") != std::string::npos);
  }
  else if (test == "model-with-buffer")
  {
   Check(&model == Mod_LoadModel(&model, 0, 0));
   Check(1 == reads);
   Check(!readIntoNull);
   Check(1 == closes);
  }
#if defined(_WIN32)
  else if (test == "exports-open-fail")
  {
   g_openResult = HFILE_ERROR;
   exportLength = GetExportNames("hl.dll", &exportNames);
   Check(0 == exportLength);
   Check(nullptr == exportNames);
   Check(0 == mappings);
  }
  else if (test == "exports-full-hunk")
  {
   BuildSyntheticImage(image);
   g_mappedImage = image.data();
   hunkFull = true;
   exportLength = GetExportNames("hl.dll", &exportNames);
   Check(0 == exportLength);
   Check(nullptr == exportNames);
   Check(1 == mappings);
  }
  else if (test == "exports-with-buffer")
  {
   BuildSyntheticImage(image);
   g_mappedImage = image.data();
   exportLength = GetExportNames("hl.dll", &exportNames);
   Check(24 == exportLength);
   Check(nullptr != exportNames);
   Check(0 == std::memcmp(exportNames, "?Foo@@YAXXZ\0?Bar@@YAXXZ", 24));
   Check(1 == mappings);
  }
#endif
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
    engine = ROOT / 'GoldSrc-SDK/engine'
    snd_mem = (engine / 'snd_mem.cpp').read_text()
    gl_model = (engine / 'gl_model.cpp').read_text()
    sys_dll = (engine / 'sys_dll.cpp').read_text()

    functions = [function(snd_mem, signature) for signature in (
        'static bool S_KeepHighRateSounds()',
        'static float S_GetResampleStep( int inrate )',
        'static int S_GetResampledLength( int samples, float stepscale )',
        'static int S_GetResampledBytes( int samples, int width, float stepscale )',
        'sfxcache_t* S_LoadSound( sfx_t* s, channel_t* ch )')]
    functions.append(function(gl_model, 'model_t* Mod_LoadModel( model_t* mod, qboolean crash, qboolean trackCRC )'))
    if os.name == 'nt':
        # GetExportNames only exists in the Windows build of sys_dll.cpp.
        functions += [function(sys_dll, signature) for signature in (
            'static byte* GetDirectoryInFile( byte* image, unsigned int directory )',
            'static char* GetExportNamesInFile( byte* image )',
            'static int GetExportNamesLength( byte* image )',
            'static void CopyExportNames( byte* image, char* output )',
            'static int GetExportNames( const char* filename, char** names )')]

    scenarios = ['sound-full-hunk', 'sound-with-buffer',
                 'model-full-hunk', 'model-with-buffer']
    if os.name == 'nt':
        # GetExportNames only exists in the Windows build of sys_dll.cpp.
        scenarios += ['exports-open-fail', 'exports-full-hunk', 'exports-with-buffer']
    with tempfile.TemporaryDirectory(prefix='goldsrc-hunk-') as folder:
        source_file, executable = Path(folder) / 'test.cpp', Path(folder) / 'test.exe'
        source_file.write_text(STUBS + '\n'.join(functions) + TESTS)
        compiler = shlex.split(os.environ.get('CXX', 'clang++' if os.name == 'nt' else 'c++'))
        subprocess.run(compiler + ['-std=c++17', str(source_file), '-o', str(executable)], check=True)
        failed = []
        for scenario in scenarios:
            result = subprocess.run([str(executable), scenario])
            if result.returncode:
                failed.append(scenario)
        if failed:
            raise SystemExit('FAILED: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
