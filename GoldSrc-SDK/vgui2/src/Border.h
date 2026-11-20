#ifndef VGUI_BORDER_H
#define VGUI_BORDER_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/IBorder.h>
#include <vgui/IScheme.h>
#include <Color.h>

class KeyValues;

namespace vgui2
{

class Border : public IBorder
{
public:
	Border();
	~Border();

	void Paint( VPANEL panel ) override;
	void Paint( int x, int y, int wide, int tall ) override;
	void Paint( int x, int y, int wide, int tall, int breakSide, int breakStart, int breakEnd ) override;

	void SetInset( int left, int top, int right, int bottom ) override;
	void GetInset( int& left, int& top, int& right, int& bottom ) override;

	void ApplySchemeSettings( IScheme* pScheme, KeyValues* inResourceData ) override;

	const char* GetName() override;
	void SetName( const char* name ) override;

	// clears the inset and lines so the next ApplySchemeSettings starts from an empty border
	void ResetSettings();

protected:
	int	_inset[4];

private:
	Border( Border& );

	void ParseSideSettings( int side_index, KeyValues* inResourceData, IScheme* pScheme );

	char*	_name;

	// border drawing description
	struct line_t
	{
		Color	col;
		int		startOffset;
		int		endOffset;
#ifdef _MSC_VER
#pragma warning( suppress : 26495 )
#endif
	};

	struct side_t
	{
		int		count;
		line_t*	lines;
	};

	side_t	_sides[4]; // left, top, right, bottom

	friend class VPanel;
};

}

#endif // VGUI_BORDER_H
