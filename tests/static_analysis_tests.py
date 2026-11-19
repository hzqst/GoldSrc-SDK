"""Check the parts of tools/static_analysis.py that do not run the tools: the
compile database selection, the changed file selection, the clang diagnostic
filter and the Cppcheck and clang-tidy command lines.

Run: python tests/static_analysis_tests.py
"""
import inspect
from pathlib import Path
import sys
import tempfile

sys.dont_write_bytecode = True
REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPOSITORY_ROOT / 'tools'))
sys.path.insert(0, str(REPOSITORY_ROOT / 'tests'))
import cxx_toolchain  # noqa: E402
import static_analysis  # noqa: E402

SOURCE_ROOT = Path(tempfile.gettempdir()) / 'goldsrc-static-analysis' / 'GoldSrc-SDK'
BUILD_DIR = SOURCE_ROOT / 'bin' / 'obj' / 'cmake' / 'preset'


def entry(path, directory=BUILD_DIR, command='cl'):
    return {'directory': str(directory), 'command': command, 'file': str(path)}


def selection_keeps_one_entry_per_project_source():
    entries = [
        entry(SOURCE_ROOT / 'engine' / 'common.cpp', command='first'),
        entry(SOURCE_ROOT / 'engine' / 'common.cpp', command='second'),
        entry('../../../../dlls/combat.cpp'),
        entry(SOURCE_ROOT / 'external' / 'SDL2' / 'sdl.c'),
        entry(SOURCE_ROOT / 'lib' / 'public' / 'stub.cpp'),
        entry(BUILD_DIR / 'a3dapi' / 'generated.cpp'),
        entry(SOURCE_ROOT / 'engine' / 'engine.rc'),
        entry(SOURCE_ROOT.parent / 'tests' / 'q_ftol_tests.cpp'),
    ]
    kept = static_analysis.select_entries(entries, BUILD_DIR, source_root=SOURCE_ROOT)
    assert ['first', 'cl'] == [kept_entry['command'] for kept_entry in kept], kept
    assert ['common.cpp', 'combat.cpp'] == [static_analysis.entry_path(e).name for e in kept], kept
    print('PASS: one entry per project source; external/, lib/, the build tree, .rc and outside files are dropped')


def selection_keeps_only_selected_sources():
    entries = [entry(SOURCE_ROOT / 'engine' / 'common.cpp'), entry(SOURCE_ROOT / 'dlls' / 'combat.cpp')]
    selected, headers = static_analysis.split_selection([SOURCE_ROOT / 'dlls' / 'combat.cpp',
                                                         SOURCE_ROOT / 'dlls' / 'cbase.h',
                                                         SOURCE_ROOT.parent / 'README.md'])
    assert [SOURCE_ROOT / 'dlls' / 'cbase.h'] == headers, headers
    kept = static_analysis.select_entries(entries, BUILD_DIR, selected, source_root=SOURCE_ROOT)
    assert ['combat.cpp'] == [static_analysis.entry_path(e).name for e in kept], kept
    assert [] == static_analysis.select_entries(entries, BUILD_DIR, set(), source_root=SOURCE_ROOT)
    print('PASS: only the selected sources are kept, and headers are returned to be reported')


def windows_paths_match_without_case_or_slash():
    if not static_analysis.WINDOWS:
        print('SKIP: Windows path comparison')
        return
    path = str(SOURCE_ROOT / 'Engine' / 'Common.cpp').replace('\\', '/').upper()
    assert static_analysis.path_key(path) == static_analysis.path_key(SOURCE_ROOT / 'engine' / 'common.cpp')
    assert static_analysis.is_excluded(str(SOURCE_ROOT / 'EXTERNAL' / 'x.h').replace('\\', '/'), SOURCE_ROOT)
    assert not static_analysis.is_excluded(SOURCE_ROOT / 'externals' / 'x.h', SOURCE_ROOT)
    print('PASS: Windows paths match in any case and with either slash; external/ does not match externals/')


def diagnostics_in_third_party_headers_are_dropped():
    external = SOURCE_ROOT / 'external' / 'SDL2' / 'SDL.h'
    project = SOURCE_ROOT / 'engine' / 'common.cpp'
    text = '\n'.join([
        f'{external}:10:2: warning: external warning [bugprone-a]',
        '  excerpt of the external warning',
        f'{project}:5:1: note: a note of the external warning',
        f'{project}:20:3: warning: project warning [bugprone-b]',
        '  excerpt of the project warning',
        f'{external}:11:4: note: a note of the project warning',
        f'{project}:30:3: error: project error [clang-diagnostic-error]',
        'warning: driver warning [clang-diagnostic-unused-command-line-argument]',
    ])
    kept, count = static_analysis.filter_diagnostics(text, SOURCE_ROOT)
    assert 2 == count, count
    assert text.split('\n')[3:] == kept.split('\n'), kept
    print('PASS: third-party diagnostics are dropped with their notes; project warnings and errors are counted')


def cppcheck_command_targets_32_bit_platforms():
    output_dir = SOURCE_ROOT / 'bin' / 'static-analysis'
    project = output_dir / 'compile_commands.json'
    windows = static_analysis.cppcheck_command('cppcheck', project, output_dir, 4, windows=True,
                                               source_root=SOURCE_ROOT)
    linux = static_analysis.cppcheck_command('cppcheck', project, output_dir, 4, exhaustive=True, windows=False,
                                             source_root=SOURCE_ROOT)
    assert '--platform=win32A' in windows and '--library=windows' in windows, windows
    assert '--platform=unix32' in linux and '--library=posix' in linux, linux
    assert '--check-level=normal' in windows and '--check-level=exhaustive' in linux
    assert f'--library={static_analysis.CPPCHECK_LIBRARY}' in windows
    assert static_analysis.CPPCHECK_LIBRARY.exists(), static_analysis.CPPCHECK_LIBRARY
    for folder in static_analysis.EXCLUDED_DIRS:
        assert f'--suppress=*:*/GoldSrc-SDK/{folder}/*' in linux, linux
    assert not any(':\\' in argument or ':/' in argument for argument in windows if argument.startswith('--suppress'))
    print('PASS: Cppcheck analyzes 32-bit win32A or unix32 with the GoldSrc library and skips third-party code')


def clang_tidy_command_reads_msvc_as_32_bit():
    windows = static_analysis.clang_tidy_command('clang-tidy', BUILD_DIR, 'common.cpp',
                                                 static_analysis.ANALYZER_CHECKS, windows=True)
    linux = static_analysis.clang_tidy_command('clang-tidy', BUILD_DIR, 'common.cpp', windows=False)
    assert static_analysis.CLANG_TIDY_WINDOWS_TARGET in windows, windows
    assert static_analysis.CLANG_TIDY_WINDOWS_TARGET not in linux, linux
    assert f'--checks={static_analysis.ANALYZER_CHECKS}' in windows, windows
    assert not any(argument.startswith('--checks') for argument in linux), linux
    assert static_analysis.ANALYZER_CHECKS.startswith('-*,clang-analyzer-*'), static_analysis.ANALYZER_CHECKS
    assert '--extra-arg=-Wno-error' in linux and 'common.cpp' == linux[-1], linux
    assert not any(argument.startswith(('-fix', '--fix')) for argument in windows + linux)
    print('PASS: clang-tidy reads MSVC command lines as i686, only the analyzer overrides the checks, never -fix')


def vcvars_environment_defaults_to_x64():
    parameter = inspect.signature(cxx_toolchain.vcvars_environment).parameters['arch']
    assert 'x64' == parameter.default, parameter.default
    print('PASS: vcvars_environment keeps the x64 tools by default')


if __name__ == '__main__':
    selection_keeps_one_entry_per_project_source()
    selection_keeps_only_selected_sources()
    windows_paths_match_without_case_or_slash()
    diagnostics_in_third_party_headers_are_dropped()
    cppcheck_command_targets_32_bit_platforms()
    clang_tidy_command_reads_msvc_as_32_bit()
    vcvars_environment_defaults_to_x64()
