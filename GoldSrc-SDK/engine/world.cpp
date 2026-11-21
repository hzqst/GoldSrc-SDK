// world.c -- world query functions
#include "world.h"
#include "conprint.h"
#include "sys.h"
#include "server.h"
#include "common.h"
#include "eiface.h"

#include <cmath>

/*

entities never clip against themselves, or their owner

line of sight checks trace->crosscontent, but bullets don't

*/

#define AREA_DEPTH 4
#define AREA_NODES 32

typedef struct
{
	vec3_t boxmins, boxmaxs;   // enclose the test object along entire move
	const float *mins, *maxs;  // size of the moving object
	vec3_t mins2, maxs2;	   // size when clipping against mosnters
	const float *start, *end;
	trace_t trace;
	short type;
	short ignoretrans;
	edict_t* passedict;
	qboolean monsterClipBrush;
} moveclip_t;

hull_t* SV_HullForStudioModel( const edict_t* ent, const float* mins, const float* maxs, float* offset, int* numhulls );
int SV_HitgroupForStudioHull( int index );
qboolean SV_CheckSphereIntersection( edict_t* ent, const float* start, const float* end );
void SV_SetGlobalTrace( trace_t* trace );

void ClearLink( link_t* l )
{
	l->prev = l->next = l;
}

void RemoveLink( link_t* l )
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore( link_t* l, link_t* before )
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter( link_t* l, link_t* after )
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
===============================================================================

HULL BOXES

===============================================================================
*/

static hull_t box_hull, beam_hull;
static dclipnode_t box_clipnodes[6];
static mplane_t box_planes[6], beam_planes[6];

/*
===================
SV_InitBoxHull

Set up the planes and clipnodes so that the six floats of a bounding box
can just be stored out and get a proper hull_t structure.
===================
*/
void SV_InitBoxHull()
{
	int i;
	int side;

	box_hull.clipnodes = box_clipnodes;
	box_hull.planes = box_planes;
	box_hull.firstclipnode = 0;
	box_hull.lastclipnode = 5;

	beam_hull = box_hull;
	beam_hull.planes = beam_planes;

	for ( i = 0; i < 6; i++ )
	{
		box_clipnodes[i].planenum = i;

		side = i & 1;

		box_clipnodes[i].children[side] = CONTENTS_EMPTY;
		if ( i != 5 )
			box_clipnodes[i].children[side ^ 1] = (short)( i + 1 );
		else
			box_clipnodes[i].children[side ^ 1] = CONTENTS_SOLID;

		box_planes[i].type = (byte)( i >> 1 );
		box_planes[i].normal[i >> 1] = 1;

		beam_planes[i].type = PLANE_ANYZ;
	}
}

/*
===================
SV_HullForBox

To keep everything totally uniform, bounding boxes are turned into small
BSP trees instead of being compared directly.
===================
*/
hull_t* SV_HullForBox( const float* mins, const float* maxs )
{
	box_planes[0].dist = maxs[0];
	box_planes[1].dist = mins[0];
	box_planes[2].dist = maxs[1];
	box_planes[3].dist = mins[1];
	box_planes[4].dist = maxs[2];
	box_planes[5].dist = mins[2];

	return &box_hull;
}

hull_t* SV_HullForBeam( const float* start, const float* end, const float* size )
{
	vec3_t tmp;

	VectorSubtract( end, start, beam_planes[0].normal );
	VectorNormalize( beam_planes[0].normal );
	VectorCopy( beam_planes[0].normal, beam_planes[1].normal );

	beam_planes[0].dist = DotProduct( end, beam_planes[0].normal );
	beam_planes[1].dist = DotProduct( start, beam_planes[0].normal );

	if ( fabs( beam_planes[0].normal[2] ) < 0.9f )
	{
		tmp[0] = 0;
		tmp[1] = 0;
		tmp[2] = 1;
	}
	else
	{
		tmp[0] = 1;
		tmp[1] = 0;
		tmp[2] = 0;
	}

	CrossProduct( beam_planes[0].normal, tmp, beam_planes[2].normal );
	VectorNormalize( beam_planes[2].normal );
	VectorCopy( beam_planes[2].normal, beam_planes[3].normal );

	beam_planes[2].dist = ( start[0] + beam_planes[2].normal[0] ) * beam_planes[2].normal[0]
						+ ( start[1] + beam_planes[2].normal[1] ) * beam_planes[2].normal[1]
						+ ( start[2] + beam_planes[2].normal[2] ) * beam_planes[2].normal[2];

	VectorSubtract( start, beam_planes[2].normal, tmp );

	beam_planes[3].dist = DotProduct( tmp, beam_planes[2].normal );
	CrossProduct( beam_planes[2].normal, beam_planes[0].normal, beam_planes[4].normal );
	VectorNormalize( beam_planes[4].normal );

	VectorCopy( beam_planes[4].normal, beam_planes[5].normal );

	beam_planes[4].dist = ( start[0] + beam_planes[4].normal[0] ) * beam_planes[4].normal[0]
						+ ( start[1] + beam_planes[4].normal[1] ) * beam_planes[4].normal[1]
						+ ( start[2] + beam_planes[4].normal[2] ) * beam_planes[4].normal[2];
	beam_planes[5].dist = ( start[0] - beam_planes[4].normal[0] ) * beam_planes[4].normal[0]
						+ ( start[1] - beam_planes[4].normal[1] ) * beam_planes[4].normal[1]
						+ ( start[2] - beam_planes[4].normal[2] ) * beam_planes[4].normal[2];

	beam_planes[0].dist += fabs( beam_planes[0].normal[0] * size[0] ) + fabs( beam_planes[0].normal[1] * size[1] )
						 + fabs( beam_planes[0].normal[2] * size[2] );
	beam_planes[1].dist -= fabs( beam_planes[1].normal[0] * size[0] ) + fabs( beam_planes[1].normal[1] * size[1] )
						 + fabs( beam_planes[1].normal[2] * size[2] );
	beam_planes[2].dist += fabs( beam_planes[2].normal[0] * size[0] ) + fabs( beam_planes[2].normal[1] * size[1] )
						 + fabs( beam_planes[2].normal[2] * size[2] );
	beam_planes[3].dist -= fabs( beam_planes[3].normal[0] * size[0] ) + fabs( beam_planes[3].normal[1] * size[1] )
						 + fabs( beam_planes[3].normal[2] * size[2] );
	beam_planes[4].dist += fabs( beam_planes[4].normal[0] * size[0] ) + fabs( beam_planes[4].normal[1] * size[1] )
						 + fabs( beam_planes[4].normal[2] * size[2] );
	beam_planes[5].dist -= fabs( beam_planes[4].normal[0] * size[0] ) + fabs( beam_planes[4].normal[1] * size[1] )
						 + fabs( beam_planes[4].normal[2] * size[2] );

	return &beam_hull;
}

hull_t* SV_HullForBsp( edict_t* ent, const float* mins, const float* maxs, float* offset )
{
	model_t* model;
	hull_t* hull;
	vec3_t size;

	model = SV_ModelForIndex( ent->v.modelindex );
	if ( !model )
		Sys_Error( "Hit a %s with no model (%s)", &pr_strings[ent->v.classname], &pr_strings[ent->v.model] );
	if ( model->type != mod_brush )
		Sys_Error( "Hit a %s with no model (%s)", &pr_strings[ent->v.classname], &pr_strings[ent->v.model] );

	VectorSubtract( maxs, mins, size );

	if ( size[0] <= 8 )
	{
		hull = &model->hulls[0];
		VectorCopy( hull->clip_mins, offset );
	}
	else
	{
		if ( size[0] <= 36 )
		{
			if ( size[2] <= 36 )
				hull = &model->hulls[3];
			else
				hull = &model->hulls[1];
		}
		else
		{
			hull = &model->hulls[2];
		}

		// calculate an offset value to center the origin
		VectorSubtract( hull->clip_mins, mins, offset );
	}

	VectorAdd( offset, ent->v.origin, offset );

	return hull;
}

/*
================
SV_HullForEntity

Returns a hull that can be used for testing or clipping an object of mins/maxs
size.
Offset is filled in to contain the adjustment that must be added to the
testing object's origin to get a point to use with the returned hull.
================
*/
hull_t* SV_HullForEntity( edict_t* ent, const float* mins, const float* maxs, float* offset )
{
	vec3_t hullmins, hullmaxs;
	hull_t* hull;

	// decide which clipping hull to use, based on the size
	if ( ent->v.solid == SOLID_BSP )
	{
		// explicit hulls in the BSP model
		if ( ent->v.movetype != MOVETYPE_PUSH && ent->v.movetype != MOVETYPE_PUSHSTEP )
			Sys_Error( "SOLID_BSP without MOVETYPE_PUSH" );

		return SV_HullForBsp( ent, mins, maxs, offset );
	}

	// create a temp hull from bounding box sizes
	VectorSubtract( ent->v.mins, maxs, hullmins );
	VectorSubtract( ent->v.maxs, mins, hullmaxs );
	hull = SV_HullForBox( hullmins, hullmaxs );

	VectorCopy( ent->v.origin, offset );

	return hull;
}

/*
===============================================================================

ENTITY AREA CHECKING

===============================================================================
*/

areanode_t sv_areanodes[AREA_NODES];
int sv_numareanodes;

static int iTouchLinkSemaphore;

/*
===============
SV_CreateAreaNode

===============
*/
areanode_t* SV_CreateAreaNode( int depth, float* mins, float* maxs )
{
	areanode_t* anode;
	vec3_t size;
	vec3_t mins1, maxs1, mins2, maxs2;

	anode = &sv_areanodes[sv_numareanodes];
	sv_numareanodes++;

	ClearLink( &anode->trigger_edicts );
	ClearLink( &anode->solid_edicts );

	if ( depth == AREA_DEPTH )
	{
		anode->axis = -1;
		anode->children[0] = anode->children[1] = NULL;
		return anode;
	}

	VectorSubtract( maxs, mins, size );
	if ( size[0] > size[1] )
		anode->axis = 0;
	else
		anode->axis = 1;

	anode->dist = (float)( 0.5 * ( mins[anode->axis] + maxs[anode->axis] ) );
	VectorCopy( mins, mins1 );
	VectorCopy( mins, mins2 );
	VectorCopy( maxs, maxs1 );
	VectorCopy( maxs, maxs2 );

	maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

	anode->children[0] = SV_CreateAreaNode( depth + 1, mins2, maxs2 );
	anode->children[1] = SV_CreateAreaNode( depth + 1, mins1, maxs1 );

	return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld()
{
	SV_InitBoxHull();

	Q_memset( sv_areanodes, 0, sizeof( sv_areanodes ) );
	sv_numareanodes = 0;
	SV_CreateAreaNode( 0, sv.worldmodel->mins, sv.worldmodel->maxs );
}

/*
===============
SV_UnlinkEdict

===============
*/
void SV_UnlinkEdict( edict_t* ent )
{
	if ( !ent->area.prev )
		return; // not linked in anywhere

	RemoveLink( &ent->area );
	ent->area.prev = ent->area.next = NULL;
}

/*
====================
SV_TouchLinks
====================
*/
void SV_TouchLinks( edict_t* ent, areanode_t* node )
{
	link_t *l, *next;
	edict_t* touch;
	model_t* pModel;
	hull_t* hull;
	vec3_t offset, localPosition;
	int groups;

	// touch linked edicts
	for ( l = node->trigger_edicts.next; l != &node->trigger_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );
		if ( touch == ent )
			continue;

		if ( touch->v.groupinfo && ent->v.groupinfo )
		{
			groups = touch->v.groupinfo & ent->v.groupinfo;
			if ( ( g_groupop == 0 && !groups ) || ( g_groupop == 1 && groups ) )
				continue;
		}

		if ( touch->v.solid != SOLID_TRIGGER )
			continue;

		if ( ent->v.absmin[0] > touch->v.absmax[0] || ent->v.absmin[1] > touch->v.absmax[1]
			 || ent->v.absmin[2] > touch->v.absmax[2] || ent->v.absmax[0] < touch->v.absmin[0]
			 || ent->v.absmax[1] < touch->v.absmin[1] || ent->v.absmax[2] < touch->v.absmin[2] )
			continue;

		pModel = SV_ModelForIndex( touch->v.modelindex );
		if ( pModel && pModel->type == mod_brush )
		{
			hull = SV_HullForBsp( touch, ent->v.mins, ent->v.maxs, offset );

			VectorSubtract( ent->v.origin, offset, localPosition );

			if ( SV_HullPointContents( hull, hull->firstclipnode, localPosition ) != CONTENTS_SOLID )
				continue;
		}

		gGlobalVariables.time = (float)sv.time;
		gEntityInterface.pfnTouch( touch, ent );
	}

	// recurse down both sides
	if ( node->axis == -1 )
		return;

	if ( ent->v.absmax[node->axis] > node->dist )
		SV_TouchLinks( ent, node->children[0] );
	if ( ent->v.absmin[node->axis] < node->dist )
		SV_TouchLinks( ent, node->children[1] );
}

/*
===============
SV_FindTouchedLeafs

===============
*/
void SV_FindTouchedLeafs( edict_t* ent, mnode_t* node, int* topnode )
{
	mplane_t* splitplane;
	mleaf_t* leaf;
	int sides;

	if ( node->contents == CONTENTS_SOLID )
		return;

	// add an efrag if the node is a leaf
	if ( node->contents < 0 )
	{
		if ( ent->num_leafs < MAX_ENT_LEAFS )
		{
			leaf = (mleaf_t*)node;
			ent->leafnums[ent->num_leafs] = (short)( leaf - sv.worldmodel->leafs - 1 );
			ent->num_leafs++;
		}
		else
		{
			ent->num_leafs = MAX_ENT_LEAFS + 1;
		}
		return;
	}

	// NODE_MIXED
	splitplane = node->plane;
	if ( splitplane->type < 3 )
	{
		if ( splitplane->dist <= ent->v.absmin[splitplane->type] )
			sides = 1;
		else if ( splitplane->dist >= ent->v.absmax[splitplane->type] )
			sides = 2;
		else
			sides = 3;
	}
	else
	{
		sides = BoxOnPlaneSide( ent->v.absmin, ent->v.absmax, splitplane );
	}

	if ( sides == 3 && *topnode == -1 )
		*topnode = (int)( node - sv.worldmodel->nodes );

	// recurse down the contacted sides
	if ( sides & 1 )
		SV_FindTouchedLeafs( ent, node->children[0], topnode );

	if ( sides & 2 )
		SV_FindTouchedLeafs( ent, node->children[1], topnode );
}

/*
===============
SV_LinkEdict

===============
*/
void SV_LinkEdict( edict_t* ent, qboolean touch_triggers )
{
	areanode_t* node;
	int topnode;

	SV_UnlinkEdict( ent ); // unlink from old position

	if ( ent == sv.edicts )
		return; // don't add the world

	if ( ent->free )
		return;

	// set the abs box
	gEntityInterface.pfnSetAbsBox( ent );

	if ( ent->v.movetype == MOVETYPE_FOLLOW && ent->v.aiment )
	{
		ent->headnode = ent->v.aiment->headnode;
		ent->num_leafs = ent->v.aiment->num_leafs;
		Q_memcpy( ent->leafnums, ent->v.aiment->leafnums, sizeof( ent->leafnums ) );
	}
	else
	{
		topnode = -1;

		// link to PVS leafs
		ent->num_leafs = 0;
		ent->headnode = -1;
		if ( ent->v.modelindex )
		{
			SV_FindTouchedLeafs( ent, sv.worldmodel->nodes, &topnode );
			if ( ent->num_leafs > MAX_ENT_LEAFS )
			{
				ent->headnode = topnode;
				ent->num_leafs = 0;
				Q_memset( ent->leafnums, -1, sizeof( ent->leafnums ) );
			}
		}
	}

	if ( ent->v.solid == SOLID_NOT && ent->v.skin >= CONTENTS_EMPTY )
		return;

	if ( ent->v.solid == SOLID_BSP && !SV_ModelForIndex( ent->v.modelindex ) && !Q_strlen( &pr_strings[ent->v.model] ) )
	{
		Con_DPrintf( "Inserted %s with no model\n", &pr_strings[ent->v.classname] );
		return;
	}

	// find the first node that the ent's box crosses
	node = sv_areanodes;
	while ( node->axis != -1 )
	{
		if ( ent->v.absmin[node->axis] > node->dist )
			node = node->children[0];
		else if ( ent->v.absmax[node->axis] < node->dist )
			node = node->children[1];
		else
			break; // crosses the node
	}

	// link it in
	if ( ent->v.solid == SOLID_TRIGGER )
		InsertLinkBefore( &ent->area, &node->trigger_edicts );
	else
		InsertLinkBefore( &ent->area, &node->solid_edicts );

	// if touch_triggers, touch all entities at this node and decend for more
	if ( touch_triggers && !iTouchLinkSemaphore )
	{
		iTouchLinkSemaphore = 1;
		SV_TouchLinks( ent, sv_areanodes );
		iTouchLinkSemaphore = 0;
	}
}

/*
===============================================================================

POINT TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_HullPointContents

==================
*/
int SV_HullPointContents( hull_t* hull, int num, const float* p )
{
	float d;
	dclipnode_t* node;
	mplane_t* plane;

	while ( num >= 0 )
	{
		if ( num < hull->firstclipnode || num > hull->lastclipnode )
			Sys_Error( "SV_HullPointContents: bad node number" );

		node = &hull->clipnodes[num];
		plane = &hull->planes[node->planenum];

		if ( plane->type < 3 )
			d = p[plane->type] - plane->dist;
		else
			d = DotProduct( plane->normal, p ) - plane->dist;

		if ( d >= 0 )
			num = node->children[0];
		else
			num = node->children[1];
	}

	return num;
}

int SV_LinkContents( areanode_t* node, const float* pos )
{
	link_t *l, *next;
	edict_t* touch;
	int contents;
	int groups;
	hull_t* hull;
	vec3_t offset, localPosition;
	model_t* pModel;

	for ( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );

		if ( touch->v.solid != SOLID_NOT )
			continue;

		if ( touch->v.groupinfo )
		{
			groups = touch->v.groupinfo & g_groupmask;
			if ( ( g_groupop == 0 && !groups ) || ( g_groupop == 1 && groups ) )
				continue;
		}

		pModel = SV_ModelForIndex( touch->v.modelindex );
		if ( !pModel || pModel->type != mod_brush )
			continue;

		if ( pos[0] > touch->v.absmax[0] || pos[1] > touch->v.absmax[1] || pos[2] > touch->v.absmax[2]
			 || pos[0] < touch->v.absmin[0] || pos[1] < touch->v.absmin[1] || pos[2] < touch->v.absmin[2] )
			continue;

		contents = touch->v.skin;
		if ( contents < -100 || contents > 100 )
			Con_DPrintf( "Invalid contents on trigger field: %s\n", &pr_strings[touch->v.classname] );

		hull = SV_HullForBsp( touch, vec3_origin, vec3_origin, offset );

		VectorSubtract( pos, offset, localPosition );

		if ( SV_HullPointContents( hull, hull->firstclipnode, localPosition ) != CONTENTS_EMPTY )
			return contents;
	}

	if ( node->axis == -1 )
		return CONTENTS_EMPTY;

	if ( pos[node->axis] > node->dist )
		return SV_LinkContents( node->children[0], pos );
	if ( pos[node->axis] < node->dist )
		return SV_LinkContents( node->children[1], pos );

	return CONTENTS_EMPTY;
}

int SV_PointContents( const float* p )
{
	int cont, entityContents;

	cont = SV_HullPointContents( sv.worldmodel->hulls, 0, p );
	if ( cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN )
		cont = CONTENTS_WATER;

	if ( cont != CONTENTS_SOLID )
	{
		entityContents = SV_LinkContents( sv_areanodes, p );
		if ( entityContents != CONTENTS_EMPTY )
			return entityContents;
	}

	return cont;
}

//===========================================================================

/*
============
SV_TestEntityPosition

A small wrapper around SV_BoxInSolidEntity that never clips against the
supplied entity.
============
*/
edict_t* SV_TestEntityPosition( edict_t* ent )
{
	trace_t trace;
	qboolean monsterClip;

	monsterClip = ( ent->v.flags & FL_MONSTERCLIP ) ? TRUE : FALSE;

	trace = SV_Move( ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NORMAL, ent, monsterClip );

	if ( trace.startsolid )
	{
		SV_SetGlobalTrace( &trace );
		return trace.ent;
	}

	return NULL;
}

/*
===============================================================================

LINE TESTING IN HULLS

===============================================================================
*/

/*
==================
SV_RecursiveHullCheck

==================
*/
qboolean SV_RecursiveHullCheck( hull_t* hull, int num, float p1f, float p2f, float* p1, float* p2, trace_t* trace )
{
	dclipnode_t* node;
	mplane_t* plane;
	float t1, t2;
	float frac;
	int i;
	vec3_t mid;
	int side;
	float midf;

	// check for empty
	if ( num < 0 )
	{
		if ( num != CONTENTS_SOLID )
		{
			trace->allsolid = FALSE;
			if ( num == CONTENTS_EMPTY )
				trace->inopen = TRUE;
			else if ( num != CONTENTS_TRANSLUCENT )
				trace->inwater = TRUE;
		}
		else
		{
			trace->startsolid = TRUE;
		}

		return TRUE; // empty
	}

	if ( num < hull->firstclipnode || num > hull->lastclipnode || !hull->planes )
		Sys_Error( "SV_RecursiveHullCheck: bad node number" );

	// find the point distances
	node = &hull->clipnodes[num];
	plane = &hull->planes[node->planenum];

	if ( plane->type < 3 )
	{
		t1 = p1[plane->type] - plane->dist;
		t2 = p2[plane->type] - plane->dist;
	}
	else
	{
		t1 = DotProduct( plane->normal, p1 ) - plane->dist;
		t2 = DotProduct( plane->normal, p2 ) - plane->dist;
	}

	if ( t1 >= 0 && t2 >= 0 )
		return SV_RecursiveHullCheck( hull, node->children[0], p1f, p2f, p1, p2, trace );
	if ( t1 < 0 && t2 < 0 )
		return SV_RecursiveHullCheck( hull, node->children[1], p1f, p2f, p1, p2, trace );

	// put the crosspoint DIST_EPSILON pixels on the near side
	if ( t1 < 0 )
		frac = (float)( ( t1 + DIST_EPSILON ) / ( t1 - t2 ) );
	else
		frac = (float)( ( t1 - DIST_EPSILON ) / ( t1 - t2 ) );
	if ( frac < 0 )
		frac = 0;
	if ( frac > 1 )
		frac = 1;
	if ( !std::isfinite( frac ) )
		return FALSE;

	midf = p1f + ( p2f - p1f ) * frac;
	for ( i = 0; i < 3; i++ )
		mid[i] = p1[i] + frac * ( p2[i] - p1[i] );

	side = ( t1 < 0 );

	// move up to the node
	if ( !SV_RecursiveHullCheck( hull, node->children[side], p1f, midf, p1, mid, trace ) )
		return FALSE;

	if ( SV_HullPointContents( hull, node->children[side ^ 1], mid ) != CONTENTS_SOLID )
	{
		// go past the node
		return SV_RecursiveHullCheck( hull, node->children[side ^ 1], midf, p2f, mid, p2, trace );
	}

	if ( trace->allsolid )
		return FALSE; // never got out of the solid area

//==================
	// the other side of the node is solid, this is the impact point
//==================
	if ( !side )
	{
		VectorCopy( plane->normal, trace->plane.normal );
		trace->plane.dist = plane->dist;
	}
	else
	{
		VectorSubtract( vec3_origin, plane->normal, trace->plane.normal );
		trace->plane.dist = -plane->dist;
	}

	while ( SV_HullPointContents( hull, hull->firstclipnode, mid ) == CONTENTS_SOLID )
	{
		// shouldn't really happen, but does occasionally
		frac = (float)( frac - 0.1 );
		if ( frac < 0 )
		{
			trace->fraction = midf;
			VectorCopy( mid, trace->endpos );
			Con_DPrintf( "backup past 0\n" );
			return FALSE;
		}
		midf = p1f + ( p2f - p1f ) * frac;
		for ( i = 0; i < 3; i++ )
			mid[i] = p1[i] + frac * ( p2[i] - p1[i] );
	}

	trace->fraction = midf;
	VectorCopy( mid, trace->endpos );

	return FALSE;
}

void SV_SingleClipMoveToEntity(
	edict_t* ent, const float* start, const float* mins, const float* maxs, const float* end, trace_t* trace )
{
	vec3_t offset;
	vec3_t start_l, end_l;
	vec3_t forward, right, up;
	vec3_t temp;
	hull_t* hull;
	model_t* pmodel;
	int numhulls, i, closest;
	qboolean rotated;
	trace_t testtrace;

	if ( !trace || !ent )
		return;

	// fill in a default trace
	Q_memset( trace, 0, sizeof( trace_t ) );
	trace->fraction = 1;
	trace->allsolid = TRUE;
	VectorCopy( end, trace->endpos );

	pmodel = SV_ModelForIndex( ent->v.modelindex );

	if ( pmodel && pmodel->type == mod_studio )
	{
		hull = SV_HullForStudioModel( ent, mins, maxs, offset, &numhulls );
	}
	else
	{
		// get the clipping hull
		hull = SV_HullForEntity( ent, mins, maxs, offset );
		numhulls = 1;
	}

	VectorSubtract( start, offset, start_l );
	VectorSubtract( end, offset, end_l );

	// rotate start and end into the models frame of reference
	if ( ent->v.solid == SOLID_BSP && ( ent->v.angles[0] != 0 || ent->v.angles[1] != 0 || ent->v.angles[2] != 0 ) )
		rotated = TRUE;
	else
		rotated = FALSE;

	if ( rotated )
	{
		AngleVectors( ent->v.angles, forward, right, up );

		VectorCopy( start_l, temp );
		start_l[0] = DotProduct( temp, forward );
		start_l[1] = -DotProduct( temp, right );
		start_l[2] = DotProduct( temp, up );

		VectorCopy( end_l, temp );
		end_l[0] = DotProduct( temp, forward );
		end_l[1] = -DotProduct( temp, right );
		end_l[2] = DotProduct( temp, up );
	}

	// trace a line through the apropriate clipping hull
	if ( numhulls == 1 )
	{
		SV_RecursiveHullCheck( hull, hull->firstclipnode, 0, 1, start_l, end_l, trace );
	}
	else
	{
		closest = 0;

		for ( i = 0; i < numhulls; i++ )
		{
			Q_memset( &testtrace, 0, sizeof( testtrace ) );
			testtrace.fraction = 1;
			testtrace.allsolid = TRUE;
			VectorCopy( end, testtrace.endpos );

			SV_RecursiveHullCheck( &hull[i], hull[i].firstclipnode, 0, 1, start_l, end_l, &testtrace );

			if ( i == 0 || testtrace.allsolid || testtrace.startsolid || testtrace.fraction < trace->fraction )
			{
				if ( trace->startsolid )
				{
					*trace = testtrace;
					trace->startsolid = TRUE;
				}
				else
				{
					*trace = testtrace;
				}

				closest = i;
			}
		}

		trace->hitgroup = SV_HitgroupForStudioHull( closest );
	}

	// fix trace up by the offset
	if ( trace->fraction != 1 )
	{
		if ( rotated )
		{
			AngleVectorsTranspose( ent->v.angles, forward, right, up );

			VectorCopy( trace->plane.normal, temp );
			trace->plane.normal[0] = DotProduct( temp, forward );
			trace->plane.normal[1] = DotProduct( temp, right );
			trace->plane.normal[2] = DotProduct( temp, up );
		}

		for ( i = 0; i < 3; i++ )
			trace->endpos[i] = start[i] + trace->fraction * ( end[i] - start[i] );
	}

	// did we clip the move?
	if ( trace->fraction < 1 || trace->startsolid )
		trace->ent = ent;
}

/*
==================
SV_ClipMoveToEntity

Handles selection or creation of a clipping hull, and offseting (and
eventually rotation) of the end points
==================
*/
trace_t SV_ClipMoveToEntity( edict_t* ent, const float* start, const float* mins, const float* maxs, const float* end )
{
	trace_t goodtrace;

	SV_SingleClipMoveToEntity( ent, start, mins, maxs, end, &goodtrace );
	return goodtrace;
}

void SV_ClipToWorldbrush( areanode_t* node, moveclip_t* clip )
{
	link_t *l, *next;
	edict_t* touch;
	trace_t trace;

	// touch linked edicts
	for ( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );
		if ( touch->v.solid != SOLID_BSP )
			continue;
		if ( touch == clip->passedict )
			continue;
		if ( !( touch->v.flags & FL_WORLDBRUSH ) )
			continue;

		if ( clip->boxmins[0] > touch->v.absmax[0] || clip->boxmins[1] > touch->v.absmax[1]
			 || clip->boxmins[2] > touch->v.absmax[2] || clip->boxmaxs[0] < touch->v.absmin[0]
			 || clip->boxmaxs[1] < touch->v.absmin[1] || clip->boxmaxs[2] < touch->v.absmin[2] )
			continue;

		// might intersect, so do an exact clip
		if ( clip->trace.allsolid )
			return;

		trace = SV_ClipMoveToEntity( touch, clip->start, clip->mins, clip->maxs, clip->end );
		if ( trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction )
		{
			trace.ent = touch;
			if ( clip->trace.startsolid )
			{
				clip->trace = trace;
				clip->trace.startsolid = TRUE;
			}
			else
			{
				clip->trace = trace;
			}
		}
	}

	// recurse down both sides
	if ( node->axis == -1 )
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToWorldbrush( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		SV_ClipToWorldbrush( node->children[1], clip );
}

/*
====================
SV_ClipToLinks

Mins and maxs enclose the entire area swept by the move
====================
*/
//===========================================================================
void SV_ClipToLinks( areanode_t* node, moveclip_t* clip )
{
	link_t *l, *next;
	edict_t* touch;
	trace_t trace;
	int groups;

	// touch linked edicts
	for ( l = node->solid_edicts.next; l != &node->solid_edicts; l = next )
	{
		next = l->next;
		touch = EDICT_FROM_AREA( l );

		if ( touch->v.groupinfo && clip->passedict && clip->passedict->v.groupinfo )
		{
			groups = touch->v.groupinfo & clip->passedict->v.groupinfo;
			if ( ( g_groupop == 0 && !groups ) || ( g_groupop == 1 && groups ) )
				continue;
		}

		if ( touch->v.solid == SOLID_NOT )
			continue;
		if ( touch == clip->passedict )
			continue;
		if ( touch->v.solid == SOLID_TRIGGER )
			Sys_Error( "Trigger in clipping list" );

		if ( gNewDLLFunctions.pfnShouldCollide && !gNewDLLFunctions.pfnShouldCollide( touch, clip->passedict ) )
			continue;

		if ( touch->v.solid == SOLID_BSP )
		{
			if ( ( touch->v.flags & FL_MONSTERCLIP ) && !clip->monsterClipBrush )
				continue;
		}
		else
		{
			if ( clip->type == MOVE_NOMONSTERS && touch->v.movetype != MOVETYPE_PUSHSTEP )
				continue;
		}

		if ( clip->ignoretrans && touch->v.rendermode != kRenderNormal && !( touch->v.flags & FL_WORLDBRUSH ) )
			continue;

		if ( clip->boxmins[0] > touch->v.absmax[0] || clip->boxmins[1] > touch->v.absmax[1]
			 || clip->boxmins[2] > touch->v.absmax[2] || clip->boxmaxs[0] < touch->v.absmin[0]
			 || clip->boxmaxs[1] < touch->v.absmin[1] || clip->boxmaxs[2] < touch->v.absmin[2] )
			continue;

		if ( touch->v.solid != SOLID_SLIDEBOX && !SV_CheckSphereIntersection( touch, clip->start, clip->end ) )
			continue;

		if ( clip->passedict && clip->passedict->v.size[0] && !touch->v.size[0] )
			continue; // points never interact

		// might intersect, so do an exact clip
		if ( clip->trace.allsolid )
			return;

		if ( clip->passedict )
		{
			if ( touch->v.owner == clip->passedict )
				continue; // don't clip against own missiles
			if ( clip->passedict->v.owner == touch )
				continue; // don't clip against owner
		}

		if ( touch->v.flags & FL_MONSTER )
			trace = SV_ClipMoveToEntity( touch, clip->start, clip->mins2, clip->maxs2, clip->end );
		else
			trace = SV_ClipMoveToEntity( touch, clip->start, clip->mins, clip->maxs, clip->end );

		if ( trace.allsolid || trace.startsolid || trace.fraction < clip->trace.fraction )
		{
			trace.ent = touch;
			if ( clip->trace.startsolid )
			{
				clip->trace = trace;
				clip->trace.startsolid = TRUE;
			}
			else
			{
				clip->trace = trace;
			}
		}
	}

	// recurse down both sides
	if ( node->axis == -1 )
		return;

	if ( clip->boxmaxs[node->axis] > node->dist )
		SV_ClipToLinks( node->children[0], clip );
	if ( clip->boxmins[node->axis] < node->dist )
		SV_ClipToLinks( node->children[1], clip );
}

/*
==================
SV_MoveBounds
==================
*/
void SV_MoveBounds(
	const float* start, const float* mins, const float* maxs, const float* end, float* boxmins, float* boxmaxs )
{
	int i;

	for ( i = 0; i < 3; i++ )
	{
		if ( end[i] > start[i] )
		{
			boxmins[i] = start[i] + mins[i] - 1;
			boxmaxs[i] = end[i] + maxs[i] + 1;
		}
		else
		{
			boxmins[i] = end[i] + mins[i] - 1;
			boxmaxs[i] = start[i] + maxs[i] + 1;
		}
	}
}

trace_t SV_MoveNoEnts( const float* start, float* mins, float* maxs, const float* end, int type, edict_t* passedict )
{
	moveclip_t clip;
	vec3_t worldEndPoint;
	float worldFraction;

	Q_memset( &clip, 0, sizeof( clip ) );

	// clip to world
	clip.trace = SV_ClipMoveToEntity( sv.edicts, start, mins, maxs, end );

	if ( clip.trace.fraction != 0 )
	{
		VectorCopy( clip.trace.endpos, worldEndPoint );
		worldFraction = clip.trace.fraction;

		clip.trace.fraction = 1;
		clip.start = start;
		clip.end = worldEndPoint;
		clip.mins = mins;
		clip.maxs = maxs;
		clip.type = (short)( type & 0xFF );
		clip.ignoretrans = (short)( type >> 8 );
		clip.passedict = passedict;
		clip.monsterClipBrush = FALSE;
		VectorCopy( mins, clip.mins2 );
		VectorCopy( maxs, clip.maxs2 );

		// create the bounding box of the entire move
		SV_MoveBounds( start, clip.mins2, clip.maxs2, worldEndPoint, clip.boxmins, clip.boxmaxs );

		// clip to entities
		SV_ClipToWorldbrush( sv_areanodes, &clip );

		gGlobalVariables.trace_ent = clip.trace.ent;
		clip.trace.fraction *= worldFraction;
	}

	return clip.trace;
}

/*
==================
SV_Move
==================
*/
trace_t SV_Move( const float* start, const float* mins, const float* maxs, const float* end, int type,
	edict_t* passedict, qboolean monsterClipBrush )
{
	moveclip_t clip;
	int i;
	vec3_t worldEndPoint;
	float worldFraction;

	Q_memset( &clip, 0, sizeof( clip ) );

	// clip to world
	clip.trace = SV_ClipMoveToEntity( sv.edicts, start, mins, maxs, end );

	if ( clip.trace.fraction != 0 )
	{
		VectorCopy( clip.trace.endpos, worldEndPoint );
		worldFraction = clip.trace.fraction;

		clip.trace.fraction = 1;
		clip.start = start;
		clip.end = worldEndPoint;
		clip.mins = mins;
		clip.maxs = maxs;
		clip.type = (short)( type & 0xFF );
		clip.ignoretrans = (short)( type >> 8 );
		clip.passedict = passedict;
		clip.monsterClipBrush = monsterClipBrush;

		if ( type == MOVE_MISSILE )
		{
			for ( i = 0; i < 3; i++ )
			{
				clip.mins2[i] = -15;
				clip.maxs2[i] = 15;
			}
		}
		else
		{
			VectorCopy( mins, clip.mins2 );
			VectorCopy( maxs, clip.maxs2 );
		}

		// create the bounding box of the entire move
		SV_MoveBounds( start, clip.mins2, clip.maxs2, worldEndPoint, clip.boxmins, clip.boxmaxs );

		// clip to entities
		SV_ClipToLinks( sv_areanodes, &clip );

		gGlobalVariables.trace_ent = clip.trace.ent;
		clip.trace.fraction *= worldFraction;
	}

	return clip.trace;
}
