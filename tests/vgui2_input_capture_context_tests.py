"""Exercise the production VGUI2 mouse capture cleanup of both input backends across input contexts.

Run: python tests/vgui2_input_capture_context_tests.py
Requires a C++17 compiler; cxx_toolchain.py picks CXX, MSVC on Windows or c++.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from cxx_toolchain import compile_program

ROOT = Path(__file__).resolve().parents[1] / 'GoldSrc-SDK'
BACKENDS = ('InputWin32.cpp', 'inputosx.cpp')


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#define ASSERT_AT( condition, line ) assert( condition )

namespace vgui2 {
typedef uintptr_t VPANEL; // the host build may be 64-bit
typedef int HInputContext;
#define DEFAULT_INPUT_CONTEXT ((vgui2::HInputContext)~0)

class VPanel {
public:
 explicit VPanel(VPanel* parent = nullptr) : parent(parent) {}
 bool HasParent(VPanel* potentialParent) {
  for (VPanel* panel = this; panel; panel = panel->parent)
   if (panel == potentialParent) return true;
  return false;
 }
 VPanel* parent;
};

class KeyValues {
public:
 explicit KeyValues(const char* name) : name(name) {}
 const char* name;
};

struct PostedMessage { VPANEL target; const char* name; };
struct CaptureCall { VPANEL panel; bool state; };

struct RecordingVGui {
 void PostMessage(VPANEL target, KeyValues* message, VPANEL) {
  posted.push_back({target, message->name});
  delete message;
 }
 std::vector<PostedMessage> posted;
};

struct RecordingSurface {
 void EnableMouseCapture(VPANEL panel, bool state) { calls.push_back({panel, state}); }
 std::vector<CaptureCall> calls;
};

RecordingVGui vguiRecorder;
RecordingSurface surfaceRecorder;
RecordingVGui* g_pIVgui = &vguiRecorder;
RecordingSurface* g_pSurface = &surfaceRecorder;

template <class T, class I> class CUtlLinkedList {
public:
 I AddToTail() { elements.emplace_back(); return (I)elements.size() - 1; }
 I Head() const { return elements.empty() ? InvalidIndex() : 0; }
 I Next(I i) const { return i + 1 < (I)elements.size() ? i + 1 : InvalidIndex(); }
 static I InvalidIndex() { return (I)-1; }
 T& operator[](I i) { return elements[i]; }
private:
 std::vector<T> elements;
};

// only the members the mouse capture functions use
class CInputWin32 {
public:
 struct InputContext_t {
  VPanel* _keyFocus = nullptr;
  VPanel* _mouseOver = nullptr;
  VPanel* _oldMouseFocus = nullptr;
  VPanel* _mouseFocus = nullptr;
  VPanel* _mouseCapture = nullptr;
  VPanel* _appModalPanel = nullptr;
 };
 void SetMouseCapture(VPANEL panel);
 void PanelDeleted(VPANEL panel);
 bool IsChildOfModalPanel(VPANEL panel);
 InputContext_t* GetInputContext(HInputContext context);
 void PanelDeleted(VPANEL focus, InputContext_t& context);
 InputContext_t m_DefaultInputContext;
 HInputContext m_hContext = DEFAULT_INPUT_CONTEXT;
 CUtlLinkedList<InputContext_t, int> m_Contexts;
};
'''

TESTS = r'''
}
using namespace vgui2;

struct Fixture {
 Fixture() {
  other = input.m_Contexts.AddToTail();
  vguiRecorder.posted.clear();
  surfaceRecorder.calls.clear();
 }
 VPANEL Panel(VPanel& panel) { return (VPANEL)&panel; }
 // A captures in the default context, then B captures in another context
 void CaptureInBothContexts() {
  input.SetMouseCapture(Panel(a));
  input.m_hContext = other;
  input.SetMouseCapture(Panel(b));
  vguiRecorder.posted.clear();
  surfaceRecorder.calls.clear();
 }
 CInputWin32 input;
 HInputContext other;
 VPanel a, b;
};

static void DeletingInactiveCaptureKeepsActiveCapture() {
 Fixture f;
 f.CaptureInBothContexts();
 f.input.PanelDeleted(f.Panel(f.a));
 assert(&f.b == f.input.GetInputContext(f.other)->_mouseCapture);
 assert(nullptr == f.input.m_DefaultInputContext._mouseCapture);
 assert(vguiRecorder.posted.empty());
 assert(surfaceRecorder.calls.empty());
 puts("PASS: deleting the capture panel of an inactive context leaves the active capture, messages and surface alone");
}

static void DeletingActiveCaptureReleasesIt() {
 Fixture f;
 f.CaptureInBothContexts();
 f.input.PanelDeleted(f.Panel(f.b));
 assert(nullptr == f.input.GetInputContext(f.other)->_mouseCapture);
 assert(&f.a == f.input.m_DefaultInputContext._mouseCapture);
 assert(1 == vguiRecorder.posted.size());
 assert(f.Panel(f.b) == vguiRecorder.posted[0].target);
 assert(0 == strcmp("MouseCaptureLost", vguiRecorder.posted[0].name));
 assert(1 == surfaceRecorder.calls.size());
 assert(f.Panel(f.b) == surfaceRecorder.calls[0].panel);
 assert(!surfaceRecorder.calls[0].state);
 puts("PASS: deleting the capture panel of the active context still releases the platform capture");
}

int main() {
 DeletingInactiveCaptureKeepsActiveCapture();
 DeletingActiveCaptureReleasesIt();
}
'''

SIGNATURES = (
    'CInputWin32::InputContext_t* CInputWin32::GetInputContext( HInputContext context )',
    'void CInputWin32::PanelDeleted( VPANEL focus, InputContext_t& context )',
    'void CInputWin32::PanelDeleted( VPANEL panel )',
    'bool CInputWin32::IsChildOfModalPanel( VPANEL panel )',
    'void CInputWin32::SetMouseCapture( VPANEL panel )',
)


def main():
    for backend in BACKENDS:
        print(backend, flush=True)
        source = (ROOT / 'vgui2/src' / backend).read_text(encoding='utf-8')
        code = STUBS + ''.join(function(source, signature) + '\n' for signature in SIGNATURES) + TESTS
        with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-input-capture-') as folder:
            folder = Path(folder)
            (folder / 'test.cpp').write_text(code, encoding='utf-8')
            executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
