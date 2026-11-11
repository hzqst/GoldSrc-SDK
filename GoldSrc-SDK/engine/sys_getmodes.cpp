#if defined( SW_RENDERER )

#include "ivideomode.h"
#include "registry.h"
#include "snd_win.h"
#include "UtlVector.h"
#include "igame.h"
#include "common.h"
#include "conprint.h"
#include "vid.h"
#include "model.h"
#include "engine_color.h"
#include "filesystem.h"
#include "sys.h"

#include "../external/sdl2/SDL.h"
#include "../external/sdl2/SDL_syswm.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <windows.h>
#include <ddraw.h>

#define MAX_VIDEO_MODES		32
#define STARTUP_IMAGE_BYTES	( 256 * 256 * 4 )

extern "C" void VOX_Init();
extern "C" void VID_UpdateWindowVars( RECT* windowRect, int centerX, int centerY );

IVideoMode* videomode;

//-----------------------------------------------------------------------------
// Purpose: Functionality shared by all video modes
//-----------------------------------------------------------------------------
class CVideoMode_Common : public IVideoMode
{
public:
	CVideoMode_Common();
	virtual ~CVideoMode_Common();
	bool Init( void* instance ) override;
	void PlayStartupSequence() override;
	void Shutdown() override;
	bool AddMode( int width, int height, int bitsPerPixel ) override;
	vmode_s* GetCurrentMode() override;
	vmode_s* GetMode( int index ) override;
	int GetModeCount() override;
	bool IsWindowedMode() const override;
	bool GetInitialized() const override;
	void SetInitialized( bool initialized ) override;
	void UpdateWindowPosition() override;
	void FlipScreen() override;
	void RestoreVideo() override;
	void ReleaseVideo() override;
	virtual int MaxBitsPerPixel();

protected:
	virtual void ReleaseFullScreen();
	virtual void ChangeDisplaySettingsToFullscreen();
	void AdjustWindowForMode();
	static float CalculatePixelAspect( unsigned int width, unsigned int height );

private:
	void CenterEngineWindow( HWND window, int width, int height );
	void DrawStartupGraphic( HWND window );
	void LoadStartupGraphic();

	struct bimage_t
	{
		byte*	buffer;
		int		x;
		int		y;
		int		width;
		int		height;
		bool	scaled;
		bool	fit;
	};

	vmode_s	m_rgModeList[MAX_VIDEO_MODES];
	vmode_s	m_safeMode;
	int		m_nNumModes;
	int		m_nGameMode;
	bool	m_bInitialized;

	CUtlVector<bimage_t>	m_ImageID;
	int						m_iBaseResX;
	int						m_iBaseResY;

protected:
	bool	m_bWindowed;
};

class CVideoMode_SoftwareFullScreen : public CVideoMode_Common
{
public:
	CVideoMode_SoftwareFullScreen( bool windowed );
	~CVideoMode_SoftwareFullScreen() override;
	const char* GetName() override;
	bool Init( void* instance ) override;
	void Shutdown() override;
	bool AddMode( int width, int height, int bitsPerPixel ) override;
	void FlipScreen() override;
	void RestoreVideo() override;
	void ReleaseVideo() override;
	int MaxBitsPerPixel() override;

private:
	bool CreateSurfaces();
	void ReleaseSurfaces();
	void RestoreSurfaces();

	LPDIRECTDRAW		m_pDirectDraw;
	LPDIRECTDRAWSURFACE	m_pPrimarySurface;
	LPDIRECTDRAWSURFACE	m_pBackBuffer;
	LPDIRECTDRAWSURFACE	m_pOffscreenSurface;
};

class CVideoMode_SoftwareWindowed : public CVideoMode_Common
{
public:
	CVideoMode_SoftwareWindowed( bool windowed );
	~CVideoMode_SoftwareWindowed() override;
	const char* GetName() override;
	bool Init( void* instance ) override;
	void Shutdown() override;
	void FlipScreen() override;
	int MaxBitsPerPixel() override;

private:
	bool CreateDIB();

	HDC		m_hdcDIB;
	HDC		m_hdcMemory;
	HBITMAP	m_hBitmap;
	void*	m_pBits;

	struct
	{
		BITMAPINFOHEADER	header;
		RGBQUAD			colors[256];
	} m_BitmapInfo;
};

class ILauncherDirectDraw
{
public:
	virtual ~ILauncherDirectDraw()
	{
	}
	virtual bool Init() = 0;
	virtual bool Shutdown() = 0;
	virtual HRESULT EnumModes( int bitsPerPixel ) = 0;
	virtual const char* GetErrorString( HRESULT error ) = 0;
};

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 26495 )
#endif
class CLauncherDirectDraw : public ILauncherDirectDraw
{
public:
	bool Init() override;
	bool Shutdown() override;
	HRESULT EnumModes( int bitsPerPixel ) override;
	const char* GetErrorString( HRESULT error ) override;

private:
	LPDIRECTDRAW	m_pDirectDraw;
};
#ifdef _MSC_VER
#pragma warning( pop )
#endif

static CLauncherDirectDraw g_LauncherDirectDraw;
static int g_EnumBitsPerPixel;

void VideoMode_Create()
{
	bool windowed = registry->ReadInt( "ScreenWindowed", 0 ) != 0;
	if ( COM_CheckParm( "-sw" ) || COM_CheckParm( "-startwindowed" ) || COM_CheckParm( "-windowed" )
		|| COM_CheckParm( "-window" ) )
		windowed = true;
	else if ( COM_CheckParm( "-full" ) || COM_CheckParm( "-fullscreen" ) )
		windowed = false;
	registry->WriteInt( "ScreenWindowed", windowed );
	if ( windowed )
		videomode = new CVideoMode_SoftwareWindowed( true );
	else
		videomode = new CVideoMode_SoftwareFullScreen( false );
	if ( !videomode )
		_wassert( L"videomode", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 2362 );
}

bool CLauncherDirectDraw::Init()
{
	if ( m_pDirectDraw )
		_wassert( L"!m_pDirectDraw", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodesdd.cpp", 82 );
	if ( DirectDrawCreate( NULL, &m_pDirectDraw, NULL ) != DD_OK )
		return false;
	m_pDirectDraw->SetCooperativeLevel( NULL, DDSCL_NORMAL );
	if ( !videomode->IsWindowedMode() )
	{
		SDL_SysWMinfo info;
		SDL_VERSION( &info.version );
		SDL_GetWindowWMInfo( (SDL_Window*)game->GetMainWindow(), &info );
		m_pDirectDraw->SetCooperativeLevel( info.info.win.window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT );
	}
	return true;
}

bool CLauncherDirectDraw::Shutdown()
{
	if ( m_pDirectDraw )
	{
		m_pDirectDraw->FlipToGDISurface();
		m_pDirectDraw->Release();
		m_pDirectDraw = NULL;
	}
	return true;
}

static HRESULT WINAPI EnumDisplayModesCallback( LPDDSURFACEDESC surface, void* context )
{
	int width = surface->dwWidth;
	int height = surface->dwHeight;
	int bpp = surface->ddpfPixelFormat.dwRGBBitCount;
	bool aspect = 9 * width == 16 * height || 3 * width == 4 * height || 4 * width == 5 * height
		|| 5 * width == 8 * height;
	if ( !context )
		_wassert( L"params", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodesdd.cpp", 141 );
	if ( bpp == 15 )
		bpp = 16;
	if ( aspect && height >= 480 && *(int*)context == bpp )
		videomode->AddMode( width, height, bpp );
	return DDENUMRET_OK;
}

HRESULT CLauncherDirectDraw::EnumModes( int bitsPerPixel )
{
	g_EnumBitsPerPixel = bitsPerPixel;
	return m_pDirectDraw->EnumDisplayModes( 0, NULL, &g_EnumBitsPerPixel, EnumDisplayModesCallback );
}

const char* CLauncherDirectDraw::GetErrorString( HRESULT error )
{
	switch ( error )
	{
	case DD_OK: return "No error";
	case DDERR_ALREADYINITIALIZED: return "Object already initialised";
	case DDERR_CANNOTATTACHSURFACE: return "Cannot attack surface";
	case DDERR_CANNOTDETACHSURFACE: return "Cannot detach surface";
	case DDERR_CURRENTLYNOTAVAIL: return "Currently Not Available";
	case DDERR_EXCEPTION: return "Exception has occured";
	case DDERR_GENERIC: return "Generic failure";
	case DDERR_HEIGHTALIGN: return "Height Alignment";
	case DDERR_INCOMPATIBLEPRIMARY: return "Incompatible Primary";
	case DDERR_INVALIDCAPS: return "Invalid Caps";
	case DDERR_INVALIDCLIPLIST: return "Invlaid Clip List";
	case DDERR_INVALIDMODE: return "Invalid Mode";
	case DDERR_INVALIDOBJECT: return "Invalid Object";
	case DDERR_INVALIDPARAMS: return "Invalid Params";
	case DDERR_INVALIDPIXELFORMAT: return "Invalid Pixel Format";
	case DDERR_INVALIDRECT: return "Invalid Rectangle";
	case DDERR_LOCKEDSURFACES: return "Locked Surfaces";
	case DDERR_NO3D: return "No 3d support present";
	case DDERR_NOALPHAHW: return "No Alpha Hardware";
	case DDERR_NOCLIPLIST: return "No Clip List";
	case DDERR_NOCOLORCONVHW: return "No Colour Conversion Hardware";
	case DDERR_NOCOOPERATIVELEVELSET: return "No Cooperative Level Set";
	case DDERR_NOCOLORKEY: return "Surface Has No Colour Key";
	case DDERR_NOCOLORKEYHW: return "No Hardware Colour Key";
	case DDERR_NODIRECTDRAWSUPPORT: return "No Direct Draw Support";
	case DDERR_NOEXCLUSIVEMODE: return "No Exclusive Mode";
	case DDERR_NOFLIPHW: return "No Flip Hardware";
	case DDERR_NOGDI: return "GDI is not present";
	case DDERR_NOMIRRORHW: return "No Mirror Hardware";
	case DDERR_NOTFOUND: return "Requested Item Was No Found";
	case DDERR_NOOVERLAYHW: return "No Overlay Hardware";
	case DDERR_NORASTEROPHW: return "No Raster Op Hardware";
	case DDERR_NOROTATIONHW: return "No Roatation Hardware";
	case DDERR_NOSTRETCHHW: return "No Stretch Hardware";
	case DDERR_NOT4BITCOLOR: return "Not 4 Bit Colour";
	case DDERR_NOT4BITCOLORINDEX: return "Not 4 Bit Colour Index";
	case DDERR_NOT8BITCOLOR: return "Not 8 Bit Colour";
	case DDERR_NOTEXTUREHW: return "No Texture Hardware";
	case DDERR_NOVSYNCHW: return "No VSync Hardware";
	case DDERR_NOZBUFFERHW: return "No Z Buffer Hardware";
	case DDERR_NOZOVERLAYHW: return "No Z Overlay Hardware";
	case DDERR_OUTOFCAPS: return "Hardware Has Already Been Allocated";
	case DDERR_OUTOFMEMORY: return "Out Of Memory";
	case DDERR_OUTOFVIDEOMEMORY: return "Out Of Video Memory";
	case DDERR_OVERLAYCANTCLIP: return "Hardware can't clip overlays";
	case DDERR_OVERLAYCOLORKEYONLYONEACTIVE: return "Can only have colour key active at one time for overlays";
	case DDERR_PALETTEBUSY: return "Palette Locked by another thread";
	case DDERR_COLORKEYNOTSET: return "Colour key is not set";
	case DDERR_SURFACEALREADYATTACHED: return "Surface already attached";
	case DDERR_SURFACEALREADYDEPENDENT: return "Surface Already Dependent";
	case DDERR_SURFACEBUSY: return "Surface Is Busy";
	case DDERR_SURFACEISOBSCURED: return "Surface Is Obscured";
	case DDERR_SURFACELOST: return "Surface Lost";
	case DDERR_SURFACENOTATTACHED: return "Surface Not Attached";
	case DDERR_TOOBIGHEIGHT: return "Height Too Big";
	case DDERR_TOOBIGSIZE: return "Size Too Big - Height and Width are individually OK";
	case DDERR_TOOBIGWIDTH: return "Width Too Big";
	case DDERR_UNSUPPORTED: return "Unsupported Operation";
	case DDERR_UNSUPPORTEDFORMAT: return "Unsupported Format";
	case DDERR_UNSUPPORTEDMASK: return "Unsupported Mask Format";
	case DDERR_VERTICALBLANKINPROGRESS: return "Vertical Blank In Progress";
	case DDERR_WASSTILLDRAWING: return "Still Drawing";
	case DDERR_XALIGN: return "X Align";
	case DDERR_INVALIDDIRECTDRAWGUID: return "Invalid Direct Draw GUID";
	case DDERR_DIRECTDRAWALREADYCREATED: return "Direct Draw Already Created";
	case DDERR_NODIRECTDRAWHW: return "No Direct Draw HardWare";
	case DDERR_PRIMARYSURFACEALREADYEXISTS: return "Primary Surface Already Exists";
	case DDERR_NOEMULATION: return "No Emulation";
	case DDERR_REGIONTOOSMALL: return "Region Too Small";
	case DDERR_CLIPPERISUSINGHWND: return "Clipper is using a HWND";
	case DDERR_NOCLIPPERATTACHED: return "No Clipper Attached";
	case DDERR_NOHWND: return "No HWND set";
	case DDERR_HWNDSUBCLASSED: return "HWND Sub-Classed";
	case DDERR_HWNDALREADYSET: return "HWND Already Set";
	case DDERR_NOPALETTEATTACHED: return "No Palette Attrached";
	case DDERR_NOPALETTEHW: return "No Palette Hardware";
	case DDERR_BLTFASTCANTCLIP: return "Blit Fast Can't Clip";
	case DDERR_NOBLTHW: return "No Blit Hardware";
	case DDERR_NODDROPSHW: return "No Direct Draw Raster OP Hardware";
	case DDERR_OVERLAYNOTVISIBLE: return "Overlay Not Visible";
	case DDERR_NOOVERLAYDEST: return "No Overlay Destination";
	case DDERR_INVALIDPOSITION: return "Invlaid Position";
	case DDERR_NOTAOVERLAYSURFACE: return "Not An Overlay Surface";
	case DDERR_EXCLUSIVEMODEALREADYSET: return "Exclusive Mode Already Set";
	case DDERR_NOTFLIPPABLE: return "Not Flippable";
	case DDERR_CANTDUPLICATE: return "Can't Duplicate";
	case DDERR_NOTLOCKED: return "Not Locked";
	case DDERR_CANTCREATEDC: return "Can't Create DC";
	case DDERR_NODC: return "No DC has ever Existed";
	case DDERR_WRONGMODE: return "Wrong Mode";
	case DDERR_IMPLICITLYCREATED: return "Surface Cannot Be Restored";
	case DDERR_NOTPALETTIZED: return "Not Palettized";
	case DDERR_UNSUPPORTEDMODE: return "Unsupported Mode";
	default: return "unk";
	}
}

bool CVideoMode_Common::AddMode( int width, int height, int bitsPerPixel )
{
	if ( m_nNumModes >= MAX_VIDEO_MODES )
		return false;
	for ( int i = 0; i < m_nNumModes; i++ )
	{
		if ( m_rgModeList[i].width == width && m_rgModeList[i].height == height && m_rgModeList[i].bpp == bitsPerPixel )
			return true;
	}
	vmode_s& mode = m_rgModeList[m_nNumModes++];
	mode.width = width;
	mode.height = height;
	mode.bpp = bitsPerPixel;
	return true;
}

bool CVideoMode_SoftwareFullScreen::AddMode( int width, int height, int bitsPerPixel )
{
	if ( width > 1599 || height > 1199 )
		return true;
	return CVideoMode_Common::AddMode( width, height, bitsPerPixel );
}

vmode_s* CVideoMode_Common::GetMode( int index )
{
	if ( index >= m_nNumModes || index < 0 )
		return &m_safeMode;
	return &m_rgModeList[index];
}

vmode_s* CVideoMode_Common::GetCurrentMode()
{
	return GetMode( m_nGameMode );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CVideoMode_Common::GetModeCount()
{
	return m_nNumModes;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::IsWindowedMode() const
{
	return m_bWindowed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CVideoMode_Common::GetInitialized() const
{
	return m_bInitialized;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : init - 
//-----------------------------------------------------------------------------
void CVideoMode_Common::SetInitialized( bool initialized )
{
	m_bInitialized = initialized;
}

int CVideoMode_Common::MaxBitsPerPixel()
{
	return 32;
}

int CVideoMode_SoftwareWindowed::MaxBitsPerPixel()
{
	return 32;
}

int CVideoMode_SoftwareFullScreen::MaxBitsPerPixel()
{
	return 16;
}

float CVideoMode_Common::CalculatePixelAspect( unsigned int width, unsigned int height )
{
	if ( width > 639 )
		return 1.0f;
	return ( (float)height / (float)width ) * 1.3333334f;
}

void SetupSDLVideoModes()
{
	SDL_DisplayMode mode;
	SDL_InitSubSystem( SDL_INIT_VIDEO );
	int count = SDL_GetNumDisplayModes( 0 );
	for ( int i = 0; i < count; i++ )
	{
		if ( !SDL_GetDisplayMode( 0, i, &mode ) && !videomode->AddMode( mode.w, mode.h, 32 ) )
			return;
	}
	SDL_QuitSubSystem( SDL_INIT_VIDEO );
}

//-----------------------------------------------------------------------------
// Purpose: Compares video modes so we can sort the list
// Input  : *arg1 - 
//			*arg2 - 
// Output : static int
//-----------------------------------------------------------------------------
static int VideoModeCompare( const void* arg1, const void* arg2 )
{
	const vmode_s* mode1 = (const vmode_s*)arg1;
	const vmode_s* mode2 = (const vmode_s*)arg2;
	if ( mode1->width < mode2->width )
		return -1;
	if ( mode1->width > mode2->width )
		return 1;
	if ( mode1->height < mode2->height )
		return -1;
	if ( mode1->height > mode2->height )
		return 1;
	return 0;
}

bool CVideoMode_Common::Init( void* )
{
	if ( !game->CreateGameWindow() )
		return false;

	SDL_DisplayMode desktop;
	SDL_GetDesktopDisplayMode( 0, &desktop );
	m_safeMode.width = desktop.w;
	m_safeMode.height = desktop.h;
	m_safeMode.bpp = 32;
	int width = registry->ReadInt( "DisplayWidth", desktop.w );
	int height = registry->ReadInt( "DisplayHeight", desktop.h );
	registry->ReadInt( "DisplayBPP", 32 );
	COM_CheckParm( "-16bpp" );
	COM_CheckParm( "-24bpp" );
	COM_CheckParm( "-32bpp" );
	MaxBitsPerPixel();

	m_nNumModes = 0;
	SetupSDLVideoModes();
	if ( m_nNumModes >= 2 )
		qsort( m_rgModeList, m_nNumModes, sizeof( m_rgModeList[0] ), VideoModeCompare );

	int arg = COM_CheckParm( "-width" );
	if ( arg )
		width = atoi( com_argv[arg + 1] );
	arg = COM_CheckParm( "-w" );
	if ( arg )
		width = atoi( com_argv[arg + 1] );
	arg = COM_CheckParm( "-height" );
	if ( arg )
		height = atoi( com_argv[arg + 1] );
	arg = COM_CheckParm( "-h" );
	if ( arg )
		height = atoi( com_argv[arg + 1] );
	if ( width > 5120 )
		width = 5120;
	if ( height > 1600 )
		height = 1600;

	if ( IsWindowedMode() )
	{
		SDL_Rect bounds;
		if ( !SDL_GetDisplayBounds( 0, &bounds ) )
		{
			if ( width > bounds.w )
				width = bounds.w;
			if ( height > bounds.h )
				height = bounds.h;
		}
	}
	if ( width > 0 && height < 0 )
		height = (int)( (double)width * 3.0 * 0.25 );

	int mode = -1;
	for ( int i = 0; i < m_nNumModes; i++ )
	{
		if ( m_rgModeList[i].width == width )
		{
			mode = i;
			if ( m_rgModeList[i].height == height )
				break;
		}
	}
	if ( mode < 0 )
		mode = 0;
	m_nGameMode = mode;
	if ( ( IsWindowedMode() || !COM_CheckParm( "-nofbo" ) ) && ( COM_CheckParm( "-forceres" ) || m_nNumModes < 2 ) )
	{
		m_rgModeList[mode].width = width;
		m_rgModeList[mode].height = height;
		if ( !m_nNumModes )
			m_nNumModes = 1;
	}
	registry->WriteInt( "DisplayWidth", m_rgModeList[mode].width );
	registry->WriteInt( "DisplayHeight", m_rgModeList[mode].height );
	registry->WriteInt( "DisplayBPP", m_rgModeList[mode].bpp );
	LoadStartupGraphic();
	AdjustWindowForMode();
	return true;
}

void CVideoMode_Common::CenterEngineWindow( HWND window, int width, int height )
{
	SDL_Rect bounds;
	SDL_GetDisplayBounds( 0, &bounds );
	int x = ( bounds.w - width ) / 2;
	int y = ( bounds.h - height ) / 2;
	if ( x < 0 )
		x = 0;
	if ( y < 0 )
		y = 0;
	game->SetWindowXY( x, y );
	SDL_SetWindowPosition( (SDL_Window*)window, x, y );
}

void CVideoMode_Common::AdjustWindowForMode()
{
	vmode_s* mode = GetCurrentMode();
	if ( !mode )
		_wassert( L"mode", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 586 );
	Snd_ReleaseBuffer();
	vid.width = mode->width;
	vid.height = mode->height;
	int width, height;
	if ( IsWindowedMode() )
	{
		width = m_rgModeList[m_nGameMode].width;
		height = m_rgModeList[m_nGameMode].height;
	}
	else
	{
		width = mode->width;
		height = mode->height;
		ChangeDisplaySettingsToFullscreen();
		SDL_SetWindowPosition( (SDL_Window*)game->GetMainWindow(), 0, 0 );
	}
	game->SetWindowSize( width, height );
	CenterEngineWindow( game->GetMainWindow(), width, height );
	Snd_AcquireBuffer();
	VOX_Init();
	UpdateWindowPosition();
}

void CVideoMode_Common::LoadStartupGraphic()
{
	FileHandle_t file = FS_Open( "resource/BackgroundLoadingLayout.txt", "rt" );
	if ( !file )
		return;
	unsigned int size = FS_Size( file );
	char* buffer = (char*)_alloca( size + 1 );
	memset( buffer, 0, size + 1 );
	FS_Read( buffer, size, 1, file );
	FS_Close( file );
	char* data = buffer;
	char token[512];
	while ( data && *data )
	{
		data = FS_ParseFile( data, token, NULL );
		if ( !data || !token[0] )
			break;
		if ( !_stricmp( token, "resolution" ) )
		{
			data = FS_ParseFile( data, token, NULL );
			m_iBaseResX = atoi( token );
			data = FS_ParseFile( data, token, NULL );
			m_iBaseResY = atoi( token );
		}
		else
		{
			bimage_t& image = m_ImageID[m_ImageID.AddToTail()];
			image.buffer = (byte*)malloc( STARTUP_IMAGE_BYTES );
			if ( !LoadTGA( token, image.buffer, STARTUP_IMAGE_BYTES, &image.width, &image.height ) )
			{
				free( image.buffer );
				m_ImageID.Remove( m_ImageID.Count() - 1 );
				return;
			}
			data = FS_ParseFile( data, token, NULL );
			image.scaled = !_stricmp( token, "scaled" );
			image.fit = !_stricmp( token, "fit" );
			data = FS_ParseFile( data, token, NULL );
			image.x = atoi( token );
			data = FS_ParseFile( data, token, NULL );
			image.y = atoi( token );
		}
	}
}

void CVideoMode_Common::DrawStartupGraphic( HWND window )
{
	if ( !m_ImageID.Count() )
		return;
	SDL_SysWMinfo info;
	SDL_VERSION( &info.version );
	SDL_GetWindowWMInfo( (SDL_Window*)window, &info );
	vmode_s* mode = GetCurrentMode();
	HDC dc = GetDC( info.info.win.window );
	HDC memoryDC = CreateCompatibleDC( dc );
	HBITMAP bitmap = CreateCompatibleBitmap( dc, mode->width, mode->height );
	SelectObject( memoryDC, bitmap );

	float xScale = (float)mode->width / (float)m_iBaseResX;
	float yScale = (float)mode->height / (float)m_iBaseResY;
	float fitScale = yScale;
	float fitWidth = (float)m_iBaseResX * fitScale;
	float fitHeight = (float)m_iBaseResY * fitScale;
	if ( (float)mode->width > fitWidth )
	{
		fitScale = xScale;
		fitWidth = (float)m_iBaseResX * fitScale;
		fitHeight = (float)m_iBaseResY * fitScale;
	}
	int fitX = mode->width / 2 - (int)( fitWidth * 0.5f );
	int fitY = mode->height / 2 - (int)( fitHeight * 0.5f );

	for ( int i = 0; i < m_ImageID.Count(); i++ )
	{
		bimage_t& image = m_ImageID[i];
		int x, y, right, bottom;
		if ( image.scaled )
		{
			x = image.x ? (int)ceil( (double)( image.x * xScale ) ) : 0;
			y = image.y ? (int)ceil( (double)( image.y * yScale ) ) : 0;
			right = (int)ceil( (double)( ( image.x + image.width ) * xScale ) );
			bottom = (int)ceil( (double)( ( image.y + image.height ) * yScale ) );
		}
		else if ( image.fit )
		{
			x = fitX + ( image.x ? (int)ceil( (double)( image.x * fitScale ) ) : 0 );
			y = fitY + ( image.y ? (int)ceil( (double)( image.y * fitScale ) ) : 0 );
			right = x + (int)ceil( (double)( image.width * fitScale ) );
			bottom = y + (int)ceil( (double)( image.height * fitScale ) );
		}
		else
		{
			x = image.x;
			y = image.y;
			right = x + image.width;
			bottom = y + image.height;
		}
		int width = right - x;
		int height = bottom - y;
		if ( !image.buffer )
			_wassert( L"rgba", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 748 );
		float rowScale = (float)image.height / (float)height;
		int columnStep = (int)( ( (float)image.width / (float)width ) * 256.0f );
		for ( int row = 0; row < height; row++ )
		{
			byte* source = image.buffer + 4 * image.width * (int)( row * rowScale );
			int column = 0;
			for ( int pixel = 0; pixel < width; pixel++ )
			{
				byte* rgba = source + 4 * ( column >> 8 );
				SetPixel( memoryDC, x + pixel, y + row, rgba[0] | ( rgba[1] << 8 ) | ( rgba[2] << 16 ) );
				column += columnStep;
			}
		}
	}
	SetViewportOrgEx( dc, 0, 0, NULL );
	BitBlt( dc, 0, 0, mode->width, mode->height, memoryDC, 0, 0, SRCCOPY );
	DeleteObject( bitmap );
	DeleteDC( memoryDC );
	ReleaseDC( window, dc );
	for ( int i = 0; i < m_ImageID.Count(); i++ )
	{
		if ( m_ImageID[i].buffer )
			free( m_ImageID[i].buffer );
	}
	m_ImageID.Purge();
}

void VideoMode_GetCurrentRenderer( char* name, int nameLength, int* windowed, int* hdModels, int* addons )
{
	if ( name && nameLength > 0 )
		strncpy( name, videomode->GetName(), nameLength );
	if ( windowed )
		*windowed = videomode->IsWindowedMode();
	if ( hdModels )
		*hdModels = registry->ReadInt( "hdmodels", 0 ) > 0;
	if ( addons )
		*addons = registry->ReadInt( "addons_folder", 0 ) > 0;
}

void VideoMode_GetVideoModes( vmode_s** modes, int* count )
{
	*count = videomode->GetModeCount();
	*modes = videomode->GetMode( 0 );
}

extern "C" void VideoMode_RestoreVideo()
{
	videomode->RestoreVideo();
}

extern "C" int VideoMode_IsWindowed()
{
	return videomode && videomode->IsWindowedMode();
}

extern "C" qboolean R_UseLegacyRender()
{
	return game->OnlyLegacyRender();
}

extern "C" void VideoMode_GetCurrentVideoMode( int* width, int* height, int* bitsPerPixel )
{
	vmode_s* mode = videomode->GetCurrentMode();
	if ( !mode )
		return;
	if ( width )
		*width = mode->width;
	if ( height )
		*height = mode->height;
	if ( bitsPerPixel )
		*bitsPerPixel = mode->bpp;
}

extern "C" void VideoMode_SetVideoMode( int width, int height, int bitsPerPixel )
{
	registry->WriteInt( "DisplayWidth", width );
	registry->WriteInt( "DisplayHeight", height );
	registry->WriteInt( "DisplayBPP", bitsPerPixel );
}

extern "C" void VideoMode_SwitchMode( int hardware, int windowed )
{
	if ( hardware )
	{
		registry->WriteString( "EngineDLL", "hw.dll" );
		registry->WriteInt( "EngineD3D", hardware == 2 );
	}
	else
	{
		registry->WriteString( "EngineDLL", "sw.dll" );
		registry->WriteInt( "EngineD3D", 0 );
	}
	registry->WriteInt( "ScreenWindowed", windowed );
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CVideoMode_Common::CVideoMode_Common()
	: m_nNumModes( 0 ), m_nGameMode( 0 ), m_bInitialized( false )
{
	m_bWindowed = false;
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode( 0, &mode );
	m_safeMode.width = mode.w;
	m_safeMode.height = mode.h;
	m_safeMode.bpp = 32;
}

CVideoMode_Common::~CVideoMode_Common()
{
}

CVideoMode_SoftwareFullScreen::CVideoMode_SoftwareFullScreen( bool windowed )
	: m_pDirectDraw( NULL ), m_pPrimarySurface( NULL ), m_pBackBuffer( NULL ), m_pOffscreenSurface( NULL )
{
	m_bWindowed = windowed;
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CVideoMode_SoftwareWindowed::CVideoMode_SoftwareWindowed( bool windowed )
	: m_hdcDIB( NULL ), m_hdcMemory( NULL ), m_hBitmap( NULL ), m_pBits( NULL )
{
	m_bWindowed = windowed;
}

CVideoMode_SoftwareFullScreen::~CVideoMode_SoftwareFullScreen()
{
}

CVideoMode_SoftwareWindowed::~CVideoMode_SoftwareWindowed()
{
}

const char* CVideoMode_SoftwareFullScreen::GetName()
{
	return "software";
}

const char* CVideoMode_SoftwareWindowed::GetName()
{
	return "software";
}

void CVideoMode_Common::FlipScreen()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_Common::RestoreVideo()
{
}

void CVideoMode_Common::ReleaseVideo()
{
}

void CVideoMode_Common::ReleaseFullScreen()
{
}

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen()
{
}

void CVideoMode_Common::UpdateWindowPosition()
{
	int x, y, width, height;
	// Get the window from the game ( right place for it? )
	game->GetWindowRect( &x, &y, &width, &height );
	RECT rect;
	rect.left = x;
	rect.top = y;
	rect.right = x + width;
	rect.bottom = y + height;
	VID_UpdateWindowVars( &rect, x + width / 2, y + height / 2 );
}

void CVideoMode_Common::PlayStartupSequence()
{
	if ( !COM_CheckParm( "-novid" ) )
		game->PlayStartupVideos();
	DrawStartupGraphic( game->GetMainWindow() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVideoMode_Common::Shutdown()
{
	ReleaseFullScreen();
	if ( !GetInitialized() )
		return;
	SetInitialized( false );
	m_nGameMode = 0;
	videomode = NULL;
	delete this;
}

bool CVideoMode_SoftwareFullScreen::Init( void* instance )
{
	bool initialized = CVideoMode_Common::Init( instance );
	if ( !initialized )
	{
		SetInitialized( false );
		return false;
	}
	if ( DirectDrawCreate( NULL, &m_pDirectDraw, NULL ) != DD_OK )
	{
		SetInitialized( false );
		return false;
	}
	vmode_s* mode = GetCurrentMode();
	if ( !mode )
	{
		SetInitialized( false );
		return false;
	}

	mode->bpp = 16;
	if ( IsWindowedMode() )
	{
		m_pDirectDraw->SetCooperativeLevel( NULL, DDSCL_NORMAL );
	}
	else
	{
		SDL_SysWMinfo info;
		SDL_VERSION( &info.version );
		SDL_GetWindowWMInfo( (SDL_Window*)game->GetMainWindow(), &info );
		m_pDirectDraw->SetCooperativeLevel( info.info.win.window,
			DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT );
		if ( m_pDirectDraw->SetDisplayMode( mode->width, mode->height, mode->bpp ) != DD_OK )
		{
			mode->bpp = 32;
			if ( m_pDirectDraw->SetDisplayMode( mode->width, mode->height, 32 ) != DD_OK )
			{
				SetInitialized( false );
				return false;
			}
		}
		SDL_SetWindowFullscreen( (SDL_Window*)game->GetMainWindow(), SDL_WINDOW_FULLSCREEN );
	}
	m_pDirectDraw->FlipToGDISurface();
	initialized = CreateSurfaces();
	SetInitialized( initialized );
	return initialized;
}

bool CVideoMode_SoftwareWindowed::Init( void* instance )
{
	bool initialized = CVideoMode_Common::Init( instance );
	if ( initialized )
		initialized = CreateDIB();
	SetInitialized( initialized );
	return initialized;
}

bool CVideoMode_SoftwareWindowed::CreateDIB()
{
	vmode_s* mode = GetCurrentMode();
	if ( !mode )
		_wassert( L"mode", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 1621 );
	vid.buffer = NULL;
	vid.conrowbytes = 0;
	vid.rowbytes = 0;
	vid.numpages = 1;
	vid.bits = mode->bpp;
	vid.maxwarpwidth = 320;
	vid.maxwarpheight = 200;
	vid.conheight = vid.height = mode->height;
	vid.conwidth = vid.width = mode->width;
	vid.aspect = CalculatePixelAspect( vid.width, vid.height );
	memset( &m_BitmapInfo, 0, sizeof( m_BitmapInfo ) );
	SDL_SysWMinfo info;
	SDL_VERSION( &info.version );
	SDL_GetWindowWMInfo( (SDL_Window*)game->GetMainWindow(), &info );
	m_hdcDIB = GetDC( info.info.win.window );
	m_BitmapInfo.header.biSize = sizeof( m_BitmapInfo.header );
	m_BitmapInfo.header.biWidth = vid.width;
	m_BitmapInfo.header.biHeight = vid.height;
	m_BitmapInfo.header.biPlanes = 1;
	m_BitmapInfo.header.biBitCount = 16;
	m_BitmapInfo.header.biCompression = BI_BITFIELDS;
	DWORD* masks = (DWORD*)m_BitmapInfo.colors;
	masks[0] = COLOR_REDMASK_565;
	masks[1] = COLOR_GREENMASK_565;
	masks[2] = COLOR_BLUEMASK;
	m_hBitmap = CreateDIBSection( m_hdcDIB, (BITMAPINFO*)&m_BitmapInfo, DIB_RGB_COLORS, &m_pBits, NULL, 0 );
	if ( !m_hBitmap )
		return false;
	vid.buffer = (byte*)m_pBits;
	vid.is15bit = 0;
	vid.rowbytes = 2 * vid.width;
	vid.conrowbytes = 2 * vid.width;
	memset( m_pBits, 255, vid.height * vid.width );
	m_hdcMemory = CreateCompatibleDC( m_hdcDIB );
	if ( !m_hdcMemory )
		return false;
	SelectObject( m_hdcMemory, m_hBitmap );
	vid.direct = vid.buffer;
	vid.conbuffer = vid.buffer;
	vid.vidtype = VT_Software;
	return true;
}

void CVideoMode_SoftwareWindowed::FlipScreen()
{
	if ( !m_hdcDIB )
		_wassert( L"ghdcDIB", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 1582 );
	for ( int row = 0; row < (int)vid.height; row++ )
		BitBlt( m_hdcDIB, 0, row, vid.width, 1, m_hdcMemory, 0, vid.height - row - 1, SRCCOPY );
}

void CVideoMode_SoftwareWindowed::Shutdown()
{
	if ( m_hdcMemory )
	{
		DeleteDC( m_hdcMemory );
		m_hdcMemory = NULL;
	}
	if ( m_hBitmap )
	{
		DeleteObject( m_hBitmap );
		m_hBitmap = NULL;
		m_pBits = NULL;
	}
	if ( m_hdcDIB )
	{
		SDL_SysWMinfo info;
		SDL_VERSION( &info.version );
		SDL_GetWindowWMInfo( (SDL_Window*)game->GetMainWindow(), &info );
		ReleaseDC( info.info.win.window, m_hdcDIB );
		m_hdcDIB = NULL;
	}
	CVideoMode_Common::Shutdown();
}

void CVideoMode_SoftwareFullScreen::Shutdown()
{
	if ( m_pDirectDraw )
	{
		m_pDirectDraw->FlipToGDISurface();
		ReleaseSurfaces();
		m_pDirectDraw->Release();
		m_pDirectDraw = NULL;
	}
	CVideoMode_Common::Shutdown();
}

void CVideoMode_SoftwareFullScreen::ReleaseVideo()
{
	if ( m_pDirectDraw )
		m_pDirectDraw->RestoreDisplayMode();
	if ( !IsWindowedMode() )
		SDL_SetWindowFullscreen( (SDL_Window*)game->GetMainWindow(), 0 );
}

void CVideoMode_SoftwareFullScreen::RestoreSurfaces()
{
	if ( m_pDirectDraw && m_pBackBuffer->Restore() != DD_OK )
	{
		CreateSurfaces();
		m_pBackBuffer->Restore();
	}
}

void CVideoMode_SoftwareFullScreen::ReleaseSurfaces()
{
	if ( m_pOffscreenSurface )
	{
		m_pOffscreenSurface->Unlock( NULL );
		m_pOffscreenSurface->Release();
		m_pOffscreenSurface = NULL;
	}
	if ( m_pPrimarySurface )
	{
		m_pPrimarySurface->DeleteAttachedSurface( 0, m_pBackBuffer );
		m_pBackBuffer->Release();
		m_pBackBuffer = NULL;
		m_pPrimarySurface->Release();
		m_pPrimarySurface = NULL;
	}
}

void CVideoMode_SoftwareFullScreen::RestoreVideo()
{
	if ( !GetInitialized() )
		return;
	RestoreSurfaces();
	if ( !IsWindowedMode() )
		SDL_SetWindowFullscreen( (SDL_Window*)game->GetMainWindow(), SDL_WINDOW_FULLSCREEN );
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4701 )
#endif
bool CVideoMode_SoftwareFullScreen::CreateSurfaces()
{
	if ( !m_pDirectDraw )
		return false;
	vmode_s* mode = GetCurrentMode();
	mode->bpp = 16;
	ReleaseSurfaces();
	SDL_SysWMinfo info;
	SDL_VERSION( &info.version );
	SDL_GetWindowWMInfo( (SDL_Window*)game->GetMainWindow(), &info );
	HRESULT error = m_pDirectDraw->SetCooperativeLevel( info.info.win.window,
		DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT );
	if ( error != DD_OK )
	{
		if ( error == DDERR_HWNDALREADYSET )
		{
			DestroyWindow( info.info.win.window );
			MessageBoxA( NULL, "Restart the game to use your newly selected mode.", "Error", 0 );
		}
		Con_Printf( "%s\n", g_LauncherDirectDraw.GetErrorString( error ) );
		return false;
	}
	if ( m_pDirectDraw->SetDisplayMode( mode->width, mode->height, mode->bpp ) != DD_OK )
	{
		mode->bpp = 32;
		error = m_pDirectDraw->SetDisplayMode( mode->width, mode->height, 32 );
		if ( error != DD_OK )
		{
			Con_Printf( "display:%s\n", g_LauncherDirectDraw.GetErrorString( error ) );
			return false;
		}
	}
	DDSURFACEDESC desc = {};
	desc.dwSize = sizeof( desc );
	desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
	desc.dwBackBufferCount = 1;
	error = m_pDirectDraw->CreateSurface( &desc, &m_pPrimarySurface, NULL );
	if ( error != DD_OK )
	{
		Con_Printf( "surface:%s\n", g_LauncherDirectDraw.GetErrorString( error ) );
		return false;
	}
	DDBLTFX effect;
	effect.dwSize = sizeof( effect );
	effect.dwFillColor = 0;
	m_pPrimarySurface->Blt( NULL, NULL, NULL, DDBLT_COLORFILL, &effect );
	DDSCAPS caps;
	caps.dwCaps = DDSCAPS_BACKBUFFER;
	vid.numpages = m_pPrimarySurface->GetAttachedSurface( &caps, &m_pBackBuffer ) == DD_OK;
	desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
	desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
	desc.dwWidth = mode->width;
	desc.dwHeight = mode->height;
	if ( mode->bpp == 32 )
	{
		desc.dwFlags |= DDSD_PIXELFORMAT;
		desc.ddpfPixelFormat.dwSize = sizeof( desc.ddpfPixelFormat );
		desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
		desc.ddpfPixelFormat.dwFourCC = 0;
		desc.ddpfPixelFormat.dwRGBBitCount = 16;
		desc.ddpfPixelFormat.dwRBitMask = COLOR_REDMASK_565;
		desc.ddpfPixelFormat.dwGBitMask = COLOR_GREENMASK_565;
		desc.ddpfPixelFormat.dwBBitMask = COLOR_BLUEMASK;
		desc.ddpfPixelFormat.dwRGBAlphaBitMask = 0;
	}
	error = m_pDirectDraw->CreateSurface( &desc, &m_pOffscreenSurface, NULL );
	DDPIXELFORMAT format;
	format.dwSize = sizeof( format );
	if ( error == DD_OK )
		error = m_pOffscreenSurface->Lock( NULL, &desc, DDLOCK_WAIT, NULL );
	if ( error == DD_OK )
		error = m_pOffscreenSurface->GetPixelFormat( &format );
	if ( error != DD_OK )
	{
		Con_Printf( "%s\n", g_LauncherDirectDraw.GetErrorString( error ) );
		return false;
	}
	vid.direct = (byte*)desc.lpSurface;
	vid.conbuffer = (byte*)desc.lpSurface;
	vid.buffer = (byte*)desc.lpSurface;
	vid.is15bit = format.dwGBitMask == COLOR_GREENMASK_555;
	vid.conrowbytes = desc.lPitch;
	vid.rowbytes = desc.lPitch;
	memset( desc.lpSurface, 0, 2 * mode->height * mode->width );
	vid.maxwarpwidth = 320;
	vid.maxwarpheight = 200;
	vid.conheight = vid.height = mode->height;
	vid.conwidth = vid.width = mode->width;
	vid.aspect = CalculatePixelAspect( vid.width, vid.height );
	vid.bits = mode->bpp;
	vid.vidtype = VT_Software;
	return true;
}
#if defined( _MSC_VER )
#pragma warning( pop )
#endif

void CVideoMode_SoftwareFullScreen::FlipScreen()
{
	RECT rect = { 0, 0, (LONG)vid.width, (LONG)vid.height };
	if ( !game->IsActiveApp() )
		return;
	if ( ( !m_pOffscreenSurface || !m_pBackBuffer ) && m_pDirectDraw && m_pBackBuffer->Restore() != DD_OK )
	{
		CreateSurfaces();
		m_pBackBuffer->Restore();
	}
	if ( !m_pOffscreenSurface || !m_pBackBuffer )
		return;
	m_pOffscreenSurface->Unlock( NULL );
	bool restored = false;
	HRESULT error = m_pBackBuffer->BltFast( 0, 0, m_pOffscreenSurface, &rect, DDBLTFAST_WAIT );
	if ( error == DDERR_SURFACELOST )
	{
		RestoreSurfaces();
		restored = true;
		m_pBackBuffer->BltFast( 0, 0, m_pOffscreenSurface, &rect, DDBLTFAST_WAIT );
		if ( vid.numpages != 1 )
			return;
	}
	if ( vid.numpages == 1 )
	{
		if ( m_pPrimarySurface->Flip( NULL, DDFLIP_WAIT ) == DDERR_SURFACELOST )
		{
			RestoreSurfaces();
			m_pPrimarySurface->Flip( NULL, DDFLIP_WAIT );
			return;
		}
	}
	if ( restored && vid.numpages == 1 )
		return;
	DDSURFACEDESC desc = {};
	desc.dwSize = sizeof( desc );
	m_pOffscreenSurface->Lock( NULL, &desc, DDLOCK_WAIT, NULL );
	vid.direct = (byte*)desc.lpSurface;
	vid.conbuffer = (byte*)desc.lpSurface;
	vid.buffer = (byte*)desc.lpSurface;
	vid.rowbytes = desc.lPitch;
}

#else

#include "ivideomode.h"
#include "registry.h"
#include "snd_win.h"
#include "UtlVector.h"
#include "igame.h"
#include "common.h"
#include "vid.h"
#include "gl_model.h"
#include "filesystem.h"
#include "sys.h"
#include "../external/sdl2/SDL.h"
#include "../external/SDL2/SDL_opengl.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <windows.h>

#define MAX_VIDEO_MODES 32

#define STARTUP_IMAGE_BYTES ( 256 * 256 * 4 )

extern "C" void VOX_Init();
extern "C" void VID_UpdateWindowVars( RECT* windowRect, int centerX, int centerY );
extern int gD3DMode;

IVideoMode* videomode;
int bNeedsFullScreenModeSwitch;

class CVideoMode_Common : public IVideoMode
{
public:
	CVideoMode_Common();
	virtual ~CVideoMode_Common();
	bool Init( void* ) override;
	void PlayStartupSequence() override;
	void Shutdown() override;
	bool AddMode( int width, int height, int bitsPerPixel ) override;
	vmode_s* GetMode( int index ) override;
	int GetModeCount() override;
	bool IsWindowedMode() const override;
	bool GetInitialized() const override;
	void SetInitialized( bool initialized ) override;
	void UpdateWindowPosition() override;
	void FlipScreen() override;
	void RestoreVideo() override;
	void ReleaseVideo() override;
	virtual int MaxBitsPerPixel();

protected:
	virtual void ReleaseFullScreen();
	virtual void ChangeDisplaySettingsToFullscreen();
	vmode_s* GetCurrentMode() override;
	void AdjustWindowForMode();
	static float CalculatePixelAspect( unsigned int width, unsigned int height );

private:
	void CenterEngineWindow( HWND window, int width, int height );
	void DrawStartupGraphic( HWND window );
	void LoadStartupGraphic();

	struct bimage_t
	{
		unsigned char* buffer;
		int x;
		int y;
		int width;
		int height;
		bool scaled;
		bool fit;
	};

	vmode_s m_rgModeList[MAX_VIDEO_MODES];
	vmode_s m_safeMode;
	int m_nNumModes;
	int m_nGameMode;
	bool m_bInitialized;
	CUtlVector<bimage_t> m_ImageID;
	int m_iBaseResX;
	int m_iBaseResY;

protected:
	bool m_bWindowed;
};

class CVideoMode_OpenGL : public CVideoMode_Common
{
public:
	CVideoMode_OpenGL( bool windowed );
	~CVideoMode_OpenGL() override;
	const char* GetName() override;
	bool Init( void* ) override;
	void ReleaseVideo() override;
	void RestoreVideo() override;
	void FlipScreen() override;

private:
	void ReleaseFullScreen() override;
	void ChangeDisplaySettingsToFullscreen() override;
};

void VideoMode_Create()
{
	bool windowed = registry->ReadInt( "ScreenWindowed", 0 ) != 0;

	if ( COM_CheckParm( "-sw" ) || COM_CheckParm( "-startwindowed" ) || COM_CheckParm( "-windowed" )
		 || COM_CheckParm( "-window" ) )
		windowed = true;
	else if ( COM_CheckParm( "-full" ) || COM_CheckParm( "-fullscreen" ) )
		windowed = false;

	registry->WriteInt( "ScreenWindowed", windowed );

	gD3DMode = 0;
	registry->ReadInt( "EngineD3D", 0 );
	if ( !COM_CheckParm( "-d3d" ) )
		COM_CheckParm( "-gl" );

	videomode = new CVideoMode_OpenGL( windowed );
	if ( !videomode )
		_wassert( L"videomode", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 2362 );
}

bool CVideoMode_Common::AddMode( int width, int height, int bitsPerPixel )
{
	int i;

	if ( m_nNumModes >= MAX_VIDEO_MODES )
		return false;

	for ( i = 0; i < m_nNumModes; i++ )
	{
		if ( m_rgModeList[i].width == width && m_rgModeList[i].height == height && m_rgModeList[i].bpp == bitsPerPixel )
			return true;
	}

	i = m_nNumModes++;
	m_rgModeList[i].width = width;
	m_rgModeList[i].height = height;
	m_rgModeList[i].bpp = bitsPerPixel;
	return true;
}

vmode_s* CVideoMode_Common::GetMode( int index )
{
	if ( index >= m_nNumModes || index < 0 )
		return &m_safeMode;
	return &m_rgModeList[index];
}

vmode_s* CVideoMode_Common::GetCurrentMode()
{
	return GetMode( m_nGameMode );
}

int CVideoMode_Common::GetModeCount()
{
	return m_nNumModes;
}

bool CVideoMode_Common::IsWindowedMode() const
{
	return m_bWindowed;
}

bool CVideoMode_Common::GetInitialized() const
{
	return m_bInitialized;
}

void CVideoMode_Common::SetInitialized( bool initialized )
{
	m_bInitialized = initialized;
}

int CVideoMode_Common::MaxBitsPerPixel()
{
	return 32;
}

float CVideoMode_Common::CalculatePixelAspect( unsigned int width, unsigned int height )
{
	if ( width > 639 )
		return 1.0f;
	return (float)( (double)height / (int)width * ( 4.0f / 3.0f ) );
}

void SetupSDLVideoModes()
{
	int i, count;
	SDL_DisplayMode mode;

	SDL_InitSubSystem( SDL_INIT_VIDEO );

	count = SDL_GetNumDisplayModes( 0 );
	for ( i = 0; i < count; i++ )
	{
		if ( !SDL_GetDisplayMode( 0, i, &mode ) && !videomode->AddMode( mode.w, mode.h, 32 ) )
			return;
	}

	SDL_QuitSubSystem( SDL_INIT_VIDEO );
}

static int VideoModeCompare( const void* arg1, const void* arg2 )
{
	const vmode_s* mode1 = (const vmode_s*)arg1;
	const vmode_s* mode2 = (const vmode_s*)arg2;

	if ( mode1->width < mode2->width )
		return -1;
	if ( mode1->width > mode2->width )
		return 1;
	if ( mode1->height < mode2->height )
		return -1;
	if ( mode1->height > mode2->height )
		return 1;
	return 0;
}

bool CVideoMode_Common::Init( void* )
{
	SDL_DisplayMode desktop;
	int width, height;
	int arg;
	int i, mode;

	if ( !game->CreateGameWindow() )
		return false;

	SDL_GetDesktopDisplayMode( 0, &desktop );
	m_safeMode.width = desktop.w;
	m_safeMode.height = desktop.h;
	m_safeMode.bpp = 32;

	width = registry->ReadInt( "DisplayWidth", desktop.w );
	height = registry->ReadInt( "DisplayHeight", desktop.h );

	registry->ReadInt( "DisplayBPP", 32 );
	COM_CheckParm( "-16bpp" );
	COM_CheckParm( "-24bpp" );
	COM_CheckParm( "-32bpp" );
	MaxBitsPerPixel();

	m_nNumModes = 0;
	SetupSDLVideoModes();
	if ( m_nNumModes >= 2 )
		qsort( m_rgModeList, m_nNumModes, sizeof( m_rgModeList[0] ), VideoModeCompare );

	arg = COM_CheckParm( "-width" );
	if ( arg )
		width = atoi( com_argv[arg + 1] );
	arg = COM_CheckParm( "-w" );
	if ( arg )
		width = atoi( com_argv[arg + 1] );
	arg = COM_CheckParm( "-height" );
	if ( arg )
		height = atoi( com_argv[arg + 1] );
	arg = COM_CheckParm( "-h" );
	if ( arg )
		height = atoi( com_argv[arg + 1] );

	if ( IsWindowedMode() )
	{
		SDL_Rect bounds;

		if ( !SDL_GetDisplayBounds( 0, &bounds ) )
		{
			if ( width > bounds.w )
				width = bounds.w;
			if ( height > bounds.h )
				height = bounds.h;
		}
	}

	if ( width > 0 && height < 0 )
		height = (int)( width * 3.0 / 4.0 );

	mode = -1;
	for ( i = 0; i < m_nNumModes; i++ )
	{
		if ( m_rgModeList[i].width == width )
		{
			mode = i;
			if ( m_rgModeList[i].height == height )
				break;
		}
	}
	if ( mode < 0 )
		mode = 0;
	m_nGameMode = mode;

	if ( ( IsWindowedMode() || !COM_CheckParm( "-nofbo" ) ) && ( COM_CheckParm( "-forceres" ) || m_nNumModes < 2 ) )
	{
		m_rgModeList[mode].width = width;
		m_rgModeList[mode].height = height;
		if ( !m_nNumModes )
			m_nNumModes = 1;
	}

	registry->WriteInt( "DisplayWidth", m_rgModeList[mode].width );
	registry->WriteInt( "DisplayHeight", m_rgModeList[mode].height );
	registry->WriteInt( "DisplayBPP", m_rgModeList[mode].bpp );

	LoadStartupGraphic();
	AdjustWindowForMode();
	return true;
}

void CVideoMode_Common::CenterEngineWindow( HWND window, int width, int height )
{
	SDL_Rect bounds;
	int x, y;

	SDL_GetDisplayBounds( 0, &bounds );

	x = ( bounds.w - width ) / 2;
	y = ( bounds.h - height ) / 2;
	if ( x < 0 )
		x = 0;
	if ( y < 0 )
		y = 0;

	game->SetWindowXY( x, y );
	SDL_SetWindowPosition( (SDL_Window*)window, x, y );
}

void CVideoMode_Common::AdjustWindowForMode()
{
	vmode_s* mode = GetCurrentMode();
	int width, height;

	if ( !mode )
		_wassert( L"mode", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 586 );

	Snd_ReleaseBuffer();

	vid.width = mode->width;
	vid.height = mode->height;

	if ( IsWindowedMode() )
	{
		width = m_rgModeList[m_nGameMode].width;
		height = m_rgModeList[m_nGameMode].height;
	}
	else
	{
		width = mode->width;
		height = mode->height;
		ChangeDisplaySettingsToFullscreen();
		SDL_SetWindowPosition( (SDL_Window*)game->GetMainWindow(), 0, 0 );
	}

	game->SetWindowSize( width, height );
	CenterEngineWindow( game->GetMainWindow(), width, height );

	Snd_AcquireBuffer();
	VOX_Init();
	UpdateWindowPosition();
}

void CVideoMode_Common::LoadStartupGraphic()
{
	FileHandle_t file;
	unsigned int size;
	char* buffer;
	char* data;
	char token[512];

	file = FS_Open( "resource/BackgroundLoadingLayout.txt", "rt" );
	if ( !file )
		return;

	size = FS_Size( file );
	buffer = (char*)_alloca( size + 1 );
	memset( buffer, 0, size + 1 );
	FS_Read( buffer, size, 1, file );
	FS_Close( file );

	data = buffer;
	while ( data && *data )
	{
		data = FS_ParseFile( data, token, NULL );
		if ( !data || !token[0] )
			break;

		if ( !_stricmp( token, "resolution" ) )
		{
			data = FS_ParseFile( data, token, NULL );
			m_iBaseResX = atoi( token );
			data = FS_ParseFile( data, token, NULL );
			m_iBaseResY = atoi( token );
		}
		else
		{
			bimage_t& image = m_ImageID[m_ImageID.AddToTail()];

			image.buffer = (byte*)malloc( STARTUP_IMAGE_BYTES );
			if ( !LoadTGA( token, image.buffer, STARTUP_IMAGE_BYTES, &image.width, &image.height ) )
			{
				free( image.buffer );
				m_ImageID.Remove( m_ImageID.Count() - 1 );
				return;
			}

			data = FS_ParseFile( data, token, NULL );
			image.scaled = !_stricmp( token, "scaled" );
			image.fit = !_stricmp( token, "fit" );
			data = FS_ParseFile( data, token, NULL );
			image.x = atoi( token );
			data = FS_ParseFile( data, token, NULL );
			image.y = atoi( token );
		}
	}
}

void VideoMode_GetCurrentRenderer( char* name, int nameLength, int* windowed, int* hdModels, int* addons )
{
	if ( name && nameLength > 0 )
		strncpy( name, videomode->GetName(), nameLength );
	if ( windowed )
		*windowed = videomode->IsWindowedMode();
	if ( hdModels )
		*hdModels = registry->ReadInt( "hdmodels", 0 ) > 0;
	if ( addons )
		*addons = registry->ReadInt( "addons_folder", 0 ) > 0;
}

void VideoMode_GetVideoModes( vmode_s** modes, int* count )
{
	*count = videomode->GetModeCount();
	*modes = videomode->GetMode( 0 );
}

extern "C" void VideoMode_RestoreVideo()
{
	videomode->RestoreVideo();
}

extern "C" int VideoMode_IsWindowed()
{
	return videomode && videomode->IsWindowedMode();
}

extern "C" qboolean R_UseLegacyRender()
{
	return game->OnlyLegacyRender();
}

extern "C" void VideoMode_GetCurrentVideoMode( int* width, int* height, int* bitsPerPixel )
{
	vmode_s* mode = videomode->GetCurrentMode();

	if ( !mode )
		return;

	if ( width )
		*width = mode->width;
	if ( height )
		*height = mode->height;
	if ( bitsPerPixel )
		*bitsPerPixel = mode->bpp;
}

extern "C" void VideoMode_SetVideoMode( int width, int height, int bitsPerPixel )
{
	registry->WriteInt( "DisplayWidth", width );
	registry->WriteInt( "DisplayHeight", height );
	registry->WriteInt( "DisplayBPP", bitsPerPixel );
}

extern "C" void VideoMode_SwitchMode( int hardware, int windowed )
{
	if ( hardware )
	{
		registry->WriteString( "EngineDLL", "hw.dll" );
		registry->WriteInt( "EngineD3D", hardware == 2 );
	}
	else
	{
		registry->WriteString( "EngineDLL", "sw.dll" );
		registry->WriteInt( "EngineD3D", 0 );
	}
	registry->WriteInt( "ScreenWindowed", windowed );
}

#ifdef _MSC_VER
#pragma warning( suppress: 26495 )
#endif
CVideoMode_Common::CVideoMode_Common()
	: m_nNumModes( 0 ), m_nGameMode( 0 ), m_bInitialized( false ), m_bWindowed( false )
{
	SDL_DisplayMode mode;

	SDL_GetDesktopDisplayMode( 0, &mode );
	m_safeMode.width = mode.w;
	m_safeMode.height = mode.h;
	m_safeMode.bpp = 32;
}

CVideoMode_OpenGL::CVideoMode_OpenGL( bool windowed )
{
	m_bWindowed = windowed;
}

CVideoMode_Common::~CVideoMode_Common()
{
}

CVideoMode_OpenGL::~CVideoMode_OpenGL()
{
}

const char* CVideoMode_OpenGL::GetName()
{
	return "gl";
}

void CVideoMode_Common::FlipScreen()
{
}

void CVideoMode_Common::RestoreVideo()
{
}

void CVideoMode_Common::ReleaseVideo()
{
}

void CVideoMode_Common::ReleaseFullScreen()
{
}

void CVideoMode_Common::ChangeDisplaySettingsToFullscreen()
{
}

void CVideoMode_OpenGL::FlipScreen()
{
	Sys_Error( "This shouldn't be called\n" );
}

void CVideoMode_OpenGL::RestoreVideo()
{
	if ( IsWindowedMode() )
		return;

	AdjustWindowForMode();
	SDL_ShowWindow( (SDL_Window*)game->GetMainWindow() );
}

void CVideoMode_OpenGL::ReleaseVideo()
{
	if ( !IsWindowedMode() )
		ReleaseFullScreen();
}

void CVideoMode_Common::UpdateWindowPosition()
{
	int x, y, width, height;
	RECT rect;

	game->GetWindowRect( &x, &y, &width, &height );

	rect.left = x;
	rect.top = y;
	rect.right = x + width;
	rect.bottom = y + height;
	VID_UpdateWindowVars( &rect, x + width / 2, y + height / 2 );
}

bool CVideoMode_OpenGL::Init( void* instance )
{
	bool initialized = CVideoMode_Common::Init( instance );

	SetInitialized( initialized );
	return initialized;
}

void CVideoMode_Common::PlayStartupSequence()
{
	if ( !COM_CheckParm( "-novid" ) )
		game->PlayStartupVideos();

	DrawStartupGraphic( game->GetMainWindow() );
}

void CVideoMode_Common::Shutdown()
{
	ReleaseFullScreen();

	if ( !GetInitialized() )
		return;

	SetInitialized( false );
	m_nGameMode = 0;
	videomode = NULL;
	delete this;
}

void CVideoMode_OpenGL::ReleaseFullScreen()
{
	if ( !IsWindowedMode() )
		SDL_SetWindowFullscreen( (SDL_Window*)game->GetMainWindow(), 0 );
}

void CVideoMode_OpenGL::ChangeDisplaySettingsToFullscreen()
{
	static int iLastScreenMode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	int screenMode;

	if ( IsWindowedMode() )
		return;

	if ( !GetCurrentMode() )
		_wassert( L"mode", L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\engine\\sys_getmodes.cpp", 1778 );

	screenMode = bNeedsFullScreenModeSwitch > 0 ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;

	if ( iLastScreenMode != screenMode )
		SDL_SetWindowFullscreen( (SDL_Window*)game->GetMainWindow(), 0 );
	SDL_SetWindowFullscreen( (SDL_Window*)game->GetMainWindow(), screenMode );
	iLastScreenMode = screenMode;
}

void CVideoMode_Common::DrawStartupGraphic( HWND window )
{
	int width, height;
	int i, bit, row;
	float xScale, yScale;
	float fitScale, fitAdjWidth, fitAdjHeight;
	int fitOffsetX, fitOffsetY;
	CUtlVector<GLuint> vecGLTex;

	if ( !m_ImageID.Count() )
		return;

	SDL_GetWindowSize( (SDL_Window*)window, &width, &height );

	glViewport( 0, 0, width, height );
	glDisable( GL_DEPTH_TEST );
	glEnable( GL_TEXTURE_2D );
	glMatrixMode( GL_PROJECTION );
	glOrtho( 0.0, width, height, 0.0, -1.0, 1.0 );
	glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
	glClear( GL_COLOR_BUFFER_BIT );

	xScale = (float)width / (float)m_iBaseResX;
	yScale = (float)height / (float)m_iBaseResY;

	fitScale = yScale;
	fitAdjWidth = (float)m_iBaseResX * fitScale;
	fitAdjHeight = (float)m_iBaseResY * fitScale;
	if ( (float)width > fitAdjWidth )
	{
		fitScale = xScale;
		fitAdjWidth = (float)m_iBaseResX * fitScale;
		fitAdjHeight = (float)m_iBaseResY * fitScale;
	}
	fitOffsetX = width / 2 - (int)( fitAdjWidth * 0.5f );
	fitOffsetY = height / 2 - (int)( fitAdjHeight * 0.5f );

	for ( i = 0; i < m_ImageID.Count(); i++ )
	{
		bimage_t& image = m_ImageID[i];
		int x, y, right, bottom;
		int wide2, tall2;
		int tex;
		float topu, topv;
		byte* pToUse;
		byte* pExpanded;

		if ( image.scaled )
		{
			x = image.x ? (int)ceil( (double)( image.x * xScale ) ) : 0;
			y = image.y ? (int)ceil( (double)( image.y * yScale ) ) : 0;
			right = (int)ceil( (double)( ( image.x + image.width ) * xScale ) );
			bottom = (int)ceil( (double)( ( image.y + image.height ) * yScale ) );
		}
		else if ( image.fit )
		{
			x = fitOffsetX + ( image.x ? (int)ceil( (double)( image.x * fitScale ) ) : 0 );
			y = fitOffsetY + ( image.y ? (int)ceil( (double)( image.y * fitScale ) ) : 0 );
			right = x + (int)ceil( (double)( image.width * fitScale ) );
			bottom = y + (int)ceil( (double)( image.height * fitScale ) );
		}
		else
		{
			x = image.x;
			y = image.y;
			right = x + image.width;
			bottom = y + image.height;
		}

		tex = vecGLTex.AddToTail();
		glGenTextures( 1, &vecGLTex[tex] );
		glBindTexture( GL_TEXTURE_2D, vecGLTex[tex] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

		wide2 = tall2 = 1;
		for ( bit = 0; bit < 32; bit++ )
		{
			wide2 = 1 << bit;
			if ( wide2 >= image.width )
				break;
		}
		for ( bit = 0; bit < 32; bit++ )
		{
			tall2 = 1 << bit;
			if ( tall2 >= image.height )
				break;
		}

		pToUse = image.buffer;
		pExpanded = NULL;
		if ( wide2 != image.width || tall2 != image.height )
		{
			pExpanded = new byte[4 * wide2 * tall2];
			if ( !pExpanded )
				return;

			memset( pExpanded, 0, 4 * wide2 * tall2 );
			if ( image.width > 0 )
			{
				for ( row = 0; row < image.height; row++ )
					memcpy( pExpanded + 4 * wide2 * row, image.buffer + 4 * image.width * row, 4 * image.width );
			}
			pToUse = pExpanded;
		}

		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, wide2, tall2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pToUse );

		topu = topv = 1.0f;
		if ( pExpanded )
		{
			topu = (float)image.width / (float)wide2;
			topv = (float)image.height / (float)tall2;
			delete[] pExpanded;
		}

		glBegin( GL_QUADS );
		glTexCoord2f( 0.0f, topv );
		glVertex3i( x, bottom, 0 );
		glTexCoord2f( topu, topv );
		glVertex3i( right, bottom, 0 );
		glTexCoord2f( topu, 0.0f );
		glVertex3i( right, y, 0 );
		glTexCoord2f( 0.0f, 0.0f );
		glVertex3i( x, y, 0 );
		glEnd();
	}

	SDL_GL_SwapWindow( (SDL_Window*)window );

	for ( i = 0; i < vecGLTex.Count(); i++ )
		glDeleteTextures( 1, &vecGLTex[i] );
	vecGLTex.Purge();

	for ( i = 0; i < m_ImageID.Count(); i++ )
	{
		if ( m_ImageID[i].buffer )
			free( m_ImageID[i].buffer );
	}
	m_ImageID.Purge();
}

#endif
