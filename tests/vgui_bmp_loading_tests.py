"""Exercise VGui_LoadBMP row-pitch alignment, bounds validation, and HW/SW caller failure paths."""
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
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef _WIN32
#define _snprintf snprintf
#else
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#endif

#define ASSERT_AT(cond, line) assert(cond)

using byte = unsigned char;

#pragma pack(push, 1)
struct BITMAPFILEHEADER {
    std::uint16_t bfType;
    std::uint32_t bfSize;
    std::uint16_t bfReserved1;
    std::uint16_t bfReserved2;
    std::uint32_t bfOffBits;
};
struct BITMAPINFOHEADER {
    std::uint32_t biSize;
    std::int32_t  biWidth;
    std::int32_t  biHeight;
    std::uint16_t biPlanes;
    std::uint16_t biBitCount;
    std::uint32_t biCompression;
    std::uint32_t biSizeImage;
    std::int32_t  biXPelsPerMeter;
    std::int32_t  biYPelsPerMeter;
    std::uint32_t biClrUsed;
    std::uint32_t biClrImportant;
};
struct RGBQUAD {
    byte rgbBlue;
    byte rgbGreen;
    byte rgbRed;
    byte rgbReserved;
};
#pragma pack(pop)

struct MemFile {
    std::vector<byte> data;
    size_t pos = 0;
    bool closed = false;
};
using FileHandle_t = MemFile*;

static MemFile* g_nextBmpFile = nullptr;
static int g_perfWarnCalls = 0;
static struct { float value = 1.0f; } fs_perf_warnings;

static unsigned int FS_Size(FileHandle_t file) { return static_cast<unsigned int>(file->data.size()); }
static int FS_Read(void* dest, int size, int count, FileHandle_t file)
{
    const size_t wanted = static_cast<size_t>(size) * static_cast<size_t>(count);
    const size_t avail = file->pos < file->data.size() ? file->data.size() - file->pos : 0;
    const size_t take = wanted < avail ? wanted : avail;
    if (take > 0)
    {
        std::memcpy(dest, file->data.data() + file->pos, take);
        file->pos += take;
    }
    return static_cast<int>(take);
}
static void FS_Close(FileHandle_t file) { file->closed = true; }
static FileHandle_t FS_Open(const char*, const char*) { return g_nextBmpFile; }
static bool LoadTGA(const char*, byte*, int, int*, int*) { return false; }
static bool IsPowerOfTwo(int v) { return v > 0 && (v & (v - 1)) == 0; }
static void Con_DPrintf(const char*, ...) { ++g_perfWarnCalls; }
'''

CALLER_HARNESS = r'''
namespace hw_surface {
struct Texture { int id = 0; int w = 0; int h = 0; };
static Texture g_tex;
static bool g_hasTex = false;
static int g_uploadCalls = 0;
static int g_bindCalls = 0;
static int g_lastW = -1;
static int g_lastH = -1;

static Texture* staticGetTextureById(int id) { return (g_hasTex && g_tex.id == id) ? &g_tex : nullptr; }

class EngineSurface {
public:
    void drawSetTexture(int) { ++g_bindCalls; }
    void drawSetTextureRGBA(int id, const char*, int wide, int tall, int, int)
    {
        ++g_uploadCalls;
        g_lastW = wide;
        g_lastH = tall;
        g_tex = { id, wide, tall };
        g_hasTex = true;
    }
    void drawSetTextureFile(int id, const char* filename, int hardwareFilter, bool forceReload);
};
'''

SW_HARNESS = r'''
namespace sw_surface {
struct vgui_texture_t { int id = 0; int wide = 0; int tall = 0; };
struct TextureStore {
    vgui_texture_t tex;
    bool hasTex = false;
    vgui_texture_t* GetTexture(int id) { return (hasTex && tex.id == id) ? &tex : nullptr; }
};
struct RenderPlat {
    TextureStore* textures = nullptr;
    vgui_texture_t* current = nullptr;
};

static int g_uploadCalls = 0;
static int g_lastW = -1;
static int g_lastH = -1;

class EngineSurface {
public:
    RenderPlat* _renderPlat = nullptr;
    void drawSetTextureRGBA(int id, const char*, int wide, int tall, int, int)
    {
        ++g_uploadCalls;
        g_lastW = wide;
        g_lastH = tall;
        _renderPlat->textures->tex = { id, wide, tall };
        _renderPlat->textures->hasTex = true;
    }
    void drawSetTextureFile(int id, const char* filename, int hardwareFilter, bool forceReload);
};
'''

TESTS = r'''
static std::vector<byte> MakeBMP8(int width, int height, int paletteEntries, const std::vector<byte>& topDownIndices)
{
    const int pitch = (width + 3) & ~3;
    BITMAPFILEHEADER bmfh = {};
    BITMAPINFOHEADER bmih = {};
    bmfh.bfType = 0x4D42;
    bmfh.bfOffBits = static_cast<std::uint32_t>(sizeof(bmfh) + sizeof(bmih) + paletteEntries * sizeof(RGBQUAD));
    bmfh.bfSize = static_cast<std::uint32_t>(bmfh.bfOffBits + pitch * height);

    bmih.biSize = sizeof(bmih);
    bmih.biWidth = width;
    bmih.biHeight = height;
    bmih.biPlanes = 1;
    bmih.biBitCount = 8;
    bmih.biCompression = 0;
    bmih.biClrUsed = static_cast<std::uint32_t>(paletteEntries);

    std::vector<byte> out(bmfh.bfSize, 0xAA);
    std::memcpy(out.data(), &bmfh, sizeof(bmfh));
    std::memcpy(out.data() + sizeof(bmfh), &bmih, sizeof(bmih));
    for (int i = 0; i < paletteEntries; ++i)
    {
        RGBQUAD q = { static_cast<byte>(i * 3), static_cast<byte>(i * 2), static_cast<byte>(i), 0 };
        std::memcpy(out.data() + sizeof(bmfh) + sizeof(bmih) + i * sizeof(RGBQUAD), &q, sizeof(q));
    }
    byte* pixels = out.data() + bmfh.bfOffBits;
    for (int y = 0; y < height; ++y)
    {
        byte* row = pixels + (height - y - 1) * pitch;
        for (int x = 0; x < width; ++x)
            row[x] = topDownIndices[y * width + x];
    }
    return out;
}

int main()
{
    // 1. Test all width mod 4 cases: 1, 2, 3, 4, 5 (widths 1, 3, 5 failed under pitch += (pitch + 4) % 4)
    for (int w : {1, 2, 3, 4, 5})
    {
        const int h = 2;
        std::vector<byte> indices;
        for (int i = 0; i < w * h; ++i)
            indices.push_back(static_cast<byte>(i + 1));

        MemFile file{MakeBMP8(w, h, 16, indices)};
        std::vector<byte> rgba(w * h * 4, 0);
        int outW = 0x5555, outH = 0x6666;
        assert(1 == VGui_LoadBMP(&file, rgba.data(), static_cast<int>(rgba.size()), &outW, &outH));
        assert(file.closed);
        assert(w == outW && h == outH);
        for (int i = 0; i < w * h; ++i)
        {
            const byte idx = indices[i];
            assert(idx == rgba[i * 4 + 0]);
            assert(static_cast<byte>(idx * 2) == rgba[i * 4 + 1]);
            assert(static_cast<byte>(idx * 3) == rgba[i * 4 + 2]);
            assert(255 == rgba[i * 4 + 3]);
        }
    }

    // 2. Rejected BMPs reset *width/*height to 0 even when callers pass dirty non-zero values.
    {
        MemFile file{MakeBMP8(3, 2, 16, {1, 2, 3, 4, 5, 6})};
        byte smallBuf[8] = {};
        int outW = 999, outH = 888;
        assert(0 == VGui_LoadBMP(&file, smallBuf, sizeof(smallBuf), &outW, &outH));
        assert(file.closed);
        assert(0 == outW && 0 == outH);
    }
    {
        auto bmp = MakeBMP8(3, 2, 16, {1, 2, 3, 4, 5, 6});
        bmp.pop_back();
        MemFile file{bmp};
        byte rgba[3 * 2 * 4] = {};
        int outW = 777, outH = 666;
        assert(0 == VGui_LoadBMP(&file, rgba, sizeof(rgba), &outW, &outH));
        assert(file.closed);
        assert(0 == outW && 0 == outH);
    }
    {
        auto bmp = MakeBMP8(2, 2, 4, {1, 2, 3, 0});
        BITMAPINFOHEADER bmih;
        std::memcpy(&bmih, bmp.data() + sizeof(BITMAPFILEHEADER), sizeof(bmih));
        bmih.biClrUsed = 300;
        std::memcpy(bmp.data() + sizeof(BITMAPFILEHEADER), &bmih, sizeof(bmih));
        MemFile file{bmp};
        byte rgba[16] = {};
        int outW = 555, outH = 444;
        assert(0 == VGui_LoadBMP(&file, rgba, sizeof(rgba), &outW, &outH));
        assert(file.closed);
        assert(0 == outW && 0 == outH);
    }

    // 3. HW EngineSurface::drawSetTextureFile does NOT upload or warn when TGA is missing and BMP fails validation.
    {
        auto badBmp = MakeBMP8(3, 2, 16, {1, 2, 3, 4, 5, 6});
        badBmp.pop_back();
        MemFile file{badBmp};
        g_nextBmpFile = &file;
        g_perfWarnCalls = 0;
        hw_surface::g_hasTex = false;
        hw_surface::g_uploadCalls = 0;
        hw_surface::g_bindCalls = 0;

        hw_surface::EngineSurface surface;
        surface.drawSetTextureFile(1, "gfx/bad", 1, true);
        assert(file.closed);
        assert(0 == hw_surface::g_uploadCalls);
        assert(0 == hw_surface::g_bindCalls);
        assert(0 == g_perfWarnCalls);

        // Missing both TGA and BMP also must not warn about uninitialized dimensions.
        g_nextBmpFile = nullptr;
        surface.drawSetTextureFile(2, "gfx/missing", 1, true);
        assert(0 == hw_surface::g_uploadCalls);
        assert(0 == g_perfWarnCalls);

        // Valid non-power-of-2 BMP succeeds, uploads, binds, and emits perf warning.
        MemFile goodFile{MakeBMP8(3, 2, 16, {1, 2, 3, 4, 5, 6})};
        g_nextBmpFile = &goodFile;
        surface.drawSetTextureFile(3, "gfx/good", 1, true);
        assert(goodFile.closed);
        assert(1 == hw_surface::g_uploadCalls);
        assert(3 == hw_surface::g_lastW && 2 == hw_surface::g_lastH);
        assert(1 == hw_surface::g_bindCalls);
        assert(1 == g_perfWarnCalls);
    }

    // 4. SW EngineSurface::drawSetTextureFile does NOT upload when TGA is missing and BMP fails validation.
    {
        auto badBmp = MakeBMP8(3, 2, 16, {1, 2, 3, 4, 5, 6});
        badBmp.pop_back();
        MemFile file{badBmp};
        g_nextBmpFile = &file;
        sw_surface::g_uploadCalls = 0;

        sw_surface::TextureStore store;
        sw_surface::RenderPlat plat{ &store, nullptr };
        sw_surface::EngineSurface surface;
        surface._renderPlat = &plat;

        surface.drawSetTextureFile(1, "gfx/bad", 1, true);
        assert(file.closed);
        assert(0 == sw_surface::g_uploadCalls);
        assert(nullptr == plat.current);

        // Valid BMP succeeds and uploads.
        MemFile goodFile{MakeBMP8(5, 2, 16, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10})};
        g_nextBmpFile = &goodFile;
        surface.drawSetTextureFile(2, "gfx/good", 1, true);
        assert(goodFile.closed);
        assert(1 == sw_surface::g_uploadCalls);
        assert(5 == sw_surface::g_lastW && 2 == sw_surface::g_lastH);
        assert(plat.current != nullptr && plat.current->id == 2);
    }
}
'''


def main():
    bmp_loader = extract('vgui_int.cpp', 'int VGui_LoadBMP( FileHandle_t file, byte* buffer, int bufferSize, int* width, int* height )')
    hw_caller = extract('vgui_EngineSurfaceHW.cpp', 'void EngineSurface::drawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload )')
    sw_caller = extract('vgui_EngineSurfaceSW.cpp', 'void EngineSurface::drawSetTextureFile( int id, const char* filename, int hardwareFilter, bool forceReload )')
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui-bmp-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text(
            '\n'.join([
                STUBS,
                bmp_loader,
                CALLER_HARNESS,
                hw_caller,
                '} // namespace hw_surface',
                SW_HARNESS,
                sw_caller,
                '} // namespace sw_surface',
                TESTS,
            ]),
            encoding='utf-8',
        )
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: VGui_LoadBMP aligns row pitch, validates bounds, and HW/SW callers guard failed loads')


if __name__ == '__main__':
    main()
