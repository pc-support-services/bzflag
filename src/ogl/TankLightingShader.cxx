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
#include "TankLightingShader.h"

// system headers
#include <string.h>
#include <stdio.h>

// common headers
#include "bzfio.h"

// ---------------------------------------------------------------- shaders --

// GLSL 1.20, compatibility-profile built-ins: feeds off the fixed-function
// lighting state the renderer already maintains (glLightfv, glMaterialfv,
// glFogf), so there is no CPU-side state mirroring. Per-pixel Blinn-Phong
// + per-pixel fog replaces the per-vertex Gouraud path. gl_LightSource[]
// positions are already in eye space (the renderer executes the sun and
// dynamic lights with the view matrix on the stack).
//
// u_lightMask: bit i set = GL_LIGHT0+i enabled. GLSL 1.20 cannot read
// glEnable(GL_LIGHTi) state, so the CPU passes the mask once per frame.
// u_fogMode: GL_FOG_MODE is likewise unreadable; 0 = exp2, 1 = linear,
// 2 = exp. -1 = fog off.
static const char* vertexShaderSource =
    "#version 120\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_eyePos;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = ftransform();\n"
    "    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
    "    vec4 eye = gl_ModelViewMatrix * gl_Vertex;\n"
    "    v_eyePos = eye.xyz;\n"
    "    v_normal = gl_NormalMatrix * gl_Normal;\n"
    "    // fixed-function color passthrough (per-part color via glColor)\n"
    "    gl_FrontColor = gl_Color;\n"
    "}\n";

static const char* fragmentShaderSource =
    "#version 120\n"
    "uniform int u_fogMode;      // -1 = off, 0 = exp2, 1 = linear, 2 = exp\n"
    "uniform int u_lightMask;    // bit i = GL_LIGHT0+i enabled\n"
    "// sun shadow map (stage C)\n"
    "uniform bool u_shadows;         // shadow mapping active\n"
    "uniform sampler2D u_shadowMap;  // sun-space depth\n"
    "uniform mat4 u_shadowProj;      // eye -> sun clip space\n"
    "uniform vec2 u_shadowTexel;     // 1/shadowMapSize, for PCF offsets\n"
    "varying vec3 v_normal;\n"
    "varying vec3 v_eyePos;\n"
    "float shadowFactor(in vec3 normal, in vec3 L)\n"
    "{\n"
    "    if (!u_shadows)\n"
    "        return 1.0;\n"
    "    // project the fragment into sun clip space; bias along the\n"
    "    // surface normal to lift self-shadowing acne off sloped hulls\n"
    "    vec4 sc = u_shadowProj * vec4(v_eyePos, 1.0);\n"
    "    sc.xyz /= sc.w;\n"
    "    if (sc.x < -1.0 || sc.x > 1.0 || sc.y < -1.0 || sc.y > 1.0 ||\n"
    "        sc.z > 1.0)\n"
    "        return 1.0;     // outside the shadow map = lit\n"
    "    float bias = max(0.0025 * (1.0 - dot(normal, L)), 0.0008);\n"
    "    float depth = sc.z - bias;\n"
    "    // 4-tap PCF to soften the edge\n"
    "    float sum = 0.0;\n"
    "    for (int y = -1; y <= 1; y += 2)\n"
    "    {\n"
    "        for (int x = -1; x <= 1; x += 2)\n"
    "        {\n"
    "            float d = texture2D(u_shadowMap,\n"
    "                                sc.xy * 0.5 + 0.5 +\n"
    "                                vec2(x, y) * u_shadowTexel).r;\n"
    "            sum += (depth <= d) ? 1.0 : 0.0;\n"
    "        }\n"
    "    }\n"
    "    return sum * 0.25;\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    vec3 normal = normalize(v_normal);\n"
    "    if (!gl_FrontFacing)\n"
    "        normal = -normal;   // two-sided like the fixed pipeline\n"
    "    vec4 baseColor = gl_Color;\n"
    "    baseColor *= texture2D(gl_Texture[0], gl_TexCoord[0]);\n"
    "\n"
    "    vec3 viewDir = normalize(-v_eyePos);\n"
    "    vec3 lit = gl_LightModel.ambient.rgb * baseColor.rgb;\n"
    "    lit += gl_FrontMaterial.emission.rgb;\n"
    "\n"
    "    // per-light diffuse + specular (sun/moon is light 0)\n"
    "    for (int i = 0; i < 8; i++)\n"
    "    {\n"
    "        if ((u_lightMask & (1 << i)) == 0)\n"
    "            continue;\n"
    "        vec3 L;\n"
    "        float atten = 1.0;\n"
    "        if (gl_LightSource[i].position.w == 0.0)\n"
    "            L = normalize(gl_LightSource[i].position.xyz);\n"
    "        else\n"
    "        {\n"
    "            vec3 delta = gl_LightSource[i].position.xyz - v_eyePos;\n"
    "            float dist = length(delta);\n"
    "            L = delta / max(dist, 1.0e-6);\n"
    "            atten = 1.0 / (gl_LightSource[i].constantAttenuation\n"
    "                           + gl_LightSource[i].linearAttenuation * dist\n"
    "                           + gl_LightSource[i].quadraticAttenuation * dist * dist);\n"
    "        }\n"
    "        float ndotl = max(dot(normal, L), 0.0);\n"
    "        if (ndotl > 0.0)\n"
    "        {\n"
    "            // sun (light 0) picks up the shadow map factor\n"
    "            float shadow = (i == 0) ? shadowFactor(normal, L) : 1.0;\n"
    "            lit += gl_LightSource[i].diffuse.rgb * baseColor.rgb * ndotl * atten * shadow;\n"
    "            vec3 H = normalize(L + viewDir);\n"
    "            float spec = pow(max(dot(normal, H), 0.0), gl_FrontMaterial.shininess);\n"
    "            lit += gl_LightSource[i].specular.rgb *\n"
    "                   gl_FrontMaterial.specular.rgb * spec * atten * shadow;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    vec4 frag = vec4(lit, baseColor.a);\n"
    "\n"
    "    // per-pixel fog (mirror of the fixed-function fog setup)\n"
    "    if (u_fogMode >= 0)\n"
    "    {\n"
    "        float depth = length(v_eyePos);\n"
    "        float f;\n"
    "        if (u_fogMode == 0)\n"
    "            f = 1.0 - exp(-gl_Fog.density * gl_Fog.density * depth * depth);\n"
    "        else if (u_fogMode == 2)\n"
    "            f = 1.0 - exp(-gl_Fog.density * depth);\n"
    "        else\n"
    "            f = (gl_Fog.end - depth) / (gl_Fog.end - gl_Fog.start);\n"
    "        frag.rgb = mix(frag.rgb, gl_Fog.color.rgb, clamp(f, 0.0, 1.0));\n"
    "    }\n"
    "\n"
    "    gl_FragColor = frag;\n"
    "}\n";

// ---------------------------------------------------------------- impl -----

TankLightingShader::TankLightingShader()
    : active(false), triedInit(false), program(0),
      unifFogMode(-1), unifLightMask(-1)
{
}

TankLightingShader::~TankLightingShader()
{
    if (program != 0)
        glDeleteProgram(program);
}

TankLightingShader& TankLightingShader::instance()
{
    static TankLightingShader theShader;
    return theShader;
}

bool TankLightingShader::compile(GLuint& shader, GLenum type, const char* source)
{
    shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE)
    {
        char log[2048];
        GLsizei len = 0;
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        logDebugMessage(1, "TankLightingShader: %s compile failed: %s\n",
                        (type == GL_VERTEX_SHADER) ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        shader = 0;
        return false;
    }
    return true;
}

bool TankLightingShader::link()
{
    GLuint vs = 0, fs = 0;
    if (!compile(vs, GL_VERTEX_SHADER, vertexShaderSource))
        return false;
    if (!compile(fs, GL_FRAGMENT_SHADER, fragmentShaderSource))
    {
        glDeleteShader(vs);
        return false;
    }

    program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (ok != GL_TRUE)
    {
        char log[2048];
        GLsizei len = 0;
        glGetProgramInfoLog(program, sizeof(log), &len, log);
        logDebugMessage(1, "TankLightingShader: link failed: %s\n", log);
        glDeleteProgram(program);
        program = 0;
        return false;
    }
    return true;
}

bool TankLightingShader::init()
{
    if (triedInit)
        return active;
    triedInit = true;

    if (!GLEW_VERSION_2_0)
    {
        logDebugMessage(1, "TankLightingShader: GL 2.0 not available, "
                        "staying on fixed-function\n");
        return false;
    }

    if (!link())
        return false;

    unifFogMode   = glGetUniformLocation(program, "u_fogMode");
    unifLightMask = glGetUniformLocation(program, "u_lightMask");
    unifShadows   = glGetUniformLocation(program, "u_shadows");
    unifShadowMap = glGetUniformLocation(program, "u_shadowMap");
    unifShadowProj  = glGetUniformLocation(program, "u_shadowProj");
    unifShadowTexel = glGetUniformLocation(program, "u_shadowTexel");

    logDebugMessage(2, "TankLightingShader: GLSL per-pixel tank lighting active\n");
    active = true;
    return true;
}

void TankLightingShader::useShader(bool on)
{
    if (!active)
        return;
    glUseProgram(on ? program : 0);
}

void TankLightingShader::setFogMode(int mode)
{
    if (!active)
        return;
    glUseProgram(program);
    glUniform1i(unifFogMode, mode);
    glUseProgram(0);
}

void TankLightingShader::setLightMask(int mask)
{
    if (!active)
        return;
    glUseProgram(program);
    glUniform1i(unifLightMask, mask);
    glUseProgram(0);
}

// stage C: bind the sun shadow map. called once per frame after the
// shadow FBO pass, before tanks are drawn.
void TankLightingShader::setShadowMap(bool on, GLint depthTexture,
                                      const GLfloat* sunClipMatrix,
                                      int mapSize)
{
    if (!active)
        return;
    glUseProgram(program);
    glUniform1i(unifShadows, on ? 1 : 0);
    if (on && (depthTexture != 0))
    {
        // depth map on texture unit 7 (units 0..N are in use by materials)
        glUniform1i(unifShadowMap, 7);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, (GLuint)depthTexture);
        glActiveTexture(GL_TEXTURE0);
        glUniformMatrix4fv(unifShadowProj, 1, GL_FALSE, sunClipMatrix);
        glUniform2f(unifShadowTexel, 1.0f / (GLfloat)mapSize,
                    1.0f / (GLfloat)mapSize);
    }
    glUseProgram(0);
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4