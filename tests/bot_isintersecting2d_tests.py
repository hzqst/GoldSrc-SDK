"""Regression tests for IsIntersecting2D writing *result and checking segment B bounds when numS == 0 (#513)."""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

SDK_ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'


def extract(relpath, signature):
    source = (SDK_ROOT / relpath).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


HEADER = r'''
#include <cassert>
#include <cmath>
#include <cstddef>

struct Vector {
    float x, y, z;
    Vector() = default;
    Vector(float a, float b, float c) : x(a), y(b), z(c) {}
    Vector operator+(const Vector& o) const { return Vector(x + o.x, y + o.y, z + o.z); }
    Vector operator-(const Vector& o) const { return Vector(x - o.x, y - o.y, z - o.z); }
};

static inline Vector operator*(float s, const Vector& v) {
    return Vector(s * v.x, s * v.y, s * v.z);
}
'''

TESTS = r'''
static bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

template <typename Fn>
static void CheckIsIntersecting2D(Fn fn) {
    // 1. startA coincides with startB (numS == 0, t == 0): must return true and write *result = startA.
    {
        Vector result(999.0f, 999.0f, 999.0f);
        bool hit = fn(Vector(10.0f, 20.0f, 35.0f), Vector(10.0f, 120.0f, 45.0f),
                      Vector(10.0f, 20.0f, 0.0f), Vector(110.0f, 20.0f, 0.0f), &result);
        assert(hit);
        assert(Near(10.0f, result.x));
        assert(Near(20.0f, result.y));
        assert(Near(35.0f, result.z));
    }

    // 2. startA lies on segment B interior (numS == 0, t == 0.5): must return true and write startA.
    {
        Vector result(-1.0f, -1.0f, -1.0f);
        bool hit = fn(Vector(50.0f, 20.0f, 12.5f), Vector(50.0f, 80.0f, 22.5f),
                      Vector(0.0f, 20.0f, 0.0f), Vector(100.0f, 20.0f, 0.0f), &result);
        assert(hit);
        assert(Near(50.0f, result.x));
        assert(Near(20.0f, result.y));
        assert(Near(12.5f, result.z));
    }

    // 3. startA lies on infinite line of B, but outside segment B (numS == 0, t < 0 and t > 1):
    //    must return false (Option A: segment B bounds are enforced even when s == 0).
    {
        Vector result(777.0f, 777.0f, 777.0f);
        // Line B runs from (100, 20) to (200, 20); startA is at (50, 20) => t = -0.5.
        bool hitBefore = fn(Vector(50.0f, 20.0f, 5.0f), Vector(50.0f, 80.0f, 15.0f),
                            Vector(100.0f, 20.0f, 0.0f), Vector(200.0f, 20.0f, 0.0f), &result);
        assert(!hitBefore);

        // Line B runs from (0, 20) to (100, 20); startA is at (150, 20) => t = 1.5.
        bool hitAfter = fn(Vector(150.0f, 20.0f, 5.0f), Vector(150.0f, 80.0f, 15.0f),
                           Vector(0.0f, 20.0f, 0.0f), Vector(100.0f, 20.0f, 0.0f), &result);
        assert(!hitAfter);
    }

    // 4. Normal interior crossing computes interpolated 3D intersection along A.
    {
        Vector result(0.0f, 0.0f, 0.0f);
        bool hit = fn(Vector(0.0f, 0.0f, 10.0f), Vector(100.0f, 100.0f, 30.0f),
                      Vector(0.0f, 100.0f, 0.0f), Vector(100.0f, 0.0f, 0.0f), &result);
        assert(hit);
        assert(Near(50.0f, result.x));
        assert(Near(50.0f, result.y));
        assert(Near(20.0f, result.z));
    }

    // 5. Parallel segments return false; nullptr result is accepted on hit.
    {
        assert(!fn(Vector(0.0f, 0.0f, 0.0f), Vector(10.0f, 0.0f, 0.0f),
                   Vector(0.0f, 5.0f, 0.0f), Vector(10.0f, 5.0f, 0.0f), nullptr));
        assert(fn(Vector(10.0f, 20.0f, 0.0f), Vector(10.0f, 50.0f, 0.0f),
                  Vector(10.0f, 20.0f, 0.0f), Vector(50.0f, 20.0f, 0.0f), nullptr));
    }
}

int main() {
    CheckIsIntersecting2D(cstrike_bot::IsIntersecting2D);
    CheckIsIntersecting2D(shared_bot::IsIntersecting2D);
    return 0;
}
'''


def main():
    cstrike_fn = extract('cstrike/game_shared/bot/bot_util.h', 'inline bool IsIntersecting2D(')
    shared_fn = extract('game_shared/bot/bot_util.h', 'inline bool IsIntersecting2D(')
    source_text = '\n'.join([
        HEADER,
        'namespace cstrike_bot {', cstrike_fn, '}',
        'namespace shared_bot {', shared_fn, '}',
        TESTS,
    ])
    with tempfile.TemporaryDirectory(prefix='goldsrc-bot-isintersecting2d-') as folder:
        work = Path(folder)
        source = work / 'test.cpp'
        source.write_text(source_text, encoding='utf-8')
        executable = compile_program([source], work / 'test', sanitizers=['address', 'undefined'])
        subprocess.run([str(executable)], cwd=work, check=True)
    print('PASS: IsIntersecting2D validates segment B bounds and writes *result when numS == 0')


if __name__ == '__main__':
    main()
