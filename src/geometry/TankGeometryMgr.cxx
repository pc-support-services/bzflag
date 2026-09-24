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

// bzflag common headers
#include "common.h"
#include "global.h"

// interface header
#include "TankGeometryMgr.h"

// system headers
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <string.h>

// common implementation headers
#include "SceneRenderer.h"
#include "StateDatabase.h"
#include "BZDBCache.h"
#include "OpenGLGState.h"


// use the namespaces
using namespace TankGeometryMgr;
using namespace TankGeometryEnums;
using namespace TankGeometryUtils;


// Local Variables
// ---------------

// one interleaved VBO batch per (shadow, lod, size, part)
static TankGeometryUtils::PartBatch
    partBatches[TankGeometryEnums::LastTankShadow][TankGeometryEnums::LastTankLOD]
               [TankGeometryEnums::LastTankSize][TankGeometryEnums::LastTankPart];

// triangle counts
static int partTriangles[TankGeometryEnums::LastTankShadow][TankGeometryEnums::LastTankLOD]
[TankGeometryEnums::LastTankSize][TankGeometryEnums::LastTankPart];

// the scaling factors
static GLfloat scaleFactors[LastTankSize][3] =
{
    {1.0f, 1.0f, 1.0f},   // Normal
    {1.0f, 1.0f, 1.0f},   // Obese
    {1.0f, 1.0f, 1.0f},   // Tiny
    {1.0f, 0.001f, 1.0f}, // Narrow
    {1.0f, 1.0f, 1.0f}    // Thief
};
// the current scaling factors
static const float* currentScaleFactor = scaleFactors[Normal];

// the current shadow mode (used to remove glNormal3f and glTexcoord2f calls)
static TankShadow shadowMode = ShadowOn;

// arrays of functions to avoid large switch statements
typedef int (*partFunction)(void);
static const partFunction partFunctions[LastTankLOD][BasicTankParts] =
{
    {
        buildLowBody,
        buildLowBarrel,
        buildLowTurret,
        buildLowLCasing,
        buildLowRCasing
    },
    {
        buildMedBody,
        NULL,
        buildMedTurret,
        buildMedLCasing,
        buildMedRCasing
    },
    {
        buildHighBody,
        buildHighBarrel,
        buildHighTurret,
        buildHighLCasing,
        buildHighRCasing
    }
};


// Local Function Prototypes
// -------------------------

static void setupScales();
static void freeContext(void *data);
static void initContext(void *data);
static void bzdbCallback(const std::string& str, void *data);


/****************************************************************************/

// TankGeometryMgr Functions
// -------------------------


void TankGeometryMgr::init()
{
    // initialize the batches to empty
    for (int shadow = 0; shadow < LastTankShadow; shadow++)
    {
        for (int lod = 0; lod < LastTankLOD; lod++)
        {
            for (int size = 0; size < LastTankSize; size++)
            {
                for (int part = 0; part < LastTankPart; part++)
                {
                    partBatches[shadow][lod][size][part] = PartBatch();
                    partTriangles[shadow][lod][size][part] = 0;
                }
            }
        }
    }

    // install the BZDB callbacks
    // This MUST be done after BZDB has been initialized in main()
    BZDB.addCallback (StateDatabase::BZDB_OBESEFACTOR, bzdbCallback, NULL);
    BZDB.addCallback (StateDatabase::BZDB_TINYFACTOR, bzdbCallback, NULL);
    BZDB.addCallback (StateDatabase::BZDB_THIEFTINYFACTOR, bzdbCallback, NULL);
    BZDB.addCallback ("animatedTreads", bzdbCallback, NULL);

    // install the context initializer
    OpenGLGState::registerContextInitializer (freeContext, initContext, NULL);

    // setup the scaleFactors
    setupScales();

    return;
}


void TankGeometryMgr::kill()
{
    // remove the BZDB callbacks
    BZDB.removeCallback (StateDatabase::BZDB_OBESEFACTOR, bzdbCallback, NULL);
    BZDB.removeCallback (StateDatabase::BZDB_TINYFACTOR, bzdbCallback, NULL);
    BZDB.removeCallback (StateDatabase::BZDB_THIEFTINYFACTOR, bzdbCallback, NULL);
    BZDB.removeCallback ("animatedTreads", bzdbCallback, NULL);

    // remove the context initializer callback
    OpenGLGState::unregisterContextInitializer(freeContext, initContext, NULL);

    return;
}


void TankGeometryMgr::deleteLists()
{
    // free all VBOs and clear the batches
    for (int shadow = 0; shadow < LastTankShadow; shadow++)
    {
        for (int lod = 0; lod < LastTankLOD; lod++)
        {
            for (int size = 0; size < LastTankSize; size++)
            {
                for (int part = 0; part < LastTankPart; part++)
                {
                    PartBatch& batch = partBatches[shadow][lod][size][part];
                    if (batch.vbo != 0)
                    {
                        bzDeleteBuffers(1, &batch.vbo);
                        batch.vbo = 0;
                    }
                    batch.data.clear();
                    batch.data.shrink_to_fit();
                    batch.runs.clear();
                    batch.runShade.clear();
                    partTriangles[shadow][lod][size][part] = 0;
                }
            }
        }
    }
    return;
}


void TankGeometryMgr::buildLists()
{
    // setup the tread style
    setTreadStyle(BZDB.evalInt("treadStyle"));

    // setup the scale factors
    setupScales();
    currentScaleFactor = scaleFactors[Normal];
    const bool animated = BZDBCache::animatedTreads;

    // setup the quality level
    const int divisionLevels[4][2] =   // wheel divs, tread divs
    {
        {4, 4},   // low
        {8, 16},  // med
        {12, 24}, // high
        {16, 32}  // experimental
    };
    int quality = RENDERER.useQuality();
    if (quality < 0)
        quality = 0;
    else if (quality > 3)
        quality = 3;
    int wheelDivs = divisionLevels[quality][0];
    int treadDivs = divisionLevels[quality][1];

    for (int shadow = 0; shadow < LastTankShadow; shadow++)
    {
        for (int lod = 0; lod < LastTankLOD; lod++)
        {
            for (int size = 0; size < LastTankSize; size++)
            {

                // only do the basics, unless we're making an animated tank
                int lastPart = BasicTankParts;
                if (animated)
                    lastPart = HighTankParts;

                // set the shadow mode for the doNormal3f() and doTexcoord2f() calls
                shadowMode = (TankShadow) shadow;

                for (int part = 0; part < lastPart; part++)
                {

                    if ((part == Barrel) && (lod == MedTankLOD))
                        continue;
                    PartBatch& batch = partBatches[shadow][lod][size][part];
                    int& count = partTriangles[shadow][lod][size][part];

                    // free any previous VBO, reset the batch
                    if (batch.vbo != 0)
                    {
                        bzDeleteBuffers(1, &batch.vbo);
                        batch.vbo = 0;
                    }
                    batch.data.clear();
                    batch.runs.clear();
                    batch.runShade.clear();

                    // capture the builder output into the batch
                    beginCapture(&batch);

                    // setup the scale factor
                    currentScaleFactor = scaleFactors[size];

                    if ((part <= Turret) || (!animated))
                    {
                        // the basic parts
                        count = partFunctions[lod][part]();
                    }
                    else
                    {
                        // the animated parts
                        if (part == LeftCasing)
                            count = buildHighLCasingAnim();
                        else if (part == RightCasing)
                            count = buildHighRCasingAnim();
                        else if (part == LeftTread)
                            count = buildHighLTread(treadDivs);
                        else if (part == RightTread)
                            count = buildHighRTread(treadDivs);
                        else if ((part >= LeftWheel0) && (part <= LeftWheel3))
                        {
                            int wheel = part - LeftWheel0;
                            count = buildHighLWheel(wheel, (float)wheel * (float)(M_PI / 2.0),
                                                    wheelDivs);
                        }
                        else if ((part >= RightWheel0) && (part <= RightWheel3))
                        {
                            int wheel = part - RightWheel0;
                            count = buildHighRWheel(wheel, (float)wheel * (float)(M_PI / 2.0),
                                                    wheelDivs);
                        }
                    }

                    endCapture();

                    // upload the interleaved vertex data to a VBO
                    if (!batch.data.empty())
                    {
                        bzGenBuffers(1, &batch.vbo);
                        glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
                        glBufferData(GL_ARRAY_BUFFER,
                                     batch.data.size() * sizeof(GLfloat),
                                     batch.data.data(), GL_STATIC_DRAW);
                        glBindBuffer(GL_ARRAY_BUFFER, 0);
                    }

                } // part
            } // size
        } // lod
    } // shadow

    return;
}


void TankGeometryMgr::drawPart(TankGeometryEnums::TankShadow shadow,
                               TankGeometryEnums::TankPart part,
                               TankGeometryEnums::TankSize size,
                               TankGeometryEnums::TankLOD lod)
{
    if ((part == Barrel) && (lod == MedTankLOD))
        lod = LowTankLOD;

    const PartBatch& batch = partBatches[shadow][lod][size][part];
    if (batch.vbo == 0 || batch.runs.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, batch.vbo);
    const GLsizei stride = 8 * sizeof(GLfloat);
    const GLbyte* base = NULL;
    glVertexPointer(3, GL_FLOAT, stride, base + 0);
    glNormalPointer(GL_FLOAT, stride, base + 3 * sizeof(GLfloat));
    glTexCoordPointer(2, GL_FLOAT, stride, base + 6 * sizeof(GLfloat));

    // save the real shade model: runShade values override it per-run, and
    // OpenGLGState's delta logic assumes nobody changes the shade model
    // behind its back - leaving a run shade applied would desync the
    // tracker and break smooth shading for later draws (flat sky bug)
    GLint savedShade = GL_SMOOTH;
    glGetIntegerv(GL_SHADE_MODEL, &savedShade);

    const size_t runCount = batch.runs.size();
    for (size_t i = 0; i < runCount; i++)
    {
        const PartBatch::Run& run = batch.runs[i];
        glShadeModel(batch.runShade[i]);
        glDrawArrays(run.mode, run.first, run.count);
    }

    glShadeModel(savedShade);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
}


int TankGeometryMgr::getPartTriangleCount(TankGeometryEnums::TankShadow sh,
        TankGeometryEnums::TankPart part,
        TankGeometryEnums::TankSize size,
        TankGeometryEnums::TankLOD lod)
{
    if ((part == Barrel) && (lod == MedTankLOD))
        lod = LowTankLOD;

    return partTriangles[sh][lod][size][part];
}


const float* TankGeometryMgr::getScaleFactor(TankSize size)
{
    return scaleFactors[size];
}


/****************************************************************************/

// Local Functions
// ---------------


static void bzdbCallback(const std::string& UNUSED(name), void * UNUSED(data))
{
    deleteLists();
    buildLists();
    return;
}


static void freeContext(void * UNUSED(data))
{
    // delete all of the lists
    deleteLists();
    return;
}


static void initContext(void * UNUSED(data))
{
    buildLists();
    return;
}


static void setupScales()
{
    float scale;

    scaleFactors[Normal][0] = BZDBCache::tankLength;
    scale = (float)atof(BZDB.getDefault(StateDatabase::BZDB_TANKLENGTH).c_str());
    scaleFactors[Normal][0] /= scale;

    scaleFactors[Normal][1] = BZDBCache::tankWidth;
    scale = (float)atof(BZDB.getDefault(StateDatabase::BZDB_TANKWIDTH).c_str());
    scaleFactors[Normal][1] /= scale;

    scaleFactors[Normal][2] = BZDBCache::tankHeight;
    scale = (float)atof(BZDB.getDefault(StateDatabase::BZDB_TANKHEIGHT).c_str());
    scaleFactors[Normal][2] /= scale;

    scale = BZDB.eval(StateDatabase::BZDB_OBESEFACTOR);
    scaleFactors[Obese][0] = scale * scaleFactors[Normal][0];
    scaleFactors[Obese][1] = scale * scaleFactors[Normal][1];
    scaleFactors[Obese][2] = scaleFactors[Normal][2];

    scale = BZDB.eval(StateDatabase::BZDB_TINYFACTOR);
    scaleFactors[Tiny][0] = scale * scaleFactors[Normal][0];
    scaleFactors[Tiny][1] = scale * scaleFactors[Normal][1];
    scaleFactors[Tiny][2] = scaleFactors[Normal][2];

    scale = BZDB.eval(StateDatabase::BZDB_THIEFTINYFACTOR);
    scaleFactors[Thief][0] = scale * scaleFactors[Normal][0];
    scaleFactors[Thief][1] = scale * scaleFactors[Normal][1];
    scaleFactors[Thief][2] = scaleFactors[Normal][2];

    scaleFactors[Narrow][0] = scaleFactors[Normal][0];
    scaleFactors[Narrow][1] = 0.001f;
    scaleFactors[Narrow][2] = scaleFactors[Normal][2];

    return;
}


/****************************************************************************/

// TankGeometryUtils Functions
// ---------------------------


// capture state (see PartBatch in TankGeometryMgr.h)
static bool capturing = false;
static PartBatch* captureBatch = NULL;
// pending vertices of the run currently being captured
static GLenum  runMode = 0;
static int     runFirst = -1;
static int     runCount = 0;
static GLenum  runShade = GL_FLAT;
// true while the current run captures a re-tessellated triangle fan
// (doVertex3f branches on it; the run itself is tagged GL_TRIANGLES)
static bool    fanRun = false;
// the current normal/texcoord, folded into each vertex as it is appended
static GLfloat lastNormal[3]   = {0.0f, 0.0f, 1.0f};
static GLfloat lastTexCoord[2] = {0.0f, 0.0f};
// the vertex most recently appended (fans need re-tessellation)
static GLfloat lastVertex[8];
static bool    haveLastVertex = false;


void TankGeometryUtils::beginCapture(PartBatch* batch)
{
    captureBatch = batch;
    capturing = true;
    runMode = 0;
    fanRun = false;
    runFirst = -1;
    runCount = 0;
    haveLastVertex = false;
}


void TankGeometryUtils::endCapture()
{
    // close any open run
    if (runCount > 0 && captureBatch != NULL)
    {
        captureBatch->runs.push_back(PartBatch::Run());
        PartBatch::Run& run = captureBatch->runs.back();
        run.mode = runMode;
        run.first = runFirst;
        run.count = runCount;
        captureBatch->runShade.push_back(runShade);
    }
    runMode = 0;
    fanRun = false;
    runFirst = -1;
    runCount = 0;
    haveLastVertex = false;
    capturing = false;
    captureBatch = NULL;
}


// start a new run inside the capture batch
void TankGeometryUtils::startRun(GLenum mode, GLenum shade)
{
    if (captureBatch == NULL)
        return;
    // close the previous run
    if (runCount > 0)
    {
        captureBatch->runs.push_back(PartBatch::Run());
        PartBatch::Run& run = captureBatch->runs.back();
        run.mode = runMode;
        run.first = runFirst;
        run.count = runCount;
        captureBatch->runShade.push_back(runShade);
        runCount = 0;
    }
    runMode = (mode == GL_TRIANGLE_FAN) ? GL_TRIANGLES : mode;
    fanRun = (mode == GL_TRIANGLE_FAN);
    runShade = shade;
    runFirst = (int)(captureBatch->data.size() / 8);
    runCount = 0;
    haveLastVertex = false;
}


// triangle-fan emulation: emit (v0, v[i-1], v[i]) as GL_TRIANGLES.
// strips are emitted natively; fans are re-tessellated because
// glDrawArrays has no fan mode. The run is tagged GL_TRIANGLES in
// startRun(), so this predicate must track the requested fan mode.
static void emitFanVertex(const GLfloat* vtx)
{
    if (captureBatch == NULL)
        return;
    const int count = runCount; // vertices emitted so far in this fan
    if (count < 3)
    {
        captureBatch->data.insert(captureBatch->data.end(), vtx, vtx + 8);
        runCount++;
        return;
    }
    // re-emit vertex 0 and the previous vertex, then this one;
    // copy v0 into a local first - inserting from a range into the same
    // vector is UB if the insert reallocates
    const GLfloat* data = &captureBatch->data[runFirst * 8];
    const GLfloat v0[8] = {data[0], data[1], data[2], data[3],
                           data[4], data[5], data[6], data[7]};
    captureBatch->data.insert(captureBatch->data.end(), v0, v0 + 8);
    captureBatch->data.insert(captureBatch->data.end(), lastVertex, lastVertex + 8);
    captureBatch->data.insert(captureBatch->data.end(), vtx, vtx + 8);
    runCount += 3;
}


void TankGeometryUtils::doVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    const float* scale = currentScaleFactor;
    x = x * scale[0];
    y = y * scale[1];
    z = z * scale[2];
    if (capturing && captureBatch != NULL)
    {
        GLfloat vtx[8];
        vtx[0] = x;
        vtx[1] = y;
        vtx[2] = z;
        vtx[3] = lastNormal[0];
        vtx[4] = lastNormal[1];
        vtx[5] = lastNormal[2];
        vtx[6] = lastTexCoord[0];
        vtx[7] = lastTexCoord[1];
        if (fanRun)
            emitFanVertex(vtx);
        else
        {
            captureBatch->data.insert(captureBatch->data.end(), vtx, vtx + 8);
            runCount++;
        }
        memcpy(lastVertex, vtx, sizeof(lastVertex));
        haveLastVertex = true;
    }
    else
        glVertex3f(x, y, z);
    return;
}


void TankGeometryUtils::doNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (shadowMode == ShadowOn)
        return;
    const float* scale = currentScaleFactor;
    GLfloat sx = x * scale[0];
    GLfloat sy = y * scale[1];
    GLfloat sz = z * scale[2];
    const GLfloat d = sqrtf ((sx * sx) + (sy * sy) + (sz * sz));
    if (d > 1.0e-5f)
    {
        x *= scale[0] / d;
        y *= scale[1] / d;
        z *= scale[2] / d;
    }
    lastNormal[0] = x;
    lastNormal[1] = y;
    lastNormal[2] = z;
    return;
}


void TankGeometryUtils::doTexCoord2f(GLfloat x, GLfloat y)
{
    if (shadowMode == ShadowOn)
        return;
    lastTexCoord[0] = x;
    lastTexCoord[1] = y;
    return;
}


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
