# VGUI2 runtime.

# Not yet ported to Linux.
goldsrc_add_module(vgui2
	TYPE SHARED
	WINDOWS_ONLY
	WARNING_LEVEL 4
	CHARSET MBCS
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	KEEP_ASSERTS
	EXPORTS
		CreateInterface
	SOURCES
		vgui2/src/Bitmap.cpp
		vgui2/src/Border.cpp
		vgui2/src/fileimage.cpp
		vgui2/src/InputWin32.cpp
		vgui2/src/LocalizedStringTable.cpp
		vgui2/src/MemoryBitmap.cpp
		vgui2/src/MessageListener.cpp
		public/characterset.cpp
		vgui2/src/public/filesystem_helpers.cpp
		vgui2/src/public/interface.cpp
		vgui2/src/public/KeyValues.cpp
		vgui2/src/public/MemPool.cpp
		vgui2/src/public/UnicodeFileHelpers.cpp
		vgui2/src/public/UtlBuffer.cpp
		vgui2/src/public/utllinkedlist_inst.cpp
		vgui2/src/public/utlpriorityqueue_inst.cpp
		vgui2/src/public/utlrbtree.cpp
		vgui2/src/public/utlsymbol.cpp
		vgui2/src/public/utlvector_inst.cpp
		vgui2/src/Scheme.cpp
		vgui2/src/Surface.cpp
		vgui2/src/System.cpp
		vgui2/src/vgui.cpp
		vgui2/src/vgui_internal.cpp
		vgui2/src/vgui_surfacelib/vgui_key_translation.cpp
		vgui2/src/VGuiKeyValues.cpp
		vgui2/src/VPanel.cpp
		vgui2/src/VPanelWrapper.cpp
		vgui2/vgui_surfacelib/FontAmalgam.cpp
		vgui2/vgui_surfacelib/FontManager.cpp
		vgui2/vgui_surfacelib/linuxfont.cpp
	INCLUDES
		vgui2/src
		vgui2/src/public
		vgui2/src/vgui_surfacelib
		public
	LINK
		shlwapi
		winmm
)
