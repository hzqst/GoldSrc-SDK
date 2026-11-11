#pragma once

#include "GameUI/ICareerUI.h"
#include "CareerProfile.h"

class BufferWriter;
class BotProfile;
class CCareerMap;
class CCareerProfileData;
class CCareerProfileDifficultyData;
class CvarLatch;

#define CAREER_MAPS_PER_TRIPLET	3

class CCareerCharacter
{
public:
	CCareerCharacter( const BotProfile* profile );
	void Print();
	char* Save( char* buffer, int& length, int hired );
	int GetPoints() const;
	const char* GetName() const;
	bool operator<( const CCareerCharacter& other ) const;
	const BotProfile* GetProfile() const { return m_pProfile; }
	int GetRange( float value ) const;
	const char* GetSkinThumbnailFname() const;
	const char* GetWeaponPreferenceAsString( int index ) const;
	int GetWeaponPreferenceCount() const;
	float GetSkill() const;
	float GetTeamwork() const;
	float GetAggression() const;

protected:
	const BotProfile* m_pProfile;
};

typedef std::vector<int> CharCostVector;
typedef std::vector<CCareerCharacter*> CharacterVec;
typedef std::vector<std::vector<CCareerMap*> > MapsVec;
typedef std::vector<CvarLatch*> CvarLatchVector;

class CCareerMap
{
public:
	CCareerMap( const char* name, const CharacterVec& bots, bool friendlyFire );
	~CCareerMap();
	const char* GetName() const { return m_name; }
	bool IsDefeated() const { return m_defeated; }
	void SetDefeated( bool defeated ) { m_defeated = defeated; }
	const CharacterVec& GetBotList() const { return m_bots; }
	virtual int GetNumBotNames() const;
	virtual const char* GetBotName( int index ) const;
	void SetTasks( ITaskVec tasks ) { m_tasks = tasks; }
	ITaskVec* GetTasks() { return &m_tasks; }
	void ResetTaskCompletion();
	void SetMinEnemyCount( int count ) { m_minEnemyCount = count; }
	int GetMinEnemyCount() const { return m_minEnemyCount; }
	void SetThreshold( int threshold ) { m_threshold = threshold; }
	int GetThreshold() const { return m_threshold; }
	int GetCurrentEnemyCount() const;
	bool IsFriendlyFireOn() const;
	void SetExtraInitialization( const char* extra );
	const char* GetExtraInitialization() const { return m_extra; }
	void SetTeammatesAllowed( bool allowed ) { m_allowTeammates = allowed; }
	bool AreTeammatesAllowed() const { return m_allowTeammates; }

protected:
	char		m_name[64];
	bool		m_defeated;
	bool		m_friendlyFire;
	CharacterVec m_bots;
	ITaskVec	m_tasks;
	int			m_minEnemyCount;
	int			m_threshold;
	char*		m_extra;
	bool		m_allowTeammates;
};

class CCareerGame : public ICareerUI
{
public:
	CCareerGame();
	~CCareerGame() override;

	static void Create();
	void Update();
	void Reset();
	void Init();
	void SetDifficulty( CareerDifficultyType difficulty );
	void StopMatch( int ctWins, int tWins );
	int GetPoints() { return m_reputationPoints; }
	int GetPointsFlashed() { return m_reputationPointsFlashed; }
	void MarkJumpButtonsFlashed() { m_reputationPointsFlashed = m_reputationPoints; }
	CharacterVec GetHiredCharacters() { return m_hiredCharacters; }
	CharacterVec GetAvailableCharacters() { return m_availableCharacters; }
	MapsVec& GetMaps() { return m_maps; }
	bool IsMedalEarned( CareerDifficultyType difficulty ) { return m_medals[difficulty]; }
	bool AreChecksumsOk() { return m_checksumsOk; }
	int GetPointsRemaining();
	int GetHighestCostAvailable( int reputationPoints );
	void PrintMaps();
	void DefeatMap( const char* name );
	const char* GetLastMap() { return m_lastMapName; }
	CCareerMap* GetLastMapPtr() { return m_pLastMap; }
	bool SetLastMap( const char* name );
	CCareerMap* GetPlayableMap( const char* name );
	CCareerMap* GetMap( const char* name );
	int GetMapTripletNum( const char* name );
	int GetMapTripletOffset( const char* name );
	bool DoesCurrentMapUnlockTriplet();
	CCareerMap* GetFirstUndefeatedMap();
	ITaskVec* GetTaskVecForMap( const char* name );
	void StartMatch();
	int GetNumTasksOnCurrentMap();
	ICareerTask* GetCurrentTask( int index );
	void StartLoading();
	void AddSavedCvar( const char* cvarName, const char* cvarVal, bool wait );
	void FinishMatchStart();
	void SendMatchCommand();
	void SetBotPoolSliderPosition( int position );
	int GetBotPoolSliderPosition();
	void SetRoundTimeLength( int length ) { m_roundTimeLength = length; }
	bool CanUnpause() const { return m_canUnpause; }
	bool Hire( const char* name );
	bool Fire( const char* name );
	void ClearRoster();

	bool ParseMedalChunk( CCareerProfileData* save, int version );
	bool ParseDifficultyChunk( CCareerProfileDifficultyData* save, int version );
	bool ParseTutorMessageDecayChunk( CCareerProfileData* save, int version );
	bool ParseSavedProfile( const unsigned char* data, int dataLength, CCareerProfileData* save );
	static const char* GetSaveGamePlayerName( const char* name );
	void ApplyTutorMessageData( int* tutorData );
	void WriteMedalChunk( CCareerProfileData* save );
	void WriteDifficultyChunk( CCareerProfileDifficultyData* save );
	void WriteTutorMessageDecayChunk( bool isNewProfile );
	int CreateProfileBuffer( CCareerProfileData* save, unsigned char* data, int dataLength, bool isNewProfile );
	void UpdateMedalChunk();
	void ApplySavedProfile( CCareerProfileData* save, CareerDifficultyType difficulty );
	void ApplyDifficultyChunk( CCareerProfileDifficultyData* save );
	void UpdateDifficultyChunk();
	bool UpdateSavedProfile();
	ProfileVector BuildProfileList();
	CCareerProfileData* GetProfileForName( const char* name );
	void DeleteProfile( const char* name );
	bool CreateBlankProfile();
	bool SaveProfile( bool updateConfig );
	bool LoadProfile( CareerDifficultyType difficulty );

	bool IsPlayingMatch() override;
	ITaskVec* GetCurrentTaskVec() override;
	bool PlayAsCT() override
	{
		return m_isCT != 0;
	}
	int GetReputationGained() override
	{
		return m_matchReputationGain;
	}
	int GetNumMapsUnlocked() override
	{
		return m_matchMapsUnlocked;
	}
	bool DoesWinUnlockAll() override;
	int GetRoundTimeLength() override;
	int GetWinfastLength() override
	{
		return m_winfastLength;
	}
	CareerDifficultyType GetDifficulty() const override
	{
		return m_difficulty;
	}
	int GetCurrentMapTriplet( MapInfo* maps ) override;
	void OnRoundEndMenuOpen( bool won ) override;
	void OnMatchEndMenuOpen( bool won ) override;
	void OnRoundEndMenuClose( bool won ) override;
	void OnMatchEndMenuClose( bool won ) override;

protected:
	static bool			m_checksumsOk;

private:
	void ExecuteProfileConfig();

	CCareerProfileData*	m_currentProfile;
	BufferWriter*		m_writer;
	int					m_reputationPoints;
	int					m_reputationPointsFlashed;
	CharCostVector		m_costAvailability;
	bool				m_queuedLoadMenu;
	std::string			m_queuedName;
	bool				m_wasMapDefeatedAtStart;
	bool				m_needAutosave;
	CharacterVec		m_hiredCharacters;
	CharacterVec		m_availableCharacters;
	MapsVec				m_maps;
	bool				m_isPlaying;
	char				m_lastMapName[64];
	CCareerMap*			m_pLastMap;
	int					m_basePoints;
	int					m_roundTime;
	int					m_bombCountdownTime;
	int					m_freezeTime;
	int					m_isCT;
	bool				m_initDone;
	int					m_botPoolSliderPosition;
	int					m_matchNumWins;
	int					m_matchWinBy;
	int					m_matchReputationGain;
	int					m_matchMapsUnlocked;
	int					m_roundTimeLength;
	int					m_winfastLength;
	CvarLatchVector		m_savedCvars;
	CareerDifficultyType m_difficulty;
	bool				m_medals[MAX_CAREER_DIFFICULTY];
	bool				m_canUnpause;
	int					m_ctWins;
	int					m_tWins;
};

extern CCareerGame* TheCareerGame;
