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
    useNorm(false),
    externalColor(false)
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
    // When the glColor* macro routed through this batch already, the
    // GL current color was never updated - keep curColor instead of
    // clobbering it with the stale GL value.
    if (!externalColor)
        glGetFloatv(GL_CURRENT_COLOR, curColor);
    externalColor = false;
    memcpy(savedColor, curColor, sizeof(savedColor));
    glGetFloatv(GL_CURRENT_TEXTURE_COORDS, savedTex);
    memcpy(curTex, savedTex, sizeof(curTex));
    glGetFloatv(GL_CURRENT_NORMAL, savedNorm);
    memcpy(curNorm, savedNorm, sizeof(curNorm));
    verts.clear();
    cols.clear();
    texs.clear();
    norms.clear();
}


void GLBatch::stampVertex()
{
    // current-value semantics: the active color/texcoord/normal at the
    // time of the glVertex call is the one bound to this vertex
    const int index = (int)verts.size() / (vsize == 2 ? 2 : 3);
    (void)index;

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
    externalColor = true;
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
    externalColor = true;
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
        return;
    }

    if (getenv("GLBATCH_DEBUG") != NULL && verts.size() >= 12)
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

    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);

    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    const int stride = 3; // verts always stored as 3 floats

    glVertexPointer(stride, GL_FLOAT, 0, vptr);
    if (cptr != NULL)
    {
        glColorPointer(csize, GL_FLOAT, 0, cptr);
    }
    else
    {
        // no per-vertex colors: use the last current color for all
        glColor4f(curColor[0], curColor[1], curColor[2], curColor[3]);
    }
    if (tptr != NULL)
        glTexCoordPointer(2, GL_FLOAT, 0, tptr);
    if (nptr != NULL)
        glNormalPointer(GL_FLOAT, 0, nptr);

    const int count = (int)verts.size() / stride;

    switch (mode)
    {
    case GL_TRIANGLE_FAN:
    {
        // expand fan to triangles: v0, vi, vi+1
        std::vector<GLfloat> tv;
        tv.reserve((count - 2) * 9);
        for (int i = 1; (i + 1) < count; i++)
        {
            for (int k = 0; k < 3; k++)
                tv.push_back(verts[k]);
            for (int k = 0; k < 3; k++)
                tv.push_back(verts[i * 3 + k]);
            for (int k = 0; k < 3; k++)
                tv.push_back(verts[(i + 1) * 3 + k]);
            if (cptr != NULL)
            {
                // colors follow the same per-vertex stride
                for (int k = 0; k < csize; k++)
                    tv.push_back(cols[k]);
                for (int k = 0; k < csize; k++)
                    tv.push_back(cols[i * csize + k]);
                for (int k = 0; k < csize; k++)
                    tv.push_back(cols[(i + 1) * csize + k]);
            }
            if (tptr != NULL)
            {
                for (int k = 0; k < 2; k++)
                    tv.push_back(texs[k]);
                for (int k = 0; k < 2; k++)
                    tv.push_back(texs[i * 2 + k]);
                for (int k = 0; k < 2; k++)
                    tv.push_back(texs[(i + 1) * 2 + k]);
            }
            if (nptr != NULL)
            {
                for (int k = 0; k < 3; k++)
                    tv.push_back(norms[k]);
                for (int k = 0; k < 3; k++)
                    tv.push_back(norms[i * 3 + k]);
                for (int k = 0; k < 3; k++)
                    tv.push_back(norms[(i + 1) * 3 + k]);
            }
        }
        // re-issue with interleaved layout
        const int tstride = 3 + (csize > 0 ? csize : 0) + (useTex ? 2 : 0) +
                            (useNorm ? 3 : 0);
        const GLfloat* base = &tv[0];
        const GLfloat* p = base;
        glVertexPointer(3, GL_FLOAT, tstride * sizeof(GLfloat), p);
        p += 3;
        if (cptr != NULL)
        {
            glColorPointer(csize, GL_FLOAT, tstride * sizeof(GLfloat), p);
            p += csize;
        }
        if (tptr != NULL)
        {
            glTexCoordPointer(2, GL_FLOAT, tstride * sizeof(GLfloat), p);
            p += 2;
        }
        if (nptr != NULL)
        {
            glNormalPointer(GL_FLOAT, tstride * sizeof(GLfloat), p);
        }
        glDrawArrays(GL_TRIANGLES, 0, (int)tv.size() / tstride);
        break;
    }

    case GL_LINE_LOOP:
        // draw as a closed loop: strip + one closing segment
        glDrawArrays(GL_LINE_STRIP, 0, count);
        {
            // one extra segment closing the loop
            GLfloat seg[6];
            seg[0] = verts[(count - 1) * 3];
            seg[1] = verts[(count - 1) * 3 + 1];
            seg[2] = verts[(count - 1) * 3 + 2];
            seg[3] = verts[0];
            seg[4] = verts[1];
            seg[5] = verts[2];
            glVertexPointer(3, GL_FLOAT, 0, seg);
            glDrawArrays(GL_LINES, 0, 2);
        }
        break;

    default:
        glDrawArrays(mode, 0, count);
        break;
    }

    glPopClientAttrib();

    // restore the current attribute state to match glBegin/glEnd
    // semantics (the current color/normal/texcoord after glEnd is the
    // last one issued inside the block, not something internal)
    ::glColor4f(curColor[0], curColor[1], curColor[2], curColor[3]);
    ::glNormal3f(curNorm[0], curNorm[1], curNorm[2]);
    ::glTexCoord2f(curTex[0], curTex[1]);
    if (csize > 0 && csize < 4)
    {
        // legacy glColor3f leaves alpha as the pre-begin value
        ::glColor4f(curColor[0], curColor[1], curColor[2], savedColor[3]);
    }

    open = false;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4