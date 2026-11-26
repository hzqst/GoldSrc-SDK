"""Regression tests for engine save record, table, and header validation (#736).

Run: python -I tests/engine_save_restore_tests.py
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
#include <vector>

#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#define VectorClear(a) ((a)[0] = (a)[1] = (a)[2] = 0)
#define MAX_LIGHTSTYLES 64
#define MAX_LEVEL_CONNECTIONS 16
#define FENTTABLE_REMOVED 0x40000000
#define SAVEFILE_HEADER (('V' << 0) + ('A' << 8) + ('L' << 16) + ('V' << 24))
#define SAVEGAME_VERSION 0x0071

typedef float vec3_t[3];
typedef int qboolean;
typedef struct FileHandle_s* FileHandle_t;
typedef struct edict_s edict_t;

enum FIELDTYPE {
    FIELD_FLOAT = 0,
    FIELD_STRING,
    FIELD_ENTITY,
    FIELD_CLASSPTR,
    FIELD_EHANDLE,
    FIELD_EVARS,
    FIELD_EDICT,
    FIELD_VECTOR,
    FIELD_POSITION_VECTOR,
    FIELD_POINTER,
    FIELD_INTEGER,
    FIELD_FUNCTION,
    FIELD_BOOLEAN,
    FIELD_SHORT,
    FIELD_CHARACTER,
    FIELD_TIME,
    FIELD_MODELNAME,
    FIELD_SOUNDNAME,
    FIELD_TYPECOUNT,
};

struct TYPEDESCRIPTION {
    FIELDTYPE fieldType;
    const char* fieldName;
    int fieldOffset;
    short fieldSize;
    short flags;
};

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

struct GAME_HEADER {
    char mapName[32];
    char comment[80];
    int mapCount;
};

struct SAVELIGHTSTYLE {
    int index;
    char style[64];
};

struct MockFile {
    std::vector<unsigned char> data;
    size_t pos = 0;
    bool closed = false;
};

static MockFile* g_activeFile = nullptr;
static int g_hunkAllocs = 0;

static void* Q_memcpy(void* d, const void* s, size_t n) { return memcpy(d, s, n); }
static void* Q_memset(void* d, int v, size_t n) { return memset(d, v, n); }
static int Q_strcmp(const char* a, const char* b) { return strcmp(a, b); }
static int Q_strcasecmp(const char* a, const char* b) {
#ifdef _WIN32
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}
static size_t Q_strlen(const char* s) { return strlen(s); }
static void Q_strcpy(char* d, const char* s) { strcpy(d, s); }
static void Con_Printf(const char*, ...) {}
static void* Mem_Calloc(int num, size_t size) { return calloc((size_t)num, size); }
static void Mem_Free(void* p) { free(p); }
static void* Hunk_AllocName(int size, const char*) { ++g_hunkAllocs; return calloc(1, (size_t)size); }
static void* Hunk_Alloc(int size) { ++g_hunkAllocs; return calloc(1, (size_t)size); }

static void SaveExit(SAVERESTOREDATA* save) {
    if (!save) return;
    if (save->pTokens) Mem_Free(save->pTokens);
    Mem_Free(save);
}

static int FS_Read(void* out, int size, int count, FileHandle_t file) {
    MockFile* f = reinterpret_cast<MockFile*>(file);
    size_t total = (size_t)size * (size_t)count;
    size_t avail = f->pos < f->data.size() ? f->data.size() - f->pos : 0;
    size_t toRead = total < avail ? total : avail;
    if (toRead > 0) memcpy(out, f->data.data() + f->pos, toRead);
    f->pos += toRead;
    return (int)toRead;
}
static unsigned int FS_Size(FileHandle_t file) {
    MockFile* f = reinterpret_cast<MockFile*>(file);
    return (unsigned int)f->data.size();
}
static void FS_Close(FileHandle_t file) {
    MockFile* f = reinterpret_cast<MockFile*>(file);
    assert(!f->closed);
    f->closed = true;
}

static struct {
    char* lightstyles[MAX_LIGHTSTYLES] = {};
    int num_edicts = 1;
} sv;

static struct {
    void (*pfnSaveReadFields)(SAVERESTOREDATA*, const char*, void*, TYPEDESCRIPTION*, int);
    void (*pfnRestoreGlobalState)(SAVERESTOREDATA*);
} gEntityInterface;

#define DEFINE_FIELD(type, name, fieldtype) { fieldtype, #name, (int)offsetof(type, name), 1, 0 }
#define DEFINE_ARRAY(type, name, fieldtype, count) { fieldtype, #name, (int)offsetof(type, name), count, 0 }

static TYPEDESCRIPTION gGameHeaderDescription[] = {
    DEFINE_ARRAY(GAME_HEADER, mapName, FIELD_CHARACTER, 32),
    DEFINE_ARRAY(GAME_HEADER, comment, FIELD_CHARACTER, 80),
    DEFINE_FIELD(GAME_HEADER, mapCount, FIELD_INTEGER),
};

static TYPEDESCRIPTION gSaveHeaderDescription[] = {
    DEFINE_FIELD(SAVE_HEADER, skillLevel, FIELD_INTEGER),
    DEFINE_FIELD(SAVE_HEADER, entityCount, FIELD_INTEGER),
    DEFINE_FIELD(SAVE_HEADER, connectionCount, FIELD_INTEGER),
    DEFINE_FIELD(SAVE_HEADER, lightStyleCount, FIELD_INTEGER),
    DEFINE_FIELD(SAVE_HEADER, time, FIELD_TIME),
    DEFINE_ARRAY(SAVE_HEADER, mapName, FIELD_CHARACTER, 32),
    DEFINE_ARRAY(SAVE_HEADER, skyName, FIELD_CHARACTER, 32),
    DEFINE_FIELD(SAVE_HEADER, skyColor_r, FIELD_INTEGER),
    DEFINE_FIELD(SAVE_HEADER, skyColor_g, FIELD_INTEGER),
    DEFINE_FIELD(SAVE_HEADER, skyColor_b, FIELD_INTEGER),
    DEFINE_FIELD(SAVE_HEADER, skyVec_x, FIELD_FLOAT),
    DEFINE_FIELD(SAVE_HEADER, skyVec_y, FIELD_FLOAT),
    DEFINE_FIELD(SAVE_HEADER, skyVec_z, FIELD_FLOAT),
};

static TYPEDESCRIPTION gAdjacencyDescription[] = {
    DEFINE_ARRAY(LEVELLIST, mapName, FIELD_CHARACTER, 32),
    DEFINE_ARRAY(LEVELLIST, landmarkName, FIELD_CHARACTER, 32),
    DEFINE_FIELD(LEVELLIST, pentLandmark, FIELD_EDICT),
    DEFINE_FIELD(LEVELLIST, vecLandmarkOrigin, FIELD_VECTOR),
};

static TYPEDESCRIPTION gLightstyleDescription[] = {
    DEFINE_FIELD(SAVELIGHTSTYLE, index, FIELD_INTEGER),
    DEFINE_ARRAY(SAVELIGHTSTYLE, style, FIELD_CHARACTER, 64),
};

static TYPEDESCRIPTION gEntityTableDescription[] = {
    DEFINE_FIELD(ENTITYTABLE, id, FIELD_INTEGER),
    DEFINE_FIELD(ENTITYTABLE, location, FIELD_INTEGER),
    DEFINE_FIELD(ENTITYTABLE, size, FIELD_INTEGER),
    DEFINE_FIELD(ENTITYTABLE, flags, FIELD_INTEGER),
    DEFINE_FIELD(ENTITYTABLE, classname, FIELD_STRING),
};

static void MockReadFields(SAVERESTOREDATA* data, const char*, void* out, TYPEDESCRIPTION* fields, int fieldCount) {
    unsigned short hdrSize, hdrTok;
    memcpy(&hdrSize, data->pCurrentData, 2); data->pCurrentData += 2;
    memcpy(&hdrTok, data->pCurrentData, 2); data->pCurrentData += 2;
    int count;
    memcpy(&count, data->pCurrentData, 4); data->pCurrentData += 4;
    for (int i = 0; i < count; ++i) {
        unsigned short fsize, ftok;
        memcpy(&fsize, data->pCurrentData, 2); data->pCurrentData += 2;
        memcpy(&ftok, data->pCurrentData, 2); data->pCurrentData += 2;
        for (int j = 0; j < fieldCount; ++j) {
            if (!Q_strcasecmp(fields[j].fieldName, data->pTokens[ftok])) {
                if (fields[j].fieldType != FIELD_STRING)
                    memcpy((char*)out + fields[j].fieldOffset, data->pCurrentData, fsize);
                break;
            }
        }
        data->pCurrentData += fsize;
    }
    data->size = (int)(data->pCurrentData - data->pBaseData);
}

static void MockRestoreGlobalState(SAVERESTOREDATA*) {}

__EXTRACTED__

struct RecordBuilder {
    std::vector<std::string> tokens;
    std::vector<unsigned char> bytes;

    unsigned short tok(const char* name) {
        for (size_t i = 0; i < tokens.size(); ++i)
            if (tokens[i] == name) return (unsigned short)i;
        tokens.push_back(name);
        return (unsigned short)(tokens.size() - 1);
    }

    void writeU16(unsigned short v) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(&v);
        bytes.insert(bytes.end(), p, p + 2);
    }
    void writeI32(int v) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(&v);
        bytes.insert(bytes.end(), p, p + 4);
    }
    void writeField(const char* name, const void* val, unsigned short len) {
        writeU16(len);
        writeU16(tok(name));
        const unsigned char* p = reinterpret_cast<const unsigned char*>(val);
        bytes.insert(bytes.end(), p, p + len);
    }
    void beginRecord(const char* name, int fieldCount) {
        writeU16(4);
        writeU16(tok(name));
        writeI32(fieldCount);
    }
};

static std::vector<unsigned char> PackTokens(const std::vector<std::string>& tokens) {
    std::vector<unsigned char> out;
    for (const auto& t : tokens) {
        out.insert(out.end(), t.begin(), t.end());
        out.push_back(0);
    }
    return out;
}

int main() {
    gEntityInterface.pfnSaveReadFields = &MockReadFields;
    gEntityInterface.pfnRestoreGlobalState = &MockRestoreGlobalState;

    // 1. SaveRecordEnd null pTokens guard
    {
        SAVERESTOREDATA d = {};
        char buf[16] = {};
        d.pBaseData = buf;
        d.pCurrentData = buf;
        d.bufferSize = sizeof(buf);
        d.tokenCount = 1;
        d.pTokens = nullptr;
        assert(SaveRecordEnd(&d, "GameHeader", gGameHeaderDescription, ARRAYSIZE(gGameHeaderDescription)) == NULL);
    }

    // 2. SaveReadHeader_0 valid + short read + unterminated token table + bad GameHeader
    {
        RecordBuilder rb;
        GAME_HEADER hdr = {};
        strcpy(hdr.mapName, "c1a0");
        strcpy(hdr.comment, "Test Save");
        hdr.mapCount = 2;
        rb.beginRecord("GameHeader", 3);
        rb.writeField("mapName", hdr.mapName, 32);
        rb.writeField("comment", hdr.comment, 80);
        rb.writeField("mapCount", &hdr.mapCount, 4);
        auto tokBytes = PackTokens(rb.tokens);

        auto buildFile = [&](int size, int tokCount, int tokSize, const std::vector<unsigned char>& tb, const std::vector<unsigned char>& db) {
            MockFile f;
            auto putI32 = [&](int v) {
                const unsigned char* p = reinterpret_cast<const unsigned char*>(&v);
                f.data.insert(f.data.end(), p, p + 4);
            };
            // SaveReadHeader already consumed tag & version (2 ints) before calling SaveReadHeader_0,
            // so place 2 dummy ints at start and advance pos to 8 so FS_Size matches real layout.
            putI32(0);
            putI32(SAVEGAME_VERSION);
            putI32(size);
            putI32(tokCount);
            putI32(tokSize);
            f.data.insert(f.data.end(), tb.begin(), tb.end());
            f.data.insert(f.data.end(), db.begin(), db.end());
            f.pos = 8;
            return f;
        };

        // Valid
        {
            MockFile f = buildFile((int)rb.bytes.size(), (int)rb.tokens.size(), (int)tokBytes.size(), tokBytes, rb.bytes);
            GAME_HEADER out = {};
            assert(SaveReadHeader_0(reinterpret_cast<FileHandle_t>(&f), &out, 1) == 1);
            assert(strcmp(out.mapName, "c1a0") == 0 && out.mapCount == 2);
            assert(!f.closed);
        }
        // Unterminated token table
        {
            auto badTok = tokBytes;
            badTok.back() = 'X';
            MockFile f = buildFile((int)rb.bytes.size(), (int)rb.tokens.size(), (int)badTok.size(), badTok, rb.bytes);
            GAME_HEADER out = {};
            assert(SaveReadHeader_0(reinterpret_cast<FileHandle_t>(&f), &out, 0) == 0);
            assert(f.closed);
        }
        // Truncated data
        {
            auto shortData = rb.bytes;
            shortData.pop_back();
            MockFile f = buildFile((int)rb.bytes.size(), (int)rb.tokens.size(), (int)tokBytes.size(), tokBytes, shortData);
            GAME_HEADER out = {};
            assert(SaveReadHeader_0(reinterpret_cast<FileHandle_t>(&f), &out, 0) == 0);
            assert(f.closed);
        }
        // Negative mapCount rejected
        {
            RecordBuilder badRb;
            GAME_HEADER badHdr = hdr;
            badHdr.mapCount = -1;
            badRb.beginRecord("GameHeader", 3);
            badRb.writeField("mapName", badHdr.mapName, 32);
            badRb.writeField("comment", badHdr.comment, 80);
            badRb.writeField("mapCount", &badHdr.mapCount, 4);
            auto badTok = PackTokens(badRb.tokens);
            MockFile f = buildFile((int)badRb.bytes.size(), (int)badRb.tokens.size(), (int)badTok.size(), badTok, badRb.bytes);
            GAME_HEADER out = {};
            assert(SaveReadHeader_0(reinterpret_cast<FileHandle_t>(&f), &out, 0) == 0);
            assert(f.closed);
        }
    }

    // 3. ParseSaveTables rejects negative entity id and preserves sv.lightstyles on failure
    {
        RecordBuilder rb;
        int negId = -1, loc = 200, sz = 0;
        rb.beginRecord("ETABLE", 3);
        rb.writeField("id", &negId, 4);
        rb.writeField("location", &loc, 4);
        rb.writeField("size", &sz, 4);

        SAVE_HEADER sh = {};
        strcpy(sh.mapName, "c1a0");
        strcpy(sh.skyName, "desert");
        sh.lightStyleCount = 1;
        rb.beginRecord("Save Header", 3);
        rb.writeField("mapName", sh.mapName, 32);
        rb.writeField("skyName", sh.skyName, 32);
        rb.writeField("lightStyleCount", &sh.lightStyleCount, 4);

        SAVELIGHTSTYLE ls = {};
        ls.index = 0;
        strcpy(ls.style, "m");
        rb.beginRecord("LIGHTSTYLE", 2);
        rb.writeField("index", &ls.index, 4);
        rb.writeField("style", ls.style, 64);

        std::vector<char*> tokPtrs;
        for (auto& s : rb.tokens) tokPtrs.push_back(&s[0]);

        ENTITYTABLE table[1] = {};
        SAVERESTOREDATA sd = {};
        sd.pBaseData = reinterpret_cast<char*>(rb.bytes.data());
        sd.pCurrentData = sd.pBaseData;
        sd.bufferSize = (int)rb.bytes.size();
        sd.tokenCount = (int)tokPtrs.size();
        sd.pTokens = tokPtrs.data();
        sd.tableCount = 1;
        sd.pTable = table;
        strcpy(sd.szCurrentMapName, "c1a0");

        char sentinel[] = "existing";
        sv.lightstyles[0] = sentinel;
        SAVE_HEADER parsed = {};
        assert(!ParseSaveTables(&sd, &parsed, 1));
        assert(sv.lightstyles[0] == sentinel);
        assert(g_hunkAllocs == 0);
    }

    puts("PASS: engine SaveRecordEnd, SaveReadHeader_0, and ParseSaveTables validation");
    return 0;
}
'''


def main():
    source = HOST_CMD.read_text(encoding='latin-1')
    extracted = '\n\n'.join([
        extract_function(source, 'static const char* SaveRecordEnd('),
        extract_function(source, 'static bool ReadSaveRecord('),
        extract_function(source, 'static bool ParseSaveTables('),
        extract_function(source, 'int SaveReadHeader_0('),
    ])
    code = HARNESS.replace('__EXTRACTED__', extracted)
    with tempfile.TemporaryDirectory(prefix='goldsrc-engine-save-restore-') as folder:
        folder = Path(folder)
        test_cpp = folder / 'test.cpp'
        test_cpp.write_text(code, encoding='utf-8')
        exe = compile_program([test_cpp], folder / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(exe)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
