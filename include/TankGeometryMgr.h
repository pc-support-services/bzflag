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

/* TankGeometryMgr:
 *  Generates the display lists for TankSceneNodes
 */

#ifndef BZF_TANK_GEOMETRY_MGR_H
#define BZF_TANK_GEOMETRY_MGR_H

#include "common.h"
#include "SceneNode.h"


namespace TankGeometryEnums
{

enum TankShadow
{
    ShadowOff = 0,
    ShadowOn,
    LastTankShadow
};

enum TankLOD
{
    LowTankLOD = 0,
    MedTankLOD,
    HighTankLOD,
    LastTankLOD
};

enum TankSize
{
    Normal = 0,
    Obese,
    Tiny,
    Narrow,
    Thief,
    LastTankSize
};

enum TankPart
{
    Body = 0,
    Barrel,
    Turret,
    LeftCasing,
    RightCasing,

    LeftTread, // animated parts
    RightTread,
    LeftWheel0,
    LeftWheel1,
    LeftWheel2,
    LeftWheel3,
    RightWheel0,
    RightWheel1,
    RightWheel2,
    RightWheel3,

    LastTankPart,
    BasicTankParts = LeftTread,
    LowTankParts = LeftTread,
    MedTankParts = LeftTread,
    HighTankParts = LastTankPart
};
}


namespace TankGeometryMgr
{

void init();
void kill();
void buildLists();
void deleteLists();

// draw a tank part from its VBO batch (replaces the old display-list call)
void drawPart(TankGeometryEnums::TankShadow shadow,
              TankGeometryEnums::TankPart part,
              TankGeometryEnums::TankSize size,
              TankGeometryEnums::TankLOD lod);

int getPartTriangleCount(TankGeometryEnums::TankShadow shadow,
                         TankGeometryEnums::TankPart part,
                         TankGeometryEnums::TankSize size,
                         TankGeometryEnums::TankLOD lod);

const float* getScaleFactor(TankGeometryEnums::TankSize size);
}


namespace TankGeometryUtils
{

enum TreadStyle
{
    Covered = 0,
    Exposed = 1
};
void setTreadStyle(int style);

// degrees / meter
float getWheelScale();
// texcoords / meter
float getTreadScale();
// texcoords
float getTreadTexLen();

// Capture state: while capturing, doVertex3f/doNormal3f/doTexCoord2f append
// into an interleaved array instead of issuing immediate-mode GL calls.
// The builder functions (buildLowBody etc.) are called unchanged inside a
// capture; the arrays are then uploaded to a VBO once per (shadow, lod, size,
// part) and drawn with glDrawArrays runs. Zero visual change vs the old
// display lists: same primitives, same order, same fixed-function state.
struct PartBatch
{
    // interleaved: 3 pos + 3 normal + 2 texcoord = 8 floats per vertex
    std::vector<GLfloat>   data;
    // runs of consecutive vertices sharing one primitive mode
    struct Run
    {
        GLenum mode;   // GL_TRIANGLE_STRIP / GL_TRIANGLE_FAN / GL_TRIANGLES
        GLint  first;  // first vertex index
        GLsizei count; // vertex count
    };
    std::vector<Run>   runs;
    // shade model in effect when each run started (GL_FLAT / GL_SMOOTH),
    // recorded so drawing can restore per-run shading exactly as the
    // display lists did
    std::vector<GLenum> runShade;
    int   triangles;
    GLuint vbo;      // 0 = not uploaded
};

// help to scale vertices and normals
void doVertex3f(GLfloat x, GLfloat y, GLfloat z);
void doNormal3f(GLfloat x, GLfloat y, GLfloat z);
void doTexCoord2f(GLfloat x, GLfloat y);

// capture control (used by TankGeometryMgr::buildLists)
void beginCapture(PartBatch* batch);
void endCapture();
// start a new primitive run (called by the geometry builders in place of
// glBegin/glShadeModel); mode is GL_TRIANGLE_STRIP/GL_TRIANGLE_FAN/GL_TRIANGLES,
// shade is GL_FLAT/GL_SMOOTH
void startRun(GLenum mode, GLenum shade);

//
// NOTE:  these all return their triangle count
//

// lowtank geometry builder
int buildLowBody (void);
int buildLowTurret (void);
int buildLowLCasing (void);
int buildLowRCasing (void);
int buildLowBarrel (void);

// medtank geometry builder
int buildMedBody (void);
int buildMedTurret (void);
int buildMedLCasing (void);
int buildMedRCasing (void);
//int buildMedBarrel (void); // Same as buildLowBarrel

// hightank geometry builder
int buildHighBody (void);
int buildHighBarrel (void);
int buildHighTurret (void);
int buildHighLCasing (void);
int buildHighRCasing (void);

// animated geometry builder
int buildHighLCasingAnim (void);
int buildHighRCasingAnim (void);
int buildHighLTread (int divs);
int buildHighRTread (int divs);
int buildHighLWheel (int wheel, float angle, int divs);
int buildHighRWheel (int wheel, float angle, int divs);
}


#endif // BZF_TANK_GEOMETRY_MGR_H


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
