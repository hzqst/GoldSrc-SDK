cmake_minimum_required(VERSION 3.25)

project(GoldSrcSDK LANGUAGES C CXX)

if(WIN32)
	enable_language(RC)
endif()

get_property(GOLDSRC_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GOLDSRC_MULTI_CONFIG)
	set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "Build configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration (Debug or Release)" FORCE)
endif()

set(GOLDSRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${GOLDSRC_ROOT}/cmake")

option(GOLDSRC_BUILD_UTILS "Build the SDK utilities" ON)
option(GOLDSRC_BUILD_TESTS "Build and register the regression tests in ../tests" OFF)
option(GOLDSRC_VGUI_ABI_COMPAT_MODE "Keep VGUI1 binary compatible with the original vgui.dll" ON)

if(GOLDSRC_VGUI_ABI_COMPAT_MODE)
	add_compile_definitions(VGUI_ABI_COMPAT_MODE=1)
else()
	if(NOT WIN32)
		message(FATAL_ERROR "GOLDSRC_VGUI_ABI_COMPAT_MODE=OFF needs vgui built from source, "
			"but Linux links the prebuilt linux/release/vgui.so")
	endif()
	add_compile_definitions(VGUI_ABI_COMPAT_MODE=0)
endif()

option(GOLDSRC_STUDIO_BOUNDS_COMPAT_MODE "Keep R_StudioComputeBounds compatible with the original engine bone-0 animation bug for force_model_samebounds" OFF)
if(GOLDSRC_STUDIO_BOUNDS_COMPAT_MODE)
	add_compile_definitions(STUDIO_BOUNDS_COMPAT_MODE=1)
else()
	add_compile_definitions(STUDIO_BOUNDS_COMPAT_MODE=0)
endif()

include(GoldSrcPlatform)
include(GoldSrcTargets)
include(GoldSrcThirdParty)

# Runtime libraries
add_subdirectory(tier0)
add_subdirectory(filesystem)
add_subdirectory(utils/vgui)
add_subdirectory(vgui2)
add_subdirectory(a3dapi)

# Engines and launchers
add_subdirectory(engine)
add_subdirectory(swds)
add_subdirectory(launcher)
add_subdirectory(dedicated)

# HLTV
add_subdirectory(HLTV/Core)
add_subdirectory(HLTV/DemoPlayer)
add_subdirectory(HLTV/Proxy)
add_subdirectory(HLTV/Console)

# UI modules
add_subdirectory(GameUI)
add_subdirectory(serverbrowser)
add_subdirectory(particleman)

# Games
add_subdirectory(dlls)
add_subdirectory(cl_dll)
add_subdirectory(gearbox)
add_subdirectory(bshift)
add_subdirectory(cstrike)
add_subdirectory(czeror)
add_subdirectory(tfc)
add_subdirectory(dod)
add_subdirectory(dmc)
add_subdirectory(ricochet)

# Tools
if(GOLDSRC_BUILD_UTILS)
	add_subdirectory(utils)
	add_subdirectory(worldcraft)
endif()

if(GOLDSRC_BUILD_TESTS)
	enable_testing()
	include(GoldSrcTests)
endif()
