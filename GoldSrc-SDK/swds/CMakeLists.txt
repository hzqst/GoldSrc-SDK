# Dedicated server engine.

include(GoldSrcEngine)

if(WIN32)
	goldsrc_add_engine(swds
		RENDERER SW
		OUTPUT_NAME swds
		DEF_FILE swds/swds.def
		MSVC_LINK_OPTIONS /IGNORE:4070
	)
else()
	# Like swds.dll, which is the software engine run by hlds, engine_i486.so is
	# the full engine (here the OpenGL one) run by hlds_linux. It therefore
	# needs the client's runtime libraries (libGL, SDL2, vgui.so) to load.
	goldsrc_add_engine(swds
		RENDERER HW
		OUTPUT_NAME engine_i486
	)
endif()
