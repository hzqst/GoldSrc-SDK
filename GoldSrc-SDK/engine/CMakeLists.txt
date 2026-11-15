# Hardware- and software-rendered engines.

include(GoldSrcEngine)

# Not yet ported to Linux.
goldsrc_add_engine(hw_engine
	WINDOWS_ONLY
	RENDERER HW
	OUTPUT_NAME hw
)

# Windows-only: DirectDraw software renderer.
goldsrc_add_engine(sw_engine
	WINDOWS_ONLY
	RENDERER SW
	OUTPUT_NAME sw
	DEF_FILE projects/vs2026/sw_engine.def
)
