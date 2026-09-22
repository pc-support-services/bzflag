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
 *  Tier 2 stage C sun shadow mapping driver. Renders the scene depth
 *  from the sun into ShadowMapFBO each frame and provides the
 *  eye-space -> sun-clip matrix consumed by TankLightingShader.
 */

#ifndef BZF_SHADOW_MAPPER_H
#define BZF_SHADOW_MAPPER_H

#include "common.h"
#include "bzfgl.h"

class SceneRenderer;
class ViewFrustum;

class ShadowMapper
{
public:
    static ShadowMapper& instance();

    // render the depth pass from the sun; sets isActive() based on the
    // gate conditions (GLSL tank path on, sun up, shadows enabled,
    // shadowMapSize > 0). Call once per frame before scene drawing.
    void renderShadowPass(SceneRenderer& renderer);

    // rebuild the eye->sun-clip matrix after the camera view is set.
    // call after frustum.executeView() but before drawing tanks.
    void updateEyeToSunClip(const ViewFrustum& frustum);

    bool isActive() const;
    const GLfloat* getEyeToSunClip() const;

private:
    ShadowMapper();
    ShadowMapper(const ShadowMapper&);
    ShadowMapper& operator=(const ShadowMapper&);

    bool active;
    GLfloat eyeToSunClip[16];   // eye space -> sun clip space
    GLfloat sunView[16];
    GLfloat sunProj[16];
};

#endif // BZF_SHADOW_MAPPER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4