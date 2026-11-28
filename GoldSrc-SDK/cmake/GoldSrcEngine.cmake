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

	# The Linux engine needs the vcpkg libraries (see GoldSrcThirdParty.cmake).
	if(NOT WIN32 AND NOT GOLDSRC_ENGINE_MEDIA_FOUND)
		return()
	endif()

	include("${GOLDSRC_ROOT}/engine/engine_sources.cmake")

	if(arg_RENDERER STREQUAL "HW")
		set(renderer_sources ${ENGINE_HW_SOURCES})
		set(renderer_define HL25_HW_ENGINE)
		set(renderer_windows_link opengl32)
		set(renderer_linux_link GL)
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
		set(engine_def_file "${GOLDSRC_ROOT}/${arg_DEF_FILE}")
		if(MSVC AND NOT GOLDSRC_VGUI_ABI_COMPAT_MODE)
			set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${engine_def_file}")
			file(READ "${engine_def_file}" engine_exports)

			# The ownership-aware classes no longer generate these implicit moves.
			set(move_classes "Bitmap|Cursor|Font|Frame|Image|Panel|Scheme")
			string(REGEX REPLACE
				"    [?][?](0(${move_classes})@vgui@@QAE|4(${move_classes})@vgui@@QAEAAV01)@[$][$]QAV01@@Z @[0-9]+\n"
				"" engine_exports "${engine_exports}")

			# These vtables are now emitted in vgui.dll. Keep their export ordinals.
			set(vtable_classes "Bitmap|Cursor|Font|Frame|Panel|Scheme")
			string(REGEX REPLACE
				"([?][?]_7(${vtable_classes})@vgui@@6B@)( @[0-9]+ DATA)"
				"\\1=vgui.\\1\\3" engine_exports "${engine_exports}")
			# MSVC emits these destructors in the engine for the ownership-aware bases.
			string(APPEND engine_exports
				"    ??1App@vgui@@QAE@XZ @369\n"
				"    ??1BitmapTGA@vgui@@UAE@XZ @370\n"
				"    ??1ImagePanel@vgui@@UAE@XZ @371\n")

			set(engine_def_file "${CMAKE_CURRENT_BINARY_DIR}/${target}.def")
			file(GENERATE OUTPUT "${engine_def_file}" CONTENT "${engine_exports}")
			set_source_files_properties("${engine_def_file}" PROPERTIES GENERATED TRUE)
		endif()
		list(APPEND options DEF_FILE "${engine_def_file}")
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
			${ENGINE_WINDOWS_SOURCES}
			engine/engine.rc
		LINUX_SOURCES
			${ENGINE_LINUX_SOURCES}
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
		LINUX_LINK
			${renderer_linux_link}
			${CMAKE_DL_LIBS}
			Threads::Threads
		MSVC_LINK_OPTIONS
			$<$<CONFIG:Release>:/OPT:NOICF>
			${arg_MSVC_LINK_OPTIONS}
	)
	if(arg_DEF_FILE AND TARGET ${target})
		set_property(TARGET ${target} PROPERTY GOLDSRC_ENGINE_DEF_FILE "${engine_def_file}")
	endif()

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
