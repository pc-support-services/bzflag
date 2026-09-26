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
#include "HUDuiMapPreview.h"

// common implementation headers
#include "bzfgl.h"
#include "GLBatch.h"
#include "FontManager.h"
#include "OpenGLGState.h"
#include "ServerMapPreview.h"
#include "TextUtils.h"

// system implementation headers
#include <math.h>
#include <string>


HUDuiMapPreview::HUDuiMapPreview()
{
}

HUDuiMapPreview::~HUDuiMapPreview()
{
}

void HUDuiMapPreview::setHint(const std::string& _hint)
{
    hint = _hint;
}

void HUDuiMapPreview::onSetFont()
{
    // nothing extra
}

bool HUDuiMapPreview::doKeyPress(const BzfKeyEvent&)
{
    return false; // not interactive
}

bool HUDuiMapPreview::doKeyRelease(const BzfKeyEvent&)
{
    return false;
}

void HUDuiMapPreview::doRender()
{
    const float x = getX();
    const float y = getY();

    // panel area: width/height set by resize(); defaults sane
    const float w = getWidth();
    const float h = getHeight();

    ServerMapPreview& smp = ServerMapPreview::instance();

    if (smp.getQuads().empty())
    {
        // nothing yet: show the hint (or nothing)
        if (!hint.empty() && getFontFace() >= 0)
        {
            FontManager &fm = FontManager::instance();
            glColor3f(0.7f, 0.7f, 0.7f);
            fm.drawString(x + w * 0.5f -
                          fm.getStrLength(getFontFace(), getFontSize(),
                                          stripAnsiCodes(hint)) * 0.5f,
                          y + h * 0.5f, 0, getFontFace(), getFontSize(), hint);
        }
        return;
    }

    // fit the world bounds into the panel, centered
    float minX, maxX, minY, maxY;
    smp.getBounds(minX, maxX, minY, maxY);
    const float worldW = (maxX - minX) > 1.0f ? (maxX - minX) : 1.0f;
    const float worldH = (maxY - minY) > 1.0f ? (maxY - minY) : 1.0f;
    const float pad = 0.06f;
    const float scale = ((w * (1.0f - 2.0f * pad)) / worldW < (h * (1.0f - 2.0f * pad)) / worldH)
                        ? (w * (1.0f - 2.0f * pad)) / worldW
                        : (h * (1.0f - 2.0f * pad)) / worldH;
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;

    // world -> panel: x' = cx + (wx - midX) * scale, y' = cy + (wy - midY) * scale
    const float midX = 0.5f * (minX + maxX);
    const float midY = 0.5f * (minY + maxY);

    // dim panel backdrop so the outline reads over the menu background
    OpenGLGState::resetState();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.35f);
    {
        static GLBatch batch;
        batch.begin(GL_TRIANGLE_FAN);
        batch.vertex2f(x, y);
        batch.vertex2f(x + w, y);
        batch.vertex2f(x + w, y + h);
        batch.vertex2f(x, y + h);
        batch.end();
    }

    glLineWidth(1.0f);
    {
        static GLBatch batch;
        const std::vector<ServerMapPreview::Quad>& quads = smp.getQuads();
        // one batch per quad would be slow; batch all same-color quads
        // as line loops one after another (batch flushes at end())
        for (size_t i = 0; i < quads.size(); i++)
        {
            const ServerMapPreview::Quad& q = quads[i];
            glColor3fv(q.color);
            batch.begin(GL_LINE_LOOP);
            for (int v = 0; v < 4; v++)
                batch.vertex2f(cx + (q.x[v] - midX) * scale,
                               cy + (q.y[v] - midY) * scale);
            batch.end();
        }
    }

    // restore state the dialog render path expects
    OpenGLGState::resetState();
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4