# Shared definition of the engine modules (hw, sw and swds), which compile the
# engine/ sources with a renderer-specific source list and define.

include_guard(GLOBAL)

# goldsrc_add_engine(<target>
#   RENDERER <HW|SW>
#   OUTPUT_NAME <name>
#   [WINDOWS_ONLY]
#   [DEF_FILE <file>]
#   [MSVC_LINK_OPTIONS ...])
function(goldsrc_add_engine target)
	cmake_parse_arguments(PARSE_ARGV 1 arg "WINDOWS_ONLY" "RENDERER;OUTPUT_NAME;DEF_FILE" "MSVC_LINK_OPTIONS")

	include("${GOLDSRC_ROOT}/engine/engine_sources.cmake")

	if(arg_RENDERER STREQUAL "HW")
		set(renderer_sources ${ENGINE_HW_SOURCES})
		set(renderer_define HL25_HW_ENGINE)
		set(renderer_windows_link opengl32)
	elseif(arg_RENDERER STREQUAL "SW")
		set(renderer_sources ${ENGINE_SW_SOURCES})
		set(renderer_define SW_RENDERER)
		set(renderer_windows_link ddraw)
	else()
		message(FATAL_ERROR "goldsrc_add_engine(${target}): RENDERER must be HW or SW")
	endif()

	set(options)
	if(arg_WINDOWS_ONLY)
		list(APPEND options WINDOWS_ONLY)
	endif()
	if(arg_DEF_FILE)
		list(APPEND options DEF_FILE ${arg_DEF_FILE})
	endif()

	goldsrc_add_module(${target}
		TYPE SHARED
		${options}
		OUTPUT_NAME ${arg_OUTPUT_NAME}
		CXX_STANDARD 17
		WARNING_LEVEL 4
		CHARSET MBCS
		WARNINGS_AS_ERRORS
		LINKER_WARNINGS_AS_ERRORS
		KEEP_ASSERTS
		OPTIMIZE_SIZE
		SOURCES
			${ENGINE_COMMON_SOURCES}
			${renderer_sources}
		WINDOWS_SOURCES
			engine/engine.rc
		INCLUDES
			engine
			common
			cl_dll
			public
			pm_shared
			utils/vgui/include
		DEFINES
			${renderer_define}
		LINK
			goldsrc::engine_media
			goldsrc::miles
			goldsrc::SDL2
			goldsrc::steam_api
			goldsrc::vgui1
		WINDOWS_LINK
			${renderer_windows_link}
			wsock32
			winmm
			dxguid
			dsound
		MSVC_LINK_OPTIONS
			$<$<CONFIG:Release>:/OPT:NOICF>
			${arg_MSVC_LINK_OPTIONS}
	)

	# Source properties are per directory; set them once for all engines in it.
	get_property(have_source_properties DIRECTORY PROPERTY GOLDSRC_ENGINE_SOURCE_PROPERTIES SET)
	if(NOT have_source_properties)
		set_property(DIRECTORY PROPERTY GOLDSRC_ENGINE_SOURCE_PROPERTIES ON)
		goldsrc_source_properties(engine/sys_dll2.cpp
			MSVC_OPTIONS /EHa
		)
		goldsrc_source_properties(engine/common.cpp
			MSVC_OPTIONS /fp:strict
		)
		goldsrc_source_properties(engine/VGUI_EngineSurfaceWrap.cpp
			INCLUDES utils/vgui/include
		)
	endif()
endfunction()
