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

#pragma once

// 1st
#include "common.h"

// System headers
#include <vector>

// common interface headers
#include "bzfgl.h"
#include "MeshDrawInfo.h"

class MeshDrawMgr
{
public:
    MeshDrawMgr(const MeshDrawInfo* drawInfo);
    ~MeshDrawMgr();

    void executeSet(int lod, int set, bool useNormals, bool useTexcoords);
    void executeSetGeometry(int lod, int set);

private:
    void rawExecuteCommands(int lod, int set);

    void makeLists();
    void freeLists();

    // VBO path: one interleaved vertex buffer + one shared element
    // buffer per source drawInfo, replacing per-frame client-array
    // setup and per-set display lists (see the tank port pattern in
    // TankGeometryMgr for the state-discipline rules)
    void makeVBOs();
    void freeVBOs();
    void drawVboSet(int lod, int set, bool useNormals, bool useTexcoords);
    void drawVboSetGeometry(int lod, int set);
    bool useVbo() const;

    static void initContext(void* data);
    static void freeContext(void* data);

    // one draw-elements command referencing the shared element VBO
    struct GpuCmd
    {
        GLenum mode;        // from DrawCmd::drawMode
        GLsizei count;      // from DrawCmd::count
        GLenum type;        // from DrawCmd::indexType (ushort/uint)
        uintptr_t offset;   // byte offset into the shared element VBO
    };
    // per (lod,set) GPU command list, parallel to DrawLod/DrawSet
    using GpuSet = std::vector<GpuCmd>;

private:
    const MeshDrawInfo* drawInfo;

    using LodList = std::vector<int>;
    std::vector<LodList> lodLists;

    // VBO state: 0 = not built; vboFailed latches upload errors so a
    // broken path does not retry every frame
    GLuint vboVerts;
    GLuint vboElem;
    bool vboFailed;
    std::vector< std::vector<GpuSet> > lodGpuSets;  // [lod][set] -> cmds
};


// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4