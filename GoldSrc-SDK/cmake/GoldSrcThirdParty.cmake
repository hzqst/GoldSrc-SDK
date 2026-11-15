# Prebuilt third-party libraries shipped in the repository.
#
# Each dependency is exposed as an INTERFACE target in the goldsrc:: namespace
# so that modules can link it the same way on every platform.

include_guard(GLOBAL)

if(NOT WIN32)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
endif()

# SDL2: lib/public/SDL2.lib on Windows, linux/libSDL2-2.0.so.0 on Linux.
add_library(goldsrc_sdl2 INTERFACE)
add_library(goldsrc::SDL2 ALIAS goldsrc_sdl2)
target_include_directories(goldsrc_sdl2 SYSTEM INTERFACE "${GOLDSRC_ROOT}/external/SDL2")
if(WIN32)
	target_link_libraries(goldsrc_sdl2 INTERFACE "${GOLDSRC_ROOT}/lib/public/SDL2.lib")
else()
	target_link_libraries(goldsrc_sdl2 INTERFACE "${GOLDSRC_ROOT}/linux/libSDL2-2.0.so.0")
endif()

# Steam API. The Linux library is optional: without it, Steam symbols are left
# for the game process (which already has libsteam_api.so loaded) to resolve.
add_library(goldsrc_steam_api INTERFACE)
add_library(goldsrc::steam_api ALIAS goldsrc_steam_api)
if(WIN32)
	target_link_libraries(goldsrc_steam_api INTERFACE "${GOLDSRC_ROOT}/lib/public/steam_api.lib")
else()
	set(GOLDSRC_STEAM_API_LIBRARY "${GOLDSRC_ROOT}/lib/public/linux32/libsteam_api.so")
	if(EXISTS "${GOLDSRC_STEAM_API_LIBRARY}")
		target_link_libraries(goldsrc_steam_api INTERFACE "${GOLDSRC_STEAM_API_LIBRARY}")
	else()
		message(STATUS "libsteam_api.so not found at ${GOLDSRC_STEAM_API_LIBRARY}; "
			"Steam API symbols are resolved at run time")
	endif()
endif()

# VGUI1. Windows builds it from utils/vgui (target "vgui"); Linux links the
# prebuilt linux/release/vgui.so. That library has no SONAME, so it is linked
# by file name to keep DT_NEEDED as a plain "vgui.so".
add_library(goldsrc_vgui1 INTERFACE)
add_library(goldsrc::vgui1 ALIAS goldsrc_vgui1)
target_include_directories(goldsrc_vgui1 INTERFACE "${GOLDSRC_ROOT}/utils/vgui/include")
if(NOT WIN32)
	set(GOLDSRC_VGUI1_DIR "${GOLDSRC_ROOT}/linux/release")
	target_link_directories(goldsrc_vgui1 INTERFACE "${GOLDSRC_VGUI1_DIR}")
	target_link_libraries(goldsrc_vgui1 INTERFACE "-l:vgui.so")
endif()

# Miles Sound System, used by the engine for MP3 playback (Windows only).
add_library(goldsrc_miles INTERFACE)
add_library(goldsrc::miles ALIAS goldsrc_miles)
if(WIN32)
	target_link_libraries(goldsrc_miles INTERFACE "${GOLDSRC_ROOT}/external/miles/mss32.lib")
endif()

# Media and font libraries used by the engine. Windows links the static vcpkg
# packages committed under external/installed/x86-windows-static-md.
add_library(goldsrc_engine_media INTERFACE)
add_library(goldsrc::engine_media ALIAS goldsrc_engine_media)
if(WIN32)
	set(vcpkg_dir "${GOLDSRC_ROOT}/external/installed/x86-windows-static-md")
	set(vcpkg_lib_dir "${vcpkg_dir}/$<$<CONFIG:Debug>:debug/>lib")
	set(debug_suffix "$<$<CONFIG:Debug>:d>")
	target_include_directories(goldsrc_engine_media INTERFACE
		"${vcpkg_dir}/include"
		"${vcpkg_dir}/include/webm")
	target_link_libraries(goldsrc_engine_media INTERFACE
		"${vcpkg_lib_dir}/brotlidec.lib"
		"${vcpkg_lib_dir}/brotlicommon.lib"
		"${vcpkg_lib_dir}/vorbisfile.lib"
		"${vcpkg_lib_dir}/vorbis.lib"
		"${vcpkg_lib_dir}/ogg.lib"
		"${vcpkg_lib_dir}/webm.lib"
		"${vcpkg_lib_dir}/vpx.lib"
		"${vcpkg_lib_dir}/bz2${debug_suffix}.lib"
		"${vcpkg_lib_dir}/freetype${debug_suffix}.lib"
		"${vcpkg_lib_dir}/libexpat${debug_suffix}MD.lib"
		"${vcpkg_lib_dir}/libpng16${debug_suffix}.lib"
		"${vcpkg_lib_dir}/zs${debug_suffix}.lib")
endif()

# Copies the prebuilt Linux runtime libraries next to the engine binaries.
if(NOT WIN32)
	set(runtime_files
		"${GOLDSRC_ROOT}/linux/libSDL2-2.0.so.0"
		"${GOLDSRC_VGUI1_DIR}/vgui.so")
	if(EXISTS "${GOLDSRC_STEAM_API_LIBRARY}")
		list(APPEND runtime_files "${GOLDSRC_STEAM_API_LIBRARY}")
	endif()
	add_custom_target(goldsrc_runtime ALL
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${GOLDSRC_BIN_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${runtime_files} "${GOLDSRC_BIN_DIR}"
		COMMENT "Copying prebuilt Linux runtime libraries"
		VERBATIM)
endif()
