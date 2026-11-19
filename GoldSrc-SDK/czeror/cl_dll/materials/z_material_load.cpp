#include "hud.h"
#include "cl_util.h"
#include "event_api.h"
#include "pmtrace.h"
#include "z_materials.h"
#include "z_bspstuff.h"
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

extern cvar_t* mat_autocreate;
extern cvar_t* mat_writerrorfile;
extern "C" char PM_FindTextureType( char* name );

CMaterialSystem g_pMaterialSystem;
CMaterialSystem* g_pMaterialSystemPointer = &g_pMaterialSystem;
char* g_PFile;

int MAT_GetMaterialIndexByType( char* name )
{
	const char* names[] =
	{
		"concrete", "metal", "dirt", "tile", "slosh", "glass",
		"snow", "paper", "grass", "custom", "generic", "wood"
	};

	for ( int i = 0; i < 12; ++i )
	{
		if ( !_stricmp( name, names[i] ) )
			return i + 1;
	}
	return 11;
}

int MAT_GetRenderMode( char* token )
{
	const char* names[] =
	{
		"kRenderNormal", "kRenderTransColor", "kRenderTransTexture",
		"kRenderGlow", "kRenderTransAlpha", "kRenderTransAdd"
	};

	for ( int i = 0; i < 6; ++i )
	{
		if ( !_stricmp( token, names[i] ) )
			return i;
	}
	return atoi( token );
}

bool CMaterialSystem::AddMaterial( char* name )
{
	if ( m_iElements > 511 || !name[0] )
		return false;

	++m_iElements;
	CMaterial* material = new CMaterial;
	if ( pFirst )
		pCurrent->pNext = material;
	else
		pFirst = material;
	material->m_iIndex = m_iElements;
	strcpy( material->m_szName, name );
	pCurrent = material;
	return true;
}

bool CMaterialSystem::CheckMaterial( char* name )
{
	for ( CMaterial* material = pFirst; material; material = material->pNext )
	{
		if ( !_stricmp( material->m_szName, name ) )
			return true;
	}
	return false;
}

int CMaterialSystem::GetMaterialIndex( char* name )
{
	for ( CMaterial* material = pFirst; material; material = material->pNext )
	{
		if ( !_stricmp( material->m_szName, name ) )
			return material->m_iIndex;
	}
	return -1;
}

materials_t* CMaterialSystem::GetMaterialByIndex( int index )
{
	for ( CMaterial* material = pFirst; material; material = material->pNext )
	{
		if ( material->m_iIndex == index )
			return material->pMaterial;
	}
	return NULL;
}

void CMaterialSystem::ClearMaterials()
{
	if ( !pFirst )
		return;

	CMaterial* material = pFirst->pNext;
	while ( material )
	{
		CMaterial* next = material->pNext;
		delete material->pMaterial;
		material->pMaterial = NULL;
		if ( pCurrent == material )
			pCurrent = NULL;
		delete material;
		--m_iElements;
		material = next;
	}

	delete pFirst->pMaterial;
	pFirst->pMaterial = NULL;
	delete pFirst;
	--m_iElements;
	pFirst = NULL;
}

CMaterialSystem::~CMaterialSystem()
{
	if ( !pFirst )
		return;

	CMaterial* material = pFirst->pNext;
	while ( material )
	{
		CMaterial* next = material->pNext;
		delete material->pMaterial;
		material->pMaterial = NULL;
		if ( pCurrent == material )
			pCurrent = NULL;
		delete material;
		--m_iElements;
		material = next;
	}
	delete pFirst->pMaterial;
	pFirst->pMaterial = NULL;
	delete pFirst;
}

void CMaterialSystem::CopyMaterials( CMaterialSystem* list )
{
	for ( CMaterial* material = list->pFirst; material; material = material->pNext )
	{
		if ( !CheckMaterial( material->m_szName ) && AddMaterial( material->m_szName ) && pCurrent )
		{
			pCurrent->pNext = NULL;
			pCurrent->pMaterial = new materials_t;
			memset( pCurrent->pMaterial, 0, sizeof( materials_t ) );
			*pCurrent->pMaterial = *material->pMaterial;
		}
	}
}

void CMaterialSystem::FixTextureName( const char* input, char* output )
{
	if ( *input == '+' || *input == '-' )
		input += 2;
	if ( *input == '~' || *input == '{' || *input == ' ' || *input == '!' )
		++input;
	strcpy( output, input );
}

bool CMaterialSystem::IsValidMaterialName( char* name )
{
	const char* excluded[] =
	{
		"aaatrigger", "invisible", "sky", "hint", "skip", "black", "white", "yellow",
		"red", "blue", "translucent", "fade", "fade2", "clip", "origin"
	};

	for ( int i = 0; i < 15; ++i )
	{
		if ( !_stricmp( name, excluded[i] ) )
			return false;
	}
	return true;
}

bool CMaterialSystem::ShouldLoad( char* name )
{
	return IsValidMaterialName( name ) && !g_pMaterialSystemPointer->CheckMaterial( name );
}

int CMaterialSystem::GetMaterialIndexAtPos( pmtrace_s* trace, float* start, float* end )
{
	const char* texture = gEngfuncs.pEventAPI->EV_TraceTexture( trace->ent, start, end );
	if ( !texture )
		return -1;

	char name[64];
	FixTextureName( texture, name );
	return g_pMaterialSystemPointer->GetMaterialIndex( name );
}

void CMaterialFIO::WriteToErrorFile( char* texture, char* error, ... )
{
	va_list arguments;
	va_start( arguments, error );

	if ( mat_writerrorfile->value != 0.0f )
	{
		char level[32], filename[256];
		COM_FileBase( gEngfuncs.pfnGetLevelName(), level );
		sprintf( filename, "%s/materials/%s.error", gEngfuncs.pfnGetGameDirectory(), level );
		static char string[1024];
		vsprintf( string, error, arguments );
		FILE* file = fopen( filename, "a+" );
		if ( file )
		{
			fprintf( file, "%s: %s\n", texture, string );
			fclose( file );
		}
	}
	va_end( arguments );
}

char* MAT_FindMaterialFromTxt( char* name )
{
	char texture[64];
	strcpy( texture, name );

	switch ( PM_FindTextureType( name ) )
	{
	case 'C': return "concrete";
	case 'D': return "dirt";
	case 'M':
	case 'V': return "metal";
	case 'N': return "snow";
	case 'S': return "slosh";
	case 'T': return "tile";
	case 'W': return "wood";
	case 'X': return "paper";
	case 'Y': return "glass";
	default: return "generic";
	}
}

void CreateMaterialFile( char* level )
{
	char filename[256];
	sprintf( filename, "%s/materials/%s.mat", gEngfuncs.pfnGetGameDirectory(), level );
	FILE* file = fopen( filename, "w+" );
	if ( file )
	{
		for ( int i = 0; i < g_iDesiredMaterials; ++i )
		{
			char* name = g_szDesiredMaterialNames[i];
			if ( g_pMaterialSystemPointer->IsValidMaterialName( name ) && !g_pMaterialSystemPointer->CheckMaterial( name ) )
			{
				char texture[64];
				strcpy( texture, name );
				fprintf( file, "texture \"%s\"\n[\n\tmaterial\n\t{\n\t\ttype \"%s\"\n\t}\n]\n",
					name, MAT_FindMaterialFromTxt( name ) );
				++g_pMaterialSystemPointer->m_iNewMaterials;
			}
		}
		fclose( file );
	}

	sprintf( filename, "materials/%s.mat", level );
	g_PFile = (char*)gEngfuncs.COM_LoadFile( filename, 5, NULL );
}

static int MAT_FileBaseLength( const char* path )
{
	int length = (int)strlen( path );
	if ( !length )
		return 0;
	int end = length - 1;
	while ( end && path[end] != '.' && path[end] != '/' && path[end] != '\\' )
		--end;
	if ( path[end] != '.' )
		end = length - 1;
	else
		--end;
	int start = length - 1;
	while ( start >= 0 && path[start] != '/' && path[start] != '\\' )
		--start;
	if ( path[start] != '/' && path[start] != '\\' )
		start = 0;
	else
		++start;
	return end - start + 1;
}

bool MAT_SetDefaults( int type, materials_t* material )
{
	char materialName[64];
	switch ( type )
	{
	case 1: strcpy( materialName, "concrete" ); break;
	case 2: strcpy( materialName, "metal" ); break;
	case 3: strcpy( materialName, "dirt" ); break;
	case 4: strcpy( materialName, "tile" ); break;
	case 5: strcpy( materialName, "slosh" ); break;
	case 6: strcpy( materialName, "glass" ); break;
	case 7: strcpy( materialName, "snow" ); break;
	case 8: strcpy( materialName, "paper" ); break;
	case 9: strcpy( materialName, "grass" ); break;
	case 10: strcpy( materialName, "custom" ); break;
	case 11: strcpy( materialName, "generic" ); break;
	case 12: strcpy( materialName, "wood" ); break;
	}

	char filename[128], token[1024];
	sprintf( filename, "materials/defaults/%s.mat", materialName );
	char* file = (char*)gEngfuncs.COM_LoadFile( filename, 5, NULL );
	if ( !file )
		return false;

	// file moves through the buffer while parsing, so keep its start to free it
	char* fileStart = file;

	while ( ( file = gEngfuncs.COM_ParseFile( file, token ) ) != NULL )
	{
		if ( !_stricmp( token, "debris" ) )
		{
			file = gEngfuncs.COM_ParseFile( file, token );
			if ( _stricmp( token, "{" ) )
			{
				gEngfuncs.Con_Printf( "Error parsing material file %s. ( expected { )\n", materialName );
				gEngfuncs.COM_FreeFile( fileStart );
				return false;
			}

			for ( ;; )
			{
				file = gEngfuncs.COM_ParseFile( file, token );
				if ( !_stricmp( token, "}" ) )
					break;

				char property[1024];
				strcpy( property, token );
				if ( _stricmp( property, "size" ) && _stricmp( property, "spritenum" ) &&
					_stricmp( property, "scalespeed" ) && _stricmp( property, "brightness" ) &&
					_stricmp( property, "fadespeed" ) && _stricmp( property, "life" ) &&
					_stricmp( property, "original_gravity" ) && _stricmp( property, "afterdamp_gravity" ) &&
					_stricmp( property, "damp_vel_mod" ) && _stricmp( property, "sprite" ) &&
					_stricmp( property, "rendermode" ) && _stricmp( property, "flags" ) &&
					_stricmp( property, "afterdamp_flags" ) && _stricmp( property, "velmod" ) &&
					_stricmp( property, "color" ) && _stricmp( property, "damping_time" ) && _stricmp( property, "allow" ) )
				{
					gEngfuncs.Con_Printf( "Error parsing material file %s. ( %s unknown )\n", materialName, property );
					gEngfuncs.COM_FreeFile( fileStart );
					return false;
				}

				file = gEngfuncs.COM_ParseFile( file, token );
				if ( !_stricmp( property, "size" ) )
					material->m_flSize = (float)atof( token );
				else if ( !_stricmp( property, "spritenum" ) )
					material->m_iDebriSpriteNum = atoi( token );
				else if ( !_stricmp( property, "scalespeed" ) )
					material->m_flScaleSpeed = (float)atof( token );
				else if ( !_stricmp( property, "brightness" ) )
					material->m_flBrightness = (float)atof( token );
				else if ( !_stricmp( property, "fadespeed" ) )
					material->m_flFadeSpeed = (float)atof( token );
				else if ( !_stricmp( property, "life" ) )
					material->m_flLife = (float)atof( token );
				else if ( !_stricmp( property, "original_gravity" ) )
					material->m_flGravity = (float)atof( token );
				else if ( !_stricmp( property, "afterdamp_gravity" ) )
					material->m_flAfterDampGravity = (float)atof( token );
				else if ( !_stricmp( property, "damp_vel_mod" ) )
					material->m_flDampingVelocity = (float)atof( token );
				else if ( !_stricmp( property, "sprite" ) )
				{
					char sprite[64] = {};
					if ( MAT_FileBaseLength( token ) < (int)sizeof( sprite ) )
						COM_FileBase( token, sprite );
					size_t length = strlen( sprite );
					if ( length && isdigit( sprite[length - 1] ) )
						sprite[length - 1] = 0;
					sprintf( material->m_szTextName, "sprites/%s.spr", sprite );
				}
				else if ( !_stricmp( property, "rendermode" ) )
					material->m_iRendermode = MAT_GetRenderMode( token );
				else if ( !_stricmp( property, "flags" ) )
					material->m_iCollisionFlags = atoi( token );
				else if ( !_stricmp( property, "afterdamp_flags" ) )
					material->m_iAfterDampFlags = atoi( token );
				else if ( !_stricmp( property, "velmod" ) )
					material->m_flVelMod = (float)atof( token );
				else if ( !_stricmp( property, "color" ) )
				{
					material->m_vColor.x = (float)atof( token );
					file = gEngfuncs.COM_ParseFile( file, token );
					material->m_vColor.y = (float)atof( token );
					file = gEngfuncs.COM_ParseFile( file, token );
					material->m_vColor.z = (float)atof( token );
				}
				else if ( !_stricmp( property, "damping_time" ) )
					material->m_flDampingTime = (float)atof( token );
				else if ( !_stricmp( property, "allow" ) )
					material->m_iSpawnDebris = atoi( token );
				else
				{
					gEngfuncs.Con_Printf( "Error parsing material file %s. ( %s unknown )\n", materialName, property );
					gEngfuncs.COM_FreeFile( fileStart );
					return false;
				}
			}
		}
		else if ( !_stricmp( token, "smoke" ) || !_stricmp( token, "sparks" ) || !_stricmp( token, "impacts" ) )
		{
			char section[16];
			strcpy( section, token );
			file = gEngfuncs.COM_ParseFile( file, token );
			if ( _stricmp( token, "{" ) )
			{
				gEngfuncs.Con_DPrintf( "Error parsing material file %s. ( expected { )\n", materialName );
				gEngfuncs.COM_FreeFile( fileStart );
				return false;
			}

			for ( ;; )
			{
				file = gEngfuncs.COM_ParseFile( file, token );
				if ( !_stricmp( token, "}" ) )
					break;
				char property[1024];
				strcpy( property, token );
				bool known = !_stricmp( property, "allow" ) ||
					( !_stricmp( section, "smoke" ) && !_stricmp( property, "color" ) ) ||
					( !_stricmp( section, "impacts" ) && ( !_stricmp( property, "decalname" ) ||
					!_stricmp( property, "minscale" ) || !_stricmp( property, "maxscale" ) ||
					!_stricmp( property, "decalnum" ) || !_stricmp( property, "randomflip" ) ) );
				if ( !known )
				{
					gEngfuncs.Con_DPrintf( "Error parsing material file %s. ( %s unknown )\n", materialName, property );
					gEngfuncs.COM_FreeFile( fileStart );
					return false;
				}
				file = gEngfuncs.COM_ParseFile( file, token );
				if ( !_stricmp( property, "allow" ) )
				{
					if ( !_stricmp( section, "smoke" ) )
						material->m_iSpawnSmoke = atoi( token );
					else if ( !_stricmp( section, "sparks" ) )
						material->m_iSpawnSparks = atoi( token );
					else
						material->m_iSpawnImpacts = atoi( token );
				}
				else if ( !_stricmp( section, "smoke" ) && !_stricmp( property, "color" ) )
				{
					material->m_vSmokeColor.x = (float)atof( token );
					file = gEngfuncs.COM_ParseFile( file, token );
					material->m_vSmokeColor.y = (float)atof( token );
					file = gEngfuncs.COM_ParseFile( file, token );
					material->m_vSmokeColor.z = (float)atof( token );
				}
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "decalname" ) )
					strcpy( material->m_szBulletImpactName, token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "minscale" ) )
					material->m_flMinScale = (float)atof( token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "maxscale" ) )
					material->m_flMaxScale = (float)atof( token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "decalnum" ) )
					material->m_iDecalNum = atoi( token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "randomflip" ) )
					material->m_bRandomFlip = atoi( token );
				else
				{
					gEngfuncs.Con_DPrintf( "Error parsing material file %s. ( %s unknown )\n", materialName, property );
					gEngfuncs.COM_FreeFile( fileStart );
					return false;
				}
			}
		}
	}

	switch ( type )
	{
	case 1: material->m_chMaterialType = 'C'; break;
	case 2: material->m_chMaterialType = 'M'; break;
	case 3: material->m_chMaterialType = 'D'; break;
	case 4: material->m_chMaterialType = 'T'; break;
	case 5: material->m_chMaterialType = 'S'; break;
	case 6: material->m_chMaterialType = 'Y'; break;
	case 7: material->m_chMaterialType = 'N'; break;
	case 8: material->m_chMaterialType = 'X'; break;
	case 9: material->m_chMaterialType = 'A'; break;
	case 10: material->m_chMaterialType = 'K'; break;
	case 12: material->m_chMaterialType = 'W'; break;
	default: material->m_chMaterialType = PM_FindTextureType( material->m_szOriginalName ); break;
	}
	gEngfuncs.COM_FreeFile( fileStart );
	return true;
}

bool CMaterialSystem::LoadMaterial( char* textureName )
{
	if ( !textureName[0] )
	{
		m_MatFIO.WriteToErrorFile( textureName, "Trying to load material with no name!" );
		return false;
	}
	if ( !g_PFile )
	{
		m_MatFIO.WriteToErrorFile( textureName,
			"Couldn't find material (enable mat_autocreate to create it automatically)" );
		return false;
	}

	char token[1024];
	g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
	bool result = g_pMaterialSystemPointer->AddMaterial( textureName );
	if ( !result )
	{
		m_MatFIO.WriteToErrorFile( textureName, "Couldn't create material (God knows why)" );
		return false;
	}
	g_pMaterialSystemPointer->pCurrent->pMaterial = new materials_t;
	materials_t* material = g_pMaterialSystemPointer->pCurrent->pMaterial;
	memset( material, 0, sizeof( *material ) );
	material->m_iIndex = g_pMaterialSystemPointer->m_iElements;
	strcpy( material->m_szOriginalName, textureName );

	int type = 0;
	bool reading = true;
	while ( g_PFile && reading )
	{
		if ( !_stricmp( token, "]" ) )
			break;
		if ( !_stricmp( token, "material" ) )
		{
			g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
			if ( _stricmp( token, "{" ) )
			{
				m_MatFIO.WriteToErrorFile( textureName, "expected { found %s", token );
				return false;
			}

			for ( ;; )
			{
				g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
				if ( !_stricmp( token, "}" ) )
					break;
				if ( _stricmp( token, "type" ) )
				{
					m_MatFIO.WriteToErrorFile( textureName, "%s unknown", token );
					return false;
				}
				g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
				type = MAT_GetMaterialIndexByType( token );
				MAT_SetDefaults( type, material );
			}
		}
		else if ( !_stricmp( token, "debris" ) || !_stricmp( token, "smoke" ) ||
			!_stricmp( token, "sparks" ) || !_stricmp( token, "impacts" ) )
		{
			char section[16];
			strcpy( section, token );
			g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
			if ( _stricmp( token, "{" ) )
			{
				m_MatFIO.WriteToErrorFile( textureName, "expected { found %s", token );
				return false;
			}

			for ( ;; )
			{
				g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
				if ( !_stricmp( token, "}" ) )
					break;
				char property[1024];
				strcpy( property, token );
				bool known;
				if ( !_stricmp( section, "debris" ) )
				{
					known = !_stricmp( property, "size" ) || !_stricmp( property, "spritenum" ) ||
						!_stricmp( property, "scalespeed" ) || !_stricmp( property, "brightness" ) ||
						!_stricmp( property, "fadespeed" ) || !_stricmp( property, "life" ) ||
						!_stricmp( property, "original_gravity" ) || !_stricmp( property, "afterdamp_gravity" ) ||
						!_stricmp( property, "damp_vel_mod" ) || !_stricmp( property, "sprite" ) ||
						!_stricmp( property, "rendermode" ) || !_stricmp( property, "flags" ) ||
						!_stricmp( property, "afterdamp_flags" ) || !_stricmp( property, "velmod" ) ||
						!_stricmp( property, "color" ) || !_stricmp( property, "damping_time" ) || !_stricmp( property, "allow" );
				}
				else
				{
					known = !_stricmp( property, "allow" ) ||
						( !_stricmp( section, "smoke" ) && !_stricmp( property, "color" ) ) ||
						( !_stricmp( section, "impacts" ) && ( !_stricmp( property, "decalname" ) ||
						!_stricmp( property, "minscale" ) || !_stricmp( property, "maxscale" ) ||
						!_stricmp( property, "decalnum" ) || !_stricmp( property, "randomflip" ) ) );
				}
				if ( !known )
				{
					m_MatFIO.WriteToErrorFile( textureName, "( %s unknown )", property );
					return false;
				}
				g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
				if ( !_stricmp( section, "debris" ) )
				{
					if ( !_stricmp( property, "size" ) )
						material->m_flSize = (float)atof( token );
					else if ( !_stricmp( property, "spritenum" ) )
						material->m_iDebriSpriteNum = atoi( token );
					else if ( !_stricmp( property, "scalespeed" ) )
						material->m_flScaleSpeed = (float)atof( token );
					else if ( !_stricmp( property, "brightness" ) )
						material->m_flBrightness = (float)atof( token );
					else if ( !_stricmp( property, "fadespeed" ) )
						material->m_flFadeSpeed = (float)atof( token );
					else if ( !_stricmp( property, "life" ) )
						material->m_flLife = (float)atof( token );
					else if ( !_stricmp( property, "original_gravity" ) )
						material->m_flGravity = (float)atof( token );
					else if ( !_stricmp( property, "afterdamp_gravity" ) )
						material->m_flAfterDampGravity = (float)atof( token );
					else if ( !_stricmp( property, "damp_vel_mod" ) )
						material->m_flDampingVelocity = (float)atof( token );
					else if ( !_stricmp( property, "sprite" ) )
					{
						char sprite[64] = {};
						if ( MAT_FileBaseLength( token ) < (int)sizeof( sprite ) )
							COM_FileBase( token, sprite );
						size_t length = strlen( sprite );
						if ( length && isdigit( sprite[length - 1] ) )
							sprite[length - 1] = 0;
						sprintf( material->m_szTextName, "sprites/%s", sprite );
					}
					else if ( !_stricmp( property, "rendermode" ) )
						material->m_iRendermode = MAT_GetRenderMode( token );
					else if ( !_stricmp( property, "flags" ) )
						material->m_iCollisionFlags = atoi( token );
					else if ( !_stricmp( property, "afterdamp_flags" ) )
						material->m_iAfterDampFlags = atoi( token );
					else if ( !_stricmp( property, "velmod" ) )
						material->m_flVelMod = (float)atof( token );
					else if ( !_stricmp( property, "color" ) )
					{
						material->m_vColor.x = (float)atof( token );
						g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
						material->m_vColor.y = (float)atof( token );
						g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
						material->m_vColor.z = (float)atof( token );
					}
					else if ( !_stricmp( property, "damping_time" ) )
						material->m_flDampingTime = (float)atof( token );
					else if ( !_stricmp( property, "allow" ) )
						material->m_iSpawnDebris = atoi( token );
					else
					{
						m_MatFIO.WriteToErrorFile( textureName, "( %s unknown )", property );
						return false;
					}
				}
				else if ( !_stricmp( property, "allow" ) )
				{
					if ( !_stricmp( section, "smoke" ) )
						material->m_iSpawnSmoke = atoi( token );
					else if ( !_stricmp( section, "sparks" ) )
						material->m_iSpawnSparks = atoi( token );
					else
						material->m_iSpawnImpacts = atoi( token );
				}
				else if ( !_stricmp( section, "smoke" ) && !_stricmp( property, "color" ) )
				{
					material->m_vSmokeColor.x = (float)atof( token );
					g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
					material->m_vSmokeColor.y = (float)atof( token );
					g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
					material->m_vSmokeColor.z = (float)atof( token );
				}
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "decalname" ) )
					strcpy( material->m_szBulletImpactName, token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "minscale" ) )
					material->m_flMinScale = (float)atof( token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "maxscale" ) )
					material->m_flMaxScale = (float)atof( token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "decalnum" ) )
					material->m_iDecalNum = atoi( token );
				else if ( !_stricmp( section, "impacts" ) && !_stricmp( property, "randomflip" ) )
					material->m_bRandomFlip = atoi( token );
				else
				{
					m_MatFIO.WriteToErrorFile( textureName, "( %s unknown )", property );
					return false;
				}
			}
		}
		g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
		if ( !_stricmp( token, "]" ) )
			reading = false;
	}

	switch ( type )
	{
	case 1: material->m_chMaterialType = 'C'; break;
	case 2: material->m_chMaterialType = 'M'; break;
	case 3: material->m_chMaterialType = 'D'; break;
	case 4: material->m_chMaterialType = 'T'; break;
	case 5: material->m_chMaterialType = 'S'; break;
	case 6: material->m_chMaterialType = 'Y'; break;
	case 7: material->m_chMaterialType = 'N'; break;
	case 8: material->m_chMaterialType = 'X'; break;
	case 9: material->m_chMaterialType = 'A'; break;
	case 10: material->m_chMaterialType = 'K'; break;
	case 12: material->m_chMaterialType = 'W'; break;
	default: material->m_chMaterialType = PM_FindTextureType( textureName ); break;
	}
	return result;
}

void LoadMaterialFromNameList()
{
	char token[1024];
	while ( g_PFile )
	{
		do
		{
			g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
			if ( !_stricmp( token, "texture" ) )
				break;
			if ( !g_PFile )
				return;
		}
		while ( true );

		g_PFile = gEngfuncs.COM_ParseFile( g_PFile, token );
		char* name = new char[strlen( token ) + 1];
		g_pMaterialSystemPointer->FixTextureName( token, name );
		if ( TextureNameInList( name ) && !g_pMaterialSystemPointer->LoadMaterial( token ) )
			g_pMaterialSystemPointer->m_MatFIO.WriteToErrorFile( token, "Couldn't Load Material\n" );
		delete[] name;
	}
}

void LoadLevelMaterials()
{
	const char* level = gEngfuncs.pfnGetLevelName();
	if ( !level[0] )
		return;

	gEngfuncs.pfnAddCommand( "mat_reload", MAT_FlushAndReload );
	CMaterialSystem* defaults = new CMaterialSystem;
	g_pMaterialSystemPointer = defaults;
	// LoadMaterialFromNameList moves g_PFile through the file, so keep the start of each file to free it
	g_PFile = (char*)gEngfuncs.COM_LoadFile( "materials/czero.mat", 5, NULL );
	char* defaultsFile = g_PFile;
	LoadTextureLump( (char*)gEngfuncs.pfnGetLevelName() );
	LoadMaterialFromNameList();
	gEngfuncs.COM_FreeFile( defaultsFile );
	g_PFile = NULL;

	char currentLevel[32], filename[128];
	COM_FileBase( level, currentLevel );
	sprintf( filename, "materials/%s.mat", currentLevel );
	g_PFile = (char*)gEngfuncs.COM_LoadFile( filename, 5, NULL );
	if ( !g_PFile && mat_autocreate->value == 1.0f )
		CreateMaterialFile( currentLevel );
	char* levelFile = g_PFile;

	g_pMaterialSystemPointer = &g_pMaterialSystem;
	g_pMaterialSystem.ClearMaterials();
	LoadMaterialFromNameList();
	g_pMaterialSystemPointer->CopyMaterials( defaults );
	gEngfuncs.COM_FreeFile( levelFile );
	g_PFile = NULL;

	if ( mat_autocreate->value == 1.0f )
	{
		gEngfuncs.Con_DPrintf( "--------------------------------------------\n%d Materials Loaded Successfully.\n"
			"%d New Materials Created.\n%d Materials Failed to Load.\n--------------------------------------------\n",
			g_pMaterialSystemPointer->m_iElements, g_pMaterialSystemPointer->m_iNewMaterials,
			g_iDesiredMaterials - g_pMaterialSystemPointer->m_iElements);
		gEngfuncs.Cvar_SetValue( "mat_autocreate", 0.0f );
	}
	else
	{
		gEngfuncs.Con_DPrintf( "--------------------------------------------\n%d Materials Loaded Successfully.\n"
			"%d Materials Failed to Load.\n--------------------------------------------\n",
			g_pMaterialSystemPointer->m_iElements, g_iDesiredMaterials - g_pMaterialSystemPointer->m_iElements);
	}
	delete defaults;
}

void MAT_FlushAndReload()
{
	gEngfuncs.Con_DPrintf( "\nFlushing All Materials...\n" );
	LoadLevelMaterials();
	if ( mat_autocreate->value == 1.0f )
		gEngfuncs.Cvar_SetValue( "mat_autocreate", 0.0f );
}
