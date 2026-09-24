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

/* GLBatch:
 *   Drop-in replacement for immediate-mode glBegin/glEnd drawing.
 *   Accumulates vertices (+ current color/texcoord/normal, with GL
 *   current-value semantics) and flushes with glDrawArrays on end().
 *   GL_TRIANGLE_FAN runs are drawn natively via glDrawArrays; GL_LINE_LOOP runs
 *   are drawn as a closed line strip. Client-array enable bits are
 *   snapshotted at begin() and restored at end() (no attrib-stack
 *   round trip); array pointers are left as set by the flush.
 */
#ifndef BZF_GL_BATCH_H
#define BZF_GL_BATCH_H

#include "bzfgl.h"
#include <vector>


class GLBatch
{
public:
    GLBatch();
    ~GLBatch();

    void begin(GLenum mode);

    void vertex2f(GLfloat x, GLfloat y);
    void vertex2fv(const GLfloat v[2]);
    void vertex3f(GLfloat x, GLfloat y, GLfloat z);
    void vertex3fv(const GLfloat v[3]);

    void color3f(GLfloat r, GLfloat g, GLfloat b);
    void color3fv(const GLfloat c[3]);
    void color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
    void color4fv(const GLfloat c[4]);

    void texCoord2f(GLfloat s, GLfloat t);
    void texCoord2fv(const GLfloat t[2]);

    void normal3f(GLfloat nx, GLfloat ny, GLfloat nz);
    void normal3fv(const GLfloat n[3]);

    void end();

private:
    void stampVertex();
    void useColors(int size);

private:
    GLenum mode;
    bool open;
    int vsize;            // 2 or 3 components per vertex
    int csize;            // 3 or 4 components per color (0 = no colors)
    bool useTex;
    bool useNorm;
    GLfloat curVert[3];
    GLfloat curColor[4];
    GLfloat curTex[4];     // 4 floats: GL_CURRENT_TEXTURE_COORDS returns s,t,r,q
    GLfloat curNorm[3];
    GLfloat savedColor[4]; // current attributes at begin(), restored at end()
    GLfloat savedTex[4];
    GLfloat savedNorm[3];
    bool savedVertexArray; // client-array enable bits at begin(), restored at end()
    bool savedColorArray;  // client-array enable bits at begin(), restored at end()
    bool savedTexCoordArray;
    bool savedNormalArray;
    std::vector<GLfloat> verts;
    std::vector<GLfloat> cols;
    std::vector<GLfloat> texs;
    std::vector<GLfloat> norms;
};

#endif // BZF_GL_BATCH_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4