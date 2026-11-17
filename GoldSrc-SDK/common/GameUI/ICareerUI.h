#pragma once

#include "interface.h"
#include <vector>

enum CareerDifficultyType
{
	CAREER_DIFFICULTY_EASY,
	CAREER_DIFFICULTY_NORMAL,
	CAREER_DIFFICULTY_HARD,
	CAREER_DIFFICULTY_EXPERT,
	MAX_CAREER_DIFFICULTY
};

class ICareerTask
{
public:
	virtual bool IsComplete() = 0;
	virtual bool IsCompletedThisRound() = 0;
	virtual bool IsCompletedThisMatch() = 0;
	virtual const char* GetMap() = 0;
	virtual const char* GetTaskName() = 0;
	virtual const wchar_t* GetLocalizedTaskName() = 0;
	virtual const wchar_t* GetLocalizedTaskNameWithCompletion() = 0;
	virtual void Reset() = 0;
	virtual void Set( bool complete ) = 0;
	virtual void StartRound() = 0;
	virtual const char* GetWeaponName() const = 0;
	virtual int GetRepeat() const = 0;
	virtual bool MustSurvive() const = 0;
	virtual bool InARow() const = 0;
	virtual int GetPartial() const = 0;
	virtual void SetPartial( int partial ) = 0;
	virtual bool IsMaxRoundTime() const = 0;
};

typedef std::vector<ICareerTask*> ITaskVec;
typedef ITaskVec::iterator ITaskVecIt;

struct MapInfo
{
	const char*	name;
	bool		defeated;
};

class ICareerUI : public IBaseInterface
{
public:
	virtual bool IsPlayingMatch() = 0;
	virtual ITaskVec* GetCurrentTaskVec() = 0;
	virtual bool PlayAsCT() = 0;
	virtual int GetReputationGained() = 0;
	virtual int GetNumMapsUnlocked() = 0;
	virtual bool DoesWinUnlockAll() = 0;
	virtual int GetRoundTimeLength() = 0;
	virtual int GetWinfastLength() = 0;
	virtual CareerDifficultyType GetDifficulty() const = 0;
	virtual int GetCurrentMapTriplet( MapInfo* maps ) = 0;
	virtual void OnRoundEndMenuOpen( bool won ) = 0;
	virtual void OnMatchEndMenuOpen( bool won ) = 0;
	virtual void OnRoundEndMenuClose( bool won ) = 0;
	virtual void OnMatchEndMenuClose( bool won ) = 0;
};
