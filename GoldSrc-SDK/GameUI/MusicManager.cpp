#include "MusicManager.h"
#include "GameUI_Interface.h"
#include "StringUtils.h"

#include "vgui/KeyValues.h"
#include "controls.h"
#include "wrect.h"
#include "cl_dll.h"

#include <stdio.h>
#include <string.h>
#include <string>

MusicManager* TheMusicManager;

EXPOSE_SINGLE_INTERFACE( MusicManager, IMusicManager, "MusicManager001" );

// playlist.txt keys of each track and whether it loops when the key is missing
static const struct
{
	TrackType	track;
	const char*	pathKey;
	const char*	loopKey;
	int			defaultLoop;
} g_playlistTracks[] =
{
	{ TRACKTYPE_SPLASH,		"Splash",		"SplashLoop",		1 },
	{ TRACKTYPE_ROUNDWIN,	"RoundWin",		"RoundWinLoop",		0 },
	{ TRACKTYPE_ROUNDLOSE,	"RoundLose",	"RoundLoseLoop",	0 },
	{ TRACKTYPE_MATCHWIN,	"MatchWin",		"MatchWinLoop",		0 },
	{ TRACKTYPE_MATCHLOSE,	"MatchLose",	"MatchLoseLoop",	0 },
	{ TRACKTYPE_CREDITS,	"Credits",		"CreditsLoop",		1 },
};

MusicManager::MusicManager()
{
	m_currentTrack = TRACKTYPE_INVALID;
	for ( int i = 0; i < TRACKTYPE_NUM; i++ )
	{
		m_tracks[i].t_path[0] = 0;
		m_tracks[i].looping = false;
	}
	m_useLoopingInfo = false;
}

MusicManager::~MusicManager()
{
}

void MusicManager::Reset()
{
	if ( m_currentTrack != TRACKTYPE_INVALID )
	{
		StopTrack();
		m_currentTrack = TRACKTYPE_INVALID;
	}
}

void MusicManager::Init()
{
	Reset();

	KeyValues* playlist = new KeyValues( "Playlist" );
	if ( playlist->LoadFromFile( vgui2::filesystem(), "playlist.txt" ) )
	{
		if ( playlist->GetInt( "Version", 1 ) > 1 )
			m_useLoopingInfo = true;

		for ( const auto& entry : g_playlistTracks )
		{
			TrackInfo& info = m_tracks[entry.track];
			// A path that does not fit is dropped instead of truncated, so the track is not played
			if ( !UTIL_CopyString( info.t_path, sizeof( info.t_path ), playlist->GetString( entry.pathKey ) ) )
				info.t_path[0] = 0;
			info.looping = playlist->GetInt( entry.loopKey, entry.defaultLoop ) != 0;
		}
	}

	playlist->deleteThis();
}

void MusicManager::StartTrack( TrackType track, bool looping )
{
	if ( m_currentTrack == track || track < TRACKTYPE_SPLASH || track >= TRACKTYPE_NUM )
		return;

	TrackInfo& info = m_tracks[track];
	if ( !info.t_path[0] )
		return;

	if ( m_useLoopingInfo )
		looping = info.looping;

	Career_Printf( "MusicManager::StartTrack - playing track %d %s\n", track, looping ? "as looping" : "" );
	engine->pfnPrimeMusicStream( info.t_path, looping );

	std::string command = std::string( "cd " ) + ( looping ? "loopfile " : "playfile " ) + info.t_path + "\n";
	engine->pfnClientCmd( (char*)command.c_str() );
	m_currentTrack = track;
}

void MusicManager::StopTrack()
{
	if ( m_currentTrack != TRACKTYPE_INVALID )
	{
		engine->pfnClientCmd( "cd stop" );
		m_currentTrack = TRACKTYPE_INVALID;
	}
}

void MusicManager::FadeOutTrack()
{
	if ( m_currentTrack != TRACKTYPE_INVALID )
	{
		engine->pfnClientCmd( "cd fadeout" );
		m_currentTrack = TRACKTYPE_INVALID;
	}
}

bool MusicManager::IsPlayingTrack()
{
	return m_currentTrack != TRACKTYPE_INVALID;
}

bool MusicManager::IsPlayingTrack( TrackType track )
{
	return m_currentTrack == track;
}

void CreateMusicManager()
{
	TheMusicManager = &__g_MusicManager_singleton;
	TheMusicManager->Init();
}

void ShutdownMusicManager()
{
	TheMusicManager = NULL;
}
