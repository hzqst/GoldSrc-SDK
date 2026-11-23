#include "HLTV/Core/delta.h"

#include "HLTV/common/common.h"
#include "HLTV/common/mem.h"
#include "HLTV/common/Bitbuffer.h"

#include "common/entity_state.h"
#include "common/event_args.h"
#include "common/usercmd.h"
#include "common/weaponinfo.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define DELTA_META_SCALE_PRECISION	4000.0f

#define DELTA_MIN_UNIT_SCALE			0.9999f
#define DELTA_MAX_UNIT_SCALE			1.0001f

#if defined( _WIN32 )
extern "C" _ACRTIMP void __cdecl _wassert( const wchar_t* message, const wchar_t* file, unsigned int line );

#define DELTA_SOURCE_PATH L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\hltv\\Core\\delta.cpp"
#endif

struct delta_link_s
{
	delta_link_s*			next;
	delta_description_t*	delta;
};

struct delta_definition_s
{
	char*	fieldName;
	int		fieldOffset;
};

struct delta_definition_list_s
{
	delta_definition_list_s*	next;
	char*					typeName;
	int						numelements;
	delta_definition_t*		pdefinition;
};

static char* DuplicateName( const char* name )
{
#ifdef _WIN32
	return _strdup( name );
#else
	return strdup( name );
#endif
}

extern "C"
{
	delta_registry_t*	g_deltaregistry;
	delta_s*			g_peventdelta;
	delta_s*			g_pweapondelta;
	delta_s*			g_pclientdelta;
	delta_s*			g_pcustomentitydelta;
	delta_s*			g_pentitydelta;
	delta_s*			g_pplayerdelta;
	bool				g_large_Time_Buffers;
	double				g_delta_Time;

#define DELTA_FIELD( type, field ) { #field, (int)offsetof( type, field ) }

	delta_definition_t g_ClientDataDefinition[] =
	{
		DELTA_FIELD( clientdata_t, origin[0] ),
		DELTA_FIELD( clientdata_t, origin[1] ),
		DELTA_FIELD( clientdata_t, origin[2] ),
		DELTA_FIELD( clientdata_t, velocity[0] ),
		DELTA_FIELD( clientdata_t, velocity[1] ),
		DELTA_FIELD( clientdata_t, velocity[2] ),
		DELTA_FIELD( clientdata_t, viewmodel ),
		DELTA_FIELD( clientdata_t, punchangle[0] ),
		DELTA_FIELD( clientdata_t, punchangle[1] ),
		DELTA_FIELD( clientdata_t, punchangle[2] ),
		DELTA_FIELD( clientdata_t, flags ),
		DELTA_FIELD( clientdata_t, waterlevel ),
		DELTA_FIELD( clientdata_t, watertype ),
		DELTA_FIELD( clientdata_t, view_ofs[0] ),
		DELTA_FIELD( clientdata_t, view_ofs[1] ),
		DELTA_FIELD( clientdata_t, view_ofs[2] ),
		DELTA_FIELD( clientdata_t, health ),
		DELTA_FIELD( clientdata_t, bInDuck ),
		DELTA_FIELD( clientdata_t, weapons ),
		DELTA_FIELD( clientdata_t, flTimeStepSound ),
		DELTA_FIELD( clientdata_t, flDuckTime ),
		DELTA_FIELD( clientdata_t, flSwimTime ),
		DELTA_FIELD( clientdata_t, waterjumptime ),
		DELTA_FIELD( clientdata_t, maxspeed ),
		DELTA_FIELD( clientdata_t, m_iId ),
		DELTA_FIELD( clientdata_t, ammo_nails ),
		DELTA_FIELD( clientdata_t, ammo_shells ),
		DELTA_FIELD( clientdata_t, ammo_cells ),
		DELTA_FIELD( clientdata_t, ammo_rockets ),
		DELTA_FIELD( clientdata_t, m_flNextAttack ),
		DELTA_FIELD( clientdata_t, physinfo ),
		DELTA_FIELD( clientdata_t, fov ),
		DELTA_FIELD( clientdata_t, weaponanim ),
		DELTA_FIELD( clientdata_t, tfstate ),
		DELTA_FIELD( clientdata_t, pushmsec ),
		DELTA_FIELD( clientdata_t, deadflag ),
		DELTA_FIELD( clientdata_t, iuser1 ),
		DELTA_FIELD( clientdata_t, iuser2 ),
		DELTA_FIELD( clientdata_t, iuser3 ),
		DELTA_FIELD( clientdata_t, iuser4 ),
		DELTA_FIELD( clientdata_t, fuser1 ),
		DELTA_FIELD( clientdata_t, fuser2 ),
		DELTA_FIELD( clientdata_t, fuser3 ),
		DELTA_FIELD( clientdata_t, fuser4 ),
		DELTA_FIELD( clientdata_t, vuser1[0] ),
		DELTA_FIELD( clientdata_t, vuser1[1] ),
		DELTA_FIELD( clientdata_t, vuser1[2] ),
		DELTA_FIELD( clientdata_t, vuser2[0] ),
		DELTA_FIELD( clientdata_t, vuser2[1] ),
		DELTA_FIELD( clientdata_t, vuser2[2] ),
		DELTA_FIELD( clientdata_t, vuser3[0] ),
		DELTA_FIELD( clientdata_t, vuser3[1] ),
		DELTA_FIELD( clientdata_t, vuser3[2] ),
		DELTA_FIELD( clientdata_t, vuser4[0] ),
		DELTA_FIELD( clientdata_t, vuser4[1] ),
		DELTA_FIELD( clientdata_t, vuser4[2] )
	};

	delta_definition_t g_WeaponDataDefinition[] =
	{
		DELTA_FIELD( weapon_data_t, m_iId ),
		DELTA_FIELD( weapon_data_t, m_iClip ),
		DELTA_FIELD( weapon_data_t, m_flNextPrimaryAttack ),
		DELTA_FIELD( weapon_data_t, m_flNextSecondaryAttack ),
		DELTA_FIELD( weapon_data_t, m_flTimeWeaponIdle ),
		DELTA_FIELD( weapon_data_t, m_fInReload ),
		DELTA_FIELD( weapon_data_t, m_fInSpecialReload ),
		DELTA_FIELD( weapon_data_t, m_flNextReload ),
		DELTA_FIELD( weapon_data_t, m_flPumpTime ),
		DELTA_FIELD( weapon_data_t, m_fReloadTime ),
		DELTA_FIELD( weapon_data_t, m_fAimedDamage ),
		DELTA_FIELD( weapon_data_t, m_fNextAimBonus ),
		DELTA_FIELD( weapon_data_t, m_fInZoom ),
		DELTA_FIELD( weapon_data_t, m_iWeaponState ),
		DELTA_FIELD( weapon_data_t, iuser1 ),
		DELTA_FIELD( weapon_data_t, iuser2 ),
		DELTA_FIELD( weapon_data_t, iuser3 ),
		DELTA_FIELD( weapon_data_t, iuser4 ),
		DELTA_FIELD( weapon_data_t, fuser1 ),
		DELTA_FIELD( weapon_data_t, fuser2 ),
		DELTA_FIELD( weapon_data_t, fuser3 ),
		DELTA_FIELD( weapon_data_t, fuser4 )
	};

	delta_definition_t g_UsercmdDataDefinition[] =
	{
		DELTA_FIELD( usercmd_t, lerp_msec ),
		DELTA_FIELD( usercmd_t, msec ),
		DELTA_FIELD( usercmd_t, lightlevel ),
		DELTA_FIELD( usercmd_t, viewangles[0] ),
		DELTA_FIELD( usercmd_t, viewangles[1] ),
		DELTA_FIELD( usercmd_t, viewangles[2] ),
		DELTA_FIELD( usercmd_t, buttons ),
		DELTA_FIELD( usercmd_t, forwardmove ),
		DELTA_FIELD( usercmd_t, sidemove ),
		DELTA_FIELD( usercmd_t, upmove ),
		DELTA_FIELD( usercmd_t, impulse ),
		DELTA_FIELD( usercmd_t, weaponselect ),
		DELTA_FIELD( usercmd_t, impact_index ),
		DELTA_FIELD( usercmd_t, impact_position[0] ),
		DELTA_FIELD( usercmd_t, impact_position[1] ),
		DELTA_FIELD( usercmd_t, impact_position[2] )
	};

	delta_definition_t g_EntityDataDefinition[] =
	{
		DELTA_FIELD( entity_state_t, startpos[0] ),
		DELTA_FIELD( entity_state_t, startpos[1] ),
		DELTA_FIELD( entity_state_t, startpos[2] ),
		DELTA_FIELD( entity_state_t, endpos[0] ),
		DELTA_FIELD( entity_state_t, endpos[1] ),
		DELTA_FIELD( entity_state_t, endpos[2] ),
		DELTA_FIELD( entity_state_t, impacttime ),
		DELTA_FIELD( entity_state_t, starttime ),
		DELTA_FIELD( entity_state_t, origin[0] ),
		DELTA_FIELD( entity_state_t, origin[1] ),
		DELTA_FIELD( entity_state_t, origin[2] ),
		DELTA_FIELD( entity_state_t, angles[0] ),
		DELTA_FIELD( entity_state_t, angles[1] ),
		DELTA_FIELD( entity_state_t, angles[2] ),
		DELTA_FIELD( entity_state_t, modelindex ),
		DELTA_FIELD( entity_state_t, frame ),
		DELTA_FIELD( entity_state_t, movetype ),
		DELTA_FIELD( entity_state_t, colormap ),
		DELTA_FIELD( entity_state_t, skin ),
		DELTA_FIELD( entity_state_t, solid ),
		DELTA_FIELD( entity_state_t, scale ),
		DELTA_FIELD( entity_state_t, effects ),
		DELTA_FIELD( entity_state_t, sequence ),
		DELTA_FIELD( entity_state_t, animtime ),
		DELTA_FIELD( entity_state_t, framerate ),
		DELTA_FIELD( entity_state_t, controller[0] ),
		DELTA_FIELD( entity_state_t, controller[1] ),
		DELTA_FIELD( entity_state_t, controller[2] ),
		DELTA_FIELD( entity_state_t, controller[3] ),
		DELTA_FIELD( entity_state_t, blending[0] ),
		DELTA_FIELD( entity_state_t, blending[1] ),
		DELTA_FIELD( entity_state_t, body ),
		DELTA_FIELD( entity_state_t, owner ),
		DELTA_FIELD( entity_state_t, rendermode ),
		DELTA_FIELD( entity_state_t, renderamt ),
		DELTA_FIELD( entity_state_t, renderfx ),
		DELTA_FIELD( entity_state_t, rendercolor.r ),
		DELTA_FIELD( entity_state_t, rendercolor.g ),
		DELTA_FIELD( entity_state_t, rendercolor.b ),
		DELTA_FIELD( entity_state_t, weaponmodel ),
		DELTA_FIELD( entity_state_t, gaitsequence ),
		DELTA_FIELD( entity_state_t, mins[0] ),
		DELTA_FIELD( entity_state_t, mins[1] ),
		DELTA_FIELD( entity_state_t, mins[2] ),
		DELTA_FIELD( entity_state_t, maxs[0] ),
		DELTA_FIELD( entity_state_t, maxs[1] ),
		DELTA_FIELD( entity_state_t, maxs[2] ),
		DELTA_FIELD( entity_state_t, aiment ),
		DELTA_FIELD( entity_state_t, basevelocity[0] ),
		DELTA_FIELD( entity_state_t, basevelocity[1] ),
		DELTA_FIELD( entity_state_t, basevelocity[2] ),
		DELTA_FIELD( entity_state_t, friction ),
		DELTA_FIELD( entity_state_t, gravity ),
		DELTA_FIELD( entity_state_t, spectator ),
		DELTA_FIELD( entity_state_t, velocity[0] ),
		DELTA_FIELD( entity_state_t, velocity[1] ),
		DELTA_FIELD( entity_state_t, velocity[2] ),
		DELTA_FIELD( entity_state_t, team ),
		DELTA_FIELD( entity_state_t, playerclass ),
		DELTA_FIELD( entity_state_t, health ),
		DELTA_FIELD( entity_state_t, usehull ),
		DELTA_FIELD( entity_state_t, oldbuttons ),
		DELTA_FIELD( entity_state_t, onground ),
		DELTA_FIELD( entity_state_t, iStepLeft ),
		DELTA_FIELD( entity_state_t, flFallVelocity ),
		DELTA_FIELD( entity_state_t, weaponanim ),
		DELTA_FIELD( entity_state_t, eflags ),
		DELTA_FIELD( entity_state_t, iuser1 ),
		DELTA_FIELD( entity_state_t, iuser2 ),
		DELTA_FIELD( entity_state_t, iuser3 ),
		DELTA_FIELD( entity_state_t, iuser4 ),
		DELTA_FIELD( entity_state_t, fuser1 ),
		DELTA_FIELD( entity_state_t, fuser2 ),
		DELTA_FIELD( entity_state_t, fuser3 ),
		DELTA_FIELD( entity_state_t, fuser4 ),
		DELTA_FIELD( entity_state_t, vuser1[0] ),
		DELTA_FIELD( entity_state_t, vuser1[1] ),
		DELTA_FIELD( entity_state_t, vuser1[2] ),
		DELTA_FIELD( entity_state_t, vuser2[0] ),
		DELTA_FIELD( entity_state_t, vuser2[1] ),
		DELTA_FIELD( entity_state_t, vuser2[2] ),
		DELTA_FIELD( entity_state_t, vuser3[0] ),
		DELTA_FIELD( entity_state_t, vuser3[1] ),
		DELTA_FIELD( entity_state_t, vuser3[2] ),
		DELTA_FIELD( entity_state_t, vuser4[0] ),
		DELTA_FIELD( entity_state_t, vuser4[1] ),
		DELTA_FIELD( entity_state_t, vuser4[2] )
	};

	delta_definition_t g_EventDataDefinition[] =
	{
		DELTA_FIELD( event_args_t, entindex ),
		DELTA_FIELD( event_args_t, origin[0] ),
		DELTA_FIELD( event_args_t, origin[1] ),
		DELTA_FIELD( event_args_t, origin[2] ),
		DELTA_FIELD( event_args_t, angles[0] ),
		DELTA_FIELD( event_args_t, angles[1] ),
		DELTA_FIELD( event_args_t, angles[2] ),
		DELTA_FIELD( event_args_t, fparam1 ),
		DELTA_FIELD( event_args_t, fparam2 ),
		DELTA_FIELD( event_args_t, iparam1 ),
		DELTA_FIELD( event_args_t, iparam2 ),
		DELTA_FIELD( event_args_t, bparam1 ),
		DELTA_FIELD( event_args_t, bparam2 ),
		DELTA_FIELD( event_args_t, ducking )
	};

#undef DELTA_FIELD

	delta_description_t g_MetaDescription[] =
	{
		{ DT_INTEGER, "fieldType", offsetof( delta_description_t, fieldType ), 1, 32, 1.0f, 1.0f, 0, { 0, 0 } },
		{ DT_STRING, "fieldName", offsetof( delta_description_t, fieldName ), 1, 1, 1.0f, 1.0f, 0, { 0, 0 } },
		{ DT_INTEGER, "fieldOffset", offsetof( delta_description_t, fieldOffset ), 1, 16, 1.0f, 1.0f, 0, { 0, 0 } },
		{ DT_INTEGER, "fieldSize", offsetof( delta_description_t, fieldSize ), 1, 8, 1.0f, 1.0f, 0, { 0, 0 } },
		{ DT_INTEGER, "significant_bits", offsetof( delta_description_t, significant_bits ),
			1, 8, 1.0f, 1.0f, 0, { 0, 0 } },
		{ DT_FLOAT, "premultiply", offsetof( delta_description_t, premultiply ),
			1, 32, DELTA_META_SCALE_PRECISION, 1.0f, 0, { 0, 0 } },
		{ DT_FLOAT, "postmultiply", offsetof( delta_description_t, postmultiply ),
			1, 32, DELTA_META_SCALE_PRECISION, 1.0f, 0, { 0, 0 } }
	};

	delta_t g_MetaDelta =
	{
		0, sizeof( g_MetaDescription ) / sizeof( g_MetaDescription[0] ), "", NULL, g_MetaDescription
	};
}

static delta_encoder_t*			g_encoders;
static delta_definition_list_t*	g_defs;

static int DELTA_ScaleNearOne( float scale )
{
	return !( scale < DELTA_MIN_UNIT_SCALE || scale > DELTA_MAX_UNIT_SCALE );
}

static int DELTA_TextChanged( const unsigned char* from, const unsigned char* to )
{
	int left;
	int right;

	left = (signed char)*from;
	if ( left == 0 )
		return *to != 0;

	right = (signed char)*to;
	if ( right == 0 )
		return 1;

	for ( ;; )
	{
		if ( tolower( left ) != tolower( right ) )
			return 1;

		left = (signed char)*++from;
		to++;
		if ( left == 0 )
			return *to != 0;

		right = (signed char)*to;
		if ( right == 0 )
			return 1;
	}
}

static int DELTA_ScaledTimeDiffers( const unsigned char* from, const unsigned char* to, int offset, float scale )
{
	int fromScaled;
	int toScaled;

	fromScaled = (int)( (double)scale * (double)*(float*)( from + offset ) );
	toScaled = (int)( (double)scale * (double)*(float*)( to + offset ) );
	return fromScaled != toScaled;
}

delta_description_t* DELTA_FindField( delta_s* pFields, const char* pszField )
{
	int fieldCount;
	int i;
	delta_description_t* pdd;

	fieldCount = pFields->fieldCount;
	if ( fieldCount <= 0 )
		return NULL;

	pdd = pFields->pdd;
	for ( i = 0; i < fieldCount; i++, pdd++ )
	{
		if ( strcasecmp( pdd->fieldName, pszField ) == 0 )
			return pdd;
	}

	return NULL;
}

int DELTA_FindFieldIndex( delta_s* pFields, const char* fieldname )
{
	int fieldCount;
	int i;

	fieldCount = pFields->fieldCount;
	if ( fieldCount <= 0 )
		return -1;

	for ( i = 0; i < fieldCount; i++ )
	{
		if ( strcasecmp( pFields->pdd[i].fieldName, fieldname ) == 0 )
			return i;
	}

	return -1;
}

void DELTA_SetField( delta_s* pFields, const char* fieldname )
{
	delta_description_t* pdd;

	pdd = DELTA_FindField( pFields, fieldname );
	if ( pdd != NULL )
		pdd->flags |= 1;
}

void DELTA_UnsetField( delta_s* pFields, const char* fieldname )
{
	delta_description_t* pdd;

	pdd = DELTA_FindField( pFields, fieldname );
	if ( pdd != NULL )
		pdd->flags &= ~1;
}

void DELTA_SetFieldByIndex( delta_s* pFields, int fieldNumber )
{
	pFields->pdd[fieldNumber].flags |= 1;
}

void DELTA_UnsetFieldByIndex( delta_s* pFields, int fieldNumber )
{
	pFields->pdd[fieldNumber].flags &= ~1;
}

void DELTA_ClearFlags( delta_s* pFields )
{
	int i;

	for ( i = 0; i < pFields->fieldCount; i++ )
		pFields->pdd[i].flags = 0;
}

int DELTA_CountSendFields( delta_s* pFields )
{
	int i;
	int count;
	delta_description_t* pdd;

	count = 0;
	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		pdd = &pFields->pdd[i];
		if ( pdd->flags & 1 )
		{
			pdd->stats.sendcount++;
			count++;
		}
	}

	return count;
}

void DELTA_MarkSendFields( unsigned char* from, unsigned char* to, delta_s* pFields )
{
	int i;
	delta_description_t* pdd;
	unsigned int type;
	int offset;

#if defined( _WIN32 )
	if ( !pFields || !pFields->pdd || !from || !to )
		_wassert( L"pFields && pFields->pdd && from && to", DELTA_SOURCE_PATH, 506 );
#endif

	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		int changed;

		pdd = &pFields->pdd[i];
		type = (unsigned int)pdd->fieldType & ~DT_SIGNED;
		offset = pdd->fieldOffset;
		changed = 0;

		if ( type == DT_BYTE )
			changed = from[offset] != to[offset];
		else if ( type == DT_SHORT )
			changed = *(unsigned short*)( to + offset ) != *(unsigned short*)( from + offset );
		else if ( type == DT_FLOAT || type == DT_INTEGER || type == DT_ANGLE )
			changed = *(unsigned int*)( from + offset ) != *(unsigned int*)( to + offset );
		else if ( type == DT_TIMEWINDOW_8 )
			changed = DELTA_ScaledTimeDiffers( from, to, offset, 100.0f );
		else if ( type == DT_TIMEWINDOW_BIG )
			changed = DELTA_ScaledTimeDiffers( from, to, offset, 1000.0f );
		else if ( type == DT_STRING )
			changed = DELTA_TextChanged( from + offset, to + offset );

		if ( changed )
			pdd->flags |= 1;
	}

	if ( pFields->conditionalencode != NULL )
		pFields->conditionalencode( pFields, from, to );
}

void DELTA_SetSendFlagBits( delta_s* pFields, int* bits, int* bytecount )
{
	int i;
	int lastbit;
	unsigned int* words;

	bits[0] = 0;
	bits[1] = 0;
	lastbit = -1;
	words = (unsigned int*)bits;

	for ( i = pFields->fieldCount - 1; i >= 0; i-- )
	{
		if ( pFields->pdd[i].flags & 1 )
		{
			if ( lastbit == -1 )
				lastbit = i;
			words[i > 31] |= 1u << ( i & 31 );
		}
	}

	if ( lastbit == -1 )
		*bytecount = 0;
	else
		*bytecount = ( lastbit >> 3 ) + 1;
}

void DELTA_WriteMarkedFields( BitBuffer* stream, unsigned char* from, unsigned char* to, delta_s* pFields )
{
	int i;

	(void)from;

	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		delta_description_t* pdd;
		unsigned int raw;
		unsigned int type;
		int isSigned;
		int offset;

		pdd = &pFields->pdd[i];
		if ( ( pdd->flags & 1 ) == 0 )
			continue;

		raw = (unsigned int)pdd->fieldType;
		type = raw & ~DT_SIGNED;
		isSigned = ( raw & DT_SIGNED ) != 0;
		offset = pdd->fieldOffset;

		if ( type == DT_BYTE )
		{
			if ( isSigned )
			{
				int encoded;

				encoded = (int)( pdd->premultiply * (float)(signed char)to[offset] );
				stream->WriteSBits( (signed char)encoded, pdd->significant_bits );
			}
			else
			{
				unsigned int encoded;

				encoded = (unsigned char)(int)( pdd->premultiply * (float)to[offset] );
				stream->WriteBits( encoded, pdd->significant_bits );
			}
		}
		else if ( type == DT_SHORT )
		{
			if ( isSigned )
			{
				int encoded;

				encoded = (int)( pdd->premultiply * (float)*(short*)( to + offset ) );
				stream->WriteSBits( (short)encoded, pdd->significant_bits );
			}
			else
			{
				unsigned int encoded;

				encoded = (unsigned short)(int)( pdd->premultiply * (float)*(unsigned short*)( to + offset ) );
				stream->WriteBits( encoded, pdd->significant_bits );
			}
		}
		else if ( type == DT_FLOAT )
		{
			int encoded;

			encoded = (int)(long long)( *(float*)( to + offset ) * pdd->premultiply );
			if ( isSigned )
				stream->WriteSBits( encoded, pdd->significant_bits );
			else
				stream->WriteBits( (unsigned int)encoded, pdd->significant_bits );
		}
		else if ( type == DT_INTEGER )
		{
			if ( isSigned )
			{
				int value;

				value = *(int*)( to + offset );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = (int)( pdd->premultiply * (float)value );
				stream->WriteSBits( value, pdd->significant_bits );
			}
			else
			{
				unsigned int value;

				value = *(unsigned int*)( to + offset );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = (unsigned int)(long long)( (float)value * pdd->premultiply );
				stream->WriteBits( value, pdd->significant_bits );
			}
		}
		else if ( type == DT_ANGLE )
		{
			stream->WriteBitAngle( *(float*)( to + offset ), pdd->significant_bits );
		}
		else if ( type == DT_TIMEWINDOW_8 )
		{
			float value;

			value = *(float*)( to + offset );
			if ( !g_large_Time_Buffers )
				stream->WriteSBits( (int)( g_delta_Time * 100.0 - value * 100.0f ), 8 );
			else
				stream->WriteFloat( value );
		}
		else if ( type == DT_TIMEWINDOW_BIG )
		{
			float value;

			value = *(float*)( to + offset );
			if ( !g_large_Time_Buffers )
			{
				int timePart;
				int valuePart;

				timePart = (int)( (double)pdd->premultiply * g_delta_Time );
				valuePart = (int)( pdd->premultiply * value );
				stream->WriteSBits( timePart - valuePart, pdd->significant_bits );
			}
			else
			{
				stream->WriteFloat( value );
			}
		}
		else if ( type == DT_STRING )
		{
			unsigned char* text;

			text = to + offset;
			if ( text != NULL )
			{
				while ( *text != 0 )
				{
					stream->WriteBits( (signed char)*text, 8 );
					text++;
				}
			}
			stream->WriteBits( 0, 8 );
		}
	}
}

int DELTA_CheckDelta( unsigned char* from, unsigned char* to, delta_s* pFields )
{
	DELTA_ClearFlags( pFields );
	DELTA_MarkSendFields( from, to, pFields );
	return DELTA_CountSendFields( pFields );
}

void DELTA_WriteHeader( BitBuffer* stream, deltacallback_t* header )
{
	unsigned int span;
	int large;

	span = (unsigned int)( header->num - header->numbase );
	large = ( span - 1u ) > 0x3eu;

	if ( !header->full )
	{
		stream->WriteBit( header->remove );
		if ( large )
		{
			stream->WriteBit( 1 );
			stream->WriteBits( (unsigned int)header->num, 11 );
		}
		else
		{
			stream->WriteBit( 0 );
			stream->WriteBits( span, 6 );
		}
	}
	else if ( span != 1u )
	{
		stream->WriteBit( 0 );
		if ( large )
		{
			stream->WriteBit( 1 );
			stream->WriteBits( (unsigned int)header->num, 11 );
		}
		else
		{
			stream->WriteBit( 0 );
			stream->WriteBits( span, 6 );
		}
	}
	else
	{
		stream->WriteBit( 1 );
	}

	header->numbase = header->num;
	if ( header->remove )
		return;

	stream->WriteBit( header->custom );
	if ( header->instanced_baseline )
	{
		if ( header->newbl )
		{
			stream->WriteBit( 1 );
			stream->WriteBits( (unsigned int)header->newblindex, 6 );
		}
		else
		{
			stream->WriteBit( 0 );
		}
	}

	if ( header->full && !header->newbl )
	{
		if ( header->offset != 0 )
		{
			stream->WriteBit( 1 );
			stream->WriteBits( (unsigned int)header->offset, 6 );
		}
		else
		{
			stream->WriteBit( 0 );
		}
	}
}

int DELTA_WriteDelta( BitBuffer* stream, unsigned char* from, unsigned char* to, bool force,
		delta_s* pFields, deltacallback_t* header )
{
	int sendCount;
	unsigned char packed[32];
	int byteCount;
	int i;

#if defined( _WIN32 )
	if ( !pFields || !from || !to )
		_wassert( L"pFields && from && to", DELTA_SOURCE_PATH, 924 );
#endif

	DELTA_ClearFlags( pFields );
	DELTA_MarkSendFields( from, to, pFields );
	sendCount = DELTA_CountSendFields( pFields );
	if ( sendCount == 0 && !force )
		return 0;

	memset( packed, 0, sizeof( packed ) );
	DELTA_SetSendFlagBits( pFields, (int*)packed, &byteCount );
	if ( header != NULL )
		DELTA_WriteHeader( stream, header );

	stream->WriteBits( (unsigned int)byteCount, 3 );
	for ( i = 0; i < byteCount; i++ )
		stream->WriteBits( packed[i], 8 );

	DELTA_WriteMarkedFields( stream, from, to, pFields );
	return 1;
}

int DELTA_ParseDelta( BitBuffer* stream, unsigned char* from, unsigned char* to, delta_s* pFields, int bufferSize )
{
	int startbit;
	int bits[2];
	int byteCount;
	int i;
	unsigned char* packed;
	unsigned int* words;

	startbit = stream->CurrentBit();
	bits[0] = 0;
	bits[1] = 0;
	byteCount = (int)stream->ReadBits( 3 );
	if ( stream->sizeError || byteCount < 0 || byteCount > (int)sizeof( bits ) )
		return stream->CurrentBit() - startbit;

	packed = (unsigned char*)bits;
	for ( i = 0; i < byteCount; i++ )
	{
		packed[i] = (unsigned char)stream->ReadBits( 8 );
		if ( stream->sizeError )
			return stream->CurrentBit() - startbit;
	}

	words = (unsigned int*)bits;
	for ( i = 0; i < pFields->fieldCount; i++ )
	{
		delta_description_t* pdd;
		unsigned int raw;
		unsigned int type;
		int isSigned;
		int offset;
		int marked;

		pdd = &pFields->pdd[i];
		offset = pdd->fieldOffset;
		if ( offset < 0 || offset >= bufferSize )
			continue;

		raw = (unsigned int)pdd->fieldType;
		type = raw & ~DT_SIGNED;
		isSigned = ( raw & DT_SIGNED ) != 0;
		marked = ( i < 64 ) && ( ( words[i > 31] & ( 1u << ( i & 31 ) ) ) != 0 );

		if ( !marked )
		{
			if ( type == DT_BYTE )
			{
				to[offset] = from[offset];
			}
			else if ( type == DT_SHORT )
			{
				if ( bufferSize - offset >= (int)sizeof( unsigned short ) )
					*(unsigned short*)( to + offset ) = *(unsigned short*)( from + offset );
			}
			else if ( type == DT_FLOAT || type == DT_INTEGER || type == DT_ANGLE ||
				type == DT_TIMEWINDOW_8 || type == DT_TIMEWINDOW_BIG )
			{
				if ( bufferSize - offset >= (int)sizeof( unsigned int ) )
					*(unsigned int*)( to + offset ) = *(unsigned int*)( from + offset );
			}
			else if ( type == DT_STRING )
			{
				unsigned char* src;
				unsigned char* dst;
				int copied;
				bool ended;

				src = from + offset;
				dst = to + offset;
				copied = 0;
				ended = false;
				while ( copied < bufferSize - offset )
				{
					unsigned char ch;

					ch = *src++;
					*dst++ = ch;
					copied++;
					if ( ch == 0 )
					{
						ended = true;
						break;
					}
				}
				if ( !ended && copied > 0 )
					dst[-1] = 0;
			}
			continue;
		}

		if ( stream->sizeError )
			break;

		pdd->stats.receivedcount++;

		if ( type == DT_BYTE )
		{
			if ( isSigned )
			{
				int value;

				value = stream->ReadSBits( pdd->significant_bits );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = (int)( (float)(signed char)value / pdd->premultiply );
				to[offset] = (unsigned char)value;
			}
			else
			{
				unsigned int value;

				value = stream->ReadBits( pdd->significant_bits );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = (unsigned int)(int)( (float)(unsigned char)value / pdd->premultiply );
				to[offset] = (unsigned char)value;
			}
		}
		else if ( type == DT_SHORT )
		{
			if ( bufferSize - offset < (int)sizeof( short ) )
				continue;

			if ( isSigned )
			{
				int value;

				value = (short)stream->ReadSBits( pdd->significant_bits );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = (short)(int)( (float)(short)value / pdd->premultiply );
				*(short*)( to + offset ) = (short)value;
			}
			else
			{
				unsigned int value;

				value = (unsigned short)stream->ReadBits( pdd->significant_bits );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = (unsigned short)(int)( (float)value / pdd->premultiply );
				*(unsigned short*)( to + offset ) = (unsigned short)value;
			}
		}
		else if ( type == DT_FLOAT )
		{
			float decoded;

			if ( bufferSize - offset < (int)sizeof( float ) )
				continue;

			if ( isSigned )
				decoded = (float)stream->ReadSBits( pdd->significant_bits );
			else
				decoded = (float)stream->ReadBits( pdd->significant_bits );
			if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
				decoded = decoded / pdd->premultiply;
			*(float*)( to + offset ) = decoded;
		}
		else if ( type == DT_INTEGER )
		{
			if ( bufferSize - offset < (int)sizeof( int ) )
				continue;

			if ( isSigned )
			{
				int value;

				value = stream->ReadSBits( pdd->significant_bits );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
				{
					value = (int)(long long)( (float)(unsigned int)value / pdd->premultiply );
				}
				*(int*)( to + offset ) = value;
			}
			else
			{
				unsigned int value;

				value = stream->ReadBits( pdd->significant_bits );
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = (unsigned int)(int)( (float)(int)value / pdd->premultiply );
				*(unsigned int*)( to + offset ) = value;
			}
		}
		else if ( type == DT_ANGLE )
		{
			if ( bufferSize - offset < (int)sizeof( float ) )
				continue;

			*(float*)( to + offset ) = stream->ReadBitAngle( pdd->significant_bits );
		}
		else if ( type == DT_TIMEWINDOW_8 )
		{
			float value;

			if ( bufferSize - offset < (int)sizeof( float ) )
				continue;

			if ( g_large_Time_Buffers )
				value = stream->ReadFloat();
			else
				value = (float)g_delta_Time - (float)stream->ReadSBits( 8 ) / 100.0f;
			*(float*)( to + offset ) = value;
		}
		else if ( type == DT_TIMEWINDOW_BIG )
		{
			float value;

			if ( bufferSize - offset < (int)sizeof( float ) )
				continue;

			if ( g_large_Time_Buffers )
			{
				value = stream->ReadFloat();
			}
			else
			{
				int ticks;

				ticks = stream->ReadSBits( pdd->significant_bits );
				value = (float)g_delta_Time * pdd->premultiply - (float)ticks;
				if ( !DELTA_ScaleNearOne( pdd->premultiply ) )
					value = value / pdd->premultiply;
			}
			*(float*)( to + offset ) = value;
		}
		else if ( type == DT_STRING )
		{
			unsigned char* dst;
			int copied;
			bool ended;

			dst = to + offset;
			copied = 0;
			ended = false;
			while ( !stream->sizeError )
			{
				unsigned char ch;

				ch = (unsigned char)stream->ReadBits( 8 );
				if ( stream->sizeError )
					break;
				if ( copied < bufferSize - offset )
				{
					*dst++ = ch;
					copied++;
					if ( ch == 0 )
					{
						ended = true;
						break;
					}
				}
				else if ( ch == 0 )
				{
					break;
				}
			}
			if ( !ended )
			{
				if ( copied < bufferSize - offset )
					*dst = 0;
				else if ( copied > 0 )
					dst[-1] = 0;
			}
		}
	}

	return stream->CurrentBit() - startbit;
}

int DELTA_TestDelta( unsigned char* from, unsigned char* to, delta_s* pFields )
{
	int i;
	int count;
	int highestBit;
	int neededBits;

#if defined( _WIN32 )
	if ( !pFields || !pFields->pdd || !from || !to )
		_wassert( L"pFields && pFields->pdd && from && to", DELTA_SOURCE_PATH, 1265 );
#endif

	count = pFields->fieldCount;
	if ( count <= 0 )
		return 0;

	highestBit = -1;
	neededBits = 0;
	for ( i = 0; i < count; i++ )
	{
		delta_description_t* pdd;
		unsigned int type;
		int offset;
		int changed;

		pdd = &pFields->pdd[i];
		type = (unsigned int)pdd->fieldType & ~DT_SIGNED;
		offset = pdd->fieldOffset;
		changed = 0;

		if ( type == DT_BYTE )
		{
			changed = from[offset] != to[offset];
		}
		else if ( type == DT_SHORT )
		{
			changed = *(unsigned short*)( from + offset ) != *(unsigned short*)( to + offset );
		}
		else if ( type == DT_FLOAT || type == DT_INTEGER || type == DT_ANGLE )
		{
			changed = *(unsigned int*)( from + offset ) != *(unsigned int*)( to + offset );
		}
		else if ( type == DT_TIMEWINDOW_8 )
		{
			changed = DELTA_ScaledTimeDiffers( from, to, offset, 100.0f );
		}
		else if ( type == DT_TIMEWINDOW_BIG )
		{
			changed = DELTA_ScaledTimeDiffers( from, to, offset, 1000.0f );
		}
		else if ( type == DT_STRING )
		{
			unsigned char* fromText;
			unsigned char* toText;
			int index;
			int different;
			int length;
			unsigned char ch;

			toText = to + offset;
			if ( toText[0] == 0 )
				continue;

			fromText = from + offset;
			index = 0;
			different = 0;
			length = 0;
			ch = toText[0];
			for ( ;; )
			{
				length = index * 8;
				if ( different == 0 )
				{
					if ( tolower( (signed char)fromText[index] ) != tolower( (signed char)ch ) )
					{
						pdd->flags |= 1;
						different = 1;
					}
					index++;
					ch = toText[index];
					if ( ch == 0 )
						break;
				}
				else
				{
					index++;
					ch = toText[index];
					if ( ch == 0 )
						break;
				}
			}

			if ( different )
				neededBits += length + 16;
			continue;
		}

		if ( changed )
		{
			highestBit = i;
			neededBits += pdd->significant_bits;
		}
	}

	if ( highestBit != -1 )
		neededBits += 8 * ( highestBit >> 3 ) + 8;

	return neededBits;
}

void DELTA_AddEncoder( char* name,
		void ( *conditionalencode )( delta_s* pFields, const unsigned char* from, const unsigned char* to ) )
{
	delta_encoder_t* encoder;

	encoder = (delta_encoder_t*)Mem_ZeroMalloc( sizeof( delta_encoder_t ) );
	encoder->name = DuplicateName( name );
	encoder->conditionalencode = conditionalencode;
	encoder->next = g_encoders;
	g_encoders = encoder;
}

void DELTA_ClearEncoders()
{
	delta_encoder_t* encoder;

	encoder = g_encoders;
	while ( encoder != NULL )
	{
		delta_encoder_t* next;

		next = encoder->next;
		free( encoder->name );
		free( encoder );
		encoder = next;
	}
	g_encoders = NULL;
}

encoder_t DELTA_LookupEncoder( char* name )
{
	delta_encoder_t* encoder;

	for ( encoder = g_encoders; encoder != NULL; encoder = encoder->next )
	{
		if ( strcasecmp( encoder->name, name ) == 0 )
			return encoder->conditionalencode;
	}

	return NULL;
}

int DELTA_CountLinks( delta_link_t* plinks )
{
	int count;

	count = 0;
	while ( plinks != NULL )
	{
		count++;
		plinks = plinks->next;
	}
	return count;
}

void DELTA_ReverseLinks( delta_link_t** plinks )
{
	delta_link_t* current;
	delta_link_t* previous;
	delta_link_t* next;

	current = *plinks;
	if ( current == NULL )
		return;

	previous = NULL;
	for ( ;; )
	{
		next = current->next;
		current->next = previous;
		previous = current;
		if ( next == NULL )
			break;
		current = next;
	}
	*plinks = current;
}

void DELTA_ClearLinks( delta_link_t** plinks )
{
	delta_link_t* link;

	link = *plinks;
	while ( link != NULL )
	{
		delta_link_t* next;

		next = link->next;
		free( link );
		link = next;
	}
	*plinks = NULL;
}

delta_s* DELTA_BuildFromLinks( delta_link_t** pplinks )
{
	delta_s* pdelta;
	int count;
	delta_description_t* descriptions;
	delta_description_t* dest;
	delta_link_t* link;

	pdelta = (delta_s*)Mem_ZeroMalloc( sizeof( delta_s ) );
	DELTA_ReverseLinks( pplinks );
	count = DELTA_CountLinks( *pplinks );
	descriptions = (delta_description_t*)Mem_ZeroMalloc( sizeof( delta_description_t ) * (unsigned int)count );
	dest = descriptions;
	for ( link = *pplinks; link != NULL; link = link->next, dest++ )
	{
		*dest = *link->delta;
		free( link->delta );
		link->delta = NULL;
	}
	DELTA_ClearLinks( pplinks );
	pdelta->dynamic = 1;
	pdelta->fieldCount = count;
	pdelta->pdd = descriptions;
	return pdelta;
}

int DELTA_FindOffset( int count, delta_definition_t* pdef, char* fieldname )
{
	int i;

	if ( count <= 0 )
		return 0;

	for ( i = 0; i < count; i++, pdef++ )
	{
		if ( strcasecmp( fieldname, pdef->fieldName ) == 0 )
			return pdef->fieldOffset;
	}

	return 0;
}

bool DELTA_ParseType( delta_description_t* pdelta, char** pstream )
{
	char* cursor;

	for ( ;; )
	{
		cursor = *pstream;
		do
		{
			cursor = COM_Parse( cursor );
			*pstream = cursor;
			if ( com_token[0] == 0 )
				return false;
		}
		while ( strcasecmp( com_token, "|" ) == 0 );

		if ( strcasecmp( com_token, "," ) == 0 )
			return true;

		if ( strcasecmp( com_token, "DT_SIGNED" ) == 0 )
			pdelta->fieldType |= (int)DT_SIGNED;
		else if ( strcasecmp( com_token, "DT_BYTE" ) == 0 )
			pdelta->fieldType |= DT_BYTE;
		else if ( strcasecmp( com_token, "DT_SHORT" ) == 0 )
			pdelta->fieldType |= DT_SHORT;
		else if ( strcasecmp( com_token, "DT_FLOAT" ) == 0 )
			pdelta->fieldType |= DT_FLOAT;
		else if ( strcasecmp( com_token, "DT_INTEGER" ) == 0 )
			pdelta->fieldType |= DT_INTEGER;
		else if ( strcasecmp( com_token, "DT_ANGLE" ) == 0 )
			pdelta->fieldType |= DT_ANGLE;
		else if ( strcasecmp( com_token, "DT_TIMEWINDOW_8" ) == 0 )
			pdelta->fieldType |= DT_TIMEWINDOW_8;
		else if ( strcasecmp( com_token, "DT_TIMEWINDOW_BIG" ) == 0 )
			pdelta->fieldType |= DT_TIMEWINDOW_BIG;
		else if ( strcasecmp( com_token, "DT_STRING" ) == 0 )
			pdelta->fieldType |= DT_STRING;
		else
			return false;
	}
}

bool DELTA_ParseField( int count, delta_definition_t* pdefinition, delta_link_t* pField, char** pstream )
{
	int readPost;
	char* cursor;
	int fieldOffset;
	int i;
	delta_description_t* delta;

	if ( strcasecmp( com_token, "DEFINE_DELTA" ) == 0 )
	{
		readPost = 0;
	}
	else if ( strcasecmp( com_token, "DEFINE_DELTA_POST" ) == 0 )
	{
		readPost = 1;
	}
	else
	{
		return false;
	}

	cursor = COM_Parse( *pstream );
	*pstream = cursor;
	if ( strcasecmp( com_token, "(" ) != 0 )
		return false;

	*pstream = COM_Parse( cursor );
	if ( com_token[0] == 0 )
		return false;

	strncpy( pField->delta->fieldName, com_token, 31 );
	pField->delta->fieldName[31] = 0;

	fieldOffset = 0;
	if ( count > 0 )
	{
		for ( i = 0; i < count; i++, pdefinition++ )
		{
			if ( strcasecmp( com_token, pdefinition->fieldName ) == 0 )
			{
				fieldOffset = pdefinition->fieldOffset;
				break;
			}
		}
	}
	pField->delta->fieldOffset = fieldOffset;

	*pstream = COM_Parse( *pstream );
	if ( !DELTA_ParseType( pField->delta, pstream ) )
		return false;

	*pstream = COM_Parse( *pstream );
	delta = pField->delta;
	delta->fieldSize = 1;
	delta->significant_bits = (int)strtol( com_token, NULL, 10 );

	cursor = COM_Parse( *pstream );
	*pstream = cursor;
	*pstream = COM_Parse( cursor );
	delta->premultiply = (float)strtod( com_token, NULL );

	if ( readPost )
	{
		cursor = COM_Parse( *pstream );
		*pstream = cursor;
		*pstream = COM_Parse( cursor );
		delta->postmultiply = (float)strtod( com_token, NULL );
	}
	else
	{
		delta->postmultiply = 1.0f;
	}

	cursor = COM_Parse( *pstream );
	*pstream = cursor;
	if ( strcasecmp( com_token, ")" ) != 0 )
		return false;

	*pstream = COM_Parse( cursor );
	if ( strcasecmp( com_token, "," ) != 0 )
		COM_UngetToken();

	return true;
}

void DELTA_FreeDescription( delta_s** ppdesc )
{
	delta_s* pdesc;

	if ( ppdesc == NULL )
		return;

	pdesc = *ppdesc;
	if ( pdesc == NULL )
		return;

	if ( pdesc->dynamic != 0 )
		free( pdesc->pdd );

	free( pdesc );
	*ppdesc = NULL;
}

void DELTA_AddDefinition( char* name, delta_definition_t* pdef, int numelements )
{
	delta_definition_list_t* entry;

	for ( entry = g_defs; entry != NULL; entry = entry->next )
	{
		if ( strcasecmp( name, entry->typeName ) == 0 )
			break;
	}

	if ( entry == NULL )
	{
		entry = (delta_definition_list_t*)Mem_ZeroMalloc( sizeof( delta_definition_list_t ) );
		entry->next = g_defs;
		g_defs = entry;
		entry->typeName = DuplicateName( name );
	}

	entry->pdefinition = pdef;
	entry->numelements = numelements;
}

void DELTA_ClearDefinitions()
{
	delta_definition_list_t* entry;

	entry = g_defs;
	while ( entry != NULL )
	{
		delta_definition_list_t* next;

		next = entry->next;
		free( entry->typeName );
		free( entry );
		entry = next;
	}
	g_defs = NULL;
}

delta_definition_t* DELTA_FindDefinition( char* name, int* count )
{
	delta_definition_list_t* entry;

	*count = 0;
	for ( entry = g_defs; entry != NULL; entry = entry->next )
	{
		if ( strcasecmp( name, entry->typeName ) == 0 )
		{
			*count = entry->numelements;
			return entry->pdefinition;
		}
	}

	return NULL;
}

void DELTA_SkipDescription( char** pstream )
{
	char* cursor;

	cursor = COM_Parse( *pstream );
	*pstream = cursor;
	do
	{
		cursor = COM_Parse( cursor );
		*pstream = cursor;
	}
	while ( com_token[0] != 0 && strcasecmp( com_token, "}" ) != 0 );
}

bool DELTA_ParseOneField( char** ppstream, delta_link_t** pplist, int count, delta_definition_t* pdefinition )
{
	for ( ;; )
	{
		delta_description_t* delta;
		int readPost;
		char* cursor;
		int fieldOffset;
		int i;
		delta_link_t* link;

		if ( strcasecmp( com_token, "}" ) == 0 )
		{
			COM_UngetToken();
			return true;
		}

		*ppstream = COM_Parse( *ppstream );
		if ( com_token[0] == 0 )
			return true;

		delta = (delta_description_t*)Mem_ZeroMalloc( sizeof( delta_description_t ) );
		if ( strcasecmp( com_token, "DEFINE_DELTA" ) == 0 )
		{
			readPost = 0;
		}
		else if ( strcasecmp( com_token, "DEFINE_DELTA_POST" ) == 0 )
		{
			readPost = 1;
		}
		else
		{
			return false;
		}

		cursor = COM_Parse( *ppstream );
		*ppstream = cursor;
		fieldOffset = strcasecmp( com_token, "(" );
		if ( fieldOffset != 0 )
			return false;

		*ppstream = COM_Parse( cursor );
		if ( com_token[0] == 0 )
			return false;

		strncpy( delta->fieldName, com_token, 31 );
		delta->fieldName[31] = 0;

		if ( count > 0 )
		{
			delta_definition_t* definition;

			definition = pdefinition;
			for ( i = 0; i < count; i++, definition++ )
			{
				if ( strcasecmp( com_token, definition->fieldName ) == 0 )
				{
					fieldOffset = definition->fieldOffset;
					break;
				}
			}
		}
		delta->fieldOffset = fieldOffset;

		*ppstream = COM_Parse( *ppstream );
		if ( !DELTA_ParseType( delta, ppstream ) )
			return false;

		*ppstream = COM_Parse( *ppstream );
		delta->fieldSize = 1;
		delta->significant_bits = (int)strtol( com_token, NULL, 10 );

		cursor = COM_Parse( *ppstream );
		*ppstream = cursor;
		*ppstream = COM_Parse( cursor );
		delta->premultiply = (float)strtod( com_token, NULL );

		if ( readPost )
		{
			cursor = COM_Parse( *ppstream );
			*ppstream = cursor;
			*ppstream = COM_Parse( cursor );
			delta->postmultiply = (float)strtod( com_token, NULL );
		}
		else
		{
			delta->postmultiply = 1.0f;
		}

		cursor = COM_Parse( *ppstream );
		*ppstream = cursor;
		if ( strcasecmp( com_token, ")" ) != 0 )
			return false;

		*ppstream = COM_Parse( cursor );
		if ( strcasecmp( com_token, "," ) != 0 )
			COM_UngetToken();

		link = (delta_link_t*)Mem_ZeroMalloc( sizeof( delta_link_t ) );
		link->delta = delta;
		link->next = *pplist;
		*pplist = link;
	}
}

bool DELTA_ParseDescription( char* name, delta_s** ppdesc, char* pstream )
{
	char encoder[32];
	char source[32];
	char* p;
	delta_link_t* links;
	int count;
	delta_definition_t* pdefinition;

	encoder[0] = 0;
	if ( ppdesc == NULL )
		return false;

	*ppdesc = NULL;
	if ( pstream == NULL )
		return false;

	p = pstream;
	links = NULL;
	for ( ;; )
	{
		p = COM_Parse( p );
		if ( com_token[0] == 0 )
		{
			delta_s* built;

			built = DELTA_BuildFromLinks( &links );
			if ( encoder[0] != 0 )
			{
				strncpy( built->conditionalencodename, encoder, 31 );
				built->conditionalencodename[31] = 0;
				built->conditionalencode = NULL;
			}
			*ppdesc = built;
			return true;
		}

		if ( strcasecmp( com_token, name ) != 0 )
		{
			p = COM_Parse( p );
			do
			{
				p = COM_Parse( p );
			}
			while ( com_token[0] != 0 && strcasecmp( com_token, "}" ) != 0 );
			continue;
		}

		pdefinition = DELTA_FindDefinition( com_token, &count );
		if ( pdefinition == NULL )
			return false;

		p = COM_Parse( p );
		if ( com_token[0] == 0 )
			return false;

		if ( strcasecmp( com_token, "none" ) != 0 )
		{
			strncpy( source, com_token, 31 );
			source[31] = 0;
			p = COM_Parse( p );
			if ( com_token[0] == 0 )
				return false;
			strncpy( encoder, com_token, 31 );
			encoder[31] = 0;
		}

		for ( ;; )
		{
			p = COM_Parse( p );
			if ( com_token[0] == 0 || strcasecmp( com_token, "}" ) == 0 )
				break;
			if ( strcasecmp( com_token, "{" ) != 0 )
				return false;

			for ( ;; )
			{
				delta_link_t field;
				delta_link_t* link;

				p = COM_Parse( p );
				if ( com_token[0] == 0 )
					break;

				field.next = NULL;
				field.delta = (delta_description_t*)Mem_ZeroMalloc( sizeof( delta_description_t ) );
				if ( !DELTA_ParseField( count, pdefinition, &field, &p ) )
					return false;

				link = (delta_link_t*)Mem_ZeroMalloc( sizeof( delta_link_t ) );
				link->next = links;
				link->delta = field.delta;
				links = link;
				if ( strcasecmp( com_token, "}" ) == 0 )
				{
					COM_UngetToken();
					break;
				}
			}
		}
	}
}

bool DELTA_Load( char* name, delta_s** ppdesc, char* pszFile )
{
	(void)name;
	(void)ppdesc;
	(void)pszFile;
	return false;
}

void DELTA_RegisterDescription( char* name )
{
	delta_registry_t* entry;

	entry = (delta_registry_t*)Mem_ZeroMalloc( sizeof( delta_registry_t ) );
	entry->next = g_deltaregistry;
	g_deltaregistry = entry;
	entry->name = DuplicateName( name );
	entry->pdesc = NULL;
}

void DELTA_ClearRegistrations()
{
	delta_registry_t* entry;

	entry = g_deltaregistry;
	while ( entry != NULL )
	{
		delta_registry_t* next;

		next = entry->next;
		free( entry->name );
		DELTA_FreeDescription( &entry->pdesc );
		free( entry );
		entry = next;
	}
	g_deltaregistry = NULL;
}

delta_s** DELTA_LookupRegistration( char* name )
{
	delta_registry_t* entry;

	for ( entry = g_deltaregistry; entry != NULL; entry = entry->next )
	{
		if ( strcasecmp( entry->name, name ) == 0 )
			return &entry->pdesc;
	}

	return NULL;
}

void DELTA_ClearStats( delta_s* p )
{
	int i;

	if ( p == NULL )
		return;

	for ( i = 0; i < p->fieldCount; i++ )
	{
		p->pdd[i].stats.sendcount = 0;
		p->pdd[i].stats.receivedcount = 0;
	}
}

void DELTA_Init()
{
	DELTA_AddDefinition( "clientdata_t", g_ClientDataDefinition, 56 );
	DELTA_AddDefinition( "weapon_data_t", g_WeaponDataDefinition, 22 );
	DELTA_AddDefinition( "usercmd_t", g_UsercmdDataDefinition, 16 );
	DELTA_AddDefinition( "entity_state_t", g_EntityDataDefinition, 87 );
	DELTA_AddDefinition( "entity_state_player_t", g_EntityDataDefinition, 87 );
	DELTA_AddDefinition( "custom_entity_state_t", g_EntityDataDefinition, 87 );
	DELTA_AddDefinition( "event_t", g_EventDataDefinition, 14 );

	DELTA_RegisterDescription( "clientdata_t" );
	DELTA_RegisterDescription( "entity_state_t" );
	DELTA_RegisterDescription( "entity_state_player_t" );
	DELTA_RegisterDescription( "custom_entity_state_t" );
	DELTA_RegisterDescription( "usercmd_t" );
	DELTA_RegisterDescription( "weapon_data_t" );
	DELTA_RegisterDescription( "event_t" );

	g_large_Time_Buffers = false;
}

void DELTA_UpdateDescriptions()
{
	g_pplayerdelta = *DELTA_LookupRegistration( "entity_state_player_t" );
	g_pentitydelta = *DELTA_LookupRegistration( "entity_state_t" );
	g_pcustomentitydelta = *DELTA_LookupRegistration( "custom_entity_state_t" );
	g_pclientdelta = *DELTA_LookupRegistration( "clientdata_t" );
	g_pweapondelta = *DELTA_LookupRegistration( "weapon_data_t" );
	g_peventdelta = *DELTA_LookupRegistration( "event_t" );
}

void DELTA_Shutdown()
{
	DELTA_ClearEncoders();
	DELTA_ClearDefinitions();
	DELTA_ClearRegistrations();
}

void DELTA_SetTime( double time )
{
	g_delta_Time = time;
}

void DELTA_SetLargeTimeBufferSize( bool bigBuffers )
{
	g_large_Time_Buffers = bigBuffers;
}
