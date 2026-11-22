# Prebuilt third-party libraries shipped in the repository.
#
# Each dependency is exposed as an INTERFACE target in the goldsrc:: namespace
# so that modules can link it the same way on every platform.

include_guard(GLOBAL)

if(NOT WIN32)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
	find_package(Threads REQUIRED)
endif()

# SDL2: lib/public/SDL2.lib on Windows, linux/libSDL2-2.0.so.0 on Linux. The
# bundled libraries are 32-bit; a 64-bit build passes its own.
add_library(goldsrc_sdl2 INTERFACE)
add_library(goldsrc::SDL2 ALIAS goldsrc_sdl2)
target_include_directories(goldsrc_sdl2 SYSTEM INTERFACE "${GOLDSRC_ROOT}/external/SDL2")
if(WIN32)
	set(GOLDSRC_SDL2_LIBRARY "${GOLDSRC_ROOT}/lib/public/SDL2.lib" CACHE FILEPATH "SDL2 import library")
	target_link_libraries(goldsrc_sdl2 INTERFACE "${GOLDSRC_SDL2_LIBRARY}")
else()
	set(GOLDSRC_SDL2_LIBRARY "${GOLDSRC_ROOT}/linux/libSDL2-2.0.so.0" CACHE FILEPATH "SDL2 shared library")
	target_link_libraries(goldsrc_sdl2 INTERFACE "${GOLDSRC_SDL2_LIBRARY}")
endif()

# Steam API. The Linux library is optional: without it, Steam symbols are left
# for the game process (which already has libsteam_api.so loaded) to resolve.
# As with SDL2, a Windows x64 build passes its own import library.
add_library(goldsrc_steam_api INTERFACE)
add_library(goldsrc::steam_api ALIAS goldsrc_steam_api)
if(WIN32)
	set(GOLDSRC_STEAM_API_LIBRARY "${GOLDSRC_ROOT}/lib/public/steam_api.lib" CACHE FILEPATH "Steam API import library")
	target_link_libraries(goldsrc_steam_api INTERFACE "${GOLDSRC_STEAM_API_LIBRARY}")
else()
	set(GOLDSRC_STEAM_API_LIBRARY "${GOLDSRC_ROOT}/lib/public/linux32/libsteam_api.so")
	if(EXISTS "${GOLDSRC_STEAM_API_LIBRARY}")
		target_link_libraries(goldsrc_steam_api INTERFACE "${GOLDSRC_STEAM_API_LIBRARY}")
	else()
		message(STATUS "libsteam_api.so not found at ${GOLDSRC_STEAM_API_LIBRARY}; "
			"Steam API symbols are resolved at run time")
	endif()
endif()

# SteamAPIBridge (MetaHookSv/SteamAPIBridge). The server browser uses the Steam client session
# that the HLND2T engine opens in the bridge's host mode. A project that builds the bridge provides
# the SteamAPIBridge target; otherwise GOLDSRC_STEAMAPIBRIDGE_SOURCE_PATH names a bridge checkout
# (which reads the Steamworks SDK from STEAMSDK_SOURCE_PATH). Without either, the server browser
# is not built.
if(NOT TARGET SteamAPIBridge)
	set(GOLDSRC_STEAMAPIBRIDGE_SOURCE_PATH "$ENV{GOLDSRC_STEAMAPIBRIDGE_SOURCE_PATH}" CACHE PATH "SteamAPIBridge source checkout")
	if(GOLDSRC_STEAMAPIBRIDGE_SOURCE_PATH)
		add_subdirectory("${GOLDSRC_STEAMAPIBRIDGE_SOURCE_PATH}" "${CMAKE_BINARY_DIR}/SteamAPIBridge")
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
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	# Linux takes the same libraries from the vcpkg manifest (vcpkg.json), plus
	# OpenAL for voice capture. The engine is 32-bit only, so a 64-bit build of
	# the UI modules looks for none of them.
	find_package(unofficial-brotli CONFIG QUIET)
	find_package(BZip2 QUIET)
	find_package(EXPAT QUIET)
	find_package(Fontconfig QUIET)
	find_package(Freetype QUIET)
	find_package(PNG QUIET)
	find_package(Ogg CONFIG QUIET)
	find_package(Vorbis CONFIG QUIET)
	find_package(unofficial-libvpx CONFIG QUIET)
	find_package(unofficial-libwebm CONFIG QUIET)
	find_package(OpenAL CONFIG QUIET)
	find_package(ZLIB QUIET)
	# Static libraries: each one must come before the libraries it uses.
	set(engine_media_targets
		Fontconfig::Fontconfig
		Freetype::Freetype
		PNG::PNG
		unofficial::brotli::brotlidec
		BZip2::BZip2
		EXPAT::EXPAT
		Vorbis::vorbisfile
		Vorbis::vorbis
		Ogg::ogg
		unofficial::libvpx::libvpx
		unofficial::libwebm::libwebm
		OpenAL::OpenAL
		ZLIB::ZLIB)
	set(GOLDSRC_ENGINE_MEDIA_FOUND TRUE)
	foreach(dependency IN LISTS engine_media_targets)
		if(NOT TARGET ${dependency})
			set(GOLDSRC_ENGINE_MEDIA_FOUND FALSE)
			list(APPEND missing_engine_media ${dependency})
		endif()
	endforeach()
	if(GOLDSRC_ENGINE_MEDIA_FOUND)
		get_target_property(webm_includes unofficial::libwebm::libwebm INTERFACE_INCLUDE_DIRECTORIES)
		target_include_directories(goldsrc_engine_media INTERFACE "${webm_includes}/webm")
		target_link_libraries(goldsrc_engine_media INTERFACE ${engine_media_targets})
	else()
		message(STATUS "Skipping the Linux engine: missing ${missing_engine_media}. "
			"Set VCPKG_ROOT to build its dependencies from vcpkg.json.")
	endif()
endif()

# Copies the prebuilt (32-bit) Linux runtime libraries next to the engine binaries.
if(NOT WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
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
