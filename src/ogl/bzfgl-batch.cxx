// bzfgl-batch.cxx: backing store for the bzfgl.h immediate-mode redirect.
//
// All immediate-mode glVertex/glColor/glTexCoord/glNormal/glRect calls
// compile into a single thread-local GLBatch (via the macros in
// bzfgl.h). A display-list bypass flag switches every macro back to the
// real GL call so glNewList capture keeps working (fonts, sky, rain,
// ground all build display lists and glDrawArrays cannot be compiled
// into one).

#include "common.h"

// interface header (bzfgl.h, for the macros to undef)
#include "bzfgl.h"

#include "GLBatch.h"

static GLBatch bzBatch;

static bool bzBuildingList = false;

bool bzGLBatchBuildingList()
{
    return bzBuildingList;
}

void bzGLBatchSetBuildingList(bool building)
{
    bzBuildingList = building;
}

GLBatch* bzGLBatchGet()
{
    return &bzBatch;
}

// --- immediate-mode wrappers ---------------------------------------------

#undef glBegin
void bzglBegin(GLenum mode)
{
    if (!bzBuildingList)
    {
        bzBatch.begin(mode);
        return;
    }
    ::glBegin(mode);
}

#undef glEnd
void bzglEnd()
{
    if (!bzBuildingList)
    {
        bzBatch.end();
        return;
    }
    ::glEnd();
}

#undef glVertex2f
void bzglVertex2f(GLfloat x, GLfloat y)
{
    if (!bzBuildingList)
    {
        bzBatch.vertex2f(x, y);
        return;
    }
    ::glVertex2f(x, y);
}

#undef glVertex2fv
void bzglVertex2fv(const GLfloat v[2])
{
    if (!bzBuildingList)
    {
        bzBatch.vertex2fv(v);
        return;
    }
    ::glVertex2fv(v);
}

#undef glVertex2i
void bzglVertex2i(GLint x, GLint y)
{
    if (!bzBuildingList)
    {
        bzBatch.vertex2f((GLfloat)x, (GLfloat)y);
        return;
    }
    ::glVertex2i(x, y);
}

#undef glVertex3f
void bzglVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (!bzBuildingList)
    {
        bzBatch.vertex3f(x, y, z);
        return;
    }
    ::glVertex3f(x, y, z);
}

#undef glVertex3fv
void bzglVertex3fv(const GLfloat v[3])
{
    if (!bzBuildingList)
    {
        bzBatch.vertex3fv(v);
        return;
    }
    ::glVertex3fv(v);
}

#undef glColor3f
void bzglColor3f(GLfloat r, GLfloat g, GLfloat b)
{
    // always update the real GL current color too: call sites set a
    // color and then draw via display lists or other draw paths that
    // read GL state
    ::glColor3f(r, g, b);
    if (!bzBuildingList)
        bzBatch.color3f(r, g, b);
}

#undef glColor3fv
void bzglColor3fv(const GLfloat c[3])
{
    ::glColor3fv(c);
    if (!bzBuildingList)
        bzBatch.color3fv(c);
}

#undef glColor4f
void bzglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    ::glColor4f(r, g, b, a);
    if (!bzBuildingList)
        bzBatch.color4f(r, g, b, a);
}

#undef glColor4fv
void bzglColor4fv(const GLfloat c[4])
{
    ::glColor4fv(c);
    if (!bzBuildingList)
        bzBatch.color4fv(c);
}

#undef glTexCoord2f
void bzglTexCoord2f(GLfloat s, GLfloat t)
{
    if (!bzBuildingList)
    {
        bzBatch.texCoord2f(s, t);
        return;
    }
    ::glTexCoord2f(s, t);
}

#undef glTexCoord2fv
void bzglTexCoord2fv(const GLfloat t[2])
{
    if (!bzBuildingList)
    {
        bzBatch.texCoord2fv(t);
        return;
    }
    ::glTexCoord2fv(t);
}

#undef glNormal3f
void bzglNormal3f(GLfloat nx, GLfloat ny, GLfloat nz)
{
    ::glNormal3f(nx, ny, nz);
    if (!bzBuildingList)
        bzBatch.normal3f(nx, ny, nz);
}

#undef glNormal3fv
void bzglNormal3fv(const GLfloat n[3])
{
    ::glNormal3fv(n);
    if (!bzBuildingList)
        bzBatch.normal3fv(n);
}

#undef glRectf
void bzglRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
    if (!bzBuildingList)
    {
        // match glRectf winding: (x1,y1) (x2,y1) (x1,y2) | (x1,y2) (x2,y1) (x2,y2)
        bzBatch.begin(GL_TRIANGLE_STRIP);
        bzBatch.vertex2f(x1, y1);
        bzBatch.vertex2f(x2, y1);
        bzBatch.vertex2f(x1, y2);
        bzBatch.vertex2f(x2, y2);
        bzBatch.end();
        return;
    }
    ::glRectf(x1, y1, x2, y2);
}

#undef glRecti
void bzglRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
    bzglRectf((GLfloat)x1, (GLfloat)y1, (GLfloat)x2, (GLfloat)y2);
}

// --- display-list bypass hooks -------------------------------------------

#undef glNewList
void bzglNewList(GLuint list, GLenum mode)
{
    bzBuildingList = true;
    ::glNewList(list, mode);
}

#undef glEndList
void bzglEndList()
{
    ::glEndList();
    bzBuildingList = false;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4