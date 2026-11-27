"""Regression tests for engine save/restore failure recovery, DirectoryExtract, EntityPatchWrite,
LoadAdjacentEntities, CL_Save, Host_Changelevel2_f, and Host_Map (ported from HLND2T PR #1691).

Run: python -I tests/engine_save_recovery_tests.py
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
#include <map>
#include <string>
#include <vector>

#ifndef _WIN32
#define _snprintf snprintf
#endif

#define MAX_PATH 260
#define MAX_QPATH 64
#define MAX_LIGHTSTYLES 64
#define MAX_LEVEL_CONNECTIONS 16
#define FILE_COPY_SIZE 4096
#define DECAL_NAME_BYTES 64
#define FENTTABLE_REMOVED 0x40000000
#define FENTTABLE_PLAYER  0x80000000
#define SAVEFILE_HEADER (('V' << 0) + ('A' << 8) + ('L' << 16) + ('V' << 24))
#define SAVEGAME_VERSION 0x0071
#define FILESYSTEM_SEEK_CURRENT 1
#define FALSE 0
#define TRUE 1
#define DLL_ACTIVE 1
#define DLL_TRANS 2
#define ca_dedicated 0
#define ca_active 1
#define src_command 0

typedef float vec_t;
typedef float vec3_t[3];
typedef int qboolean;
typedef unsigned char byte;
typedef struct FileHandle_s* FileHandle_t;
typedef struct edict_s { int id; } edict_t;

#define VectorSubtract(a, b, c) ((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])

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

struct SAVE_HEADER {
    int saveId;
    int version;
    int skillLevel;
    int entityCount;
    int connectionCount;
    int lightStyleCount;
    float time;
    char mapName[32];
    char skyName[32];
    int skyColor_r;
    int skyColor_g;
    int skyColor_b;
    float skyVec_x;
    float skyVec_y;
    float skyVec_z;
};

struct DECALLIST {
    char* name;
    short entityIndex;
    byte depth;
    byte flags;
    vec3_t position;
};

struct UserMsg {
    UserMsg* next;
};

// Mock virtual filesystem
struct VFile {
    std::string path;
    std::vector<unsigned char> data;
    size_t pos = 0;
    bool writable = false;
    bool ok = true;
    bool closed = false;
};

static std::map<std::string, std::vector<unsigned char>> g_fs;
static std::vector<std::string> g_removedFiles;
static std::string g_failWritePath;
static std::string g_failFlushPath;
static std::string g_failOpenPath;
static bool g_failReplace = false;

static void NormalizePath(char* p) {
    for (; *p; ++p) if (*p == '\\') *p = '/';
}
static std::string NormStr(const char* s) {
    std::string r = s ? s : "";
    for (char& c : r) if (c == '\\') c = '/';
    return r;
}

static const char* Host_SaveGameDirectory() { return "SAVE/"; }
static void COM_FixSlashes(char* p) { NormalizePath(p); }
static char* COM_LastFileExtension(char* in) {
    char* dot = strrchr(in, '.');
    return dot ? dot + 1 : const_cast<char*>("");
}
static void* Q_memset(void* d, int v, size_t n) { return memset(d, v, n); }
static int Q_strcmp(const char* a, const char* b) { return strcmp(a, b); }
static const char* Q_strstr(const char* a, const char* b) { return strstr(a, b); }
static char* Q_strncpy(char* d, const char* s, size_t n) { return strncpy(d, s, n); }
static char* Q_strcpy(char* d, const char* s) { return strcpy(d, s); }
static void Con_Printf(const char*, ...) {}
static void Sys_Error(const char* fmt, ...) { (void)fmt; assert(false && "Sys_Error called"); }

static FileHandle_t FS_OpenPathID(const char* path, const char* mode, const char* pathID) {
    (void)pathID;
    std::string norm = NormStr(path);
    if (!g_failOpenPath.empty() && norm == g_failOpenPath)
        return NULL;
    VFile* f = new VFile();
    f->path = norm;
    if (mode && mode[0] == 'w') {
        f->writable = true;
    } else {
        auto it = g_fs.find(norm);
        if (it == g_fs.end()) {
            delete f;
            return NULL;
        }
        f->data = it->second;
    }
    return reinterpret_cast<FileHandle_t>(f);
}

static int FS_Read(void* out, int size, int count, FileHandle_t file) {
    VFile* f = reinterpret_cast<VFile*>(file);
    size_t total = (size_t)size * (size_t)count;
    size_t avail = f->pos < f->data.size() ? f->data.size() - f->pos : 0;
    size_t n = total < avail ? total : avail;
    if (n > 0) memcpy(out, f->data.data() + f->pos, n);
    f->pos += n;
    return (int)n;
}

static int FS_Write(const void* in, int size, int count, FileHandle_t file) {
    VFile* f = reinterpret_cast<VFile*>(file);
    if (!g_failWritePath.empty() && f->path == g_failWritePath)
        return size > 0 ? size - 1 : -1;
    size_t total = (size_t)size * (size_t)count;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(in);
    f->data.insert(f->data.end(), p, p + total);
    f->pos = f->data.size();
    return (int)total;
}

static void FS_Flush(FileHandle_t file) {
    VFile* f = reinterpret_cast<VFile*>(file);
    if (!g_failFlushPath.empty() && f->path == g_failFlushPath)
        f->ok = false;
}

static int FS_IsOk(FileHandle_t file) {
    return reinterpret_cast<VFile*>(file)->ok ? 1 : 0;
}

static unsigned int FS_Size(FileHandle_t file) {
    return (unsigned int)reinterpret_cast<VFile*>(file)->data.size();
}

static unsigned int FS_Tell(FileHandle_t file) {
    return (unsigned int)reinterpret_cast<VFile*>(file)->pos;
}

static void FS_Seek(FileHandle_t file, int offset, int seekType) {
    VFile* f = reinterpret_cast<VFile*>(file);
    if (seekType == FILESYSTEM_SEEK_CURRENT)
        f->pos = (size_t)((int64_t)f->pos + offset);
}

static void FS_Close(FileHandle_t file) {
    VFile* f = reinterpret_cast<VFile*>(file);
    assert(!f->closed);
    f->closed = true;
    if (f->writable)
        g_fs[f->path] = f->data;
    delete f;
}

static void FS_RemoveFile(const char* path, const char* pathID) {
    (void)pathID;
    std::string norm = NormStr(path);
    g_removedFiles.push_back(norm);
    g_fs.erase(norm);
}

static bool ReplaceSaveFile(const char* temporary, const char* name) {
    if (g_failReplace)
        return false;
    std::string src = NormStr(temporary);
    std::string dst = NormStr(name);
    auto it = g_fs.find(src);
    if (it == g_fs.end())
        return false;
    g_fs[dst] = it->second;
    g_fs.erase(it);
    return true;
}

// Engine state stubs for LoadAdjacentEntities / Host_Changelevel2_f / Host_Map / CL_Save
static struct {
    qboolean active = TRUE;
    qboolean paused = FALSE;
    qboolean loadgame = FALSE;
    double time = 10.0;
    char name[MAX_QPATH] = "c1a0a";
} sv;

static struct {
    int maxclients = 1;
    int serverflags = 0;
} svs;

static struct {
    int state = ca_active;
    qboolean demoplayback = FALSE;
    char mapstring[64] = {};
    char spawnparms[256] = {};
} cls;

static struct {
    float value = 0.0f;
} sv_newunit;

static struct {
    float time = 0.0f;
    SAVERESTOREDATA* pSaveData = NULL;
} gGlobalVariables;

static int giActive = DLL_ACTIVE;
static int key_dest = 0;
static const int key_game = 1;
static const char* com_empty_string = "";
static UserMsg* sv_gpUserMsgs = NULL;
static UserMsg* sv_gpNewUserMsgs = NULL;

static int g_loadingPlaqueBegins = 0;
static int g_loadingPlaqueEnds = 0;
static int g_shutdownServerCalls = 0;
static int g_clearMemoryCalls = 0;
static int g_clearSaveDirCalls = 0;
static int g_activateServerCalls = 0;
static std::vector<std::string> g_cbufCommands;
static std::vector<std::string> g_execCommands;
static std::vector<std::string> g_cmdArgs;

static int Cmd_Argc() { return (int)g_cmdArgs.size(); }
static const char* Cmd_Argv(int i) { return (i >= 0 && i < (int)g_cmdArgs.size()) ? g_cmdArgs[i].c_str() : ""; }
static void SCR_BeginLoadingPlaque(qboolean) { ++g_loadingPlaqueBegins; }
static void SCR_EndLoadingPlaque() { ++g_loadingPlaqueEnds; }
static void S_StopAllSounds(qboolean) {}
static void Host_ShutdownServer(qboolean) { ++g_shutdownServerCalls; }
static void Host_ClearMemory(qboolean) { ++g_clearMemoryCalls; }
static void Host_ClearSaveDirectory() { ++g_clearSaveDirCalls; }
static void Host_ClearGameState() {}
static void SV_InactivateClients() {}
static void SV_ServerShutdown() {}
static void FS_LogLevelLoadStarted(const char*) {}
static void ContinueLoadingProgressBar(const char*, int, float) {}
static void Steam_UpdateRichPresence() {}
static void Cbuf_AddText(const char* cmd) { g_cbufCommands.push_back(cmd); }
static void Cmd_ExecuteString(const char* cmd, int) { g_execCommands.push_back(cmd); }

static bool g_spawnServerResult = true;
static bool g_loadGamestateResult = false;
static bool g_activateLeavesActive = true;
static qboolean SV_SpawnServer(qboolean, const char*, const char*) { return g_spawnServerResult ? TRUE : FALSE; }
static void SV_LoadEntities() {}
static void SV_ActivateServer(int) {
    ++g_activateServerCalls;
    sv.active = g_activateLeavesActive ? TRUE : FALSE;
}
static int LoadGamestate(const char*, int) { return g_loadGamestateResult ? 1 : 0; }

static std::vector<std::string> g_adjacentMaps;
static std::map<std::string, int> g_movedPerMap;
static std::vector<std::string> g_patchedMaps;
static std::map<std::string, std::string> g_loadedMapLinkTarget;
static int g_decalCount = 0;
static char g_decalName[64] = "{shot1";

static int DecalListCreate(DECALLIST* list) {
    for (int i = 0; i < g_decalCount; ++i) {
        list[i].name = g_decalName;
        list[i].entityIndex = (short)i;
        list[i].depth = 1;
        list[i].flags = 0;
        list[i].position[0] = 1.0f;
        list[i].position[1] = 2.0f;
        list[i].position[2] = 3.0f;
    }
    return g_decalCount;
}

static struct {
    void (*pfnParmsChangeLevel)() = []() {
        SAVERESTOREDATA* d = gGlobalVariables.pSaveData;
        d->connectionCount = (int)g_adjacentMaps.size();
        for (int i = 0; i < d->connectionCount; ++i) {
            strncpy(d->levelList[i].mapName, g_adjacentMaps[i].c_str(), 31);
            d->levelList[i].mapName[31] = '\0';
            strncpy(d->levelList[i].landmarkName, "lm1", 31);
            d->levelList[i].landmarkName[31] = '\0';
        }
    };
} gEntityInterface;

static SAVERESTOREDATA* LoadSaveData(const char* level) {
    SAVERESTOREDATA* sd = new SAVERESTOREDATA();
    memset(sd, 0, sizeof(*sd));
    strncpy(sd->szCurrentMapName, level, 31);
    sd->tableCount = 2;
    sd->pTable = new ENTITYTABLE[2]();
    sd->pTable[0].flags = FENTTABLE_REMOVED;
    auto it = g_loadedMapLinkTarget.find(level);
    if (it != g_loadedMapLinkTarget.end()) {
        sd->connectionCount = 1;
        strncpy(sd->levelList[0].mapName, it->second.c_str(), 31);
    }
    return sd;
}
static bool ParseSaveTables(SAVERESTOREDATA*, SAVE_HEADER*, int) { return true; }
static void SaveExit(SAVERESTOREDATA* sd) {
    if (!sd) return;
    delete[] sd->pTable;
    delete sd;
    gGlobalVariables.pSaveData = NULL;
}
static void SaveWriteExit(SAVERESTOREDATA* sd) { SaveExit(sd); }
static void LandmarkOrigin(SAVERESTOREDATA*, vec_t* out, const char*) { out[0] = out[1] = out[2] = 0.0f; }
static int CreateEntityList(SAVERESTOREDATA* sd, int) {
    auto it = g_movedPerMap.find(sd->szCurrentMapName);
    return it != g_movedPerMap.end() ? it->second : 0;
}

static bool g_saveGamestateSucceeds = true;
static SAVERESTOREDATA* SaveGamestate() {
    if (!g_saveGamestateSucceeds) return NULL;
    SAVERESTOREDATA* sd = new SAVERESTOREDATA();
    memset(sd, 0, sizeof(*sd));
    return sd;
}

__EXTRACTED__

static void ResetMocks() {
    g_fs.clear();
    g_removedFiles.clear();
    g_failWritePath.clear();
    g_failFlushPath.clear();
    g_failOpenPath.clear();
    g_failReplace = false;
    g_loadingPlaqueBegins = 0;
    g_loadingPlaqueEnds = 0;
    g_shutdownServerCalls = 0;
    g_clearMemoryCalls = 0;
    g_clearSaveDirCalls = 0;
    g_activateServerCalls = 0;
    g_cbufCommands.clear();
    g_execCommands.clear();
    g_cmdArgs.clear();
    g_adjacentMaps.clear();
    g_movedPerMap.clear();
    g_loadedMapLinkTarget.clear();
    g_decalCount = 0;
    g_saveGamestateSucceeds = true;
    g_spawnServerResult = true;
    g_loadGamestateResult = false;
    g_activateLeavesActive = true;
    sv.active = TRUE;
    sv.paused = FALSE;
    sv.loadgame = FALSE;
    strcpy(sv.name, "c1a0b");
    svs.maxclients = 1;
    cls.state = ca_active;
    cls.demoplayback = FALSE;
    sv_newunit.value = 0.0f;
    giActive = DLL_ACTIVE;
}

int main() {
    // 1. DirectoryExtract rejects negative count, short read, negative size, oversized file, and write failure
    {
        ResetMocks();
        assert(!DirectoryExtract(NULL, 1));
        VFile empty;
        assert(!DirectoryExtract(reinterpret_cast<FileHandle_t>(&empty), -1));

        // Build valid 1-file archive
        auto makeArchive = [](const char* fname, int declaredSize, const std::vector<unsigned char>& payload) {
            VFile* f = new VFile();
            f->data.resize(MAX_PATH, 0);
            strncpy(reinterpret_cast<char*>(f->data.data()), fname, MAX_PATH - 1);
            const unsigned char* szBytes = reinterpret_cast<const unsigned char*>(&declaredSize);
            f->data.insert(f->data.end(), szBytes, szBytes + sizeof(int));
            f->data.insert(f->data.end(), payload.begin(), payload.end());
            return f;
        };

        // Valid extraction
        {
            VFile* f = makeArchive("c1a0.HL1", 4, {1, 2, 3, 4});
            assert(DirectoryExtract(reinterpret_cast<FileHandle_t>(f), 1));
            assert(g_fs["SAVE/c1a0.HL1"] == std::vector<unsigned char>({1, 2, 3, 4}));
            delete f;
        }
        // Negative fileSize rejected
        {
            VFile* f = makeArchive("c1a0.HL1", -5, {1, 2, 3, 4});
            assert(!DirectoryExtract(reinterpret_cast<FileHandle_t>(f), 1));
            delete f;
        }
        // Declared fileSize exceeds remaining bytes rejected
        {
            VFile* f = makeArchive("c1a0.HL1", 100, {1, 2, 3, 4});
            assert(!DirectoryExtract(reinterpret_cast<FileHandle_t>(f), 1));
            delete f;
        }
        // Write failure removes partial output file
        {
            ResetMocks();
            g_failWritePath = "SAVE/c1a0.HL1";
            VFile* f = makeArchive("c1a0.HL1", 4, {1, 2, 3, 4});
            assert(!DirectoryExtract(reinterpret_cast<FileHandle_t>(f), 1));
            assert(g_fs.find("SAVE/c1a0.HL1") == g_fs.end());
            delete f;
        }
        // Flush failure removes partial output file
        {
            ResetMocks();
            g_failFlushPath = "SAVE/c1a0.HL1";
            VFile* f = makeArchive("c1a0.HL1", 4, {1, 2, 3, 4});
            assert(!DirectoryExtract(reinterpret_cast<FileHandle_t>(f), 1));
            assert(g_fs.find("SAVE/c1a0.HL1") == g_fs.end());
            delete f;
        }
    }

    // 2. EntityPatchRead null guard & EntityPatchWrite atomic replacement preserving old .HL3 on failure
    {
        ResetMocks();
        EntityPatchRead(NULL, "c1a0");
        SAVERESTOREDATA emptySd = {};
        EntityPatchRead(&emptySd, "c1a0");

        g_fs["SAVE/c1a0.HL3"] = {0xAA, 0xBB};
        ENTITYTABLE tbl[2] = {};
        tbl[1].flags = FENTTABLE_REMOVED;
        SAVERESTOREDATA sd = {};
        sd.tableCount = 2;
        sd.pTable = tbl;

        // Write failure keeps original .HL3 and cleans up .HL3.tmp
        g_failWritePath = "SAVE/c1a0.HL3.tmp";
        assert(!EntityPatchWrite(&sd, "c1a0"));
        assert(g_fs["SAVE/c1a0.HL3"] == std::vector<unsigned char>({0xAA, 0xBB}));
        assert(g_fs.find("SAVE/c1a0.HL3.tmp") == g_fs.end());

        // Replace failure keeps original .HL3 and cleans up .HL3.tmp
        g_failWritePath.clear();
        g_failReplace = true;
        assert(!EntityPatchWrite(&sd, "c1a0"));
        assert(g_fs["SAVE/c1a0.HL3"] == std::vector<unsigned char>({0xAA, 0xBB}));
        assert(g_fs.find("SAVE/c1a0.HL3.tmp") == g_fs.end());

        // Success replaces .HL3 atomically
        g_failReplace = false;
        assert(EntityPatchWrite(&sd, "c1a0"));
        assert(g_fs["SAVE/c1a0.HL3"].size() == 8);
        assert(g_fs.find("SAVE/c1a0.HL3.tmp") == g_fs.end());
    }

    // 3. CL_Save checks write/flush failure and removes partial .HL2
    {
        ResetMocks();
        g_decalCount = 2;
        assert(CL_Save("SAVE/c1a0.HL2"));
        assert(g_fs.find("SAVE/c1a0.HL2") != g_fs.end());

        g_fs.clear();
        g_failWritePath = "SAVE/c1a0.HL2";
        assert(!CL_Save("SAVE/c1a0.HL2"));
        assert(g_fs.find("SAVE/c1a0.HL2") == g_fs.end());

        g_failWritePath.clear();
        g_failFlushPath = "SAVE/c1a0.HL2";
        assert(!CL_Save("SAVE/c1a0.HL2"));
        assert(g_fs.find("SAVE/c1a0.HL2") == g_fs.end());
    }

    // 4. LoadAdjacentEntities resets movedCount per adjacent map
    {
        ResetMocks();
        strcpy(sv.name, "c1a0b");
        g_adjacentMaps = {"mapA", "mapB"};
        g_loadedMapLinkTarget["mapA"] = "c1a0b";
        // mapB does NOT link to c1a0b and is not oldLevel -> flags == 0
        g_movedPerMap["mapA"] = 1;
        g_movedPerMap["mapB"] = 0;

        assert(LoadAdjacentEntities("mapA", "lm1"));
        assert(g_fs.find("SAVE/mapA.HL3") != g_fs.end());
        assert(g_fs.find("SAVE/mapB.HL3") == g_fs.end());
    }

    // 5. Host_Changelevel2_f: invalid args must not leave giActive == DLL_TRANS
    {
        ResetMocks();
        giActive = DLL_ACTIVE;
        g_cmdArgs = {"changelevel2"}; // missing map arg
        Host_Changelevel2_f();
        assert(giActive == DLL_ACTIVE);

        g_cmdArgs = {"changelevel2", "c1a0b"};
        svs.maxclients = 4; // multiplayer rejected
        Host_Changelevel2_f();
        assert(giActive == DLL_ACTIVE);
    }

    // 6. Host_Changelevel2_f: LoadAdjacentEntities failure aborts cleanly without clearing save dir or activating server
    {
        ResetMocks();
        g_cmdArgs = {"changelevel2", "c1a0b", "lm1"};
        strcpy(sv.name, "c1a0b");
        sv_newunit.value = 1.0f;
        g_adjacentMaps = {"c1a0b"};
        g_loadedMapLinkTarget["c1a0b"] = "c1a0b";
        g_movedPerMap["c1a0b"] = 1;
        g_failWritePath = "SAVE/c1a0b.HL3.tmp";

        Host_Changelevel2_f();
        assert(g_clearSaveDirCalls == 0);
        assert(g_activateServerCalls == 0);
        assert(g_shutdownServerCalls == 1);
        assert(g_clearMemoryCalls == 1);
        assert(giActive == DLL_ACTIVE);
        assert(g_loadingPlaqueEnds == 1);
        assert(!g_cbufCommands.empty() && g_cbufCommands.back() == "\ndisconnect\n");
    }

    // 7. Host_Map in loadGame mode returns false and ends loading plaque when SV_ActivateServer leaves !sv.active
    {
        ResetMocks();
        g_spawnServerResult = true;
        g_loadGamestateResult = true;
        g_activateLeavesActive = false;
        char mapstr[] = "map c1a0\n";
        char mapname[] = "c1a0";
        assert(!Host_Map(FALSE, mapstr, mapname, TRUE));
        assert(g_loadingPlaqueEnds == 1);
        assert(g_execCommands.empty());
    }

    puts("PASS: engine save/restore failure recovery, DirectoryExtract, EntityPatchWrite, LoadAdjacentEntities, CL_Save, Host_Changelevel2_f, and Host_Map");
    return 0;
}
'''


def main():
    source = HOST_CMD.read_text(encoding='latin-1')
    extracted = '\n\n'.join([
        extract_function(source, 'bool FileCopy('),
        extract_function(source, 'void EntityPatchRead('),
        extract_function(source, 'static bool Host_Map('),
        extract_function(source, 'bool EntityPatchWrite('),
        extract_function(source, 'bool LoadAdjacentEntities('),
        extract_function(source, 'bool CL_Save('),
        extract_function(source, 'void Host_Changelevel2_f('),
        extract_function(source, 'bool DirectoryExtract('),
    ])
    code = HARNESS.replace('__EXTRACTED__', extracted)
    with tempfile.TemporaryDirectory(prefix='goldsrc-engine-save-recovery-') as folder:
        folder = Path(folder)
        test_cpp = folder / 'test.cpp'
        test_cpp.write_text(code, encoding='utf-8')
        exe = compile_program([test_cpp], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(exe)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
