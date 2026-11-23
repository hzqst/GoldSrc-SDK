"""Exercise Netchan_CopyFileFragments decompression error and allocation failure handling."""
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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef _WIN32
#define _stricmp strcasecmp
#include <strings.h>
#endif

using byte = unsigned char;
using qboolean = int;
constexpr qboolean FALSE = 0;
constexpr qboolean TRUE = 1;
constexpr int MAX_PATH = 260;
constexpr int FRAG_NORMAL_STREAM = 0;
constexpr int FRAG_FILE_STREAM = 1;
constexpr int MAX_STREAMS = 2;
constexpr int FRAGMENT_SIZE = 1400;
constexpr int BZ_OK = 0;
constexpr int BZ_DATA_ERROR = -4;
constexpr int BZ_OUTBUFF_FULL = -8;
constexpr std::uint16_t FSB_OVERFLOWED = 2;
using FileHandle_t = void*;

struct sizebuf_t {
    const char* buffername = "";
    std::uint16_t flags = 0;
    byte* data = nullptr;
    int maxsize = 0;
    int cursize = 0;
};

struct fragbuf_t {
    fragbuf_t* next = nullptr;
    int bufferid = 0;
    sizebuf_t frag_message;
    byte frag_message_buf[FRAGMENT_SIZE];
};

struct netchan_t {
    sizebuf_t message;
    fragbuf_t* incomingbufs[MAX_STREAMS] = {};
    qboolean incomingready[MAX_STREAMS] = {};
    char incomingfilename[MAX_PATH] = {};
    char* tempbuffer = nullptr;
    int tempbuffersize = 0;
};

static byte g_netMsgBuf[256];
static sizebuf_t net_message = { "net_message", 0, g_netMsgBuf, sizeof(g_netMsgBuf), 0 };
static int msg_readcount = 0;
static qboolean g_bIsDedicatedServer = FALSE;
static struct { float value = 1.0f; } cl_allow_download;

static int g_liveAllocs = 0;
static bool g_failNextMalloc = false;
static int g_bzResult = BZ_OK;
static int g_bzCalls = 0;
static std::string g_bzOutput;
static int g_openCalls = 0;
static int g_writeCalls = 0;
static std::string g_writtenData;

static void* Mem_Malloc(size_t n)
{
    if (g_failNextMalloc)
    {
        g_failNextMalloc = false;
        return nullptr;
    }
    ++g_liveAllocs;
    return std::malloc(n);
}
static void* Mem_ZeroMalloc(size_t n) { ++g_liveAllocs; return std::calloc(1, n); }
static void Mem_Free(void* p) { if (p) { --g_liveAllocs; std::free(p); } }

static void Con_Printf(const char*, ...) {}
static void Con_DPrintf(const char*, ...) {}
static void Sys_Printf(const char*, ...) {}
static void Q_strncpy(char* d, const char* s, int n) { std::strncpy(d, s, n); }
static int Q_strlen(const char* s) { return static_cast<int>(std::strlen(s)); }
static const char* Q_strstr(const char* a, const char* b) { return std::strstr(a, b); }
static void Q_memcpy(void* d, const void* s, int n) { std::memcpy(d, s, n); }
static void COM_FixSlashes(char*) {}
static bool Host_IsServerActive() { return false; }
static bool IsSafeFileToDownload(const char*) { return true; }
static bool FS_FileExists(const char*) { return false; }
static void FS_CreateDirHierarchy(const char*, const char*) {}
static FileHandle_t FS_OpenPathID(const char*, const char*, const char*)
{
    ++g_openCalls;
    return reinterpret_cast<FileHandle_t>(1);
}
static void FS_Write(const void* data, int size, int count, FileHandle_t)
{
    ++g_writeCalls;
    g_writtenData.assign(static_cast<const char*>(data), static_cast<size_t>(size * count));
}
static void FS_Close(FileHandle_t) {}

static void SZ_Clear(sizebuf_t* sb) { sb->cursize = 0; sb->flags &= ~FSB_OVERFLOWED; }
static void SZ_Write(sizebuf_t* sb, const void* src, int length)
{
    assert(sb->cursize + length <= sb->maxsize);
    std::memcpy(sb->data + sb->cursize, src, length);
    sb->cursize += length;
}
static void MSG_BeginReading() { msg_readcount = 0; }
static char* MSG_ReadString()
{
    static char out[256];
    int i = 0;
    while (msg_readcount < net_message.cursize && net_message.data[msg_readcount] != 0)
    {
        if (i < 255) out[i++] = static_cast<char>(net_message.data[msg_readcount]);
        ++msg_readcount;
    }
    if (msg_readcount < net_message.cursize) ++msg_readcount;
    out[i] = 0;
    return out;
}
static int MSG_ReadLong()
{
    unsigned v = 0;
    for (int i = 0; i < 4; ++i)
        v |= static_cast<unsigned>(net_message.data[msg_readcount++]) << (i * 8);
    return static_cast<int>(v);
}

static int BZ2_bzBuffToBuffDecompress(char* dest, unsigned int* destLen, char*, unsigned int, int, int)
{
    ++g_bzCalls;
    assert(dest != nullptr);
    assert(destLen != nullptr && *destLen > 0);
    if (g_bzResult != BZ_OK)
        return g_bzResult;
    assert(g_bzOutput.size() <= *destLen);
    std::memcpy(dest, g_bzOutput.data(), g_bzOutput.size());
    *destLen = static_cast<unsigned int>(g_bzOutput.size());
    return BZ_OK;
}
'''

TESTS = r'''
static void SetupCompressedFile(netchan_t& chan, const char* filename, unsigned int declaredSize)
{
    std::vector<byte> chunk;
    auto pushStr = [&](const char* s) {
        chunk.insert(chunk.end(), reinterpret_cast<const byte*>(s), reinterpret_cast<const byte*>(s) + std::strlen(s) + 1);
    };
    pushStr(filename);
    pushStr("bz2");
    for (int i = 0; i < 4; ++i)
        chunk.push_back(static_cast<byte>((declaredSize >> (i * 8)) & 0xFF));
    chunk.insert(chunk.end(), { 'B', 'Z', 'h', '9', 0x11, 0x22 });

    fragbuf_t* f = static_cast<fragbuf_t*>(Mem_ZeroMalloc(sizeof(fragbuf_t)));
    f->bufferid = (1 << 16) | 1;
    f->frag_message.data = f->frag_message_buf;
    f->frag_message.maxsize = FRAGMENT_SIZE;
    f->frag_message.cursize = static_cast<int>(chunk.size());
    std::memcpy(f->frag_message_buf, chunk.data(), chunk.size());

    chan.incomingbufs[FRAG_FILE_STREAM] = f;
    chan.incomingready[FRAG_FILE_STREAM] = TRUE;
}

static void ResetCounters()
{
    g_failNextMalloc = false;
    g_bzResult = BZ_OK;
    g_bzCalls = 0;
    g_bzOutput.clear();
    g_openCalls = 0;
    g_writeCalls = 0;
    g_writtenData.clear();
}

int main()
{
    // 1. Declared uncompressedSize == 0 is rejected without calling BZ2 or writing a file.
    {
        ResetCounters();
        netchan_t chan;
        SetupCompressedFile(chan, "maps/zero.bsp", 0);

        assert(FALSE == Netchan_CopyFileFragments(&chan));
        assert(0 == g_bzCalls);
        assert(0 == g_openCalls);
        assert(0 == g_writeCalls);
        assert(nullptr == chan.incomingbufs[FRAG_FILE_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_FILE_STREAM]);
        assert(0 == net_message.cursize);
        assert(0 == msg_readcount);
        assert(0 == g_liveAllocs);
    }

    // 2. Mem_Malloc failure for uncompressed buffer is rejected without calling BZ2 or writing.
    {
        ResetCounters();
        netchan_t chan;
        SetupCompressedFile(chan, "maps/oom.bsp", 1024);
        g_failNextMalloc = true;

        assert(FALSE == Netchan_CopyFileFragments(&chan));
        assert(0 == g_bzCalls);
        assert(0 == g_openCalls);
        assert(0 == g_writeCalls);
        assert(nullptr == chan.incomingbufs[FRAG_FILE_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_FILE_STREAM]);
        assert(0 == g_liveAllocs);
    }

    // 3. BZ2 decompression failure on normal download does not write corrupt file to disk.
    {
        ResetCounters();
        netchan_t chan;
        SetupCompressedFile(chan, "maps/corrupt.bsp", 64);
        g_bzResult = BZ_DATA_ERROR;

        assert(FALSE == Netchan_CopyFileFragments(&chan));
        assert(1 == g_bzCalls);
        assert(0 == g_openCalls);
        assert(0 == g_writeCalls);
        assert(nullptr == chan.incomingbufs[FRAG_FILE_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_FILE_STREAM]);
        assert(0 == g_liveAllocs);
    }

    // 4. BZ2 decompression failure on custom decal ('!') does not replace chan->tempbuffer.
    {
        ResetCounters();
        netchan_t chan;
        char* prior = static_cast<char*>(Mem_Malloc(4));
        std::memcpy(prior, "keep", 4);
        chan.tempbuffer = prior;
        chan.tempbuffersize = 4;

        SetupCompressedFile(chan, "!MD5abc", 64);
        g_bzResult = BZ_OUTBUFF_FULL;

        assert(FALSE == Netchan_CopyFileFragments(&chan));
        assert(1 == g_bzCalls);
        assert(prior == chan.tempbuffer);
        assert(4 == chan.tempbuffersize);
        assert(nullptr == chan.incomingbufs[FRAG_FILE_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_FILE_STREAM]);
        Mem_Free(chan.tempbuffer);
        assert(0 == g_liveAllocs);
    }

    // 5. BZ_OK decompression whose actual length is shorter than declared header length is rejected (file).
    {
        ResetCounters();
        netchan_t chan;
        SetupCompressedFile(chan, "maps/short.bsp", 32);
        g_bzResult = BZ_OK;
        g_bzOutput = "too-short";

        assert(FALSE == Netchan_CopyFileFragments(&chan));
        assert(1 == g_bzCalls);
        assert(0 == g_openCalls);
        assert(0 == g_writeCalls);
        assert(nullptr == chan.incomingbufs[FRAG_FILE_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_FILE_STREAM]);
        assert(0 == g_liveAllocs);
    }

    // 6. BZ_OK decompression whose actual length is shorter than declared header length is rejected ('!' decal).
    {
        ResetCounters();
        netchan_t chan;
        char* prior = static_cast<char*>(Mem_Malloc(4));
        std::memcpy(prior, "keep", 4);
        chan.tempbuffer = prior;
        chan.tempbuffersize = 4;

        SetupCompressedFile(chan, "!MD5short", 32);
        g_bzResult = BZ_OK;
        g_bzOutput = "too-short";

        assert(FALSE == Netchan_CopyFileFragments(&chan));
        assert(1 == g_bzCalls);
        assert(prior == chan.tempbuffer);
        assert(4 == chan.tempbuffersize);
        assert(nullptr == chan.incomingbufs[FRAG_FILE_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_FILE_STREAM]);
        Mem_Free(chan.tempbuffer);
        assert(0 == g_liveAllocs);
    }

    // 7. Valid BZ2 decompression whose actual length matches declared header length writes exact payload.
    {
        ResetCounters();
        netchan_t chan;
        g_bzOutput = "hello-uncompressed";
        SetupCompressedFile(chan, "maps/good.bsp", static_cast<unsigned int>(g_bzOutput.size()));
        g_bzResult = BZ_OK;

        assert(TRUE == Netchan_CopyFileFragments(&chan));
        assert(1 == g_bzCalls);
        assert(1 == g_openCalls);
        assert(1 == g_writeCalls);
        assert(std::string("hello-uncompressed") == g_writtenData);
        assert(nullptr == chan.incomingbufs[FRAG_FILE_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_FILE_STREAM]);
        assert(0 == g_liveAllocs);
    }
}
'''


def main():
    production = [
        extract('net_chan.cpp', 'void Netchan_FlushIncoming( netchan_t* chan, int stream )'),
        extract('net_chan.cpp', 'static void Netchan_DropCopiedFileFragments( netchan_t* chan )'),
        extract('net_chan.cpp', 'qboolean Netchan_CopyFileFragments( netchan_t* chan )'),
    ]

    with tempfile.TemporaryDirectory(prefix='goldsrc-netchan-decompress-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: Netchan_CopyFileFragments rejects invalid decompression and allocation failures')


if __name__ == '__main__':
    main()
