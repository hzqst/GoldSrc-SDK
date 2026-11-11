//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "OptionsSubMultiplayer.h"
#include "BitmapImagePanel.h"
#include "CreateMultiplayerGameServerPage.h"
#include "CvarSlider.h"
#include "CvarTextEntry.h"
#include "CvarToggleCheckButton.h"
#include "GameUI_Interface.h"
#include "LabeledCommandComboBox.h"
#include "ModInfo.h"
#include "MultiplayerAdvancedDialog.h"

#include "Button.h"
#include "ComboBox.h"
#include "FileSystem.h"
#include "ImagePanel.h"
#include "Label.h"
#include "../engine/utlbuffer.h"
#include "utlrbtree.h"
#include "utlsymbol.h"
#include "vgui/ISurface.h"
#include "vgui/KeyValues.h"
#include "vgui_controls/Controls.h"
#include "common/common.h"
#include "../game_shared/shared_util.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PostMessage
#undef PostMessage
#endif

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#define LOGO_COLOR_COMMAND	"cl_logocolor "
#define BMP_PALETTE_COLORS	256
#define BMP_PALETTE_BYTES	( sizeof( BITMAPINFOHEADER ) + BMP_PALETTE_COLORS * sizeof( RGBQUAD ) )
//#include <windows.h>

#define DIB_HEADER_MARKER	( (WORD)( 'M' << 8 ) | 'B' )

bool UpdateLogoWAD( void* image, int imageSize, int r, int g, int b );
bool UpdateLogoWADFromTGA( const char* filename );

struct ColorItem_t
{
	const char*	name;
	int			r;
	int			g;
	int			b;
};

static ColorItem_t itemlist[] =
{
	{ "#Valve_Orange", 255, 120, 24 },
	{ "#Valve_Yellow", 225, 180, 24 },
	{ "#Valve_Blue", 0, 60, 255 },
	{ "#Valve_Ltblue", 0, 167, 255 },
	{ "#Valve_Green", 0, 167, 0 },
	{ "#Valve_Red", 255, 43, 0 },
	{ "#Valve_Brown", 123, 73, 0 },
	{ "#Valve_Ltgray", 100, 100, 100 },
	{ "#Valve_Dkgray", 36, 36, 36 }
};

static ColorItem_t s_crosshairColors[] =
{
	{ "#Valve_Green", 50, 250, 50 },
	{ "#Valve_Red", 250, 50, 50 },
	{ "#Valve_Blue", 50, 50, 250 },
	{ "#Valve_Yellow", 250, 250, 50 },
	{ "#Valve_Ltblue", 50, 250, 250 }
};

class CrosshairImagePanel : public vgui2::ImagePanel
{
public:
	CrosshairImagePanel( vgui2::Panel* parent, const char* name, CCvarToggleCheckButton* additive );
	~CrosshairImagePanel() override;
	void UpdateCrosshair( int r, int g, int b, int size );

protected:
	void Paint() override;

private:
	int						m_R;
	int						m_G;
	int						m_B;
	int						m_barSize;
	int						m_barGap;
	CCvarToggleCheckButton*	m_pAdditive;
};

CrosshairImagePanel::CrosshairImagePanel( vgui2::Panel* parent, const char* name, CCvarToggleCheckButton* additive )
	: vgui2::ImagePanel( parent, name )
{
	SetHDProportional( true );
	m_R = 50;
	m_G = 250;
	m_B = 50;
	m_pAdditive = additive;
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	int scale = 1024;
	if ( wide > 640 )
		scale = wide < 1024 ? 800 : 640;

	m_barSize = 9 * wide / scale;
	m_barGap = 5 * wide / scale;
}

CrosshairImagePanel::~CrosshairImagePanel()
{
}

void CrosshairImagePanel::UpdateCrosshair( int r, int g, int b, int size )
{
	m_R = r;
	m_G = g;
	m_B = b;
	int wide, tall;
	vgui2::surface()->GetScreenSize( wide, tall );
	int scale = 1024;
	if ( size == 0 && wide > 640 )
		scale = wide < 1024 ? 800 : 640;
	else if ( size == 2 )
		scale = 800;
	else if ( size == 3 )
		scale = 640;

	m_barSize = 9 * wide / scale;
	m_barGap = 5 * wide / scale;
}

void CrosshairImagePanel::Paint()
{
	vgui2::ImagePanel::Paint();
	if ( !m_pAdditive )
		return;

	int wide, tall;
	GetSize( wide, tall );
	if ( m_pAdditive->IsSelected() )
	{
		engine->pfnFillRGBA( wide / 2 - m_barGap - m_barSize + 1, tall / 2, m_barSize, 1, m_R, m_G, m_B, 255 );
		engine->pfnFillRGBA( wide / 2 + m_barGap, tall / 2, m_barSize, 1, m_R, m_G, m_B, 255 );
		engine->pfnFillRGBA( wide / 2, tall / 2 - m_barGap - m_barSize + 1, 1, m_barSize, m_R, m_G, m_B, 255 );
		engine->pfnFillRGBA( wide / 2, tall / 2 + m_barGap, 1, m_barSize, m_R, m_G, m_B, 255 );
	}
	else
	{
		engine->pfnFillRGBABlend( wide / 2 - m_barGap - m_barSize + 1, tall / 2, m_barSize, 1, m_R, m_G, m_B, 255 );
		engine->pfnFillRGBABlend( wide / 2 + m_barGap, tall / 2, m_barSize, 1, m_R, m_G, m_B, 255 );
		engine->pfnFillRGBABlend( wide / 2, tall / 2 - m_barGap - m_barSize + 1, 1, m_barSize, m_R, m_G, m_B, 255 );
		engine->pfnFillRGBABlend( wide / 2, tall / 2 + m_barGap, 1, m_barSize, m_R, m_G, m_B, 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsSubMultiplayer::COptionsSubMultiplayer( vgui2::Panel* parent ) : BaseClass( parent, "OptionsSubMultiplayer", true )
{
	vgui2::Button* advanced = new vgui2::Button( this, "Advanced", "#GameUI_AdvancedEllipsis" );
	advanced->SetCommand( "Advanced" );
	new vgui2::Label( this, "NameLabel", "#GameUI_PlayerName" );
	m_pNameTextEntry = new CCvarTextEntry( this, "NameEntry", "name" );
	m_pNameTextEntry->SetCatchEnterKey( true );
	m_pPrimaryColorSlider = new CCvarSlider( this, "Primary Color Slider", "#GameUI_PrimaryColor",
		0.0f, 255.0f, "topcolor", false );
	m_pSecondaryColorSlider = new CCvarSlider( this, "Secondary Color Slider", "#GameUI_SecondaryColor",
		0.0f, 255.0f, "bottomcolor", false );
	m_pHighQualityModelCheckBox = new CCvarToggleCheckButton( this, "High Quality Models", "#GameUI_HighModels",
		"cl_himodels" );
	m_pModelList = new CLabeledCommandComboBox( this, "Player model" );
	InitModelList( m_pModelList );
	m_pLogoList = new CLabeledCommandComboBox( this, "SpraypaintList" );
	m_LogoName[0] = 0;
	InitLogoList( m_pLogoList );
	m_pColorList = new CLabeledCommandComboBox( this, "SpraypaintColor" );
	const char* color = engine->pfnGetCvarString( (char*)"cl_logocolor" );
	int selection = 0;
	for ( int i = 0; i < (int)( sizeof( itemlist ) / sizeof( itemlist[0] ) ); i++ )
	{
		if ( color && stricmp( color, itemlist[i].name ) == 0 )
			selection = i;

		char command[256];
		sprintf( command, LOGO_COLOR_COMMAND "%s\n", itemlist[i].name );
		m_pColorList->AddItem( itemlist[i].name, command );
	}
	m_pColorList->SetInitialItem( selection );
	m_pColorList->AddActionSignalTarget( this );
	m_pModelImage = new CBitmapImagePanel( this, "ModelImage", NULL );
	m_pModelImage->AddActionSignalTarget( this );
	m_pLogoImage = new CBitmapImagePanel( this, "LogoImage", NULL );
	m_pLogoImage->AddActionSignalTarget( this );
	m_nTopColor = 30;
	m_nBottomColor = 6;
	m_pCrosshairImage = NULL;
	m_pCrosshairColorComboBox = NULL;
	m_pCrosshairSize = NULL;
	m_pCrosshairTranslucencyCheckbox = NULL;
	m_displayedCrosshairColorRed = -1;
	m_displayedCrosshairColorGreen = -1;
	m_displayedCrosshairColorBlue = -1;
	if ( stricmp( gEngfuncs.pfnGetGameDirectory(), "czero" ) == 0
		|| stricmp( gEngfuncs.pfnGetGameDirectory(), "terror" ) == 0
		|| stricmp( gEngfuncs.pfnGetGameDirectory(), "cstrike" ) == 0
		|| stricmp( gEngfuncs.pfnGetGameDirectory(), "cstrike_beta" ) == 0 )
	{
		m_displayedCrosshairColorRed = 50;
		m_displayedCrosshairColorGreen = 250;
		m_displayedCrosshairColorBlue = 50;
		m_pCrosshairColorComboBox = new vgui2::ComboBox( this, "CrosshairColorComboBox", 6, false );
		InitCrosshairColorEntries();
		m_pCrosshairSize = new CLabeledCommandComboBox( this, "CrosshairSizeComboBox" );
		m_pCrosshairTranslucencyCheckbox = new CCvarToggleCheckButton( this, "CrosshairTranslucencyCheckbox",
			"#GameUI_Translucent", "cl_crosshair_translucent" );
		InitCrosshairSizeList( m_pCrosshairSize );
		m_pCrosshairImage = new CrosshairImagePanel( this, "CrosshairImage", m_pCrosshairTranslucencyCheckbox );
		RedrawCrosshairImage();
	}
	m_nLogoR = 255;
	m_nLogoG = 255;
	m_nLogoB = 255;
	vgui2::Button* okay = new vgui2::Button( this, "OK", "#GameUI_OK" );
	vgui2::Button* apply = new vgui2::Button( this, "Apply", "#GameUI_Apply" );
	vgui2::Button* cancel = new vgui2::Button( this, "Cancel", "#GameUI_Cancel" );
	LoadControlSettings( "Resource/OptionsSubMultiplayer.res", NULL );
	okay->SetEnabled( false );
	okay->SetVisible( false );
	apply->SetEnabled( false );
	apply->SetVisible( false );
	cancel->SetEnabled( false );
	cancel->SetVisible( false );
	if ( m_pCrosshairImage )
		m_pCrosshairImage->SetImageSize( GetProportionalScaledValue( 64 ), GetProportionalScaledValue( 64 ) );

	// turn off model selection stuff if the mod specifies "nomodels" in the liblist.gam file
	if ( ModInfo()->NoModels() )
	{
		if ( m_pModelImage )
			m_pModelImage->SetVisible( false );
		if ( m_pModelList )
			m_pModelList->SetVisible( false );
		if ( m_pPrimaryColorSlider )
			m_pPrimaryColorSlider->SetVisible( false );
		if ( m_pSecondaryColorSlider )
			m_pSecondaryColorSlider->SetVisible( false );
		vgui2::Panel* label = FindChildByName( "Label1", false );
		if ( label )
			label->SetVisible( false );
		label = FindChildByName( "Colors", false );
		if ( label )
			label->SetVisible( false );
	}
	// turn off the himodel stuff if the mod specifies "nohimodel" in the liblist.gam file
	if ( ModInfo()->NoHiModel() )
	{
		if ( m_pHighQualityModelCheckBox )
			m_pHighQualityModelCheckBox->SetVisible( false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
COptionsSubMultiplayer::~COptionsSubMultiplayer()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnCommand( const char* command )
{
	if ( stricmp( command, "Advanced" ) == 0 )
	{
		if ( !m_hMultiplayerAdvancedDialog.Get() )
			m_hMultiplayerAdvancedDialog = new CMultiplayerAdvancedDialog( this );

		m_hMultiplayerAdvancedDialog->Activate();
	}
	else if ( stricmp( command, "ShowKeyboard" ) == 0 )
	{
		engine->pfnClientCmd( (char*)"show_osk 609 273" );
	}
	BaseClass::OnCommand( command );
}

void COptionsSubMultiplayer::InitModelList( CLabeledCommandComboBox* combo )
{
	vgui2::filesystem()->RemoveFile( "models/player/remapped.bmp", "GAMECONFIG" );
	vgui2::filesystem()->RemoveFile( "models/player/remapped.bmp", "GAME" );
	vgui2::filesystem()->RemoveFile( "models/player/remapped.bmp", NULL );
	char currentModel[64];
	currentModel[0] = 0;
	const char* model = engine->pfnGetCvarString( (char*)"model" );
	if ( model )
	{
		strncpy( currentModel, model, sizeof( currentModel ) );
		currentModel[sizeof( currentModel ) - 1] = 0;
	}
	FileFindHandle_t findHandle = 0;
	const char* filename = vgui2::filesystem()->FindFirst( "models/player/*.*", &findHandle, NULL );
	int selection = 0;
	int count = 0;
	while ( filename )
	{
		if ( vgui2::filesystem()->FindIsDirectory( findHandle ) && filename[0] != '.' && !combo->HasItem( filename ) )
		{
			char command[256];
			_snprintf( command, sizeof( command ), "model %s\n", filename );
			command[sizeof( command ) - 1] = 0;
			if ( stricmp( currentModel, filename ) == 0 )
				selection = count;

			combo->AddItem( filename, command );
			count++;
		}
		filename = vgui2::filesystem()->FindNext( findHandle );
	}
	vgui2::filesystem()->FindClose( findHandle );
	combo->SetInitialItem( selection );
}

void COptionsSubMultiplayer::InitLogoList( CLabeledCommandComboBox* combo )
{
	vgui2::filesystem()->RemoveFile( "logos/remapped.bmp", "GAMECONFIG" );
	vgui2::filesystem()->RemoveFile( "logos/remapped.bmp", "GAME" );
	vgui2::filesystem()->RemoveFile( "logos/remapped.bmp", NULL );
	CUtlRBTree<CUtlSymbol, short> names( 0, 0, CaselessStringLessThan );
	FileFindHandle_t findHandle = 0;
	char directory[512];
	const char* logo = engine->pfnGetCvarString( (char*)"cl_logofile" );
	sprintf( directory, "logos/*.bmp" );
	const char* filename = vgui2::filesystem()->FindFirst( directory, &findHandle, NULL );
	while ( filename )
	{
		if ( stricmp( filename, "remapped.bmp" ) != 0 && filename[0] && filename[0] != '.' )
		{
			// Strip the .bmp
			char name[512];
			strcpy( name, filename );
			size_t length = strlen( name );
			if ( length > 3 )
				name[length - 4] = 0;

			if ( names.Find( name ) == names.InvalidIndex() )
				names.Insert( name );
		}
		filename = vgui2::filesystem()->FindNext( findHandle );
	}
	vgui2::filesystem()->FindClose( findHandle );
	int selection = 0;
	for ( short index = names.FirstInorder(); index != names.InvalidIndex(); index = names.NextInorder( index ) )
	{
		const char* name = names[index].String();
		combo->AddItem( name, "" );
		if ( stricmp( name, logo ) == 0 )
			combo->SetInitialItem( selection );

		selection++;
	}
}

void COptionsSubMultiplayer::InitCrosshairColorEntries()
{
	int r = 50;
	int g = 250;
	int b = 50;
	const char* color = engine->pfnGetCvarString( (char*)"cl_crosshair_color" );
	if ( color )
	{
		const char* next = SharedParse( color );
		const char* token = SharedGetToken();
		if ( token )
		{
			r = atoi( token );
			next = SharedParse( next );
			token = SharedGetToken();
			if ( token )
			{
				g = atoi( token );
				SharedParse( next );
				token = SharedGetToken();
				if ( token )
					b = atoi( token );
			}
		}
	}
	if ( m_pCrosshairColorComboBox )
	{
		KeyValues* data = new KeyValues( "data" );
		data->Clear();
		int selection = 0;
		for ( int i = 0; i < (int)( sizeof( s_crosshairColors ) / sizeof( s_crosshairColors[0] ) ); i++ )
		{
			ColorItem_t& colorItem = s_crosshairColors[i];
			data->SetInt( "red", colorItem.r );
			data->SetInt( "green", colorItem.g );
			data->SetInt( "blue", colorItem.b );
			m_pCrosshairColorComboBox->AddItem( colorItem.name, data );
			if ( r == colorItem.r && g == colorItem.g && b == colorItem.b )
				selection = i;
		}
		m_pCrosshairColorComboBox->ActivateItemByRow( selection );
		data->deleteThis();
	}
	RedrawCrosshairImage();
}

void COptionsSubMultiplayer::InitCrosshairSizeList( CLabeledCommandComboBox* combo )
{
	if ( !combo )
		return;

	combo->Reset();
	combo->AddItem( "#GameUI_Auto", "cl_crosshair_size auto" );
	combo->AddItem( "#GameUI_Small", "cl_crosshair_size small" );
	combo->AddItem( "#GameUI_Medium", "cl_crosshair_size medium" );
	combo->AddItem( "#GameUI_Large", "cl_crosshair_size large" );
	const char* size = engine->pfnGetCvarString( (char*)"cl_crosshair_size" );
	int selection = 0;
	if ( size && stricmp( "auto", size ) != 0 )
	{
		if ( stricmp( "small", size ) == 0 )
			selection = 1;
		else if ( stricmp( "medium", size ) == 0 )
			selection = 2;
		else if ( stricmp( "large", size ) == 0 )
			selection = 3;
	}
	combo->SetInitialItem( selection );
}

void COptionsSubMultiplayer::RedrawCrosshairImage()
{
	bool enableApplyButton = false;

	if ( m_pCrosshairColorComboBox )
	{
		KeyValues* data = m_pCrosshairColorComboBox->GetActiveItemUserData();
		int r = data->GetInt( "red", 0 );
		int g = data->GetInt( "green", 0 );
		int b = data->GetInt( "blue", 0 );
		if ( r != m_displayedCrosshairColorRed || g != m_displayedCrosshairColorGreen || b != m_displayedCrosshairColorBlue )
			enableApplyButton = true;

		m_displayedCrosshairColorRed = r;
		m_displayedCrosshairColorGreen = g;
		m_displayedCrosshairColorBlue = b;

		if ( enableApplyButton )
			OnApplyButtonEnable();

		if ( m_pCrosshairImage && m_pCrosshairSize )
		{
			int size = m_pCrosshairSize->GetActiveItem();
			m_pCrosshairImage->UpdateCrosshair( m_displayedCrosshairColorRed, m_displayedCrosshairColorGreen,
				m_displayedCrosshairColorBlue, size );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapModel()
{
	char modelname[256];
	m_pModelList->GetText( modelname, sizeof( modelname ) );
	if ( modelname[0] )
	{
		char texture[256];
		sprintf( texture, "models/player/remapped" );
		RemapPalette( modelname, m_nTopColor, m_nBottomColor );
		m_pModelImage->setTexture( texture );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::ColorForName( const char* name, int& r, int& g, int& b )
{
	r = g = b = 0;
	for ( int i = 0; i < (int)( sizeof( itemlist ) / sizeof( itemlist[0] ) ); i++ )
	{
		if ( strnicmp( name, itemlist[i].name, strlen( itemlist[i].name ) ) == 0 )
		{
			r = itemlist[i].r;
			g = itemlist[i].g;
			b = itemlist[i].b;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapLogo()
{
	char logoname[256];
	m_pLogoList->GetText( logoname, sizeof( logoname ) );
	if ( !logoname[0] )
		return;

	char texture[256];
	_snprintf( texture, sizeof( texture ), "logos/%s.tga", logoname );
	texture[sizeof( texture ) - 1] = 0;
	if ( vgui2::filesystem()->FileExists( texture ) )
	{
		_snprintf( texture, sizeof( texture ), "logos/%s", logoname );
		texture[sizeof( texture ) - 1] = 0;
		m_pColorList->SetEnabled( false );
	}
	else
	{
		sprintf( texture, "logos/remapped" );

		int r, g, b;
		const char* colorname = m_pColorList->GetActiveItemCommand();
		if ( !colorname || !colorname[0] )
			return;

		ColorForName( colorname + sizeof( LOGO_COLOR_COMMAND ) - 1, r, g, b );
		RemapLogoPalette( logoname, r, g, b );
		m_pColorList->SetEnabled( true );
	}
	m_pLogoImage->setTexture( texture );
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever model name changes
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnTextChanged( vgui2::Panel* panel )
{
	if ( panel != m_pNameTextEntry )
	{
		RemapModel();
		RemapLogo();
		RedrawCrosshairImage();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnSliderMoved( KeyValues* )
{
	m_nTopColor = (int)m_pPrimaryColorSlider->GetSliderValue();
	m_nBottomColor = (int)m_pSecondaryColorSlider->GetSliderValue();
	RemapModel();
	RedrawCrosshairImage();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnApplyButtonEnable()
{
	PostMessage( GetParent(), new KeyValues( "ApplyButtonEnable" ), 0.0f );
	InvalidateLayout( false, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static void PaletteHueReplace( RGBQUAD* palSrc, int newHue, int Start, int end )
{
	int i;
	float r, b, g;
	float maxcol, mincol;
	float hue, val, sat;

	hue = (float)( newHue * ( 360.0 / 255 ) );

	for ( i = Start; i <= end; i++ )
	{
		b = palSrc[i].rgbBlue;
		g = palSrc[i].rgbGreen;
		r = palSrc[i].rgbRed;

		maxcol = max( max( r, g ), b ) / 255.0f;
		mincol = min( min( r, g ), b ) / 255.0f;

		if ( maxcol == 0 )
			continue;

		val = maxcol;
		sat = ( maxcol - mincol ) / maxcol;

		mincol = val * ( 1.0f - sat );

		if ( hue <= 120 )
		{
			b = mincol;
			if ( hue < 60 )
			{
				r = val;
				g = mincol + hue * ( val - mincol ) / ( 120 - hue );
			}
			else
			{
				g = val;
				r = mincol + ( 120 - hue ) * ( val - mincol ) / hue;
			}
		}
		else if ( hue <= 240 )
		{
			r = mincol;
			if ( hue < 180 )
			{
				g = val;
				b = mincol + ( hue - 120 ) * ( val - mincol ) / ( 240 - hue );
			}
			else
			{
				b = val;
				g = mincol + ( 240 - hue ) * ( val - mincol ) / ( hue - 120 );
			}
		}
		else
		{
			g = mincol;
			if ( hue < 300 )
			{
				b = val;
				r = mincol + ( hue - 240 ) * ( val - mincol ) / ( 360 - hue );
			}
			else
			{
				r = val;
				b = mincol + ( 360 - hue ) * ( val - mincol ) / ( hue - 240 );
			}
		}

		palSrc[i].rgbBlue = (unsigned char)( b * 255 );
		palSrc[i].rgbGreen = (unsigned char)( g * 255 );
		palSrc[i].rgbRed = (unsigned char)( r * 255 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapPalette( char* filename, int topcolor, int bottomcolor )
{
	char infile[256];
	char outfile[256];

	FileHandle_t file;
	CUtlBuffer outbuffer( 20480, 20480, false );

	_snprintf( infile, sizeof( infile ), "models/player/%s/%s.bmp", filename, filename );
	infile[sizeof( infile ) - 1] = 0;
	sprintf( outfile, "models/player/remapped.bmp" );

	file = vgui2::filesystem()->Open( infile, "rb", NULL );
	if ( file )
	{
		BITMAPFILEHEADER bmfHeader = {};
		DWORD dwBitsSize, dwFileSize;
		LPBITMAPINFO lpbmi;

		dwFileSize = vgui2::filesystem()->Size( file );

		vgui2::filesystem()->Read( &bmfHeader, sizeof( bmfHeader ), file );
		outbuffer.Put( &bmfHeader, sizeof( bmfHeader ) );

		if ( bmfHeader.bfType == DIB_HEADER_MARKER && dwFileSize >= sizeof( bmfHeader ) + BMP_PALETTE_BYTES )
		{
			dwBitsSize = dwFileSize - sizeof( bmfHeader );

			char* pDIB = (char*)malloc( dwBitsSize );
			vgui2::filesystem()->Read( pDIB, dwBitsSize, file );

			lpbmi = (LPBITMAPINFO)pDIB;

			// Remap palette
			if ( topcolor < 223 )
				PaletteHueReplace( lpbmi->bmiColors, topcolor, 192, 223 );
			if ( bottomcolor < 191 )
				PaletteHueReplace( lpbmi->bmiColors, bottomcolor, 160, 191 );

			outbuffer.Put( pDIB, dwBitsSize );
			free( pDIB );
		}

		vgui2::filesystem()->Close( file );

		vgui2::filesystem()->RemoveFile( outfile, "GAMECONFIG" );
		vgui2::filesystem()->CreateDirHierarchy( "models/player", "GAMECONFIG" );
		file = vgui2::filesystem()->Open( outfile, "wb", "GAMECONFIG" );
		if ( file )
		{
			vgui2::filesystem()->Write( outbuffer.Base(), outbuffer.TellPut(), file );
			vgui2::filesystem()->Close( file );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::RemapLogoPalette( char* filename, int r, int g, int b )
{
	char infile[256];
	FileHandle_t file;
	CUtlBuffer outbuffer( 20480, 20480, false );

	_snprintf( infile, sizeof( infile ), "logos/%s.bmp", filename );
	infile[sizeof( infile ) - 1] = 0;

	file = vgui2::filesystem()->Open( infile, "rb", NULL );
	if ( file )
	{
		BITMAPFILEHEADER bmfHeader = {};
		DWORD dwBitsSize, dwFileSize;
		LPBITMAPINFO lpbmi;

		dwFileSize = vgui2::filesystem()->Size( file );

		vgui2::filesystem()->Read( &bmfHeader, sizeof( bmfHeader ), file );
		outbuffer.Put( &bmfHeader, sizeof( bmfHeader ) );

		if ( bmfHeader.bfType == DIB_HEADER_MARKER && dwFileSize >= sizeof( bmfHeader ) + BMP_PALETTE_BYTES )
		{
			dwBitsSize = dwFileSize - sizeof( bmfHeader );

			char* pDIB = (char*)malloc( dwBitsSize );
			vgui2::filesystem()->Read( pDIB, dwBitsSize, file );

			lpbmi = (LPBITMAPINFO)pDIB;

			lpbmi->bmiHeader.biClrUsed = BMP_PALETTE_COLORS;
			float f = 0.0f;
			for ( int i = 0; (DWORD)i < lpbmi->bmiHeader.biClrUsed; i++ )
			{
				float t = f / (float)lpbmi->bmiHeader.biClrUsed;
				lpbmi->bmiColors[i].rgbRed = (unsigned char)(int)( r * t );
				lpbmi->bmiColors[i].rgbGreen = (unsigned char)(int)( g * t );
				lpbmi->bmiColors[i].rgbBlue = (unsigned char)(int)( b * t );
				f += 1.0f;
			}

			outbuffer.Put( pDIB, dwBitsSize );
			free( pDIB );
		}

		vgui2::filesystem()->Close( file );

		char outfile[256];
		sprintf( outfile, "logos/remapped.bmp" );
		vgui2::filesystem()->RemoveFile( outfile, "GAMECONFIG" );
		vgui2::filesystem()->CreateDirHierarchy( "logos", "GAMECONFIG" );
		file = vgui2::filesystem()->Open( outfile, "wb", "GAMECONFIG" );
		if ( file )
		{
			vgui2::filesystem()->Write( outbuffer.Base(), outbuffer.TellPut(), file );
			vgui2::filesystem()->Close( file );
		}
	}
}

void COptionsSubMultiplayer::OnResetData()
{
}

void COptionsSubMultiplayer::ApplyCrosshairColorChanges()
{
	char cmd[256];
	cmd[0] = 0;

	if ( m_displayedCrosshairColorRed != -1 && m_displayedCrosshairColorGreen != -1 && m_displayedCrosshairColorBlue != -1 )
	{
		sprintf( cmd, "%d %d %d", m_displayedCrosshairColorRed, m_displayedCrosshairColorGreen,
			m_displayedCrosshairColorBlue );
		engine->Cvar_Set( (char*)"cl_crosshair_color", cmd );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void COptionsSubMultiplayer::OnApplyChanges()
{
	m_pPrimaryColorSlider->ApplyChanges();
	m_pSecondaryColorSlider->ApplyChanges();
	m_pModelList->ApplyChanges();
	m_pLogoList->ApplyChanges();
	m_pLogoList->GetText( m_LogoName, sizeof( m_LogoName ) );
	m_pColorList->ApplyChanges();
	m_pHighQualityModelCheckBox->ApplyChanges();
	m_pNameTextEntry->ApplyChanges( false );
	if ( m_pCrosshairSize )
		m_pCrosshairSize->ApplyChanges();
	if ( m_pCrosshairTranslucencyCheckbox )
		m_pCrosshairTranslucencyCheckbox->ApplyChanges();

	ApplyCrosshairColorChanges();

	// hack, get command and skip over cvarname
	const char* colorname = m_pColorList->GetActiveItemCommand();
	if ( colorname && colorname[0] )
	{
		// save the logo name
		char cmd[512];
		_snprintf( cmd, sizeof( cmd ) - 1, "cl_logofile %s\n", m_LogoName );
		engine->pfnClientCmd( cmd );

		char infile[256];
		sprintf( infile, "logos/%s.tga", m_LogoName );
		if ( vgui2::filesystem()->FileExists( infile ) )
		{
			UpdateLogoWADFromTGA( infile );
		}
		else
		{
			int r, g, b;
			ColorForName( colorname + sizeof( LOGO_COLOR_COMMAND ) - 1, r, g, b );

			sprintf( infile, "logos/remapped.bmp" );
			FileHandle_t file = vgui2::filesystem()->Open( infile, "rb", NULL );
			if ( file )
			{
				BITMAPFILEHEADER bmfHeader = {};
				DWORD dwBitsSize, dwFileSize;

				dwFileSize = vgui2::filesystem()->Size( file );

				vgui2::filesystem()->Read( &bmfHeader, sizeof( bmfHeader ), file );
				if ( bmfHeader.bfType == DIB_HEADER_MARKER )
				{
					dwBitsSize = dwFileSize - sizeof( bmfHeader );

					char* pDIB = (char*)malloc( dwBitsSize );
					vgui2::filesystem()->Read( pDIB, dwBitsSize, file );

					UpdateLogoWAD( pDIB, (int)dwBitsSize, r, g, b );

					free( pDIB );
				}

				vgui2::filesystem()->Close( file );
			}
		}
	}
}
