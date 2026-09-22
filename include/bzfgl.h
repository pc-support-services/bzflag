/* bzflag
 * Copyright (c) 1993-2025 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __BZFGL_H__
#define __BZFGL_H__

/** this file contains headers necessary for opengl */

#include "common.h"

#include <GL/glew.h>

#ifndef GL_VERSION_1_1
# error OpenGL version 1.1 functionality is required
#endif


/* These will track glBegin/End pairs to make sure that they match */
#ifdef DEBUG
#include <assert.h>
extern int __beginendCount;
#define glBegin(_value) {\
  if (__beginendCount==0) { \
    __beginendCount++;\
  } else {\
    std::cerr << "ERROR: glBegin called on " << __FILE__ << ':' << __LINE__ << " without calling glEnd before\n"; \
    assert(__beginendCount==0 && "glBegin called without glEnd"); \
  } \
  bzglBegin(_value);\
}
#define glEnd() {\
  if (__beginendCount==0) { \
    std::cerr << "ERROR: glEnd called on " << __FILE__ << ':' << __LINE__ << " without calling glBegin before\n"; \
    assert(__beginendCount!=0 && "glEnd called without glBegin"); \
  } else {\
    __beginendCount--;\
  } \
  bzglEnd();\
}
#endif

/* Tier 3: redirect every remaining immediate-mode call through the
 * GLBatch accumulator (bzfgl-batch.cxx). While a display list is being
 * built the macros pass through to the real GL calls, because
 * glDrawArrays cannot be compiled into a display list (fonts, sky,
 * rain, ground all rely on display-list capture).
 */
extern void  bzglBegin(GLenum mode);
extern void  bzglEnd();
extern void  bzglVertex2f(GLfloat x, GLfloat y);
extern void  bzglVertex2fv(const GLfloat v[2]);
extern void  bzglVertex2i(GLint x, GLint y);
extern void  bzglVertex3f(GLfloat x, GLfloat y, GLfloat z);
extern void  bzglVertex3fv(const GLfloat v[3]);
extern void  bzglColor3f(GLfloat r, GLfloat g, GLfloat b);
extern void  bzglColor3fv(const GLfloat c[3]);
extern void  bzglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
extern void  bzglColor4fv(const GLfloat c[4]);
extern void  bzglTexCoord2f(GLfloat s, GLfloat t);
extern void  bzglTexCoord2fv(const GLfloat t[2]);
extern void  bzglNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);
extern void  bzglNormal3fv(const GLfloat n[3]);
extern void  bzglRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
extern void  bzglRecti(GLint x1, GLint y1, GLint x2, GLint y2);
extern void  bzglNewList(GLuint list, GLenum mode);
extern void  bzglEndList();
extern bool  bzGLBatchBuildingList();
extern void  bzGLBatchSetBuildingList(bool building);

#ifndef DEBUG
#define glBegin(_value)         bzglBegin(_value)
#define glEnd()                 bzglEnd()
#endif
#define glVertex2f(x, y)        bzglVertex2f((x), (y))
#define glVertex2fv(v)          bzglVertex2fv(v)
#define glVertex2i(x, y)        bzglVertex2i((x), (y))
#define glVertex3f(x, y, z)     bzglVertex3f((x), (y), (z))
#define glVertex3fv(v)          bzglVertex3fv(v)
#define glColor3f(r, g, b)      bzglColor3f((r), (g), (b))
#define glColor3fv(c)           bzglColor3fv(c)
#define glColor4f(r, g, b, a)   bzglColor4f((r), (g), (b), (a))
#define glColor4fv(c)           bzglColor4fv(c)
#define glTexCoord2f(s, t)      bzglTexCoord2f((s), (t))
#define glTexCoord2fv(t)        bzglTexCoord2fv(t)
#define glNormal3f(x, y, z)     bzglNormal3f((x), (y), (z))
#define glNormal3fv(n)          bzglNormal3fv(n)
#define glRectf(x1, y1, x2, y2) bzglRectf((x1), (y1), (x2), (y2))
#define glRecti(x1, y1, x2, y2) bzglRecti((x1), (y1), (x2), (y2))
#define glNewList(list, mode)   bzglNewList((list), (mode))
#define glEndList()             bzglEndList()


// glGenTextures() should never return 0
#define INVALID_GL_TEXTURE_ID ((GLuint) 0)

// glGenLists() will only return 0 for errors
#define INVALID_GL_LIST_ID ((GLuint) 0)


/* Protect us from ourselves. Warn when these
 * are called inside of the wrong context code
 * sections (freeing and initializing).
 */
//#define DEBUG_GL_MATRIX_STACKS
#ifdef DEBUG
#  define glGenLists(count)         bzGenLists((count))
#  define glGenTextures(count, textures)    bzGenTextures((count), (textures))
#  ifdef DEBUG_GL_MATRIX_STACKS
#    define glPushMatrix()          bzPushMatrix()
#    define glPopMatrix()           bzPopMatrix()
#    define glMatrixMode(mode)          bzMatrixMode(mode)
#  endif // DEBUG_GL_MATRIX_STACKS
#endif
// always swap these calls (context protection)
#define glDeleteLists(base, count)      bzDeleteLists((base), (count))
#define glDeleteTextures(count, textures)   bzDeleteTextures((count), (textures))

// these are housed at the end of OpenGLGState.cxx, for now
extern void   bzNewList(GLuint list, GLenum mode);
extern GLuint bzGenLists(GLsizei count);
extern void   bzGenTextures(GLsizei count, GLuint *textures);
extern void   bzDeleteLists(GLuint base, GLsizei count);
extern void   bzDeleteTextures(GLsizei count, const GLuint *textures);
extern void   bzPushMatrix();
extern void   bzPopMatrix();
extern void   bzMatrixMode(GLenum mode);


#endif /* __BZFGL_H__ */

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
