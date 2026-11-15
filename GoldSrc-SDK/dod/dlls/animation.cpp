/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/nowin.h"

typedef int BOOL;
#define TRUE	 1	
#define FALSE	0

// hack into header files that we can ship
typedef int qboolean;
typedef unsigned char byte;
#include "../../utils/common/mathlib.h"
#include "const.h"
#include "progdefs.h"
#include "edict.h"
#include "eiface.h"

#include "studio.h"
#include "com_model.h"
#include "r_studioint.h"

#ifndef ACTIVITY_H
#include "activity.h"
#endif

#include "activitymap.h"

#ifndef ANIMATION_H
#include "animation.h"
#endif

#ifndef SCRIPTEVENT_H
#include "scriptevent.h"
#endif

#ifndef ENGINECALLBACK_H
#include "enginecallback.h"
#endif

extern globalvars_t				*gpGlobals;

studiohdr_t *g_pstudiohdr;
float (*g_pRotationMatrix)[3][4];
float (*g_pBoneTransform)[128][3][4];
server_studio_api_t IEngineStudio;
float cosom, omega, sinom, sclp, sclq;
static float zero[3];

extern float GetPlayerYaw( const edict_t *pEdict );
extern float GetPlayerPitchBlend( const edict_t *pEdict );
extern float GetPlayerGaitYaw( const edict_t *pEdict );
extern int GetPlayerGaitsequence( const edict_t *pEdict );
void SV_StudioSetupBones( model_t *pModel, float frame, int sequence,
	const float *angles, const float *origin, const byte *pcontroller,
	const byte *pblending, int iBone, const edict_t *pEdict );

sv_blending_interface_t svBlending = { SV_BLENDING_INTERFACE_VERSION, SV_StudioSetupBones };

#ifdef _WIN32
#define BLENDING_DLLEXPORT __declspec( dllexport )
#else
#define BLENDING_DLLEXPORT __attribute__ ((visibility("default")))
#endif

extern "C" BLENDING_DLLEXPORT int Server_GetBlendingInterface( int version,
	sv_blending_interface_t **ppinterface, server_studio_api_t *pstudio,
	float (*rotationmatrix)[3][4], float (*bonetransform)[128][3][4] )
{
	if ( version != SV_BLENDING_INTERFACE_VERSION )
		return 0;
	*ppinterface = &svBlending;
	IEngineStudio = *pstudio;
	g_pRotationMatrix = rotationmatrix;
	g_pBoneTransform = bonetransform;
	return 1;
}

void AngleQuaternion( float *angles, float *quaternion )
{
	float angle = (float)( angles[2] * 0.5 );
	float sy = (float)sin(angle), cy = (float)cos(angle);
	angle = (float)( angles[1] * 0.5 );
	float sp = (float)sin(angle), cp = (float)cos(angle);
	angle = (float)( angles[0] * 0.5 );
	float sr = (float)sin(angle), cr = (float)cos(angle);
	quaternion[0] = sr*cp*cy-cr*sp*sy;
	quaternion[1] = cr*sp*cy+sr*cp*sy;
	quaternion[2] = cr*cp*sy-sr*sp*cy;
	quaternion[3] = cr*cp*cy+sr*sp*sy;
}

void QuaternionSlerp( float *p, float *q, float t, float *qt )
{
	float a = 0, b = 0;
	int i;
	for ( i = 0; i < 4; ++i )
	{
		a += (p[i]-q[i])*(p[i]-q[i]);
		b += (p[i]+q[i])*(p[i]+q[i]);
	}
	if ( a > b )
	{
		for ( i = 0; i < 4; ++i )
			q[i] = -q[i];
	}
	cosom = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];
	if ( 1.0 + cosom > 0.000001 )
	{
		if ( 1.0 - cosom > 0.000001 )
		{
			omega = acosf(cosom);
			sinom = sinf(omega);
			sclp = (float)( sin((1.0-t)*omega)/sinom );
			sclq = (float)( sin(t*omega)/sinom );
		}
		else
		{
			sclp = (float)( 1.0-t );
			sclq = t;
		}
		for ( i = 0; i < 4; ++i )
			qt[i] = sclp*p[i] + sclq*q[i];
	}
	else
	{
		qt[0] = -q[1];
		qt[1] = q[0];
		qt[2] = -q[3];
		qt[3] = q[2];
		sclp = (float)sin((1.0-t)*(0.5*Q_PI));
		sclq = (float)sin(t*(0.5*Q_PI));
		for ( i = 0; i < 3; ++i )
			qt[i] = sclp*p[i] + sclq*qt[i];
	}
}

void QuaternionMatrix( float *quaternion, float (*matrix)[4] )
{
	matrix[0][0] = (float)( 1.0 - 2.0*quaternion[1]*quaternion[1] - 2.0*quaternion[2]*quaternion[2] );
	matrix[1][0] = (float)( 2.0*quaternion[0]*quaternion[1] + 2.0*quaternion[3]*quaternion[2] );
	matrix[2][0] = (float)( 2.0*quaternion[0]*quaternion[2] - 2.0*quaternion[3]*quaternion[1] );
	matrix[0][1] = (float)( 2.0*quaternion[0]*quaternion[1] - 2.0*quaternion[3]*quaternion[2] );
	matrix[1][1] = (float)( 1.0 - 2.0*quaternion[0]*quaternion[0] - 2.0*quaternion[2]*quaternion[2] );
	matrix[2][1] = (float)( 2.0*quaternion[1]*quaternion[2] + 2.0*quaternion[3]*quaternion[0] );
	matrix[0][2] = (float)( 2.0*quaternion[0]*quaternion[2] + 2.0*quaternion[3]*quaternion[1] );
	matrix[1][2] = (float)( 2.0*quaternion[1]*quaternion[2] - 2.0*quaternion[3]*quaternion[0] );
	matrix[2][2] = (float)( 1.0 - 2.0*quaternion[0]*quaternion[0] - 2.0*quaternion[1]*quaternion[1] );
}

void ConcatTransforms( float in1[3][4], float in2[3][4], float out[3][4] )
{
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
			out[i][j] = in1[i][0]*in2[0][j] + in1[i][1]*in2[1][j] + in1[i][2]*in2[2][j];
		out[i][3] = in1[i][0]*in2[0][3] + in1[i][1]*in2[1][3] + in1[i][2]*in2[2][3] + in1[i][3];
	}
}

mstudioanim_t *StudioGetAnim( model_t *pModel, mstudioseqdesc_t *pseqdesc )
{
	if ( pseqdesc->seqgroup == 0 )
		return (mstudioanim_t *)((byte *)g_pstudiohdr + pseqdesc->animindex);
	cache_user_t *pCache = (cache_user_t *)pModel->submodels;
	if ( pCache == NULL )
	{
		pCache = (cache_user_t *)IEngineStudio.Mem_Calloc(16, sizeof(cache_user_t));
		pModel->submodels = (dmodel_t *)pCache;
	}
	mstudioseqgroup_t *pseqgroup = (mstudioseqgroup_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqgroupindex);
	if ( IEngineStudio.Cache_Check(&pCache[pseqdesc->seqgroup]) == NULL )
		IEngineStudio.LoadCacheFile(pseqgroup[pseqdesc->seqgroup].name, &pCache[pseqdesc->seqgroup]);
	return (mstudioanim_t *)((byte *)pCache[pseqdesc->seqgroup].data + pseqdesc->animindex);
}

mstudioanim_t *LookupAnimation( studiohdr_t *pstudiohdr, model_t *pModel, mstudioseqdesc_t *pseqdesc, int index )
{
	mstudioanim_t *panim = StudioGetAnim(pModel, pseqdesc);
	if ( index >= 0 && index < pseqdesc->numblends )
		panim += pstudiohdr->numbones * index;
	return panim;
}

void StudioCalcBoneAdj( float dadt, float *adj, const byte *pcontroller1, const byte *pcontroller2, byte mouthopen )
{
	mstudiobonecontroller_t *pcontroller = (mstudiobonecontroller_t *)((byte *)g_pstudiohdr + g_pstudiohdr->bonecontrollerindex);
	for ( int j = 0; j < g_pstudiohdr->numbonecontrollers; ++j )
	{
		int i = pcontroller[j].index;
		float value;
		if ( i <= 3 )
		{
			if ( pcontroller[j].type & STUDIO_RLOOP )
			{
				if ( abs(pcontroller1[i]-pcontroller2[i]) > 128 )
				{
					int a = (pcontroller1[j]+128)%256;
					int b = (pcontroller2[j]+128)%256;
					value = (float)( (a*dadt + b*(1.0-dadt)-128)*(360.0/256.0) + pcontroller[j].start );
				}
				else
					value = (float)( (pcontroller1[i]*dadt + pcontroller2[i]*(1.0-dadt))*(360.0/256.0) + pcontroller[j].start );
			}
			else
			{
				value = (float)( (pcontroller1[i]*dadt + pcontroller2[i]*(1.0-dadt))/255.0 );
				if ( value < 0 ) value = 0;
				if ( value > 1 ) value = 1;
				value = (float)( (1.0-value)*pcontroller[j].start + value*pcontroller[j].end );
			}
		}
		else
		{
			value = (float)( mouthopen/64.0 );
			if ( value > 1 ) value = 1;
			value = (float)( (1.0-value)*pcontroller[j].start + value*pcontroller[j].end );
		}
		switch ( pcontroller[j].type & STUDIO_TYPES )
		{
		case STUDIO_XR:
		case STUDIO_YR:
		case STUDIO_ZR:
			adj[j] = (float)( value*(Q_PI/180.0) );
			break;
		case STUDIO_X:
		case STUDIO_Y:
		case STUDIO_Z:
			adj[j] = value;
			break;
		}
	}
}

void StudioCalcBoneQuaterion( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *q )
{
	float angle1[3], angle2[3], q1[4], q2[4];
	for ( int j = 0; j < 3; ++j )
	{
		if ( panim->offset[j+3] == 0 )
			angle2[j] = angle1[j] = pbone->value[j+3];
		else
		{
			mstudioanimvalue_t *pvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j+3]);
			int k = frame;
			if ( pvalue->num.total < pvalue->num.valid ) k = 0;
			while ( pvalue->num.total <= k )
			{
				k -= pvalue->num.total;
				pvalue += pvalue->num.valid+1;
				if ( pvalue->num.total < pvalue->num.valid ) k = 0;
			}
			if ( pvalue->num.valid > k )
			{
				angle1[j] = pvalue[k+1].value;
				if ( pvalue->num.valid > k+1 )
					angle2[j] = pvalue[k+2].value;
				else if ( pvalue->num.total > k+1 )
					angle2[j] = angle1[j];
				else
					angle2[j] = pvalue[pvalue->num.valid+2].value;
			}
			else
			{
				angle1[j] = pvalue[pvalue->num.valid].value;
				angle2[j] = pvalue->num.total > k+1 ? angle1[j] : pvalue[pvalue->num.valid+2].value;
			}
			angle1[j] = pbone->value[j+3] + angle1[j]*pbone->scale[j+3];
			angle2[j] = pbone->value[j+3] + angle2[j]*pbone->scale[j+3];
		}
		if ( pbone->bonecontroller[j+3] != -1 )
		{
			angle1[j] += adj[pbone->bonecontroller[j+3]];
			angle2[j] += adj[pbone->bonecontroller[j+3]];
		}
	}
	if ( VectorCompare(angle1, angle2) )
		AngleQuaternion(angle1, q);
	else
	{
		AngleQuaternion(angle1, q1);
		AngleQuaternion(angle2, q2);
		QuaternionSlerp(q1, q2, s, q);
	}
}

void StudioCalcBonePosition( int frame, float s, mstudiobone_t *pbone, mstudioanim_t *panim, float *adj, float *pos )
{
	for ( int j = 0; j < 3; ++j )
	{
		pos[j] = pbone->value[j];
		if ( panim->offset[j] != 0 )
		{
			mstudioanimvalue_t *pvalue = (mstudioanimvalue_t *)((byte *)panim + panim->offset[j]);
			int k = frame;
			if ( pvalue->num.total < pvalue->num.valid ) k = 0;
			while ( pvalue->num.total <= k )
			{
				k -= pvalue->num.total;
				pvalue += pvalue->num.valid+1;
				if ( pvalue->num.total < pvalue->num.valid ) k = 0;
			}
			if ( pvalue->num.valid > k )
			{
				if ( pvalue->num.valid > k+1 )
					pos[j] = (float)( pos[j] + (pvalue[k+1].value*(1.0-s) + s*pvalue[k+2].value)*pbone->scale[j] );
				else
					pos[j] += pvalue[k+1].value*pbone->scale[j];
			}
			else if ( pvalue->num.total <= k+1 )
				pos[j] = (float)( pos[j] + (pvalue[pvalue->num.valid].value*(1.0-s) + s*pvalue[pvalue->num.valid+2].value)*pbone->scale[j] );
			else
				pos[j] += pvalue[pvalue->num.valid].value*pbone->scale[j];
		}
		if ( pbone->bonecontroller[j] != -1 && adj != NULL )
			pos[j] += adj[pbone->bonecontroller[j]];
	}
}

void StudioSlerpBones( float q1[][4], float pos1[][3], float q2[][4], float pos2[][3], float s )
{
	if ( s < 0 ) s = 0;
	if ( s > 1 ) s = 1;
	float s1 = (float)( 1.0-s );
	for ( int i = 0; i < g_pstudiohdr->numbones; ++i )
	{
		float q3[4];
		QuaternionSlerp(q1[i], q2[i], s, q3);
		memcpy(q1[i], q3, sizeof(q3));
		for ( int j = 0; j < 3; ++j )
			pos1[i][j] = s1*pos1[i][j] + s*pos2[i][j];
	}
}

void StudioCalcRotations( mstudiobone_t *pbones, int *chain, int chainlength, float *adj,
	float pos[][3], float q[][4], mstudioseqdesc_t *, mstudioanim_t *panim, float f, float s )
{
	for ( int j = chainlength-1; j >= 0; --j )
	{
		int i = chain[j];
		StudioCalcBoneQuaterion((int)f, s, &pbones[i], &panim[i], adj, q[i]);
		StudioCalcBonePosition((int)f, s, &pbones[i], &panim[i], adj, pos[i]);
	}
}

void SV_StudioSetupBones( model_t *pModel, float frame, int sequence, const float *angles,
	const float *origin, const byte *pcontroller, const byte *pblending, int iBone, const edict_t *pEdict )
{
	static float pos[MAXSTUDIOBONES][3], pos2[MAXSTUDIOBONES][3];
	static float pos3[MAXSTUDIOBONES][3], pos4[MAXSTUDIOBONES][3];
	static float q[MAXSTUDIOBONES][4], q2[MAXSTUDIOBONES][4];
	static float q3[MAXSTUDIOBONES][4], q4[MAXSTUDIOBONES][4];
	g_pstudiohdr = (studiohdr_t *)IEngineStudio.Mod_Extradata(pModel);
	if ( sequence < 0 || sequence >= g_pstudiohdr->numseq ) sequence = 0;
	mstudioseqdesc_t *pseqdesc = (mstudioseqdesc_t *)((byte *)g_pstudiohdr + g_pstudiohdr->seqindex) + sequence;
	mstudiobone_t *pbones = (mstudiobone_t *)((byte *)g_pstudiohdr + g_pstudiohdr->boneindex);
	mstudioanim_t *panim = StudioGetAnim(pModel, pseqdesc);
	if ( iBone < -1 || iBone >= g_pstudiohdr->numbones ) iBone = 0;
	int chain[MAXSTUDIOBONES], chainlength = 0;
	if ( iBone == -1 )
	{
		chainlength = g_pstudiohdr->numbones;
		for ( int i = 0; i < chainlength; ++i ) chain[chainlength-i-1] = i;
	}
	else
	{
		do
		{
			chain[chainlength++] = iBone;
			iBone = pbones[iBone].parent;
		}
		while ( iBone != -1 );
	}
	float f = (float)( pseqdesc->numframes > 1 ? frame*(pseqdesc->numframes-1)/256.0 : 0 );
	float subframe = f-(int)f;
	float adj[MAXSTUDIOCONTROLLERS];
	StudioCalcBoneAdj(0, adj, pcontroller, pcontroller, 0);
	StudioCalcRotations(pbones, chain, chainlength, adj, pos, q, pseqdesc, panim, f, subframe);
	if ( pseqdesc->numblends == 9 )
	{
		float yaw = GetPlayerYaw(pEdict);
		float pitch = GetPlayerPitchBlend(pEdict);
		int blend = 0;
		if ( yaw > 127 )
		{
			blend = 1;
			yaw = (yaw-127)*2;
		}
		else yaw *= 2;
		if ( pitch > 127 )
		{
			blend += 3;
			pitch = (pitch-127)*2;
		}
		else pitch *= 2;
		StudioCalcRotations(pbones, chain, chainlength, adj, pos, q, pseqdesc,
			blend == 0 ? panim : LookupAnimation(g_pstudiohdr,pModel,pseqdesc,blend), f,subframe);
		StudioCalcRotations(pbones, chain, chainlength, adj, pos2,q2,pseqdesc,LookupAnimation(g_pstudiohdr,pModel,pseqdesc,blend+1),f,subframe);
		StudioCalcRotations(pbones, chain, chainlength, adj, pos3,q3,pseqdesc,LookupAnimation(g_pstudiohdr,pModel,pseqdesc,blend+3),f,subframe);
		StudioCalcRotations(pbones, chain, chainlength, adj, pos4,q4,pseqdesc,LookupAnimation(g_pstudiohdr,pModel,pseqdesc,blend+4),f,subframe);
		StudioSlerpBones(q,pos,q2,pos2,(float)( yaw/255.0 ));
		StudioSlerpBones(q3,pos3,q4,pos4,(float)( yaw/255.0 ));
		StudioSlerpBones(q,pos,q3,pos3,(float)( pitch/255.0 ));
	}
	else if ( pseqdesc->numblends > 1 )
	{
		StudioCalcRotations(pbones,chain,chainlength,adj,pos2,q2,pseqdesc,
			StudioGetAnim(pModel,pseqdesc)+g_pstudiohdr->numbones,f,subframe);
		StudioSlerpBones(q,pos,q2,pos2,(float)( pblending[0]/255.0 ));
	}
	if ( (pseqdesc->numblends == 9 && !(sequence >= 19 && sequence <= 24) && sequence != 7 && sequence != 8)
		|| (pseqdesc->numblends == 1 && sequence > 238 && sequence <= 286) )
	{
		int gaitsequence = GetPlayerGaitsequence(pEdict);
		if ( gaitsequence < 0 || gaitsequence >= g_pstudiohdr->numseq ) gaitsequence = 0;
		mstudioseqdesc_t *pgait = (mstudioseqdesc_t *)((byte *)g_pstudiohdr+g_pstudiohdr->seqindex)+gaitsequence;
		StudioCalcRotations(pbones,chain,chainlength,adj,pos2,q2,pgait,StudioGetAnim(pModel,pgait),0,0);
		BOOL copy = TRUE;
		for ( int i = 0; i < g_pstudiohdr->numbones; ++i )
		{
			if ( strcmp(pbones[i].name,"Bip01 Spine") == 0 )
				copy = FALSE;
			else if ( strcmp(pbones[pbones[i].parent].name,"Bip01 Pelvis") == 0 || copy )
			{
				memcpy(pos[i],pos2[i],sizeof(pos[i]));
				memcpy(q[i],q2[i],sizeof(q[i]));
				copy = TRUE;
			}
		}
	}
	float tempAngles[3] = { angles[0],angles[1],angles[2] };
	if ( pEdict != NULL && _stricmp("player",gpGlobals->pStringBase+pEdict->v.classname) == 0 )
	{
		tempAngles[0] = 0;
		tempAngles[1] = GetPlayerGaitYaw(pEdict);
	}
	AngleMatrix(tempAngles,*g_pRotationMatrix);
	for ( int axis = 0; axis < 3; ++axis ) (*g_pRotationMatrix)[axis][3] = origin[axis];
	for ( int j = chainlength-1; j >= 0; --j )
	{
		int i = chain[j];
		float bonematrix[3][4];
		QuaternionMatrix(q[i],bonematrix);
		for ( int axis = 0; axis < 3; ++axis ) bonematrix[axis][3] = pos[i][axis];
		if ( pbones[i].parent == -1 )
			ConcatTransforms(*g_pRotationMatrix,bonematrix,(*g_pBoneTransform)[i]);
		else
			ConcatTransforms((*g_pBoneTransform)[pbones[i].parent],bonematrix,(*g_pBoneTransform)[i]);
	}
}

void DrawLine( float *start, float *end, byte r, byte g, byte b )
{
	const int svcTempEntity = 23;
	MESSAGE_BEGIN(MSG_ALL,svcTempEntity,zero,NULL);
	WRITE_BYTE(TE_SHOWLINE);
	WRITE_COORD(start[0]); WRITE_COORD(start[1]); WRITE_COORD(start[2]);
	WRITE_COORD(end[0]); WRITE_COORD(end[1]); WRITE_COORD(end[2]);
	WRITE_SHORT(5);
	WRITE_BYTE(r); WRITE_BYTE(g); WRITE_BYTE(b);
	MESSAGE_END();
}

void DrawBox( float p[][3], byte r, byte g, byte b )
{
	DrawLine(p[0],p[1],r,g,b); DrawLine(p[0],p[4],r,g,b); DrawLine(p[0],p[2],r,g,b);
	DrawLine(p[7],p[3],r,g,b); DrawLine(p[7],p[5],r,g,b); DrawLine(p[7],p[6],r,g,b);
	DrawLine(p[7],p[3],r,g,b); DrawLine(p[7],p[5],r,g,b); DrawLine(p[7],p[6],r,g,b);
	DrawLine(p[6],p[4],r,g,b); DrawLine(p[6],p[2],r,g,b);
	DrawLine(p[5],p[1],r,g,b); DrawLine(p[5],p[4],r,g,b);
	DrawLine(p[3],p[1],r,g,b); DrawLine(p[3],p[2],r,g,b);
}

#pragma warning( disable : 4244 )



int ExtractBbox( void *pmodel, int sequence, float *mins, float *maxs )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);
	
	mins[0] = pseqdesc[ sequence ].bbmin[0];
	mins[1] = pseqdesc[ sequence ].bbmin[1];
	mins[2] = pseqdesc[ sequence ].bbmin[2];

	maxs[0] = pseqdesc[ sequence ].bbmax[0];
	maxs[1] = pseqdesc[ sequence ].bbmax[1];
	maxs[2] = pseqdesc[ sequence ].bbmax[2];

	return 1;
}


int LookupActivity( void *pmodel, entvars_t *, int activity )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	int weighttotal = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (pseqdesc[i].activity == activity)
		{
			weighttotal += pseqdesc[i].actweight;
			if (!weighttotal || RANDOM_LONG(0,weighttotal-1) < pseqdesc[i].actweight)
				seq = i;
		}
	}

	return seq;
}


int LookupActivityHeaviest( void *pmodel, entvars_t *, int activity )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr )
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	int weight = 0;
	int seq = ACTIVITY_NOT_AVAILABLE;
	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (pseqdesc[i].activity == activity)
		{
			if ( pseqdesc[i].actweight > weight )
			{
				weight = pseqdesc[i].actweight;
				seq = i;
			}
		}
	}

	return seq;
}

void GetEyePosition ( void *pmodel, float *vecEyePosition )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;

	if ( !pstudiohdr )
	{
		ALERT ( at_console, "GetEyePosition() Can't get pstudiohdr ptr!\n" );
		return;
	}

	VectorCopy ( pstudiohdr->eyeposition, vecEyePosition );
}

int LookupSequence( void *pmodel, const char *label )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	for (int i = 0; i < pstudiohdr->numseq; i++)
	{
		if (stricmp( pseqdesc[i].label, label ) == 0)
			return i;
	}

	return -1;
}


int IsSoundEvent( int eventNumber )
{
	if ( eventNumber == SCRIPT_EVENT_SOUND || eventNumber == SCRIPT_EVENT_SOUND_VOICE )
		return 1;
	return 0;
}


void SequencePrecache( void *pmodel, const char *pSequenceName )
{
	int index = LookupSequence( pmodel, pSequenceName );
	if ( index >= 0 )
	{
		studiohdr_t *pstudiohdr;
	
		pstudiohdr = (studiohdr_t *)pmodel;
		if ( !pstudiohdr || index >= pstudiohdr->numseq )
			return;

		mstudioseqdesc_t	*pseqdesc;
		mstudioevent_t		*pevent;

		pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + index;
		pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

		for (int i = 0; i < pseqdesc->numevents; i++)
		{
			// Don't send client-side events to the server AI
			if ( pevent[i].event >= EVENT_CLIENT )
				continue;

			// UNDONE: Add a callback to check to see if a sound is precached yet and don't allocate a copy
			// of it's name if it is.
			if ( IsSoundEvent( pevent[i].event ) )
			{
				if ( !strlen(pevent[i].options) )
				{
					ALERT( at_error, "Bad sound event %d in sequence %s :: %s (sound is \"%s\")\n", pevent[i].event, pstudiohdr->name, pSequenceName, pevent[i].options );
				}

				PRECACHE_SOUND( (char *)(gpGlobals->pStringBase + ALLOC_STRING(pevent[i].options) ) );
			}
		}
	}
}



void GetSequenceInfo( void *pmodel, entvars_t *pev, float *pflFrameRate, float *pflGroundSpeed )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return;

	mstudioseqdesc_t	*pseqdesc;

	if (pev->sequence >= pstudiohdr->numseq)
	{
		*pflFrameRate = 0.0;
		*pflGroundSpeed = 0.0;
		return;
	}

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;

	if (pseqdesc->numframes > 1)
	{
		*pflFrameRate = 256 * pseqdesc->fps / (pseqdesc->numframes - 1);
		*pflGroundSpeed = sqrt( pseqdesc->linearmovement[0]*pseqdesc->linearmovement[0]+ pseqdesc->linearmovement[1]*pseqdesc->linearmovement[1]+ pseqdesc->linearmovement[2]*pseqdesc->linearmovement[2] );
		*pflGroundSpeed = *pflGroundSpeed * pseqdesc->fps / (pseqdesc->numframes - 1);
	}
	else
	{
		*pflFrameRate = 256.0;
		*pflGroundSpeed = 0.0;
	}
}


int GetSequenceFlags( void *pmodel, entvars_t *pev )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr || pev->sequence >= pstudiohdr->numseq )
		return 0;

	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;

	return pseqdesc->flags;
}


int GetAnimationEvent( void *pmodel, entvars_t *pev, MonsterEvent_t *pMonsterEvent, float flStart, float flEnd, int index )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if ( !pstudiohdr || pev->sequence >= pstudiohdr->numseq || !pMonsterEvent )
		return 0;


	mstudioseqdesc_t	*pseqdesc;
	mstudioevent_t		*pevent;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;
	pevent = (mstudioevent_t *)((byte *)pstudiohdr + pseqdesc->eventindex);

	if (pseqdesc->numevents == 0 || index > pseqdesc->numevents )
		return 0;

	if (pseqdesc->numframes > 1)
	{
		flStart *= (pseqdesc->numframes - 1) / 256.0;
		flEnd *= (pseqdesc->numframes - 1) / 256.0;
	}
	else
	{
		flStart = 0;
		flEnd = 1.0;
	}

	for (; index < pseqdesc->numevents; index++)
	{
		// Don't send client-side events to the server AI
		if ( pevent[index].event >= EVENT_CLIENT )
			continue;

		if ( (pevent[index].frame >= flStart && pevent[index].frame < flEnd) || 
			((pseqdesc->flags & STUDIO_LOOPING) && flEnd >= pseqdesc->numframes - 1 && pevent[index].frame < flEnd - pseqdesc->numframes + 1) )
		{
			pMonsterEvent->event = pevent[index].event;
			pMonsterEvent->options = pevent[index].options;
			return index + 1;
		}
	}
	return 0;
}

float SetController( void *pmodel, entvars_t *pev, int iController, float flValue )
{
	studiohdr_t *pstudiohdr;
	int i;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return flValue;

	mstudiobonecontroller_t	*pbonecontroller = (mstudiobonecontroller_t *)((byte *)pstudiohdr + pstudiohdr->bonecontrollerindex);

	// find first controller that matches the index
	for ( i = 0; i < pstudiohdr->numbonecontrollers; i++, pbonecontroller++)
	{
		if (pbonecontroller->index == iController)
			break;
	}
	if (i >= pstudiohdr->numbonecontrollers)
		return flValue;

	// wrap 0..360 if it's a rotational controller

	if (pbonecontroller->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pbonecontroller->end < pbonecontroller->start)
			flValue = -flValue;

		// does the controller not wrap?
		if (pbonecontroller->start + 359.0 >= pbonecontroller->end)
		{
			if (flValue > ((pbonecontroller->start + pbonecontroller->end) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pbonecontroller->start + pbonecontroller->end) / 2.0) - 180)
				flValue = flValue + 360;
		}
		else
		{
			if (flValue > 360)
				flValue = flValue - (int)(flValue / 360.0) * 360.0;
			else if (flValue < 0)
				flValue = flValue + (int)((flValue / -360.0) + 1) * 360.0;
		}
	}

	int setting = 255 * (flValue - pbonecontroller->start) / (pbonecontroller->end - pbonecontroller->start);

	if (setting < 0) setting = 0;
	if (setting > 255) setting = 255;
	pev->controller[iController] = setting;

	return setting * (1.0 / 255.0) * (pbonecontroller->end - pbonecontroller->start) + pbonecontroller->start;
}


float SetBlending( void *pmodel, entvars_t *pev, int iBlender, float flValue )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return flValue;

	mstudioseqdesc_t	*pseqdesc;

	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex) + (int)pev->sequence;

	if (pseqdesc->blendtype[iBlender] == 0)
		return flValue;

	if (pseqdesc->blendtype[iBlender] & (STUDIO_XR | STUDIO_YR | STUDIO_ZR))
	{
		// ugly hack, invert value if end < start
		if (pseqdesc->blendend[iBlender] < pseqdesc->blendstart[iBlender])
			flValue = -flValue;

		// does the controller not wrap?
		if (pseqdesc->blendstart[iBlender] + 359.0 >= pseqdesc->blendend[iBlender])
		{
			if (flValue > ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) + 180)
				flValue = flValue - 360;
			if (flValue < ((pseqdesc->blendstart[iBlender] + pseqdesc->blendend[iBlender]) / 2.0) - 180)
				flValue = flValue + 360;
		}
	}

	int setting = 255 * (flValue - pseqdesc->blendstart[iBlender]) / (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]);

	if (setting < 0) setting = 0;
	if (setting > 255) setting = 255;

	pev->blending[iBlender] = setting;

	return setting * (1.0 / 255.0) * (pseqdesc->blendend[iBlender] - pseqdesc->blendstart[iBlender]) + pseqdesc->blendstart[iBlender];
}




int FindTransition( void *pmodel, int iEndingAnim, int iGoalAnim, int *piDir )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return iGoalAnim;

	mstudioseqdesc_t	*pseqdesc;
	pseqdesc = (mstudioseqdesc_t *)((byte *)pstudiohdr + pstudiohdr->seqindex);

	// bail if we're going to or from a node 0
	if (pseqdesc[iEndingAnim].entrynode == 0 || pseqdesc[iGoalAnim].entrynode == 0)
	{
		return iGoalAnim;
	}

	int	iEndNode;

	// ALERT( at_console, "from %d to %d: ", pEndNode->iEndNode, pGoalNode->iStartNode );

	if (*piDir > 0)
	{
		iEndNode = pseqdesc[iEndingAnim].exitnode;
	}
	else
	{
		iEndNode = pseqdesc[iEndingAnim].entrynode;
	}

	if (iEndNode == pseqdesc[iGoalAnim].entrynode)
	{
		*piDir = 1;
		return iGoalAnim;
	}

	byte *pTransition = ((byte *)pstudiohdr + pstudiohdr->transitionindex);

	int iInternNode = pTransition[(iEndNode-1)*pstudiohdr->numtransitions + (pseqdesc[iGoalAnim].entrynode-1)];

	if (iInternNode == 0)
		return iGoalAnim;

	int i;

	// look for someone going
	for (i = 0; i < pstudiohdr->numseq; i++)
	{
		if (pseqdesc[i].entrynode == iEndNode && pseqdesc[i].exitnode == iInternNode)
		{
			*piDir = 1;
			return i;
		}
		if (pseqdesc[i].nodeflags)
		{
			if (pseqdesc[i].exitnode == iEndNode && pseqdesc[i].entrynode == iInternNode)
			{
				*piDir = -1;
				return i;
			}
		}
	}

	ALERT( at_console, "error in transition graph" );
	return iGoalAnim;
}

void SetBodygroup( void *pmodel, entvars_t *pev, int iGroup, int iValue )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return;

	if (iGroup > pstudiohdr->numbodyparts)
		return;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (iValue >= pbodypart->nummodels)
		return;

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;

	pev->body = (pev->body - (iCurrent * pbodypart->base) + (iValue * pbodypart->base));
}


int GetBodygroup( void *pmodel, entvars_t *pev, int iGroup )
{
	studiohdr_t *pstudiohdr;
	
	pstudiohdr = (studiohdr_t *)pmodel;
	if (! pstudiohdr)
		return 0;

	if (iGroup > pstudiohdr->numbodyparts)
		return 0;

	mstudiobodyparts_t *pbodypart = (mstudiobodyparts_t *)((byte *)pstudiohdr + pstudiohdr->bodypartindex) + iGroup;

	if (pbodypart->nummodels <= 1)
		return 0;

	int iCurrent = (pev->body / pbodypart->base) % pbodypart->nummodels;

	return iCurrent;
}
