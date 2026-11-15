# A3D audio API.

# Windows-only: COM server wrapping DirectSound.
if(NOT WIN32)
	return()
endif()

# A3dGeom.cpp is compiled once per geometry section, each with its own code
# generation settings. Every section gets a small wrapper translation unit.
set(geometry_sections OBJECTS FINALIZE ACOUSTICS SUBMISSION STATE)
set(geometry_sources)
foreach(section IN LISTS geometry_sections)
	string(TOLOWER "${section}" name)
	set(file "${CMAKE_CURRENT_BINARY_DIR}/geometry_${name}.cpp")
	file(CONFIGURE OUTPUT "${file}"
		CONTENT "#define A3D_GEOMETRY_${section}\n#include \"${GOLDSRC_ROOT}/a3dapi/A3dGeom.cpp\"\n"
		@ONLY)
	list(APPEND geometry_sources "${file}")
endforeach()

goldsrc_add_module(a3dapi
	TYPE SHARED
	CXX_STANDARD 17
	WARNING_LEVEL 4
	RUNTIME STATIC
	CHARSET MBCS
	DEF_FILE a3dapi/a3dapi.def
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	SOURCES
		a3dapi/A3d3.cpp
		a3dapi/a3d_guids.cpp
		a3dapi/resman.cpp
		a3dapi/dal_a2d.cpp
		a3dapi/dalinfo.cpp
		a3dapi/A3dGeom.cpp
		a3dapi/A3dScene.cpp
		a3dapi/apimapper.cpp
		a3dapi/A3dSource.cpp
		a3dapi/matrix_inverse.cpp
		a3dapi/a2d.cpp
		a3dapi/source_reflections.cpp
		a3dapi/a2dbuffer.cpp
		a3dapi/source_geometry.cpp
		a3dapi/spatial_audio.cpp
		a3dapi/rotation.cpp
		${geometry_sources}
		a3dapi/a3dapi.rc
	INCLUDES
		a3dapi
	DEFINES
		WIN32_LEAN_AND_MEAN
		NOMINMAX
	LINK
		version
		winmm
		dxguid
	MSVC_LINK_OPTIONS
		/IGNORE:4222
		/BASE:0x10000000
)

# Files that are always optimized, even in Debug builds. /O2 cannot be combined
# with run-time checks, so /RTC1 is removed from the directory's Debug flags and
# added back to every other file.
string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
set(optimized_sources
	a3dapi/a2d.cpp
	a3dapi/source_reflections.cpp
	a3dapi/a2dbuffer.cpp
	a3dapi/source_geometry.cpp
	a3dapi/spatial_audio.cpp
	a3dapi/rotation.cpp
	"${CMAKE_CURRENT_BINARY_DIR}/geometry_finalize.cpp"
	"${CMAKE_CURRENT_BINARY_DIR}/geometry_acoustics.cpp"
	"${CMAKE_CURRENT_BINARY_DIR}/geometry_submission.cpp"
	"${CMAKE_CURRENT_BINARY_DIR}/geometry_state.cpp"
)
goldsrc_source_properties(
	a3dapi/A3d3.cpp
	a3dapi/a3d_guids.cpp
	a3dapi/resman.cpp
	a3dapi/dal_a2d.cpp
	a3dapi/dalinfo.cpp
	a3dapi/A3dGeom.cpp
	a3dapi/A3dScene.cpp
	a3dapi/apimapper.cpp
	a3dapi/A3dSource.cpp
	a3dapi/matrix_inverse.cpp
	"${CMAKE_CURRENT_BINARY_DIR}/geometry_objects.cpp"
	MSVC_OPTIONS $<$<CONFIG:Debug>:/RTC1>
)
goldsrc_source_properties(${optimized_sources}
	MSVC_OPTIONS /O2
)

goldsrc_source_properties(a3dapi/A3dGeom.cpp
	DEFINES A3D_GEOMETRY_CORE
	MSVC_OPTIONS /arch:IA32 /fp:precise
)
goldsrc_source_properties(a3dapi/A3dScene.cpp
	MSVC_OPTIONS /arch:IA32 /fp:precise
)
goldsrc_source_properties(a3dapi/apimapper.cpp a3dapi/A3dSource.cpp a3dapi/matrix_inverse.cpp
	MSVC_OPTIONS /arch:IA32 /fp:strict
)
goldsrc_source_properties(a3dapi/a2d.cpp a3dapi/source_reflections.cpp
	MSVC_OPTIONS /Oi /arch:IA32 /fp:precise
)
goldsrc_source_properties(a3dapi/a2dbuffer.cpp a3dapi/source_geometry.cpp
	MSVC_OPTIONS /arch:IA32 /fp:precise
)
goldsrc_source_properties(a3dapi/spatial_audio.cpp
	MSVC_OPTIONS /Oi /arch:IA32 /fp:strict
)
goldsrc_source_properties(a3dapi/rotation.cpp
	MSVC_OPTIONS /Oi /arch:IA32 /fp:fast
)
goldsrc_source_properties("${CMAKE_CURRENT_BINARY_DIR}/geometry_finalize.cpp"
	MSVC_OPTIONS /Oi /arch:IA32 /fp:precise
)
goldsrc_source_properties("${CMAKE_CURRENT_BINARY_DIR}/geometry_acoustics.cpp"
	MSVC_OPTIONS /arch:IA32 /fp:strict
)
goldsrc_source_properties("${CMAKE_CURRENT_BINARY_DIR}/geometry_submission.cpp"
	MSVC_OPTIONS /Oi /arch:IA32 /fp:precise /EHa
)
goldsrc_source_properties("${CMAKE_CURRENT_BINARY_DIR}/geometry_state.cpp"
	MSVC_OPTIONS /Oi
)
