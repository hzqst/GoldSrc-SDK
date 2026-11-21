"""Check WAD directory bounds and packed mip and palette data."""
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
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
using byte = unsigned char;
using qboolean = bool;
using FileHandle_t = int;
constexpr int TEX_MAX_WADS = 128, WAD_LUMP_NAME_BYTES = 16, FILESYSTEM_SEEK_HEAD = 0;
constexpr int MIPLEVELS = 4;
struct miptex_t { char name[16]; unsigned width, height, offsets[MIPLEVELS]; };
struct wadinfo_t { char identification[4]; int numlumps, infotableofs; };
struct lumpinfo_t
{
    int filepos, disksize, size;
    char type, compression, pad1, pad2, name[WAD_LUMP_NAME_BYTES];
};
struct texlumpinfo_t { lumpinfo_t lump; int iTexFile; };
static FileHandle_t texfiles[TEX_MAX_WADS];
static int nTexFiles, nTexLumps;
static texlumpinfo_t* lumpinfo;
static std::vector<byte> wad;
static size_t cursor;
static FileHandle_t FS_Open(const char*, const char*) { cursor = 0; return 1; }
static void FS_Close(FileHandle_t) {}
static unsigned FS_Size(FileHandle_t) { return static_cast<unsigned>(wad.size()); }
static void FS_Seek(FileHandle_t, int offset, int) { cursor = offset; }
static int FS_Read(void* out, int count, int, FileHandle_t)
{
    if (count < 0 || cursor > wad.size() || static_cast<size_t>(count) > wad.size() - cursor) return 0;
    std::memcpy(out, wad.data() + cursor, count);
    cursor += count;
    return count;
}
#define Q_strncpy std::strncpy
#define Q_strcat std::strcat
#define Q_strstr std::strstr
#define Q_strncmp std::strncmp
static bool COM_FileBase_s(const char*, char* out, size_t size)
{
    return 0 < std::snprintf(out, size, "fixture");
}
static void COM_DefaultExtension(char*, const char*) {}
static void Con_DPrintf(const char*, ...) {}
static void Con_SafePrintf(const char*, ...) {}
static void Sys_Error(const char* message, ...) { throw std::runtime_error(message); }
static void* Mem_Realloc(void* memory, size_t size) { return std::realloc(memory, size); }
static void Mem_Free(void* memory) { std::free(memory); }
static unsigned LittleLong(unsigned value) { return value; }
static unsigned short LittleShort(unsigned short value) { return value; }
'''

TESTS = r'''
static void MakeWad(const std::vector<byte>& lumpBytes, int count, int directory, int lumpSize)
{
    const wadinfo_t header{{'W', 'A', 'D', '3'}, count, directory};
    lumpinfo_t entry{};
    entry.filepos = sizeof(header);
    entry.disksize = lumpSize;
    std::strcpy(entry.name, "brick");
    wad.resize(sizeof(header) + lumpBytes.size() + sizeof(entry));
    std::memcpy(wad.data(), &header, sizeof(header));
    std::memcpy(wad.data() + sizeof(header), lumpBytes.data(), lumpBytes.size());
    std::memcpy(wad.data() + sizeof(header) + lumpBytes.size(), &entry, sizeof(entry));
}

static bool InitWad()
{
    char name[] = "fixture";
    try { return TEX_InitFromWad(name); }
    catch (const std::runtime_error&) { TEX_CleanupWadInfo(); return false; }
}

int main()
{
    constexpr unsigned side = 16;
    constexpr unsigned mipBytes = 256 + 64 + 16 + 4;
    constexpr unsigned short colors = 256;
    miptex_t header{"brick", side, side, {40, 296, 360, 376}};
    std::vector<byte> bytes(sizeof(header) + mipBytes + sizeof(colors) + colors * 3);
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header) + mipBytes, &colors, sizeof(colors));
    assert(TEX_ValidateMiptex(bytes.data(), bytes.size()));
    assert(!TEX_ValidateMiptex(bytes.data(), sizeof(header) - 1));
    assert(!TEX_ValidateMiptex(bytes.data(), sizeof(header) + mipBytes - 1));
    assert(!TEX_ValidateMiptex(bytes.data(), bytes.size() - 1));

    const int directory = static_cast<int>(sizeof(wadinfo_t) + bytes.size());
    const int length = static_cast<int>(bytes.size());
    MakeWad(bytes, 1, directory, length);
    assert(InitWad());
    char name[] = "brick";
    std::vector<byte> loaded(bytes.size());
    assert(length == TEX_LoadLump(name, loaded.data(), length));
    assert(bytes == loaded);
    TEX_CleanupWadInfo();

    MakeWad(bytes, 2, directory, length);
    assert(!InitWad());
    MakeWad(bytes, -1, directory, length);
    assert(!InitWad());
    MakeWad(bytes, 1, directory, -1);
    assert(!InitWad());

    header.width = 0;
    std::memcpy(bytes.data(), &header, sizeof(header));
    assert(!TEX_ValidateMiptex(bytes.data(), bytes.size()));
}
'''


def main():
    production = [
        extract('textures.cpp', 'void CleanupName('),
        extract('textures.cpp', 'int lump_sorter('),
        extract('textures.cpp', 'void SafeRead('),
        extract('textures.cpp', 'void ForwardSlashes('),
        extract('textures.cpp', 'qboolean TEX_InitFromWad('),
        extract('textures.cpp', 'void TEX_CleanupWadInfo('),
        extract("textures.cpp", "static bool TEX_ValidateMiptex"),
        extract('textures.cpp', 'int TEX_LoadLump('),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-wad-miptex-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)


if __name__ == '__main__':
    main()
