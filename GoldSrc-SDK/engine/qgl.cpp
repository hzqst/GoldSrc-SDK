#include "filesystem.h"
#include "const.h"
#include "../external/SDL2/SDL_opengl.h"
#include "glquake.h"
#include "../external/SDL2/SDL_video.h"

#include <cstdio>
#include <ctime>

typedef struct glwstate_s
{
	HINSTANCE hInstance;
	void* wndproc;
	HINSTANCE hinstOpenGL;
	qboolean minidriver;
	qboolean allowdisplaydepthchange;
	qboolean mcd_accelerated;
	FileHandle_t log_fp;
} glwstate_t;

glwstate_t glw_state;

void( APIENTRY* dllAccum )( GLenum op, GLfloat value );
void( APIENTRY* dllAlphaFunc )( GLenum func, GLclampf ref );
GLboolean( APIENTRY* dllAreTexturesResident )( GLsizei n, const GLuint* textures, GLboolean* residences );
void( APIENTRY* dllArrayElement )( GLint i );
void( APIENTRY* dllBegin )( GLenum mode );
void( APIENTRY* dllBindTexture )( GLenum target, GLuint texture );
void( APIENTRY* dllBitmap )( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove,
	const GLubyte* bitmap );
void( APIENTRY* dllBlendFunc )( GLenum sfactor, GLenum dfactor );
void( APIENTRY* dllCallList )( GLuint list );
void( APIENTRY* dllCallLists )( GLsizei n, GLenum type, const void* lists );
void( APIENTRY* dllClear )( GLbitfield mask );
void( APIENTRY* dllClearAccum )( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
void( APIENTRY* dllClearColor )( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
void( APIENTRY* dllClearDepth )( GLclampd depth );
void( APIENTRY* dllClearIndex )( GLfloat color );
void( APIENTRY* dllClearStencil )( GLint stencil );
void( APIENTRY* dllClipPlane )( GLenum plane, const GLdouble* equation );
void( APIENTRY* dllColor3b )( GLbyte red, GLbyte green, GLbyte blue );
void( APIENTRY* dllColor3bv )( const GLbyte* color );
void( APIENTRY* dllColor3d )( GLdouble red, GLdouble green, GLdouble blue );
void( APIENTRY* dllColor3dv )( const GLdouble* color );
void( APIENTRY* dllColor3f )( GLfloat red, GLfloat green, GLfloat blue );
void( APIENTRY* dllColor3fv )( const GLfloat* color );
void( APIENTRY* dllColor3i )( GLint red, GLint green, GLint blue );
void( APIENTRY* dllColor3iv )( const GLint* color );
void( APIENTRY* dllColor3s )( GLshort red, GLshort green, GLshort blue );
void( APIENTRY* dllColor3sv )( const GLshort* color );
void( APIENTRY* dllColor3ub )( GLubyte red, GLubyte green, GLubyte blue );
void( APIENTRY* dllColor3ubv )( const GLubyte* color );
void( APIENTRY* dllColor3ui )( GLuint red, GLuint green, GLuint blue );
void( APIENTRY* dllColor3uiv )( const GLuint* color );
void( APIENTRY* dllColor3us )( GLushort red, GLushort green, GLushort blue );
void( APIENTRY* dllColor3usv )( const GLushort* color );
void( APIENTRY* dllColor4b )( GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha );
void( APIENTRY* dllColor4bv )( const GLbyte* color );
void( APIENTRY* dllColor4d )( GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha );
void( APIENTRY* dllColor4dv )( const GLdouble* color );
void( APIENTRY* dllColor4f )( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
void( APIENTRY* dllColor4fv )( const GLfloat* color );
void( APIENTRY* dllColor4i )( GLint red, GLint green, GLint blue, GLint alpha );
void( APIENTRY* dllColor4iv )( const GLint* color );
void( APIENTRY* dllColor4s )( GLshort red, GLshort green, GLshort blue, GLshort alpha );
void( APIENTRY* dllColor4sv )( const GLshort* color );
void( APIENTRY* dllColor4ub )( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
void( APIENTRY* dllColor4ubv )( const GLubyte* color );
void( APIENTRY* dllColor4ui )( GLuint red, GLuint green, GLuint blue, GLuint alpha );
void( APIENTRY* dllColor4uiv )( const GLuint* color );
void( APIENTRY* dllColor4us )( GLushort red, GLushort green, GLushort blue, GLushort alpha );
void( APIENTRY* dllColor4usv )( const GLushort* color );
void( APIENTRY* dllColorMask )( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha );
void( APIENTRY* dllColorMaterial )( GLenum face, GLenum mode );
void( APIENTRY* dllColorPointer )( GLint size, GLenum type, GLsizei stride, const void* pointer );
void( APIENTRY* dllCopyPixels )( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type );
void( APIENTRY* dllCopyTexImage1D )( GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width,
	GLint border );
void( APIENTRY* dllCopyTexImage2D )( GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width,
	GLsizei height, GLint border );
void( APIENTRY* dllCopyTexSubImage1D )( GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width );
void( APIENTRY* dllCopyTexSubImage2D )( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
	GLsizei width, GLsizei height );
void( APIENTRY* dllCullFace )( GLenum mode );
void( APIENTRY* dllDeleteLists )( GLuint list, GLsizei range );
void( APIENTRY* dllDeleteTextures )( GLsizei n, const GLuint* textures );
void( APIENTRY* dllDepthFunc )( GLenum func );
void( APIENTRY* dllDepthMask )( GLboolean flag );
void( APIENTRY* dllDepthRange )( GLclampd zNear, GLclampd zFar );
void( APIENTRY* dllDisable )( GLenum cap );
void( APIENTRY* dllDisableClientState )( GLenum array );
void( APIENTRY* dllDrawArrays )( GLenum mode, GLint first, GLsizei count );
void( APIENTRY* dllDrawBuffer )( GLenum mode );
void( APIENTRY* dllDrawElements )( GLenum mode, GLsizei count, GLenum type, const void* indices );
void( APIENTRY* dllDrawPixels )( GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels );
void( APIENTRY* dllEdgeFlag )( GLboolean flag );
void( APIENTRY* dllEdgeFlagPointer )( GLsizei stride, const void* pointer );
void( APIENTRY* dllEdgeFlagv )( const GLboolean* flag );
void( APIENTRY* dllEnable )( GLenum cap );
void( APIENTRY* dllEnableClientState )( GLenum array );
void( APIENTRY* dllEnd )();
void( APIENTRY* dllEndList )();
void( APIENTRY* dllEvalCoord1d )( GLdouble u );
void( APIENTRY* dllEvalCoord1dv )( const GLdouble* u );
void( APIENTRY* dllEvalCoord1f )( GLfloat u );
void( APIENTRY* dllEvalCoord1fv )( const GLfloat* u );
void( APIENTRY* dllEvalCoord2d )( GLdouble u, GLdouble v );
void( APIENTRY* dllEvalCoord2dv )( const GLdouble* u );
void( APIENTRY* dllEvalCoord2f )( GLfloat u, GLfloat v );
void( APIENTRY* dllEvalCoord2fv )( const GLfloat* u );
void( APIENTRY* dllEvalMesh1 )( GLenum mode, GLint i1, GLint i2 );
void( APIENTRY* dllEvalMesh2 )( GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2 );
void( APIENTRY* dllEvalPoint1 )( GLint i );
void( APIENTRY* dllEvalPoint2 )( GLint i, GLint j );
void( APIENTRY* dllFeedbackBuffer )( GLsizei size, GLenum type, GLfloat* buffer );
void( APIENTRY* dllFinish )();
void( APIENTRY* dllFlush )();
void( APIENTRY* dllFogf )( GLenum pname, GLfloat param );
void( APIENTRY* dllFogfv )( GLenum pname, const GLfloat* params );
void( APIENTRY* dllFogi )( GLenum pname, GLint param );
void( APIENTRY* dllFogiv )( GLenum pname, const GLint* params );
void( APIENTRY* dllFrontFace )( GLenum mode );
void( APIENTRY* dllFrustum )( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear,
	GLdouble zFar );
GLuint( APIENTRY* dllGenLists )( GLsizei range );
void( APIENTRY* dllGenTextures )( GLsizei n, GLuint* textures );
void( APIENTRY* dllGetBooleanv )( GLenum pname, GLboolean* params );
void( APIENTRY* dllGetClipPlane )( GLenum plane, GLdouble* equation );
void( APIENTRY* dllGetDoublev )( GLenum pname, GLdouble* params );
GLenum( APIENTRY* dllGetError )();
void( APIENTRY* dllGetFloatv )( GLenum pname, GLfloat* params );
void( APIENTRY* dllGetIntegerv )( GLenum pname, GLint* params );
void( APIENTRY* dllGetLightfv )( GLenum light, GLenum pname, GLfloat* params );
void( APIENTRY* dllGetLightiv )( GLenum light, GLenum pname, GLint* params );
void( APIENTRY* dllGetMapdv )( GLenum target, GLenum query, GLdouble* v );
void( APIENTRY* dllGetMapfv )( GLenum target, GLenum query, GLfloat* v );
void( APIENTRY* dllGetMapiv )( GLenum target, GLenum query, GLint* v );
void( APIENTRY* dllGetMaterialfv )( GLenum face, GLenum pname, GLfloat* params );
void( APIENTRY* dllGetMaterialiv )( GLenum face, GLenum pname, GLint* params );
void( APIENTRY* dllGetPixelMapfv )( GLenum map, GLfloat* values );
void( APIENTRY* dllGetPixelMapuiv )( GLenum map, GLuint* values );
void( APIENTRY* dllGetPixelMapusv )( GLenum map, GLushort* values );
void( APIENTRY* dllGetPointerv )( GLenum pname, void** params );
void( APIENTRY* dllGetPolygonStipple )( GLubyte* mask );
const GLubyte*( APIENTRY* dllGetString )( GLenum name );
void( APIENTRY* dllGetTexEnvfv )( GLenum target, GLenum pname, GLfloat* params );
void( APIENTRY* dllGetTexEnviv )( GLenum target, GLenum pname, GLint* params );
void( APIENTRY* dllGetTexGendv )( GLenum coord, GLenum pname, GLdouble* params );
void( APIENTRY* dllGetTexGenfv )( GLenum coord, GLenum pname, GLfloat* params );
void( APIENTRY* dllGetTexGeniv )( GLenum coord, GLenum pname, GLint* params );
void( APIENTRY* dllGetTexImage )( GLenum target, GLint level, GLenum format, GLenum type, void* pixels );
void( APIENTRY* dllGetTexLevelParameterfv )( GLenum target, GLint level, GLenum pname, GLfloat* params );
void( APIENTRY* dllGetTexLevelParameteriv )( GLenum target, GLint level, GLenum pname, GLint* params );
void( APIENTRY* dllGetTexParameterfv )( GLenum target, GLenum pname, GLfloat* params );
void( APIENTRY* dllGetTexParameteriv )( GLenum target, GLenum pname, GLint* params );
void( APIENTRY* dllHint )( GLenum target, GLenum mode );
void( APIENTRY* dllIndexMask )( GLuint mask );
void( APIENTRY* dllIndexPointer )( GLenum type, GLsizei stride, const void* pointer );
void( APIENTRY* dllIndexd )( GLdouble c );
void( APIENTRY* dllIndexdv )( const GLdouble* c );
void( APIENTRY* dllIndexf )( GLfloat c );
void( APIENTRY* dllIndexfv )( const GLfloat* c );
void( APIENTRY* dllIndexi )( GLint c );
void( APIENTRY* dllIndexiv )( const GLint* c );
void( APIENTRY* dllIndexs )( GLshort c );
void( APIENTRY* dllIndexsv )( const GLshort* c );
void( APIENTRY* dllIndexub )( GLubyte c );
void( APIENTRY* dllIndexubv )( const GLubyte* c );
void( APIENTRY* dllInitNames )();
void( APIENTRY* dllInterleavedArrays )( GLenum format, GLsizei stride, const void* pointer );
GLboolean( APIENTRY* dllIsEnabled )( GLenum cap );
GLboolean( APIENTRY* dllIsList )( GLuint list );
GLboolean( APIENTRY* dllIsTexture )( GLuint texture );
void( APIENTRY* dllLightModelf )( GLenum pname, GLfloat param );
void( APIENTRY* dllLightModelfv )( GLenum pname, const GLfloat* params );
void( APIENTRY* dllLightModeli )( GLenum pname, GLint param );
void( APIENTRY* dllLightModeliv )( GLenum pname, const GLint* params );
void( APIENTRY* dllLightf )( GLenum light, GLenum pname, GLfloat param );
void( APIENTRY* dllLightfv )( GLenum light, GLenum pname, const GLfloat* params );
void( APIENTRY* dllLighti )( GLenum light, GLenum pname, GLint param );
void( APIENTRY* dllLightiv )( GLenum light, GLenum pname, const GLint* params );
void( APIENTRY* dllLineStipple )( GLint factor, GLushort pattern );
void( APIENTRY* dllLineWidth )( GLfloat width );
void( APIENTRY* dllListBase )( GLuint base );
void( APIENTRY* dllLoadIdentity )();
void( APIENTRY* dllLoadMatrixd )( const GLdouble* m );
void( APIENTRY* dllLoadMatrixf )( const GLfloat* m );
void( APIENTRY* dllLoadName )( GLuint name );
void( APIENTRY* dllLogicOp )( GLenum opcode );
void( APIENTRY* dllMap1d )( GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
	const GLdouble* points );
void( APIENTRY* dllMap1f )( GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat* points );
void( APIENTRY* dllMap2d )( GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1,
	GLdouble v2, GLint vstride, GLint vorder, const GLdouble* points );
void( APIENTRY* dllMap2f )( GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2,
	GLint vstride, GLint vorder, const GLfloat* points );
void( APIENTRY* dllMapGrid1d )( GLint un, GLdouble u1, GLdouble u2 );
void( APIENTRY* dllMapGrid1f )( GLint un, GLfloat u1, GLfloat u2 );
void( APIENTRY* dllMapGrid2d )( GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2 );
void( APIENTRY* dllMapGrid2f )( GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2 );
void( APIENTRY* dllMaterialf )( GLenum face, GLenum pname, GLfloat param );
void( APIENTRY* dllMaterialfv )( GLenum face, GLenum pname, const GLfloat* params );
void( APIENTRY* dllMateriali )( GLenum face, GLenum pname, GLint param );
void( APIENTRY* dllMaterialiv )( GLenum face, GLenum pname, const GLint* params );
void( APIENTRY* dllMatrixMode )( GLenum mode );
void( APIENTRY* dllMultMatrixd )( const GLdouble* m );
void( APIENTRY* dllMultMatrixf )( const GLfloat* m );
void( APIENTRY* dllNewList )( GLuint list, GLenum mode );
void( APIENTRY* dllNormal3b )( GLbyte nx, GLbyte ny, GLbyte nz );
void( APIENTRY* dllNormal3bv )( const GLbyte* v );
void( APIENTRY* dllNormal3d )( GLdouble nx, GLdouble ny, GLdouble nz );
void( APIENTRY* dllNormal3dv )( const GLdouble* v );
void( APIENTRY* dllNormal3f )( GLfloat nx, GLfloat ny, GLfloat nz );
void( APIENTRY* dllNormal3fv )( const GLfloat* v );
void( APIENTRY* dllNormal3i )( GLint nx, GLint ny, GLint nz );
void( APIENTRY* dllNormal3iv )( const GLint* v );
void( APIENTRY* dllNormal3s )( GLshort nx, GLshort ny, GLshort nz );
void( APIENTRY* dllNormal3sv )( const GLshort* v );
void( APIENTRY* dllNormalPointer )( GLenum type, GLsizei stride, const void* pointer );
void( APIENTRY* dllOrtho )( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear,
	GLdouble zFar );
void( APIENTRY* dllPassThrough )( GLfloat token );
void( APIENTRY* dllPixelMapfv )( GLenum map, GLsizei mapsize, const GLfloat* values );
void( APIENTRY* dllPixelMapuiv )( GLenum map, GLsizei mapsize, const GLuint* values );
void( APIENTRY* dllPixelMapusv )( GLenum map, GLsizei mapsize, const GLushort* values );
void( APIENTRY* dllPixelStoref )( GLenum pname, GLfloat param );
void( APIENTRY* dllPixelStorei )( GLenum pname, GLint param );
void( APIENTRY* dllPixelTransferf )( GLenum pname, GLfloat param );
void( APIENTRY* dllPixelTransferi )( GLenum pname, GLint param );
void( APIENTRY* dllPixelZoom )( GLfloat xfactor, GLfloat yfactor );
void( APIENTRY* dllPointSize )( GLfloat size );
void( APIENTRY* dllPolygonMode )( GLenum face, GLenum mode );
void( APIENTRY* dllPolygonOffset )( GLfloat factor, GLfloat units );
void( APIENTRY* dllPolygonStipple )( const GLubyte* mask );
void( APIENTRY* dllPopAttrib )();
void( APIENTRY* dllPopClientAttrib )();
void( APIENTRY* dllPopMatrix )();
void( APIENTRY* dllPopName )();
void( APIENTRY* dllPrioritizeTextures )( GLsizei n, const GLuint* textures, const GLclampf* priorities );
void( APIENTRY* dllPushAttrib )( GLbitfield mask );
void( APIENTRY* dllPushClientAttrib )( GLbitfield mask );
void( APIENTRY* dllPushMatrix )();
void( APIENTRY* dllPushName )( GLuint name );
void( APIENTRY* dllRasterPos2d )( GLdouble x, GLdouble y );
void( APIENTRY* dllRasterPos2dv )( const GLdouble* v );
void( APIENTRY* dllRasterPos2f )( GLfloat x, GLfloat y );
void( APIENTRY* dllRasterPos2fv )( const GLfloat* v );
void( APIENTRY* dllRasterPos2i )( GLint x, GLint y );
void( APIENTRY* dllRasterPos2iv )( const GLint* v );
void( APIENTRY* dllRasterPos2s )( GLshort x, GLshort y );
void( APIENTRY* dllRasterPos2sv )( const GLshort* v );
void( APIENTRY* dllRasterPos3d )( GLdouble x, GLdouble y, GLdouble z );
void( APIENTRY* dllRasterPos3dv )( const GLdouble* v );
void( APIENTRY* dllRasterPos3f )( GLfloat x, GLfloat y, GLfloat z );
void( APIENTRY* dllRasterPos3fv )( const GLfloat* v );
void( APIENTRY* dllRasterPos3i )( GLint x, GLint y, GLint z );
void( APIENTRY* dllRasterPos3iv )( const GLint* v );
void( APIENTRY* dllRasterPos3s )( GLshort x, GLshort y, GLshort z );
void( APIENTRY* dllRasterPos3sv )( const GLshort* v );
void( APIENTRY* dllRasterPos4d )( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
void( APIENTRY* dllRasterPos4dv )( const GLdouble* v );
void( APIENTRY* dllRasterPos4f )( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
void( APIENTRY* dllRasterPos4fv )( const GLfloat* v );
void( APIENTRY* dllRasterPos4i )( GLint x, GLint y, GLint z, GLint w );
void( APIENTRY* dllRasterPos4iv )( const GLint* v );
void( APIENTRY* dllRasterPos4s )( GLshort x, GLshort y, GLshort z, GLshort w );
void( APIENTRY* dllRasterPos4sv )( const GLshort* v );
void( APIENTRY* dllReadBuffer )( GLenum mode );
void( APIENTRY* dllReadPixels )( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
	void* pixels );
void( APIENTRY* dllRectd )( GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2 );
void( APIENTRY* dllRectdv )( const GLdouble* v1, const GLdouble* v2 );
void( APIENTRY* dllRectf )( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 );
void( APIENTRY* dllRectfv )( const GLfloat* v1, const GLfloat* v2 );
void( APIENTRY* dllRecti )( GLint x1, GLint y1, GLint x2, GLint y2 );
void( APIENTRY* dllRectiv )( const GLint* v1, const GLint* v2 );
void( APIENTRY* dllRects )( GLshort x1, GLshort y1, GLshort x2, GLshort y2 );
void( APIENTRY* dllRectsv )( const GLshort* v1, const GLshort* v2 );
GLint( APIENTRY* dllRenderMode )( GLenum mode );
void( APIENTRY* dllRotated )( GLdouble angle, GLdouble x, GLdouble y, GLdouble z );
void( APIENTRY* dllRotatef )( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
void( APIENTRY* dllScaled )( GLdouble x, GLdouble y, GLdouble z );
void( APIENTRY* dllScalef )( GLfloat x, GLfloat y, GLfloat z );
void( APIENTRY* dllScissor )( GLint x, GLint y, GLsizei width, GLsizei height );
void( APIENTRY* dllSelectBuffer )( GLsizei size, GLuint* buffer );
void( APIENTRY* dllShadeModel )( GLenum mode );
void( APIENTRY* dllStencilFunc )( GLenum func, GLint ref, GLuint mask );
void( APIENTRY* dllStencilMask )( GLuint mask );
void( APIENTRY* dllStencilOp )( GLenum fail, GLenum zfail, GLenum zpass );
void( APIENTRY* dllTexCoord1d )( GLdouble s );
void( APIENTRY* dllTexCoord1dv )( const GLdouble* v );
void( APIENTRY* dllTexCoord1f )( GLfloat s );
void( APIENTRY* dllTexCoord1fv )( const GLfloat* v );
void( APIENTRY* dllTexCoord1i )( GLint s );
void( APIENTRY* dllTexCoord1iv )( const GLint* v );
void( APIENTRY* dllTexCoord1s )( GLshort s );
void( APIENTRY* dllTexCoord1sv )( const GLshort* v );
void( APIENTRY* dllTexCoord2d )( GLdouble s, GLdouble t );
void( APIENTRY* dllTexCoord2dv )( const GLdouble* v );
void( APIENTRY* dllTexCoord2f )( GLfloat s, GLfloat t );
void( APIENTRY* dllTexCoord2fv )( const GLfloat* v );
void( APIENTRY* dllTexCoord2i )( GLint s, GLint t );
void( APIENTRY* dllTexCoord2iv )( const GLint* v );
void( APIENTRY* dllTexCoord2s )( GLshort s, GLshort t );
void( APIENTRY* dllTexCoord2sv )( const GLshort* v );
void( APIENTRY* dllTexCoord3d )( GLdouble s, GLdouble t, GLdouble r );
void( APIENTRY* dllTexCoord3dv )( const GLdouble* v );
void( APIENTRY* dllTexCoord3f )( GLfloat s, GLfloat t, GLfloat r );
void( APIENTRY* dllTexCoord3fv )( const GLfloat* v );
void( APIENTRY* dllTexCoord3i )( GLint s, GLint t, GLint r );
void( APIENTRY* dllTexCoord3iv )( const GLint* v );
void( APIENTRY* dllTexCoord3s )( GLshort s, GLshort t, GLshort r );
void( APIENTRY* dllTexCoord3sv )( const GLshort* v );
void( APIENTRY* dllTexCoord4d )( GLdouble s, GLdouble t, GLdouble r, GLdouble q );
void( APIENTRY* dllTexCoord4dv )( const GLdouble* v );
void( APIENTRY* dllTexCoord4f )( GLfloat s, GLfloat t, GLfloat r, GLfloat q );
void( APIENTRY* dllTexCoord4fv )( const GLfloat* v );
void( APIENTRY* dllTexCoord4i )( GLint s, GLint t, GLint r, GLint q );
void( APIENTRY* dllTexCoord4iv )( const GLint* v );
void( APIENTRY* dllTexCoord4s )( GLshort s, GLshort t, GLshort r, GLshort q );
void( APIENTRY* dllTexCoord4sv )( const GLshort* v );
void( APIENTRY* dllTexCoordPointer )( GLint size, GLenum type, GLsizei stride, const void* pointer );
void( APIENTRY* dllTexEnvf )( GLenum target, GLenum pname, GLfloat param );
void( APIENTRY* dllTexEnvfv )( GLenum target, GLenum pname, const GLfloat* params );
void( APIENTRY* dllTexEnvi )( GLenum target, GLenum pname, GLint param );
void( APIENTRY* dllTexEnviv )( GLenum target, GLenum pname, const GLint* params );
void( APIENTRY* dllTexGend )( GLenum coord, GLenum pname, GLdouble param );
void( APIENTRY* dllTexGendv )( GLenum coord, GLenum pname, const GLdouble* params );
void( APIENTRY* dllTexGenf )( GLenum coord, GLenum pname, GLfloat param );
void( APIENTRY* dllTexGenfv )( GLenum coord, GLenum pname, const GLfloat* params );
void( APIENTRY* dllTexGeni )( GLenum coord, GLenum pname, GLint param );
void( APIENTRY* dllTexGeniv )( GLenum coord, GLenum pname, const GLint* params );
void( APIENTRY* dllTexImage1D )( GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border,
	GLenum format, GLenum type, const void* pixels );
void( APIENTRY* dllTexImage2D )( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
	GLint border, GLenum format, GLenum type, const void* pixels );
void( APIENTRY* dllTexParameterf )( GLenum target, GLenum pname, GLfloat param );
void( APIENTRY* dllTexParameterfv )( GLenum target, GLenum pname, const GLfloat* params );
void( APIENTRY* dllTexParameteri )( GLenum target, GLenum pname, GLint param );
void( APIENTRY* dllTexParameteriv )( GLenum target, GLenum pname, const GLint* params );
void( APIENTRY* dllTexSubImage1D )( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format,
	GLenum type, const void* pixels );
void( APIENTRY* dllTexSubImage2D )( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
	GLsizei height, GLenum format, GLenum type, const void* pixels );
void( APIENTRY* dllTranslated )( GLdouble x, GLdouble y, GLdouble z );
void( APIENTRY* dllTranslatef )( GLfloat x, GLfloat y, GLfloat z );
void( APIENTRY* dllVertex2d )( GLdouble x, GLdouble y );
void( APIENTRY* dllVertex2dv )( const GLdouble* v );
void( APIENTRY* dllVertex2f )( GLfloat x, GLfloat y );
void( APIENTRY* dllVertex2fv )( const GLfloat* v );
void( APIENTRY* dllVertex2i )( GLint x, GLint y );
void( APIENTRY* dllVertex2iv )( const GLint* v );
void( APIENTRY* dllVertex2s )( GLshort x, GLshort y );
void( APIENTRY* dllVertex2sv )( const GLshort* v );
void( APIENTRY* dllVertex3d )( GLdouble x, GLdouble y, GLdouble z );
void( APIENTRY* dllVertex3dv )( const GLdouble* v );
void( APIENTRY* dllVertex3f )( GLfloat x, GLfloat y, GLfloat z );
void( APIENTRY* dllVertex3fv )( const GLfloat* v );
void( APIENTRY* dllVertex3i )( GLint x, GLint y, GLint z );
void( APIENTRY* dllVertex3iv )( const GLint* v );
void( APIENTRY* dllVertex3s )( GLshort x, GLshort y, GLshort z );
void( APIENTRY* dllVertex3sv )( const GLshort* v );
void( APIENTRY* dllVertex4d )( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
void( APIENTRY* dllVertex4dv )( const GLdouble* v );
void( APIENTRY* dllVertex4f )( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
void( APIENTRY* dllVertex4fv )( const GLfloat* v );
void( APIENTRY* dllVertex4i )( GLint x, GLint y, GLint z, GLint w );
void( APIENTRY* dllVertex4iv )( const GLint* v );
void( APIENTRY* dllVertex4s )( GLshort x, GLshort y, GLshort z, GLshort w );
void( APIENTRY* dllVertex4sv )( const GLshort* v );
void( APIENTRY* dllVertexPointer )( GLint size, GLenum type, GLsizei stride, const void* pointer );
void( APIENTRY* dllViewport )( GLint x, GLint y, GLsizei width, GLsizei height );

GLuint( APIENTRY* dllCreateShader )( GLenum type );
void( APIENTRY* dllDeleteShader )( GLuint shader );
void( APIENTRY* dllShaderSource )( GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length );
void( APIENTRY* dllGetProgramiv )( GLuint program, GLenum pname, GLint* params );
void( APIENTRY* dllGetProgramInfoLog )( GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog );
void( APIENTRY* dllAttachShader )( GLuint program, GLuint shader );
void( APIENTRY* dllLinkProgram )( GLuint program );
GLuint( APIENTRY* dllCreateProgram )();
void( APIENTRY* dllDeleteProgram )( GLuint program );
void( APIENTRY* dllUseProgram )( GLuint program );
void( APIENTRY* dllCompileShader )( GLuint shader );
void( APIENTRY* dllGetShaderiv )( GLuint shader, GLenum pname, GLint* params );
void( APIENTRY* dllGetShaderInfoLog )( GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog );
GLint( APIENTRY* dllGetUniformLocation )( GLuint program, const GLchar* name );
void( APIENTRY* dllUniform1ui )( GLint location, GLuint v0 );

decltype( dllAccum ) qglAccum;
decltype( dllAlphaFunc ) qglAlphaFunc;
decltype( dllAreTexturesResident ) qglAreTexturesResident;
decltype( dllArrayElement ) qglArrayElement;
decltype( dllBegin ) qglBegin;
decltype( dllBindTexture ) qglBindTexture;
decltype( dllBitmap ) qglBitmap;
decltype( dllBlendFunc ) qglBlendFunc;
decltype( dllCallList ) qglCallList;
decltype( dllCallLists ) qglCallLists;
decltype( dllClear ) qglClear;
decltype( dllClearAccum ) qglClearAccum;
decltype( dllClearColor ) qglClearColor;
decltype( dllClearDepth ) qglClearDepth;
decltype( dllClearIndex ) qglClearIndex;
decltype( dllClearStencil ) qglClearStencil;
decltype( dllClipPlane ) qglClipPlane;
decltype( dllColor3b ) qglColor3b;
decltype( dllColor3bv ) qglColor3bv;
decltype( dllColor3d ) qglColor3d;
decltype( dllColor3dv ) qglColor3dv;
decltype( dllColor3f ) qglColor3f;
decltype( dllColor3fv ) qglColor3fv;
decltype( dllColor3i ) qglColor3i;
decltype( dllColor3iv ) qglColor3iv;
decltype( dllColor3s ) qglColor3s;
decltype( dllColor3sv ) qglColor3sv;
decltype( dllColor3ub ) qglColor3ub;
decltype( dllColor3ubv ) qglColor3ubv;
decltype( dllColor3ui ) qglColor3ui;
decltype( dllColor3uiv ) qglColor3uiv;
decltype( dllColor3us ) qglColor3us;
decltype( dllColor3usv ) qglColor3usv;
decltype( dllColor4b ) qglColor4b;
decltype( dllColor4bv ) qglColor4bv;
decltype( dllColor4d ) qglColor4d;
decltype( dllColor4dv ) qglColor4dv;
decltype( dllColor4f ) qglColor4f;
decltype( dllColor4fv ) qglColor4fv;
decltype( dllColor4i ) qglColor4i;
decltype( dllColor4iv ) qglColor4iv;
decltype( dllColor4s ) qglColor4s;
decltype( dllColor4sv ) qglColor4sv;
decltype( dllColor4ub ) qglColor4ub;
decltype( dllColor4ubv ) qglColor4ubv;
decltype( dllColor4ui ) qglColor4ui;
decltype( dllColor4uiv ) qglColor4uiv;
decltype( dllColor4us ) qglColor4us;
decltype( dllColor4usv ) qglColor4usv;
decltype( dllColorMask ) qglColorMask;
decltype( dllColorMaterial ) qglColorMaterial;
decltype( dllColorPointer ) qglColorPointer;
decltype( dllCopyPixels ) qglCopyPixels;
decltype( dllCopyTexImage1D ) qglCopyTexImage1D;
decltype( dllCopyTexImage2D ) qglCopyTexImage2D;
decltype( dllCopyTexSubImage1D ) qglCopyTexSubImage1D;
decltype( dllCopyTexSubImage2D ) qglCopyTexSubImage2D;
decltype( dllCullFace ) qglCullFace;
decltype( dllDeleteLists ) qglDeleteLists;
decltype( dllDeleteTextures ) qglDeleteTextures;
decltype( dllDepthFunc ) qglDepthFunc;
decltype( dllDepthMask ) qglDepthMask;
decltype( dllDepthRange ) qglDepthRange;
decltype( dllDisable ) qglDisable;
decltype( dllDisableClientState ) qglDisableClientState;
decltype( dllDrawArrays ) qglDrawArrays;
decltype( dllDrawBuffer ) qglDrawBuffer;
decltype( dllDrawElements ) qglDrawElements;
decltype( dllDrawPixels ) qglDrawPixels;
decltype( dllEdgeFlag ) qglEdgeFlag;
decltype( dllEdgeFlagPointer ) qglEdgeFlagPointer;
decltype( dllEdgeFlagv ) qglEdgeFlagv;
decltype( dllEnable ) qglEnable;
decltype( dllEnableClientState ) qglEnableClientState;
decltype( dllEnd ) qglEnd;
decltype( dllEndList ) qglEndList;
decltype( dllEvalCoord1d ) qglEvalCoord1d;
decltype( dllEvalCoord1dv ) qglEvalCoord1dv;
decltype( dllEvalCoord1f ) qglEvalCoord1f;
decltype( dllEvalCoord1fv ) qglEvalCoord1fv;
decltype( dllEvalCoord2d ) qglEvalCoord2d;
decltype( dllEvalCoord2dv ) qglEvalCoord2dv;
decltype( dllEvalCoord2f ) qglEvalCoord2f;
decltype( dllEvalCoord2fv ) qglEvalCoord2fv;
decltype( dllEvalMesh1 ) qglEvalMesh1;
decltype( dllEvalMesh2 ) qglEvalMesh2;
decltype( dllEvalPoint1 ) qglEvalPoint1;
decltype( dllEvalPoint2 ) qglEvalPoint2;
decltype( dllFeedbackBuffer ) qglFeedbackBuffer;
decltype( dllFinish ) qglFinish;
decltype( dllFlush ) qglFlush;
decltype( dllFogf ) qglFogf;
decltype( dllFogfv ) qglFogfv;
decltype( dllFogi ) qglFogi;
decltype( dllFogiv ) qglFogiv;
decltype( dllFrontFace ) qglFrontFace;
decltype( dllFrustum ) qglFrustum;
decltype( dllGenLists ) qglGenLists;
decltype( dllGenTextures ) qglGenTextures;
decltype( dllGetBooleanv ) qglGetBooleanv;
decltype( dllGetClipPlane ) qglGetClipPlane;
decltype( dllGetDoublev ) qglGetDoublev;
decltype( dllGetError ) qglGetError;
decltype( dllGetFloatv ) qglGetFloatv;
decltype( dllGetIntegerv ) qglGetIntegerv;
decltype( dllGetLightfv ) qglGetLightfv;
decltype( dllGetLightiv ) qglGetLightiv;
decltype( dllGetMapdv ) qglGetMapdv;
decltype( dllGetMapfv ) qglGetMapfv;
decltype( dllGetMapiv ) qglGetMapiv;
decltype( dllGetMaterialfv ) qglGetMaterialfv;
decltype( dllGetMaterialiv ) qglGetMaterialiv;
decltype( dllGetPixelMapfv ) qglGetPixelMapfv;
decltype( dllGetPixelMapuiv ) qglGetPixelMapuiv;
decltype( dllGetPixelMapusv ) qglGetPixelMapusv;
decltype( dllGetPointerv ) qglGetPointerv;
decltype( dllGetPolygonStipple ) qglGetPolygonStipple;
decltype( dllGetString ) qglGetString;
decltype( dllGetTexEnvfv ) qglGetTexEnvfv;
decltype( dllGetTexEnviv ) qglGetTexEnviv;
decltype( dllGetTexGendv ) qglGetTexGendv;
decltype( dllGetTexGenfv ) qglGetTexGenfv;
decltype( dllGetTexGeniv ) qglGetTexGeniv;
decltype( dllGetTexImage ) qglGetTexImage;
decltype( dllGetTexLevelParameterfv ) qglGetTexLevelParameterfv;
decltype( dllGetTexLevelParameteriv ) qglGetTexLevelParameteriv;
decltype( dllGetTexParameterfv ) qglGetTexParameterfv;
decltype( dllGetTexParameteriv ) qglGetTexParameteriv;
decltype( dllHint ) qglHint;
decltype( dllIndexMask ) qglIndexMask;
decltype( dllIndexPointer ) qglIndexPointer;
decltype( dllIndexd ) qglIndexd;
decltype( dllIndexdv ) qglIndexdv;
decltype( dllIndexf ) qglIndexf;
decltype( dllIndexfv ) qglIndexfv;
decltype( dllIndexi ) qglIndexi;
decltype( dllIndexiv ) qglIndexiv;
decltype( dllIndexs ) qglIndexs;
decltype( dllIndexsv ) qglIndexsv;
decltype( dllIndexub ) qglIndexub;
decltype( dllIndexubv ) qglIndexubv;
decltype( dllInitNames ) qglInitNames;
decltype( dllInterleavedArrays ) qglInterleavedArrays;
decltype( dllIsEnabled ) qglIsEnabled;
decltype( dllIsList ) qglIsList;
decltype( dllIsTexture ) qglIsTexture;
decltype( dllLightModelf ) qglLightModelf;
decltype( dllLightModelfv ) qglLightModelfv;
decltype( dllLightModeli ) qglLightModeli;
decltype( dllLightModeliv ) qglLightModeliv;
decltype( dllLightf ) qglLightf;
decltype( dllLightfv ) qglLightfv;
decltype( dllLighti ) qglLighti;
decltype( dllLightiv ) qglLightiv;
decltype( dllLineStipple ) qglLineStipple;
decltype( dllLineWidth ) qglLineWidth;
decltype( dllListBase ) qglListBase;
decltype( dllLoadIdentity ) qglLoadIdentity;
decltype( dllLoadMatrixd ) qglLoadMatrixd;
decltype( dllLoadMatrixf ) qglLoadMatrixf;
decltype( dllLoadName ) qglLoadName;
decltype( dllLogicOp ) qglLogicOp;
decltype( dllMap1d ) qglMap1d;
decltype( dllMap1f ) qglMap1f;
decltype( dllMap2d ) qglMap2d;
decltype( dllMap2f ) qglMap2f;
decltype( dllMapGrid1d ) qglMapGrid1d;
decltype( dllMapGrid1f ) qglMapGrid1f;
decltype( dllMapGrid2d ) qglMapGrid2d;
decltype( dllMapGrid2f ) qglMapGrid2f;
decltype( dllMaterialf ) qglMaterialf;
decltype( dllMaterialfv ) qglMaterialfv;
decltype( dllMateriali ) qglMateriali;
decltype( dllMaterialiv ) qglMaterialiv;
decltype( dllMatrixMode ) qglMatrixMode;
decltype( dllMultMatrixd ) qglMultMatrixd;
decltype( dllMultMatrixf ) qglMultMatrixf;
decltype( dllNewList ) qglNewList;
decltype( dllNormal3b ) qglNormal3b;
decltype( dllNormal3bv ) qglNormal3bv;
decltype( dllNormal3d ) qglNormal3d;
decltype( dllNormal3dv ) qglNormal3dv;
decltype( dllNormal3f ) qglNormal3f;
decltype( dllNormal3fv ) qglNormal3fv;
decltype( dllNormal3i ) qglNormal3i;
decltype( dllNormal3iv ) qglNormal3iv;
decltype( dllNormal3s ) qglNormal3s;
decltype( dllNormal3sv ) qglNormal3sv;
decltype( dllNormalPointer ) qglNormalPointer;
decltype( dllOrtho ) qglOrtho;
decltype( dllPassThrough ) qglPassThrough;
decltype( dllPixelMapfv ) qglPixelMapfv;
decltype( dllPixelMapuiv ) qglPixelMapuiv;
decltype( dllPixelMapusv ) qglPixelMapusv;
decltype( dllPixelStoref ) qglPixelStoref;
decltype( dllPixelStorei ) qglPixelStorei;
decltype( dllPixelTransferf ) qglPixelTransferf;
decltype( dllPixelTransferi ) qglPixelTransferi;
decltype( dllPixelZoom ) qglPixelZoom;
decltype( dllPointSize ) qglPointSize;
decltype( dllPolygonMode ) qglPolygonMode;
decltype( dllPolygonOffset ) qglPolygonOffset;
decltype( dllPolygonStipple ) qglPolygonStipple;
decltype( dllPopAttrib ) qglPopAttrib;
decltype( dllPopClientAttrib ) qglPopClientAttrib;
decltype( dllPopMatrix ) qglPopMatrix;
decltype( dllPopName ) qglPopName;
decltype( dllPrioritizeTextures ) qglPrioritizeTextures;
decltype( dllPushAttrib ) qglPushAttrib;
decltype( dllPushClientAttrib ) qglPushClientAttrib;
decltype( dllPushMatrix ) qglPushMatrix;
decltype( dllPushName ) qglPushName;
decltype( dllRasterPos2d ) qglRasterPos2d;
decltype( dllRasterPos2dv ) qglRasterPos2dv;
decltype( dllRasterPos2f ) qglRasterPos2f;
decltype( dllRasterPos2fv ) qglRasterPos2fv;
decltype( dllRasterPos2i ) qglRasterPos2i;
decltype( dllRasterPos2iv ) qglRasterPos2iv;
decltype( dllRasterPos2s ) qglRasterPos2s;
decltype( dllRasterPos2sv ) qglRasterPos2sv;
decltype( dllRasterPos3d ) qglRasterPos3d;
decltype( dllRasterPos3dv ) qglRasterPos3dv;
decltype( dllRasterPos3f ) qglRasterPos3f;
decltype( dllRasterPos3fv ) qglRasterPos3fv;
decltype( dllRasterPos3i ) qglRasterPos3i;
decltype( dllRasterPos3iv ) qglRasterPos3iv;
decltype( dllRasterPos3s ) qglRasterPos3s;
decltype( dllRasterPos3sv ) qglRasterPos3sv;
decltype( dllRasterPos4d ) qglRasterPos4d;
decltype( dllRasterPos4dv ) qglRasterPos4dv;
decltype( dllRasterPos4f ) qglRasterPos4f;
decltype( dllRasterPos4fv ) qglRasterPos4fv;
decltype( dllRasterPos4i ) qglRasterPos4i;
decltype( dllRasterPos4iv ) qglRasterPos4iv;
decltype( dllRasterPos4s ) qglRasterPos4s;
decltype( dllRasterPos4sv ) qglRasterPos4sv;
decltype( dllReadBuffer ) qglReadBuffer;
decltype( dllReadPixels ) qglReadPixels;
decltype( dllRectd ) qglRectd;
decltype( dllRectdv ) qglRectdv;
decltype( dllRectf ) qglRectf;
decltype( dllRectfv ) qglRectfv;
decltype( dllRecti ) qglRecti;
decltype( dllRectiv ) qglRectiv;
decltype( dllRects ) qglRects;
decltype( dllRectsv ) qglRectsv;
decltype( dllRenderMode ) qglRenderMode;
decltype( dllRotated ) qglRotated;
decltype( dllRotatef ) qglRotatef;
decltype( dllScaled ) qglScaled;
decltype( dllScalef ) qglScalef;
decltype( dllScissor ) qglScissor;
decltype( dllSelectBuffer ) qglSelectBuffer;
decltype( dllShadeModel ) qglShadeModel;
decltype( dllStencilFunc ) qglStencilFunc;
decltype( dllStencilMask ) qglStencilMask;
decltype( dllStencilOp ) qglStencilOp;
decltype( dllTexCoord1d ) qglTexCoord1d;
decltype( dllTexCoord1dv ) qglTexCoord1dv;
decltype( dllTexCoord1f ) qglTexCoord1f;
decltype( dllTexCoord1fv ) qglTexCoord1fv;
decltype( dllTexCoord1i ) qglTexCoord1i;
decltype( dllTexCoord1iv ) qglTexCoord1iv;
decltype( dllTexCoord1s ) qglTexCoord1s;
decltype( dllTexCoord1sv ) qglTexCoord1sv;
decltype( dllTexCoord2d ) qglTexCoord2d;
decltype( dllTexCoord2dv ) qglTexCoord2dv;
decltype( dllTexCoord2f ) qglTexCoord2f;
decltype( dllTexCoord2fv ) qglTexCoord2fv;
decltype( dllTexCoord2i ) qglTexCoord2i;
decltype( dllTexCoord2iv ) qglTexCoord2iv;
decltype( dllTexCoord2s ) qglTexCoord2s;
decltype( dllTexCoord2sv ) qglTexCoord2sv;
decltype( dllTexCoord3d ) qglTexCoord3d;
decltype( dllTexCoord3dv ) qglTexCoord3dv;
decltype( dllTexCoord3f ) qglTexCoord3f;
decltype( dllTexCoord3fv ) qglTexCoord3fv;
decltype( dllTexCoord3i ) qglTexCoord3i;
decltype( dllTexCoord3iv ) qglTexCoord3iv;
decltype( dllTexCoord3s ) qglTexCoord3s;
decltype( dllTexCoord3sv ) qglTexCoord3sv;
decltype( dllTexCoord4d ) qglTexCoord4d;
decltype( dllTexCoord4dv ) qglTexCoord4dv;
decltype( dllTexCoord4f ) qglTexCoord4f;
decltype( dllTexCoord4fv ) qglTexCoord4fv;
decltype( dllTexCoord4i ) qglTexCoord4i;
decltype( dllTexCoord4iv ) qglTexCoord4iv;
decltype( dllTexCoord4s ) qglTexCoord4s;
decltype( dllTexCoord4sv ) qglTexCoord4sv;
decltype( dllTexCoordPointer ) qglTexCoordPointer;
decltype( dllTexEnvf ) qglTexEnvf;
decltype( dllTexEnvfv ) qglTexEnvfv;
decltype( dllTexEnvi ) qglTexEnvi;
decltype( dllTexEnviv ) qglTexEnviv;
decltype( dllTexGend ) qglTexGend;
decltype( dllTexGendv ) qglTexGendv;
decltype( dllTexGenf ) qglTexGenf;
decltype( dllTexGenfv ) qglTexGenfv;
decltype( dllTexGeni ) qglTexGeni;
decltype( dllTexGeniv ) qglTexGeniv;
decltype( dllTexImage1D ) qglTexImage1D;
decltype( dllTexImage2D ) qglTexImage2D;
decltype( dllTexParameterf ) qglTexParameterf;
decltype( dllTexParameterfv ) qglTexParameterfv;
decltype( dllTexParameteri ) qglTexParameteri;
decltype( dllTexParameteriv ) qglTexParameteriv;
decltype( dllTexSubImage1D ) qglTexSubImage1D;
decltype( dllTexSubImage2D ) qglTexSubImage2D;
decltype( dllTranslated ) qglTranslated;
decltype( dllTranslatef ) qglTranslatef;
decltype( dllVertex2d ) qglVertex2d;
decltype( dllVertex2dv ) qglVertex2dv;
decltype( dllVertex2f ) qglVertex2f;
decltype( dllVertex2fv ) qglVertex2fv;
decltype( dllVertex2i ) qglVertex2i;
decltype( dllVertex2iv ) qglVertex2iv;
decltype( dllVertex2s ) qglVertex2s;
decltype( dllVertex2sv ) qglVertex2sv;
decltype( dllVertex3d ) qglVertex3d;
decltype( dllVertex3dv ) qglVertex3dv;
decltype( dllVertex3f ) qglVertex3f;
decltype( dllVertex3fv ) qglVertex3fv;
decltype( dllVertex3i ) qglVertex3i;
decltype( dllVertex3iv ) qglVertex3iv;
decltype( dllVertex3s ) qglVertex3s;
decltype( dllVertex3sv ) qglVertex3sv;
decltype( dllVertex4d ) qglVertex4d;
decltype( dllVertex4dv ) qglVertex4dv;
decltype( dllVertex4f ) qglVertex4f;
decltype( dllVertex4fv ) qglVertex4fv;
decltype( dllVertex4i ) qglVertex4i;
decltype( dllVertex4iv ) qglVertex4iv;
decltype( dllVertex4s ) qglVertex4s;
decltype( dllVertex4sv ) qglVertex4sv;
decltype( dllVertexPointer ) qglVertexPointer;
decltype( dllViewport ) qglViewport;

decltype( dllCreateShader ) qglCreateShader;
decltype( dllDeleteShader ) qglDeleteShader;
decltype( dllShaderSource ) qglShaderSource;
decltype( dllGetProgramiv ) qglGetProgramiv;
decltype( dllGetProgramInfoLog ) qglGetProgramInfoLog;
decltype( dllAttachShader ) qglAttachShader;
decltype( dllLinkProgram ) qglLinkProgram;
decltype( dllCreateProgram ) qglCreateProgram;
decltype( dllDeleteProgram ) qglDeleteProgram;
decltype( dllUseProgram ) qglUseProgram;
decltype( dllCompileShader ) qglCompileShader;
decltype( dllGetShaderiv ) qglGetShaderiv;
decltype( dllGetShaderInfoLog ) qglGetShaderInfoLog;
decltype( dllGetUniformLocation ) qglGetUniformLocation;
decltype( dllUniform1ui ) qglUniform1ui;

GLboolean( APIENTRY* qglIsRenderbufferEXT )( GLuint renderbuffer );
void( APIENTRY* qglBindRenderbufferEXT )( GLenum target, GLuint renderbuffer );
void( APIENTRY* qglDeleteRenderbuffersEXT )( GLsizei n, const GLuint* renderbuffers );
void( APIENTRY* qglGenRenderbuffersEXT )( GLsizei n, GLuint* renderbuffers );
void( APIENTRY* qglRenderbufferStorageEXT )( GLenum target, GLenum internalformat, GLsizei width, GLsizei height );
void( APIENTRY* qglGetRenderbufferParameterivEXT )( GLenum target, GLenum pname, GLint* params );
void( APIENTRY* qglBindFramebufferEXT )( GLenum target, GLuint framebuffer );
void( APIENTRY* qglDeleteFramebuffersEXT )( GLsizei n, const GLuint* framebuffers );
void( APIENTRY* qglGenFramebuffersEXT )( GLsizei n, GLuint* framebuffers );
GLenum( APIENTRY* qglCheckFramebufferStatusEXT )( GLenum target );
void( APIENTRY* qglFramebufferTexture2DEXT )( GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
	GLint level );
void( APIENTRY* qglFramebufferRenderbufferEXT )( GLenum target, GLenum attachment, GLenum renderbuffertarget,
	GLuint renderbuffer );
void( APIENTRY* qglBlitFramebufferEXT )( GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
	GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter );
void( APIENTRY* qglRenderbufferStorageMultisampleEXT )( GLenum target, GLsizei samples, GLenum internalformat,
	GLsizei width, GLsizei height );
void( APIENTRY* qglPointParameterfEXT )( GLenum param, GLfloat value );
void( APIENTRY* qglPointParameterfvEXT )( GLenum param, const GLfloat* value );
void( APIENTRY* qglColorTableEXT )( GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type,
	const void* table );
void( APIENTRY* qglSelectTextureSGIS )( GLenum texture );
void( APIENTRY* qglMTexCoord2fSGIS )( GLenum target, GLfloat s, GLfloat t );
BOOL( WINAPI* qwglSwapBuffers )( HDC hdc );

#if defined( _WIN32 )
extern "C"
{
DLL_EXPORT BOOL( WINAPI* qwglMakeCurrent )( HDC hdc, HGLRC context ) = NULL;
DLL_EXPORT HDC( WINAPI* qwglGetCurrentDC )() = NULL;
DLL_EXPORT HGLRC( WINAPI* qwglGetCurrentContext )() = NULL;
DLL_EXPORT BOOL( WINAPI* qwglDeleteContext )( HGLRC context ) = NULL;
DLL_EXPORT HGLRC( WINAPI* qwglCreateContext )( HDC hdc ) = NULL;
DLL_EXPORT BOOL( WINAPI* qwglSetPixelFormat )( HDC hdc, int format, const PIXELFORMATDESCRIPTOR* descriptor ) = NULL;
int( WINAPI* qwglDescribePixelFormat )( HDC hdc, int format, UINT size, PIXELFORMATDESCRIPTOR* descriptor ) = NULL;
DLL_EXPORT int( WINAPI* qwglChoosePixelFormat )( HDC hdc, const PIXELFORMATDESCRIPTOR* descriptor ) = NULL;
}
#endif

static void APIENTRY logAccum( GLenum op, GLfloat value )
{
	FS_FPrintf( glw_state.log_fp, "glAccum\n" );
	dllAccum( op, value );
}

static void APIENTRY logAlphaFunc( GLenum func, GLclampf ref )
{
	FS_FPrintf( glw_state.log_fp, "glAlphaFunc( 0x%x, %f )\n", func, ref );
	dllAlphaFunc( func, ref );
}

static GLboolean APIENTRY logAreTexturesResident( GLsizei n, const GLuint* textures, GLboolean* residences )
{
	FS_FPrintf( glw_state.log_fp, "glAreTexturesResident\n" );
	return dllAreTexturesResident( n, textures, residences );
}

static void APIENTRY logArrayElement( GLint i )
{
	FS_FPrintf( glw_state.log_fp, "glArrayElement\n" );
	dllArrayElement( i );
}

static void APIENTRY logBegin( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glBegin( 0x%x )\n", mode );
	dllBegin( mode );
}

static void APIENTRY logBindTexture( GLenum target, GLuint texture )
{
	FS_FPrintf( glw_state.log_fp, "glBindTexture( 0x%x, %u )\n", target, texture );
	dllBindTexture( target, texture );
}

static void APIENTRY logBitmap( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove,
	GLfloat ymove, const GLubyte* bitmap )
{
	FS_FPrintf( glw_state.log_fp, "glBitmap\n" );
	dllBitmap( width, height, xorig, yorig, xmove, ymove, bitmap );
}

static void APIENTRY logBlendFunc( GLenum sfactor, GLenum dfactor )
{
	FS_FPrintf( glw_state.log_fp, "glBlendFunc( 0x%x, 0x%x )\n", sfactor, dfactor );
	dllBlendFunc( sfactor, dfactor );
}

static void APIENTRY logCallList( GLuint list )
{
	FS_FPrintf( glw_state.log_fp, "glCallList( %u )\n", list );
	dllCallList( list );
}

static void APIENTRY logCallLists( GLsizei n, GLenum type, const void* lists )
{
	FS_FPrintf( glw_state.log_fp, "glCallLists\n" );
	dllCallLists( n, type, lists );
}

static void APIENTRY logClear( GLbitfield mask )
{
	FS_FPrintf( glw_state.log_fp, "glClear\n" );
	dllClear( mask );
}

static void APIENTRY logClearAccum( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha )
{
	FS_FPrintf( glw_state.log_fp, "glClearAccum\n" );
	dllClearAccum( red, green, blue, alpha );
}

static void APIENTRY logClearColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha )
{
	FS_FPrintf( glw_state.log_fp, "glClearColor\n" );
	dllClearColor( red, green, blue, alpha );
}

static void APIENTRY logClearDepth( GLclampd depth )
{
	FS_FPrintf( glw_state.log_fp, "glClearDepth\n" );
	dllClearDepth( depth );
}

static void APIENTRY logClearIndex( GLfloat color )
{
	FS_FPrintf( glw_state.log_fp, "glClearIndex\n" );
	dllClearIndex( color );
}

static void APIENTRY logClearStencil( GLint stencil )
{
	FS_FPrintf( glw_state.log_fp, "glClearStencil\n" );
	dllClearStencil( stencil );
}

static void APIENTRY logClipPlane( GLenum plane, const GLdouble* equation )
{
	FS_FPrintf( glw_state.log_fp, "glClipPlane\n" );
	dllClipPlane( plane, equation );
}

static void APIENTRY logColor3b( GLbyte red, GLbyte green, GLbyte blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3b\n" );
	dllColor3b( red, green, blue );
}

static void APIENTRY logColor3bv( const GLbyte* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3bv\n" );
	dllColor3bv( color );
}

static void APIENTRY logColor3d( GLdouble red, GLdouble green, GLdouble blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3d\n" );
	dllColor3d( red, green, blue );
}

static void APIENTRY logColor3dv( const GLdouble* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3dv\n" );
	dllColor3dv( color );
}

static void APIENTRY logColor3f( GLfloat red, GLfloat green, GLfloat blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3f\n" );
	dllColor3f( red, green, blue );
}

static void APIENTRY logColor3fv( const GLfloat* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3fv\n" );
	dllColor3fv( color );
}

static void APIENTRY logColor3i( GLint red, GLint green, GLint blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3i\n" );
	dllColor3i( red, green, blue );
}

static void APIENTRY logColor3iv( const GLint* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3iv\n" );
	dllColor3iv( color );
}

static void APIENTRY logColor3s( GLshort red, GLshort green, GLshort blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3s\n" );
	dllColor3s( red, green, blue );
}

static void APIENTRY logColor3sv( const GLshort* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3sv\n" );
	dllColor3sv( color );
}

static void APIENTRY logColor3ub( GLubyte red, GLubyte green, GLubyte blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3ub\n" );
	dllColor3ub( red, green, blue );
}

static void APIENTRY logColor3ubv( const GLubyte* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3ubv\n" );
	dllColor3ubv( color );
}

static void APIENTRY logColor3ui( GLuint red, GLuint green, GLuint blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3ui\n" );
	dllColor3ui( red, green, blue );
}

static void APIENTRY logColor3uiv( const GLuint* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3uiv\n" );
	dllColor3uiv( color );
}

static void APIENTRY logColor3us( GLushort red, GLushort green, GLushort blue )
{
	FS_FPrintf( glw_state.log_fp, "glColor3us\n" );
	dllColor3us( red, green, blue );
}

static void APIENTRY logColor3usv( const GLushort* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor3usv\n" );
	dllColor3usv( color );
}

static void APIENTRY logColor4b( GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4b\n" );
	dllColor4b( red, green, blue, alpha );
}

static void APIENTRY logColor4bv( const GLbyte* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4bv\n" );
	dllColor4bv( color );
}

static void APIENTRY logColor4d( GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4d\n" );
	dllColor4d( red, green, blue, alpha );
}

static void APIENTRY logColor4dv( const GLdouble* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4dv\n" );
	dllColor4dv( color );
}

static void APIENTRY logColor4f( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4f( %f,%f,%f,%f )\n", red, green, blue, alpha );
	dllColor4f( red, green, blue, alpha );
}

static void APIENTRY logColor4fv( const GLfloat* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4fv( %f,%f,%f,%f )\n", color[0], color[1], color[2], color[3] );
	dllColor4fv( color );
}

static void APIENTRY logColor4i( GLint red, GLint green, GLint blue, GLint alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4i\n" );
	dllColor4i( red, green, blue, alpha );
}

static void APIENTRY logColor4iv( const GLint* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4iv\n" );
	dllColor4iv( color );
}

static void APIENTRY logColor4s( GLshort red, GLshort green, GLshort blue, GLshort alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4s\n" );
	dllColor4s( red, green, blue, alpha );
}

static void APIENTRY logColor4sv( const GLshort* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4sv\n" );
	dllColor4sv( color );
}

static void APIENTRY logColor4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4b\n" );
	dllColor4b( (GLbyte)red, (GLbyte)green, (GLbyte)blue, (GLbyte)alpha );
}

static void APIENTRY logColor4ubv( const GLubyte* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4ubv\n" );
	dllColor4ubv( color );
}

static void APIENTRY logColor4ui( GLuint red, GLuint green, GLuint blue, GLuint alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4ui\n" );
	dllColor4ui( red, green, blue, alpha );
}

static void APIENTRY logColor4uiv( const GLuint* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4uiv\n" );
	dllColor4uiv( color );
}

static void APIENTRY logColor4us( GLushort red, GLushort green, GLushort blue, GLushort alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColor4us\n" );
	dllColor4us( red, green, blue, alpha );
}

static void APIENTRY logColor4usv( const GLushort* color )
{
	FS_FPrintf( glw_state.log_fp, "glColor4usv\n" );
	dllColor4usv( color );
}

static void APIENTRY logColorMask( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha )
{
	FS_FPrintf( glw_state.log_fp, "glColorMask\n" );
	dllColorMask( red, green, blue, alpha );
}

static void APIENTRY logColorMaterial( GLenum face, GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glColorMaterial\n" );
	dllColorMaterial( face, mode );
}

static void APIENTRY logColorPointer( GLint size, GLenum type, GLsizei stride, const void* pointer )
{
	FS_FPrintf( glw_state.log_fp, "glColorPointer\n" );
	dllColorPointer( size, type, stride, pointer );
}

static void APIENTRY logCopyPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type )
{
	FS_FPrintf( glw_state.log_fp, "glCopyPixels\n" );
	dllCopyPixels( x, y, width, height, type );
}

static void APIENTRY logCopyTexImage1D( GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
	GLsizei width, GLint border )
{
	FS_FPrintf( glw_state.log_fp, "glCopyTexImage1D\n" );
	dllCopyTexImage1D( target, level, internalFormat, x, y, width, border );
}

static void APIENTRY logCopyTexImage2D( GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y,
	GLsizei width, GLsizei height, GLint border )
{
	FS_FPrintf( glw_state.log_fp, "glCopyTexImage2D\n" );
	dllCopyTexImage2D( target, level, internalFormat, x, y, width, height, border );
}

static void APIENTRY logCopyTexSubImage1D( GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width )
{
	FS_FPrintf( glw_state.log_fp, "glCopyTexSubImage1D\n" );
	dllCopyTexSubImage1D( target, level, xoffset, x, y, width );
}

static void APIENTRY logCopyTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y,
	GLsizei width, GLsizei height )
{
	FS_FPrintf( glw_state.log_fp, "glCopyTexSubImage2D\n" );
	dllCopyTexSubImage2D( target, level, xoffset, yoffset, x, y, width, height );
}

static void APIENTRY logCullFace( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glCullFace\n" );
	dllCullFace( mode );
}

static void APIENTRY logDeleteLists( GLuint list, GLsizei range )
{
	FS_FPrintf( glw_state.log_fp, "glDeleteLists\n" );
	dllDeleteLists( list, range );
}

static void APIENTRY logDeleteTextures( GLsizei n, const GLuint* textures )
{
	FS_FPrintf( glw_state.log_fp, "glDeleteTextures\n" );
	dllDeleteTextures( n, textures );
}

static void APIENTRY logDepthFunc( GLenum func )
{
	FS_FPrintf( glw_state.log_fp, "glDepthFunc\n" );
	dllDepthFunc( func );
}

static void APIENTRY logDepthMask( GLboolean flag )
{
	FS_FPrintf( glw_state.log_fp, "glDepthMask\n" );
	dllDepthMask( flag );
}

static void APIENTRY logDepthRange( GLclampd zNear, GLclampd zFar )
{
	FS_FPrintf( glw_state.log_fp, "glDepthRange\n" );
	dllDepthRange( zNear, zFar );
}

static void APIENTRY logDisable( GLenum cap )
{
	FS_FPrintf( glw_state.log_fp, "glDisable( 0x%x )\n", cap );
	dllDisable( cap );
}

static void APIENTRY logDisableClientState( GLenum array )
{
	FS_FPrintf( glw_state.log_fp, "glDisableClientState\n" );
	dllDisableClientState( array );
}

static void APIENTRY logDrawArrays( GLenum mode, GLint first, GLsizei count )
{
	FS_FPrintf( glw_state.log_fp, "glDrawArrays\n" );
	dllDrawArrays( mode, first, count );
}

static void APIENTRY logDrawBuffer( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glDrawBuffer\n" );
	dllDrawBuffer( mode );
}

static void APIENTRY logDrawElements( GLenum mode, GLsizei count, GLenum type, const void* indices )
{
	FS_FPrintf( glw_state.log_fp, "glDrawElements\n" );
	dllDrawElements( mode, count, type, indices );
}

static void APIENTRY logDrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels )
{
	FS_FPrintf( glw_state.log_fp, "glDrawPixels\n" );
	dllDrawPixels( width, height, format, type, pixels );
}

static void APIENTRY logEdgeFlag( GLboolean flag )
{
	FS_FPrintf( glw_state.log_fp, "glEdgeFlag\n" );
	dllEdgeFlag( flag );
}

static void APIENTRY logEdgeFlagPointer( GLsizei stride, const void* pointer )
{
	FS_FPrintf( glw_state.log_fp, "glEdgeFlagPointer\n" );
	dllEdgeFlagPointer( stride, pointer );
}

static void APIENTRY logEdgeFlagv( const GLboolean* flag )
{
	FS_FPrintf( glw_state.log_fp, "glEdgeFlagv\n" );
	dllEdgeFlagv( flag );
}

static void APIENTRY logEnable( GLenum cap )
{
	FS_FPrintf( glw_state.log_fp, "glEnable( 0x%x )\n", cap );
	dllEnable( cap );
}

static void APIENTRY logEnableClientState( GLenum array )
{
	FS_FPrintf( glw_state.log_fp, "glEnableClientState\n" );
	dllEnableClientState( array );
}

static void APIENTRY logEnd()
{
	FS_FPrintf( glw_state.log_fp, "glEnd\n" );
	dllEnd();
}

static void APIENTRY logEndList()
{
	FS_FPrintf( glw_state.log_fp, "glEndList\n" );
	dllEndList();
}

static void APIENTRY logEvalCoord1d( GLdouble u )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord1d\n" );
	dllEvalCoord1d( u );
}

static void APIENTRY logEvalCoord1dv( const GLdouble* u )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord1dv\n" );
	dllEvalCoord1dv( u );
}

static void APIENTRY logEvalCoord1f( GLfloat u )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord1f\n" );
	dllEvalCoord1f( u );
}

static void APIENTRY logEvalCoord1fv( const GLfloat* u )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord1fv\n" );
	dllEvalCoord1fv( u );
}

static void APIENTRY logEvalCoord2d( GLdouble u, GLdouble v )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord2d\n" );
	dllEvalCoord2d( u, v );
}

static void APIENTRY logEvalCoord2dv( const GLdouble* u )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord2dv\n" );
	dllEvalCoord2dv( u );
}

static void APIENTRY logEvalCoord2f( GLfloat u, GLfloat v )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord2f\n" );
	dllEvalCoord2f( u, v );
}

static void APIENTRY logEvalCoord2fv( const GLfloat* u )
{
	FS_FPrintf( glw_state.log_fp, "glEvalCoord2fv\n" );
	dllEvalCoord2fv( u );
}

static void APIENTRY logEvalMesh1( GLenum mode, GLint i1, GLint i2 )
{
	FS_FPrintf( glw_state.log_fp, "glEvalMesh1\n" );
	dllEvalMesh1( mode, i1, i2 );
}

static void APIENTRY logEvalMesh2( GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2 )
{
	FS_FPrintf( glw_state.log_fp, "glEvalMesh2\n" );
	dllEvalMesh2( mode, i1, i2, j1, j2 );
}

static void APIENTRY logEvalPoint1( GLint i )
{
	FS_FPrintf( glw_state.log_fp, "glEvalPoint1\n" );
	dllEvalPoint1( i );
}

static void APIENTRY logEvalPoint2( GLint i, GLint j )
{
	FS_FPrintf( glw_state.log_fp, "glEvalPoint2\n" );
	dllEvalPoint2( i, j );
}

static void APIENTRY logFeedbackBuffer( GLsizei size, GLenum type, GLfloat* buffer )
{
	FS_FPrintf( glw_state.log_fp, "glFeedbackBuffer\n" );
	dllFeedbackBuffer( size, type, buffer );
}

static void APIENTRY logFinish()
{
	FS_FPrintf( glw_state.log_fp, "glFinish\n" );
	dllFinish();
}

static void APIENTRY logFlush()
{
	FS_FPrintf( glw_state.log_fp, "glFlush\n" );
	dllFlush();
}

static void APIENTRY logFogf( GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glFogf\n" );
	dllFogf( pname, param );
}

static void APIENTRY logFogfv( GLenum pname, const GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glFogfv\n" );
	dllFogfv( pname, params );
}

static void APIENTRY logFogi( GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glFogi\n" );
	dllFogi( pname, param );
}

static void APIENTRY logFogiv( GLenum pname, const GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glFogiv\n" );
	dllFogiv( pname, params );
}

static void APIENTRY logFrontFace( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glFrontFace\n" );
	dllFrontFace( mode );
}

static void APIENTRY logFrustum( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear,
	GLdouble zFar )
{
	FS_FPrintf( glw_state.log_fp, "glFrustum\n" );
	dllFrustum( left, right, bottom, top, zNear, zFar );
}

static GLuint APIENTRY logGenLists( GLsizei range )
{
	FS_FPrintf( glw_state.log_fp, "glGenLists\n" );
	return dllGenLists( range );
}

static void APIENTRY logGenTextures( GLsizei n, GLuint* textures )
{
	FS_FPrintf( glw_state.log_fp, "glGenTextures\n" );
	dllGenTextures( n, textures );
}

static void APIENTRY logGetBooleanv( GLenum pname, GLboolean* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetBooleanv\n" );
	dllGetBooleanv( pname, params );
}

static void APIENTRY logGetClipPlane( GLenum plane, GLdouble* equation )
{
	FS_FPrintf( glw_state.log_fp, "glGetClipPlane\n" );
	dllGetClipPlane( plane, equation );
}

static void APIENTRY logGetDoublev( GLenum pname, GLdouble* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetDoublev\n" );
	dllGetDoublev( pname, params );
}

static GLenum APIENTRY logGetError()
{
	FS_FPrintf( glw_state.log_fp, "glGetError\n" );
	return dllGetError();
}

static void APIENTRY logGetFloatv( GLenum pname, GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetFloatv\n" );
	dllGetFloatv( pname, params );
}

static void APIENTRY logGetIntegerv( GLenum pname, GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetIntegerv\n" );
	dllGetIntegerv( pname, params );
}

static void APIENTRY logGetLightfv( GLenum light, GLenum pname, GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetLightfv\n" );
	dllGetLightfv( light, pname, params );
}

static void APIENTRY logGetLightiv( GLenum light, GLenum pname, GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetLightiv\n" );
	dllGetLightiv( light, pname, params );
}

static void APIENTRY logGetMapdv( GLenum target, GLenum query, GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glGetMapdv\n" );
	dllGetMapdv( target, query, v );
}

static void APIENTRY logGetMapfv( GLenum target, GLenum query, GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glGetMapfv\n" );
	dllGetMapfv( target, query, v );
}

static void APIENTRY logGetMapiv( GLenum target, GLenum query, GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glGetMapiv\n" );
	dllGetMapiv( target, query, v );
}

static void APIENTRY logGetMaterialfv( GLenum face, GLenum pname, GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetMaterialfv\n" );
	dllGetMaterialfv( face, pname, params );
}

static void APIENTRY logGetMaterialiv( GLenum face, GLenum pname, GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetMaterialiv\n" );
	dllGetMaterialiv( face, pname, params );
}

static void APIENTRY logGetPixelMapfv( GLenum map, GLfloat* values )
{
	FS_FPrintf( glw_state.log_fp, "glGetPixelMapfv\n" );
	dllGetPixelMapfv( map, values );
}

static void APIENTRY logGetPixelMapuiv( GLenum map, GLuint* values )
{
	FS_FPrintf( glw_state.log_fp, "glGetPixelMapuiv\n" );
	dllGetPixelMapuiv( map, values );
}

static void APIENTRY logGetPixelMapusv( GLenum map, GLushort* values )
{
	FS_FPrintf( glw_state.log_fp, "glGetPixelMapusv\n" );
	dllGetPixelMapusv( map, values );
}

static void APIENTRY logGetPointerv( GLenum pname, void** params )
{
	FS_FPrintf( glw_state.log_fp, "glGetPointerv\n" );
	dllGetPointerv( pname, params );
}

static void APIENTRY logGetPolygonStipple( GLubyte* mask )
{
	FS_FPrintf( glw_state.log_fp, "glGetPolygonStipple\n" );
	dllGetPolygonStipple( mask );
}

static const GLubyte* APIENTRY logGetString( GLenum name )
{
	FS_FPrintf( glw_state.log_fp, "glGetString\n" );
	return dllGetString( name );
}

static void APIENTRY logGetTexEnvfv( GLenum target, GLenum pname, GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexEnvfv\n" );
	dllGetTexEnvfv( target, pname, params );
}

static void APIENTRY logGetTexEnviv( GLenum target, GLenum pname, GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexEnviv\n" );
	dllGetTexEnviv( target, pname, params );
}

static void APIENTRY logGetTexGendv( GLenum coord, GLenum pname, GLdouble* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexGendv\n" );
	dllGetTexGendv( coord, pname, params );
}

static void APIENTRY logGetTexGenfv( GLenum coord, GLenum pname, GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexGenfv\n" );
	dllGetTexGenfv( coord, pname, params );
}

static void APIENTRY logGetTexGeniv( GLenum coord, GLenum pname, GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexGeniv\n" );
	dllGetTexGeniv( coord, pname, params );
}

static void APIENTRY logGetTexImage( GLenum target, GLint level, GLenum format, GLenum type, void* pixels )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexImage\n" );
	dllGetTexImage( target, level, format, type, pixels );
}

static void APIENTRY logGetTexLevelParameterfv( GLenum target, GLint level, GLenum pname, GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexLevelParameterfv\n" );
	dllGetTexLevelParameterfv( target, level, pname, params );
}

static void APIENTRY logGetTexLevelParameteriv( GLenum target, GLint level, GLenum pname, GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexLevelParameteriv\n" );
	dllGetTexLevelParameteriv( target, level, pname, params );
}

static void APIENTRY logGetTexParameterfv( GLenum target, GLenum pname, GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexParameterfv\n" );
	dllGetTexParameterfv( target, pname, params );
}

static void APIENTRY logGetTexParameteriv( GLenum target, GLenum pname, GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glGetTexParameteriv\n" );
	dllGetTexParameteriv( target, pname, params );
}

static void APIENTRY logHint( GLenum target, GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glHint( 0x%x, 0x%x )\n", target, mode );
	dllHint( target, mode );
}

static void APIENTRY logIndexMask( GLuint mask )
{
	FS_FPrintf( glw_state.log_fp, "glIndexMask\n" );
	dllIndexMask( mask );
}

static void APIENTRY logIndexPointer( GLenum type, GLsizei stride, const void* pointer )
{
	FS_FPrintf( glw_state.log_fp, "glIndexPointer\n" );
	dllIndexPointer( type, stride, pointer );
}

static void APIENTRY logIndexd( GLdouble c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexd\n" );
	dllIndexd( c );
}

static void APIENTRY logIndexdv( const GLdouble* c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexdv\n" );
	dllIndexdv( c );
}

static void APIENTRY logIndexf( GLfloat c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexf\n" );
	dllIndexf( c );
}

static void APIENTRY logIndexfv( const GLfloat* c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexfv\n" );
	dllIndexfv( c );
}

static void APIENTRY logIndexi( GLint c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexi\n" );
	dllIndexi( c );
}

static void APIENTRY logIndexiv( const GLint* c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexiv\n" );
	dllIndexiv( c );
}

static void APIENTRY logIndexs( GLshort c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexs\n" );
	dllIndexs( c );
}

static void APIENTRY logIndexsv( const GLshort* c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexsv\n" );
	dllIndexsv( c );
}

static void APIENTRY logIndexub( GLubyte c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexub\n" );
	dllIndexub( c );
}

static void APIENTRY logIndexubv( const GLubyte* c )
{
	FS_FPrintf( glw_state.log_fp, "glIndexubv\n" );
	dllIndexubv( c );
}

static void APIENTRY logInitNames()
{
	FS_FPrintf( glw_state.log_fp, "glInitNames\n" );
	dllInitNames();
}

static void APIENTRY logInterleavedArrays( GLenum format, GLsizei stride, const void* pointer )
{
	FS_FPrintf( glw_state.log_fp, "glInterleavedArrays\n" );
	dllInterleavedArrays( format, stride, pointer );
}

static GLboolean APIENTRY logIsEnabled( GLenum cap )
{
	FS_FPrintf( glw_state.log_fp, "glIsEnabled\n" );
	return dllIsEnabled( cap );
}

static GLboolean APIENTRY logIsList( GLuint list )
{
	FS_FPrintf( glw_state.log_fp, "glIsList\n" );
	return dllIsList( list );
}

static GLboolean APIENTRY logIsTexture( GLuint texture )
{
	FS_FPrintf( glw_state.log_fp, "glIsTexture\n" );
	return dllIsTexture( texture );
}

static void APIENTRY logLightModelf( GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glLightModelf\n" );
	dllLightModelf( pname, param );
}

static void APIENTRY logLightModelfv( GLenum pname, const GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glLightModelfv\n" );
	dllLightModelfv( pname, params );
}

static void APIENTRY logLightModeli( GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glLightModeli\n" );
	dllLightModeli( pname, param );
}

static void APIENTRY logLightModeliv( GLenum pname, const GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glLightModeliv\n" );
	dllLightModeliv( pname, params );
}

static void APIENTRY logLightf( GLenum light, GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glLightf\n" );
	dllLightf( light, pname, param );
}

static void APIENTRY logLightfv( GLenum light, GLenum pname, const GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glLightfv\n" );
	dllLightfv( light, pname, params );
}

static void APIENTRY logLighti( GLenum light, GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glLighti\n" );
	dllLighti( light, pname, param );
}

static void APIENTRY logLightiv( GLenum light, GLenum pname, const GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glLightiv\n" );
	dllLightiv( light, pname, params );
}

static void APIENTRY logLineStipple( GLint factor, GLushort pattern )
{
	FS_FPrintf( glw_state.log_fp, "glLineStipple\n" );
	dllLineStipple( factor, pattern );
}

static void APIENTRY logLineWidth( GLfloat width )
{
	FS_FPrintf( glw_state.log_fp, "glLineWidth\n" );
	dllLineWidth( width );
}

static void APIENTRY logListBase( GLuint base )
{
	FS_FPrintf( glw_state.log_fp, "glListBase\n" );
	dllListBase( base );
}

static void APIENTRY logLoadIdentity()
{
	FS_FPrintf( glw_state.log_fp, "glLoadIdentity\n" );
	dllLoadIdentity();
}

static void APIENTRY logLoadMatrixd( const GLdouble* m )
{
	FS_FPrintf( glw_state.log_fp, "glLoadMatrixd\n" );
	dllLoadMatrixd( m );
}

static void APIENTRY logLoadMatrixf( const GLfloat* m )
{
	FS_FPrintf( glw_state.log_fp, "glLoadMatrixf\n" );
	dllLoadMatrixf( m );
}

static void APIENTRY logLoadName( GLuint name )
{
	FS_FPrintf( glw_state.log_fp, "glLoadName\n" );
	dllLoadName( name );
}

static void APIENTRY logLogicOp( GLenum opcode )
{
	FS_FPrintf( glw_state.log_fp, "glLogicOp\n" );
	dllLogicOp( opcode );
}

static void APIENTRY logMap1d( GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
	const GLdouble* points )
{
	FS_FPrintf( glw_state.log_fp, "glMap1d\n" );
	dllMap1d( target, u1, u2, stride, order, points );
}

static void APIENTRY logMap1f( GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat* points )
{
	FS_FPrintf( glw_state.log_fp, "glMap1f\n" );
	dllMap1f( target, u1, u2, stride, order, points );
}

static void APIENTRY logMap2d( GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1,
	GLdouble v2, GLint vstride, GLint vorder, const GLdouble* points )
{
	FS_FPrintf( glw_state.log_fp, "glMap2d\n" );
	dllMap2d( target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points );
}

static void APIENTRY logMap2f( GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1,
	GLfloat v2, GLint vstride, GLint vorder, const GLfloat* points )
{
	FS_FPrintf( glw_state.log_fp, "glMap2f\n" );
	dllMap2f( target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points );
}

static void APIENTRY logMapGrid1d( GLint un, GLdouble u1, GLdouble u2 )
{
	FS_FPrintf( glw_state.log_fp, "glMapGrid1d\n" );
	dllMapGrid1d( un, u1, u2 );
}

static void APIENTRY logMapGrid1f( GLint un, GLfloat u1, GLfloat u2 )
{
	FS_FPrintf( glw_state.log_fp, "glMapGrid1f\n" );
	dllMapGrid1f( un, u1, u2 );
}

static void APIENTRY logMapGrid2d( GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2 )
{
	FS_FPrintf( glw_state.log_fp, "glMapGrid2d\n" );
	dllMapGrid2d( un, u1, u2, vn, v1, v2 );
}

static void APIENTRY logMapGrid2f( GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2 )
{
	FS_FPrintf( glw_state.log_fp, "glMapGrid2f\n" );
	dllMapGrid2f( un, u1, u2, vn, v1, v2 );
}

static void APIENTRY logMaterialf( GLenum face, GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glMaterialf\n" );
	dllMaterialf( face, pname, param );
}

static void APIENTRY logMaterialfv( GLenum face, GLenum pname, const GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glMaterialfv\n" );
	dllMaterialfv( face, pname, params );
}

static void APIENTRY logMateriali( GLenum face, GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glMateriali\n" );
	dllMateriali( face, pname, param );
}

static void APIENTRY logMaterialiv( GLenum face, GLenum pname, const GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glMaterialiv\n" );
	dllMaterialiv( face, pname, params );
}

static void APIENTRY logMatrixMode( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glMatrixMode\n" );
	dllMatrixMode( mode );
}

static void APIENTRY logMultMatrixd( const GLdouble* m )
{
	FS_FPrintf( glw_state.log_fp, "glMultMatrixd\n" );
	dllMultMatrixd( m );
}

static void APIENTRY logMultMatrixf( const GLfloat* m )
{
	FS_FPrintf( glw_state.log_fp, "glMultMatrixf\n" );
	dllMultMatrixf( m );
}

static void APIENTRY logNewList( GLuint list, GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glNewList\n" );
	dllNewList( list, mode );
}

static void APIENTRY logNormal3b( GLbyte nx, GLbyte ny, GLbyte nz )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3b\n" );
	dllNormal3b( nx, ny, nz );
}

static void APIENTRY logNormal3bv( const GLbyte* v )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3bv\n" );
	dllNormal3bv( v );
}

static void APIENTRY logNormal3d( GLdouble nx, GLdouble ny, GLdouble nz )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3d\n" );
	dllNormal3d( nx, ny, nz );
}

static void APIENTRY logNormal3dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3dv\n" );
	dllNormal3dv( v );
}

static void APIENTRY logNormal3f( GLfloat nx, GLfloat ny, GLfloat nz )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3f\n" );
	dllNormal3f( nx, ny, nz );
}

static void APIENTRY logNormal3fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3fv\n" );
	dllNormal3fv( v );
}

static void APIENTRY logNormal3i( GLint nx, GLint ny, GLint nz )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3i\n" );
	dllNormal3i( nx, ny, nz );
}

static void APIENTRY logNormal3iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3iv\n" );
	dllNormal3iv( v );
}

static void APIENTRY logNormal3s( GLshort nx, GLshort ny, GLshort nz )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3s\n" );
	dllNormal3s( nx, ny, nz );
}

static void APIENTRY logNormal3sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glNormal3sv\n" );
	dllNormal3sv( v );
}

static void APIENTRY logNormalPointer( GLenum type, GLsizei stride, const void* pointer )
{
	FS_FPrintf( glw_state.log_fp, "glNormalPointer\n" );
	dllNormalPointer( type, stride, pointer );
}

static void APIENTRY logOrtho( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear,
	GLdouble zFar )
{
	FS_FPrintf( glw_state.log_fp, "glOrtho\n" );
	dllOrtho( left, right, bottom, top, zNear, zFar );
}

static void APIENTRY logPassThrough( GLfloat token )
{
	FS_FPrintf( glw_state.log_fp, "glPassThrough\n" );
	dllPassThrough( token );
}

static void APIENTRY logPixelMapfv( GLenum map, GLsizei mapsize, const GLfloat* values )
{
	FS_FPrintf( glw_state.log_fp, "glPixelMapfv\n" );
	dllPixelMapfv( map, mapsize, values );
}

static void APIENTRY logPixelMapuiv( GLenum map, GLsizei mapsize, const GLuint* values )
{
	FS_FPrintf( glw_state.log_fp, "glPixelMapuiv\n" );
	dllPixelMapuiv( map, mapsize, values );
}

static void APIENTRY logPixelMapusv( GLenum map, GLsizei mapsize, const GLushort* values )
{
	FS_FPrintf( glw_state.log_fp, "glPixelMapusv\n" );
	dllPixelMapusv( map, mapsize, values );
}

static void APIENTRY logPixelStoref( GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glPixelStoref\n" );
	dllPixelStoref( pname, param );
}

static void APIENTRY logPixelStorei( GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glPixelStorei\n" );
	dllPixelStorei( pname, param );
}

static void APIENTRY logPixelTransferf( GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glPixelTransferf\n" );
	dllPixelTransferf( pname, param );
}

static void APIENTRY logPixelTransferi( GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glPixelTransferi\n" );
	dllPixelTransferi( pname, param );
}

static void APIENTRY logPixelZoom( GLfloat xfactor, GLfloat yfactor )
{
	FS_FPrintf( glw_state.log_fp, "glPixelZoom\n" );
	dllPixelZoom( xfactor, yfactor );
}

static void APIENTRY logPointSize( GLfloat size )
{
	FS_FPrintf( glw_state.log_fp, "glPointSize\n" );
	dllPointSize( size );
}

static void APIENTRY logPolygonMode( GLenum face, GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glPolygonMode( 0x%x, 0x%x )\n", face, mode );
	dllPolygonMode( face, mode );
}

static void APIENTRY logPolygonOffset( GLfloat factor, GLfloat units )
{
	FS_FPrintf( glw_state.log_fp, "glPolygonOffset\n" );
	dllPolygonOffset( factor, units );
}

static void APIENTRY logPolygonStipple( const GLubyte* mask )
{
	FS_FPrintf( glw_state.log_fp, "glPolygonStipple\n" );
	dllPolygonStipple( mask );
}

static void APIENTRY logPopAttrib()
{
	FS_FPrintf( glw_state.log_fp, "glPopAttrib\n" );
	dllPopAttrib();
}

static void APIENTRY logPopClientAttrib()
{
	FS_FPrintf( glw_state.log_fp, "glPopClientAttrib\n" );
	dllPopClientAttrib();
}

static void APIENTRY logPopMatrix()
{
	FS_FPrintf( glw_state.log_fp, "glPopMatrix\n" );
	dllPopMatrix();
}

static void APIENTRY logPopName()
{
	FS_FPrintf( glw_state.log_fp, "glPopName\n" );
	dllPopName();
}

static void APIENTRY logPrioritizeTextures( GLsizei n, const GLuint* textures, const GLclampf* priorities )
{
	FS_FPrintf( glw_state.log_fp, "glPrioritizeTextures\n" );
	dllPrioritizeTextures( n, textures, priorities );
}

static void APIENTRY logPushAttrib( GLbitfield mask )
{
	FS_FPrintf( glw_state.log_fp, "glPushAttrib\n" );
	dllPushAttrib( mask );
}

static void APIENTRY logPushClientAttrib( GLbitfield mask )
{
	FS_FPrintf( glw_state.log_fp, "glPushClientAttrib\n" );
	dllPushClientAttrib( mask );
}

static void APIENTRY logPushMatrix()
{
	FS_FPrintf( glw_state.log_fp, "glPushMatrix\n" );
	dllPushMatrix();
}

static void APIENTRY logPushName( GLuint name )
{
	FS_FPrintf( glw_state.log_fp, "glPushName\n" );
	dllPushName( name );
}

static void APIENTRY logRasterPos2d( GLdouble x, GLdouble y )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPot2d\n" );
	dllRasterPos2d( x, y );
}

static void APIENTRY logRasterPos2dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos2dv\n" );
	dllRasterPos2dv( v );
}

static void APIENTRY logRasterPos2f( GLfloat x, GLfloat y )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos2f\n" );
	dllRasterPos2f( x, y );
}

static void APIENTRY logRasterPos2fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos2dv\n" );
	dllRasterPos2fv( v );
}

static void APIENTRY logRasterPos2i( GLint x, GLint y )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos2if\n" );
	dllRasterPos2i( x, y );
}

static void APIENTRY logRasterPos2iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos2iv\n" );
	dllRasterPos2iv( v );
}

static void APIENTRY logRasterPos2s( GLshort x, GLshort y )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos2s\n" );
	dllRasterPos2s( x, y );
}

static void APIENTRY logRasterPos2sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos2sv\n" );
	dllRasterPos2sv( v );
}

static void APIENTRY logRasterPos3d( GLdouble x, GLdouble y, GLdouble z )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3d\n" );
	dllRasterPos3d( x, y, z );
}

static void APIENTRY logRasterPos3dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3dv\n" );
	dllRasterPos3dv( v );
}

static void APIENTRY logRasterPos3f( GLfloat x, GLfloat y, GLfloat z )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3f\n" );
	dllRasterPos3f( x, y, z );
}

static void APIENTRY logRasterPos3fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3fv\n" );
	dllRasterPos3fv( v );
}

static void APIENTRY logRasterPos3i( GLint x, GLint y, GLint z )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3i\n" );
	dllRasterPos3i( x, y, z );
}

static void APIENTRY logRasterPos3iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3iv\n" );
	dllRasterPos3iv( v );
}

static void APIENTRY logRasterPos3s( GLshort x, GLshort y, GLshort z )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3s\n" );
	dllRasterPos3s( x, y, z );
}

static void APIENTRY logRasterPos3sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos3sv\n" );
	dllRasterPos3sv( v );
}

static void APIENTRY logRasterPos4d( GLdouble x, GLdouble y, GLdouble z, GLdouble w )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4d\n" );
	dllRasterPos4d( x, y, z, w );
}

static void APIENTRY logRasterPos4dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4dv\n" );
	dllRasterPos4dv( v );
}

static void APIENTRY logRasterPos4f( GLfloat x, GLfloat y, GLfloat z, GLfloat w )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4f\n" );
	dllRasterPos4f( x, y, z, w );
}

static void APIENTRY logRasterPos4fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4fv\n" );
	dllRasterPos4fv( v );
}

static void APIENTRY logRasterPos4i( GLint x, GLint y, GLint z, GLint w )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4i\n" );
	dllRasterPos4i( x, y, z, w );
}

static void APIENTRY logRasterPos4iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4iv\n" );
	dllRasterPos4iv( v );
}

static void APIENTRY logRasterPos4s( GLshort x, GLshort y, GLshort z, GLshort w )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4s\n" );
	dllRasterPos4s( x, y, z, w );
}

static void APIENTRY logRasterPos4sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glRasterPos4sv\n" );
	dllRasterPos4sv( v );
}

static void APIENTRY logReadBuffer( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glReadBuffer\n" );
	dllReadBuffer( mode );
}

static void APIENTRY logReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
	void* pixels )
{
	FS_FPrintf( glw_state.log_fp, "glReadPixels\n" );
	dllReadPixels( x, y, width, height, format, type, pixels );
}

static void APIENTRY logRectd( GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2 )
{
	FS_FPrintf( glw_state.log_fp, "glRectd\n" );
	dllRectd( x1, y1, x2, y2 );
}

static void APIENTRY logRectdv( const GLdouble* v1, const GLdouble* v2 )
{
	FS_FPrintf( glw_state.log_fp, "glRectdv\n" );
	dllRectdv( v1, v2 );
}

static void APIENTRY logRectf( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 )
{
	FS_FPrintf( glw_state.log_fp, "glRectf\n" );
	dllRectf( x1, y1, x2, y2 );
}

static void APIENTRY logRectfv( const GLfloat* v1, const GLfloat* v2 )
{
	FS_FPrintf( glw_state.log_fp, "glRectfv\n" );
	dllRectfv( v1, v2 );
}

static void APIENTRY logRecti( GLint x1, GLint y1, GLint x2, GLint y2 )
{
	FS_FPrintf( glw_state.log_fp, "glRecti\n" );
	dllRecti( x1, y1, x2, y2 );
}

static void APIENTRY logRectiv( const GLint* v1, const GLint* v2 )
{
	FS_FPrintf( glw_state.log_fp, "glRectiv\n" );
	dllRectiv( v1, v2 );
}

static void APIENTRY logRects( GLshort x1, GLshort y1, GLshort x2, GLshort y2 )
{
	FS_FPrintf( glw_state.log_fp, "glRects\n" );
	dllRects( x1, y1, x2, y2 );
}

static void APIENTRY logRectsv( const GLshort* v1, const GLshort* v2 )
{
	FS_FPrintf( glw_state.log_fp, "glRectsv\n" );
	dllRectsv( v1, v2 );
}

static GLint APIENTRY logRenderMode( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glRenderMode\n" );
	return dllRenderMode( mode );
}

static void APIENTRY logRotated( GLdouble angle, GLdouble x, GLdouble y, GLdouble z )
{
	FS_FPrintf( glw_state.log_fp, "glRotated\n" );
	dllRotated( angle, x, y, z );
}

static void APIENTRY logRotatef( GLfloat angle, GLfloat x, GLfloat y, GLfloat z )
{
	FS_FPrintf( glw_state.log_fp, "glRotatef\n" );
	dllRotatef( angle, x, y, z );
}

static void APIENTRY logScaled( GLdouble x, GLdouble y, GLdouble z )
{
	FS_FPrintf( glw_state.log_fp, "glScaled\n" );
	dllScaled( x, y, z );
}

static void APIENTRY logScalef( GLfloat x, GLfloat y, GLfloat z )
{
	FS_FPrintf( glw_state.log_fp, "glScalef\n" );
	dllScalef( x, y, z );
}

static void APIENTRY logScissor( GLint x, GLint y, GLsizei width, GLsizei height )
{
	FS_FPrintf( glw_state.log_fp, "glScissor\n" );
	dllScissor( x, y, width, height );
}

static void APIENTRY logSelectBuffer( GLsizei size, GLuint* buffer )
{
	FS_FPrintf( glw_state.log_fp, "glSelectBuffer\n" );
	dllSelectBuffer( size, buffer );
}

static void APIENTRY logShadeModel( GLenum mode )
{
	FS_FPrintf( glw_state.log_fp, "glShadeModel\n" );
	dllShadeModel( mode );
}

static void APIENTRY logStencilFunc( GLenum func, GLint ref, GLuint mask )
{
	FS_FPrintf( glw_state.log_fp, "glStencilFunc\n" );
	dllStencilFunc( func, ref, mask );
}

static void APIENTRY logStencilMask( GLuint mask )
{
	FS_FPrintf( glw_state.log_fp, "glStencilMask\n" );
	dllStencilMask( mask );
}

static void APIENTRY logStencilOp( GLenum fail, GLenum zfail, GLenum zpass )
{
	FS_FPrintf( glw_state.log_fp, "glStencilOp\n" );
	dllStencilOp( fail, zfail, zpass );
}

static void APIENTRY logTexCoord1d( GLdouble s )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1d\n" );
	dllTexCoord1d( s );
}

static void APIENTRY logTexCoord1dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1dv\n" );
	dllTexCoord1dv( v );
}

static void APIENTRY logTexCoord1f( GLfloat s )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1f\n" );
	dllTexCoord1f( s );
}

static void APIENTRY logTexCoord1fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1fv\n" );
	dllTexCoord1fv( v );
}

static void APIENTRY logTexCoord1i( GLint s )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1i\n" );
	dllTexCoord1i( s );
}

static void APIENTRY logTexCoord1iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1iv\n" );
	dllTexCoord1iv( v );
}

static void APIENTRY logTexCoord1s( GLshort s )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1s\n" );
	dllTexCoord1s( s );
}

static void APIENTRY logTexCoord1sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord1sv\n" );
	dllTexCoord1sv( v );
}

static void APIENTRY logTexCoord2d( GLdouble s, GLdouble t )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2d\n" );
	dllTexCoord2d( s, t );
}

static void APIENTRY logTexCoord2dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2dv\n" );
	dllTexCoord2dv( v );
}

static void APIENTRY logTexCoord2f( GLfloat s, GLfloat t )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2f\n" );
	dllTexCoord2f( s, t );
}

static void APIENTRY logTexCoord2fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2fv\n" );
	dllTexCoord2fv( v );
}

static void APIENTRY logTexCoord2i( GLint s, GLint t )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2i\n" );
	dllTexCoord2i( s, t );
}

static void APIENTRY logTexCoord2iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2iv\n" );
	dllTexCoord2iv( v );
}

static void APIENTRY logTexCoord2s( GLshort s, GLshort t )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2s\n" );
	dllTexCoord2s( s, t );
}

static void APIENTRY logTexCoord2sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord2sv\n" );
	dllTexCoord2sv( v );
}

static void APIENTRY logTexCoord3d( GLdouble s, GLdouble t, GLdouble r )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3d\n" );
	dllTexCoord3d( s, t, r );
}

static void APIENTRY logTexCoord3dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3dv\n" );
	dllTexCoord3dv( v );
}

static void APIENTRY logTexCoord3f( GLfloat s, GLfloat t, GLfloat r )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3f\n" );
	dllTexCoord3f( s, t, r );
}

static void APIENTRY logTexCoord3fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3fv\n" );
	dllTexCoord3fv( v );
}

static void APIENTRY logTexCoord3i( GLint s, GLint t, GLint r )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3i\n" );
	dllTexCoord3i( s, t, r );
}

static void APIENTRY logTexCoord3iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3iv\n" );
	dllTexCoord3iv( v );
}

static void APIENTRY logTexCoord3s( GLshort s, GLshort t, GLshort r )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3s\n" );
	dllTexCoord3s( s, t, r );
}

static void APIENTRY logTexCoord3sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord3sv\n" );
	dllTexCoord3sv( v );
}

static void APIENTRY logTexCoord4d( GLdouble s, GLdouble t, GLdouble r, GLdouble q )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4d\n" );
	dllTexCoord4d( s, t, r, q );
}

static void APIENTRY logTexCoord4dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4dv\n" );
	dllTexCoord4dv( v );
}

static void APIENTRY logTexCoord4f( GLfloat s, GLfloat t, GLfloat r, GLfloat q )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4f\n" );
	dllTexCoord4f( s, t, r, q );
}

static void APIENTRY logTexCoord4fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4fv\n" );
	dllTexCoord4fv( v );
}

static void APIENTRY logTexCoord4i( GLint s, GLint t, GLint r, GLint q )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4i\n" );
	dllTexCoord4i( s, t, r, q );
}

static void APIENTRY logTexCoord4iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4iv\n" );
	dllTexCoord4iv( v );
}

static void APIENTRY logTexCoord4s( GLshort s, GLshort t, GLshort r, GLshort q )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4s\n" );
	dllTexCoord4s( s, t, r, q );
}

static void APIENTRY logTexCoord4sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoord4sv\n" );
	dllTexCoord4sv( v );
}

static void APIENTRY logTexCoordPointer( GLint size, GLenum type, GLsizei stride, const void* pointer )
{
	FS_FPrintf( glw_state.log_fp, "glTexCoordPointer\n" );
	dllTexCoordPointer( size, type, stride, pointer );
}

static void APIENTRY logTexEnvf( GLenum target, GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glTexEnvf( 0x%x, 0x%x, %f )\n", target, pname, param );
	dllTexEnvf( target, pname, param );
}

static void APIENTRY logTexEnvfv( GLenum target, GLenum pname, const GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glTexEnvfv\n" );
	dllTexEnvfv( target, pname, params );
}

static void APIENTRY logTexEnvi( GLenum target, GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glTexEnvi( 0x%x, 0x%x, 0x%x )\n", target, pname, param );
	dllTexEnvi( target, pname, param );
}

static void APIENTRY logTexEnviv( GLenum target, GLenum pname, const GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glTexEnviv\n" );
	dllTexEnviv( target, pname, params );
}

static void APIENTRY logTexGend( GLenum coord, GLenum pname, GLdouble param )
{
	FS_FPrintf( glw_state.log_fp, "glTexGend\n" );
	dllTexGend( coord, pname, param );
}

static void APIENTRY logTexGendv( GLenum coord, GLenum pname, const GLdouble* params )
{
	FS_FPrintf( glw_state.log_fp, "glTexGendv\n" );
	dllTexGendv( coord, pname, params );
}

static void APIENTRY logTexGenf( GLenum coord, GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glTexGenf\n" );
	dllTexGenf( coord, pname, param );
}

static void APIENTRY logTexGenfv( GLenum coord, GLenum pname, const GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glTexGenfv\n" );
	dllTexGenfv( coord, pname, params );
}

static void APIENTRY logTexGeni( GLenum coord, GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glTexGeni\n" );
	dllTexGeni( coord, pname, param );
}

static void APIENTRY logTexGeniv( GLenum coord, GLenum pname, const GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glTexGeniv\n" );
	dllTexGeniv( coord, pname, params );
}

static void APIENTRY logTexImage1D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border,
	GLenum format, GLenum type, const void* pixels )
{
	FS_FPrintf( glw_state.log_fp, "glTexImage1D\n" );
	dllTexImage1D( target, level, internalformat, width, border, format, type, pixels );
}

static void APIENTRY logTexImage2D( GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
	GLint border, GLenum format, GLenum type, const void* pixels )
{
	FS_FPrintf( glw_state.log_fp, "glTexImage2D\n" );
	dllTexImage2D( target, level, internalformat, width, height, border, format, type, pixels );
}

static void APIENTRY logTexParameterf( GLenum target, GLenum pname, GLfloat param )
{
	FS_FPrintf( glw_state.log_fp, "glTexParameterf( 0x%x, 0x%x, %f )\n", target, pname, param );
	dllTexParameterf( target, pname, param );
}

static void APIENTRY logTexParameterfv( GLenum target, GLenum pname, const GLfloat* params )
{
	FS_FPrintf( glw_state.log_fp, "glTexParameterfv\n" );
	dllTexParameterfv( target, pname, params );
}

static void APIENTRY logTexParameteri( GLenum target, GLenum pname, GLint param )
{
	FS_FPrintf( glw_state.log_fp, "glTexParameteri( 0x%x, 0x%x, 0x%x )\n", target, pname, param );
	dllTexParameteri( target, pname, param );
}

static void APIENTRY logTexParameteriv( GLenum target, GLenum pname, const GLint* params )
{
	FS_FPrintf( glw_state.log_fp, "glTexParameteriv\n" );
	dllTexParameteriv( target, pname, params );
}

static void APIENTRY logTexSubImage1D( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format,
	GLenum type, const void* pixels )
{
	FS_FPrintf( glw_state.log_fp, "glTexSubImage1D\n" );
	dllTexSubImage1D( target, level, xoffset, width, format, type, pixels );
}

static void APIENTRY logTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
	GLsizei height, GLenum format, GLenum type, const void* pixels )
{
	FS_FPrintf( glw_state.log_fp, "glTexSubImage2D\n" );
	dllTexSubImage2D( target, level, xoffset, yoffset, width, height, format, type, pixels );
}

static void APIENTRY logTranslated( GLdouble x, GLdouble y, GLdouble z )
{
	FS_FPrintf( glw_state.log_fp, "glTranslated\n" );
	dllTranslated( x, y, z );
}

static void APIENTRY logTranslatef( GLfloat x, GLfloat y, GLfloat z )
{
	FS_FPrintf( glw_state.log_fp, "glTranslatef\n" );
	dllTranslatef( x, y, z );
}

static void APIENTRY logVertex2d( GLdouble x, GLdouble y )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2d\n" );
	dllVertex2d( x, y );
}

static void APIENTRY logVertex2dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2dv\n" );
	dllVertex2dv( v );
}

static void APIENTRY logVertex2f( GLfloat x, GLfloat y )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2f\n" );
	dllVertex2f( x, y );
}

static void APIENTRY logVertex2fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2fv\n" );
	dllVertex2fv( v );
}

static void APIENTRY logVertex2i( GLint x, GLint y )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2i\n" );
	dllVertex2i( x, y );
}

static void APIENTRY logVertex2iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2iv\n" );
	dllVertex2iv( v );
}

static void APIENTRY logVertex2s( GLshort x, GLshort y )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2s\n" );
	dllVertex2s( x, y );
}

static void APIENTRY logVertex2sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex2sv\n" );
	dllVertex2sv( v );
}

static void APIENTRY logVertex3d( GLdouble x, GLdouble y, GLdouble z )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3d\n" );
	dllVertex3d( x, y, z );
}

static void APIENTRY logVertex3dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3dv\n" );
	dllVertex3dv( v );
}

static void APIENTRY logVertex3f( GLfloat x, GLfloat y, GLfloat z )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3f\n" );
	dllVertex3f( x, y, z );
}

static void APIENTRY logVertex3fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3fv\n" );
	dllVertex3fv( v );
}

static void APIENTRY logVertex3i( GLint x, GLint y, GLint z )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3i\n" );
	dllVertex3i( x, y, z );
}

static void APIENTRY logVertex3iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3iv\n" );
	dllVertex3iv( v );
}

static void APIENTRY logVertex3s( GLshort x, GLshort y, GLshort z )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3s\n" );
	dllVertex3s( x, y, z );
}

static void APIENTRY logVertex3sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex3sv\n" );
	dllVertex3sv( v );
}

static void APIENTRY logVertex4d( GLdouble x, GLdouble y, GLdouble z, GLdouble w )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4d\n" );
	dllVertex4d( x, y, z, w );
}

static void APIENTRY logVertex4dv( const GLdouble* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4dv\n" );
	dllVertex4dv( v );
}

static void APIENTRY logVertex4f( GLfloat x, GLfloat y, GLfloat z, GLfloat w )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4f\n" );
	dllVertex4f( x, y, z, w );
}

static void APIENTRY logVertex4fv( const GLfloat* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4fv\n" );
	dllVertex4fv( v );
}

static void APIENTRY logVertex4i( GLint x, GLint y, GLint z, GLint w )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4i\n" );
	dllVertex4i( x, y, z, w );
}

static void APIENTRY logVertex4iv( const GLint* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4iv\n" );
	dllVertex4iv( v );
}

static void APIENTRY logVertex4s( GLshort x, GLshort y, GLshort z, GLshort w )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4s\n" );
	dllVertex4s( x, y, z, w );
}

static void APIENTRY logVertex4sv( const GLshort* v )
{
	FS_FPrintf( glw_state.log_fp, "glVertex4sv\n" );
	dllVertex4sv( v );
}

static void APIENTRY logVertexPointer( GLint size, GLenum type, GLsizei stride, const void* pointer )
{
	FS_FPrintf( glw_state.log_fp, "glVertexPointer\n" );
	dllVertexPointer( size, type, stride, pointer );
}

static void APIENTRY logViewport( GLint x, GLint y, GLsizei width, GLsizei height )
{
	FS_FPrintf( glw_state.log_fp, "glViewport\n" );
	dllViewport( x, y, width, height );
}

extern "C" void QGL_Shutdown()
{
	glw_state.hinstOpenGL = NULL;

	qglAccum = NULL;
	qglAlphaFunc = NULL;
	qglAreTexturesResident = NULL;
	qglArrayElement = NULL;
	qglBegin = NULL;
	qglBindTexture = NULL;
	qglBitmap = NULL;
	qglBlendFunc = NULL;
	qglCallList = NULL;
	qglCallLists = NULL;
	qglClear = NULL;
	qglClearAccum = NULL;
	qglClearColor = NULL;
	qglClearDepth = NULL;
	qglClearIndex = NULL;
	qglClearStencil = NULL;
	qglClipPlane = NULL;
	qglColor3b = NULL;
	qglColor3bv = NULL;
	qglColor3d = NULL;
	qglColor3dv = NULL;
	qglColor3f = NULL;
	qglColor3fv = NULL;
	qglColor3i = NULL;
	qglColor3iv = NULL;
	qglColor3s = NULL;
	qglColor3sv = NULL;
	qglColor3ub = NULL;
	qglColor3ubv = NULL;
	qglColor3ui = NULL;
	qglColor3uiv = NULL;
	qglColor3us = NULL;
	qglColor3usv = NULL;
	qglColor4b = NULL;
	qglColor4bv = NULL;
	qglColor4d = NULL;
	qglColor4dv = NULL;
	qglColor4f = NULL;
	qglColor4fv = NULL;
	qglColor4i = NULL;
	qglColor4iv = NULL;
	qglColor4s = NULL;
	qglColor4sv = NULL;
	qglColor4ub = NULL;
	qglColor4ubv = NULL;
	qglColor4ui = NULL;
	qglColor4uiv = NULL;
	qglColor4us = NULL;
	qglColor4usv = NULL;
	qglColorMask = NULL;
	qglColorMaterial = NULL;
	qglColorPointer = NULL;
	qglCopyPixels = NULL;
	qglCopyTexImage1D = NULL;
	qglCopyTexImage2D = NULL;
	qglCopyTexSubImage1D = NULL;
	qglCopyTexSubImage2D = NULL;
	qglCullFace = NULL;
	qglDeleteLists = NULL;
	qglDeleteTextures = NULL;
	qglDepthFunc = NULL;
	qglDepthMask = NULL;
	qglDepthRange = NULL;
	qglDisable = NULL;
	qglDisableClientState = NULL;
	qglDrawArrays = NULL;
	qglDrawBuffer = NULL;
	qglDrawElements = NULL;
	qglDrawPixels = NULL;
	qglEdgeFlag = NULL;
	qglEdgeFlagPointer = NULL;
	qglEdgeFlagv = NULL;
	qglEnable = NULL;
	qglEnableClientState = NULL;
	qglEnd = NULL;
	qglEndList = NULL;
	qglEvalCoord1d = NULL;
	qglEvalCoord1dv = NULL;
	qglEvalCoord1f = NULL;
	qglEvalCoord1fv = NULL;
	qglEvalCoord2d = NULL;
	qglEvalCoord2dv = NULL;
	qglEvalCoord2f = NULL;
	qglEvalCoord2fv = NULL;
	qglEvalMesh1 = NULL;
	qglEvalMesh2 = NULL;
	qglEvalPoint1 = NULL;
	qglEvalPoint2 = NULL;
	qglFeedbackBuffer = NULL;
	qglFinish = NULL;
	qglFlush = NULL;
	qglFogf = NULL;
	qglFogfv = NULL;
	qglFogi = NULL;
	qglFogiv = NULL;
	qglFrontFace = NULL;
	qglFrustum = NULL;
	qglGenLists = NULL;
	qglGenTextures = NULL;
	qglGetBooleanv = NULL;
	qglGetClipPlane = NULL;
	qglGetDoublev = NULL;
	qglGetError = NULL;
	qglGetFloatv = NULL;
	qglGetIntegerv = NULL;
	qglGetLightfv = NULL;
	qglGetLightiv = NULL;
	qglGetMapdv = NULL;
	qglGetMapfv = NULL;
	qglGetMapiv = NULL;
	qglGetMaterialfv = NULL;
	qglGetMaterialiv = NULL;
	qglGetPixelMapfv = NULL;
	qglGetPixelMapuiv = NULL;
	qglGetPixelMapusv = NULL;
	qglGetPointerv = NULL;
	qglGetPolygonStipple = NULL;
	qglGetString = NULL;
	qglGetTexEnvfv = NULL;
	qglGetTexEnviv = NULL;
	qglGetTexGendv = NULL;
	qglGetTexGenfv = NULL;
	qglGetTexGeniv = NULL;
	qglGetTexImage = NULL;
	qglGetTexLevelParameterfv = NULL;
	qglGetTexLevelParameteriv = NULL;
	qglGetTexParameterfv = NULL;
	qglGetTexParameteriv = NULL;
	qglHint = NULL;
	qglIndexMask = NULL;
	qglIndexPointer = NULL;
	qglIndexd = NULL;
	qglIndexdv = NULL;
	qglIndexf = NULL;
	qglIndexfv = NULL;
	qglIndexi = NULL;
	qglIndexiv = NULL;
	qglIndexs = NULL;
	qglIndexsv = NULL;
	qglIndexub = NULL;
	qglIndexubv = NULL;
	qglInitNames = NULL;
	qglInterleavedArrays = NULL;
	qglIsEnabled = NULL;
	qglIsList = NULL;
	qglIsTexture = NULL;
	qglLightModelf = NULL;
	qglLightModelfv = NULL;
	qglLightModeli = NULL;
	qglLightModeliv = NULL;
	qglLightf = NULL;
	qglLightfv = NULL;
	qglLighti = NULL;
	qglLightiv = NULL;
	qglLineStipple = NULL;
	qglLineWidth = NULL;
	qglListBase = NULL;
	qglLoadIdentity = NULL;
	qglLoadMatrixd = NULL;
	qglLoadMatrixf = NULL;
	qglLoadName = NULL;
	qglLogicOp = NULL;
	qglMap1d = NULL;
	qglMap1f = NULL;
	qglMap2d = NULL;
	qglMap2f = NULL;
	qglMapGrid1d = NULL;
	qglMapGrid1f = NULL;
	qglMapGrid2d = NULL;
	qglMapGrid2f = NULL;
	qglMaterialf = NULL;
	qglMaterialfv = NULL;
	qglMateriali = NULL;
	qglMaterialiv = NULL;
	qglMatrixMode = NULL;
	qglMultMatrixd = NULL;
	qglMultMatrixf = NULL;
	qglNewList = NULL;
	qglNormal3b = NULL;
	qglNormal3bv = NULL;
	qglNormal3d = NULL;
	qglNormal3dv = NULL;
	qglNormal3f = NULL;
	qglNormal3fv = NULL;
	qglNormal3i = NULL;
	qglNormal3iv = NULL;
	qglNormal3s = NULL;
	qglNormal3sv = NULL;
	qglNormalPointer = NULL;
	qglOrtho = NULL;
	qglPassThrough = NULL;
	qglPixelMapfv = NULL;
	qglPixelMapuiv = NULL;
	qglPixelMapusv = NULL;
	qglPixelStoref = NULL;
	qglPixelStorei = NULL;
	qglPixelTransferf = NULL;
	qglPixelTransferi = NULL;
	qglPixelZoom = NULL;
	qglPointSize = NULL;
	qglPolygonMode = NULL;
	qglPolygonOffset = NULL;
	qglPolygonStipple = NULL;
	qglPopAttrib = NULL;
	qglPopClientAttrib = NULL;
	qglPopMatrix = NULL;
	qglPopName = NULL;
	qglPrioritizeTextures = NULL;
	qglPushAttrib = NULL;
	qglPushClientAttrib = NULL;
	qglPushMatrix = NULL;
	qglPushName = NULL;
	qglRasterPos2d = NULL;
	qglRasterPos2dv = NULL;
	qglRasterPos2f = NULL;
	qglRasterPos2fv = NULL;
	qglRasterPos2i = NULL;
	qglRasterPos2iv = NULL;
	qglRasterPos2s = NULL;
	qglRasterPos2sv = NULL;
	qglRasterPos3d = NULL;
	qglRasterPos3dv = NULL;
	qglRasterPos3f = NULL;
	qglRasterPos3fv = NULL;
	qglRasterPos3i = NULL;
	qglRasterPos3iv = NULL;
	qglRasterPos3s = NULL;
	qglRasterPos3sv = NULL;
	qglRasterPos4d = NULL;
	qglRasterPos4dv = NULL;
	qglRasterPos4f = NULL;
	qglRasterPos4fv = NULL;
	qglRasterPos4i = NULL;
	qglRasterPos4iv = NULL;
	qglRasterPos4s = NULL;
	qglRasterPos4sv = NULL;
	qglReadBuffer = NULL;
	qglReadPixels = NULL;
	qglRectd = NULL;
	qglRectdv = NULL;
	qglRectf = NULL;
	qglRectfv = NULL;
	qglRecti = NULL;
	qglRectiv = NULL;
	qglRects = NULL;
	qglRectsv = NULL;
	qglRenderMode = NULL;
	qglRotated = NULL;
	qglRotatef = NULL;
	qglScaled = NULL;
	qglScalef = NULL;
	qglScissor = NULL;
	qglSelectBuffer = NULL;
	qglShadeModel = NULL;
	qglStencilFunc = NULL;
	qglStencilMask = NULL;
	qglStencilOp = NULL;
	qglTexCoord1d = NULL;
	qglTexCoord1dv = NULL;
	qglTexCoord1f = NULL;
	qglTexCoord1fv = NULL;
	qglTexCoord1i = NULL;
	qglTexCoord1iv = NULL;
	qglTexCoord1s = NULL;
	qglTexCoord1sv = NULL;
	qglTexCoord2d = NULL;
	qglTexCoord2dv = NULL;
	qglTexCoord2f = NULL;
	qglTexCoord2fv = NULL;
	qglTexCoord2i = NULL;
	qglTexCoord2iv = NULL;
	qglTexCoord2s = NULL;
	qglTexCoord2sv = NULL;
	qglTexCoord3d = NULL;
	qglTexCoord3dv = NULL;
	qglTexCoord3f = NULL;
	qglTexCoord3fv = NULL;
	qglTexCoord3i = NULL;
	qglTexCoord3iv = NULL;
	qglTexCoord3s = NULL;
	qglTexCoord3sv = NULL;
	qglTexCoord4d = NULL;
	qglTexCoord4dv = NULL;
	qglTexCoord4f = NULL;
	qglTexCoord4fv = NULL;
	qglTexCoord4i = NULL;
	qglTexCoord4iv = NULL;
	qglTexCoord4s = NULL;
	qglTexCoord4sv = NULL;
	qglTexCoordPointer = NULL;
	qglTexEnvf = NULL;
	qglTexEnvfv = NULL;
	qglTexEnvi = NULL;
	qglTexEnviv = NULL;
	qglTexGend = NULL;
	qglTexGendv = NULL;
	qglTexGenf = NULL;
	qglTexGenfv = NULL;
	qglTexGeni = NULL;
	qglTexGeniv = NULL;
	qglTexImage1D = NULL;
	qglTexImage2D = NULL;
	qglTexParameterf = NULL;
	qglTexParameterfv = NULL;
	qglTexParameteri = NULL;
	qglTexParameteriv = NULL;
	qglTexSubImage1D = NULL;
	qglTexSubImage2D = NULL;
	qglTranslated = NULL;
	qglTranslatef = NULL;
	qglVertex2d = NULL;
	qglVertex2dv = NULL;
	qglVertex2f = NULL;
	qglVertex2fv = NULL;
	qglVertex2i = NULL;
	qglVertex2iv = NULL;
	qglVertex2s = NULL;
	qglVertex2sv = NULL;
	qglVertex3d = NULL;
	qglVertex3dv = NULL;
	qglVertex3f = NULL;
	qglVertex3fv = NULL;
	qglVertex3i = NULL;
	qglVertex3iv = NULL;
	qglVertex3s = NULL;
	qglVertex3sv = NULL;
	qglVertex4d = NULL;
	qglVertex4dv = NULL;
	qglVertex4f = NULL;
	qglVertex4fv = NULL;
	qglVertex4i = NULL;
	qglVertex4iv = NULL;
	qglVertex4s = NULL;
	qglVertex4sv = NULL;
	qglVertexPointer = NULL;
	qglViewport = NULL;

	qglCreateShader = NULL;
	qglDeleteShader = NULL;
	qglShaderSource = NULL;
	qglGetProgramiv = NULL;
	qglGetProgramInfoLog = NULL;
	qglAttachShader = NULL;
	qglLinkProgram = NULL;
	qglCreateProgram = NULL;
	qglDeleteProgram = NULL;
	qglUseProgram = NULL;
	qglCompileShader = NULL;
	qglGetShaderiv = NULL;
	qglGetShaderInfoLog = NULL;
	qglGetUniformLocation = NULL;
	qglUniform1ui = NULL;
	qwglSwapBuffers = NULL;
}

// Resolves the OpenGL entry points bound by QGL_Init().
static void* ( SDLCALL* QGL_GetProcAddress )( const char* name ) = SDL_GL_GetProcAddress;

#if !defined( _WIN32 )
// A dedicated server runs the renderer's resource code (texture uploads, light
// maps) without a GL context. QGL_InitNull() binds every entry point to a
// function that does nothing and returns 0. Calling it through the GL function
// types is safe here because APIENTRY is cdecl: the caller removes the arguments.
static int QGL_NullFunction()
{
	return 0;
}

static void* SDLCALL QGL_GetNullProcAddress( const char* )
{
	return reinterpret_cast<void*>( &QGL_NullFunction );
}

extern "C" HINSTANCE QGL_Init( const char*, const char* );

extern "C" HINSTANCE QGL_InitNull()
{
	QGL_GetProcAddress = QGL_GetNullProcAddress;
	return QGL_Init( NULL, NULL );
}
#endif

extern "C" HINSTANCE QGL_Init( const char*, const char* )
{
	glw_state.hinstOpenGL = (HINSTANCE)TRUE;

	qglAccum = dllAccum = (decltype( dllAccum ))QGL_GetProcAddress( "glAccum" );
	qglAlphaFunc = dllAlphaFunc = (decltype( dllAlphaFunc ))QGL_GetProcAddress( "glAlphaFunc" );
	qglAreTexturesResident = dllAreTexturesResident =
		(decltype( dllAreTexturesResident ))QGL_GetProcAddress( "glAreTexturesResident" );
	qglArrayElement = dllArrayElement = (decltype( dllArrayElement ))QGL_GetProcAddress( "glArrayElement" );
	qglBegin = dllBegin = (decltype( dllBegin ))QGL_GetProcAddress( "glBegin" );
	qglBindTexture = dllBindTexture = (decltype( dllBindTexture ))QGL_GetProcAddress( "glBindTexture" );
	qglBitmap = dllBitmap = (decltype( dllBitmap ))QGL_GetProcAddress( "glBitmap" );
	qglBlendFunc = dllBlendFunc = (decltype( dllBlendFunc ))QGL_GetProcAddress( "glBlendFunc" );
	qglCallList = dllCallList = (decltype( dllCallList ))QGL_GetProcAddress( "glCallList" );
	qglCallLists = dllCallLists = (decltype( dllCallLists ))QGL_GetProcAddress( "glCallLists" );
	qglClear = dllClear = (decltype( dllClear ))QGL_GetProcAddress( "glClear" );
	qglClearAccum = dllClearAccum = (decltype( dllClearAccum ))QGL_GetProcAddress( "glClearAccum" );
	qglClearColor = dllClearColor = (decltype( dllClearColor ))QGL_GetProcAddress( "glClearColor" );
	qglClearDepth = dllClearDepth = (decltype( dllClearDepth ))QGL_GetProcAddress( "glClearDepth" );
	qglClearIndex = dllClearIndex = (decltype( dllClearIndex ))QGL_GetProcAddress( "glClearIndex" );
	qglClearStencil = dllClearStencil = (decltype( dllClearStencil ))QGL_GetProcAddress( "glClearStencil" );
	qglClipPlane = dllClipPlane = (decltype( dllClipPlane ))QGL_GetProcAddress( "glClipPlane" );
	qglColor3b = dllColor3b = (decltype( dllColor3b ))QGL_GetProcAddress( "glColor3b" );
	qglColor3bv = dllColor3bv = (decltype( dllColor3bv ))QGL_GetProcAddress( "glColor3bv" );
	qglColor3d = dllColor3d = (decltype( dllColor3d ))QGL_GetProcAddress( "glColor3d" );
	qglColor3dv = dllColor3dv = (decltype( dllColor3dv ))QGL_GetProcAddress( "glColor3dv" );
	qglColor3f = dllColor3f = (decltype( dllColor3f ))QGL_GetProcAddress( "glColor3f" );
	qglColor3fv = dllColor3fv = (decltype( dllColor3fv ))QGL_GetProcAddress( "glColor3fv" );
	qglColor3i = dllColor3i = (decltype( dllColor3i ))QGL_GetProcAddress( "glColor3i" );
	qglColor3iv = dllColor3iv = (decltype( dllColor3iv ))QGL_GetProcAddress( "glColor3iv" );
	qglColor3s = dllColor3s = (decltype( dllColor3s ))QGL_GetProcAddress( "glColor3s" );
	qglColor3sv = dllColor3sv = (decltype( dllColor3sv ))QGL_GetProcAddress( "glColor3sv" );
	qglColor3ub = dllColor3ub = (decltype( dllColor3ub ))QGL_GetProcAddress( "glColor3ub" );
	qglColor3ubv = dllColor3ubv = (decltype( dllColor3ubv ))QGL_GetProcAddress( "glColor3ubv" );
	qglColor3ui = dllColor3ui = (decltype( dllColor3ui ))QGL_GetProcAddress( "glColor3ui" );
	qglColor3uiv = dllColor3uiv = (decltype( dllColor3uiv ))QGL_GetProcAddress( "glColor3uiv" );
	qglColor3us = dllColor3us = (decltype( dllColor3us ))QGL_GetProcAddress( "glColor3us" );
	qglColor3usv = dllColor3usv = (decltype( dllColor3usv ))QGL_GetProcAddress( "glColor3usv" );
	qglColor4b = dllColor4b = (decltype( dllColor4b ))QGL_GetProcAddress( "glColor4b" );
	qglColor4bv = dllColor4bv = (decltype( dllColor4bv ))QGL_GetProcAddress( "glColor4bv" );
	qglColor4d = dllColor4d = (decltype( dllColor4d ))QGL_GetProcAddress( "glColor4d" );
	qglColor4dv = dllColor4dv = (decltype( dllColor4dv ))QGL_GetProcAddress( "glColor4dv" );
	qglColor4f = dllColor4f = (decltype( dllColor4f ))QGL_GetProcAddress( "glColor4f" );
	qglColor4fv = dllColor4fv = (decltype( dllColor4fv ))QGL_GetProcAddress( "glColor4fv" );
	qglColor4i = dllColor4i = (decltype( dllColor4i ))QGL_GetProcAddress( "glColor4i" );
	qglColor4iv = dllColor4iv = (decltype( dllColor4iv ))QGL_GetProcAddress( "glColor4iv" );
	qglColor4s = dllColor4s = (decltype( dllColor4s ))QGL_GetProcAddress( "glColor4s" );
	qglColor4sv = dllColor4sv = (decltype( dllColor4sv ))QGL_GetProcAddress( "glColor4sv" );
	qglColor4ub = dllColor4ub = (decltype( dllColor4ub ))QGL_GetProcAddress( "glColor4ub" );
	qglColor4ubv = dllColor4ubv = (decltype( dllColor4ubv ))QGL_GetProcAddress( "glColor4ubv" );
	qglColor4ui = dllColor4ui = (decltype( dllColor4ui ))QGL_GetProcAddress( "glColor4ui" );
	qglColor4uiv = dllColor4uiv = (decltype( dllColor4uiv ))QGL_GetProcAddress( "glColor4uiv" );
	qglColor4us = dllColor4us = (decltype( dllColor4us ))QGL_GetProcAddress( "glColor4us" );
	qglColor4usv = dllColor4usv = (decltype( dllColor4usv ))QGL_GetProcAddress( "glColor4usv" );
	qglColorMask = dllColorMask = (decltype( dllColorMask ))QGL_GetProcAddress( "glColorMask" );
	qglColorMaterial = dllColorMaterial = (decltype( dllColorMaterial ))QGL_GetProcAddress( "glColorMaterial" );
	qglColorPointer = dllColorPointer = (decltype( dllColorPointer ))QGL_GetProcAddress( "glColorPointer" );
	qglCopyPixels = dllCopyPixels = (decltype( dllCopyPixels ))QGL_GetProcAddress( "glCopyPixels" );
	qglCopyTexImage1D = dllCopyTexImage1D = (decltype( dllCopyTexImage1D ))QGL_GetProcAddress( "glCopyTexImage1D" );
	qglCopyTexImage2D = dllCopyTexImage2D = (decltype( dllCopyTexImage2D ))QGL_GetProcAddress( "glCopyTexImage2D" );
	qglCopyTexSubImage1D = dllCopyTexSubImage1D =
		(decltype( dllCopyTexSubImage1D ))QGL_GetProcAddress( "glCopyTexSubImage1D" );
	qglCopyTexSubImage2D = dllCopyTexSubImage2D =
		(decltype( dllCopyTexSubImage2D ))QGL_GetProcAddress( "glCopyTexSubImage2D" );
	qglCullFace = dllCullFace = (decltype( dllCullFace ))QGL_GetProcAddress( "glCullFace" );
	qglDeleteLists = dllDeleteLists = (decltype( dllDeleteLists ))QGL_GetProcAddress( "glDeleteLists" );
	qglDeleteTextures = dllDeleteTextures = (decltype( dllDeleteTextures ))QGL_GetProcAddress( "glDeleteTextures" );
	qglDepthFunc = dllDepthFunc = (decltype( dllDepthFunc ))QGL_GetProcAddress( "glDepthFunc" );
	qglDepthMask = dllDepthMask = (decltype( dllDepthMask ))QGL_GetProcAddress( "glDepthMask" );
	qglDepthRange = dllDepthRange = (decltype( dllDepthRange ))QGL_GetProcAddress( "glDepthRange" );
	qglDisable = dllDisable = (decltype( dllDisable ))QGL_GetProcAddress( "glDisable" );
	qglDisableClientState = dllDisableClientState =
		(decltype( dllDisableClientState ))QGL_GetProcAddress( "glDisableClientState" );
	qglDrawArrays = dllDrawArrays = (decltype( dllDrawArrays ))QGL_GetProcAddress( "glDrawArrays" );
	qglDrawBuffer = dllDrawBuffer = (decltype( dllDrawBuffer ))QGL_GetProcAddress( "glDrawBuffer" );
	qglDrawElements = dllDrawElements = (decltype( dllDrawElements ))QGL_GetProcAddress( "glDrawElements" );
	qglDrawPixels = dllDrawPixels = (decltype( dllDrawPixels ))QGL_GetProcAddress( "glDrawPixels" );
	qglEdgeFlag = dllEdgeFlag = (decltype( dllEdgeFlag ))QGL_GetProcAddress( "glEdgeFlag" );
	qglEdgeFlagPointer = dllEdgeFlagPointer =
		(decltype( dllEdgeFlagPointer ))QGL_GetProcAddress( "glEdgeFlagPointer" );
	qglEdgeFlagv = dllEdgeFlagv = (decltype( dllEdgeFlagv ))QGL_GetProcAddress( "glEdgeFlagv" );
	qglEnable = dllEnable = (decltype( dllEnable ))QGL_GetProcAddress( "glEnable" );
	qglEnableClientState = dllEnableClientState =
		(decltype( dllEnableClientState ))QGL_GetProcAddress( "glEnableClientState" );
	qglEnd = dllEnd = (decltype( dllEnd ))QGL_GetProcAddress( "glEnd" );
	qglEndList = dllEndList = (decltype( dllEndList ))QGL_GetProcAddress( "glEndList" );
	qglEvalCoord1d = dllEvalCoord1d = (decltype( dllEvalCoord1d ))QGL_GetProcAddress( "glEvalCoord1d" );
	qglEvalCoord1dv = dllEvalCoord1dv = (decltype( dllEvalCoord1dv ))QGL_GetProcAddress( "glEvalCoord1dv" );
	qglEvalCoord1f = dllEvalCoord1f = (decltype( dllEvalCoord1f ))QGL_GetProcAddress( "glEvalCoord1f" );
	qglEvalCoord1fv = dllEvalCoord1fv = (decltype( dllEvalCoord1fv ))QGL_GetProcAddress( "glEvalCoord1fv" );
	qglEvalCoord2d = dllEvalCoord2d = (decltype( dllEvalCoord2d ))QGL_GetProcAddress( "glEvalCoord2d" );
	qglEvalCoord2dv = dllEvalCoord2dv = (decltype( dllEvalCoord2dv ))QGL_GetProcAddress( "glEvalCoord2dv" );
	qglEvalCoord2f = dllEvalCoord2f = (decltype( dllEvalCoord2f ))QGL_GetProcAddress( "glEvalCoord2f" );
	qglEvalCoord2fv = dllEvalCoord2fv = (decltype( dllEvalCoord2fv ))QGL_GetProcAddress( "glEvalCoord2fv" );
	qglEvalMesh1 = dllEvalMesh1 = (decltype( dllEvalMesh1 ))QGL_GetProcAddress( "glEvalMesh1" );
	qglEvalMesh2 = dllEvalMesh2 = (decltype( dllEvalMesh2 ))QGL_GetProcAddress( "glEvalMesh2" );
	qglEvalPoint1 = dllEvalPoint1 = (decltype( dllEvalPoint1 ))QGL_GetProcAddress( "glEvalPoint1" );
	qglEvalPoint2 = dllEvalPoint2 = (decltype( dllEvalPoint2 ))QGL_GetProcAddress( "glEvalPoint2" );
	qglFeedbackBuffer = dllFeedbackBuffer = (decltype( dllFeedbackBuffer ))QGL_GetProcAddress( "glFeedbackBuffer" );
	qglFinish = dllFinish = (decltype( dllFinish ))QGL_GetProcAddress( "glFinish" );
	qglFlush = dllFlush = (decltype( dllFlush ))QGL_GetProcAddress( "glFlush" );
	qglFogf = dllFogf = (decltype( dllFogf ))QGL_GetProcAddress( "glFogf" );
	qglFogfv = dllFogfv = (decltype( dllFogfv ))QGL_GetProcAddress( "glFogfv" );
	qglFogi = dllFogi = (decltype( dllFogi ))QGL_GetProcAddress( "glFogi" );
	qglFogiv = dllFogiv = (decltype( dllFogiv ))QGL_GetProcAddress( "glFogiv" );
	qglFrontFace = dllFrontFace = (decltype( dllFrontFace ))QGL_GetProcAddress( "glFrontFace" );
	qglFrustum = dllFrustum = (decltype( dllFrustum ))QGL_GetProcAddress( "glFrustum" );
	qglGenLists = dllGenLists = (decltype( dllGenLists ))QGL_GetProcAddress( "glGenLists" );
	qglGenTextures = dllGenTextures = (decltype( dllGenTextures ))QGL_GetProcAddress( "glGenTextures" );
	qglGetBooleanv = dllGetBooleanv = (decltype( dllGetBooleanv ))QGL_GetProcAddress( "glGetBooleanv" );
	qglGetClipPlane = dllGetClipPlane = (decltype( dllGetClipPlane ))QGL_GetProcAddress( "glGetClipPlane" );
	qglGetDoublev = dllGetDoublev = (decltype( dllGetDoublev ))QGL_GetProcAddress( "glGetDoublev" );
	qglGetError = dllGetError = (decltype( dllGetError ))QGL_GetProcAddress( "glGetError" );
	qglGetFloatv = dllGetFloatv = (decltype( dllGetFloatv ))QGL_GetProcAddress( "glGetFloatv" );
	qglGetIntegerv = dllGetIntegerv = (decltype( dllGetIntegerv ))QGL_GetProcAddress( "glGetIntegerv" );
	qglGetLightfv = dllGetLightfv = (decltype( dllGetLightfv ))QGL_GetProcAddress( "glGetLightfv" );
	qglGetLightiv = dllGetLightiv = (decltype( dllGetLightiv ))QGL_GetProcAddress( "glGetLightiv" );
	qglGetMapdv = dllGetMapdv = (decltype( dllGetMapdv ))QGL_GetProcAddress( "glGetMapdv" );
	qglGetMapfv = dllGetMapfv = (decltype( dllGetMapfv ))QGL_GetProcAddress( "glGetMapfv" );
	qglGetMapiv = dllGetMapiv = (decltype( dllGetMapiv ))QGL_GetProcAddress( "glGetMapiv" );
	qglGetMaterialfv = dllGetMaterialfv = (decltype( dllGetMaterialfv ))QGL_GetProcAddress( "glGetMaterialfv" );
	qglGetMaterialiv = dllGetMaterialiv = (decltype( dllGetMaterialiv ))QGL_GetProcAddress( "glGetMaterialiv" );
	qglGetPixelMapfv = dllGetPixelMapfv = (decltype( dllGetPixelMapfv ))QGL_GetProcAddress( "glGetPixelMapfv" );
	qglGetPixelMapuiv = dllGetPixelMapuiv = (decltype( dllGetPixelMapuiv ))QGL_GetProcAddress( "glGetPixelMapuiv" );
	qglGetPixelMapusv = dllGetPixelMapusv = (decltype( dllGetPixelMapusv ))QGL_GetProcAddress( "glGetPixelMapusv" );
	qglGetPointerv = dllGetPointerv = (decltype( dllGetPointerv ))QGL_GetProcAddress( "glGetPointerv" );
	qglGetPolygonStipple = dllGetPolygonStipple =
		(decltype( dllGetPolygonStipple ))QGL_GetProcAddress( "glGetPolygonStipple" );
	qglGetString = dllGetString = (decltype( dllGetString ))QGL_GetProcAddress( "glGetString" );
	qglGetTexEnvfv = dllGetTexEnvfv = (decltype( dllGetTexEnvfv ))QGL_GetProcAddress( "glGetTexEnvfv" );
	qglGetTexEnviv = dllGetTexEnviv = (decltype( dllGetTexEnviv ))QGL_GetProcAddress( "glGetTexEnviv" );
	qglGetTexGendv = dllGetTexGendv = (decltype( dllGetTexGendv ))QGL_GetProcAddress( "glGetTexGendv" );
	qglGetTexGenfv = dllGetTexGenfv = (decltype( dllGetTexGenfv ))QGL_GetProcAddress( "glGetTexGenfv" );
	qglGetTexGeniv = dllGetTexGeniv = (decltype( dllGetTexGeniv ))QGL_GetProcAddress( "glGetTexGeniv" );
	qglGetTexImage = dllGetTexImage = (decltype( dllGetTexImage ))QGL_GetProcAddress( "glGetTexImage" );
	qglGetTexLevelParameterfv = dllGetTexLevelParameterfv =
		(decltype( dllGetTexLevelParameterfv ))QGL_GetProcAddress( "glGetLevelParameterfv" );
	qglGetTexLevelParameteriv = dllGetTexLevelParameteriv =
		(decltype( dllGetTexLevelParameteriv ))QGL_GetProcAddress( "glGetLevelParameteriv" );
	qglGetTexParameterfv = dllGetTexParameterfv =
		(decltype( dllGetTexParameterfv ))QGL_GetProcAddress( "glGetTexParameterfv" );
	qglGetTexParameteriv = dllGetTexParameteriv =
		(decltype( dllGetTexParameteriv ))QGL_GetProcAddress( "glGetTexParameteriv" );
	qglHint = dllHint = (decltype( dllHint ))QGL_GetProcAddress( "glHint" );
	qglIndexMask = dllIndexMask = (decltype( dllIndexMask ))QGL_GetProcAddress( "glIndexMask" );
	qglIndexPointer = dllIndexPointer = (decltype( dllIndexPointer ))QGL_GetProcAddress( "glIndexPointer" );
	qglIndexd = dllIndexd = (decltype( dllIndexd ))QGL_GetProcAddress( "glIndexd" );
	qglIndexdv = dllIndexdv = (decltype( dllIndexdv ))QGL_GetProcAddress( "glIndexdv" );
	qglIndexf = dllIndexf = (decltype( dllIndexf ))QGL_GetProcAddress( "glIndexf" );
	qglIndexfv = dllIndexfv = (decltype( dllIndexfv ))QGL_GetProcAddress( "glIndexfv" );
	qglIndexi = dllIndexi = (decltype( dllIndexi ))QGL_GetProcAddress( "glIndexi" );
	qglIndexiv = dllIndexiv = (decltype( dllIndexiv ))QGL_GetProcAddress( "glIndexiv" );
	qglIndexs = dllIndexs = (decltype( dllIndexs ))QGL_GetProcAddress( "glIndexs" );
	qglIndexsv = dllIndexsv = (decltype( dllIndexsv ))QGL_GetProcAddress( "glIndexsv" );
	qglIndexub = dllIndexub = (decltype( dllIndexub ))QGL_GetProcAddress( "glIndexub" );
	qglIndexubv = dllIndexubv = (decltype( dllIndexubv ))QGL_GetProcAddress( "glIndexubv" );
	qglInitNames = dllInitNames = (decltype( dllInitNames ))QGL_GetProcAddress( "glInitNames" );
	qglInterleavedArrays = dllInterleavedArrays =
		(decltype( dllInterleavedArrays ))QGL_GetProcAddress( "glInterleavedArrays" );
	qglIsEnabled = dllIsEnabled = (decltype( dllIsEnabled ))QGL_GetProcAddress( "glIsEnabled" );
	qglIsList = dllIsList = (decltype( dllIsList ))QGL_GetProcAddress( "glIsList" );
	qglIsTexture = dllIsTexture = (decltype( dllIsTexture ))QGL_GetProcAddress( "glIsTexture" );
	qglLightModelf = dllLightModelf = (decltype( dllLightModelf ))QGL_GetProcAddress( "glLightModelf" );
	qglLightModelfv = dllLightModelfv = (decltype( dllLightModelfv ))QGL_GetProcAddress( "glLightModelfv" );
	qglLightModeli = dllLightModeli = (decltype( dllLightModeli ))QGL_GetProcAddress( "glLightModeli" );
	qglLightModeliv = dllLightModeliv = (decltype( dllLightModeliv ))QGL_GetProcAddress( "glLightModeliv" );
	qglLightf = dllLightf = (decltype( dllLightf ))QGL_GetProcAddress( "glLightf" );
	qglLightfv = dllLightfv = (decltype( dllLightfv ))QGL_GetProcAddress( "glLightfv" );
	qglLighti = dllLighti = (decltype( dllLighti ))QGL_GetProcAddress( "glLighti" );
	qglLightiv = dllLightiv = (decltype( dllLightiv ))QGL_GetProcAddress( "glLightiv" );
	qglLineStipple = dllLineStipple = (decltype( dllLineStipple ))QGL_GetProcAddress( "glLineStipple" );
	qglLineWidth = dllLineWidth = (decltype( dllLineWidth ))QGL_GetProcAddress( "glLineWidth" );
	qglListBase = dllListBase = (decltype( dllListBase ))QGL_GetProcAddress( "glListBase" );
	qglLoadIdentity = dllLoadIdentity = (decltype( dllLoadIdentity ))QGL_GetProcAddress( "glLoadIdentity" );
	qglLoadMatrixd = dllLoadMatrixd = (decltype( dllLoadMatrixd ))QGL_GetProcAddress( "glLoadMatrixd" );
	qglLoadMatrixf = dllLoadMatrixf = (decltype( dllLoadMatrixf ))QGL_GetProcAddress( "glLoadMatrixf" );
	qglLoadName = dllLoadName = (decltype( dllLoadName ))QGL_GetProcAddress( "glLoadName" );
	qglLogicOp = dllLogicOp = (decltype( dllLogicOp ))QGL_GetProcAddress( "glLogicOp" );
	qglMap1d = dllMap1d = (decltype( dllMap1d ))QGL_GetProcAddress( "glMap1d" );
	qglMap1f = dllMap1f = (decltype( dllMap1f ))QGL_GetProcAddress( "glMap1f" );
	qglMap2d = dllMap2d = (decltype( dllMap2d ))QGL_GetProcAddress( "glMap2d" );
	qglMap2f = dllMap2f = (decltype( dllMap2f ))QGL_GetProcAddress( "glMap2f" );
	qglMapGrid1d = dllMapGrid1d = (decltype( dllMapGrid1d ))QGL_GetProcAddress( "glMapGrid1d" );
	qglMapGrid1f = dllMapGrid1f = (decltype( dllMapGrid1f ))QGL_GetProcAddress( "glMapGrid1f" );
	qglMapGrid2d = dllMapGrid2d = (decltype( dllMapGrid2d ))QGL_GetProcAddress( "glMapGrid2d" );
	qglMapGrid2f = dllMapGrid2f = (decltype( dllMapGrid2f ))QGL_GetProcAddress( "glMapGrid2f" );
	qglMaterialf = dllMaterialf = (decltype( dllMaterialf ))QGL_GetProcAddress( "glMaterialf" );
	qglMaterialfv = dllMaterialfv = (decltype( dllMaterialfv ))QGL_GetProcAddress( "glMaterialfv" );
	qglMateriali = dllMateriali = (decltype( dllMateriali ))QGL_GetProcAddress( "glMateriali" );
	qglMaterialiv = dllMaterialiv = (decltype( dllMaterialiv ))QGL_GetProcAddress( "glMaterialiv" );
	qglMatrixMode = dllMatrixMode = (decltype( dllMatrixMode ))QGL_GetProcAddress( "glMatrixMode" );
	qglMultMatrixd = dllMultMatrixd = (decltype( dllMultMatrixd ))QGL_GetProcAddress( "glMultMatrixd" );
	qglMultMatrixf = dllMultMatrixf = (decltype( dllMultMatrixf ))QGL_GetProcAddress( "glMultMatrixf" );
	qglNewList = dllNewList = (decltype( dllNewList ))QGL_GetProcAddress( "glNewList" );
	qglNormal3b = dllNormal3b = (decltype( dllNormal3b ))QGL_GetProcAddress( "glNormal3b" );
	qglNormal3bv = dllNormal3bv = (decltype( dllNormal3bv ))QGL_GetProcAddress( "glNormal3bv" );
	qglNormal3d = dllNormal3d = (decltype( dllNormal3d ))QGL_GetProcAddress( "glNormal3d" );
	qglNormal3dv = dllNormal3dv = (decltype( dllNormal3dv ))QGL_GetProcAddress( "glNormal3dv" );
	qglNormal3f = dllNormal3f = (decltype( dllNormal3f ))QGL_GetProcAddress( "glNormal3f" );
	qglNormal3fv = dllNormal3fv = (decltype( dllNormal3fv ))QGL_GetProcAddress( "glNormal3fv" );
	qglNormal3i = dllNormal3i = (decltype( dllNormal3i ))QGL_GetProcAddress( "glNormal3i" );
	qglNormal3iv = dllNormal3iv = (decltype( dllNormal3iv ))QGL_GetProcAddress( "glNormal3iv" );
	qglNormal3s = dllNormal3s = (decltype( dllNormal3s ))QGL_GetProcAddress( "glNormal3s" );
	qglNormal3sv = dllNormal3sv = (decltype( dllNormal3sv ))QGL_GetProcAddress( "glNormal3sv" );
	qglNormalPointer = dllNormalPointer = (decltype( dllNormalPointer ))QGL_GetProcAddress( "glNormalPointer" );
	qglOrtho = dllOrtho = (decltype( dllOrtho ))QGL_GetProcAddress( "glOrtho" );
	qglPassThrough = dllPassThrough = (decltype( dllPassThrough ))QGL_GetProcAddress( "glPassThrough" );
	qglPixelMapfv = dllPixelMapfv = (decltype( dllPixelMapfv ))QGL_GetProcAddress( "glPixelMapfv" );
	qglPixelMapuiv = dllPixelMapuiv = (decltype( dllPixelMapuiv ))QGL_GetProcAddress( "glPixelMapuiv" );
	qglPixelMapusv = dllPixelMapusv = (decltype( dllPixelMapusv ))QGL_GetProcAddress( "glPixelMapusv" );
	qglPixelStoref = dllPixelStoref = (decltype( dllPixelStoref ))QGL_GetProcAddress( "glPixelStoref" );
	qglPixelStorei = dllPixelStorei = (decltype( dllPixelStorei ))QGL_GetProcAddress( "glPixelStorei" );
	qglPixelTransferf = dllPixelTransferf = (decltype( dllPixelTransferf ))QGL_GetProcAddress( "glPixelTransferf" );
	qglPixelTransferi = dllPixelTransferi = (decltype( dllPixelTransferi ))QGL_GetProcAddress( "glPixelTransferi" );
	qglPixelZoom = dllPixelZoom = (decltype( dllPixelZoom ))QGL_GetProcAddress( "glPixelZoom" );
	qglPointSize = dllPointSize = (decltype( dllPointSize ))QGL_GetProcAddress( "glPointSize" );
	qglPolygonMode = dllPolygonMode = (decltype( dllPolygonMode ))QGL_GetProcAddress( "glPolygonMode" );
	qglPolygonOffset = dllPolygonOffset = (decltype( dllPolygonOffset ))QGL_GetProcAddress( "glPolygonOffset" );
	qglPolygonStipple = dllPolygonStipple = (decltype( dllPolygonStipple ))QGL_GetProcAddress( "glPolygonStipple" );
	qglPopAttrib = dllPopAttrib = (decltype( dllPopAttrib ))QGL_GetProcAddress( "glPopAttrib" );
	qglPopClientAttrib = dllPopClientAttrib =
		(decltype( dllPopClientAttrib ))QGL_GetProcAddress( "glPopClientAttrib" );
	qglPopMatrix = dllPopMatrix = (decltype( dllPopMatrix ))QGL_GetProcAddress( "glPopMatrix" );
	qglPopName = dllPopName = (decltype( dllPopName ))QGL_GetProcAddress( "glPopName" );
	qglPrioritizeTextures = dllPrioritizeTextures =
		(decltype( dllPrioritizeTextures ))QGL_GetProcAddress( "glPrioritizeTextures" );
	qglPushAttrib = dllPushAttrib = (decltype( dllPushAttrib ))QGL_GetProcAddress( "glPushAttrib" );
	qglPushClientAttrib = dllPushClientAttrib =
		(decltype( dllPushClientAttrib ))QGL_GetProcAddress( "glPushClientAttrib" );
	qglPushMatrix = dllPushMatrix = (decltype( dllPushMatrix ))QGL_GetProcAddress( "glPushMatrix" );
	qglPushName = dllPushName = (decltype( dllPushName ))QGL_GetProcAddress( "glPushName" );
	qglRasterPos2d = dllRasterPos2d = (decltype( dllRasterPos2d ))QGL_GetProcAddress( "glRasterPos2d" );
	qglRasterPos2dv = dllRasterPos2dv = (decltype( dllRasterPos2dv ))QGL_GetProcAddress( "glRasterPos2dv" );
	qglRasterPos2f = dllRasterPos2f = (decltype( dllRasterPos2f ))QGL_GetProcAddress( "glRasterPos2f" );
	qglRasterPos2fv = dllRasterPos2fv = (decltype( dllRasterPos2fv ))QGL_GetProcAddress( "glRasterPos2fv" );
	qglRasterPos2i = dllRasterPos2i = (decltype( dllRasterPos2i ))QGL_GetProcAddress( "glRasterPos2i" );
	qglRasterPos2iv = dllRasterPos2iv = (decltype( dllRasterPos2iv ))QGL_GetProcAddress( "glRasterPos2iv" );
	qglRasterPos2s = dllRasterPos2s = (decltype( dllRasterPos2s ))QGL_GetProcAddress( "glRasterPos2s" );
	qglRasterPos2sv = dllRasterPos2sv = (decltype( dllRasterPos2sv ))QGL_GetProcAddress( "glRasterPos2sv" );
	qglRasterPos3d = dllRasterPos3d = (decltype( dllRasterPos3d ))QGL_GetProcAddress( "glRasterPos3d" );
	qglRasterPos3dv = dllRasterPos3dv = (decltype( dllRasterPos3dv ))QGL_GetProcAddress( "glRasterPos3dv" );
	qglRasterPos3f = dllRasterPos3f = (decltype( dllRasterPos3f ))QGL_GetProcAddress( "glRasterPos3f" );
	qglRasterPos3fv = dllRasterPos3fv = (decltype( dllRasterPos3fv ))QGL_GetProcAddress( "glRasterPos3fv" );
	qglRasterPos3i = dllRasterPos3i = (decltype( dllRasterPos3i ))QGL_GetProcAddress( "glRasterPos3i" );
	qglRasterPos3iv = dllRasterPos3iv = (decltype( dllRasterPos3iv ))QGL_GetProcAddress( "glRasterPos3iv" );
	qglRasterPos3s = dllRasterPos3s = (decltype( dllRasterPos3s ))QGL_GetProcAddress( "glRasterPos3s" );
	qglRasterPos3sv = dllRasterPos3sv = (decltype( dllRasterPos3sv ))QGL_GetProcAddress( "glRasterPos3sv" );
	qglRasterPos4d = dllRasterPos4d = (decltype( dllRasterPos4d ))QGL_GetProcAddress( "glRasterPos4d" );
	qglRasterPos4dv = dllRasterPos4dv = (decltype( dllRasterPos4dv ))QGL_GetProcAddress( "glRasterPos4dv" );
	qglRasterPos4f = dllRasterPos4f = (decltype( dllRasterPos4f ))QGL_GetProcAddress( "glRasterPos4f" );
	qglRasterPos4fv = dllRasterPos4fv = (decltype( dllRasterPos4fv ))QGL_GetProcAddress( "glRasterPos4fv" );
	qglRasterPos4i = dllRasterPos4i = (decltype( dllRasterPos4i ))QGL_GetProcAddress( "glRasterPos4i" );
	qglRasterPos4iv = dllRasterPos4iv = (decltype( dllRasterPos4iv ))QGL_GetProcAddress( "glRasterPos4iv" );
	qglRasterPos4s = dllRasterPos4s = (decltype( dllRasterPos4s ))QGL_GetProcAddress( "glRasterPos4s" );
	qglRasterPos4sv = dllRasterPos4sv = (decltype( dllRasterPos4sv ))QGL_GetProcAddress( "glRasterPos4sv" );
	qglReadBuffer = dllReadBuffer = (decltype( dllReadBuffer ))QGL_GetProcAddress( "glReadBuffer" );
	qglReadPixels = dllReadPixels = (decltype( dllReadPixels ))QGL_GetProcAddress( "glReadPixels" );
	qglRectd = dllRectd = (decltype( dllRectd ))QGL_GetProcAddress( "glRectd" );
	qglRectdv = dllRectdv = (decltype( dllRectdv ))QGL_GetProcAddress( "glRectdv" );
	qglRectf = dllRectf = (decltype( dllRectf ))QGL_GetProcAddress( "glRectf" );
	qglRectfv = dllRectfv = (decltype( dllRectfv ))QGL_GetProcAddress( "glRectfv" );
	qglRecti = dllRecti = (decltype( dllRecti ))QGL_GetProcAddress( "glRecti" );
	qglRectiv = dllRectiv = (decltype( dllRectiv ))QGL_GetProcAddress( "glRectiv" );
	qglRects = dllRects = (decltype( dllRects ))QGL_GetProcAddress( "glRects" );
	qglRectsv = dllRectsv = (decltype( dllRectsv ))QGL_GetProcAddress( "glRectsv" );
	qglRenderMode = dllRenderMode = (decltype( dllRenderMode ))QGL_GetProcAddress( "glRenderMode" );
	qglRotated = dllRotated = (decltype( dllRotated ))QGL_GetProcAddress( "glRotated" );
	qglRotatef = dllRotatef = (decltype( dllRotatef ))QGL_GetProcAddress( "glRotatef" );
	qglScaled = dllScaled = (decltype( dllScaled ))QGL_GetProcAddress( "glScaled" );
	qglScalef = dllScalef = (decltype( dllScalef ))QGL_GetProcAddress( "glScalef" );
	qglScissor = dllScissor = (decltype( dllScissor ))QGL_GetProcAddress( "glScissor" );
	qglSelectBuffer = dllSelectBuffer = (decltype( dllSelectBuffer ))QGL_GetProcAddress( "glSelectBuffer" );
	qglShadeModel = dllShadeModel = (decltype( dllShadeModel ))QGL_GetProcAddress( "glShadeModel" );
	qglStencilFunc = dllStencilFunc = (decltype( dllStencilFunc ))QGL_GetProcAddress( "glStencilFunc" );
	qglStencilMask = dllStencilMask = (decltype( dllStencilMask ))QGL_GetProcAddress( "glStencilMask" );
	qglStencilOp = dllStencilOp = (decltype( dllStencilOp ))QGL_GetProcAddress( "glStencilOp" );
	qglTexCoord1d = dllTexCoord1d = (decltype( dllTexCoord1d ))QGL_GetProcAddress( "glTexCoord1d" );
	qglTexCoord1dv = dllTexCoord1dv = (decltype( dllTexCoord1dv ))QGL_GetProcAddress( "glTexCoord1dv" );
	qglTexCoord1f = dllTexCoord1f = (decltype( dllTexCoord1f ))QGL_GetProcAddress( "glTexCoord1f" );
	qglTexCoord1fv = dllTexCoord1fv = (decltype( dllTexCoord1fv ))QGL_GetProcAddress( "glTexCoord1fv" );
	qglTexCoord1i = dllTexCoord1i = (decltype( dllTexCoord1i ))QGL_GetProcAddress( "glTexCoord1i" );
	qglTexCoord1iv = dllTexCoord1iv = (decltype( dllTexCoord1iv ))QGL_GetProcAddress( "glTexCoord1iv" );
	qglTexCoord1s = dllTexCoord1s = (decltype( dllTexCoord1s ))QGL_GetProcAddress( "glTexCoord1s" );
	qglTexCoord1sv = dllTexCoord1sv = (decltype( dllTexCoord1sv ))QGL_GetProcAddress( "glTexCoord1sv" );
	qglTexCoord2d = dllTexCoord2d = (decltype( dllTexCoord2d ))QGL_GetProcAddress( "glTexCoord2d" );
	qglTexCoord2dv = dllTexCoord2dv = (decltype( dllTexCoord2dv ))QGL_GetProcAddress( "glTexCoord2dv" );
	qglTexCoord2f = dllTexCoord2f = (decltype( dllTexCoord2f ))QGL_GetProcAddress( "glTexCoord2f" );
	qglTexCoord2fv = dllTexCoord2fv = (decltype( dllTexCoord2fv ))QGL_GetProcAddress( "glTexCoord2fv" );
	qglTexCoord2i = dllTexCoord2i = (decltype( dllTexCoord2i ))QGL_GetProcAddress( "glTexCoord2i" );
	qglTexCoord2iv = dllTexCoord2iv = (decltype( dllTexCoord2iv ))QGL_GetProcAddress( "glTexCoord2iv" );
	qglTexCoord2s = dllTexCoord2s = (decltype( dllTexCoord2s ))QGL_GetProcAddress( "glTexCoord2s" );
	qglTexCoord2sv = dllTexCoord2sv = (decltype( dllTexCoord2sv ))QGL_GetProcAddress( "glTexCoord2sv" );
	qglTexCoord3d = dllTexCoord3d = (decltype( dllTexCoord3d ))QGL_GetProcAddress( "glTexCoord3d" );
	qglTexCoord3dv = dllTexCoord3dv = (decltype( dllTexCoord3dv ))QGL_GetProcAddress( "glTexCoord3dv" );
	qglTexCoord3f = dllTexCoord3f = (decltype( dllTexCoord3f ))QGL_GetProcAddress( "glTexCoord3f" );
	qglTexCoord3fv = dllTexCoord3fv = (decltype( dllTexCoord3fv ))QGL_GetProcAddress( "glTexCoord3fv" );
	qglTexCoord3i = dllTexCoord3i = (decltype( dllTexCoord3i ))QGL_GetProcAddress( "glTexCoord3i" );
	qglTexCoord3iv = dllTexCoord3iv = (decltype( dllTexCoord3iv ))QGL_GetProcAddress( "glTexCoord3iv" );
	qglTexCoord3s = dllTexCoord3s = (decltype( dllTexCoord3s ))QGL_GetProcAddress( "glTexCoord3s" );
	qglTexCoord3sv = dllTexCoord3sv = (decltype( dllTexCoord3sv ))QGL_GetProcAddress( "glTexCoord3sv" );
	qglTexCoord4d = dllTexCoord4d = (decltype( dllTexCoord4d ))QGL_GetProcAddress( "glTexCoord4d" );
	qglTexCoord4dv = dllTexCoord4dv = (decltype( dllTexCoord4dv ))QGL_GetProcAddress( "glTexCoord4dv" );
	qglTexCoord4f = dllTexCoord4f = (decltype( dllTexCoord4f ))QGL_GetProcAddress( "glTexCoord4f" );
	qglTexCoord4fv = dllTexCoord4fv = (decltype( dllTexCoord4fv ))QGL_GetProcAddress( "glTexCoord4fv" );
	qglTexCoord4i = dllTexCoord4i = (decltype( dllTexCoord4i ))QGL_GetProcAddress( "glTexCoord4i" );
	qglTexCoord4iv = dllTexCoord4iv = (decltype( dllTexCoord4iv ))QGL_GetProcAddress( "glTexCoord4iv" );
	qglTexCoord4s = dllTexCoord4s = (decltype( dllTexCoord4s ))QGL_GetProcAddress( "glTexCoord4s" );
	qglTexCoord4sv = dllTexCoord4sv = (decltype( dllTexCoord4sv ))QGL_GetProcAddress( "glTexCoord4sv" );
	qglTexCoordPointer = dllTexCoordPointer =
		(decltype( dllTexCoordPointer ))QGL_GetProcAddress( "glTexCoordPointer" );
	qglTexEnvf = dllTexEnvf = (decltype( dllTexEnvf ))QGL_GetProcAddress( "glTexEnvf" );
	qglTexEnvfv = dllTexEnvfv = (decltype( dllTexEnvfv ))QGL_GetProcAddress( "glTexEnvfv" );
	qglTexEnvi = dllTexEnvi = (decltype( dllTexEnvi ))QGL_GetProcAddress( "glTexEnvi" );
	qglTexEnviv = dllTexEnviv = (decltype( dllTexEnviv ))QGL_GetProcAddress( "glTexEnviv" );
	qglTexGend = dllTexGend = (decltype( dllTexGend ))QGL_GetProcAddress( "glTexGend" );
	qglTexGendv = dllTexGendv = (decltype( dllTexGendv ))QGL_GetProcAddress( "glTexGendv" );
	qglTexGenf = dllTexGenf = (decltype( dllTexGenf ))QGL_GetProcAddress( "glTexGenf" );
	qglTexGenfv = dllTexGenfv = (decltype( dllTexGenfv ))QGL_GetProcAddress( "glTexGenfv" );
	qglTexGeni = dllTexGeni = (decltype( dllTexGeni ))QGL_GetProcAddress( "glTexGeni" );
	qglTexGeniv = dllTexGeniv = (decltype( dllTexGeniv ))QGL_GetProcAddress( "glTexGeniv" );
	qglTexImage1D = dllTexImage1D = (decltype( dllTexImage1D ))QGL_GetProcAddress( "glTexImage1D" );
	qglTexImage2D = dllTexImage2D = (decltype( dllTexImage2D ))QGL_GetProcAddress( "glTexImage2D" );
	qglTexParameterf = dllTexParameterf = (decltype( dllTexParameterf ))QGL_GetProcAddress( "glTexParameterf" );
	qglTexParameterfv = dllTexParameterfv = (decltype( dllTexParameterfv ))QGL_GetProcAddress( "glTexParameterfv" );
	qglTexParameteri = dllTexParameteri = (decltype( dllTexParameteri ))QGL_GetProcAddress( "glTexParameteri" );
	qglTexParameteriv = dllTexParameteriv = (decltype( dllTexParameteriv ))QGL_GetProcAddress( "glTexParameteriv" );
	qglTexSubImage1D = dllTexSubImage1D = (decltype( dllTexSubImage1D ))QGL_GetProcAddress( "glTexSubImage1D" );
	qglTexSubImage2D = dllTexSubImage2D = (decltype( dllTexSubImage2D ))QGL_GetProcAddress( "glTexSubImage2D" );
	qglTranslated = dllTranslated = (decltype( dllTranslated ))QGL_GetProcAddress( "glTranslated" );
	qglTranslatef = dllTranslatef = (decltype( dllTranslatef ))QGL_GetProcAddress( "glTranslatef" );
	qglVertex2d = dllVertex2d = (decltype( dllVertex2d ))QGL_GetProcAddress( "glVertex2d" );
	qglVertex2dv = dllVertex2dv = (decltype( dllVertex2dv ))QGL_GetProcAddress( "glVertex2dv" );
	qglVertex2f = dllVertex2f = (decltype( dllVertex2f ))QGL_GetProcAddress( "glVertex2f" );
	qglVertex2fv = dllVertex2fv = (decltype( dllVertex2fv ))QGL_GetProcAddress( "glVertex2fv" );
	qglVertex2i = dllVertex2i = (decltype( dllVertex2i ))QGL_GetProcAddress( "glVertex2i" );
	qglVertex2iv = dllVertex2iv = (decltype( dllVertex2iv ))QGL_GetProcAddress( "glVertex2iv" );
	qglVertex2s = dllVertex2s = (decltype( dllVertex2s ))QGL_GetProcAddress( "glVertex2s" );
	qglVertex2sv = dllVertex2sv = (decltype( dllVertex2sv ))QGL_GetProcAddress( "glVertex2sv" );
	qglVertex3d = dllVertex3d = (decltype( dllVertex3d ))QGL_GetProcAddress( "glVertex3d" );
	qglVertex3dv = dllVertex3dv = (decltype( dllVertex3dv ))QGL_GetProcAddress( "glVertex3dv" );
	qglVertex3f = dllVertex3f = (decltype( dllVertex3f ))QGL_GetProcAddress( "glVertex3f" );
	qglVertex3fv = dllVertex3fv = (decltype( dllVertex3fv ))QGL_GetProcAddress( "glVertex3fv" );
	qglVertex3i = dllVertex3i = (decltype( dllVertex3i ))QGL_GetProcAddress( "glVertex3i" );
	qglVertex3iv = dllVertex3iv = (decltype( dllVertex3iv ))QGL_GetProcAddress( "glVertex3iv" );
	qglVertex3s = dllVertex3s = (decltype( dllVertex3s ))QGL_GetProcAddress( "glVertex3s" );
	qglVertex3sv = dllVertex3sv = (decltype( dllVertex3sv ))QGL_GetProcAddress( "glVertex3sv" );
	qglVertex4d = dllVertex4d = (decltype( dllVertex4d ))QGL_GetProcAddress( "glVertex4d" );
	qglVertex4dv = dllVertex4dv = (decltype( dllVertex4dv ))QGL_GetProcAddress( "glVertex4dv" );
	qglVertex4f = dllVertex4f = (decltype( dllVertex4f ))QGL_GetProcAddress( "glVertex4f" );
	qglVertex4fv = dllVertex4fv = (decltype( dllVertex4fv ))QGL_GetProcAddress( "glVertex4fv" );
	qglVertex4i = dllVertex4i = (decltype( dllVertex4i ))QGL_GetProcAddress( "glVertex4i" );
	qglVertex4iv = dllVertex4iv = (decltype( dllVertex4iv ))QGL_GetProcAddress( "glVertex4iv" );
	qglVertex4s = dllVertex4s = (decltype( dllVertex4s ))QGL_GetProcAddress( "glVertex4s" );
	qglVertex4sv = dllVertex4sv = (decltype( dllVertex4sv ))QGL_GetProcAddress( "glVertex4sv" );
	qglVertexPointer = dllVertexPointer = (decltype( dllVertexPointer ))QGL_GetProcAddress( "glVertexPointer" );
	qglViewport = dllViewport = (decltype( dllViewport ))QGL_GetProcAddress( "glViewport" );

	qglCreateShader = dllCreateShader = (decltype( dllCreateShader ))QGL_GetProcAddress( "glCreateShader" );
	qglDeleteShader = dllDeleteShader = (decltype( dllDeleteShader ))QGL_GetProcAddress( "glDeleteShader" );
	qglShaderSource = dllShaderSource = (decltype( dllShaderSource ))QGL_GetProcAddress( "glShaderSource" );
	qglGetProgramiv = dllGetProgramiv = (decltype( dllGetProgramiv ))QGL_GetProcAddress( "glGetProgramiv" );
	qglGetProgramInfoLog = dllGetProgramInfoLog =
		(decltype( dllGetProgramInfoLog ))QGL_GetProcAddress( "glGetProgramInfoLog" );
	qglAttachShader = dllAttachShader = (decltype( dllAttachShader ))QGL_GetProcAddress( "glAttachShader" );
	qglLinkProgram = dllLinkProgram = (decltype( dllLinkProgram ))QGL_GetProcAddress( "glLinkProgram" );
	qglCreateProgram = dllCreateProgram = (decltype( dllCreateProgram ))QGL_GetProcAddress( "glCreateProgram" );
	qglDeleteProgram = dllDeleteProgram = (decltype( dllDeleteProgram ))QGL_GetProcAddress( "glDeleteProgram" );
	qglUseProgram = dllUseProgram = (decltype( dllUseProgram ))QGL_GetProcAddress( "glUseProgram" );
	qglCompileShader = dllCompileShader = (decltype( dllCompileShader ))QGL_GetProcAddress( "glCompileShader" );
	qglGetShaderiv = dllGetShaderiv = (decltype( dllGetShaderiv ))QGL_GetProcAddress( "glGetShaderiv" );
	qglGetShaderInfoLog = dllGetShaderInfoLog =
		(decltype( dllGetShaderInfoLog ))QGL_GetProcAddress( "glGetShaderInfoLog" );
	qglGetUniformLocation = dllGetUniformLocation =
		(decltype( dllGetUniformLocation ))QGL_GetProcAddress( "glGetUniformLocation" );
	qglUniform1ui = dllUniform1ui = (decltype( dllUniform1ui ))QGL_GetProcAddress( "glUniform1ui" );

	qglIsRenderbufferEXT = (decltype( qglIsRenderbufferEXT ))QGL_GetProcAddress( "glIsRenderbufferEXT" );
	qglBindRenderbufferEXT = (decltype( qglBindRenderbufferEXT ))QGL_GetProcAddress( "glBindRenderbufferEXT" );
	qglDeleteRenderbuffersEXT =
		(decltype( qglDeleteRenderbuffersEXT ))QGL_GetProcAddress( "glDeleteRenderbuffersEXT" );
	qglGenRenderbuffersEXT = (decltype( qglGenRenderbuffersEXT ))QGL_GetProcAddress( "glGenRenderbuffersEXT" );
	qglRenderbufferStorageEXT =
		(decltype( qglRenderbufferStorageEXT ))QGL_GetProcAddress( "glRenderbufferStorageEXT" );
	qglGetRenderbufferParameterivEXT =
		(decltype( qglGetRenderbufferParameterivEXT ))QGL_GetProcAddress( "glGetRenderbufferParameterivEXT" );
	qglBindFramebufferEXT = (decltype( qglBindFramebufferEXT ))QGL_GetProcAddress( "glBindFramebufferEXT" );
	qglDeleteFramebuffersEXT = (decltype( qglDeleteFramebuffersEXT ))QGL_GetProcAddress( "glDeleteFramebuffersEXT" );
	qglGenFramebuffersEXT = (decltype( qglGenFramebuffersEXT ))QGL_GetProcAddress( "glGenFramebuffersEXT" );
	qglCheckFramebufferStatusEXT =
		(decltype( qglCheckFramebufferStatusEXT ))QGL_GetProcAddress( "glCheckFramebufferStatusEXT" );
	qglFramebufferTexture2DEXT =
		(decltype( qglFramebufferTexture2DEXT ))QGL_GetProcAddress( "glFramebufferTexture2DEXT" );
	qglFramebufferRenderbufferEXT =
		(decltype( qglFramebufferRenderbufferEXT ))QGL_GetProcAddress( "glFramebufferRenderbufferEXT" );
	qglBlitFramebufferEXT = (decltype( qglBlitFramebufferEXT ))QGL_GetProcAddress( "glBlitFramebufferEXT" );
	qglRenderbufferStorageMultisampleEXT =
		(decltype( qglRenderbufferStorageMultisampleEXT ))QGL_GetProcAddress( "glRenderbufferStorageMultisampleEXT" );

	qglPointParameterfEXT = NULL;
	qglPointParameterfvEXT = NULL;
	qglColorTableEXT = NULL;
	qglSelectTextureSGIS = NULL;
	qglMTexCoord2fSGIS = NULL;
	qwglSwapBuffers = NULL;

	return glw_state.hinstOpenGL;
}

extern "C" void GLimp_EnableLogging()
{
	if ( !glw_state.log_fp )
	{
		time_t aclock;
		struct tm* newtime;
		char buffer[1024];

		time( &aclock );
		newtime = localtime( &aclock );
#ifdef _MSC_VER
#pragma warning( suppress: 6031 )
#endif
		asctime( newtime );

		snprintf( buffer, sizeof( buffer ), "\\gl.log" );
		glw_state.log_fp = FS_Open( buffer, "wt" );
		FS_FPrintf( glw_state.log_fp, "%s\n", asctime( newtime ) );

		qglAccum = logAccum;
		qglAlphaFunc = logAlphaFunc;
		qglAreTexturesResident = logAreTexturesResident;
		qglArrayElement = logArrayElement;
		qglBegin = logBegin;
		qglBindTexture = logBindTexture;
		qglBitmap = logBitmap;
		qglBlendFunc = logBlendFunc;
		qglCallList = logCallList;
		qglCallLists = logCallLists;
		qglClear = logClear;
		qglClearAccum = logClearAccum;
		qglClearColor = logClearColor;
		qglClearDepth = logClearDepth;
		qglClearIndex = logClearIndex;
		qglClearStencil = logClearStencil;
		qglClipPlane = logClipPlane;
		qglColor3b = logColor3b;
		qglColor3bv = logColor3bv;
		qglColor3d = logColor3d;
		qglColor3dv = logColor3dv;
		qglColor3f = logColor3f;
		qglColor3fv = logColor3fv;
		qglColor3i = logColor3i;
		qglColor3iv = logColor3iv;
		qglColor3s = logColor3s;
		qglColor3sv = logColor3sv;
		qglColor3ub = logColor3ub;
		qglColor3ubv = logColor3ubv;
		qglColor3ui = logColor3ui;
		qglColor3uiv = logColor3uiv;
		qglColor3us = logColor3us;
		qglColor3usv = logColor3usv;
		qglColor4b = logColor4b;
		qglColor4bv = logColor4bv;
		qglColor4d = logColor4d;
		qglColor4dv = logColor4dv;
		qglColor4f = logColor4f;
		qglColor4fv = logColor4fv;
		qglColor4i = logColor4i;
		qglColor4iv = logColor4iv;
		qglColor4s = logColor4s;
		qglColor4sv = logColor4sv;
		qglColor4ub = logColor4ub;
		qglColor4ubv = logColor4ubv;
		qglColor4ui = logColor4ui;
		qglColor4uiv = logColor4uiv;
		qglColor4us = logColor4us;
		qglColor4usv = logColor4usv;
		qglColorMask = logColorMask;
		qglColorMaterial = logColorMaterial;
		qglColorPointer = logColorPointer;
		qglCopyPixels = logCopyPixels;
		qglCopyTexImage1D = logCopyTexImage1D;
		qglCopyTexImage2D = logCopyTexImage2D;
		qglCopyTexSubImage1D = logCopyTexSubImage1D;
		qglCopyTexSubImage2D = logCopyTexSubImage2D;
		qglCullFace = logCullFace;
		qglDeleteLists = logDeleteLists;
		qglDeleteTextures = logDeleteTextures;
		qglDepthFunc = logDepthFunc;
		qglDepthMask = logDepthMask;
		qglDepthRange = logDepthRange;
		qglDisable = logDisable;
		qglDisableClientState = logDisableClientState;
		qglDrawArrays = logDrawArrays;
		qglDrawBuffer = logDrawBuffer;
		qglDrawElements = logDrawElements;
		qglDrawPixels = logDrawPixels;
		qglEdgeFlag = logEdgeFlag;
		qglEdgeFlagPointer = logEdgeFlagPointer;
		qglEdgeFlagv = logEdgeFlagv;
		qglEnable = logEnable;
		qglEnableClientState = logEnableClientState;
		qglEnd = logEnd;
		qglEndList = logEndList;
		qglEvalCoord1d = logEvalCoord1d;
		qglEvalCoord1dv = logEvalCoord1dv;
		qglEvalCoord1f = logEvalCoord1f;
		qglEvalCoord1fv = logEvalCoord1fv;
		qglEvalCoord2d = logEvalCoord2d;
		qglEvalCoord2dv = logEvalCoord2dv;
		qglEvalCoord2f = logEvalCoord2f;
		qglEvalCoord2fv = logEvalCoord2fv;
		qglEvalMesh1 = logEvalMesh1;
		qglEvalMesh2 = logEvalMesh2;
		qglEvalPoint1 = logEvalPoint1;
		qglEvalPoint2 = logEvalPoint2;
		qglFeedbackBuffer = logFeedbackBuffer;
		qglFinish = logFinish;
		qglFlush = logFlush;
		qglFogf = logFogf;
		qglFogfv = logFogfv;
		qglFogi = logFogi;
		qglFogiv = logFogiv;
		qglFrontFace = logFrontFace;
		qglFrustum = logFrustum;
		qglGenLists = logGenLists;
		qglGenTextures = logGenTextures;
		qglGetBooleanv = logGetBooleanv;
		qglGetClipPlane = logGetClipPlane;
		qglGetDoublev = logGetDoublev;
		qglGetError = logGetError;
		qglGetFloatv = logGetFloatv;
		qglGetIntegerv = logGetIntegerv;
		qglGetLightfv = logGetLightfv;
		qglGetLightiv = logGetLightiv;
		qglGetMapdv = logGetMapdv;
		qglGetMapfv = logGetMapfv;
		qglGetMapiv = logGetMapiv;
		qglGetMaterialfv = logGetMaterialfv;
		qglGetMaterialiv = logGetMaterialiv;
		qglGetPixelMapfv = logGetPixelMapfv;
		qglGetPixelMapuiv = logGetPixelMapuiv;
		qglGetPixelMapusv = logGetPixelMapusv;
		qglGetPointerv = logGetPointerv;
		qglGetPolygonStipple = logGetPolygonStipple;
		qglGetString = logGetString;
		qglGetTexEnvfv = logGetTexEnvfv;
		qglGetTexEnviv = logGetTexEnviv;
		qglGetTexGendv = logGetTexGendv;
		qglGetTexGenfv = logGetTexGenfv;
		qglGetTexGeniv = logGetTexGeniv;
		qglGetTexImage = logGetTexImage;
		qglGetTexLevelParameterfv = logGetTexLevelParameterfv;
		qglGetTexLevelParameteriv = logGetTexLevelParameteriv;
		qglGetTexParameterfv = logGetTexParameterfv;
		qglGetTexParameteriv = logGetTexParameteriv;
		qglHint = logHint;
		qglIndexMask = logIndexMask;
		qglIndexPointer = logIndexPointer;
		qglIndexd = logIndexd;
		qglIndexdv = logIndexdv;
		qglIndexf = logIndexf;
		qglIndexfv = logIndexfv;
		qglIndexi = logIndexi;
		qglIndexiv = logIndexiv;
		qglIndexs = logIndexs;
		qglIndexsv = logIndexsv;
		qglIndexub = logIndexub;
		qglIndexubv = logIndexubv;
		qglInitNames = logInitNames;
		qglInterleavedArrays = logInterleavedArrays;
		qglIsEnabled = logIsEnabled;
		qglIsList = logIsList;
		qglIsTexture = logIsTexture;
		qglLightModelf = logLightModelf;
		qglLightModelfv = logLightModelfv;
		qglLightModeli = logLightModeli;
		qglLightModeliv = logLightModeliv;
		qglLightf = logLightf;
		qglLightfv = logLightfv;
		qglLighti = logLighti;
		qglLightiv = logLightiv;
		qglLineStipple = logLineStipple;
		qglLineWidth = logLineWidth;
		qglListBase = logListBase;
		qglLoadIdentity = logLoadIdentity;
		qglLoadMatrixd = logLoadMatrixd;
		qglLoadMatrixf = logLoadMatrixf;
		qglLoadName = logLoadName;
		qglLogicOp = logLogicOp;
		qglMap1d = logMap1d;
		qglMap1f = logMap1f;
		qglMap2d = logMap2d;
		qglMap2f = logMap2f;
		qglMapGrid1d = logMapGrid1d;
		qglMapGrid1f = logMapGrid1f;
		qglMapGrid2d = logMapGrid2d;
		qglMapGrid2f = logMapGrid2f;
		qglMaterialf = logMaterialf;
		qglMaterialfv = logMaterialfv;
		qglMateriali = logMateriali;
		qglMaterialiv = logMaterialiv;
		qglMatrixMode = logMatrixMode;
		qglMultMatrixd = logMultMatrixd;
		qglMultMatrixf = logMultMatrixf;
		qglNewList = logNewList;
		qglNormal3b = logNormal3b;
		qglNormal3bv = logNormal3bv;
		qglNormal3d = logNormal3d;
		qglNormal3dv = logNormal3dv;
		qglNormal3f = logNormal3f;
		qglNormal3fv = logNormal3fv;
		qglNormal3i = logNormal3i;
		qglNormal3iv = logNormal3iv;
		qglNormal3s = logNormal3s;
		qglNormal3sv = logNormal3sv;
		qglNormalPointer = logNormalPointer;
		qglOrtho = logOrtho;
		qglPassThrough = logPassThrough;
		qglPixelMapfv = logPixelMapfv;
		qglPixelMapuiv = logPixelMapuiv;
		qglPixelMapusv = logPixelMapusv;
		qglPixelStoref = logPixelStoref;
		qglPixelStorei = logPixelStorei;
		qglPixelTransferf = logPixelTransferf;
		qglPixelTransferi = logPixelTransferi;
		qglPixelZoom = logPixelZoom;
		qglPointSize = logPointSize;
		qglPolygonMode = logPolygonMode;
		qglPolygonOffset = logPolygonOffset;
		qglPolygonStipple = logPolygonStipple;
		qglPopAttrib = logPopAttrib;
		qglPopClientAttrib = logPopClientAttrib;
		qglPopMatrix = logPopMatrix;
		qglPopName = logPopName;
		qglPrioritizeTextures = logPrioritizeTextures;
		qglPushAttrib = logPushAttrib;
		qglPushClientAttrib = logPushClientAttrib;
		qglPushMatrix = logPushMatrix;
		qglPushName = logPushName;
		qglRasterPos2d = logRasterPos2d;
		qglRasterPos2dv = logRasterPos2dv;
		qglRasterPos2f = logRasterPos2f;
		qglRasterPos2fv = logRasterPos2fv;
		qglRasterPos2i = logRasterPos2i;
		qglRasterPos2iv = logRasterPos2iv;
		qglRasterPos2s = logRasterPos2s;
		qglRasterPos2sv = logRasterPos2sv;
		qglRasterPos3d = logRasterPos3d;
		qglRasterPos3dv = logRasterPos3dv;
		qglRasterPos3f = logRasterPos3f;
		qglRasterPos3fv = logRasterPos3fv;
		qglRasterPos3i = logRasterPos3i;
		qglRasterPos3iv = logRasterPos3iv;
		qglRasterPos3s = logRasterPos3s;
		qglRasterPos3sv = logRasterPos3sv;
		qglRasterPos4d = logRasterPos4d;
		qglRasterPos4dv = logRasterPos4dv;
		qglRasterPos4f = logRasterPos4f;
		qglRasterPos4fv = logRasterPos4fv;
		qglRasterPos4i = logRasterPos4i;
		qglRasterPos4iv = logRasterPos4iv;
		qglRasterPos4s = logRasterPos4s;
		qglRasterPos4sv = logRasterPos4sv;
		qglReadBuffer = logReadBuffer;
		qglReadPixels = logReadPixels;
		qglRectd = logRectd;
		qglRectdv = logRectdv;
		qglRectf = logRectf;
		qglRectfv = logRectfv;
		qglRecti = logRecti;
		qglRectiv = logRectiv;
		qglRects = logRects;
		qglRectsv = logRectsv;
		qglRenderMode = logRenderMode;
		qglRotated = logRotated;
		qglRotatef = logRotatef;
		qglScaled = logScaled;
		qglScalef = logScalef;
		qglScissor = logScissor;
		qglSelectBuffer = logSelectBuffer;
		qglShadeModel = logShadeModel;
		qglStencilFunc = logStencilFunc;
		qglStencilMask = logStencilMask;
		qglStencilOp = logStencilOp;
		qglTexCoord1d = logTexCoord1d;
		qglTexCoord1dv = logTexCoord1dv;
		qglTexCoord1f = logTexCoord1f;
		qglTexCoord1fv = logTexCoord1fv;
		qglTexCoord1i = logTexCoord1i;
		qglTexCoord1iv = logTexCoord1iv;
		qglTexCoord1s = logTexCoord1s;
		qglTexCoord1sv = logTexCoord1sv;
		qglTexCoord2d = logTexCoord2d;
		qglTexCoord2dv = logTexCoord2dv;
		qglTexCoord2f = logTexCoord2f;
		qglTexCoord2fv = logTexCoord2fv;
		qglTexCoord2i = logTexCoord2i;
		qglTexCoord2iv = logTexCoord2iv;
		qglTexCoord2s = logTexCoord2s;
		qglTexCoord2sv = logTexCoord2sv;
		qglTexCoord3d = logTexCoord3d;
		qglTexCoord3dv = logTexCoord3dv;
		qglTexCoord3f = logTexCoord3f;
		qglTexCoord3fv = logTexCoord3fv;
		qglTexCoord3i = logTexCoord3i;
		qglTexCoord3iv = logTexCoord3iv;
		qglTexCoord3s = logTexCoord3s;
		qglTexCoord3sv = logTexCoord3sv;
		qglTexCoord4d = logTexCoord4d;
		qglTexCoord4dv = logTexCoord4dv;
		qglTexCoord4f = logTexCoord4f;
		qglTexCoord4fv = logTexCoord4fv;
		qglTexCoord4i = logTexCoord4i;
		qglTexCoord4iv = logTexCoord4iv;
		qglTexCoord4s = logTexCoord4s;
		qglTexCoord4sv = logTexCoord4sv;
		qglTexCoordPointer = logTexCoordPointer;
		qglTexEnvf = logTexEnvf;
		qglTexEnvfv = logTexEnvfv;
		qglTexEnvi = logTexEnvi;
		qglTexEnviv = logTexEnviv;
		qglTexGend = logTexGend;
		qglTexGendv = logTexGendv;
		qglTexGenf = logTexGenf;
		qglTexGenfv = logTexGenfv;
		qglTexGeni = logTexGeni;
		qglTexGeniv = logTexGeniv;
		qglTexImage1D = logTexImage1D;
		qglTexImage2D = logTexImage2D;
		qglTexParameterf = logTexParameterf;
		qglTexParameterfv = logTexParameterfv;
		qglTexParameteri = logTexParameteri;
		qglTexParameteriv = logTexParameteriv;
		qglTexSubImage1D = logTexSubImage1D;
		qglTexSubImage2D = logTexSubImage2D;
		qglTranslated = logTranslated;
		qglTranslatef = logTranslatef;
		qglVertex2d = logVertex2d;
		qglVertex2dv = logVertex2dv;
		qglVertex2f = logVertex2f;
		qglVertex2fv = logVertex2fv;
		qglVertex2i = logVertex2i;
		qglVertex2iv = logVertex2iv;
		qglVertex2s = logVertex2s;
		qglVertex2sv = logVertex2sv;
		qglVertex3d = logVertex3d;
		qglVertex3dv = logVertex3dv;
		qglVertex3f = logVertex3f;
		qglVertex3fv = logVertex3fv;
		qglVertex3i = logVertex3i;
		qglVertex3iv = logVertex3iv;
		qglVertex3s = logVertex3s;
		qglVertex3sv = logVertex3sv;
		qglVertex4d = logVertex4d;
		qglVertex4dv = logVertex4dv;
		qglVertex4f = logVertex4f;
		qglVertex4fv = logVertex4fv;
		qglVertex4i = logVertex4i;
		qglVertex4iv = logVertex4iv;
		qglVertex4s = logVertex4s;
		qglVertex4sv = logVertex4sv;
		qglVertexPointer = logVertexPointer;
		qglViewport = logViewport;
	}
	else
	{
		FS_Close( glw_state.log_fp );
		glw_state.log_fp = NULL;

		qglAccum = dllAccum;
		qglAlphaFunc = dllAlphaFunc;
		qglAreTexturesResident = dllAreTexturesResident;
		qglArrayElement = dllArrayElement;
		qglBegin = dllBegin;
		qglBindTexture = dllBindTexture;
		qglBitmap = dllBitmap;
		qglBlendFunc = dllBlendFunc;
		qglCallList = dllCallList;
		qglCallLists = dllCallLists;
		qglClear = dllClear;
		qglClearAccum = dllClearAccum;
		qglClearColor = dllClearColor;
		qglClearDepth = dllClearDepth;
		qglClearIndex = dllClearIndex;
		qglClearStencil = dllClearStencil;
		qglClipPlane = dllClipPlane;
		qglColor3b = dllColor3b;
		qglColor3bv = dllColor3bv;
		qglColor3d = dllColor3d;
		qglColor3dv = dllColor3dv;
		qglColor3f = dllColor3f;
		qglColor3fv = dllColor3fv;
		qglColor3i = dllColor3i;
		qglColor3iv = dllColor3iv;
		qglColor3s = dllColor3s;
		qglColor3sv = dllColor3sv;
		qglColor3ub = dllColor3ub;
		qglColor3ubv = dllColor3ubv;
		qglColor3ui = dllColor3ui;
		qglColor3uiv = dllColor3uiv;
		qglColor3us = dllColor3us;
		qglColor3usv = dllColor3usv;
		qglColor4b = dllColor4b;
		qglColor4bv = dllColor4bv;
		qglColor4d = dllColor4d;
		qglColor4dv = dllColor4dv;
		qglColor4f = dllColor4f;
		qglColor4fv = dllColor4fv;
		qglColor4i = dllColor4i;
		qglColor4iv = dllColor4iv;
		qglColor4s = dllColor4s;
		qglColor4sv = dllColor4sv;
		qglColor4ub = dllColor4ub;
		qglColor4ubv = dllColor4ubv;
		qglColor4ui = dllColor4ui;
		qglColor4uiv = dllColor4uiv;
		qglColor4us = dllColor4us;
		qglColor4usv = dllColor4usv;
		qglColorMask = dllColorMask;
		qglColorMaterial = dllColorMaterial;
		qglColorPointer = dllColorPointer;
		qglCopyPixels = dllCopyPixels;
		qglCopyTexImage1D = dllCopyTexImage1D;
		qglCopyTexImage2D = dllCopyTexImage2D;
		qglCopyTexSubImage1D = dllCopyTexSubImage1D;
		qglCopyTexSubImage2D = dllCopyTexSubImage2D;
		qglCullFace = dllCullFace;
		qglDeleteLists = dllDeleteLists;
		qglDeleteTextures = dllDeleteTextures;
		qglDepthFunc = dllDepthFunc;
		qglDepthMask = dllDepthMask;
		qglDepthRange = dllDepthRange;
		qglDisable = dllDisable;
		qglDisableClientState = dllDisableClientState;
		qglDrawArrays = dllDrawArrays;
		qglDrawBuffer = dllDrawBuffer;
		qglDrawElements = dllDrawElements;
		qglDrawPixels = dllDrawPixels;
		qglEdgeFlag = dllEdgeFlag;
		qglEdgeFlagPointer = dllEdgeFlagPointer;
		qglEdgeFlagv = dllEdgeFlagv;
		qglEnable = dllEnable;
		qglEnableClientState = dllEnableClientState;
		qglEnd = dllEnd;
		qglEndList = dllEndList;
		qglEvalCoord1d = dllEvalCoord1d;
		qglEvalCoord1dv = dllEvalCoord1dv;
		qglEvalCoord1f = dllEvalCoord1f;
		qglEvalCoord1fv = dllEvalCoord1fv;
		qglEvalCoord2d = dllEvalCoord2d;
		qglEvalCoord2dv = dllEvalCoord2dv;
		qglEvalCoord2f = dllEvalCoord2f;
		qglEvalCoord2fv = dllEvalCoord2fv;
		qglEvalMesh1 = dllEvalMesh1;
		qglEvalMesh2 = dllEvalMesh2;
		qglEvalPoint1 = dllEvalPoint1;
		qglEvalPoint2 = dllEvalPoint2;
		qglFeedbackBuffer = dllFeedbackBuffer;
		qglFinish = dllFinish;
		qglFlush = dllFlush;
		qglFogf = dllFogf;
		qglFogfv = dllFogfv;
		qglFogi = dllFogi;
		qglFogiv = dllFogiv;
		qglFrontFace = dllFrontFace;
		qglFrustum = dllFrustum;
		qglGenLists = dllGenLists;
		qglGenTextures = dllGenTextures;
		qglGetBooleanv = dllGetBooleanv;
		qglGetClipPlane = dllGetClipPlane;
		qglGetDoublev = dllGetDoublev;
		qglGetError = dllGetError;
		qglGetFloatv = dllGetFloatv;
		qglGetIntegerv = dllGetIntegerv;
		qglGetLightfv = dllGetLightfv;
		qglGetLightiv = dllGetLightiv;
		qglGetMapdv = dllGetMapdv;
		qglGetMapfv = dllGetMapfv;
		qglGetMapiv = dllGetMapiv;
		qglGetMaterialfv = dllGetMaterialfv;
		qglGetMaterialiv = dllGetMaterialiv;
		qglGetPixelMapfv = dllGetPixelMapfv;
		qglGetPixelMapuiv = dllGetPixelMapuiv;
		qglGetPixelMapusv = dllGetPixelMapusv;
		qglGetPointerv = dllGetPointerv;
		qglGetPolygonStipple = dllGetPolygonStipple;
		qglGetString = dllGetString;
		qglGetTexEnvfv = dllGetTexEnvfv;
		qglGetTexEnviv = dllGetTexEnviv;
		qglGetTexGendv = dllGetTexGendv;
		qglGetTexGenfv = dllGetTexGenfv;
		qglGetTexGeniv = dllGetTexGeniv;
		qglGetTexImage = dllGetTexImage;
		qglGetTexLevelParameterfv = dllGetTexLevelParameterfv;
		qglGetTexLevelParameteriv = dllGetTexLevelParameteriv;
		qglGetTexParameterfv = dllGetTexParameterfv;
		qglGetTexParameteriv = dllGetTexParameteriv;
		qglHint = dllHint;
		qglIndexMask = dllIndexMask;
		qglIndexPointer = dllIndexPointer;
		qglIndexd = dllIndexd;
		qglIndexdv = dllIndexdv;
		qglIndexf = dllIndexf;
		qglIndexfv = dllIndexfv;
		qglIndexi = dllIndexi;
		qglIndexiv = dllIndexiv;
		qglIndexs = dllIndexs;
		qglIndexsv = dllIndexsv;
		qglIndexub = dllIndexub;
		qglIndexubv = dllIndexubv;
		qglInitNames = dllInitNames;
		qglInterleavedArrays = dllInterleavedArrays;
		qglIsEnabled = dllIsEnabled;
		qglIsList = dllIsList;
		qglIsTexture = dllIsTexture;
		qglLightModelf = dllLightModelf;
		qglLightModelfv = dllLightModelfv;
		qglLightModeli = dllLightModeli;
		qglLightModeliv = dllLightModeliv;
		qglLightf = dllLightf;
		qglLightfv = dllLightfv;
		qglLighti = dllLighti;
		qglLightiv = dllLightiv;
		qglLineStipple = dllLineStipple;
		qglLineWidth = dllLineWidth;
		qglListBase = dllListBase;
		qglLoadIdentity = dllLoadIdentity;
		qglLoadMatrixd = dllLoadMatrixd;
		qglLoadMatrixf = dllLoadMatrixf;
		qglLoadName = dllLoadName;
		qglLogicOp = dllLogicOp;
		qglMap1d = dllMap1d;
		qglMap1f = dllMap1f;
		qglMap2d = dllMap2d;
		qglMap2f = dllMap2f;
		qglMapGrid1d = dllMapGrid1d;
		qglMapGrid1f = dllMapGrid1f;
		qglMapGrid2d = dllMapGrid2d;
		qglMapGrid2f = dllMapGrid2f;
		qglMaterialf = dllMaterialf;
		qglMaterialfv = dllMaterialfv;
		qglMateriali = dllMateriali;
		qglMaterialiv = dllMaterialiv;
		qglMatrixMode = dllMatrixMode;
		qglMultMatrixd = dllMultMatrixd;
		qglMultMatrixf = dllMultMatrixf;
		qglNewList = dllNewList;
		qglNormal3b = dllNormal3b;
		qglNormal3bv = dllNormal3bv;
		qglNormal3d = dllNormal3d;
		qglNormal3dv = dllNormal3dv;
		qglNormal3f = dllNormal3f;
		qglNormal3fv = dllNormal3fv;
		qglNormal3i = dllNormal3i;
		qglNormal3iv = dllNormal3iv;
		qglNormal3s = dllNormal3s;
		qglNormal3sv = dllNormal3sv;
		qglNormalPointer = dllNormalPointer;
		qglOrtho = dllOrtho;
		qglPassThrough = dllPassThrough;
		qglPixelMapfv = dllPixelMapfv;
		qglPixelMapuiv = dllPixelMapuiv;
		qglPixelMapusv = dllPixelMapusv;
		qglPixelStoref = dllPixelStoref;
		qglPixelStorei = dllPixelStorei;
		qglPixelTransferf = dllPixelTransferf;
		qglPixelTransferi = dllPixelTransferi;
		qglPixelZoom = dllPixelZoom;
		qglPointSize = dllPointSize;
		qglPolygonMode = dllPolygonMode;
		qglPolygonOffset = dllPolygonOffset;
		qglPolygonStipple = dllPolygonStipple;
		qglPopAttrib = dllPopAttrib;
		qglPopClientAttrib = dllPopClientAttrib;
		qglPopMatrix = dllPopMatrix;
		qglPopName = dllPopName;
		qglPrioritizeTextures = dllPrioritizeTextures;
		qglPushAttrib = dllPushAttrib;
		qglPushClientAttrib = dllPushClientAttrib;
		qglPushMatrix = dllPushMatrix;
		qglPushName = dllPushName;
		qglRasterPos2d = dllRasterPos2d;
		qglRasterPos2dv = dllRasterPos2dv;
		qglRasterPos2f = dllRasterPos2f;
		qglRasterPos2fv = dllRasterPos2fv;
		qglRasterPos2i = dllRasterPos2i;
		qglRasterPos2iv = dllRasterPos2iv;
		qglRasterPos2s = dllRasterPos2s;
		qglRasterPos2sv = dllRasterPos2sv;
		qglRasterPos3d = dllRasterPos3d;
		qglRasterPos3dv = dllRasterPos3dv;
		qglRasterPos3f = dllRasterPos3f;
		qglRasterPos3fv = dllRasterPos3fv;
		qglRasterPos3i = dllRasterPos3i;
		qglRasterPos3iv = dllRasterPos3iv;
		qglRasterPos3s = dllRasterPos3s;
		qglRasterPos3sv = dllRasterPos3sv;
		qglRasterPos4d = dllRasterPos4d;
		qglRasterPos4dv = dllRasterPos4dv;
		qglRasterPos4f = dllRasterPos4f;
		qglRasterPos4fv = dllRasterPos4fv;
		qglRasterPos4i = dllRasterPos4i;
		qglRasterPos4iv = dllRasterPos4iv;
		qglRasterPos4s = dllRasterPos4s;
		qglRasterPos4sv = dllRasterPos4sv;
		qglReadBuffer = dllReadBuffer;
		qglReadPixels = dllReadPixels;
		qglRectd = dllRectd;
		qglRectdv = dllRectdv;
		qglRectf = dllRectf;
		qglRectfv = dllRectfv;
		qglRecti = dllRecti;
		qglRectiv = dllRectiv;
		qglRects = dllRects;
		qglRectsv = dllRectsv;
		qglRenderMode = dllRenderMode;
		qglRotated = dllRotated;
		qglRotatef = dllRotatef;
		qglScaled = dllScaled;
		qglScalef = dllScalef;
		qglScissor = dllScissor;
		qglSelectBuffer = dllSelectBuffer;
		qglShadeModel = dllShadeModel;
		qglStencilFunc = dllStencilFunc;
		qglStencilMask = dllStencilMask;
		qglStencilOp = dllStencilOp;
		qglTexCoord1d = dllTexCoord1d;
		qglTexCoord1dv = dllTexCoord1dv;
		qglTexCoord1f = dllTexCoord1f;
		qglTexCoord1fv = dllTexCoord1fv;
		qglTexCoord1i = dllTexCoord1i;
		qglTexCoord1iv = dllTexCoord1iv;
		qglTexCoord1s = dllTexCoord1s;
		qglTexCoord1sv = dllTexCoord1sv;
		qglTexCoord2d = dllTexCoord2d;
		qglTexCoord2dv = dllTexCoord2dv;
		qglTexCoord2f = dllTexCoord2f;
		qglTexCoord2fv = dllTexCoord2fv;
		qglTexCoord2i = dllTexCoord2i;
		qglTexCoord2iv = dllTexCoord2iv;
		qglTexCoord2s = dllTexCoord2s;
		qglTexCoord2sv = dllTexCoord2sv;
		qglTexCoord3d = dllTexCoord3d;
		qglTexCoord3dv = dllTexCoord3dv;
		qglTexCoord3f = dllTexCoord3f;
		qglTexCoord3fv = dllTexCoord3fv;
		qglTexCoord3i = dllTexCoord3i;
		qglTexCoord3iv = dllTexCoord3iv;
		qglTexCoord3s = dllTexCoord3s;
		qglTexCoord3sv = dllTexCoord3sv;
		qglTexCoord4d = dllTexCoord4d;
		qglTexCoord4dv = dllTexCoord4dv;
		qglTexCoord4f = dllTexCoord4f;
		qglTexCoord4fv = dllTexCoord4fv;
		qglTexCoord4i = dllTexCoord4i;
		qglTexCoord4iv = dllTexCoord4iv;
		qglTexCoord4s = dllTexCoord4s;
		qglTexCoord4sv = dllTexCoord4sv;
		qglTexCoordPointer = dllTexCoordPointer;
		qglTexEnvf = dllTexEnvf;
		qglTexEnvfv = dllTexEnvfv;
		qglTexEnvi = dllTexEnvi;
		qglTexEnviv = dllTexEnviv;
		qglTexGend = dllTexGend;
		qglTexGendv = dllTexGendv;
		qglTexGenf = dllTexGenf;
		qglTexGenfv = dllTexGenfv;
		qglTexGeni = dllTexGeni;
		qglTexGeniv = dllTexGeniv;
		qglTexImage1D = dllTexImage1D;
		qglTexImage2D = dllTexImage2D;
		qglTexParameterf = dllTexParameterf;
		qglTexParameterfv = dllTexParameterfv;
		qglTexParameteri = dllTexParameteri;
		qglTexParameteriv = dllTexParameteriv;
		qglTexSubImage1D = dllTexSubImage1D;
		qglTexSubImage2D = dllTexSubImage2D;
		qglTranslated = dllTranslated;
		qglTranslatef = dllTranslatef;
		qglVertex2d = dllVertex2d;
		qglVertex2dv = dllVertex2dv;
		qglVertex2f = dllVertex2f;
		qglVertex2fv = dllVertex2fv;
		qglVertex2i = dllVertex2i;
		qglVertex2iv = dllVertex2iv;
		qglVertex2s = dllVertex2s;
		qglVertex2sv = dllVertex2sv;
		qglVertex3d = dllVertex3d;
		qglVertex3dv = dllVertex3dv;
		qglVertex3f = dllVertex3f;
		qglVertex3fv = dllVertex3fv;
		qglVertex3i = dllVertex3i;
		qglVertex3iv = dllVertex3iv;
		qglVertex3s = dllVertex3s;
		qglVertex3sv = dllVertex3sv;
		qglVertex4d = dllVertex4d;
		qglVertex4dv = dllVertex4dv;
		qglVertex4f = dllVertex4f;
		qglVertex4fv = dllVertex4fv;
		qglVertex4i = dllVertex4i;
		qglVertex4iv = dllVertex4iv;
		qglVertex4s = dllVertex4s;
		qglVertex4sv = dllVertex4sv;
		qglVertexPointer = dllVertexPointer;
		qglViewport = dllViewport;
	}
}

extern "C" void GLimp_LogNewFrame()
{
	if ( glw_state.log_fp )
		FS_FPrintf( glw_state.log_fp, "*** R_BeginFrame ***\n" );
}
