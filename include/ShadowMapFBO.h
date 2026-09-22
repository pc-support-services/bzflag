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

/* ShadowMapFBO:
 *  Encapsulates the depth-only framebuffer used for Tier 2 stage C
 *  sun shadow mapping of tank geometry.
 *
 *  Pipeline per frame (see BackgroundRenderer::drawGroundShadows):
 *    1. render scene depth from the sun into a DEPTH_COMPONENT32 FBO
 *       (color attachment not needed; draw buffers disabled)
 *    2. sample that depth map in the tank lighting fragment shader;
 *       fragments closer to the sun than the tank fragment are lit,
 *       the rest are in shadow (diffuse+specular only, ambient stays)
 *
 *  The map covers an orthographic box around the camera focus area
 *  (world origin + worldSize); BZFlag maps are bounded so this is
 *  sufficient and keeps the shadow projection independent of camera
 *  orientation. Resolution is set from the "shadowMapSize" BZDB var.
 *
 *  All objects created lazily on first use per context; freed via the
 *  OpenGLGState context callbacks like OpenGLFramebuffer.
 */

#ifndef SHADOWMAPFBO_H
#define SHADOWMAPFBO_H

#include "common.h"
#include "bzfgl.h"

class ShadowMapFBO
{
public:
    static ShadowMapFBO& instance();

    // (re)create the FBO at the given resolution; safe to call every frame
    // (no-ops when size unchanged). returns false if FBO setup failed
    // (then callers must skip the shadow map path).
    bool checkState(int size);

    // bind for depth rendering from the sun; also stores the projection
    // matrix used (so the draw pass can rebuild the compare coords)
    void bindForWriting(const GLfloat projection[16], const GLfloat view[16]);

    // bind the depth texture for reading (texture unit must be active)
    void bindForReading(GLenum textureUnit);

    const GLfloat* getProjection() const
    {
        return projection;
    }
    const GLfloat* getView() const
    {
        return view;
    }
    GLint getDepthTexture() const
    {
        return depthTexture;
    }
    int getSize() const
    {
        return size;
    }
    bool isUsable() const
    {
        return usable;
    }

    static void freeContext(void*);
    static void initContext(void*);

private:
    ShadowMapFBO();
    ~ShadowMapFBO();
    ShadowMapFBO(const ShadowMapFBO&);
    ShadowMapFBO& operator=(const ShadowMapFBO&);

    void destroy();

    bool contextActive;
    bool usable;
    int size;
    GLuint framebuffer;
    GLuint depthTexture;
    GLfloat projection[16];
    GLfloat view[16];
};

#endif // SHADOWMAPFBO_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4