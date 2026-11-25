# VGUI2 runtime.

# On Linux, like Valve's vgui2.so, the library has no surface or fonts: the
# engine supplies the surface, and SDL2 provides the clipboard.
goldsrc_add_module(vgui2
	TYPE SHARED
	WARNING_LEVEL 4
	CHARSET MBCS
	WARNINGS_AS_ERRORS
	LINKER_WARNINGS_AS_ERRORS
	KEEP_ASSERTS
	HIDDEN_VISIBILITY
	EXPORTS
		CreateInterface
	SOURCES
		vgui2/src/Bitmap.cpp
		vgui2/src/Border.cpp
		vgui2/src/fileimage.cpp
		vgui2/src/LocalizedStringTable.cpp
		vgui2/src/MemoryBitmap.cpp
		vgui2/src/MessageListener.cpp
		public/characterset.cpp
		vgui2/src/public/filesystem_helpers.cpp
		public/interface.cpp
		vgui2/src/public/KeyValues.cpp
		vgui2/src/public/MemPool.cpp
		vgui2/src/public/UnicodeFileHelpers.cpp
		vgui2/src/public/UtlBuffer.cpp
		vgui2/src/public/utllinkedlist_inst.cpp
		vgui2/src/public/utlpriorityqueue_inst.cpp
		vgui2/src/public/utlsymbol.cpp
		vgui2/src/public/utlvector_inst.cpp
		vgui2/src/Scheme.cpp
		vgui2/src/vgui.cpp
		vgui2/src/vgui_internal.cpp
		vgui2/src/vgui_surfacelib/vgui_key_translation.cpp
		vgui2/src/VGuiKeyValues.cpp
		vgui2/src/VPanel.cpp
		vgui2/src/VPanelWrapper.cpp
	WINDOWS_SOURCES
		vgui2/src/InputWin32.cpp
		vgui2/src/Surface.cpp
		vgui2/src/System.cpp
		vgui2/vgui_surfacelib/FontAmalgam.cpp
		vgui2/vgui_surfacelib/FontEffects.cpp
		vgui2/vgui_surfacelib/FontManager.cpp
		vgui2/vgui_surfacelib/Win32Font.cpp
	LINUX_SOURCES
		vgui2/src/inputosx.cpp
		vgui2/src/system_posix.cpp
	INCLUDES
		vgui2/src
		vgui2/src/public
		vgui2/src/vgui_surfacelib
		public
	WINDOWS_DEFINES
		UTL_ENGINE_ASSERTS
	WINDOWS_LINK
		shlwapi
		winmm
	LINUX_LINK
		goldsrc::SDL2
		dl
)
