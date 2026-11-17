"""Compile the C++ programs built by tests/*_tests.py.

The compiler is chosen in this order:
- CXX, as a gcc-style driver command line (for example CXX='c++ -m32'), or as
  MSVC when its first word is cl / cl.exe (the caller provides the vcvars
  environment);
- on Windows, cl from a Visual Studio developer environment, or the latest
  Visual Studio with the C++ x64 tools found through vswhere;
- clang++ on Windows without Visual Studio, c++ elsewhere.

Requested sanitizers are probed, and the largest supported subset is enabled.
MSVC only supports AddressSanitizer; its runtime DLL is copied next to the
executable.
"""
import functools
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile

MSVC_ARCH = 'x64'
MSVC_COMPONENT = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
VSWHERE = (Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)'))
           / 'Microsoft Visual Studio/Installer/vswhere.exe')
# Virtual calls and std::string pull in the C++ parts of the sanitizer runtimes,
# which a C-only probe would link without.
PROBE_SOURCE = r'''#include <string>
struct Base { virtual ~Base() {} virtual int Value() { return 1; } };
struct Derived : Base { int Value() override { return 0; } };
int main()
{
 Base* object = new Derived;
 const std::string text = "probe";
 const int result = object->Value() + static_cast<int>(text.size()) - 5;
 delete object;
 return result;
}
'''


class Toolchain:
    def __init__(self, name, command, msvc=False, env=None):
        self.name = name
        self.command = command
        self.msvc = msvc
        self.env = env

    def executable(self, output):
        output = Path(output)
        return output.with_suffix('.exe') if os.name == 'nt' and not output.suffix else output

    def arguments(self, sources, executable, includes, defines, debug, sanitizers, strict_warnings):
        if self.msvc:
            arguments = ['/nologo', '/EHsc', '/std:c++17', '/permissive-', '/Zc:preprocessor']
            arguments += [f'/I{include}' for include in includes] + [f'/D{define}' for define in defines]
            # AddressSanitizer needs debug information to report source lines.
            if debug or 'address' in sanitizers:
                arguments += ['/Zi', f'/Fd:{executable.with_suffix(".pdb")}']
            if strict_warnings:
                arguments += ['/W4', '/WX']
            if 'address' in sanitizers:
                arguments.append('/fsanitize=address')
            return arguments + [f'/Fo:{executable.parent}{os.sep}'] + sources + [f'/Fe:{executable}']
        arguments = ['-std=c++17'] + [f'-I{include}' for include in includes] + [f'-D{define}' for define in defines]
        if debug:
            arguments.append('-g')
        if strict_warnings:
            arguments += ['-Wall', '-Wextra', '-Werror']
        if sanitizers:
            arguments.append('-fsanitize=' + ','.join(sanitizers))
        if 'undefined' in sanitizers:
            arguments.append('-fno-sanitize-recover=all')
        return arguments + sources + ['-o', str(executable)]

    def run(self, sources, output, includes=(), defines=(), debug=False, sanitizers=(), strict_warnings=False,
            check=True):
        executable = self.executable(output)
        command = self.command + self.arguments([str(source) for source in sources], executable, includes, defines,
                                                debug, sanitizers, strict_warnings)
        result = subprocess.run(command, env=self.env, check=check,
                                stdout=None if check else subprocess.DEVNULL,
                                stderr=None if check else subprocess.DEVNULL)
        if result.returncode == 0 and self.msvc and 'address' in sanitizers:
            self.copy_asan_runtime(executable.parent)
        return executable if result.returncode == 0 else None

    def copy_asan_runtime(self, folder):
        env = self.env or os.environ
        if 'VCToolsInstallDir' not in env:
            return
        # vcvars records its architectures; a developer prompt may target x86.
        host = env.get('VSCMD_ARG_HOST_ARCH', MSVC_ARCH)
        target = env.get('VSCMD_ARG_TGT_ARCH', MSVC_ARCH)
        for runtime in (Path(env['VCToolsInstallDir']) / 'bin' / f'Host{host}' / target).glob(
                'clang_rt.asan_dynamic-*.dll'):
            shutil.copy(runtime, folder)


def environment_path(env):
    return next((value for key, value in env.items() if key.upper() == 'PATH'), '')


def vcvars_environment():
    if not VSWHERE.exists():
        return None
    root = subprocess.run([str(VSWHERE), '-latest', '-products', '*', '-requires', MSVC_COMPONENT,
                           '-property', 'installationPath'], capture_output=True, text=True).stdout.strip()
    vcvars = Path(root) / 'VC/Auxiliary/Build/vcvarsall.bat'
    if not root or not vcvars.exists():
        return None
    # cmd /u writes the environment as UTF-16 so non-ASCII paths survive.
    result = subprocess.run(f'cmd /u /c ""{vcvars}" {MSVC_ARCH} >nul && set"', capture_output=True)
    if result.returncode:
        return None
    lines = result.stdout.decode('utf-16-le').splitlines()
    return dict(line.split('=', 1) for line in lines if '=' in line and not line.startswith('='))


@functools.lru_cache(maxsize=None)
def toolchain():
    cxx = os.environ.get('CXX')
    if cxx:
        command = shlex.split(cxx)
        if Path(command[0]).name.lower() in ('cl', 'cl.exe'):
            return Toolchain('MSVC', command, msvc=True)
        return Toolchain(command[0], command)
    if os.name != 'nt':
        return Toolchain('c++', ['c++'])
    if 'VCToolsInstallDir' in os.environ and shutil.which('cl'):
        return Toolchain('MSVC', [shutil.which('cl')], msvc=True)
    env = vcvars_environment()
    cl = shutil.which('cl', path=environment_path(env)) if env else None
    if cl:
        return Toolchain('MSVC', [cl], msvc=True, env=env)
    return Toolchain('clang++', ['clang++'])


@functools.lru_cache(maxsize=None)
def supported_sanitizers(requested):
    tools = toolchain()
    if tools.msvc:
        requested = tuple(sanitizer for sanitizer in requested if sanitizer == 'address')
    candidates = [requested] + [(sanitizer,) for sanitizer in requested] if requested else []
    enabled = ()
    with tempfile.TemporaryDirectory(prefix='goldsrc-cxx-probe-') as folder:
        probe = Path(folder) / 'probe.cpp'
        probe.write_text(PROBE_SOURCE)
        for candidate in candidates:
            executable = tools.run([probe], Path(folder) / 'probe', sanitizers=candidate, check=False)
            if executable and subprocess.run([str(executable)], stdout=subprocess.DEVNULL,
                                             stderr=subprocess.DEVNULL).returncode == 0:
                enabled = candidate
                break
    print(f'C++ toolchain: {tools.name}; sanitizers: {", ".join(enabled) or "none"}', file=sys.stderr)
    return enabled


def compile_program(sources, output, includes=(), defines=(), debug=False, sanitizers=(), strict_warnings=False):
    """Compile C++17 sources into an executable and return its path.

    sanitizers lists the wanted sanitizers ('address', 'undefined'); only the
    ones the toolchain supports are enabled.
    """
    return toolchain().run(sources, output, includes, defines, debug, supported_sanitizers(tuple(sanitizers)),
                           strict_warnings)
