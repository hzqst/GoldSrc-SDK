"""Run Cppcheck, clang-tidy and the Clang Static Analyzer on the GoldSrc sources.

The tools read the compile database of a CMake preset: windows-x86-analyze
(Ninja + MSVC x86) on Windows, linux-x86-debug on Linux. The preset is
configured when its compile_commands.json does not exist yet; on Windows this
and the clang-tidy runs use the vcvarsall x86 environment found through
vswhere, so a developer prompt is not needed.

The compile database is reduced to the project sources (no external/, lib/ or
files generated in the build tree), with one entry per source file, and written
to the output directory, so that the three tools analyze the same files. The
Clang Static Analyzer runs as the clang-analyzer-* checks of clang-tidy, which
reads both the MSVC and the GCC compile commands.

The tools only report: the exit status is not 0 only when a tool is missing or
fails to run, never because of the warnings it found.

Run from the repository root, for example:
  python -I tools/static_analysis.py all --changed
  python -I tools/static_analysis.py cppcheck
  python -I tools/static_analysis.py clang-tidy --files GoldSrc-SDK/GameUI/SaveGameDialog.cpp
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ElementTree

REPOSITORY_ROOT = Path(__file__).resolve().parent.parent
SOURCE_ROOT = REPOSITORY_ROOT / 'GoldSrc-SDK'
sys.dont_write_bytecode = True
sys.path.insert(0, str(REPOSITORY_ROOT / 'tests'))
from cxx_toolchain import environment_path, vcvars_environment  # noqa: E402

WINDOWS = os.name == 'nt'
DEFAULT_PRESET = 'windows-x86-analyze' if WINDOWS else 'linux-x86-debug'
DEFAULT_OUTPUT_DIR = SOURCE_ROOT / 'bin' / 'static-analysis'
# Third-party code under the source root, which is not analyzed.
EXCLUDED_DIRS = ('external', 'lib')
SOURCE_SUFFIXES = ('.c', '.cc', '.cpp', '.cxx')
HEADER_SUFFIXES = ('.h', '.hh', '.hpp', '.hxx', '.inl')
CPPCHECK_LIBRARY = REPOSITORY_ROOT / 'docs' / 'cppcheck-goldsrc.cfg'
# Cppcheck warnings about the design of the engine and game code rather than
# defects: C structs without constructors, the m_SaveData of every entity
# class, C casts, Vector parameters passed by value, which are the exported and
# virtual interfaces of the games, and the classes of the same name in separate
# modules, such as the Vector of every game DLL.
CPPCHECK_SUPPRESSED_IDS = ('missingIncludeSystem', 'uninitMemberVarNoCtor', 'duplInheritedMember',
                           'dangerousTypeCast', 'passedByValue', 'ctuOneDefinitionRuleViolation')
# Without a target, clang reads an MSVC command line as 64-bit and gets every
# pointer size and sizeof assertion wrong.
CLANG_TIDY_WINDOWS_TARGET = '--extra-arg-before=--target=i686-pc-windows-msvc'
# /WX and -Werror would report compiler warnings as errors that hide the checks;
# the engine adds /fp:strict after the default /fp:precise, which clang reports.
CLANG_TIDY_EXTRA_ARGUMENTS = ('--extra-arg=-Wno-error', '--extra-arg=-Wno-overriding-option')
# The analyzer checks without the Apple platform ones, and without the C library
# buffer function warnings, which flag every strcpy/sprintf of the C code.
ANALYZER_CHECKS = ('-*,clang-analyzer-*,-clang-analyzer-osx.*,-clang-analyzer-optin.osx.*,'
                   '-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,'
                   '-clang-analyzer-security.insecureAPI.strcpy')
# path:line:column: warning|error: message, the first line of a clang diagnostic;
# its notes and source excerpts follow it.
CLANG_DIAGNOSTIC = re.compile(r'^(?P<path>.+?):\d+:\d+: (?P<kind>warning|error|note): ')


def path_key(path):
    """Compare paths as Windows does: without case, with either slash."""
    text = os.path.normpath(str(path))
    return os.path.normcase(text) if WINDOWS else text


def is_below(path, folder):
    path, folder = path_key(path), path_key(folder)
    return path == folder or path.startswith(folder.rstrip(os.sep) + os.sep)


def is_excluded(path, source_root=SOURCE_ROOT):
    return any(is_below(path, Path(source_root) / folder) for folder in EXCLUDED_DIRS)


def entry_path(entry):
    return Path(entry['directory'], entry['file'])


def select_entries(entries, build_dir, selected=None, source_root=SOURCE_ROOT):
    """Keep the first entry of each project source file, optionally only of the
    selected files (a set of path_key values)."""
    kept = {}
    for entry in entries:
        path = entry_path(entry)
        key = path_key(path)
        if (path.suffix.lower() not in SOURCE_SUFFIXES or not is_below(path, source_root)
                or is_below(path, build_dir) or is_excluded(path, source_root) or key in kept
                or (selected is not None and key not in selected)):
            continue
        kept[key] = entry
    return list(kept.values())


def git_lines(*arguments):
    result = subprocess.run(['git', *arguments], cwd=REPOSITORY_ROOT, capture_output=True, text=True, check=True)
    return [line for line in result.stdout.splitlines() if line]


def changed_files(base):
    """The files changed since base, committed or not, and the untracked files."""
    names = git_lines('diff', '--name-only', base) + git_lines('ls-files', '--others', '--exclude-standard')
    return [REPOSITORY_ROOT / name for name in dict.fromkeys(names)]


def split_selection(paths):
    """Return the selected source files as a set of path_key values, and the
    selected headers, which are reported but not expanded into their sources."""
    sources = {path_key(Path(path).resolve()) for path in paths if Path(path).suffix.lower() in SOURCE_SUFFIXES}
    headers = [Path(path) for path in paths if Path(path).suffix.lower() in HEADER_SUFFIXES]
    return sources, headers


def filter_diagnostics(text, source_root=SOURCE_ROOT):
    """Drop the clang diagnostics, with their notes and excerpts, that are in
    third-party headers, and return the rest and the number of warnings and
    errors in it."""
    kept, count, keep = [], 0, True
    for line in text.splitlines():
        match = CLANG_DIAGNOSTIC.match(line)
        if match and match['kind'] != 'note':
            keep = not is_excluded(match['path'], source_root)
            count += keep
        if keep:
            kept.append(line)
    return '\n'.join(kept), count


def cppcheck_command(cppcheck, project, output_dir, jobs, exhaustive=False, windows=WINDOWS,
                     source_root=SOURCE_ROOT):
    command = [cppcheck, f'--project={project}', '--enable=warning,performance,portability',
               f'--check-level={"exhaustive" if exhaustive else "normal"}',
               '--platform=win32A' if windows else '--platform=unix32',
               '--library=windows' if windows else '--library=posix', f'--library={CPPCHECK_LIBRARY}',
               '--inline-suppr', '--quiet', *(f'--suppress={check}' for check in CPPCHECK_SUPPRESSED_IDS),
               f'-j{jobs}',
               f'--cppcheck-build-dir={output_dir / "cppcheck-build"}',
               '--xml', '--xml-version=2', f'--output-file={output_dir / "cppcheck.xml"}']
    # Cppcheck reads the colon after a drive letter as the start of a line
    # number, so the suppressed folders are matched without the drive.
    return command + [f'--suppress=*:*/{Path(source_root).name}/{folder}/*' for folder in EXCLUDED_DIRS]


def cppcheck_report(xml_path):
    """Return the Cppcheck XML results as file:line: severity: message [id] lines."""
    lines = []
    for error in ElementTree.parse(xml_path).getroot().iter('error'):
        location = error.find('location')
        where = f'{location.get("file")}:{location.get("line")}' if location is not None else 'nofile'
        lines.append(f'{where}: {error.get("severity")}: {error.get("msg")} [{error.get("id")}]')
    return lines


def clang_tidy_command(clang_tidy, database_dir, source, checks=None, windows=WINDOWS):
    command = [clang_tidy, '-p', str(database_dir), '--quiet', *CLANG_TIDY_EXTRA_ARGUMENTS]
    if checks:
        command.append(f'--checks={checks}')
    if windows:
        command.append(CLANG_TIDY_WINDOWS_TARGET)
    return command + [str(source)]


def find_tool(name, path=None):
    tool = shutil.which(path or name)
    if not tool:
        sys.exit(f'{path} was not found' if path else f'{name} was not found in PATH')
    return tool


def find_clang_tidy(path, env):
    """The given clang-tidy, else on Windows the one of the Visual Studio C++
    Clang tools, which knows its MSVC headers, else the one in PATH."""
    vc_folder = (env or os.environ).get('VCINSTALLDIR')
    if not path and vc_folder:
        for folder in ('Tools/Llvm/x64/bin', 'Tools/Llvm/bin'):
            candidate = Path(vc_folder, folder, 'clang-tidy.exe')
            if candidate.exists():
                return str(candidate)
    return find_tool('clang-tidy', path)


def build_environment():
    if not WINDOWS or 'VCToolsInstallDir' in os.environ:
        return None
    env = vcvars_environment('x86')
    if env is None:
        sys.exit('No Visual Studio with the C++ x86 tools was found through vswhere')
    return env


def compile_database(preset, env):
    path = SOURCE_ROOT / 'bin' / 'obj' / 'cmake' / preset / 'compile_commands.json'
    if not path.exists():
        cmake = shutil.which('cmake', path=environment_path(env) if env else None) or find_tool('cmake')
        subprocess.run([cmake, '--preset', preset], cwd=SOURCE_ROOT, env=env, check=True)
    if not path.exists():
        sys.exit(f'The {preset} preset did not write {path}; it needs CMAKE_EXPORT_COMPILE_COMMANDS and a '
                 'Ninja or Makefile generator')
    return path


def run_cppcheck(args, database, output_dir):
    command = cppcheck_command(find_tool('cppcheck', args.cppcheck), database, output_dir, args.jobs, args.exhaustive)
    (output_dir / 'cppcheck-build').mkdir(exist_ok=True)
    if subprocess.run(command).returncode:
        sys.exit('cppcheck failed')
    lines = cppcheck_report(output_dir / 'cppcheck.xml')
    (output_dir / 'cppcheck.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return 'cppcheck', len(lines), output_dir / 'cppcheck.txt'


def run_clang_tidy(args, entries, output_dir, env, name, checks):
    clang_tidy = find_clang_tidy(args.clang_tidy, env)
    print(f'{name}: {clang_tidy}')

    def analyze(entry):
        result = subprocess.run(clang_tidy_command(clang_tidy, output_dir, entry_path(entry), checks),
                                env=env, capture_output=True, text=True, errors='replace')
        output = result.stdout + result.stderr
        # clang-tidy exits with 1 when the file has compile errors, which are reported.
        if result.returncode not in (0, 1):
            return output, None
        return filter_diagnostics(output)

    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(analyze, entries))
    failed = [entry_path(entry) for entry, (_, count) in zip(entries, results) if count is None]
    report = output_dir / f'{name}.txt'
    report.write_text('\n'.join(text for text, _ in results if text) + '\n', encoding='utf-8')
    if failed:
        sys.exit(f'{name} failed on {", ".join(map(str, failed))}; see {report}')
    return name, sum(count for _, count in results), report


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('tool', choices=('cppcheck', 'clang-tidy', 'analyzer', 'all'))
    parser.add_argument('--preset', default=DEFAULT_PRESET, help=f'CMake configure preset (default {DEFAULT_PRESET})')
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument('--files', nargs='+', type=Path, help='analyze only these source files')
    selection.add_argument('--changed', nargs='?', const='main', metavar='BASE',
                           help='analyze only the source files changed since BASE (default main)')
    parser.add_argument('-j', '--jobs', type=int, default=os.cpu_count() or 1)
    parser.add_argument('--output-dir', type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument('--cppcheck', help='Cppcheck executable (default cppcheck in PATH)')
    parser.add_argument('--clang-tidy', help='clang-tidy executable (default the Visual Studio one on Windows, '
                        'else clang-tidy in PATH)')
    parser.add_argument('--exhaustive', action='store_true', help='run Cppcheck with --check-level=exhaustive')
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_arguments(argv)
    paths = args.files if args.files else changed_files(args.changed) if args.changed else None
    selected = None
    if paths is not None:
        selected, headers = split_selection(paths)
        for header in headers:
            print(f'note: header {header} is not analyzed on its own; select the sources that include it')
    env = build_environment()
    database = compile_database(args.preset, env)
    entries = select_entries(json.loads(database.read_text(encoding='utf-8')), database.parent, selected)
    if not entries:
        print('No project source files to analyze')
        return 0
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    # clang-tidy -p looks for a file of this name in the given folder.
    filtered = output_dir / 'compile_commands.json'
    filtered.write_text(json.dumps(entries, indent=1), encoding='utf-8')
    print(f'Analyzing {len(entries)} source files from {database}')

    summary = []
    if args.tool in ('cppcheck', 'all'):
        summary.append(run_cppcheck(args, filtered, output_dir))
    if args.tool in ('clang-tidy', 'all'):
        summary.append(run_clang_tidy(args, entries, output_dir, env, 'clang-tidy', None))
    if args.tool in ('analyzer', 'all'):
        summary.append(run_clang_tidy(args, entries, output_dir, env, 'clang-analyzer', ANALYZER_CHECKS))
    for name, count, report in summary:
        print(f'{name}: {count} warnings, see {report}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
