# Hardware- and software-rendered engines.

include(GoldSrcEngine)

goldsrc_add_engine(hw_engine
	RENDERER HW
	OUTPUT_NAME hw
)

# Windows-only: DirectDraw software renderer.
goldsrc_add_engine(sw_engine
	WINDOWS_ONLY
	RENDERER SW
	OUTPUT_NAME sw
	DEF_FILE engine/sw_engine.def
)
