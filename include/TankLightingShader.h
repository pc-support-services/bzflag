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

/* TankLightingShader:
 *  GLSL replacement of the fixed-function lighting path for tank geometry.
 *
 *  Why: fixed-function lighting is per-vertex (Gouraud). Tank parts are large
 *  flat quads (hulls, casings), so per-vertex specular washes out and point
 *  light falloff misses. This shader computes Blinn-Phong lighting per-PIXEL.
 *
 *  It reads the compatibility-profile built-in uniforms directly:
 *    gl_LightSource[0..7]   - sun (light 0) + dynamic point lights; the
 *                             renderer already maintains these via glLightfv,
 *                             positions in eye space
 *    gl_LightModel.ambient  - global ambient (glLightModelfv)
 *    gl_FrontMaterial       - specular/shininess/emission (glMaterialfv)
 *    gl_Fog                 - density/start/end/color (glFogf/glFogfv)
 *  Only two CPU-side uniforms are needed, for state GLSL 1.20 cannot read:
 *    u_fogMode   (GL_FOG_MODE)      - -1 off, 0 exp2, 1 linear, 2 exp
 *    u_lightMask (glEnable LIGHTi)  - bit i set = GL_LIGHT0+i enabled
 *
 *  Draw-path compatibility: the shader consumes the built-in attributes
 *  (gl_Vertex/gl_Normal/gl_MultiTexCoord0), so the existing
 *  glVertexPointer/glNormalPointer/glTexCoordPointer client state from the
 *  Tier 2 VBOs feeds it unchanged, and ftransform()/gl_NormalMatrix/
 *  gl_TextureMatrix[0] keep explosion transforms and animated-tread texture
 *  matrices working with no extra plumbing.
 *
 *  Fallback: if compile/link fails, isActive() stays false and callers keep
 *  the fixed-function path -- never breaks the game.
 */

#ifndef BZF_TANK_LIGHTING_SHADER_H
#define BZF_TANK_LIGHTING_SHADER_H

#include "common.h"
#include "bzfgl.h"

class TankLightingShader
{
public:
    static TankLightingShader& instance();

    // compile/link the program. safe to call repeatedly; no-ops after first
    // attempt. returns true if the shader path is active.
    bool init();

    // true if the GLSL path is active (init() succeeded)
    bool isActive() const
    {
        return active;
    }

    // bind (on) or unbind (off) the program. no-op when inactive.
    void useShader(bool on);

    // fog mode mirror (GL_EXP2 / GL_LINEAR / GL_EXP), set when the renderer
    // sets up map fog; pass -1 to disable fog in the shader
    void setFogMode(int mode);

    // bitmask of enabled fixed-function lights (bit i = GL_LIGHT0+i);
    // call after the renderer finishes light setup, once per frame
    void setLightMask(int mask);

    // stage C: bind the sun shadow map for the fragment shader.
    // on=false disables shadow sampling. depthTexture is bound to
    // texture unit 7; sunClipMatrix maps eye space -> sun clip space.
    void setShadowMap(bool on, GLint depthTexture,
                      const GLfloat* sunClipMatrix, int mapSize);

private:
    TankLightingShader();
    ~TankLightingShader();
    TankLightingShader(const TankLightingShader&);
    TankLightingShader& operator=(const TankLightingShader&);

    bool compile(GLuint& shader, GLenum type, const char* source);
    bool link();

    bool active;
    bool triedInit;
    GLuint program;
    GLint unifFogMode;
    GLint unifLightMask;
    GLint unifShadows;
    GLint unifShadowMap;
    GLint unifShadowProj;
    GLint unifShadowTexel;
};

#endif // BZF_TANK_LIGHTING_SHADER_H

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4