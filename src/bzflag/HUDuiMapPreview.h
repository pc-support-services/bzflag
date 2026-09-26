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

/*
 * HUDuiMapPreview:
 *  HUDui control that draws a top-down outline of the selected server's
 *  map.  Data comes from ServerMapPreview (world database fetched from
 *  the server or the local world cache).  Positioned by ServerMenu's
 *  resize() in the right-hand column beside the server list.
 */

#ifndef __HUDUIMAPPREVIEW_H__
#define __HUDUIMAPPREVIEW_H__

// ancestor class
#include "HUDuiControl.h"


class HUDuiMapPreview final : public HUDuiControl
{
public:
    HUDuiMapPreview();
    ~HUDuiMapPreview();

    // render placeholder text while the fetch runs
    void setHint(const std::string& hint);

protected:
    void        onSetFont() override;
    bool        doKeyPress(const BzfKeyEvent&) override;
    bool        doKeyRelease(const BzfKeyEvent&) override;
    void        doRender() override;

private:
    std::string hint;
};

#endif //__HUDUIMAPPREVIEW_H__

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4