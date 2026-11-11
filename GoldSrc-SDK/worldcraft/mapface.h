//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef MAPFACE_H
#define MAPFACE_H
#pragma once

#include <fstream.h>
#include "worldcraft_mathlib.h"
#include "MapAtom.h"
#include "Vector4d.h"
#include "utlvector.h"
#include "Render3D.h"

class CCheckFaceInfo;
class IEditorTexture;
class CRender3D;
class CMapWorld;

struct LoadFace_t;



//
// Flags for CMapFace::CopyFrom.
//
#define COPY_FACE_PLANE			0x00000001			// Copies only the face's plane. Used for carving.
#define COPY_FACE_POINTS		0x00000002			// Copies the face's points and plane.

//
// Used for storing the extrema of a face. Each element of the Extents_t array
// contains a point that represents a local extreme along a particular dimension.
//
enum
{
	EXTENTS_XMIN = 0,
	EXTENTS_XMAX,
	EXTENTS_YMIN,
	EXTENTS_YMAX,
	EXTENTS_ZMIN,
	EXTENTS_ZMAX,
	NUM_EXTENTS_DIMS,
};

typedef Vector Extents_t[NUM_EXTENTS_DIMS];

typedef struct
{
	Vector		normal;
	float		dist;
	Vector		planepts[3];
} PLANE;

typedef struct
{
	int		numpoints;
	Vector	*p;			// variable sized
} winding_t;

enum FaceOrientation_t
{
	FACE_ORIENTATION_FLOOR = 0,
	FACE_ORIENTATION_CEILING,
	FACE_ORIENTATION_NORTH_WALL,
	FACE_ORIENTATION_SOUTH_WALL,
	FACE_ORIENTATION_EAST_WALL,
	FACE_ORIENTATION_WEST_WALL,
	FACE_ORIENTATION_INVALID
};

//
// Both an enumeration and bitflags. Used as bitflags when querying a face for its texture
// alignment because it could be world aligned and face aligned at the same time.
//
enum TextureAlignment_t
{
	TEXTURE_ALIGN_NONE	= 0x0000,
	TEXTURE_ALIGN_WORLD = 0x0001,
	TEXTURE_ALIGN_FACE	= 0x0002,
	TEXTURE_ALIGN_QUAKE = 0x0004
};

enum TextureJustification_t
{
	TEXTURE_JUSTIFY_NONE = 0,
	TEXTURE_JUSTIFY_TOP,
	TEXTURE_JUSTIFY_BOTTOM,
	TEXTURE_JUSTIFY_LEFT,
	TEXTURE_JUSTIFY_CENTER,
	TEXTURE_JUSTIFY_RIGHT,
	TEXTURE_JUSTIFY_FIT,
	TEXTURE_JUSTIFY_MAX
};

#define INIT_TEXTURE_FORCE 0x0001
#define INIT_TEXTURE_AXES			0x0002
#define INIT_TEXTURE_ROTATION		0x0004
#define INIT_TEXTURE_SHIFT			0x0008
#define INIT_TEXTURE_SCALE			0x0010
#define INIT_TEXTURE_ALL			(INIT_TEXTURE_AXES | INIT_TEXTURE_ROTATION | INIT_TEXTURE_SHIFT | INIT_TEXTURE_SCALE)

//
// Flags for CreateFace.
//
#define CREATE_FACE_PRESERVE_PLANE	0x0001		// Hack to prevent plane from being recalculated while building a solid from its planes.
#define CREATE_FACE_CLIPPING		0x0002

//
// Serialized data structure. Do not modify!
//
struct TEXTURE_21
{
	char	texture[MAX_PATH];
	float	rotate;
	float	shift[2];
	float	scale[2];
	BYTE	smooth;
	BYTE	material;
	DWORD	q2surface;
	DWORD	q2contents;
	DWORD	q2value;
};

//
// Post 2.1 explicit texture U/V axes were added.
//
// Serialized data structure. Do not modify!
//
struct TEXTURE_22
{
	char texture[MAX_PATH];
	float UAxis[4];				// Must remain float[4] for RMF serialization.
	float VAxis[4];				// Must remain float[4] for RMF serialization.
	float rotate;
	float scale[2];
	BYTE smooth;
	BYTE material;
	DWORD q2surface;
	DWORD q2contents;
	DWORD q2value;
};

struct TEXTURE
{
	char texture[MAX_PATH];
	Vector4D UAxis;
	Vector4D VAxis;
	float rotate;
	float scale[2];
	BYTE smooth;
	BYTE material;
	DWORD q2surface;
	DWORD q2contents;
	DWORD	 q2value;

	TEXTURE& operator=( TEXTURE const& src )
	{
		// necessary since operator= is private for UAxis
		memcpy( this, &src, sizeof(TEXTURE) );
		return *this;
	}
};

class CMapFace : public CMapAtom
{
	public:

		CMapFace(void);
		~CMapFace(void);

		void SetTexture(const char *pszNewTex);
		void SetTexture(IEditorTexture *pTexture);
		void GetTextureName(char *pszName) const;

		inline IEditorTexture *GetTexture(void) const;

		//
		// Serialization.
		//
		int SerializeRMF( fstream&, BOOL );
		int SerializeMAP(fstream&, BOOL);

		BOOL CheckFace(CCheckFaceInfo* = NULL);
		BOOL Fix(void);

		float GetNormalDistance(Vector& fPoint);

		inline int GetPointCount(void);
		inline void GetPoint(Vector& Point, int nPoint);

		inline void GetTexCoord( Vector2D& TexCoord, int nTexCoord );

		// Texture alignment.
		void GetCenter(Vector& Center);
		FaceOrientation_t GetOrientation(void) const;
		void RotateTextureAxes(float fDegrees);
		void InitializeTextureAxes(TextureAlignment_t eAlignment, DWORD dwFlags);
		void JustifyTexture(TextureJustification_t eJustification);
		void JustifyTextureUsingExtents(TextureJustification_t eJustification, Extents_t Extents);
		void GetFaceBounds(Vector& pfMins, Vector& pfMaxs) const;
		void GetFaceExtents(Extents_t Extents) const;
		void GetTextureExtents(Extents_t Extents, Vector2D & TopLeft, Vector2D & BottomRight) const;
		int GetTextureAlignment(void) const;
		void GetFaceTextureExtents(Vector2D & TopLeft, Vector2D & BottomRight) const;

		void CalcTextureCoordAtPoint( const Vector& pt, Vector2D& texCoord );

		void NormalizeTextureShifts(void);
		BOOL IsTextureAxisValid(void) const;

		// Old code for setting up texture axes. Needed for backwards compatibility.
		void InitializeQuakeStyleTextureAxes(Vector4D& UAxis, Vector4D& VAxis);

		void CreateFace(Vector *pPoints, int nPoints);
		void CreateFace(winding_t *w, int nFlags = 0);
		CMapFace *CopyFrom(const CMapFace *pFrom, DWORD dwFlags = COPY_FACE_POINTS, bool bUpdateDependencies = true );
		size_t AllocatePoints(int nPoints);

		void CalcPlane(void);
		void CalcPlaneFromFacePoints(void);

		void CalcTextureCoords();
		void OffsetTexture(const Vector &Delta);
		void SetTextureCoords(int nPoint, float u, float v);

		void Render3D( CRender3D* pRender );
		void Render3DGrid(CRender3D *pRender);

		static void SetShowSelection(bool bShowSelection);

		inline BOOL IsIgnored(void) const { return(m_bIgnored); }
		inline void SetIgnored(BOOL bIgnored) { m_bIgnored = bIgnored; }

		inline void SetRenderAlpha(unsigned char uchAlpha) { m_uchAlpha = uchAlpha; } // HACK: should be in CMapAtom

		inline void GetFaceNormal( Vector& normal );
		bool TraceLine(Vector &HitPos, Vector &HitNormal, Vector const &Start, Vector const &End);
		bool TraceLineInside( Vector &HitPos, Vector &HitNormal, Vector const &Start, Vector const &End, bool bNoDisp = false );

		void GetDownVector( int index, Vector& downVect );

		bool GetRender2DBox( Vector& boundMin, Vector& boundMax );
		bool GetCullBox( Vector& boundMin, Vector& boundMax );
		size_t GetDataSize( void );

		inline int GetFaceID(void);
		inline void SetFaceID(int nFaceID);

		// Indicates this guy should be unlit
		void RenderUnlit( bool enable );

		// (begin serialized information
		TEXTURE texture;					// Texture info.
		Vector *Points;						// Array of face points, dynamically allocated.
		int	nPoints;						// The number of points in the array.
		// end serialized information)

		PLANE plane;

	protected:

		void RenderGridIfCloseEnough( CRender3D* pRender, Vector& viewPoint );
		void RenderTextureAxes( CRender3D* pRender );

		//
		// Serialization (chunk handlers).
		//

		unsigned char m_uchAlpha;			// HACK: should be in CMapAtom

		int m_nFaceID;						// The unique ID of this face in the world.

		IEditorTexture *m_pTexture;				// Texture that is applied to this face.

		static bool m_bShowFaceSelection;	// Whether to render faces with a special color when they are selected.

		Vector2D *m_pTextureCoords;			// An array of texture coordinates, one per face point.

		BOOL m_bIgnored;

		// should this be affected by lighting?
		bool m_bIgnoreLighting;
};

//-----------------------------------------------------------------------------
// Purpose: Returns the unique ID of this face.
//-----------------------------------------------------------------------------
inline int CMapFace::GetFaceID(void)
{
	return(m_nFaceID);
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : TexCoord -
//			nTexCoord -
//-----------------------------------------------------------------------------
inline void CMapFace::GetTexCoord( Vector2D& TexCoord, int nTexCoord )
{
    ASSERT( nTexCoord < nPoints );
    TexCoord[0] = m_pTextureCoords[nTexCoord][0];
    TexCoord[1] = m_pTextureCoords[nTexCoord][1];
}

//-----------------------------------------------------------------------------
// Purpose: Returns a pointer to the texture that is applied to this face, NULL if none.
//-----------------------------------------------------------------------------
inline IEditorTexture *CMapFace::GetTexture(void) const
{
	return(m_pTexture);
}

//-----------------------------------------------------------------------------
// Purpose: Returns the number of vertices that define this face.
//-----------------------------------------------------------------------------
inline int CMapFace::GetPointCount(void)
{
	return(nPoints);
}

//-----------------------------------------------------------------------------
// Purpose: Retrieves a point on this face by its index.
// Input  : Point - Receives point coordinates.
//			nPoint - Index of point to retrieve.
//-----------------------------------------------------------------------------
inline void CMapFace::GetPoint(Vector& Point, int nPoint)
{
	ASSERT(nPoint < nPoints);
	Point = Points[nPoint];
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CMapFace::GetFaceNormal( Vector& normal )
{
	normal = plane.normal;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the unique ID of this face.
//-----------------------------------------------------------------------------
inline void CMapFace::SetFaceID(int nID)
{
	m_nFaceID = nID;
}

//-----------------------------------------------------------------------------
// Defines a container class for a list of face pointers.
//-----------------------------------------------------------------------------
class CMapFaceList : public CUtlVector<CMapFace *>
{
public:

	inline CMapFaceList(void) {}
	inline CMapFaceList(CMapFaceList const &other);
	inline CMapFaceList &CMapFaceList::operator =(CMapFaceList const &other);

	inline int FindFaceID(int nFaceID);
	void Intersect(CMapFaceList &IntersectWith, CMapFaceList &In, CMapFaceList &Out);
};

//-----------------------------------------------------------------------------
// Purpose: Copy constructor.
//-----------------------------------------------------------------------------
CMapFaceList::CMapFaceList(CMapFaceList const &other)
{
	*this = other;
}

//-----------------------------------------------------------------------------
// Purpose: Assignment operator for copying face lists.
// Input  : other -
//-----------------------------------------------------------------------------
CMapFaceList &CMapFaceList::operator =(CMapFaceList const &other)
{
	AddVectorToTail(other);
	return *this;
}

//-----------------------------------------------------------------------------
// Purpose: Searches the list for a face with the given ID.
// Input  : nFaceID - Numeric face ID to search for.
// Output : Index of found element, -1 if none.
//-----------------------------------------------------------------------------
int CMapFaceList::FindFaceID(int nFaceID)
{
	for (int i = 0; i < Count(); i++)
	{
		if ((Element(i) != NULL) && (Element(i)->GetFaceID() == nFaceID))
		{
			return(i);
		}
	}

	return(-1);
}

//-----------------------------------------------------------------------------
// Defines a container class for a list of face IDs.
//-----------------------------------------------------------------------------
class CMapFaceIDList : public CUtlVector<int>
{
public:

	inline CMapFaceIDList(void) {}
	inline CMapFaceIDList(CMapFaceIDList const &other);
	inline CMapFaceIDList &CMapFaceIDList::operator =(CMapFaceIDList const &other);

	void Intersect(CMapFaceIDList &IntersectWith, CMapFaceIDList &In, CMapFaceIDList &Out);
};

//-----------------------------------------------------------------------------
// Purpose: Copy constructor.
//-----------------------------------------------------------------------------
CMapFaceIDList::CMapFaceIDList(CMapFaceIDList const &other)
{
	*this = other;
}

//-----------------------------------------------------------------------------
// Purpose: Assignment operator for copying face ID lists.
// Input  : other -
//-----------------------------------------------------------------------------
CMapFaceIDList &CMapFaceIDList::operator =(CMapFaceIDList const &other)
{
	AddVectorToTail(other);
	return *this;
}

#endif // MAPFACE_H
