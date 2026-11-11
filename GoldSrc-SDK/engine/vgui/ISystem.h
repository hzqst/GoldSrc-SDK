//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include "../interface.h"
#include "vgui/KeyCode.h"

class KeyValues;

namespace vgui2
{
//-----------------------------------------------------------------------------
// Purpose: Wraps contextless windows system functions
//-----------------------------------------------------------------------------
class ISystem : public IBaseInterface
{
public:
	virtual void Shutdown();
	virtual void RunFrame();
	// use this with the "open" command to launch web browsers/explorer windows, eg. ShellExecute("open", "www.valvesoftware.com")
	virtual void ShellExecute( const char* command, const char* file );

	virtual double GetFrameTime();
	virtual double GetCurrentTime();
	virtual long GetTimeMillis();

	virtual int GetClipboardTextCount();
	virtual void SetClipboardText( const char* text, int textLength );
	virtual void SetClipboardText( const wchar_t* text, int textLength );
	virtual int GetClipboardText( int offset, char* buffer, int bufferLength );
	virtual int GetClipboardText( int offset, wchar_t* buffer, int bufferLength );

	// windows registry
	virtual bool SetRegistryString( const char* key, const char* value );
	virtual bool GetRegistryString( const char* key, char* value, int valueLength );
	virtual bool SetRegistryInteger( const char* key, int value );
	virtual bool GetRegistryInteger( const char* key, int& value );

	// user config
	virtual KeyValues* GetUserConfigFileData( const char* dialogName, int dialogID );
	// sets the name of the config file to save/restore from.  Settings are loaded immediately.
	virtual void SetUserConfigFile( const char* fileName, const char* pathID );
	virtual void SaveUserConfigFile();

	virtual bool SetWatchForComputerUse( bool watch );
	virtual double GetTimeSinceLastUse();
	virtual int GetAvailableDrives( char* buffer, int bufferLength );
	virtual bool CommandLineParamExists( const char* parameter );
	virtual const char* GetFullCommandLine();
	// returns the current local time and date
	// fills in every field that a pointer is given to it for
	virtual bool GetCurrentTimeAndDate( int* year, int* month, int* dayOfWeek, int* day, int* hour, int* minute,
		int* second );
	// returns the amount of available disk space, in bytes, on the drive
	// path can be any path, drive letter is stripped out
	virtual double GetFreeDiskSpace( const char* path );

	virtual bool CreateShortcut( const char* linkFile, const char* targetFile, const char* arguments,
		const char* workingDirectory, const char* iconFile );
	virtual bool GetShortcutTarget( const char* linkFile, char* targetFile, char* arguments, int bufferLength );
	virtual bool ModifyShortcutTarget( const char* linkFile, const char* targetFile, const char* arguments,
		const char* workingDirectory );

	virtual bool GetCommandLineParamValue( const char* parameter, char* value, int valueLength );
	virtual bool DeleteRegistryKey( const char* key );
	virtual const char* GetDesktopFolderPath();

	virtual KeyCode KeyCode_VirtualKeyToVGUI( int virtualKey );
	virtual int KeyCode_VGUIToVirtualKey( KeyCode key );
};
}
