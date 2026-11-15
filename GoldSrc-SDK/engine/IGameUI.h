#pragma once

#include "interface.h"

struct cl_enginefuncs_s;
class IBaseSystem;

#ifdef _MSC_VER
class __declspec( novtable ) IGameUI : public IBaseInterface
#else
class IGameUI : public IBaseInterface
#endif
{
public:
	virtual void Initialize( CreateInterfaceFn* factories, int count ) = 0;
	virtual void Start( cl_enginefuncs_s* engineFunctions, int interfaceVersion, IBaseSystem* system ) = 0;
	virtual void Shutdown() = 0;

	virtual int ActivateGameUI() = 0;
	virtual int ActivateDemoUI() = 0;
	virtual int HasExclusiveInput() = 0;
	virtual void RunFrame() = 0;

	virtual void ConnectToServer( const char* game, int address, int port ) = 0;
	virtual void DisconnectFromServer() = 0;

	virtual void HideGameUI() = 0;
	virtual bool IsGameUIActive() = 0;

	virtual void LoadingStarted( const char* resourceType, const char* resourceName ) = 0;
	virtual void LoadingFinished( const char* resourceType, const char* resourceName ) = 0;
	virtual void StartProgressBar( const char* progressType, int steps ) = 0;
	virtual int ContinueProgressBar( int point, float fraction ) = 0;
	virtual void StopProgressBar( bool error, const char* failureReason, const char* extendedReason ) = 0;
	virtual int SetProgressBarStatusText( const char* statusText ) = 0;
	virtual void SetSecondaryProgressBar( float progress ) = 0;
	virtual void SetSecondaryProgressBarText( const char* statusText ) = 0;

	virtual void ValidateCDKey( bool force, bool connect ) = 0;
	virtual void OnDisconnectFromServer( int failureCode, const char* username ) = 0;

	virtual void SetIsRunningOnSteamDeck( bool running ) = 0;
	virtual bool IsRunningOnSteamDeck() const = 0;
};
