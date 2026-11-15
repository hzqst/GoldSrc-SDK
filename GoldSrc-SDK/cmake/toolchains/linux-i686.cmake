# Toolchain for 32-bit x86 Linux builds with the host GCC/Clang multilib.
#
# GoldSrc binaries are 32-bit only. Requires gcc-multilib/g++-multilib (or the
# Steam Runtime toolchain) so that -m32 can find the i386 C/C++ runtime.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_FLAGS_INIT "-m32")
set(CMAKE_CXX_FLAGS_INIT "-m32")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m32")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-m32")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-m32")

# Search the Debian/Ubuntu multiarch directories for i386 system libraries.
set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
