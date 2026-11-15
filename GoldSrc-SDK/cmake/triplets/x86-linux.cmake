# 32-bit Linux triplet for the engine's third-party libraries.
#
# The libraries are linked statically into shared engine modules, so they are
# built as position-independent code with the same x87 settings as the engine.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

set(VCPKG_C_FLAGS "-march=pentium-m -mfpmath=387 -fPIC")
set(VCPKG_CXX_FLAGS "-march=pentium-m -mfpmath=387 -fPIC")

# SDL2 replaces the prebuilt linux/libSDL2-2.0.so.0, so it is a shared library.
# Its X11 and audio backends come from the system's i386 development packages.
if(PORT STREQUAL "sdl2")
	set(VCPKG_LIBRARY_LINKAGE dynamic)
	set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DCMAKE_LIBRARY_ARCHITECTURE=i386-linux-gnu)
	set(ENV{PKG_CONFIG_PATH} "/usr/lib/i386-linux-gnu/pkgconfig")
endif()
