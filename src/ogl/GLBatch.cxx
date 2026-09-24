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

// implementation header
#include "GLBatch.h"

// system headers
#include "bzfgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


GLBatch::GLBatch() :
    mode(GL_POINTS),
    open(false),
    vsize(3),
    csize(0),
    useTex(false),
    useNorm(false)
{
    curColor[0] = curColor[1] = curColor[2] = curColor[3] = 1.0f;
    curTex[0] = curTex[1] = 0.0f;
    curNorm[0] = 0.0f;
    curNorm[1] = 0.0f;
    curNorm[2] = 1.0f;
}


GLBatch::~GLBatch()
{
}


void GLBatch::begin(GLenum _mode)
{
    mode = _mode;
    open = true;
    vsize = 3;
    csize = 0;
    useTex = false;
    useNorm = false;
    // inherit the current GL color (sites set glColor* before drawing).
    glGetFloatv(GL_CURRENT_COLOR, curColor);
    memcpy(savedColor, curColor, sizeof(savedColor));
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, savedTex); // writes 4 floats (s,t,r,q)
    memcpy(curTex, savedTex, sizeof(curTex));
    glGetFloatv(GL_CURRENT_NORMAL, savedNorm);
    memcpy(curNorm, savedNorm, sizeof(curNorm));
    // snapshot client-array enables: end() restores exactly these bits,
    // so a flush never leaks array state into the next draw path (tanks
    // draw through their own VBO pointers and depend on the enable bits
    // they inherited, not ones a GLBatch flush left behind)
    GLboolean b;
    glGetBooleanv(GL_VERTEX_ARRAY, &b);
    savedVertexArray = (b != GL_FALSE);
    glGetBooleanv(GL_COLOR_ARRAY, &b);
    savedColorArray = (b != GL_FALSE);
    glGetBooleanv(GL_TEXTURE_COORD_ARRAY, &b);
    savedTexCoordArray = (b != GL_FALSE);
    glGetBooleanv(GL_NORMAL_ARRAY, &b);
    savedNormalArray = (b != GL_FALSE);
    verts.clear();
    cols.clear();
    texs.clear();
    norms.clear();
}


void GLBatch::stampVertex()
{
    if (vsize == 2)
    {
        verts.push_back(curVert[0]);
        verts.push_back(curVert[1]);
        verts.push_back(0.0f); // z placeholder keeps stride uniform at 3
    }
    else
    {
        verts.push_back(curVert[0]);
        verts.push_back(curVert[1]);
        verts.push_back(curVert[2]);
    }

    if (csize > 0)
        cols.insert(cols.end(), curColor, curColor + csize);
    if (useTex)
        texs.insert(texs.end(), curTex, curTex + 2);
    if (useNorm)
        norms.insert(norms.end(), curNorm, curNorm + 3);
}


void GLBatch::useColors(int size)
{
    if (csize != size)
    {
        csize = size;
    }
}


void GLBatch::vertex2f(GLfloat x, GLfloat y)
{
    curVert[0] = x;
    curVert[1] = y;
    curVert[2] = 0.0f;
    stampVertex();
}


void GLBatch::vertex2fv(const GLfloat v[2])
{
    vertex2f(v[0], v[1]);
}


void GLBatch::vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    curVert[0] = x;
    curVert[1] = y;
    curVert[2] = z;
    stampVertex();
}


void GLBatch::vertex3fv(const GLfloat v[3])
{
    vertex3f(v[0], v[1], v[2]);
}


void GLBatch::color3f(GLfloat r, GLfloat g, GLfloat b)
{
    useColors(3);
    curColor[0] = r;
    curColor[1] = g;
    curColor[2] = b;
}


void GLBatch::color3fv(const GLfloat c[3])
{
    color3f(c[0], c[1], c[2]);
}


void GLBatch::color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    useColors(4);
    curColor[0] = r;
    curColor[1] = g;
    curColor[2] = b;
    curColor[3] = a;
}


void GLBatch::color4fv(const GLfloat c[4])
{
    color4f(c[0], c[1], c[2], c[3]);
}


void GLBatch::texCoord2f(GLfloat s, GLfloat t)
{
    useTex = true;
    curTex[0] = s;
    curTex[1] = t;
}


void GLBatch::texCoord2fv(const GLfloat t[2])
{
    texCoord2f(t[0], t[1]);
}


void GLBatch::normal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    useNorm = true;
    curNorm[0] = nx;
    curNorm[1] = ny;
    curNorm[2] = nz;
}


void GLBatch::normal3fv(const GLfloat n[3])
{
    normal3f(n[0], n[1], n[2]);
}


void GLBatch::end()
{
    if (!open || verts.empty())
    {
        open = false;
        // drop any staged attributes with no vertices (staged texcoords
        // would otherwise linger until the next begin())
        cols.clear();
        texs.clear();
        norms.clear();
        return;
    }

    static const bool dbgEnabled = (getenv("GLBATCH_DEBUG") != NULL);
    if (dbgEnabled && verts.size() >= 12)
    {
        static int dbgCount = 0;
        if (dbgCount++ < 400)
            fprintf(stderr, "GLBatch mode=%d count=%d csize=%d cols=%d tex=%d norm=%d color=%.2f,%.2f,%.2f,%.2f v=(%.1f,%.1f)-(%.1f,%.1f)\n",
                    (int)mode, (int)verts.size() / 3, csize, (int)cols.size(),
                    (int)useTex, (int)useNorm,
                    curColor[0], curColor[1], curColor[2], curColor[3],
                    verts[0], verts[1], verts[3], verts[4]);
    }

    const GLfloat* vptr = &verts[0];
    const GLfloat* cptr = csize > 0 && !cols.empty() ? &cols[0] : NULL;
    const GLfloat* tptr = useTex && !texs.empty() ? &texs[0] : NULL;
    const GLfloat* nptr = useNorm && !norms.empty() ? &norms[0] : NULL;

    // NOTE: no glPushClientAttrib/glPopClientAttrib here - this runs dozens
    // of times per frame (radar + HUD), and the push/pop is pure overhead.
    // Instead set every client-array enable bit explicitly below and leave
    // the last state behind, exactly like the legacy immediate-mode path
    // never touched array state at all. Consumers that care (renderRadar
    // fast path) already manage their own enables around their draws.

    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    const int stride = 3; // verts always stored as 3 floats

    glVertexPointer(stride, GL_FLOAT, 0, vptr);
    glEnableClientState(GL_VERTEX_ARRAY);
    if (cptr != NULL)
    {
        glColorPointer(csize, GL_FLOAT, 0, cptr);
        glEnableClientState(GL_COLOR_ARRAY);
    }
    else
    {
        // no per-vertex colors: use the last current color for all
        glColor4f(curColor[0], curColor[1], curColor[2], curColor[3]);
    }
    if (tptr != NULL)
    {
        glTexCoordPointer(2, GL_FLOAT, 0, tptr);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    if (nptr != NULL)
    {
        glNormalPointer(GL_FLOAT, 0, nptr);
        glEnableClientState(GL_NORMAL_ARRAY);
    }

    const int count = (int)verts.size() / stride;

    switch (mode)
    {
    case GL_TRIANGLE_FAN:
        // GL 1.1 client arrays support GL_TRIANGLE_FAN directly (and
        // GL 3.x core does not, but this codebase is compat-profile) -
        // draw natively instead of re-tessellating every frame.
        glDrawArrays(GL_TRIANGLE_FAN, 0, count);
        break;

    case GL_LINE_LOOP:
        // draw as a closed loop: strip + one closing segment
        glDrawArrays(GL_LINE_STRIP, 0, count);
        {
            // one extra segment closing the loop; set its own attribute
            // pointers so the closer does not sample the strip's arrays
            // (which would color it with v0/v1's attributes)
            GLfloat seg[6];
            seg[0] = verts[(count - 1) * 3];
            seg[1] = verts[(count - 1) * 3 + 1];
            seg[2] = verts[(count - 1) * 3 + 2];
            seg[3] = verts[0];
            seg[4] = verts[1];
            seg[5] = verts[2];
            glDisableClientState(GL_COLOR_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_NORMAL_ARRAY);
            glVertexPointer(3, GL_FLOAT, 0, seg);
            glDrawArrays(GL_LINES, 0, 2);
            // repoint the vertex array at the batch buffer: the restore
            // block below may re-enable GL_VERTEX_ARRAY (begin() snapshot),
            // and it must not keep referencing the stack-local seg buffer
            glVertexPointer(3, GL_FLOAT, 0, vptr);
        }
        break;

    default:
        glDrawArrays(mode, 0, count);
        break;
    }

    // restore the client-array enable bits to their begin() state so a
    // flush never leaks array state into the next draw path (the tank VBO
    // path inherits enable bits instead of setting its own). Cheap: three
    // enable/disable calls, no attrib-stack round trip.
    if (savedVertexArray)
        glEnableClientState(GL_VERTEX_ARRAY);
    else
        glDisableClientState(GL_VERTEX_ARRAY);
    if (savedColorArray)
        glEnableClientState(GL_COLOR_ARRAY);
    else
        glDisableClientState(GL_COLOR_ARRAY);
    if (savedTexCoordArray)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    if (savedNormalArray)
        glEnableClientState(GL_NORMAL_ARRAY);
    else
        glDisableClientState(GL_NORMAL_ARRAY);

    // restore only the current attribute values to match glEnd semantics
    // (the current color/normal/texcoord after glEnd is the last one issued
    // inside the block, not something internal).
    ::glColor4f(curColor[0], curColor[1], curColor[2], curColor[3]);
    if (csize > 0 && csize < 4)
    {
        // legacy glColor3f leaves alpha as the pre-begin value
        ::glColor4f(curColor[0], curColor[1], curColor[2], savedColor[3]);
    }
    if (useNorm)
        ::glNormal3f(curNorm[0], curNorm[1], curNorm[2]);
    if (useTex)
        ::glTexCoord2f(curTex[0], curTex[1]);

    open = false;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4