"""Regression tests for CMiniMem::ProcessAll ordering (Think/Die/reclaim before visibility enqueue and Draw).

Run: python -I tests/particleman_processall_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract_function(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    float& operator[](int i) { return i == 0 ? x : (i == 1 ? y : z); }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};

struct cl_entity_t {
    Vector origin;
};

static float g_clientTime = 1.0f;
static float g_flOldTime = 0.0f;
static cl_entity_t g_localPlayer;

static bool IsGamePaused() {
    return g_clientTime == g_flOldTime;
}

struct EngineFuncs {
    float (*GetClientTime)();
    cl_entity_t* (*GetLocalPlayer)();
    void (*Con_DPrintf)(const char*, ...);
};

static float StubGetClientTime() { return g_clientTime; }
static cl_entity_t* StubGetLocalPlayer() { return &g_localPlayer; }
static void StubConDPrintf(const char*, ...) {}
static EngineFuncs gEngfuncs = {StubGetClientTime, StubGetLocalPlayer, StubConDPrintf};

struct CCoreTriangleEffect {
    float m_flDieTime = 0.0f;
    Vector m_vOrigin;
    float m_flPlayerDistance = 0.0f;

    virtual ~CCoreTriangleEffect() = default;
    virtual void Think(float time) = 0;
    virtual bool CheckVisibility() = 0;
    virtual void Draw() = 0;
    virtual void Die() = 0;

    float GetPlayerDistance() const { return m_flPlayerDistance; }
    void SetPlayerDistance(float d) { m_flPlayerDistance = d; }
};

struct visibleparticles_t {
    CCoreTriangleEffect* pVisibleParticle;
};

class MemoryBlock {
    char* m_pData;
public:
    explicit MemoryBlock(long lBlockSize) : next(nullptr), prev(nullptr) {
        m_pData = new char[lBlockSize];
    }
    ~MemoryBlock() { delete[] m_pData; }
    char* Memory() { return m_pData; }
    MemoryBlock* next;
    MemoryBlock* prev;
};

class MemList {
public:
    MemList() : m_pHead(nullptr) {}
    ~MemList() { Reset(); }
    void Push(MemoryBlock* newItem) {
        if (!m_pHead) {
            m_pHead = newItem;
            newItem->next = nullptr;
            newItem->prev = nullptr;
            return;
        }
        MemoryBlock* temp = m_pHead;
        m_pHead = newItem;
        m_pHead->next = temp;
        m_pHead->prev = nullptr;
        temp->prev = m_pHead;
    }
    MemoryBlock* Front() { return m_pHead; }
    MemoryBlock* Pop() {
        if (!m_pHead) return nullptr;
        MemoryBlock* temp = m_pHead;
        m_pHead = m_pHead->next;
        if (m_pHead) m_pHead->prev = nullptr;
        temp->next = nullptr;
        temp->prev = nullptr;
        return temp;
    }
    void Delete(MemoryBlock* pItem) {
        if (m_pHead == pItem) {
            MemoryBlock* temp = m_pHead;
            m_pHead = m_pHead->next;
            if (m_pHead) m_pHead->prev = nullptr;
            temp->next = nullptr;
            temp->prev = nullptr;
            return;
        }
        MemoryBlock* prev = pItem->prev;
        MemoryBlock* next = pItem->next;
        if (prev) prev->next = next;
        if (next) next->prev = prev;
        pItem->next = nullptr;
        pItem->prev = nullptr;
    }
    void Reset() {
        while (m_pHead) Delete(m_pHead);
    }
private:
    MemoryBlock* m_pHead;
};

static MemList m_FreeMem;
static MemList m_ActiveMem;

class CMiniMem {
public:
    static std::vector<MemoryBlock*> m_vecMemoryPool;
    static long m_lMemoryBlockSize;
    static long m_lMaxBlocks;
    static CMiniMem* _instance;

    int m_iTotalParticles = 0;
    int m_iParticlesDrawn = 0;
    visibleparticles_t* m_pVisibleParticles = nullptr;

    CMiniMem(long maxBlocks, long blockSize) {
        m_lMaxBlocks = maxBlocks;
        m_lMemoryBlockSize = blockSize;
        for (long i = 0; i < m_lMaxBlocks; ++i) {
            MemoryBlock* b = new MemoryBlock(m_lMemoryBlockSize);
            m_vecMemoryPool.push_back(b);
            m_FreeMem.Push(b);
        }
        m_pVisibleParticles = new visibleparticles_t[m_lMaxBlocks];
    }

    ~CMiniMem() {
        m_ActiveMem.Reset();
        m_FreeMem.Reset();
        for (MemoryBlock* b : m_vecMemoryPool)
            delete b;
        m_vecMemoryPool.clear();
        delete[] m_pVisibleParticles;
    }

    static CMiniMem* Instance() { return _instance; }

    char* newBlock() {
        MemoryBlock* b = m_FreeMem.Pop();
        if (!b) return nullptr;
        m_ActiveMem.Push(b);
        return b->Memory();
    }

    void deleteBlock(MemoryBlock* block) {
        m_ActiveMem.Delete(block);
        m_FreeMem.Push(block);
    }

    void Reset() {
        m_ActiveMem.Reset();
        m_FreeMem.Reset();
        for (MemoryBlock* b : m_vecMemoryPool)
            m_FreeMem.Push(b);
    }

    void IncreaseParticlesDrawn() { m_iParticlesDrawn++; }
    void ProcessAll();
};

std::vector<MemoryBlock*> CMiniMem::m_vecMemoryPool;
long CMiniMem::m_lMemoryBlockSize = 0;
long CMiniMem::m_lMaxBlocks = 0;
CMiniMem* CMiniMem::_instance = nullptr;
'''

TESTS = r'''
static std::vector<int> g_drawnIds;

struct TrackingParticle : CCoreTriangleEffect {
    int id = 0;
    bool visible = true;
    bool dieInThink = false;
    bool spawnReplacementInThink = false;
    int thinkCount = 0;
    int visCount = 0;
    int dieCount = 0;
    int drawCount = 0;
    bool drewAfterDie = false;
    int* externalDieLog = nullptr;
    int* externalDrawLog = nullptr;

    void Think(float time) override {
        ++thinkCount;
        if (dieInThink) {
            m_flDieTime = time;
        }
        if (spawnReplacementInThink) {
            char* mem = CMiniMem::Instance()->newBlock();
            assert(mem != nullptr);
            TrackingParticle* replacement = new (mem) TrackingParticle();
            replacement->id = 999;
            replacement->visible = true;
            replacement->m_flDieTime = time + 10.0f;
            replacement->m_vOrigin = Vector(5.0f, 0.0f, 0.0f);
        }
    }

    bool CheckVisibility() override {
        ++visCount;
        return visible;
    }

    void Draw() override {
        ++drawCount;
        if (dieCount > 0)
            drewAfterDie = true;
        if (externalDrawLog)
            ++(*externalDrawLog);
        g_drawnIds.push_back(id);
    }

    void Die() override {
        ++dieCount;
        if (externalDieLog)
            ++(*externalDieLog);
    }
};

template <typename T>
static T* SpawnInPool() {
    char* mem = CMiniMem::Instance()->newBlock();
    assert(mem != nullptr);
    return new (mem) T();
}

int main() {
    CMiniMem mem(4, static_cast<long>(sizeof(TrackingParticle)));
    CMiniMem::_instance = &mem;
    g_localPlayer.origin = Vector(0.0f, 0.0f, 0.0f);

    // 1. A particle that expires on this frame (already past dieTime or killed in Think) must call Die() and never Draw().
    g_clientTime = 5.0f;
    g_flOldTime = 4.0f;
    g_drawnIds.clear();

    int expiredDieCount = 0, expiredDrawCount = 0;
    int thinkDieCount = 0, thinkDrawCount = 0;

    TrackingParticle* alreadyExpired = SpawnInPool<TrackingParticle>();
    alreadyExpired->id = 1;
    alreadyExpired->m_flDieTime = 4.5f;
    alreadyExpired->externalDieLog = &expiredDieCount;
    alreadyExpired->externalDrawLog = &expiredDrawCount;

    TrackingParticle* diesInThink = SpawnInPool<TrackingParticle>();
    diesInThink->id = 2;
    diesInThink->m_flDieTime = 10.0f;
    diesInThink->dieInThink = true;
    diesInThink->externalDieLog = &thinkDieCount;
    diesInThink->externalDrawLog = &thinkDrawCount;

    TrackingParticle* aliveNear = SpawnInPool<TrackingParticle>();
    aliveNear->id = 3;
    aliveNear->m_flDieTime = 10.0f;
    aliveNear->m_vOrigin = Vector(10.0f, 0.0f, 0.0f);

    TrackingParticle* aliveFar = SpawnInPool<TrackingParticle>();
    aliveFar->id = 4;
    aliveFar->m_flDieTime = 10.0f;
    aliveFar->m_vOrigin = Vector(100.0f, 0.0f, 0.0f);

    mem.ProcessAll();

    assert(expiredDieCount == 1 && expiredDrawCount == 0);
    assert(thinkDieCount == 1 && thinkDrawCount == 0);
    assert(aliveNear->dieCount == 0 && aliveNear->drawCount == 1 && !aliveNear->drewAfterDie);
    assert(aliveFar->dieCount == 0 && aliveFar->drawCount == 1 && !aliveFar->drewAfterDie);
    assert(g_drawnIds.size() == 2 && g_drawnIds[0] == 4 && g_drawnIds[1] == 3);
    std::puts("PASS: expired particles die without drawing and surviving particles sort far-to-near");

    // 2. Pool block reuse within the same frame: particle A dies first, then particle B allocates a replacement
    //    into A's reclaimed block; the draw queue must never draw the recycled block as A's stale entry.
    mem.Reset();
    g_clientTime = 10.0f;
    g_flOldTime = 9.0f;
    g_drawnIds.clear();

    // MemList::Push prepends at head, so push B first, then A so ProcessAll visits A first, then B.
    TrackingParticle* spawnerB = SpawnInPool<TrackingParticle>();
    spawnerB->id = 20;
    spawnerB->m_flDieTime = 20.0f;
    spawnerB->m_vOrigin = Vector(20.0f, 0.0f, 0.0f);
    spawnerB->spawnReplacementInThink = true;

    int aDie = 0, aDraw = 0;
    TrackingParticle* dyingA = SpawnInPool<TrackingParticle>();
    dyingA->id = 10;
    dyingA->m_flDieTime = 9.5f;
    dyingA->externalDieLog = &aDie;
    dyingA->externalDrawLog = &aDraw;

    // Fill remaining 2 blocks with inert invisible particles so A's freed block is guaranteed to be reused by B.
    TrackingParticle* filler1 = SpawnInPool<TrackingParticle>();
    filler1->id = 30;
    filler1->visible = false;
    filler1->m_flDieTime = 20.0f;
    TrackingParticle* filler2 = SpawnInPool<TrackingParticle>();
    filler2->id = 31;
    filler2->visible = false;
    filler2->m_flDieTime = 20.0f;

    mem.ProcessAll();

    assert(aDie == 1 && aDraw == 0);
    assert(g_drawnIds.size() == 1 && g_drawnIds[0] == 20);
    std::puts("PASS: reclaimed block reused in the same frame is not drawn via a stale draw-queue entry");

    CMiniMem::_instance = nullptr;
    return 0;
}
'''


def main():
    source = '\n'.join([
        STUBS,
        extract_function('particleman/pman_particlemem.cpp', 'int ParticleSortFn('),
        extract_function('particleman/pman_particlemem.cpp', 'void CMiniMem::ProcessAll('),
        TESTS,
    ])
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'particleman_processall_tests.cpp'
        src.write_text(source, encoding='utf-8')
        exe = compile_program([src], Path(tmp) / 'particleman_processall_tests',
                              defines=['_CRT_SECURE_NO_WARNINGS'],
                              sanitizers=('address', 'undefined'))
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
