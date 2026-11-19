"""Check built Windows engine DLLs against the .def files they are linked with.

Each --module DLL must export exactly the names and ordinals listed in its .def
file, and every forwarded export must name a symbol that the target DLL, found
next to the module, really exports. The linker does not check forwarders, so a
VGUI header or vgui.def change can leave sw.dll or swds.dll pointing at a
symbol vgui.dll no longer has, which only fails when the engine is loaded.

  python -I tests/engine_def_exports_tests.py --dumpbin <dumpbin.exe>
      --module <sw.dll> <sw_engine.def> [--module <dll> <def> ...]
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

DEF_ENTRY = re.compile(r'^\s+(?P<name>[^\s=]+)(?:=(?P<forward>\S+))?\s+@(?P<ordinal>\d+)(?:\s+DATA)?\s*$')
DEF_HEADER = re.compile(r'^\s*(LIBRARY\b.*|EXPORTS\s*|;.*)?$')
FORWARDED = re.compile(r'^\s+(?P<ordinal>\d+)\s+[0-9A-F]+\s+(?P<name>\S+) \(forwarded to (?P<forward>\S+)\)$')
EXPORTED = re.compile(r'^\s+(?P<ordinal>\d+)\s+[0-9A-F]+\s+[0-9A-F]{8}\s+(?P<name>\S+)')


def def_exports(path):
    """Return {name: (ordinal, forward or None)} from a .def file."""
    exports = {}
    for number, line in enumerate(path.read_text(encoding='ascii').splitlines(), 1):
        match = DEF_ENTRY.match(line)
        if match:
            exports[match['name']] = (int(match['ordinal']), match['forward'])
        elif not DEF_HEADER.match(line):
            sys.exit(f'{path}:{number}: unrecognized .def line: {line.strip()}')
    return exports


def dll_exports(dumpbin, path):
    """Return {name: (ordinal, forward or None)} from dumpbin /exports."""
    output = subprocess.run([dumpbin, '/nologo', '/exports', str(path)], check=True,
                            capture_output=True, text=True).stdout
    exports = {}
    for line in output.splitlines():
        match = FORWARDED.match(line) or EXPORTED.match(line)
        if match:
            exports[match['name']] = (int(match['ordinal']), match.groupdict().get('forward'))
    if not exports:
        sys.exit(f'{path}: dumpbin reported no exports')
    return exports


def describe(entry):
    ordinal, forward = entry
    return f'@{ordinal}' + (f' forwarded to {forward}' if forward else '')


def check_module(dumpbin, dll, definition, cache):
    expected = def_exports(definition)
    actual = dll_exports(dumpbin, dll)
    errors = []
    if len(expected) != len(actual):
        errors.append(f'{dll.name}: {len(actual)} exports, {definition.name} lists {len(expected)}')
    for name in sorted(expected.keys() - actual.keys()):
        errors.append(f'{dll.name}: missing export {name}')
    for name in sorted(actual.keys() - expected.keys()):
        errors.append(f'{dll.name}: unexpected export {name}')
    for name in sorted(expected.keys() & actual.keys()):
        if expected[name] != actual[name]:
            errors.append(f'{dll.name}: {name} is {describe(actual[name])}, {definition.name} expects {describe(expected[name])}')
    for name, (_, forward) in sorted(actual.items()):
        if not forward:
            continue
        module, _, symbol = forward.partition('.')
        target = dll.with_name(module + '.dll')
        if not target.is_file():
            errors.append(f'{dll.name}: {name} is forwarded to {forward}, but {target} does not exist')
            continue
        if target not in cache:
            cache[target] = dll_exports(dumpbin, target)
        if symbol not in cache[target]:
            errors.append(f'{dll.name}: {name} is forwarded to {forward}, which {target.name} does not export')
    return len(actual), errors


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--dumpbin', required=True, help='dumpbin.exe from the MSVC toolchain')
    parser.add_argument('--module', nargs=2, action='append', required=True, metavar=('DLL', 'DEF'),
                        help='built DLL and the .def file it is linked with')
    args = parser.parse_args()
    cache = {}
    errors = []
    for dll, definition in args.module:
        dll = Path(dll)
        if not dll.is_file():
            sys.exit(f'{dll} does not exist; build it before running this test')
        count, module_errors = check_module(args.dumpbin, dll, Path(definition), cache)
        errors += module_errors
        if not module_errors:
            print(f'PASS: {dll.name} exports the {count} entries of {Path(definition).name}')
    if errors:
        sys.exit('\n'.join(['FAIL:'] + errors))


if __name__ == '__main__':
    main()
