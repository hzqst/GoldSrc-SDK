"""Regression tests for engine on-demand save buffer growth and safe file replacement (#749).

Run: python -I tests/engine_save_grow_tests.py
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1]
HOST_CMD = ROOT / 'GoldSrc-SDK/engine/host_cmd.cpp'


def extract_function(source, signature):
    pos = 0
    while True:
        start = source.index(signature, pos)
        close_paren = source.index(')', start)
        tail = source[close_paren + 1:].lstrip()
        if tail.startswith('{'):
            brace = source.index('{', close_paren)
            break
        pos = close_paren + 1
    depth = 0
    for idx in range(brace, len(source)):
        ch = source[idx]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return source[start:idx + 1]
    raise RuntimeError(f'Unclosed function: {signature}')


HARNESS = r'''
#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_PATH 260
#define MAX_LEVEL_CONNECTIONS 16

typedef float vec3_t[3];
typedef struct FileHandle_s* FileHandle_t;
typedef struct edict_s { int id; } edict_t;

static vec3_t vec3_origin = { 0.0f, 0.0f, 0.0f };
#define VectorCopy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])

struct LEVELLIST {
    char mapName[32];
    char landmarkName[32];
    edict_t* pentLandmark;
    vec3_t vecLandmarkOrigin;
};

struct ENTITYTABLE {
    int id;
    edict_t* pent;
    int location;
    int size;
    int flags;
    int classname;
};

struct SAVERESTOREDATA {
    char* pBaseData;
    char* pCurrentData;
    int size;
    int bufferSize;
    int tokenSize;
    int tokenCount;
    char** pTokens;
    int currentIndex;
    int tableCount;
    int connectionCount;
    ENTITYTABLE* pTable;
    LEVELLIST levelList[MAX_LEVEL_CONNECTIONS];
    int fUseLandmark;
    char szLandmarkName[20];
    vec3_t vecLandmarkOffset;
    float time;
    char szCurrentMapName[32];
};

struct TYPEDESCRIPTION { int dummy; };

static struct {
    edict_t edicts[4] = {};
    int num_edicts = 4;
} sv;

static struct {
    float time = 12.5f;
    SAVERESTOREDATA* pSaveData = NULL;
} gGlobalVariables;

static struct {
    void (*pfnSaveWriteFields)(SAVERESTOREDATA*, const char*, void*, TYPEDESCRIPTION*, int) = NULL;
} gEntityInterface;

static void* Mem_Calloc(int num, size_t size) { return calloc((size_t)num, size); }
static void* Mem_Realloc(void* p, size_t size) { return realloc(p, size); }
static void Mem_Free(void* p) { free(p); }
static void* Q_memcpy(void* d, const void* s, size_t n) { return memcpy(d, s, n); }
static void* Q_memset(void* d, int v, size_t n) { return memset(d, v, n); }
static size_t Q_strlen(const char* s) { return strlen(s); }
static int Q_strcasecmp(const char* a, const char* b) {
#ifdef _WIN32
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}
static void Con_Printf(const char*, ...) {}
static void COM_FixSlashes(char* path) {
#ifdef _WIN32
    for (char* p = path; *p; ++p) if (*p == '/') *p = '\\';
#else
    (void)path;
#endif
}

static void SaveExit(SAVERESTOREDATA* save) {
    if (!save) return;
    if (save->pTokens) Mem_Free(save->pTokens);
    Mem_Free(save);
    gGlobalVariables.pSaveData = NULL;
}

static std::string g_mockLocalPath;
static bool g_mockExistsInGameConfig = false;
static FileHandle_t FS_OpenPathID(const char*, const char*, const char*) {
    return g_mockExistsInGameConfig ? reinterpret_cast<FileHandle_t>(1) : NULL;
}
static void FS_Close(FileHandle_t) {}
static const char* FS_GetLocalPath(const char*, char* out, int outSize) {
    if (g_mockLocalPath.empty()) return NULL;
    snprintf(out, (size_t)outSize, "%s", g_mockLocalPath.c_str());
    return out;
}

static constexpr int SAVE_INITIAL_BUFFER_SIZE = 0x80000;
static constexpr int SAVE_MIN_HEADROOM        = 0x40000;
static constexpr int SAVE_MAX_BUFFER_SIZE     = 0x10000000;
static constexpr int SAVE_TOKEN_COUNT         = 0xFFF;

__EXTRACTED__

static void WriteChunk(SAVERESTOREDATA* data, int bytes, unsigned char fill) {
    if (data->size + bytes >= data->bufferSize) {
        // Mimics CSave::BufferData overflow behavior: pins size to bufferSize.
        data->size = data->bufferSize;
        data->pCurrentData = data->pBaseData + data->bufferSize;
        return;
    }
    memset(data->pCurrentData, fill, (size_t)bytes);
    data->pCurrentData += bytes;
    data->size += bytes;
}

int main() {
    // 1. Small write (< 512 KiB) stays at initial 512 KiB buffer
    {
        SAVERESTOREDATA* sd = SaveInit(0);
        assert(sd != NULL && sd->bufferSize == SAVE_INITIAL_BUFFER_SIZE);
        int largest = 0;
        assert(SaveWriteGrowing(sd, &largest, [&]() { WriteChunk(sd, 1024, 0x2A); }));
        assert(sd->size == 1024 && largest == 1024);
        assert(sd->bufferSize == SAVE_INITIAL_BUFFER_SIZE);
        sd->pTokens[0] = const_cast<char*>("GameHeader");
        const char* tokData = SaveWriteTokens(sd);
        assert(tokData != NULL && sd->tokenSize >= sd->tokenCount);
        SaveWriteExit(sd);
    }

    // 2. Large single-entity write (> 512 KiB) triggers growth & retry without corrupting prior data
    {
        SAVERESTOREDATA* sd = SaveInit(0);
        assert(sd != NULL);
        int largest = 0;
        assert(SaveWriteGrowing(sd, &largest, [&]() { WriteChunk(sd, 4096, 0x11); }));
        int calls = 0;
        assert(SaveWriteGrowing(sd, &largest, [&]() {
            ++calls;
            WriteChunk(sd, 900 * 1024, 0x22);
        }));
        assert(calls >= 2);
        assert(sd->size == 4096 + 900 * 1024);
        assert(sd->bufferSize >= 1024 * 1024);
        for (int i = 0; i < 4096; ++i) assert((unsigned char)sd->pBaseData[i] == 0x11);
        for (int i = 4096; i < sd->size; ++i) assert((unsigned char)sd->pBaseData[i] == 0x22);
        assert(SaveWriteTokens(sd) != NULL);
        SaveWriteExit(sd);
    }

    // 3. Overflow above SAVE_MAX_BUFFER_SIZE fails cleanly
    {
        SAVERESTOREDATA* sd = SaveInit(0);
        assert(sd != NULL);
        assert(!SaveGrowData(sd, (int64_t)SAVE_MAX_BUFFER_SIZE + 1));
        SaveWriteExit(sd);
    }

    // 4. ReplaceSaveFile rejects truncated / mismatched suffix paths
    {
        g_mockExistsInGameConfig = true;
        g_mockLocalPath = "C:\\game\\valve\\SAVE\\truncated_path";
        assert(!ReplaceSaveFile("SAVE\\slot.sav.tmp", "SAVE\\slot.sav"));
        g_mockLocalPath = "C:\\game\\valve\\not_SAVE\\slot.sav.tmp";
        assert(!ReplaceSaveFile("SAVE\\slot.sav.tmp", "SAVE\\slot.sav"));
        g_mockExistsInGameConfig = false;
        g_mockLocalPath = "C:\\game\\valve\\SAVE\\slot.sav.tmp";
        assert(!ReplaceSaveFile("SAVE\\slot.sav.tmp", "SAVE\\slot.sav"));
    }

    puts("PASS: engine on-demand save buffer growth, token reservation, and ReplaceSaveFile guards");
    return 0;
}
'''


def main():
    source = HOST_CMD.read_text(encoding='latin-1')
    extracted = '\n\n'.join([
        extract_function(source, 'static void SaveWriteExit('),
        extract_function(source, 'static SAVERESTOREDATA* SaveInit('),
        extract_function(source, 'static bool SaveBufferValid('),
        extract_function(source, 'static bool SaveGrowData('),
        extract_function(source, 'template <typename Write>\nstatic bool SaveWriteGrowing('),
        extract_function(source, 'static bool SaveReserveTokens('),
        extract_function(source, 'static const char* SaveWriteTokens('),
        extract_function(source, 'static bool SaveFileExistsInGameConfig('),
        extract_function(source, 'static bool ReplaceSaveFile('),
    ])
    code = HARNESS.replace('__EXTRACTED__', extracted)
    with tempfile.TemporaryDirectory(prefix='goldsrc-engine-save-grow-') as folder:
        folder = Path(folder)
        test_cpp = folder / 'test.cpp'
        test_cpp.write_text(code, encoding='utf-8')
        exe = compile_program([test_cpp], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(exe)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
