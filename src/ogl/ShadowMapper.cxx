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

/* ShadowMapper:
 *  Tier 2 stage C: renders the scene depth from the sun into the
 *  ShadowMapFBO and hands the resulting matrix/texture to the tank
 *  lighting shader.
 *
 *  The sun shadow projection is a single orthographic box covering the
 *  whole map (BZFlag worlds are bounded by worldSize), so the map is
 *  view-independent and only needs re-rendering when the sun direction
 *  or scene changes (re-rendered every frame here for correctness;
 *  depth pass is cheap -- no shaders, no color writes).
 *
 *  Matrix chain handed to the shader maps EYE space (fragment v_eyePos)
 *  -> sun clip space:
 *      sunClip = Proj_sun * View_sun * Inv_view_camera
 *  where Inv_view_camera is built from the camera view matrix Frustum
 *  provides (rotation-only upper 3x3 + translation, so the inverse is
 *  the transpose of the 3x3 plus the negated translation).
 */

// interface header
#include "ShadowMapper.h"

// system headers
#include <string.h>
#include <math.h>

// common headers
#include "ShadowMapFBO.h"
#include "TankLightingShader.h"
#include "SceneRenderer.h"
#include "ViewFrustum.h"
#include "SceneNode.h"
#include "OpenGLGState.h"
#include "bzfgl.h"
#include "BZDBCache.h"
#include "StateDatabase.h"

ShadowMapper::ShadowMapper()
    : active(false)
{
    memset(eyeToSunClip, 0, sizeof(eyeToSunClip));
    memset(sunView, 0, sizeof(sunView));
    memset(sunProj, 0, sizeof(sunProj));
}

ShadowMapper& ShadowMapper::instance()
{
    static ShadowMapper theMapper;
    return theMapper;
}

bool ShadowMapper::isActive() const
{
    return active;
}

// build a look-at style view matrix looking down -dir with world up
static void buildSunView(const float* dir, GLfloat* m)
{
    // orthonormal basis: dir points FROM the scene TO the sun
    const float d[3] = { dir[0], dir[1], dir[2] };
    // right = normalize(cross(d, worldUp)) with worldUp = (0,0,1)
    float r[3] = { d[1], -d[0], 0.0f };
    float rl = sqrtf(r[0]*r[0] + r[1]*r[1]);
    if (rl < 1.0e-6f)
    {
        // sun straight up: use x axis
        r[0] = 1.0f;
        r[1] = 0.0f;
        rl = 1.0f;
    }
    r[0] /= rl;
    r[1] /= rl;
    // true up = cross(r, d)
    float u[3] =
    {
        r[1]*d[2] - r[2]*d[1],
        r[2]*d[0] - r[0]*d[2],
        r[0]*d[1] - r[1]*d[0]
    };

    m[0] = r[0];
    m[4] = r[1];
    m[8]  = r[2];
    m[12] = 0.0f;
    m[1] = u[0];
    m[5] = u[1];
    m[9]  = u[2];
    m[13] = 0.0f;
    m[2] = -d[0];
    m[6] = -d[1];
    m[10] = -d[2];
    m[14] = 0.0f;
    m[3] = 0.0f;
    m[7] = 0.0f;
    m[11] = 0.0f;
    m[15] = 1.0f;
}

void ShadowMapper::renderShadowPass(SceneRenderer& renderer)
{
    // gate: needs the GLSL tank path, sun/moon above horizon, shadows on
    const GLfloat* sunDir = renderer.getSunDirection();
    active = TankLightingShader::instance().isActive()
             && (sunDir != NULL)
             && BZDBCache::shadows
             && !BZDB.isTrue(StateDatabase::BZDB_NOSHADOWS)
             && BZDB.evalInt("shadowMapSize") > 0;
    if (!active)
        return;

    // (re)create the depth FBO at the requested size
    if (!ShadowMapFBO::instance().checkState(
                BZDB.evalInt("shadowMapSize")))
    {
        active = false;
        return;
    }

    // --- sun orthographic projection covering the map ---
    // sun view: look from +sunDir toward the origin, z up
    buildSunView(sunDir, sunView);

    // half-extent: cover the map plus tank height margin
    const float halfExtent = 0.75f * BZDBCache::worldSize;
    const float nearZ = -2.0f * BZDBCache::worldSize;
    const float farZ  =  2.0f * BZDBCache::worldSize;

    memset(sunProj, 0, sizeof(sunProj));
    sunProj[0]  = 1.0f / halfExtent;    // x: [-he, he] -> [-1, 1]
    sunProj[5]  = 1.0f / halfExtent;
    sunProj[10] = 2.0f / (farZ - nearZ);
    sunProj[14] = -(farZ + nearZ) / (farZ - nearZ);
    sunProj[15] = 1.0f;

    // --- render scene depth from the sun ---
    ShadowMapFBO::instance().bindForWriting(sunProj, sunView);

    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_VIEWPORT_BIT |
                 GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                 GL_LIGHTING_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glPolygonOffset(2.0f, 4.0f);    // pull depth back vs the lit pass
    glEnable(GL_POLYGON_OFFSET_FILL);
    glShadeModel(GL_FLAT);

    // sun view transform: load the sun view matrix, then draw the scene
    // nodes in WORLD space (their render()s push their own transforms)
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadMatrixf(sunView);

    // everything that casts a shadow (same node set the projected
    // ground shadows use, minus the ground-receiver-only nodes)
    renderer.getShadowList().render();

    glPopMatrix();
    glPopAttrib();

    // restore the camera FBO + viewport + transform
    OpenGLGState::resetState();
}

const GLfloat* ShadowMapper::getEyeToSunClip() const
{
    return eyeToSunClip;
}

// combine: sunClip = Proj_sun * View_sun * inv(View_camera)
void ShadowMapper::updateEyeToSunClip(const ViewFrustum& frustum)
{
    if (!active)
        return;

    // inverse of the camera view matrix: rotation-only 3x3 (columns are
    // the camera axes in world space) + translation
    const GLfloat* v = frustum.getViewMatrix();
    GLfloat inv[16];
    // transpose rotation part
    inv[0] = v[0];
    inv[4] = v[1];
    inv[8]  = v[2];
    inv[12] = 0.0f;
    inv[1] = v[4];
    inv[5] = v[5];
    inv[9]  = v[6];
    inv[13] = 0.0f;
    inv[2] = v[8];
    inv[6] = v[9];
    inv[10] = v[10];
    inv[14] = 0.0f;
    // translation: -R^T * t
    const float t[3] = { v[12], v[13], v[14] };
    inv[12] = -(v[0]*t[0] + v[1]*t[1] + v[2]*t[2]);
    inv[13] = -(v[4]*t[0] + v[5]*t[1] + v[6]*t[2]);
    inv[14] = -(v[8]*t[0] + v[9]*t[1] + v[10]*t[2]);
    inv[3] = 0.0f;
    inv[7] = 0.0f;
    inv[11] = 0.0f;
    inv[15] = 1.0f;

    // eyeToSunClip = sunProj * sunView * inv
    GLfloat tmp[16];
    for (int c = 0; c < 4; c++)
    {
        for (int r = 0; r < 4; r++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += sunProj[k*4 + r] * sunView[c*4 + k];
            tmp[c*4 + r] = sum;
        }
    }
    for (int c = 0; c < 4; c++)
    {
        for (int r = 0; r < 4; r++)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++)
                sum += tmp[k*4 + r] * inv[c*4 + k];
            eyeToSunClip[c*4 + r] = sum;
        }
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4