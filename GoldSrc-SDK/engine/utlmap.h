//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============
//
// Purpose: An associative container built on CUtlRBTree. Pretty much identical to std::map.
//
// $NoKeywords: $
//=============================================================================
#pragma once

#include "utlrbtree.h"

//-----------------------------------------------------------------------------
// The default less func, using operator< of the key type
//-----------------------------------------------------------------------------
template <typename T>
class CDefOps
{
public:
	static bool LessFunc( const T& lhs, const T& rhs ) { return lhs < rhs; }
};

#define DefLessFunc( type ) CDefOps<type>::LessFunc

template <typename MAP_T>
void SetDefLessFunc( MAP_T& map )
{
	map.SetLessFunc( DefLessFunc( typename MAP_T::KeyType_t ) );
}

template <typename K, typename T, typename I = unsigned short>
class CUtlMap
{
public:
	typedef K KeyType_t;
	typedef T ElemType_t;
	typedef I IndexType_t;

	// Returns true if the first parameter is "less" than the second
	typedef bool ( *LessFunc_t )( const KeyType_t&, const KeyType_t& );

	// Left at growSize = 0, the memory will first allocate 1 element and double in size
	// at each increment. LessFunc_t is required, but may be set after the constructor
	CUtlMap( int growSize = 0, int initSize = 0, LessFunc_t lessfunc = NULL )
		: m_Tree( growSize, initSize, CKeyLess( lessfunc ) )
	{
	}

	explicit CUtlMap( LessFunc_t lessfunc )
		: m_Tree( 0, 0, CKeyLess( lessfunc ) )
	{
	}

	ElemType_t& Element( IndexType_t i ) { return m_Tree.Element( i ).elem; }
	const ElemType_t& Element( IndexType_t i ) const { return m_Tree.Element( i ).elem; }
	ElemType_t& operator[]( IndexType_t i ) { return m_Tree.Element( i ).elem; }
	const ElemType_t& operator[]( IndexType_t i ) const { return m_Tree.Element( i ).elem; }
	KeyType_t& Key( IndexType_t i ) { return m_Tree.Element( i ).key; }
	const KeyType_t& Key( IndexType_t i ) const { return m_Tree.Element( i ).key; }

	unsigned int Count() const { return m_Tree.Count(); }
	IndexType_t MaxElement() const { return m_Tree.MaxElement(); }
	bool IsValidIndex( IndexType_t i ) const { return m_Tree.IsValidIndex( i ); }
	bool IsValid() const { return m_Tree.IsValid(); }
	static IndexType_t InvalidIndex() { return CTree::InvalidIndex(); }

	void SetLessFunc( LessFunc_t func ) { m_Tree.SetLessFunc( CKeyLess( func ) ); }

	// Inserts in order; an existing key keeps its element and its index is returned
	IndexType_t Insert( const KeyType_t& key, const ElemType_t& insert )
	{
		IndexType_t i = Find( key );
		if ( i != InvalidIndex() )
			return i;

		Node_t node;
		node.key = key;
		node.elem = insert;
		return m_Tree.Insert( node );
	}

	IndexType_t Insert( const KeyType_t& key )
	{
		IndexType_t i = Find( key );
		if ( i != InvalidIndex() )
			return i;

		Node_t node;
		node.key = key;
		return m_Tree.Insert( node );
	}

	IndexType_t InsertOrReplace( const KeyType_t& key, const ElemType_t& insert )
	{
		IndexType_t i = Find( key );
		if ( i != InvalidIndex() )
		{
			Element( i ) = insert;
			return i;
		}

		return Insert( key, insert );
	}

	IndexType_t Find( const KeyType_t& key ) const
	{
		Node_t dummyNode;
		dummyNode.key = key;
		return m_Tree.Find( dummyNode );
	}

	void RemoveAt( IndexType_t i ) { m_Tree.RemoveAt( i ); }

	bool Remove( const KeyType_t& key )
	{
		IndexType_t i = Find( key );
		if ( i == InvalidIndex() )
			return false;

		RemoveAt( i );
		return true;
	}

	void RemoveAll() { m_Tree.RemoveAll(); }

	IndexType_t FirstInorder() const { return m_Tree.FirstInorder(); }
	IndexType_t NextInorder( IndexType_t i ) const { return m_Tree.NextInorder( i ); }
	IndexType_t PrevInorder( IndexType_t i ) const { return m_Tree.PrevInorder( i ); }
	IndexType_t LastInorder() const { return m_Tree.LastInorder(); }

	struct Node_t
	{
		KeyType_t key;
		ElemType_t elem;
	};

	class CKeyLess
	{
	public:
		CKeyLess( LessFunc_t lessFunc ) : m_LessFunc( lessFunc ) {}

		bool operator!() const
		{
			return !m_LessFunc;
		}

		bool operator()( const Node_t& left, const Node_t& right ) const
		{
			return m_LessFunc( left.key, right.key );
		}

		LessFunc_t m_LessFunc;
	};

	typedef CUtlRBTree<Node_t, I, CKeyLess> CTree;

	CTree* AccessTree() { return &m_Tree; }

protected:
	CTree m_Tree;
};
