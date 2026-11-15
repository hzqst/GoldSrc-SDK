#include "TextConsoleUnix.h"
#include "IBaseSystem.h"

#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define CONSOLE_ESCAPE		'\x1b'
#define CONSOLE_DELETE		'\x7f'
#define DEFAULT_CONSOLE_WIDTH	80

bool CTextConsoleUnix::Init( IBaseSystem* system )
{
	m_bTerminal = isatty( STDIN_FILENO ) != 0;
	m_bRestoreTerminal = false;

	// Read the terminal one key at a time, without echo; CTextConsole echoes
	// the edited line itself.
	if ( m_bTerminal && tcgetattr( STDIN_FILENO, &m_SavedTerminal ) == 0 )
	{
		struct termios terminal = m_SavedTerminal;
		terminal.c_lflag &= ~( ICANON | ECHO );
		terminal.c_cc[VMIN] = 0;
		terminal.c_cc[VTIME] = 0;
		m_bRestoreTerminal = tcsetattr( STDIN_FILENO, TCSANOW, &terminal ) == 0;
	}

	int flags = fcntl( STDIN_FILENO, F_GETFL, 0 );
	if ( flags != -1 )
		fcntl( STDIN_FILENO, F_SETFL, flags | O_NONBLOCK );

	return CTextConsole::Init( system );
}

void CTextConsoleUnix::ShutDown()
{
	if ( m_bRestoreTerminal )
	{
		tcsetattr( STDIN_FILENO, TCSANOW, &m_SavedTerminal );
		m_bRestoreTerminal = false;
	}

	CTextConsole::ShutDown();
}

void CTextConsoleUnix::PrintRaw( char* text, int length )
{
	size_t count = length ? (size_t)length : strlen( text );
	while ( count > 0 )
	{
		ssize_t written = write( STDOUT_FILENO, text, count );
		if ( written <= 0 )
			break;
		text += written;
		count -= (size_t)written;
	}
}

void CTextConsoleUnix::Echo( char* text, int length )
{
	PrintRaw( text, length );
}

bool CTextConsoleUnix::ReadChar( char& character )
{
	return read( STDIN_FILENO, &character, 1 ) == 1;
}

char* CTextConsoleUnix::GetLine()
{
	char character;
	while ( ReadChar( character ) )
	{
		// Arrow keys arrive as ESC [ A..D.
		if ( character == CONSOLE_ESCAPE )
		{
			char sequence[2];
			if ( !ReadChar( sequence[0] ) || sequence[0] != '[' || !ReadChar( sequence[1] ) )
				continue;

			switch ( sequence[1] )
			{
			case 'A':
				ReceiveUpArrow();
				break;
			case 'B':
				ReceiveDownArrow();
				break;
			case 'C':
				ReceiveRightArrow();
				break;
			case 'D':
				ReceiveLeftArrow();
				break;
			}
			continue;
		}

		switch ( character )
		{
		case '\b':
		case CONSOLE_DELETE:
			ReceiveBackspace();
			break;
		case '\t':
			ReceiveTab();
			break;
		case '\n':
		case '\r':
			if ( ReceiveNewline() )
				return m_szConsoleText;
			break;
		default:
			if ( character >= ' ' && character <= '~' )
				ReceiveStandardChar( character );
			break;
		}
	}

	return NULL;
}

int CTextConsoleUnix::GetWidth()
{
	struct winsize size;
	if ( !m_bTerminal || ioctl( STDOUT_FILENO, TIOCGWINSZ, &size ) != 0 || size.ws_col <= 1 )
		return DEFAULT_CONSOLE_WIDTH;
	return size.ws_col;
}
