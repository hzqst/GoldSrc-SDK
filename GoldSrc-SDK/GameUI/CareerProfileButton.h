#pragma once

#include "Button.h"
#include "GameUI/ICareerUI.h"
#include <string>

class CCareerProfileFrame;

namespace vgui2
{
class IImage;
class TextImage;
}

class CProfileButton : public vgui2::Button
{
public:
	CProfileButton( CCareerProfileFrame* parent, const char* name, const wchar_t* text, bool* medals );
	~CProfileButton() override;
	void ApplySchemeSettings( vgui2::IScheme* scheme ) override;
	void Paint() override;
	void OnMouseDoublePressed( vgui2::MouseCode mouseCode ) override;
	void SetProfileName( const char* name );
	void SetCoop( bool coop ) { m_isCoop = coop; }
	bool IsCoop() { return m_isCoop; }
	Color GetBgColor() override;
	void SetActive( bool active ) { m_active = active; }
	bool IsActive() { return m_active; }
	const char* GetProfileName() { return m_name.c_str(); }
	vgui2::IBorder* GetBorder( bool depressed, bool armed, bool selected, bool keyFocus ) override;

protected:
	bool					m_medals[MAX_CAREER_DIFFICULTY];
	bool					m_active;
	bool					m_isCoop;
	vgui2::IImage*			m_image[MAX_CAREER_DIFFICULTY];
	vgui2::TextImage*		m_textImage;
	int						m_preTextPad;
	int						m_postTextPad;
	int						m_interImagePad;
	int						m_postImagePad;
	CCareerProfileFrame*	m_pParent;
	std::string				m_name;
	Color					m_bgColor;
	Color					m_activeBgColor;
	vgui2::IBorder*			m_armedBorder;
	vgui2::IBorder*			m_depressedBorder;
};
