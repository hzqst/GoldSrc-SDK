"""Regression tests for ForceList::UpdateForce unlinking and deletion without leaks.

Run: python -I tests/particleman_forcelist_tests.py
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


def extract_force_header():
    text = (SDK_ROOT / 'particleman' / 'pman_force.h').read_text(encoding='utf-8')
    lines = [
        line for line in text.splitlines()
        if not line.strip().startswith('#pragma once')
        and not line.strip().startswith('#include "pman_main.h"')
    ]
    return '\n'.join(lines)


STUBS = r'''
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <vector>

static int g_liveVectors = 0;

struct Vector {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vector() { ++g_liveVectors; }
    Vector(float a, float b, float c) : x(a), y(b), z(c) { ++g_liveVectors; }
    Vector(const Vector& o) : x(o.x), y(o.y), z(o.z) { ++g_liveVectors; }
    Vector& operator=(const Vector& o) = default;
    ~Vector() { --g_liveVectors; }
};

static std::vector<float> g_appliedStrengths;

struct CMiniMem {
    static int ApplyForce(Vector, Vector, float, float strength) {
        g_appliedStrengths.push_back(strength);
        return 1;
    }
};
'''

TESTS = r'''
static int LiveForceMembers() {
    assert(g_liveVectors % 2 == 0);
    return g_liveVectors / 2;
}

static void CheckListInvariants(const ForceList& list, const std::vector<float>& expectedStrengths) {
    assert(list.m_iElements == static_cast<int>(expectedStrengths.size()));
    assert(LiveForceMembers() == static_cast<int>(expectedStrengths.size()));

    if (expectedStrengths.empty()) {
        assert(list.pFirst == nullptr);
        assert(list.pLast == nullptr);
        assert(list.pCurrent == nullptr);
        return;
    }

    assert(list.pFirst != nullptr);
    assert(list.pFirst->m_pPrevious == nullptr);
    assert(list.pLast != nullptr);
    assert(list.pLast->m_pNext == nullptr);
    assert(list.pCurrent == list.pLast);

    const ForceMember* prev = nullptr;
    const ForceMember* node = list.pFirst;
    size_t idx = 0;
    while (node) {
        assert(node->m_pPrevious == prev);
        assert(idx < expectedStrengths.size());
        assert(node->m_flStrength == expectedStrengths[idx]);
        prev = node;
        node = node->m_pNext;
        ++idx;
    }
    assert(idx == expectedStrengths.size());
    assert(prev == list.pLast);
}

static ForceMember* PushForce(ForceList& list, float strength, float dieTime) {
    ForceMember* f = list.AddForceMember();
    assert(f != nullptr);
    f->m_flStrength = strength;
    f->m_flDieTime = dieTime;
    return f;
}

int main() {
    // 1. Single node expires.
    {
        ForceList list;
        PushForce(list, 10.0f, 1.0f);
        CheckListInvariants(list, {10.0f});

        g_appliedStrengths.clear();
        list.UpdateForce(2.0f);
        CheckListInvariants(list, {});
        assert(g_appliedStrengths.empty());

        // Adding after empty must still work.
        PushForce(list, 99.0f, 5.0f);
        CheckListInvariants(list, {99.0f});
    }
    assert(LiveForceMembers() == 0);
    std::puts("PASS: single expired ForceMember is deleted cleanly and list remains reusable");

    // 2. Head node expires.
    {
        ForceList list;
        PushForce(list, 1.0f, 1.0f);
        PushForce(list, 2.0f, 10.0f);
        PushForce(list, 3.0f, 10.0f);

        g_appliedStrengths.clear();
        list.UpdateForce(2.0f);
        CheckListInvariants(list, {2.0f, 3.0f});
        assert((g_appliedStrengths == std::vector<float>{2.0f, 3.0f}));
    }
    assert(LiveForceMembers() == 0);
    std::puts("PASS: expired head ForceMember is unlinked and deleted");

    // 3. Middle node expires.
    {
        ForceList list;
        PushForce(list, 1.0f, 10.0f);
        PushForce(list, 2.0f, 1.0f);
        PushForce(list, 3.0f, 10.0f);

        g_appliedStrengths.clear();
        list.UpdateForce(2.0f);
        CheckListInvariants(list, {1.0f, 3.0f});
        assert((g_appliedStrengths == std::vector<float>{1.0f, 3.0f}));
    }
    assert(LiveForceMembers() == 0);
    std::puts("PASS: expired middle ForceMember is unlinked and deleted");

    // 4. Tail node expires, and subsequent AddForceMember appends after the new tail.
    {
        ForceList list;
        PushForce(list, 1.0f, 10.0f);
        PushForce(list, 2.0f, 10.0f);
        PushForce(list, 3.0f, 1.0f);

        g_appliedStrengths.clear();
        list.UpdateForce(2.0f);
        CheckListInvariants(list, {1.0f, 2.0f});
        assert((g_appliedStrengths == std::vector<float>{1.0f, 2.0f}));

        PushForce(list, 4.0f, 10.0f);
        CheckListInvariants(list, {1.0f, 2.0f, 4.0f});
    }
    assert(LiveForceMembers() == 0);
    std::puts("PASS: expired tail ForceMember is unlinked, deleted, and updates pLast/pCurrent");

    // 5. All nodes expire in one UpdateForce sweep & ClearForceMembers resets pointers for single node.
    {
        ForceList list;
        PushForce(list, 1.0f, 1.0f);
        PushForce(list, 2.0f, 1.5f);
        PushForce(list, 3.0f, 2.0f);
        list.UpdateForce(3.0f);
        CheckListInvariants(list, {});

        PushForce(list, 42.0f, 10.0f);
        list.ClearForceMembers();
        CheckListInvariants(list, {});
    }
    assert(LiveForceMembers() == 0);
    std::puts("PASS: bulk expiration and ClearForceMembers free all nodes and reset pointers");

    return 0;
}
'''


def main():
    source = '\n'.join([
        STUBS,
        extract_force_header(),
        TESTS,
    ])
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'particleman_forcelist_tests.cpp'
        src.write_text(source, encoding='utf-8')
        exe = compile_program([src], Path(tmp) / 'particleman_forcelist_tests',
                              defines=['_CRT_SECURE_NO_WARNINGS'],
                              sanitizers=('address', 'undefined'))
        subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
