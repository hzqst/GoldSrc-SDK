#include "pman_main.h"
#include "pman_misc.h"

#include "cl_entity.h"

#include <stdlib.h>
#include <string.h>

VectorOfMemoryBlocks CMiniMem::m_vecMemoryPool;
long CMiniMem::m_lMemoryBlockSize = 0;
long CMiniMem::m_lMaxBlocks = 0;
long CMiniMem::m_lMemoryPoolSize = 0;

long g_lMaxParticleClassSize = 0;
CMiniMem* CMiniMem::_instance = NULL;

MemList m_FreeMem;
MemList m_ActiveMem;

float g_flOldTime = 0;

long MaxParticleClassSize( unsigned long size )
{
	unsigned long maxSize = g_lMaxParticleClassSize;

	if ( maxSize < sizeof( CBaseParticle ) )
		maxSize = sizeof( CBaseParticle );

	if ( size != 0 && size > (unsigned long)g_lMaxParticleClassSize )
		maxSize = size;

	g_lMaxParticleClassSize = maxSize;

	return g_lMaxParticleClassSize;
}

CMiniMem::CMiniMem( long poolSize, long blockSize )
{
	m_lMemoryPoolSize = poolSize;
	m_lMemoryBlockSize = blockSize;
	m_lMaxBlocks = m_lMemoryPoolSize / sizeof( MemoryBlock );

	for ( long i = 0; i < m_lMaxBlocks; ++i )
	{
		MemoryBlock* block = new MemoryBlock( m_lMemoryBlockSize );
		m_vecMemoryPool.push_back( block );
		m_FreeMem.Push( block );
	}

	m_pVisibleParticles = new visibleparticles_t[m_lMaxBlocks];
}

CMiniMem::~CMiniMem()
{
	MemoryBlock* block = m_FreeMem.Front();

	while ( block )
	{
		MemoryBlock* next = block->next;
		m_FreeMem.Delete( block );

		delete block;
		block = next;
	}

	m_vecMemoryPool.clear();

	delete[] m_pVisibleParticles;
}

void CMiniMem::Shutdown()
{
	delete _instance;
}

char* CMiniMem::AllocateFreeBlock()
{
	MemoryBlock* block = m_FreeMem.Pop();
	if ( !block )
		return NULL;

	m_ActiveMem.Push( block );
	return block->Memory();
}

bool CMiniMem::CheckSize( int size )
{
	if ( (unsigned long)size > (unsigned long)CMiniMem::Instance()->MaxBlockSize() )
		return false;

	return true;
}

char* CMiniMem::newBlock()
{
	char* block = AllocateFreeBlock();
	if ( !block )
		gEngfuncs.Con_DPrintf( "Particleman is out of memory, too many particles" );

	return block;
}

void CMiniMem::deleteBlock( MemoryBlock* block )
{
	m_ActiveMem.Delete( block );
	m_FreeMem.Push( block );
}

long CMiniMem::PercentUsed()
{
	long freeBlocks = 0;
	MemoryBlock* block = m_FreeMem.Front();

	while ( block )
	{
		freeBlocks++;
		block = block->next;
	}

	return ( freeBlocks * 100 ) / m_lMaxBlocks;
}

long CMiniMem::MaxBlockSize()
{
	return m_lMemoryBlockSize;
}

CMiniMem* CMiniMem::Instance()
{
	if ( !_instance )
		_instance = new CMiniMem( 30000, g_lMaxParticleClassSize );

	return _instance;
}

int ParticleSortFn( const void* first, const void* second )
{
	const visibleparticles_t* firstParticle = (const visibleparticles_t*)first;
	const visibleparticles_t* secondParticle = (const visibleparticles_t*)second;
	float firstDistance = firstParticle->pVisibleParticle->GetPlayerDistance();
	float secondDistance = secondParticle->pVisibleParticle->GetPlayerDistance();

	if ( firstDistance > secondDistance )
		return -1;
	if ( firstDistance == secondDistance )
		return 0;
	return 1;
}

void CMiniMem::ProcessAll()
{
	float time = gEngfuncs.GetClientTime();

	MemoryBlock* currentBlock = m_ActiveMem.Front();

	m_iTotalParticles = 0;
	m_iParticlesDrawn = 0;

	memset( m_pVisibleParticles, 0, sizeof( visibleparticles_t ) * m_lMaxBlocks );

	while ( currentBlock )
	{
		CCoreTriangleEffect* effect = (CCoreTriangleEffect*)currentBlock->Memory();

		if ( !effect )
		{
			currentBlock = currentBlock->next;
			continue;
		}

		m_iTotalParticles++;

		if ( !IsGamePaused() )
			effect->Think( time );

		if ( effect->m_flDieTime != 0 && time >= effect->m_flDieTime )
		{
			effect->Die();

			MemoryBlock* deadBlock = currentBlock;
			currentBlock = currentBlock->next;
			deleteBlock( deadBlock );
			continue;
		}

		if ( effect->CheckVisibility() == true )
		{
			cl_entity_t* localPlayer = gEngfuncs.GetLocalPlayer();
			Vector playerOrigin = localPlayer ? localPlayer->origin : Vector( 0, 0, 0 );
			float distance = 0;

			for ( int axis = 0; axis < 3; axis++ )
			{
				float delta = playerOrigin[axis] - effect->m_vOrigin[axis];
				distance += delta * delta;
			}

			effect->SetPlayerDistance( distance );

			m_pVisibleParticles[m_iParticlesDrawn].pVisibleParticle = effect;
			IncreaseParticlesDrawn();
		}

		currentBlock = currentBlock->next;
	}

	qsort( m_pVisibleParticles, m_iParticlesDrawn, sizeof( visibleparticles_t ), ParticleSortFn );

	for ( int i = 0; i < m_iParticlesDrawn; i++ )
		m_pVisibleParticles[i].pVisibleParticle->Draw();

	g_flOldTime = time;
}

int CMiniMem::ApplyForce( Vector origin, Vector direction, float radius, float strength )
{
	MemoryBlock* currentBlock = m_ActiveMem.Front();

	radius = radius * radius;

	while ( currentBlock )
	{
		CCoreTriangleEffect* effect = (CCoreTriangleEffect*)currentBlock->Memory();

		if ( !effect )
			continue;

		if ( effect->m_bAffectedByForce )
		{
			float size = effect->m_flSize / 5.0f;
			Vector mins = effect->m_vOrigin - Vector( size, size, size );
			Vector maxs = effect->m_vOrigin + Vector( size, size, size );
			float distanceSquared = 0;

			for ( int axis = 0; axis < 3 && distanceSquared <= radius; axis++ )
			{
				float distance;

				if ( origin[axis] < mins[axis] )
					distance = origin[axis] - mins[axis];
				else if ( origin[axis] > maxs[axis] )
					distance = origin[axis] - maxs[axis];
				else
					distance = 0;

				distanceSquared += distance * distance;
			}

			if ( distanceSquared > radius )
			{
				currentBlock = currentBlock->next;
				continue;
			}

			float falloff = ( strength / ( radius * 0.5f ) ) * ( origin - effect->m_vOrigin ).Length();
			float adjustedStrength = strength - falloff;

			if ( adjustedStrength < 0 )
				adjustedStrength = 0;

			if ( direction == Vector( 0, 0, 0 ) )
				effect->m_vVelocity = ( ( origin - effect->m_vOrigin ).Normalize() + effect->m_vVelocity.Normalize() )
					* -( adjustedStrength / effect->m_flMass );
			else
				effect->m_vVelocity = ( direction.Normalize() + effect->m_vVelocity.Normalize() )
					* ( adjustedStrength / effect->m_flMass );

			effect->Force();
		}

		currentBlock = currentBlock->next;
	}

	return 1;
}

void CMiniMem::Reset()
{
	m_ActiveMem.Reset();
	m_FreeMem.Reset();

	for ( unsigned int i = 0; i < m_vecMemoryPool.size(); i++ )
		m_FreeMem.Push( m_vecMemoryPool[i] );
}

void UpdateParticles()
{
	CMiniMem::Instance()->ProcessAll();
}
