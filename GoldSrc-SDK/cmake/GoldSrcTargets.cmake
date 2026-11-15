# Helpers that declare GoldSrc targets.
#
# All source, include and definition-file paths given to these helpers are
# relative to the GoldSrc-SDK source root (GOLDSRC_ROOT), not to the calling
# CMakeLists.txt, because most modules compile files from several top-level
# directories.

include_guard(GLOBAL)

# goldsrc_root_paths(<out-var> <path>...)
#
# Resolves paths relative to GOLDSRC_ROOT; absolute paths and generator
# expressions are passed through unchanged.
function(goldsrc_root_paths out_var)
	set(result)
	foreach(path IN LISTS ARGN)
		if(IS_ABSOLUTE "${path}" OR path MATCHES "^\\$<")
			list(APPEND result "${path}")
		else()
			list(APPEND result "${GOLDSRC_ROOT}/${path}")
		endif()
	endforeach()
	set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# goldsrc_add_module(<target>
#   TYPE <SHARED|EXECUTABLE|STATIC>
#   [WINDOWS_ONLY]              Not built on other platforms.
#   [OUTPUT_NAME <name>]        File name without extension (default: <target>).
#   [LINUX_OUTPUT_NAME <name>]  File name on Linux, if different.
#   [OUTPUT_DIR <dir>]          Directory below bin/<platform>/<config>/.
#   [GUI]                       Windows subsystem executable (WinMain).
#   [CXX_STANDARD <14|17>]      Default 14, as with the MSVC default.
#   [WARNING_LEVEL <3|4>]       MSVC warning level (default 3).
#   [WARNINGS_AS_ERRORS]        MSVC /WX.
#   [LINKER_WARNINGS_AS_ERRORS] MSVC /WX for the linker.
#   [RUNTIME <DLL|STATIC>]      MSVC C runtime (default DLL).
#   [EXCEPTIONS <SYNC|ASYNC|NONE>]  MSVC exception model (default SYNC).
#   [NO_RTTI]
#   [FLOAT_MODEL <PRECISE|STRICT|FAST>]  MSVC floating point model.
#   [NO_SSE]                    MSVC /arch:IA32.
#   [CHARSET <MBCS|UNICODE>]
#   [KEEP_ASSERTS]              Release builds keep assert() (no NDEBUG).
#   [OPTIMIZE_SIZE]             Release builds optimize for size.
#   [WHOLE_PROGRAM]             MSVC link-time code generation in Release.
#   [LARGE_ADDRESS_AWARE]
#   [MFC <STATIC|SHARED>]       MFC linkage; STATIC also needs RUNTIME STATIC.
#   [DEF_FILE <file>]           MSVC module-definition file.
#   [EXPORTS <symbol>...]       Additional MSVC /EXPORT entries.
#   [HIDDEN_VISIBILITY]         Hide symbols by default on ELF platforms.
#   [SOURCES ...] [WINDOWS_SOURCES ...] [LINUX_SOURCES ...]
#   [INCLUDES ...] [WINDOWS_INCLUDES ...] [LINUX_INCLUDES ...]
#   [DEFINES ...] [WINDOWS_DEFINES ...] [LINUX_DEFINES ...]
#   [LINK ...] [WINDOWS_LINK ...] [LINUX_LINK ...]
#   [MSVC_OPTIONS ...] [MSVC_LINK_OPTIONS ...]
#   [GCC_OPTIONS ...] [GCC_LINK_OPTIONS ...])
function(goldsrc_add_module target)
	set(flags WINDOWS_ONLY GUI WARNINGS_AS_ERRORS LINKER_WARNINGS_AS_ERRORS NO_RTTI NO_SSE
		KEEP_ASSERTS OPTIMIZE_SIZE WHOLE_PROGRAM LARGE_ADDRESS_AWARE HIDDEN_VISIBILITY)
	set(one_value TYPE OUTPUT_NAME LINUX_OUTPUT_NAME OUTPUT_DIR CXX_STANDARD WARNING_LEVEL RUNTIME
		EXCEPTIONS FLOAT_MODEL CHARSET MFC DEF_FILE)
	set(multi_value EXPORTS
		SOURCES WINDOWS_SOURCES LINUX_SOURCES
		INCLUDES WINDOWS_INCLUDES LINUX_INCLUDES
		DEFINES WINDOWS_DEFINES LINUX_DEFINES
		LINK WINDOWS_LINK LINUX_LINK
		MSVC_OPTIONS MSVC_LINK_OPTIONS GCC_OPTIONS GCC_LINK_OPTIONS)
	cmake_parse_arguments(PARSE_ARGV 1 arg "${flags}" "${one_value}" "${multi_value}")

	if(arg_UNPARSED_ARGUMENTS)
		message(FATAL_ERROR "goldsrc_add_module(${target}): unknown arguments ${arg_UNPARSED_ARGUMENTS}")
	endif()
	if(arg_WINDOWS_ONLY AND NOT WIN32)
		return()
	endif()

	set(sources ${arg_SOURCES})
	set(includes ${arg_INCLUDES})
	set(defines ${arg_DEFINES})
	set(link ${arg_LINK})
	if(WIN32)
		list(APPEND sources ${arg_WINDOWS_SOURCES})
		list(APPEND includes ${arg_WINDOWS_INCLUDES})
		list(APPEND defines ${arg_WINDOWS_DEFINES})
		list(APPEND link ${arg_WINDOWS_LINK})
		if(MSVC AND arg_DEF_FILE)
			list(APPEND sources ${arg_DEF_FILE})
		endif()
	else()
		list(APPEND sources ${arg_LINUX_SOURCES})
		list(APPEND includes ${arg_LINUX_INCLUDES})
		list(APPEND defines ${arg_LINUX_DEFINES})
		list(APPEND link ${arg_LINUX_LINK})
	endif()
	goldsrc_root_paths(sources ${sources})
	goldsrc_root_paths(includes ${includes})

	if(arg_TYPE STREQUAL "SHARED")
		add_library(${target} SHARED ${sources})
	elseif(arg_TYPE STREQUAL "STATIC")
		add_library(${target} STATIC ${sources})
	elseif(arg_TYPE STREQUAL "EXECUTABLE")
		if(arg_GUI)
			add_executable(${target} WIN32 ${sources})
		else()
			add_executable(${target} ${sources})
		endif()
	else()
		message(FATAL_ERROR "goldsrc_add_module(${target}): TYPE must be SHARED, STATIC or EXECUTABLE")
	endif()

	target_include_directories(${target} PRIVATE ${includes})
	target_compile_definitions(${target} PRIVATE ${defines})
	target_link_libraries(${target} PRIVATE ${link})

	set(output_name "${target}")
	if(arg_OUTPUT_NAME)
		set(output_name "${arg_OUTPUT_NAME}")
	endif()
	if(NOT WIN32 AND arg_LINUX_OUTPUT_NAME)
		set(output_name "${arg_LINUX_OUTPUT_NAME}")
	endif()

	set(output_dir "${GOLDSRC_BIN_DIR}")
	if(arg_OUTPUT_DIR)
		string(APPEND output_dir "/${arg_OUTPUT_DIR}")
	endif()
	set(obj_dir "${GOLDSRC_OBJ_DIR}/${target}/${GOLDSRC_PLATFORM_DIR}/$<CONFIG>")

	set(cxx_standard 14)
	if(arg_CXX_STANDARD)
		set(cxx_standard ${arg_CXX_STANDARD})
	endif()

	set_target_properties(${target} PROPERTIES
		OUTPUT_NAME "${output_name}"
		RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
		LIBRARY_OUTPUT_DIRECTORY "${output_dir}"
		ARCHIVE_OUTPUT_DIRECTORY "${obj_dir}"
		PDB_OUTPUT_DIRECTORY "${obj_dir}"
		COMPILE_PDB_OUTPUT_DIRECTORY "${obj_dir}"
		CXX_STANDARD ${cxx_standard}
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS ON
		# The C sources predate C23, which makes bool/true/false keywords.
		C_STANDARD 11
		C_EXTENSIONS ON
	)
	if(NOT WIN32)
		# Game and engine modules are loaded by file name, never as lib<name>.so.
		set_target_properties(${target} PROPERTIES PREFIX "")
		if(arg_HIDDEN_VISIBILITY)
			set_target_properties(${target} PROPERTIES
				C_VISIBILITY_PRESET hidden
				CXX_VISIBILITY_PRESET hidden
				VISIBILITY_INLINES_HIDDEN ON)
		endif()
		if(NOT arg_TYPE STREQUAL "STATIC")
			# Binaries are used straight from bin/, so keep the build tree out of
			# the run path; dependencies are found next to the binary or are
			# already loaded by the engine.
			set_target_properties(${target} PROPERTIES
				BUILD_WITH_INSTALL_RPATH ON
				INSTALL_RPATH "$ORIGIN")
		endif()
	endif()

	if(MSVC AND arg_WHOLE_PROGRAM)
		set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
	endif()

	if(arg_KEEP_ASSERTS)
		# CMake's default Release flags define NDEBUG; these targets keep asserts.
		if(MSVC)
			target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/UNDEBUG>)
		else()
			target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:-UNDEBUG>)
		endif()
	endif()

	if(arg_CHARSET STREQUAL "MBCS")
		target_compile_definitions(${target} PRIVATE _MBCS)
	elseif(arg_CHARSET STREQUAL "UNICODE")
		target_compile_definitions(${target} PRIVATE UNICODE _UNICODE)
	endif()

	if(MSVC)
		_goldsrc_apply_msvc_settings(${target})
	else()
		target_compile_options(${target} PRIVATE ${arg_GCC_OPTIONS})
		target_link_options(${target} PRIVATE ${arg_GCC_LINK_OPTIONS})
	endif()
endfunction()

# Applies the MSVC-specific options parsed by goldsrc_add_module(). Called from
# goldsrc_add_module() only, so it reads the caller's arg_* variables.
macro(_goldsrc_apply_msvc_settings target)
	set(_warning_level 3)
	if(arg_WARNING_LEVEL)
		set(_warning_level ${arg_WARNING_LEVEL})
	endif()
	target_compile_options(${target} PRIVATE /W${_warning_level})
	if(arg_WARNINGS_AS_ERRORS)
		target_compile_options(${target} PRIVATE /WX)
	endif()
	if(arg_LINKER_WARNINGS_AS_ERRORS)
		target_link_options(${target} PRIVATE /WX)
	endif()

	if(arg_RUNTIME STREQUAL "STATIC")
		set_target_properties(${target} PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()

	if(arg_EXCEPTIONS STREQUAL "ASYNC")
		target_compile_options(${target} PRIVATE /EHa)
	elseif(NOT arg_EXCEPTIONS STREQUAL "NONE")
		target_compile_options(${target} PRIVATE /EHsc)
	endif()
	if(arg_NO_RTTI)
		target_compile_options(${target} PRIVATE /GR-)
	endif()

	if(arg_FLOAT_MODEL STREQUAL "STRICT")
		target_compile_options(${target} PRIVATE /fp:strict)
	elseif(arg_FLOAT_MODEL STREQUAL "FAST")
		target_compile_options(${target} PRIVATE /fp:fast)
	else()
		target_compile_options(${target} PRIVATE /fp:precise)
	endif()
	if(arg_NO_SSE)
		target_compile_options(${target} PRIVATE /arch:IA32)
	endif()

	if(arg_OPTIMIZE_SIZE)
		target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/O1>)
	else()
		target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/O2>)
	endif()

	# MFC headers select their libraries through #pragma comment(lib); the
	# shared MFC build is chosen by _AFXDLL and must pair with the DLL runtime.
	if(arg_MFC STREQUAL "SHARED")
		target_compile_definitions(${target} PRIVATE _AFXDLL)
	endif()

	if(arg_LARGE_ADDRESS_AWARE)
		target_link_options(${target} PRIVATE /LARGEADDRESSAWARE)
	endif()
	foreach(_symbol IN LISTS arg_EXPORTS)
		target_link_options(${target} PRIVATE "/EXPORT:${_symbol}")
	endforeach()

	target_compile_options(${target} PRIVATE ${arg_MSVC_OPTIONS})
	target_link_options(${target} PRIVATE ${arg_MSVC_LINK_OPTIONS})
endmacro()

# goldsrc_source_properties(<file>...
#   [DEFINES ...] [INCLUDES ...] [MSVC_OPTIONS ...] [GCC_OPTIONS ...]
#   [LANGUAGE <C|CXX>])
#
# Per-file settings for targets created in the calling directory.
function(goldsrc_source_properties)
	cmake_parse_arguments(PARSE_ARGV 0 arg "" "LANGUAGE" "DEFINES;INCLUDES;MSVC_OPTIONS;GCC_OPTIONS")
	goldsrc_root_paths(files ${arg_UNPARSED_ARGUMENTS})
	goldsrc_root_paths(includes ${arg_INCLUDES})
	if(arg_DEFINES)
		set_property(SOURCE ${files} APPEND PROPERTY COMPILE_DEFINITIONS ${arg_DEFINES})
	endif()
	if(includes)
		set_property(SOURCE ${files} APPEND PROPERTY INCLUDE_DIRECTORIES ${includes})
	endif()
	if(MSVC AND arg_MSVC_OPTIONS)
		set_property(SOURCE ${files} APPEND PROPERTY COMPILE_OPTIONS ${arg_MSVC_OPTIONS})
	endif()
	if(NOT MSVC AND arg_GCC_OPTIONS)
		set_property(SOURCE ${files} APPEND PROPERTY COMPILE_OPTIONS ${arg_GCC_OPTIONS})
	endif()
	if(arg_LANGUAGE)
		set_property(SOURCE ${files} PROPERTY LANGUAGE ${arg_LANGUAGE})
	endif()
endfunction()
