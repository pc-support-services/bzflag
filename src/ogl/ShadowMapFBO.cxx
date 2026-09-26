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

// interface header
#include "ShadowMapFBO.h"

// system headers
#include <string.h>
#include <stdio.h>

// common headers
#include "OpenGLGState.h"
#include "bzfio.h"

ShadowMapFBO::ShadowMapFBO()
    : contextActive(false), usable(false), size(0),
      framebuffer(0), depthTexture(0)
{
    memset(projection, 0, sizeof(projection));
    memset(view, 0, sizeof(view));
    OpenGLGState::registerContextInitializer(freeContext, initContext,
            (void*)this);
}

ShadowMapFBO::~ShadowMapFBO()
{
    OpenGLGState::unregisterContextInitializer(freeContext, initContext,
            (void*)this);
    if (contextActive)
        destroy();
}

ShadowMapFBO& ShadowMapFBO::instance()
{
    static ShadowMapFBO theShadowFBO;
    return theShadowFBO;
}

void ShadowMapFBO::destroy()
{
    if (framebuffer != 0)
        glDeleteFramebuffers(1, &framebuffer);
    if (depthTexture != 0)
        glDeleteTextures(1, &depthTexture);
    framebuffer = 0;
    depthTexture = 0;
    usable = false;
    contextActive = false;
}

bool ShadowMapFBO::checkState(int newSize)
{
    if (newSize < 64)
        newSize = 64;
    if (newSize > 4096)
        newSize = 4096;
    if ((size == newSize) && usable)
        return true;
    size = newSize;

    if (!contextActive)
        return false;

    destroy();

    // depth texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32,
                 size, size, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // hardware depth compare off: the shader does the compare so it can
    // blend shadow falloff; sample raw depth instead
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    // clamp: outside the map = far (lit); avoids shadow projection wrap
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // fbo with depth attachment only
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        logDebugMessage(1, "ShadowMapFBO: incomplete (0x%04x), "
                        "shadow mapping disabled\n", status);
        usable = false;
        return false;
    }

    usable = true;
    logDebugMessage(2, "ShadowMapFBO: %dx%d depth map ready\n", size, size);
    return true;
}

void ShadowMapFBO::bindForWriting(const GLfloat proj[16],
                                  const GLfloat viewM[16])
{
    if (!usable)
        return;
    memcpy(projection, proj, sizeof(projection));
    memcpy(view, viewM, sizeof(view));

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, size, size);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMapFBO::bindForReading(GLenum textureUnit)
{
    if (!usable)
        return;
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
}

void ShadowMapFBO::freeContext(void* /*self*/)
{
    instance().destroy();
}

void ShadowMapFBO::initContext(void* /*self*/)
{
    instance().contextActive = true;
    // rebuild at the current size on next checkState
    const int s = instance().size;
    instance().size = 0;
    if (s > 0)
        instance().checkState(s);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4