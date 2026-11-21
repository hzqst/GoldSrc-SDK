"""Exercise the demo editing commands against temporary demo directories."""
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
    return source[start:source.index(end, start) + len(end)]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>
#ifdef _WIN32
#define Q_strcasecmp _stricmp
#else
#include <strings.h>
#define Q_strcasecmp strcasecmp
#endif
#define _unlink std::remove
#define Q_strstr std::strstr
#define Q_strncmp std::strncmp
#define Q_strncpy std::strncpy
#define Q_atoi std::atoi
constexpr int MAX_PATH = 260, MAX_DEMO_NAME = 256, MAX_DEMO_ENTRIES = 16;
constexpr int src_command = 1, FILESYSTEM_SEEK_HEAD = SEEK_SET, FILESYSTEM_SEEK_TAIL = SEEK_END;
constexpr int PROTOCOL_VERSION = 48, DEMO_PROTOCOL = 5;
enum { FDEMO_TITLE=1, FDEMO_CDTRACK=2, FDEMO_FADE_IN_SLOW=4, FDEMO_FADE_IN_FAST=8, FDEMO_FADE_OUT_SLOW=16, FDEMO_FADE_OUT_FAST=32 };
using FileHandle_t = FILE*;
struct demoentry_t { int nOffset, nFileLength, nFlags, nCDTrack; char szDescription[24]; };
static struct { char szFileStamp[8]; int nNetProtocolVersion, nDemoProtocol, nDirectoryOffset; } demoheader;
static struct { int nEntries; demoentry_t* p_rgEntries; } demodir;
static struct { bool demorecording, demoplayback; FileHandle_t demofile; } cls;
static int cmd_source = src_command;
static std::vector<const char*> command;
static int Cmd_Argc() { return static_cast<int>(command.size()); }
static const char* Cmd_Argv(int index) { return index < Cmd_Argc() ? command[index] : ""; }
static void Con_Printf(const char*, ...) {}
static bool COM_HasFileExtension(const char* filename, const char* extension)
{
    const char* dot = std::strrchr(filename, '.');
    return dot && 0 == std::strcmp(dot + 1, extension);
}
static void COM_DefaultExtension(char* filename, const char* extension)
{
    if (!std::strrchr(filename, '.')) std::strcat(filename, extension);
}
static void COM_StripExtension(const char* filename, char* output)
{
    const char* dot = std::strrchr(filename, '.');
    const size_t count = dot ? dot - filename : std::strlen(filename);
    std::memcpy(output, filename, count);
    output[count] = '\0';
}
static void COM_AddExtension(char* filename, const char* extension, size_t size)
{
    assert(std::strlen(filename) + std::strlen(extension) < size);
    std::strcat(filename, extension);
}
static FILE* FS_Open(const char* name, const char* mode) { return std::fopen(name, mode); }
static FILE* FS_OpenPathID(const char* name, const char* mode, const char*) { return FS_Open(name, mode); }
static void FS_Close(FILE* file) { std::fclose(file); }
static void FS_Seek(FILE* file, int offset, int from) { assert(0 == std::fseek(file, offset, from)); }
static long FS_Tell(FILE* file) { return std::ftell(file); }
static void FS_Read(void* dest, size_t size, size_t count, FILE* file) { assert(count == std::fread(dest, size, count, file)); }
static void FS_Write(const void* src, size_t size, size_t count, FILE* file) { assert(count == std::fwrite(src, size, count, file)); }
static void* Mem_Malloc(size_t size) { return std::malloc(size); }
static void Mem_Free(void* memory) { std::free(memory); }
static void CL_DemoCopyFileChunk(FILE* dst, FILE* src, int size)
{
    for (int i = 0; i < size; ++i)
    {
        const int value = std::fgetc(src);
        assert(value != EOF && EOF != std::fputc(value, dst));
    }
}
'''

TESTS = r'''
static void WriteDemo(const char* filename)
{
    FILE* file = FS_Open(filename, "wb");
    assert(file);
    demoheader = {"HLDEMO", PROTOCOL_VERSION, DEMO_PROTOCOL, sizeof(demoheader) + 3};
    FS_Write(&demoheader, sizeof(demoheader), 1, file);
    FS_Write("abc", 1, 3, file);
    const int count = 3;
    FS_Write(&count, sizeof(count), 1, file);
    for (int i = 0; i < count; ++i)
    {
        const demoentry_t entry{static_cast<int>(sizeof(demoheader)) + i, 1, 0, i + 1, ""};
        FS_Write(&entry, sizeof(entry), 1, file);
    }
    FS_Close(file);
}

static std::vector<demoentry_t> ReadEntries(const char* filename)
{
    FILE* file = FS_Open(filename, "rb");
    assert(file);
    FS_Read(&demoheader, sizeof(demoheader), 1, file);
    FS_Seek(file, demoheader.nDirectoryOffset, SEEK_SET);
    int count;
    FS_Read(&count, sizeof(count), 1, file);
    std::vector<demoentry_t> entries(count);
    FS_Read(entries.data(), sizeof(demoentry_t), entries.size(), file);
    FS_Close(file);
    return entries;
}

int main()
{
    WriteDemo("recording.dem");
    command = {"removedemo", "recording.dem", "4"};
    CL_RemoveDemo_f();
    assert(3 == ReadEntries("recording.dem").size());
    command[2] = "2";
    CL_RemoveDemo_f();
    auto entries = ReadEntries("recording.dem");
    assert(2 == entries.size() && 3 == entries[1].nCDTrack);

    WriteDemo("swap.dem");
    command = {"swapdemo", "swap.dem", "3", "1"};
    CL_SwapDemo_f();
    assert(1 == ReadEntries("swap.dem")[0].nCDTrack);
    command[3] = "2";
    CL_SwapDemo_f();
    assert(3 == ReadEntries("swap.dem")[1].nCDTrack);

    command = {"setdemoinfo", "swap.dem", "4", "TITLE", "Updated"};
    CL_SetDemoInfo_f();
    assert(0 == ReadEntries("swap.dem")[2].nFlags);
    command[2] = "3";
    CL_SetDemoInfo_f();
    entries = ReadEntries("swap.dem");
    assert(FDEMO_TITLE == entries[2].nFlags);
    assert(0 == std::strcmp("Updated", entries[2].szDescription));
}
'''


def main():
    production = [
        extract("cl_demo.cpp", "void CL_RemoveDemo_f( void )\n{"),
        extract("cl_demo.cpp", "void CL_SwapDemo_f( void )\n{"),
        extract("cl_demo.cpp", "void CL_SetDemoInfo_f( void )\n{"),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-demo-edit-commands-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
