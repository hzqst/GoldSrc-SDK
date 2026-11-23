"""Exercise net_chan fragment list ordering, normal fragment overflow, file open failure, and overflow flush."""
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
constexpr std::uint16_t FSB_ALLOWOVERFLOW = 1;
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
static int g_bzDecompressCalls = 0;
static bool g_openSucceeds = false;

static void* Mem_Malloc(size_t n) { ++g_liveAllocs; return std::malloc(n); }
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
    return g_openSucceeds ? reinterpret_cast<FileHandle_t>(1) : nullptr;
}
static void FS_Write(const void*, int, int, FileHandle_t) {}
static void FS_Close(FileHandle_t) {}

static void SZ_Clear(sizebuf_t* sb) { sb->cursize = 0; sb->flags &= ~FSB_OVERFLOWED; }
static bool SZ_HasSpace(sizebuf_t* sb, int length) { return sb->cursize + length <= sb->maxsize; }
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

static int BZ2_bzBuffToBuffDecompress(char*, unsigned int*, char*, unsigned int, int, int)
{
    ++g_bzDecompressCalls;
    return 0;
}
'''

TESTS = r'''
static fragbuf_t* MakeFrag(int seqId, int total, const void* data, int len)
{
    fragbuf_t* b = static_cast<fragbuf_t*>(Mem_ZeroMalloc(sizeof(fragbuf_t)));
    b->bufferid = (seqId << 16) | total;
    b->frag_message.data = b->frag_message_buf;
    b->frag_message.maxsize = FRAGMENT_SIZE;
    b->frag_message.cursize = len;
    if (data && len > 0)
        std::memcpy(b->frag_message_buf, data, len);
    return b;
}

int main()
{
    // 1. Netchan_AddBufferToList: out-of-order insertion (3, 1, 2) and (1, 3, 2) keeps every node sorted.
    for (auto order : { std::vector<int>{3, 1, 2}, std::vector<int>{1, 3, 2}, std::vector<int>{2, 1, 3} })
    {
        fragbuf_t* head = nullptr;
        for (int id : order)
            Netchan_AddBufferToList(&head, MakeFrag(id, 3, nullptr, 0));

        std::vector<int> got;
        for (fragbuf_t* p = head; p;)
        {
            got.push_back(p->bufferid >> 16);
            fragbuf_t* next = p->next;
            Mem_Free(p);
            p = next;
        }
        assert((std::vector<int>{1, 2, 3}) == got);
        assert(0 == g_liveAllocs);
    }

    // 2. Netchan_CopyNormalFragments: oversized stream fails without decompressing stale BZ2 header.
    {
        netchan_t chan;
        const byte firstPayload[8] = { 'B', 'Z', '2', 0, 1, 2, 3, 4 };
        std::vector<byte> huge(300, 0x55);
        fragbuf_t* f1 = MakeFrag(1, 2, firstPayload, sizeof(firstPayload));
        fragbuf_t* f2 = MakeFrag(2, 2, huge.data(), static_cast<int>(huge.size()));
        f1->next = f2;
        chan.incomingbufs[FRAG_NORMAL_STREAM] = f1;
        chan.incomingready[FRAG_NORMAL_STREAM] = TRUE;

        g_bzDecompressCalls = 0;
        assert(FALSE == Netchan_CopyNormalFragments(&chan));
        assert(0 == g_bzDecompressCalls);
        assert(nullptr == chan.incomingbufs[FRAG_NORMAL_STREAM]);
        assert(FALSE == chan.incomingready[FRAG_NORMAL_STREAM]);
        assert(0 == g_liveAllocs);
    }

    // 3. Netchan_CopyFileFragments: FS_OpenPathID failure does NOT double-free fragments and does NOT leak buffer.
    {
        netchan_t chan;
        std::vector<byte> chunk;
        auto pushStr = [&](const char* s) {
            chunk.insert(chunk.end(), reinterpret_cast<const byte*>(s), reinterpret_cast<const byte*>(s) + std::strlen(s) + 1);
        };
        pushStr("maps/test.txt");
        pushStr("uncompressed");
        for (int i = 0; i < 4; ++i) chunk.push_back(i == 0 ? 4 : 0);
        chunk.insert(chunk.end(), {'t', 'e', 's', 't'});

        fragbuf_t* f1 = MakeFrag(1, 1, chunk.data(), static_cast<int>(chunk.size()));
        chan.incomingbufs[FRAG_FILE_STREAM] = f1;
        chan.incomingready[FRAG_FILE_STREAM] = TRUE;
        g_openSucceeds = false;

        assert(FALSE == Netchan_CopyFileFragments(&chan));
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
        extract('net_chan.cpp', 'qboolean Netchan_CopyNormalFragments( netchan_t* chan )'),
        extract('net_chan.cpp', 'void Netchan_AddBufferToList( fragbuf_t** pplist, fragbuf_t* pbuf )\n{'),
    ]
    # Also verify Netchan_Process flushes incoming stream on fragment overflow.
    net_chan_src = (ROOT / 'net_chan.cpp').read_text(encoding='utf-8')
    proc_start = net_chan_src.index('if ( pbuf->frag_message.flags & FSB_OVERFLOWED )')
    proc_block = net_chan_src[proc_start:net_chan_src.index('\n\t\t\t\t}', proc_start) + 6]
    assert 'Netchan_FlushIncoming( chan, i );' in proc_block, 'Netchan_Process must flush on overflow'

    with tempfile.TemporaryDirectory(prefix='goldsrc-netchan-frag-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text('\n'.join([STUBS, *production, TESTS]), encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: net_chan fragment reassembly list ordering, overflow handling, and file open failure')


if __name__ == '__main__':
    main()
