#pragma once

#include "interface.h"

enum TrackType
{
	TRACKTYPE_INVALID = 1,
	TRACKTYPE_SPLASH,
	TRACKTYPE_ROUNDWIN,
	TRACKTYPE_ROUNDLOSE,
	TRACKTYPE_MATCHWIN,
	TRACKTYPE_MATCHLOSE,
	TRACKTYPE_CREDITS,
	TRACKTYPE_NUM
};

class IMusicManager : public IBaseInterface
{
public:
	virtual void StartTrack( TrackType type, bool loop ) = 0;
	virtual void StopTrack() = 0;
	virtual void FadeOutTrack() = 0;
	virtual bool IsPlayingTrack() = 0;
	virtual bool IsPlayingTrack( TrackType type ) = 0;
};
