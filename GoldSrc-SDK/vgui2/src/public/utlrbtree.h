//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================

#ifndef UTLRBTREE_H
#define UTLRBTREE_H

#ifdef _WIN32
#pragma once
#endif

#include "UtlMemory.h"

#include <cstring>
#include <new>

#ifdef UTL_ENGINE_ASSERTS
#ifndef UTLRBTREE_SOURCE_PATH
#define UTLRBTREE_SOURCE_PATH	L"c:\\buildslave\\goldsrc_win32\\build\\GoldSrc\\public\\utlrbtree.h"
#endif
#endif

//-----------------------------------------------------------------------------
// A red-black binary search tree
//-----------------------------------------------------------------------------

template <class T, class I = unsigned short, class L = bool ( * )( const T&, const T& )> class CUtlRBTree
{
public:
	typedef T KeyType_t;
	typedef T ElemType_t;
	typedef I IndexType_t;
	typedef L LessFunc_t;
	struct Links_t
	{
		I m_Left;
		I m_Right;
		I m_Parent;
		I m_Tag;
	};
	struct Node_t : Links_t
	{
		T m_Data;
	};

	CUtlRBTree( int growSize = 0, int initialSize = 0, LessFunc_t less = NULL )
		: m_LessFunc( less ), m_Elements( growSize, initialSize ), m_Root( InvalidIndex() ), m_NumElements( 0 ),
		  m_FirstFree( InvalidIndex() ), m_TotalElements( 0 )
	{
	}

	static constexpr I InvalidIndex()
	{
		return (I)-1;
	}

	T& Element( I index )
	{
		return m_Elements[index].m_Data;
	}

	const T& Element( I index ) const
	{
		return m_Elements[index].m_Data;
	}

	T& operator[]( I index )
	{
		return Element( index );
	}

	const T& operator[]( I index ) const
	{
		return Element( index );
	}

	void SetLessFunc( LessFunc_t less )
	{
		if ( !m_LessFunc )
			m_LessFunc = less;
#ifdef UTL_ENGINE_ASSERTS
		else
			_wassert( L"0", UTLRBTREE_SOURCE_PATH, 1139 );
#endif
	}

	const Links_t& Links( I index ) const
	{
		static const Links_t sentinel = { InvalidIndex(), InvalidIndex(), InvalidIndex(), BLACK };
		return index == InvalidIndex() ? sentinel : (const Links_t&)m_Elements[index];
	}

	Links_t& Links( I index )
	{
#ifdef UTL_ENGINE_ASSERTS
		if ( index == InvalidIndex() )
			_wassert( L"i != InvalidIndex()", UTLRBTREE_SOURCE_PATH, 410 );
#endif
		return m_Elements[index];
	}

	I Parent( I index ) const
	{
		return Links( index ).m_Parent;
	}

	I LeftChild( I index ) const
	{
		return Links( index ).m_Left;
	}

	I RightChild( I index ) const
	{
		return Links( index ).m_Right;
	}

	bool IsRoot( I index ) const
	{
		return index == m_Root;
	}

	bool IsLeftChild( I index ) const
	{
		return LeftChild( Parent( index ) ) == index;
	}

	bool IsRightChild( I index ) const
	{
		return RightChild( Parent( index ) ) == index;
	}

	bool IsValidIndex( I index ) const
	{
		return Links( index ).m_Left != index;
	}

	bool IsValid() const
	{
		return m_NumElements == 0 || ( m_Root < m_TotalElements && Parent( m_Root ) == InvalidIndex() );
	}

	I Find( const T& data ) const
	{
		if ( !m_LessFunc )
		{
#ifdef UTL_ENGINE_ASSERTS
			_wassert( L"m_LessFunc", UTLRBTREE_SOURCE_PATH, 1190 );
#endif
			return InvalidIndex();
		}
		I index = m_Root;
		while ( index != InvalidIndex() )
		{
			if ( m_LessFunc( (T&)data, (T&)Element( index ) ) )
				index = LeftChild( index );
			else if ( m_LessFunc( (T&)Element( index ), (T&)data ) )
				index = RightChild( index );
			else
				break;
		}
		return index;
	}

	I Count() const
	{
		return m_NumElements;
	}

	I MaxElement() const
	{
		return m_TotalElements;
	}

	I FirstInorder() const
	{
		I index = m_Root;
		while ( LeftChild( index ) != InvalidIndex() )
			index = LeftChild( index );
		return index;
	}

	I NextInorder( I index ) const
	{
#ifdef UTL_ENGINE_ASSERTS
		if ( !IsValidIndex( index ) )
			_wassert( L"IsValidIndex(i)", UTLRBTREE_SOURCE_PATH, 894 );
#endif
		if ( RightChild( index ) != InvalidIndex() )
		{
			index = RightChild( index );
			while ( LeftChild( index ) != InvalidIndex() )
				index = LeftChild( index );
			return index;
		}

		I parent = Parent( index );
		while ( IsRightChild( index ) )
		{
			index = parent;
			if ( index == InvalidIndex() )
				break;
			parent = Parent( index );
		}
		return parent;
	}

	I LastInorder() const
	{
		I index = m_Root;
		while ( RightChild( index ) != InvalidIndex() )
			index = RightChild( index );
		return index;
	}

	I PrevInorder( I index ) const
	{
#ifdef UTL_ENGINE_ASSERTS
		if ( !IsValidIndex( index ) )
			_wassert( L"IsValidIndex(i)", UTLRBTREE_SOURCE_PATH, 917 );
#endif
		if ( LeftChild( index ) != InvalidIndex() )
		{
			index = LeftChild( index );
			while ( RightChild( index ) != InvalidIndex() )
				index = RightChild( index );
			return index;
		}

		I parent = Parent( index );
		while ( IsLeftChild( index ) )
		{
			index = parent;
			if ( index == InvalidIndex() )
				break;
			parent = Parent( index );
		}
		return parent;
	}

	void RemoveAt( I index )
	{
		if ( index == InvalidIndex() )
			return;

		I replacement = index;
		if ( LeftChild( index ) != InvalidIndex() && RightChild( index ) != InvalidIndex() )
		{
			replacement = RightChild( index );
			while ( LeftChild( replacement ) != InvalidIndex() )
				replacement = LeftChild( replacement );
		}

		I child = LeftChild( replacement );
		if ( child == InvalidIndex() )
			child = RightChild( replacement );

		if ( child != InvalidIndex() )
			SetParent( child, Parent( replacement ) );
		if ( !IsRoot( replacement ) )
		{
			if ( IsLeftChild( replacement ) )
				SetLeftChild( Parent( replacement ), child );
			else
				SetRightChild( Parent( replacement ), child );
		}
		else
			m_Root = child;

		I removedColor = Tag( replacement );
		if ( replacement != index )
		{
			SetParent( replacement, Parent( index ) );
			SetRightChild( replacement, RightChild( index ) );
			SetLeftChild( replacement, LeftChild( index ) );

			if ( !IsRoot( index ) )
			{
				if ( IsLeftChild( index ) )
					SetLeftChild( Parent( index ), replacement );
				else
					SetRightChild( Parent( index ), replacement );
			}
			else
				m_Root = replacement;

			if ( LeftChild( replacement ) != InvalidIndex() )
				SetParent( LeftChild( replacement ), replacement );
			if ( RightChild( replacement ) != InvalidIndex() )
				SetParent( RightChild( replacement ), replacement );

			Links( replacement ).m_Tag = Tag( index );
		}

		if ( child != InvalidIndex() && removedColor == BLACK )
			RemoveRebalance( child );

		FreeNode( index );
		--m_NumElements;

#ifdef UTL_ENGINE_ASSERTS
		if ( !IsValid() )
			_wassert( L"IsValid()", UTLRBTREE_SOURCE_PATH, 834 );
#endif
	}

	void RemoveAll()
	{
		I nextFree = InvalidIndex();
		for ( int i = (int)m_TotalElements; --i >= 0; )
		{
			I index = (I)i;
			if ( IsValidIndex( index ) )
				Element( index ).~T();
			SetRightChild( index, nextFree );
			SetLeftChild( index, index );
			nextFree = index;
		}
		m_FirstFree = nextFree;
		m_Root = InvalidIndex();
		m_NumElements = 0;
	}

	I Insert( const T& data )
	{
		I index = CreateInsertedNode( data );
		new ( &Element( index ) ) T( data );
		return index;
	}

	I CreateInsertedNode( const T& data )
	{
		I parent;
		bool left;
		FindInsertionPosition( data, parent, left );
		return InsertAt( parent, left );
	}

	I InsertAt( I parent, bool left )
	{
		I index = NewNode();
		LinkToParent( index, parent, left );
		++m_NumElements;
#ifdef UTL_ENGINE_ASSERTS
		if ( !IsValid() )
			_wassert( L"IsValid()", UTLRBTREE_SOURCE_PATH, 657 );
#endif
		return index;
	}

	void RotateLeft( I index )
	{
		I right = RightChild( index );
		SetRightChild( index, LeftChild( right ) );
		if ( LeftChild( right ) != InvalidIndex() )
			SetParent( LeftChild( right ), index );
		if ( right != InvalidIndex() )
			SetParent( right, Parent( index ) );
		if ( !IsRoot( index ) )
		{
			if ( IsLeftChild( index ) )
				SetLeftChild( Parent( index ), right );
			else
				SetRightChild( Parent( index ), right );
		}
		else
			m_Root = right;
		SetLeftChild( right, index );
		if ( index != InvalidIndex() )
			SetParent( index, right );
	}

	void RotateRight( I index )
	{
		I left = LeftChild( index );
		SetLeftChild( index, RightChild( left ) );
		if ( RightChild( left ) != InvalidIndex() )
			SetParent( RightChild( left ), index );
		if ( left != InvalidIndex() )
			SetParent( left, Parent( index ) );
		if ( !IsRoot( index ) )
		{
			if ( IsRightChild( index ) )
				SetRightChild( Parent( index ), left );
			else
				SetLeftChild( Parent( index ), left );
		}
		else
			m_Root = left;
		SetRightChild( left, index );
		if ( index != InvalidIndex() )
			SetParent( index, left );
	}

	void InsertRebalance( I index )
	{
		while ( index != m_Root && Tag( Parent( index ) ) == RED )
		{
			I parent = Parent( index );
			I grandparent = Parent( parent );
			if ( parent == LeftChild( grandparent ) )
			{
				I uncle = RightChild( grandparent );
				if ( Tag( uncle ) == RED )
				{
					Links( parent ).m_Tag = BLACK;
					Links( uncle ).m_Tag = BLACK;
					Links( grandparent ).m_Tag = RED;
					index = grandparent;
				}
				else
				{
					if ( index == RightChild( parent ) )
					{
						index = parent;
						RotateLeft( index );
						parent = Parent( index );
						grandparent = Parent( parent );
					}
					Links( parent ).m_Tag = BLACK;
					Links( grandparent ).m_Tag = RED;
					RotateRight( grandparent );
				}
			}
			else
			{
				I uncle = LeftChild( grandparent );
				if ( Tag( uncle ) == RED )
				{
					Links( parent ).m_Tag = BLACK;
					Links( uncle ).m_Tag = BLACK;
					Links( grandparent ).m_Tag = RED;
					index = grandparent;
				}
				else
				{
					if ( index == LeftChild( parent ) )
					{
						index = parent;
						RotateRight( index );
						parent = Parent( index );
						grandparent = Parent( parent );
					}
					Links( parent ).m_Tag = BLACK;
					Links( grandparent ).m_Tag = RED;
					RotateLeft( grandparent );
				}
			}
		}
		Links( m_Root ).m_Tag = BLACK;
	}

	void RemoveRebalance( I index )
	{
		while ( index != m_Root && Tag( index ) == BLACK )
		{
			I parent = Parent( index );

			if ( index == LeftChild( parent ) )
			{
				I sibling = RightChild( parent );
				if ( Tag( sibling ) == RED )
				{
					Links( sibling ).m_Tag = BLACK;
					Links( parent ).m_Tag = RED;
					RotateLeft( parent );

					parent = Parent( index );
					sibling = RightChild( parent );
				}
				if ( Tag( LeftChild( sibling ) ) == BLACK && Tag( RightChild( sibling ) ) == BLACK )
				{
					if ( sibling != InvalidIndex() )
						Links( sibling ).m_Tag = RED;
					index = parent;
				}
				else
				{
					if ( Tag( RightChild( sibling ) ) == BLACK )
					{
						Links( LeftChild( sibling ) ).m_Tag = BLACK;
						Links( sibling ).m_Tag = RED;
						RotateRight( sibling );

						parent = Parent( index );
						sibling = RightChild( parent );
					}
					Links( sibling ).m_Tag = Tag( parent );
					Links( parent ).m_Tag = BLACK;
					Links( RightChild( sibling ) ).m_Tag = BLACK;
					RotateLeft( parent );
					index = m_Root;
				}
			}
			else
			{
				I sibling = LeftChild( parent );
				if ( Tag( sibling ) == RED )
				{
					Links( sibling ).m_Tag = BLACK;
					Links( parent ).m_Tag = RED;
					RotateRight( parent );

					parent = Parent( index );
					sibling = LeftChild( parent );
				}
				if ( Tag( RightChild( sibling ) ) == BLACK && Tag( LeftChild( sibling ) ) == BLACK )
				{
					if ( sibling != InvalidIndex() )
						Links( sibling ).m_Tag = RED;
					index = parent;
				}
				else
				{
					if ( Tag( LeftChild( sibling ) ) == BLACK )
					{
						Links( RightChild( sibling ) ).m_Tag = BLACK;
						Links( sibling ).m_Tag = RED;
						RotateLeft( sibling );

						parent = Parent( index );
						sibling = LeftChild( parent );
					}
					Links( sibling ).m_Tag = Tag( parent );
					Links( parent ).m_Tag = BLACK;
					Links( LeftChild( sibling ) ).m_Tag = BLACK;
					RotateRight( parent );
					index = m_Root;
				}
			}
		}
		Links( index ).m_Tag = BLACK;
	}

protected:
	void SetParent( I index, I parent )
	{
		Links( index ).m_Parent = parent;
	}

	void SetLeftChild( I index, I child )
	{
		Links( index ).m_Left = child;
	}

	void SetRightChild( I index, I child )
	{
		Links( index ).m_Right = child;
	}

	I NewNode()
	{
		I index;
		if ( m_FirstFree == InvalidIndex() )
		{
			if ( m_Elements.NumAllocated() == (int)m_TotalElements )
				m_Elements.Grow( 0 );
			index = m_TotalElements;
			++m_TotalElements;
		}
		else
		{
			index = m_FirstFree;
			m_FirstFree = RightChild( m_FirstFree );
		}
		new ( &Element( index ) ) T;
		return index;
	}

	void LinkToParent( I index, I parent, bool isLeft )
	{
		Links_t& elem = Links( index );
		elem.m_Parent = parent;
		elem.m_Left = elem.m_Right = InvalidIndex();
		elem.m_Tag = RED;

		if ( parent != InvalidIndex() )
		{
			if ( isLeft )
				Links( parent ).m_Left = index;
			else
				Links( parent ).m_Right = index;
		}
		else
		{
			m_Root = index;
		}

		InsertRebalance( index );
	}

	void FindInsertionPosition( const T& data, I& parent, bool& left )
	{
#ifdef UTL_ENGINE_ASSERTS
		if ( !m_LessFunc )
			_wassert( L"m_LessFunc", UTLRBTREE_SOURCE_PATH, 1152 );
#endif
		I index = m_Root;
		parent = InvalidIndex();
		left = false;
		while ( index != InvalidIndex() )
		{
			parent = index;
			if ( m_LessFunc( (T&)data, Element( index ) ) )
			{
				left = true;
				index = LeftChild( index );
			}
			else
			{
				left = false;
				index = RightChild( index );
			}
		}
	}

	void FreeNode( I index )
	{
#ifdef UTL_ENGINE_ASSERTS
		if ( !( IsValidIndex( index ) && ( index != InvalidIndex() ) ) )
			_wassert( L"IsValidIndex(i) && (i != InvalidIndex())", UTLRBTREE_SOURCE_PATH, 484 );
#endif
		Element( index ).~T();
		SetLeftChild( index, index );
		SetRightChild( index, m_FirstFree );
		m_FirstFree = index;
	}

private:
	// copy constructors not allowed
	CUtlRBTree( const CUtlRBTree<T, I, L>& tree );
	CUtlRBTree<T, I, L>& operator=( const CUtlRBTree<T, I, L>& other );

	enum
	{
		RED = 0,
		BLACK = 1
	};

	I Tag( I index ) const
	{
		return Links( index ).m_Tag;
	}

	LessFunc_t m_LessFunc;
	CUtlMemory<Node_t> m_Elements;
	I m_Root;
	I m_NumElements;
	I m_FirstFree;
	I m_TotalElements;
};

#endif // UTLRBTREE_H
