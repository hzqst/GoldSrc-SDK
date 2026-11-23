"""Exercise BaseUISurface::AddCustomFontFile local-path tracking and Shutdown unregistration."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK/engine'


def extract(relpath, signature):
    source = (ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#ifndef _WIN32
#define _WIN32 1
#endif

constexpr int MAX_PATH = 260;
constexpr int FR_PRIVATE = 0x10;
constexpr int MAX_DEFAULT_CURSORS = 2;

struct SDL_Cursor {};
static void SDL_FreeCursor(SDL_Cursor*) {}

struct CUtlSymbol {
    std::string value;
    explicit CUtlSymbol(const char* s = "") : value(s ? s : "") {}
    const char* String() const { return value.c_str(); }
    bool operator==(const CUtlSymbol& o) const { return value == o.value; }
};

template <typename T>
struct CUtlVector {
    std::vector<T> items;
    int Count() const { return static_cast<int>(items.size()); }
    const T& operator[](int i) const { return items[i]; }
    void AddToTail(const T& v) { items.push_back(v); }
    void RemoveAll() { items.clear(); }
};

static std::map<std::string, std::string> g_paths;
static std::vector<std::string> g_addExCalls;
static std::vector<std::string> g_addFallbackCalls;
static std::vector<std::string> g_removeExCalls;
static std::vector<std::string> g_removeFallbackCalls;
static bool g_addExSucceeds = true;
static bool g_addFallbackSucceeds = true;
static bool g_removeExSucceeds = true;

static int AddFontResourceExA(const char* path, int, void*)
{
    g_addExCalls.emplace_back(path);
    return g_addExSucceeds ? 1 : 0;
}
static int AddFontResourceA(const char* path)
{
    g_addFallbackCalls.emplace_back(path);
    return g_addFallbackSucceeds ? 1 : 0;
}
static int RemoveFontResourceExA(const char* path, int, void*)
{
    g_removeExCalls.emplace_back(path);
    return g_removeExSucceeds ? 1 : 0;
}
static int RemoveFontResourceA(const char* path)
{
    g_removeFallbackCalls.emplace_back(path);
    return 1;
}

namespace vgui2 {
struct IFileSystem {
    const char* GetLocalPath(const char* rel, char* out, int outSize)
    {
        auto it = g_paths.find(rel ? rel : "");
        if (it == g_paths.end())
            return nullptr;
        std::strncpy(out, it->second.c_str(), outSize - 1);
        out[outSize - 1] = 0;
        return out;
    }
};
static IFileSystem g_fs;
static IFileSystem* filesystem() { return &g_fs; }
}

struct IHTMLChromeController { void Shutdown() {} };
static SDL_Cursor* staticDefaultCursor[MAX_DEFAULT_CURSORS] = {};
static CUtlVector<int> staticPopupList;

struct BaseUISurface {
    IHTMLChromeController* m_pChromeController = nullptr;
    CUtlVector<CUtlSymbol> m_CustomFontFileNames;
    bool AddCustomFontFile(const char* filename);
    void Shutdown();
};
'''

TESTS = r'''
int main()
{
    g_paths["resource/marlett.ttf"] = "C:/game/valve/resource/marlett.ttf";
    g_paths["resource/fallback.ttf"] = "C:/game/valve/resource/fallback.ttf";
    g_paths["resource/broken.ttf"] = "C:/game/valve/resource/broken.ttf";

    BaseUISurface surface;

    // 1. Missing file: GetLocalPath fails -> returns false and does not register or track.
    assert(!surface.AddCustomFontFile("resource/missing.ttf"));
    assert(0 == surface.m_CustomFontFileNames.Count());
    assert(g_addExCalls.empty());

    // 2. Normal registration: stores localPath, deduplicates second registration.
    g_addExSucceeds = true;
    assert(surface.AddCustomFontFile("resource/marlett.ttf"));
    assert(surface.AddCustomFontFile("resource/marlett.ttf"));
    assert(1 == surface.m_CustomFontFileNames.Count());
    assert(0 == std::strcmp("C:/game/valve/resource/marlett.ttf", surface.m_CustomFontFileNames[0].String()));
    assert(1 == g_addExCalls.size());

    // 3. Fallback registration when AddFontResourceExA fails but AddFontResourceA succeeds.
    g_addExSucceeds = false;
    g_addFallbackSucceeds = true;
    assert(surface.AddCustomFontFile("resource/fallback.ttf"));
    assert(2 == surface.m_CustomFontFileNames.Count());
    assert(1 == g_addFallbackCalls.size());

    // 4. Both fail -> not added to list.
    g_addFallbackSucceeds = false;
    assert(!surface.AddCustomFontFile("resource/broken.ttf"));
    assert(2 == surface.m_CustomFontFileNames.Count());

    // 5. Shutdown unregisters using local paths and falls back to RemoveFontResourceA when RemoveFontResourceExA returns 0.
    g_removeExSucceeds = false;
    surface.Shutdown();
    assert(0 == surface.m_CustomFontFileNames.Count());
    assert(2 == g_removeExCalls.size());
    assert("C:/game/valve/resource/marlett.ttf" == g_removeExCalls[0]);
    assert("C:/game/valve/resource/fallback.ttf" == g_removeExCalls[1]);
    assert(2 == g_removeFallbackCalls.size());
    assert("C:/game/valve/resource/marlett.ttf" == g_removeFallbackCalls[0]);
    assert("C:/game/valve/resource/fallback.ttf" == g_removeFallbackCalls[1]);
}
'''


def main():
    production = [
        extract('vgui2/BaseUISurface.cpp', 'bool BaseUISurface::AddCustomFontFile( const char* filename )'),
        extract('vgui2/BaseUISurface.cpp', 'void BaseUISurface::Shutdown()'),
    ]
    with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-custom-font-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: BaseUISurface::AddCustomFontFile tracks local paths and Shutdown unregisters them')


if __name__ == '__main__':
    main()
