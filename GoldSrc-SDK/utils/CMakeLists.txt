# SDK utilities.

# Not yet ported to Linux.
goldsrc_add_module(bspinfo
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/bspinfo/bspinfo.c
		utils/common/bspfile.c
		utils/common/cmdlib.c
		utils/common/scriplib.c
	INCLUDES
		utils/common
)

# Not yet ported to Linux.
goldsrc_add_module(light
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/common/bspfile.c
		utils/common/cmdlib.c
		utils/common/mathlib.c
		utils/common/scriplib.c
		utils/common/threads.c
		utils/light/light.c
		utils/light/ltface.c
		utils/light/trace.c
	INCLUDES
		utils/common
)

# Windows-only: GDI font rasterizer.
goldsrc_add_module(makefont
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CHARSET MBCS
	WHOLE_PROGRAM
	SOURCES
		utils/common/cmdlib.c
		utils/common/wadlib.c
		utils/makefont/makefont.cpp
	INCLUDES
		common
		public
		utils/common
	LINK
		winmm
)

# Windows-only: Win32 file APIs.
goldsrc_add_module(makels
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/makels/makels.cpp
)

# Not yet ported to Linux.
goldsrc_add_module(mdldec
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CXX_STANDARD 17
	WARNING_LEVEL 4
	RUNTIME STATIC
	FLOAT_MODEL STRICT
	WARNINGS_AS_ERRORS
	NO_SSE
	SOURCES
		utils/mdldec/mdldec.cpp
		utils/mdldec/write.cpp
		utils/mdldec/smdwrite.cpp
		utils/mdldec/bmpwrite.cpp
		utils/mdldec/mathlib.cpp
	INCLUDES
		common
	DEFINES
		_USE_MATH_DEFINES
	LINK
		legacy_stdio_float_rounding.obj
)

# Not yet ported to Linux.
goldsrc_add_module(mdlviewer
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CHARSET MBCS
	WHOLE_PROGRAM
	SOURCES
		utils/common/mathlib.c
		utils/mdlviewer/mdlviewer.cpp
		utils/mdlviewer/studio_render.cpp
		utils/mdlviewer/studio_utils.cpp
	INCLUDES
		utils/common
		common
	LINK
		opengl32
		glu32
		external/glut/lib/x86/glut32.lib
	MSVC_OPTIONS
		/external:I${GOLDSRC_ROOT}/external/glut/include
)

# Windows-only: Win32 file APIs.
goldsrc_add_module(mkmovie
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CHARSET MBCS
	WHOLE_PROGRAM
	SOURCES
		utils/mkmovie/mkmovie.c
	INCLUDES
		utils/common
)

# Windows-only: Win32 process information.
goldsrc_add_module(procinfo
	TYPE STATIC
	WINDOWS_ONLY
	CHARSET MBCS
	SOURCES
		utils/procinfo/procinfo.cpp
)

# Not yet ported to Linux.
goldsrc_add_module(qbsp2
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/common/bsplib.c
		utils/common/cmdlib.c
		utils/common/mathlib.c
		utils/common/scriplib.c
		utils/common/threads.c
		utils/qbsp2/gldraw.c
		utils/qbsp2/merge.c
		utils/qbsp2/outside.c
		utils/qbsp2/portals.c
		utils/qbsp2/qbsp.c
		utils/qbsp2/solidbsp.c
		utils/qbsp2/surfaces.c
		utils/qbsp2/tjunc.c
		utils/qbsp2/writebsp.c
	INCLUDES
		utils/common
		external
	DEFINES
		DOUBLEVEC_T
		$<$<CONFIG:Debug>:_NOENUMQBOOL>
	LINK
		opengl32
		glu32
		external/glaux/lib/x86/glaux.lib
		legacy_stdio_definitions
)

# Not yet ported to Linux.
goldsrc_add_module(qcsg
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CHARSET MBCS
	WHOLE_PROGRAM
	SOURCES
		utils/common/bspfile.c
		utils/common/cmdlib.c
		utils/common/mathlib.c
		utils/common/polylib.c
		utils/common/scriplib.c
		utils/common/threads.c
		utils/qcsg/brush.c
		utils/qcsg/gldraw.c
		utils/qcsg/hullfile.c
		utils/qcsg/map.c
		utils/qcsg/qcsg.c
		utils/qcsg/textures.c
	INCLUDES
		utils/common
		external
	DEFINES
		DOUBLEVEC_T
	LINK
		opengl32
		glu32
		external/glaux/lib/x86/glaux.lib
		legacy_stdio_definitions
)

# Not yet ported to Linux.
goldsrc_add_module(qlumpy
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/common/cmdlib.c
		utils/common/lbmlib.c
		utils/common/scriplib.c
		utils/common/wadlib.c
		utils/qlumpy/qlumpy.c
		utils/qlumpy/quakegrb.c
	INCLUDES
		utils/common
)

# Not yet ported to Linux.
goldsrc_add_module(qrad
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/common/bspfile.c
		utils/common/cmdlib.c
		utils/common/mathlib.c
		utils/common/polylib.c
		utils/common/scriplib.c
		utils/common/threads.c
		utils/qrad/lightmap.c
		utils/qrad/qrad.c
		utils/qrad/trace.c
		utils/qrad/vismat.c
	INCLUDES
		utils/common
)

# Windows-only: MFC application.
goldsrc_add_module(serverctrl
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CHARSET MBCS
	MFC SHARED
	GUI
	WHOLE_PROGRAM
	SOURCES
		utils/serverctrl/ServerCtrl.cpp
		utils/serverctrl/ServerCtrlDlg.cpp
		utils/serverctrl/StdAfx.cpp
	WINDOWS_SOURCES
		utils/serverctrl/ServerCtrl.rc
	LINK
		winmm
)

# Windows-only: 3ds Max plug-in.
set(GOLDSRC_MAX_SDK_DIR "${GOLDSRC_ROOT}/external/3dsmax42/maxsdk" CACHE PATH "3ds Max SDK directory")
set(GOLDSRC_CHARACTER_STUDIO_SDK_DIR "${GOLDSRC_ROOT}/external/3dsmax42/cstudio/sdk" CACHE PATH
	"Character Studio SDK directory")
goldsrc_add_module(smdlexp
	TYPE SHARED
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CHARSET MBCS
	DEF_FILE utils/smdlexp/smdlexp.def
	SOURCES
		utils/smdlexp/smdlexp.cpp
	WINDOWS_SOURCES
		utils/smdlexp/smdlexp.rc
	INCLUDES
		${GOLDSRC_MAX_SDK_DIR}/Include
		${GOLDSRC_CHARACTER_STUDIO_SDK_DIR}
	LINK
		comctl32
		${GOLDSRC_MAX_SDK_DIR}/Lib/maxutil.lib
		${GOLDSRC_MAX_SDK_DIR}/Lib/geom.lib
		${GOLDSRC_MAX_SDK_DIR}/Lib/mesh.lib
		${GOLDSRC_MAX_SDK_DIR}/Lib/core.lib
	MSVC_OPTIONS
		/Zc:wchar_t-
)
if(TARGET smdlexp)
	# 3ds Max loads export plug-ins with the .dle extension.
	set_target_properties(smdlexp PROPERTIES SUFFIX ".dle")
endif()

# Not yet ported to Linux.
goldsrc_add_module(sprgen
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/common/cmdlib.c
		utils/common/lbmlib.c
		utils/common/scriplib.c
		utils/sprgen/sprgen.c
	INCLUDES
		utils/common
)

# Not yet ported to Linux.
goldsrc_add_module(studiomdl
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/common/cmdlib.c
		utils/common/lbmlib.c
		utils/common/mathlib.c
		utils/common/scriplib.c
		utils/common/trilib.c
		utils/studiomdl/bmpread.c
		utils/studiomdl/studiomdl.c
		utils/studiomdl/tristrip.c
		utils/studiomdl/write.c
	INCLUDES
		utils/common
		common
		public
)

# Not yet ported to Linux.
goldsrc_add_module(vis
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	WHOLE_PROGRAM
	SOURCES
		utils/common/bspfile.c
		utils/common/cmdlib.c
		utils/common/mathlib.c
		utils/common/scriplib.c
		utils/common/threads.c
		utils/visx2/flow.c
		utils/visx2/soundpvs.c
		utils/visx2/vis.c
	INCLUDES
		utils/common
)

# Not yet ported to Linux.
goldsrc_add_module(xwad
	TYPE EXECUTABLE
	WINDOWS_ONLY
	OUTPUT_DIR utils
	CHARSET MBCS
	WHOLE_PROGRAM
	SOURCES
		utils/common/cmdlib.c
		utils/common/lbmlib.c
		utils/common/wadlib.c
		utils/xwad/xwad.c
	INCLUDES
		utils/common
	DEFINES
		DOUBLEVEC_T
)
