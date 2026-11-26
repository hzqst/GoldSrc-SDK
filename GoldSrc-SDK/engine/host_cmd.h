#pragma once

#include "client.h"

typedef struct saverestore_s SAVERESTOREDATA;

struct SAVE_HEADER
{
	int saveId;
	int version;
	int skillLevel;
	int entityCount;
	int connectionCount;
	int lightStyleCount;
	float time;
	char mapName[32];
	char skyName[32];
	int skyColor_r;
	int skyColor_g;
	int skyColor_b;
	float skyVec_x;
	float skyVec_y;
	float skyVec_z;
};

struct GAME_HEADER
{
	char mapName[32];
	char comment[80];
	int mapCount;
};

struct SAVELIGHTSTYLE
{
	int index;
	char style[MAX_LIGHTSTYLES];
};

SAVERESTOREDATA*	LoadSaveData( const char* level );
void				EntityPatchRead( SAVERESTOREDATA* save, const char* level );
void				EntityPatchWrite( SAVERESTOREDATA* save, const char* level );
int					CreateEntityList( SAVERESTOREDATA* save, int levelMask );
void				LoadAdjacentEntities( const char* oldLevel, const char* landmarkName );
int					LoadGamestate( char* level, int createPlayers );
