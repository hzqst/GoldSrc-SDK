#pragma once

namespace vgui2
{
class MessageBox;
}

class CTutorStateWindow
{
public:
	CTutorStateWindow();
	virtual ~CTutorStateWindow();
	virtual void Activate( const char* message );
	virtual void Activate( const wchar_t* message );
	virtual void UpdatePosition();
	void GetPosition( int& x, int& y );
	void ComputeWindowPos( int screenwidth, int screenheight, int& xpos, int& ypos );

private:
	vgui2::MessageBox*	m_messageBox;
};
