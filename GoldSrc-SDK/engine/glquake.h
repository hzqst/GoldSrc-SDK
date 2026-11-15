#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif
#include <GL/gl.h>

#include "const.h"
#include "mathlib.h"

struct mplane_s;

#define TEX_TYPE_NONE 0
#define TEX_TYPE_ALPHA 1
#define TEX_TYPE_LUM 2
#define TEX_TYPE_ALPHA_GRADIENT 3
#define TEX_TYPE_RGBA 4

#define MAX_LIGHTMAPS 64
#define BLOCK_WIDTH 128
#define BLOCK_HEIGHT 128

#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_TEXTURE0_SGIS 0x835E
#define GL_TEXTURE1_SGIS 0x835F
#define GL_TEXTURE2_SGIS 0x8360

typedef struct alight_s
{
	int ambientlight; // clip at 128
	int shadelight; // clip at 192 - ambientlight
	vec3_t color;
	float* plightvec;
} alight_t;

typedef enum
{
	GLT_SYSTEM,
	GLT_DECAL,
	GLT_HUDSPRITE,
	GLT_STUDIO,
	GLT_WORLD,
	GLT_SPRITE
} GL_TEXTURETYPE;

extern void( APIENTRY* qglAlphaFunc )( GLenum function, GLclampf reference );
extern void( APIENTRY* qglBegin )( GLenum mode );
extern void( APIENTRY* qglBindTexture )( GLenum target, GLuint texture );
extern void( APIENTRY* qglBlendFunc )( GLenum source, GLenum destination );
extern void( APIENTRY* qglClear )( GLbitfield mask );
extern void( APIENTRY* qglClearColor )( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
extern void( APIENTRY* qglColor3f )( GLfloat red, GLfloat green, GLfloat blue );
extern void( APIENTRY* qglColor4f )( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
extern void( APIENTRY* qglColor4ub )( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
extern void( APIENTRY* qglColor4ubv )( const GLubyte* color );
extern void( APIENTRY* qglCullFace )( GLenum mode );
extern void( APIENTRY* qglDeleteTextures )( GLsizei count, const GLuint* textures );
extern void( APIENTRY* qglDepthFunc )( GLenum function );
extern void( APIENTRY* qglDepthMask )( GLboolean enabled );
extern void( APIENTRY* qglDepthRange )( GLclampd nearDepth, GLclampd farDepth );
extern void( APIENTRY* qglDisable )( GLenum capability );
extern void( APIENTRY* qglDrawArrays )( GLenum mode, GLint first, GLsizei count );
extern void( APIENTRY* qglDrawBuffer )( GLenum mode );
extern void( APIENTRY* qglEnable )( GLenum capability );
extern void( APIENTRY* qglEnableClientState )( GLenum array );
extern void( APIENTRY* qglEnd )();
extern void( APIENTRY* qglFinish )();
extern void( APIENTRY* qglFogf )( GLenum name, GLfloat value );
extern void( APIENTRY* qglFogfv )( GLenum name, const GLfloat* values );
extern void( APIENTRY* qglFrustum )(
	GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearDepth, GLdouble farDepth );
extern void( APIENTRY* qglGenTextures )( GLsizei count, GLuint* textures );
extern void( APIENTRY* qglGetFloatv )( GLenum name, GLfloat* values );
extern void( APIENTRY* qglGetIntegerv )( GLenum name, GLint* value );
extern const GLubyte*( APIENTRY* qglGetString )( GLenum name );
extern void( APIENTRY* qglHint )( GLenum target, GLenum mode );
extern GLboolean( APIENTRY* qglIsEnabled )( GLenum capability );
extern void( APIENTRY* qglLoadIdentity )();
extern void( APIENTRY* qglMatrixMode )( GLenum mode );
extern void( APIENTRY* qglOrtho )(
	GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearDepth, GLdouble farDepth );
extern void( APIENTRY* qglPixelStorei )( GLenum name, GLint value );
extern void( APIENTRY* qglPolygonMode )( GLenum face, GLenum mode );
extern void( APIENTRY* qglPolygonOffset )( GLfloat factor, GLfloat units );
extern void( APIENTRY* qglPopMatrix )();
extern void( APIENTRY* qglPushMatrix )();
extern void( APIENTRY* qglReadBuffer )( GLenum mode );
extern void( APIENTRY* qglReadPixels )(
	GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels );
extern void( APIENTRY* qglRotatef )( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
extern void( APIENTRY* qglScalef )( GLfloat x, GLfloat y, GLfloat z );
extern void( APIENTRY* qglScissor )( GLint x, GLint y, GLsizei width, GLsizei height );
extern void( APIENTRY* qglShadeModel )( GLenum mode );
extern void( APIENTRY* qglTexCoord2f )( GLfloat s, GLfloat t );
extern void( APIENTRY* qglTexCoordPointer )( GLint size, GLenum type, GLsizei stride, const GLvoid* pointer );
extern void( APIENTRY* qglTexEnvf )( GLenum target, GLenum name, GLfloat value );
extern void( APIENTRY* qglTexEnvi )( GLenum target, GLenum name, GLint value );
extern void( APIENTRY* qglTexImage2D )( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height,
	GLint border, GLenum format, GLenum type, const GLvoid* pixels );
extern void( APIENTRY* qglTexParameterf )( GLenum target, GLenum name, GLfloat value );
extern void( APIENTRY* qglTexParameteri )( GLenum target, GLenum name, GLint value );
extern void( APIENTRY* qglTexSubImage2D )( GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height,
	GLenum format, GLenum type, const GLvoid* pixels );
extern void( APIENTRY* qglTranslatef )( GLfloat x, GLfloat y, GLfloat z );
extern void( APIENTRY* qglVertex2f )( GLfloat x, GLfloat y );
extern void( APIENTRY* qglVertex3f )( GLfloat x, GLfloat y, GLfloat z );
extern void( APIENTRY* qglVertex3fv )( const GLfloat* vertex );
extern void( APIENTRY* qglVertexPointer )( GLint size, GLenum type, GLsizei stride, const GLvoid* pointer );
extern void( APIENTRY* qglViewport )( GLint x, GLint y, GLsizei width, GLsizei height );

extern void( APIENTRY* qglColorTableEXT )(
	GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const void* table );
extern void( APIENTRY* qglMTexCoord2fSGIS )( GLenum target, GLfloat s, GLfloat t );
extern void( APIENTRY* qglSelectTextureSGIS )( GLenum texture );

extern void( APIENTRY* qglBindFramebufferEXT )( GLenum target, GLuint framebuffer );
extern void( APIENTRY* qglBindRenderbufferEXT )( GLenum target, GLuint renderbuffer );
extern void( APIENTRY* qglBlitFramebufferEXT )( GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
	GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter );
extern GLenum( APIENTRY* qglCheckFramebufferStatusEXT )( GLenum target );
extern void( APIENTRY* qglDeleteFramebuffersEXT )( GLsizei count, const GLuint* framebuffers );
extern void( APIENTRY* qglDeleteRenderbuffersEXT )( GLsizei count, const GLuint* renderbuffers );
extern void( APIENTRY* qglFramebufferRenderbufferEXT )(
	GLenum target, GLenum attachment, GLenum renderbufferTarget, GLuint renderbuffer );
extern void( APIENTRY* qglFramebufferTexture2DEXT )(
	GLenum target, GLenum attachment, GLenum textureTarget, GLuint texture, GLint level );
extern void( APIENTRY* qglGenFramebuffersEXT )( GLsizei count, GLuint* framebuffers );
extern void( APIENTRY* qglGenRenderbuffersEXT )( GLsizei count, GLuint* renderbuffers );
extern void( APIENTRY* qglRenderbufferStorageEXT )(
	GLenum target, GLenum internalFormat, GLsizei width, GLsizei height );
extern void( APIENTRY* qglRenderbufferStorageMultisampleEXT )(
	GLenum target, GLsizei samples, GLenum internalFormat, GLsizei width, GLsizei height );

extern void( APIENTRY* qglAttachShader )( GLuint program, GLuint shader );
extern void( APIENTRY* qglCompileShader )( GLuint shader );
extern GLuint( APIENTRY* qglCreateProgram )();
extern GLuint( APIENTRY* qglCreateShader )( GLenum type );
extern void( APIENTRY* qglDeleteProgram )( GLuint program );
extern void( APIENTRY* qglDeleteShader )( GLuint shader );
extern void( APIENTRY* qglGetProgramInfoLog )( GLuint program, GLsizei size, GLsizei* length, char* log );
extern void( APIENTRY* qglGetProgramiv )( GLuint program, GLenum name, GLint* value );
extern void( APIENTRY* qglGetShaderInfoLog )( GLuint shader, GLsizei size, GLsizei* length, char* log );
extern void( APIENTRY* qglGetShaderiv )( GLuint shader, GLenum name, GLint* value );
extern GLint( APIENTRY* qglGetUniformLocation )( GLuint program, const char* name );
extern void( APIENTRY* qglLinkProgram )( GLuint program );
extern void( APIENTRY* qglShaderSource )(
	GLuint shader, GLsizei count, const char* const* strings, const GLint* lengths );
extern void( APIENTRY* qglUniform1ui )( GLint location, GLuint value );
extern void( APIENTRY* qglUseProgram )( GLuint program );

//
// gl_draw.c
//
int			GL_LoadTexture( char* identifier, GL_TEXTURETYPE textureType, int width, int height, byte* data,
				int mipmap, int type, byte* palette );
int			GL_LoadTexture2( char* identifier, GL_TEXTURETYPE textureType, int width, int height, byte* data,
				qboolean mipmap, int type, byte* palette, int filter );
void		GL_UnloadTexture( char* name );
GLuint		GL_GenTexture();
qboolean	IsPowerOfTwo( int value );

void		Draw_FillRGBA( int x, int y, int width, int height, int red, int green, int blue, int alpha );
void		Draw_FillRGBABlend( int x, int y, int width, int height, int red, int green, int blue, int alpha );
void		Draw_SetTextColor( float red, float green, float blue );
int			Draw_String( int x, int y, char* text );

void		GLBeginHud();
void		GLFinishHud();

//
// gl_rmain.c
//
void		AllowFog( int allowed );
int			SignbitsForPlane( mplane_s* plane );

extern "C" qboolean R_UseLegacyRender();
