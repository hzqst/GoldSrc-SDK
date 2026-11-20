"""Exercise the production VGUI2 app modal surface functions of both input backends across input contexts.

Run: python tests/vgui2_input_modal_context_tests.py
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

// only the members the app modal functions use
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
 VPANEL GetAppModalSurface();
 void SetAppModalSurface(VPANEL panel);
 void ReleaseAppModalSurface();
 void PanelDeleted(VPANEL panel);
 bool IsChildOfModalPanel(VPANEL panel);
 InputContext_t* GetInputContext(HInputContext context);
 void PanelDeleted(VPANEL focus, InputContext_t& context);
 void SetMouseCapture(VPANEL) {}
 InputContext_t m_DefaultInputContext;
 HInputContext m_hContext = DEFAULT_INPUT_CONTEXT;
 CUtlLinkedList<InputContext_t, int> m_Contexts;
};
'''

TESTS = r'''
}
using namespace vgui2;

struct Fixture {
 Fixture() : child(&modal) { other = input.m_Contexts.AddToTail(); }
 VPANEL Panel(VPanel& panel) { return (VPANEL)&panel; }
 CInputWin32 input;
 HInputContext other;
 VPanel modal, child, outside;
};

static void ModalInOtherContextFiltersPanelsOutsideIt() {
 Fixture f;
 f.input.m_hContext = f.other;
 f.input.SetAppModalSurface(f.Panel(f.modal));
 assert(f.Panel(f.modal) == f.input.GetAppModalSurface());
 assert(f.input.IsChildOfModalPanel(f.Panel(f.modal)));
 assert(f.input.IsChildOfModalPanel(f.Panel(f.child)));
 assert(!f.input.IsChildOfModalPanel(f.Panel(f.outside)));
 puts("PASS: a modal panel set in a non-default context filters panels outside it in that context");
}

static void ModalInOtherContextLeavesDefaultContextAlone() {
 Fixture f;
 f.input.m_hContext = f.other;
 f.input.SetAppModalSurface(f.Panel(f.modal));
 f.input.m_hContext = DEFAULT_INPUT_CONTEXT;
 assert(0 == f.input.GetAppModalSurface());
 assert(f.input.IsChildOfModalPanel(f.Panel(f.outside)));
 f.input.ReleaseAppModalSurface();
 f.input.m_hContext = f.other;
 assert(f.Panel(f.modal) == f.input.GetAppModalSurface());
 puts("PASS: switching to the default context sees no modal panel and releasing there keeps the other context modal");
}

static void ReleasingTheModalRestoresInput() {
 Fixture f;
 f.input.m_hContext = f.other;
 f.input.SetAppModalSurface(f.Panel(f.modal));
 f.input.ReleaseAppModalSurface();
 assert(0 == f.input.GetAppModalSurface());
 assert(f.input.IsChildOfModalPanel(f.Panel(f.outside)));
 puts("PASS: releasing the modal panel lets panels outside it receive input again");
}

static void DeletingTheModalClearsItsOwnContext() {
 Fixture f;
 f.input.m_hContext = f.other;
 f.input.SetAppModalSurface(f.Panel(f.modal));
 f.input.m_hContext = DEFAULT_INPUT_CONTEXT;
 f.input.SetAppModalSurface(f.Panel(f.outside));
 f.input.PanelDeleted(f.Panel(f.modal));
 assert(f.Panel(f.outside) == f.input.GetAppModalSurface());
 f.input.m_hContext = f.other;
 assert(0 == f.input.GetAppModalSurface());
 assert(f.input.IsChildOfModalPanel(f.Panel(f.outside)));
 puts("PASS: deleting a modal panel while another context is active clears only the context it belongs to");
}

int main() {
 ModalInOtherContextFiltersPanelsOutsideIt();
 ModalInOtherContextLeavesDefaultContextAlone();
 ReleasingTheModalRestoresInput();
 DeletingTheModalClearsItsOwnContext();
}
'''

SIGNATURES = (
    'CInputWin32::InputContext_t* CInputWin32::GetInputContext( HInputContext context )',
    'void CInputWin32::PanelDeleted( VPANEL focus, InputContext_t& context )',
    'void CInputWin32::PanelDeleted( VPANEL panel )',
    'bool CInputWin32::IsChildOfModalPanel( VPANEL panel )',
    'VPANEL CInputWin32::GetAppModalSurface()',
    'void CInputWin32::SetAppModalSurface( VPANEL panel )',
    'void CInputWin32::ReleaseAppModalSurface()',
)


def main():
    for backend in BACKENDS:
        print(backend, flush=True)
        source = (ROOT / 'vgui2/src' / backend).read_text(encoding='utf-8')
        code = STUBS + ''.join(function(source, signature) + '\n' for signature in SIGNATURES) + TESTS
        with tempfile.TemporaryDirectory(prefix='goldsrc-vgui2-input-modal-') as folder:
            folder = Path(folder)
            (folder / 'test.cpp').write_text(code, encoding='utf-8')
            executable = compile_program([folder / 'test.cpp'], folder / 'test', sanitizers=['address', 'undefined'])
            subprocess.run([str(executable)], check=True, cwd=folder)


if __name__ == '__main__':
    main()
