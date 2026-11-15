# Particle manager.

goldsrc_add_module(particleman
	TYPE SHARED
	OUTPUT_DIR valve/cl_dlls
	CXX_STANDARD 17
	WARNING_LEVEL 4
	DEF_FILE projects/vs2026/particleman.def
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	SOURCES
		particleman/pman_frustum.cpp
		particleman/pman_main.cpp
		particleman/pman_math.cpp
		particleman/pman_particlemem.cpp
		particleman/pman_triangleeffect.cpp
		public/interface.cpp
	INCLUDES
		particleman
		cl_dll
		engine
		common
		public
		pm_shared
		external
		dlls
	DEFINES
		CLIENT_DLL
)

goldsrc_source_properties(public/interface.cpp
	DEFINES BASEMONSTER_H
	INCLUDES bshift/dlls dlls common
	MSVC_OPTIONS /FI${GOLDSRC_ROOT}/bshift/dlls/extdll.h /FI${GOLDSRC_ROOT}/bshift/dlls/util.h /FI${GOLDSRC_ROOT}/bshift/dlls/cbase.h
	GCC_OPTIONS -include ${GOLDSRC_ROOT}/bshift/dlls/extdll.h -include ${GOLDSRC_ROOT}/bshift/dlls/util.h -include ${GOLDSRC_ROOT}/bshift/dlls/cbase.h
)
