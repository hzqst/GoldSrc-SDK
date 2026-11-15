# Dedicated server engine.

include(GoldSrcEngine)

# Not yet ported to Linux.
goldsrc_add_engine(swds
	WINDOWS_ONLY
	RENDERER SW
	OUTPUT_NAME swds
	DEF_FILE swds/swds.def
	MSVC_LINK_OPTIONS /IGNORE:4070
)
