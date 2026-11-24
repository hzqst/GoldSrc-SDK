#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "cs_bot.h"
#include "bot_profile.h"
#include <algorithm>

#include "buildslave_assert.h"
#define ASSERT_FILE BUILDSLAVE_PATH( L"cstrike\\dlls\\bot\\cs_bot_pathfind.cpp" )

float PathCost::operator()( CNavArea* area, CNavArea* fromArea, const CNavLadder* ladder )
{
	if ( fromArea == NULL && m_route == FASTEST_ROUTE )
		return 0.0f;
	const float aggression = m_bot->GetProfile()->GetAggression();
	const float dangerPenalty = (1.0f - aggression * 0.95f) * 100.0f;
	if ( fromArea == NULL )
		return area->GetDanger( m_bot->m_iTeam - 1 ) * dangerPenalty;

	float distance = ladder ? ladder->m_length : (*area->GetCenter() - *fromArea->GetCenter()).Length();
	float cost = fromArea->GetCostSoFar() + distance;
	if ( cv_bot_zombie.value > 0.0f )
		return cost;

	if ( !area->IsConnected( fromArea, NUM_DIRECTIONS ) )
	{
		float damage = -fromArea->ComputeHeightChange( area ) * 0.2178f - 26.0f;
		if ( damage > 0.0f )
		{
			if ( damage + 10.0f >= m_bot->pev->health )
				return -1.0f;
			if ( m_route != FASTEST_ROUTE || damage > 15.0f * aggression + 10.0f )
				cost += damage * (100.0f * damage);
		}
	}
	if ( area->GetAttributes() & NAV_CROUCH )
	{
		float penalty = m_route == FASTEST_ROUTE ? 20.0f : 5.0f;
		if ( m_bot->GetHostageEscortCount() )
			penalty *= 3.0f;
		cost += penalty * distance;
	}
	if ( area->GetAttributes() & NAV_JUMP )
		cost += distance;
	if ( m_route == SAFEST_ROUTE )
		cost += area->GetDanger( m_bot->m_iTeam - 1 ) * (distance * dangerPenalty);
	if ( !m_bot->IsAttacking() )
	{
		float size = (area->GetSizeX() + area->GetSizeY()) * 0.5f;
		if ( size >= 1.0f )
			cost += area->GetPlayerCount( m_bot->m_iTeam, m_bot ) * 50000.0f / size;
	}
	return cost;
}

bool CCSBot::ComputePath( CNavArea* goalArea, const Vector* goalPosition, RouteType route )
{
	if ( !m_repathTimer.IsElapsed() )
		return false;
	m_repathTimer.Start( RANDOM_FLOAT( 0.4f, 0.6f ) );
	m_pathLength = 0;
	m_ladder = NULL;
	if ( m_lastKnownArea == NULL || (goalArea == NULL && goalPosition == NULL) )
		return false;

	Vector goal = goalPosition ? *goalPosition : *goalArea->GetCenter();
	if ( goalArea )
	{
		goal.z = goalArea->GetZ( &goal );
		if ( m_lastKnownArea == goalArea )
		{
			BuildTrivialPath( &goal );
			return true;
		}
	}
	else
		GetGroundHeight( &goal, &goal.z, NULL );

	CNavArea* closestArea;
	PathCost cost( this, route );
	if ( !NavAreaBuildPath( m_lastKnownArea, goalArea, goalPosition, cost, &closestArea ) )
	{
		goalArea = closestArea;
		if ( goalArea == NULL )
			return false;
	}
	else if ( goalArea == NULL )
		return false;

	int count = 0;
	for ( CNavArea* area = goalArea; area; area = area->GetParent() )
		++count;
	if ( count > 255 )
		count = 255;
	else if ( count == 1 )
	{
		BuildTrivialPath( &goal );
		return true;
	}
	m_pathLength = count;
	for ( CNavArea* area = goalArea; count && area; area = area->GetParent() )
	{
		--count;
		m_path[count].area = area;
		m_path[count].how = area->GetParentHow();
	}
	if ( !ComputePathPositions() )
	{
		PrintIfWatched( "Error building path\n" );
		m_pathLength = 0;
		m_ladder = NULL;
		return false;
	}
	if ( goalPosition == NULL )
	{
		const PathSegment& last = m_path[m_pathLength - 1];
		switch ( last.how )
		{
		case GO_NORTH:
		case GO_SOUTH:
			goal.x = last.position.x;
			goal.y = goalArea->GetCenter()->y;
			break;
		case GO_EAST:
		case GO_WEST:
			goal.x = goalArea->GetCenter()->x;
			goal.y = last.position.y;
			break;
		default:
			break;
		}
		GetGroundHeight( &goal, &goal.z, NULL );
	}
	PathSegment& end = m_path[m_pathLength++];
	end.area = goalArea;
	end.how = NUM_TRAVERSE_TYPES;
	end.position = goal;
	end.ladder = NULL;
	m_pathIndex = 1;
	m_areaEnteredTimestamp = gpGlobals->time;
	m_spotEncounter = NULL;
	m_goalPosition = m_path[1].position;
	if ( m_path[1].ladder )
		SetupLadderMovement();
	else
		m_ladder = NULL;
	return true;
}

void CCSBot::BuildTrivialPath( const Vector* goal )
{
	m_pathIndex = 1;
	m_pathLength = 2;
	m_path[0].area = m_lastKnownArea;
	m_path[0].position = pev->origin;
	m_path[0].position.z = m_lastKnownArea->GetZ( &pev->origin );
	m_path[0].ladder = NULL;
	m_path[0].how = NUM_TRAVERSE_TYPES;
	m_path[1].area = m_lastKnownArea;
	m_path[1].position = *goal;
	m_path[1].position.z = m_lastKnownArea->GetZ( goal );
	m_path[1].ladder = NULL;
	m_path[1].how = NUM_TRAVERSE_TYPES;
	m_spotEncounter = NULL;
	m_ladder = NULL;
	m_areaEnteredTimestamp = gpGlobals->time;
	m_goalPosition = *goal;
}

void CCSBot::SetPathIndex( int index )
{
	m_pathIndex = std::min( m_pathLength - 1, index );
	m_areaEnteredTimestamp = gpGlobals->time;
	if ( m_path[m_pathIndex].ladder )
	{
		SetupLadderMovement();
		return;
	}
	if ( m_pathIndex > 1 && m_pathIndex < m_pathLength )
		m_spotEncounter = m_path[m_pathIndex - 1].area->GetSpotEncounter( m_path[m_pathIndex - 2].area, m_path[m_pathIndex].area );
	else
		m_spotEncounter = NULL;
	m_ladder = NULL;
}

bool CCSBot::ComputePathPositions()
{
	if ( m_pathLength == 0 )
		return false;
	m_path[0].position = *m_path[0].area->GetCenter();
	m_path[0].ladder = NULL;
	m_path[0].how = NUM_TRAVERSE_TYPES;
	for ( int i = 1; i < m_pathLength; ++i )
	{
		PathSegment& segment = m_path[i];
		CNavArea* from = m_path[i - 1].area;
		if ( segment.how <= GO_WEST )
		{
			segment.ladder = NULL;
			NavDirType direction = static_cast<NavDirType>( segment.how );
			from->ComputeClosestPointInPortal( segment.area, direction, &m_path[i - 1].position, &segment.position );
			AddDirectionVector( &segment.position, direction, 5.0f );
			segment.position.z = from->GetZ( &segment.position );
			if ( !segment.area->IsConnected( from, NUM_DIRECTIONS ) )
			{
				Vector step( 0, 0, 0 );
				AddDirectionVector( &step, direction, 25.0f );
				segment.position = segment.position + step;
				if ( m_pathLength <= 254 )
				{
					for ( int j = m_pathLength - 1; j >= i; --j )
						m_path[j + 1] = m_path[j];
					++m_pathLength;
					m_path[i + 1].position = segment.position + step;
					m_path[i + 1].position.z = segment.area->GetZ( &m_path[i + 1].position );
					++i;
				}
			}
		}
		else if ( segment.how == GO_LADDER_UP || segment.how == GO_LADDER_DOWN )
		{
			const NavLadderList* ladders = from->GetLadderList( segment.how == GO_LADDER_UP ? LADDER_UP : LADDER_DOWN );
			const CNavLadder* ladder = NULL;
			for ( NavLadderList::const_iterator iter = ladders->begin(); iter != ladders->end(); ++iter )
			{
				const CNavLadder* candidate = *iter;
				if ( segment.how == GO_LADDER_UP ? (candidate->m_topForwardArea == segment.area || candidate->m_topLeftArea == segment.area || candidate->m_topRightArea == segment.area) : candidate->m_bottomArea == segment.area )
				{
					ladder = candidate;
					break;
				}
			}
			if ( ladder == NULL )
			{
				PrintIfWatched( "ERROR: Can't find ladder in path\n" );
				return false;
			}
			segment.ladder = ladder;
			segment.position = segment.how == GO_LADDER_UP ? ladder->m_bottom : ladder->m_top;
			NavDirType direction = segment.how == GO_LADDER_UP ? ladder->m_dir : OppositeDirection( ladder->m_dir );
			AddDirectionVector( &segment.position, direction, 32.0f );
		}
	}
	return true;
}

float CCSBot::GetPathDistanceRemaining() const
{
	if ( m_pathLength == 0 )
		return -1.0f;
	int index = std::min( m_pathLength - 1, m_pathIndex );
	const Vector* previous = m_path[m_pathIndex].area->GetCenter();
	float distance = 0.0f;
	for ( int i = index + 1; i < m_pathLength; ++i )
	{
		const Vector* center = m_path[i].area->GetCenter();
		distance += (*center - *previous).Length();
		previous = center;
	}
	return distance;
}

int CCSBot::FindClosestPointOnPath( const Vector* position, int start, int end, Vector* closest ) const
{
	if ( m_pathLength == 0 || closest == NULL )
		return 0;
	float bestDistance = 10000000000.0f;
	for ( int i = start; i <= end; ++i )
	{
		const Vector& from = m_path[i - 1].position;
		const Vector& to = m_path[i].position;
		Vector direction = to - from;
		float length = direction.NormalizeInPlace();
		float along = DotProduct( *position - from, direction );
		Vector point = along <= 0.0f ? from : (along < length ? from + direction * along : to);
		float distance = (point - *position).LengthSquared();
		if ( bestDistance > distance )
		{
			*closest = point;
			bestDistance = distance;
		}
	}
	return 1;
}

int CCSBot::FindOurPositionOnPath( Vector* closest, bool local ) const
{
	if ( m_pathLength == 0 )
		return -1;
	Vector feet = pev->origin;
	feet.z -= IsCrouching() ? 18.0f : 36.0f;
	int start = 1;
	int end = m_pathLength;
	if ( local )
	{
		start = std::max( 1, m_pathIndex - 3 );
		end = std::min( end, m_pathIndex + 3 );
	}
	float bestDistance = 10000000000.0f;
	int best = -1;
	for ( int i = start; i < end; ++i )
	{
		const Vector& from = m_path[i - 1].position;
		const Vector& to = m_path[i].position;
		Vector direction = to - from;
		float length = direction.NormalizeInPlace();
		float along = DotProduct( feet - from, direction );
		Vector point = along <= 0.0f ? from : (along < length ? from + direction * along : to);
		float distance = (point - feet).LengthSquared();
		if ( bestDistance > distance )
		{
			Vector traceStart = feet + Vector( 0, 0, 36 );
			Vector traceEnd = point + Vector( 0, 0, 36 );
			edict_t* ignore = NULL;
			TraceResult trace;
			for ( ;; )
			{
				UTIL_TraceLine( traceStart, traceEnd, ignore_monsters, ignore, &trace );
				if ( trace.flFraction == 1.0f )
					break;
				ignore = trace.pHit;
				entvars_t* hit = VARS( ignore );
				if ( !FClassnameIs( hit, "func_door" ) && !FClassnameIs( hit, "func_door_rotating" ) && (!FClassnameIs( hit, "func_breakable" ) || hit->takedamage != DAMAGE_YES) )
					break;
				Vector step = traceEnd - (feet + Vector( 0, 0, 36 ));
				step.NormalizeInPlace();
				traceStart = trace.vecEndPos + step * 5.0f;
			}
			if ( trace.flFraction == 1.0f )
			{
				if ( closest )
					*closest = point;
				bestDistance = distance;
				best = i - 1;
			}
		}
	}
	return best;
}

int CCSBot::FindPathPoint( float aheadRange, Vector* point, int* previousIndex )
{
	Vector close;
	int start = FindOurPositionOnPath( &close, true );
	if ( previousIndex )
		*previousIndex = start;
	if ( start <= 0 )
		return m_pathIndex;
	if ( IsCrouching() )
	{
		int index = std::min( start + 1, m_pathLength - 1 );
		*point = m_path[index].position;
		while ( (*point - close).Make2D().IsLengthLessThan( 20.0f ) )
		{
			if ( ++index >= m_pathLength )
				return m_pathLength - 1;
			*point = m_path[index].position;
		}
		return index;
	}
	while ( start < m_pathLength - 1 && (m_path[start + 1].position - close).Make2D().IsLengthLessThan( 20.0f ) )
		++start;
	if ( start > m_pathIndex && start < m_pathLength && (m_path[start].ladder || (m_path[start].area->GetAttributes() & NAV_JUMP)) )
	{
		*point = m_path[start].position;
		return start;
	}
	start = std::min( start + 1, m_pathLength - 1 );
	if ( m_path[start].ladder || (m_path[start].area->GetAttributes() & NAV_JUMP) )
	{
		*point = m_path[start].position;
		return start;
	}
	Vector initialDirection = m_path[start].position - m_path[start - 1].position;
	initialDirection.NormalizeInPlace();
	Vector feet = pev->origin;
	feet.z -= IsCrouching() ? 18.0f : 36.0f;
	Vector eyes = feet + Vector( 0, 0, 36 );
	Vector previousDirection = initialDirection;
	float range = 0.0f;
	bool visible = true;
	bool corner = false;
	int index;
	for ( index = start; index < m_pathLength; ++index )
	{
		const Vector& position = m_path[index].position;
		Vector direction = (position - m_path[index - 1].position).Normalize();
		if ( DotProduct( direction, initialDirection ) < 0.0f )
		{
			--index;
			break;
		}
		if ( DotProduct( direction, previousDirection ) < 0.5f )
		{
			corner = true;
			--index;
			break;
		}
		Vector probe = position + Vector( 0, 0, 36 );
		if ( !IsWalkableTraceLineClear( eyes, probe, WALK_THRU_BREAKABLES ) )
		{
			visible = false;
			break;
		}
		if ( m_path[index].ladder || (m_path[index].area->GetAttributes() & NAV_JUMP) )
			break;
		range += (position - (index == start ? feet : m_path[index - 1].position)).Length2D();
		if ( range >= aheadRange )
			break;
		previousDirection = direction;
	}
	int after = std::max( start, std::min( index, m_pathLength - 1 ) );
	if ( after == 0 )
		*point = m_path[0].position;
	else
	{
		const Vector& before = m_path[after - 1].position;
		Vector delta = m_path[after].position - before;
		float length = delta.Length2D();
		float fraction = 1.0f - (range - aheadRange) / length;
		if ( fraction < 0.0f )
			fraction = 0.0f;
		else if ( fraction > 1.0f )
			fraction = 1.0f;
		*point = before + delta * fraction;
		if ( !visible )
		{
			Vector probe = *point + Vector( 0, 0, 36 );
			while ( fraction > 0.0f && !IsWalkableTraceLineClear( eyes, probe, WALK_THRU_BREAKABLES ) )
			{
				fraction -= 25.0f / length;
				*point = before + delta * fraction;
			}
			if ( fraction <= 0.0f )
				*point = before;
		}
	}
	if ( !corner )
	{
		Vector2D toPoint = (*point - pev->origin).Make2D();
		if ( DotProduct( toPoint, initialDirection.Make2D() ) < 0.0f || toPoint.IsLengthLessThan( 50.0f ) )
		{
			for ( index = start; index < m_pathLength; ++index )
			{
				toPoint = (m_path[index].position - pev->origin).Make2D();
				if ( m_path[index].ladder || (m_path[index].area->GetAttributes() & NAV_JUMP) || toPoint.IsLengthGreaterThan( 50.0f ) )
				{
					*point = m_path[index].position;
					start = index;
					break;
				}
			}
			if ( index == m_pathLength )
			{
				*point = m_path[m_pathLength - 1].position;
				start = m_pathLength - 1;
			}
		}
	}
	return std::min( start, m_pathLength - 1 );
}

int CCSBot::UpdatePathMovement( bool allowSpeedChange )
{
	if ( m_pathLength == 0 )
		return 2;
	if ( cv_bot_walk.value != 0.0f )
		Walk();
	if ( UpdateLadderMovement() )
		return 0;
	if ( m_pathLength == 0 )
		return 2;
	if ( IsOnLadder() )
		Jump( true );
	ASSERT_AT( m_pathIndex < m_pathLength, 1324 );
	bool nearEnd = false;
	if ( m_pathIndex >= m_pathLength - 1 )
	{
		Vector feet = pev->origin;
		feet.z -= IsCrouching() ? 18.0f : 36.0f;
		float distance = (m_path[m_pathLength - 1].position - feet).LengthSquared();
		if ( distance < 40000.0f )
		{
			if ( !IsCrouching() && allowSpeedChange )
				Walk();
			if ( distance < 400.0f )
			{
				m_pathLength = 0;
				m_ladder = NULL;
				if ( allowSpeedChange )
					Run();
				return 1;
			}
			nearEnd = distance < 2500.0f;
		}
	}
	int behind = 0;
	int next = FindPathPoint( 300.0f, &m_goalPosition, &behind );
	if ( m_pathIndex < m_pathLength - 1 && (m_goalPosition - pev->origin).IsLengthLessThan( 100.0f ) )
	{
		m_lookAtState = 0;
		m_lookAtDescription = NULL;
		InhibitLookAround( 0.5f );
	}
	if ( next > m_pathIndex )
		SetPathIndex( next );
	if ( m_ladder == NULL )
	{
		bool crouch = false;
		// behind is -1 when we are not on the path
		for ( int i = std::max( behind, 0 ); i < m_pathLength; ++i )
		{
			CNavArea* area = m_path[i].area;
			if ( area->GetAttributes() & NAV_JUMP )
				break;
			Vector close;
			area->GetClosestPointOnArea( &pev->origin, &close );
			if ( (close - pev->origin).Make2D().IsLengthGreaterThan( 50.0f ) )
				break;
			if ( area->GetAttributes() & NAV_CROUCH )
			{
				Crouch();
				crouch = true;
				break;
			}
		}
		if ( !crouch && !IsJumping() )
			StandUp();
	}
	Vector goalDelta = m_goalPosition - pev->origin;
	m_lookAheadMovementYaw = UTIL_VecToYaw( goalDelta );
	Vector look;
	if ( m_pathIndex == 0 )
		look = m_path[1].position;
	else if ( m_pathIndex >= m_pathLength )
		look = m_path[m_pathLength - 1].position - pev->origin;
	else
	{
		look = m_path[m_pathIndex].position - pev->origin;
		if ( m_path[m_pathIndex].ladder == NULL && !IsNearJump() && look.Make2D().IsLengthLessThan( 500.0f ) )
		{
			float range = look.Length2D();
			int index;
			for ( index = m_pathIndex + 1; index < m_pathLength; ++index )
			{
				Vector delta = m_path[index].position - m_path[index - 1].position;
				float total = range + delta.Length2D();
				if ( total >= 500.0f )
				{
					float fraction = (500.0f - range) / total;
					Vector position;
					if ( fraction <= 0.0f )
						position = m_path[index - 1].position;
					else if ( fraction >= 1.0f )
						position = m_path[index].position;
					else
						position = m_path[index - 1].position + delta * fraction;
					look = position - pev->origin;
					break;
				}
				range = total;
				if ( m_path[index].ladder || (m_path[index].area->GetAttributes() & NAV_JUMP) )
				{
					look = m_path[index].position - pev->origin;
					break;
				}
			}
			if ( index == m_pathLength )
				look = m_path[m_pathLength - 1].position - pev->origin;
		}
	}
	m_lookAheadYaw = UTIL_VecToYaw( look );
	Vector adjustedGoal = m_goalPosition;
	if ( !nearEnd && !IsNearJump() && !IsJumping() )
		FeelerReflexAdjustment( &adjustedGoal );
	if ( (cv_bot_traceview.value == 1.0f && IsLocalPlayerWatchingMe()) || cv_bot_traceview.value == 10.0f )
	{
		if ( m_pathLength )
			DrawPath();
		const Vector& position = m_path[m_pathIndex].position;
		UTIL_DrawBeamPoints( position, position + Vector( 0, 0, 50 ), 1, 255, 255, 0 );
		UTIL_DrawBeamPoints( adjustedGoal, adjustedGoal + Vector( 0, 0, 50 ), 1, 255, 0, 255 );
		UTIL_DrawBeamPoints( pev->origin, adjustedGoal + Vector( 0, 0, 50 ), 1, 255, 0, 255 );
	}
	if ( IsAttacking() || !IsFriendInTheWay( &m_goalPosition ) )
	{
		if ( m_friendAvoidanceFlag )
		{
			m_friendAvoidanceFlag = false;
			ResetStuckMonitor();
		}
	}
	else if ( !m_friendAvoidanceFlag )
	{
		m_friendAvoidanceFlag = true;
		m_friendAvoidanceTimer.Start( 5.0f - 3.0f * GetProfile()->GetAggression() );
	}
	else if ( m_friendAvoidanceTimer.IsElapsed() )
	{
		m_friendAvoidanceFlag = false;
		ResetStuckMonitor();
		m_pathLength = 0;
		m_ladder = NULL;
	}
	if ( !m_friendAvoidanceFlag || m_friendAvoidanceTimer.IsElapsed() )
	{
		MoveTowardsPosition( &adjustedGoal );
		if ( m_stuck && !IsJumping() )
			Wiggle();
	}
	float feetHeight = pev->origin.z - (IsCrouching() ? 18.0f : 36.0f);
	bool fell = false;
	if ( m_goalPosition.z - feetHeight > 58.0f && (pev->origin - m_goalPosition).Make2D().IsLengthLessThan( 75.0f ) )
	{
		if ( m_pathIndex >= m_pathLength - 1 || m_path[m_pathIndex + 1].position.z - feetHeight > 58.0f )
		{
			PrintIfWatched( "I fell off!\n" );
			fell = true;
		}
	}
	if ( !fell && gpGlobals->time - m_areaEnteredTimestamp <= 5.0f )
		return 0;
	if ( m_pathIndex >= m_pathLength - 1 )
		PrintIfWatched( "Giving up trying to get to end of path\n" );
	else
		PrintIfWatched( "Giving up trying to get to area #%d\n", m_path[m_pathIndex].area->GetID() );
	Run();
	StandUp();
	m_pathLength = 0;
	m_ladder = NULL;
	return 2;
}


void CCSBot::DrawPath()
{
	if ( m_pathLength == 0 )
		return;
	for ( int i = 1; i < m_pathLength; ++i )
		UTIL_DrawBeamPoints( m_path[i - 1].position + Vector( 0, 0, 36 ), m_path[i].position + Vector( 0, 0, 36 ), 2, 255, 75, 0 );
	Vector close;
	if ( FindOurPositionOnPath( &close, true ) >= 0 )
	{
		close.z += cv_bot_nav_zdraw.value;
		UTIL_DrawBeamPoints( close + Vector( 0, 0, 25 ), close, 1, 0, 255, 0 );
		UTIL_DrawBeamPoints( close + Vector( 25, 0, 0 ), close - Vector( 25, 0, 0 ), 1, 0, 255, 0 );
		UTIL_DrawBeamPoints( close + Vector( 0, 25, 0 ), close - Vector( 0, 25, 0 ), 1, 0, 255, 0 );
	}
}
