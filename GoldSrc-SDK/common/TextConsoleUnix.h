#pragma once

#include "textconsole.h"

#include <termios.h>

// Text console on a POSIX terminal. Line editing, history and command
// completion come from CTextConsole; this class reads key presses from stdin
// without blocking and writes to stdout.
class CTextConsoleUnix : public CTextConsole
{
public:
	bool Init( IBaseSystem* system ) override;
	void ShutDown() override;
	void PrintRaw( char* text, int length = 0 ) override;
	void Echo( char* text, int length = 0 ) override;
	char* GetLine() override;
	int GetWidth() override;

private:
	bool ReadChar( char& character );

	bool			m_bTerminal;
	bool			m_bRestoreTerminal;
	struct termios	m_SavedTerminal;
};
