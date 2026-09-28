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

// implementation header
#include "MeshDrawMgr.h"

// common headers
#include "bzfgl.h"
#include "OpenGLGState.h"
#include "MeshDrawInfo.h"
#include "BZDBCache.h"
#include "bzfio.h" // for DEBUGx()

// system headers
#include <string.h>


MeshDrawMgr::MeshDrawMgr(const MeshDrawInfo* drawInfo_)
    : drawInfo(drawInfo_), vboVerts(0), vboElem(0), vboFailed(false)
{
    if ((drawInfo == nullptr) || !drawInfo->isValid())
    {
        printf("MeshDrawMgr: invalid drawInfo\n");
        fflush(stdout);
        return;
    }
    else
    {
        logDebugMessage(4,"MeshDrawMgr: initializing\n");
        fflush(stdout);
    }

    auto lodCount = drawInfo->getLodCount();
    lodLists.resize(lodCount);

    // size the VBO command lists to match
    lodGpuSets.resize(lodCount);

    // This pointer is a convenience way to iterate over the DrawLod objects known to the MeshDrawInfo. A first-class
    // iterator would be better
    auto curDrawLod = drawInfo->getDrawLods();

    // size each LodList to the corresponding DrawLod
    for (int lod = 0; lod < lodCount; lod++)
    {
        lodLists[lod].assign(curDrawLod->count, INVALID_GL_LIST_ID);
        lodGpuSets[lod].resize(curDrawLod->count);
        curDrawLod++;
    }

    makeLists();
    makeVBOs();
    OpenGLGState::registerContextInitializer(freeContext, initContext, this);
}


MeshDrawMgr::~MeshDrawMgr()
{
    logDebugMessage(4,"MeshDrawMgr: killing\n");

    OpenGLGState::unregisterContextInitializer(freeContext, initContext, this);
    freeLists();
    freeVBOs();

    return;
}


inline void MeshDrawMgr::rawExecuteCommands(int lod, int set)
{
    auto drawLods = drawInfo->getDrawLods();
    const DrawLod& drawLod = drawLods[lod];
    const DrawSet& drawSet = drawLod.sets[set];
    const int cmdCount = drawSet.count;
    for (int i = 0; i < cmdCount; i++)
    {
        const DrawCmd& cmd = drawSet.cmds[i];
        glDrawElements(cmd.drawMode, cmd.count, cmd.indexType, cmd.indices);
    }
    return;
}


void MeshDrawMgr::executeSet(int lod, int set, bool useNormals, bool useTexcoords)
{
    // FIXME (what is broken?)
    const AnimationInfo* animInfo = drawInfo->getAnimationInfo();
    if (animInfo != nullptr)
    {
        glPushMatrix();
        glRotatef(animInfo->angle, 0.0f, 0.0f, 1.0f);
    }

    if (useVbo() && (vboVerts != 0))
        drawVboSet(lod, set, useNormals, useTexcoords);
    else
    {
        const GLuint list = lodLists[lod][set];
        if (list != INVALID_GL_LIST_ID)
            glCallList(list);
        else
        {
            auto vertices  = reinterpret_cast<GLfloat const*>(drawInfo->getVertices());
            auto normals   = reinterpret_cast<GLfloat const*>(drawInfo->getNormals());
            auto texcoords = reinterpret_cast<GLfloat const*>(drawInfo->getTexcoords());

            glVertexPointer(3, GL_FLOAT, 0, vertices);

            if (useNormals)
                glNormalPointer(GL_FLOAT, 0, normals);
            else
                glDisableClientState(GL_NORMAL_ARRAY);
            if (useTexcoords)
                glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
            else
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);

            rawExecuteCommands(lod, set);

            if (!useNormals)
                glEnableClientState(GL_NORMAL_ARRAY);
            if (!useTexcoords)
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        }
    }

    if (animInfo != nullptr)
        glPopMatrix();

    return;
}


void MeshDrawMgr::executeSetGeometry(int lod, int set)
{
    // FIXME
    const AnimationInfo* animInfo = drawInfo->getAnimationInfo();
    if (animInfo != NULL)
    {
        glPushMatrix();
        glRotatef(animInfo->angle, 0.0f, 0.0f, 1.0f);
    }

    if (useVbo() && (vboVerts != 0))
        drawVboSetGeometry(lod, set);
    else
    {
        const GLuint list = lodLists[lod][set];
        if (list != INVALID_GL_LIST_ID)
            glCallList(list);
        else
        {
            auto vertices = reinterpret_cast<GLfloat const *>(drawInfo->getVertices());

            glVertexPointer(3, GL_FLOAT, 0, vertices);
            rawExecuteCommands(lod, set);
        }
    }

    if (animInfo != NULL)
        glPopMatrix();

    return;
}


void MeshDrawMgr::makeLists()
{
    GLenum error;
    int errCount = 0;
    // reset the error state
    while (true)
    {
        error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        errCount++; // avoid a possible spin-lock?
        if (errCount > 666)
        {
            logDebugMessage(1,"MeshDrawMgr::makeLists() glError: %i\n", error);
            return; // don't make the lists, something is borked
        }
    };

    auto vertices  = reinterpret_cast<GLfloat const*>(drawInfo->getVertices());
    auto normals   = reinterpret_cast<GLfloat const*>(drawInfo->getNormals());
    auto texcoords = reinterpret_cast<GLfloat const*>(drawInfo->getTexcoords());

    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glNormalPointer(GL_FLOAT, 0, normals);
    glEnableClientState(GL_NORMAL_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    auto lod = 0;
    auto curDrawLod = drawInfo->getDrawLods();

    for (auto &item : lodLists)
    {
        const DrawLod& drawLod = *(curDrawLod++);
        for (auto set = 0; set < drawLod.count; set++)
        {
            const DrawSet& drawSet = drawLod.sets[set];
            if (!drawSet.wantList)
                continue;

            item[set] = glGenLists(1);

            glNewList(item[set], GL_COMPILE);
            {
                rawExecuteCommands(lod, set);
            }
            glEndList();

            error = glGetError();
            if (error != GL_NO_ERROR)
            {
                logDebugMessage(1,"MeshDrawMgr::makeLists() %i/%i glError: %i\n",
                                lod, set, error);
                item[set] = INVALID_GL_LIST_ID;
            }
            else
                logDebugMessage(3,"MeshDrawMgr::makeLists() %i/%i created\n", lod, set);
        }
        lod++;
    }

    return;
}


void MeshDrawMgr::freeLists()
{
    for (auto &item : lodLists)
        for (auto &itemSet : item)
            if (itemSet != INVALID_GL_LIST_ID)
            {
                glDeleteLists(itemSet, 1);
                itemSet = INVALID_GL_LIST_ID;
            }

    return;
}


void MeshDrawMgr::initContext(void* data)
{
    ((MeshDrawMgr*)data)->makeLists();
    ((MeshDrawMgr*)data)->makeVBOs();
    return;
}


void MeshDrawMgr::freeContext(void* data)
{
    ((MeshDrawMgr*)data)->freeLists();
    ((MeshDrawMgr*)data)->freeVBOs();
    return;
}


bool MeshDrawMgr::useVbo() const
{
    return BZDBCache::meshVBO && !vboFailed;
}


void MeshDrawMgr::makeVBOs()
{
    if (vboVerts != 0)
        return; // already built

    if ((drawInfo == nullptr) || !drawInfo->isValid())
        return;

    // drain any pending GL errors, same discipline as makeLists()
    GLenum error;
    int errCount = 0;
    while (true)
    {
        error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        errCount++;
        if (errCount > 666)
        {
            logDebugMessage(1,"MeshDrawMgr::makeVBOs() glError: %i\n", error);
            return;
        }
    }

    const int cornerCount = drawInfo->getCornerCount();
    if (cornerCount <= 0)
        return;

    // build the interleaved staging buffer: pos(3) | normal(3) | texcoord(2)
    // per corner, expanded exactly like MeshDrawInfo::clientSetup() does
    const afvec3* verts = drawInfo->getVertices();
    const afvec3* norms = drawInfo->getNormals();
    const afvec2* txcds = drawInfo->getTexcoords();

    std::vector<GLfloat> staging;
    staging.reserve((size_t)cornerCount * 8);
    for (int i = 0; i < cornerCount; i++)
    {
        staging.push_back(verts[i][0]);
        staging.push_back(verts[i][1]);
        staging.push_back(verts[i][2]);
        staging.push_back(norms[i][0]);
        staging.push_back(norms[i][1]);
        staging.push_back(norms[i][2]);
        staging.push_back(txcds[i][0]);
        staging.push_back(txcds[i][1]);
    }

    // build the shared element staging buffer, concatenating every
    // lod/set/cmd index array in rawExecuteCommands() iteration order,
    // and fill the per-(lod,set) GpuCmd lists
    std::vector<uint8_t> elemStaging;
    const auto drawLods = drawInfo->getDrawLods();
    lodGpuSets.resize(drawInfo->getLodCount());
    for (int lod = 0; lod < drawInfo->getLodCount(); lod++)
    {
        const DrawLod& drawLod = drawLods[lod];
        lodGpuSets[lod].resize(drawLod.count);
        for (int set = 0; set < drawLod.count; set++)
        {
            const DrawSet& drawSet = drawLod.sets[set];
            GpuSet& gpuSet = lodGpuSets[lod][set];
            gpuSet.clear();
            gpuSet.reserve(drawSet.count);
            for (int c = 0; c < drawSet.count; c++)
            {
                const DrawCmd& cmd = drawSet.cmds[c];
                if (cmd.count <= 0 || (cmd.indices == nullptr))
                    continue;
                GpuCmd gpuCmd;
                gpuCmd.mode = cmd.drawMode;
                gpuCmd.count = cmd.count;
                gpuCmd.type = cmd.indexType;
                gpuCmd.offset = elemStaging.size();
                const size_t elemSize =
                    (cmd.indexType == DrawCmd::DrawIndexUShort)
                    ? sizeof(unsigned short) : sizeof(unsigned int);
                const uint8_t* src = (const uint8_t*)cmd.indices;
                elemStaging.insert(elemStaging.end(), src, src + (size_t)cmd.count * elemSize);
                gpuSet.push_back(gpuCmd);
            }
        }
    }

    if (staging.empty() || elemStaging.empty())
        return;

    // upload: tank pattern (bzGenBuffers + glBufferData + unbind)
    bzGenBuffers(1, &vboVerts);
    glBindBuffer(GL_ARRAY_BUFFER, vboVerts);
    glBufferData(GL_ARRAY_BUFFER,
                 staging.size() * sizeof(GLfloat),
                 staging.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    bzGenBuffers(1, &vboElem);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboElem);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 elemStaging.size(), elemStaging.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    error = glGetError();
    if (error != GL_NO_ERROR)
    {
        logDebugMessage(1,"MeshDrawMgr::makeVBOs() upload glError: %i\n", error);
        bzDeleteBuffers(1, &vboVerts);
        bzDeleteBuffers(1, &vboElem);
        vboVerts = 0;
        vboElem = 0;
        vboFailed = true;
    }
    else
        logDebugMessage(3,"MeshDrawMgr::makeVBOs() built (%d corners, %d elements)\n",
                        cornerCount, (int)elemStaging.size());

    return;
}


void MeshDrawMgr::freeVBOs()
{
    if (vboVerts != 0)
    {
        bzDeleteBuffers(1, &vboVerts);
        vboVerts = 0;
    }
    if (vboElem != 0)
    {
        bzDeleteBuffers(1, &vboElem);
        vboElem = 0;
    }
    vboFailed = false;
    lodGpuSets.clear();
    return;
}


void MeshDrawMgr::drawVboSet(int lod, int set, bool useNormals, bool useTexcoords)
{
    if ((lod >= (int)lodGpuSets.size()) || (set >= (int)lodGpuSets[lod].size()))
        return;
    const GpuSet& gpuSet = lodGpuSets[lod][set];
    if (gpuSet.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, vboVerts);
    const GLsizei stride = 8 * sizeof(GLfloat);
    const GLbyte* base = NULL;
    glVertexPointer(3, GL_FLOAT, stride, base + 0);
    glNormalPointer(GL_FLOAT, stride, base + 3 * sizeof(GLfloat));
    glTexCoordPointer(2, GL_FLOAT, stride, base + 6 * sizeof(GLfloat));

    // set the enable bits explicitly: GLBatch restores enable bits, not
    // pointers, so never inherit this state from the previous draw
    glEnableClientState(GL_VERTEX_ARRAY);
    if (useNormals)
        glEnableClientState(GL_NORMAL_ARRAY);
    else
        glDisableClientState(GL_NORMAL_ARRAY);
    if (useTexcoords)
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    else
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboElem);
    for (size_t i = 0; i < gpuSet.size(); i++)
    {
        const GpuCmd& cmd = gpuSet[i];
        glDrawElements(cmd.mode, cmd.count, cmd.type, (const GLvoid*)cmd.offset);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    // CRITICAL: unbind both targets; GLBatch sets client pointers from CPU
    // memory and a left-bound GL_ARRAY_BUFFER poisons its pointer captures
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
}


void MeshDrawMgr::drawVboSetGeometry(int lod, int set)
{
    if ((lod >= (int)lodGpuSets.size()) || (set >= (int)lodGpuSets[lod].size()))
        return;
    const GpuSet& gpuSet = lodGpuSets[lod][set];
    if (gpuSet.empty())
        return;

    // vertex-only path (shadow + radar): do NOT touch the normal or
    // texcoord enable bits or pointers -- the radar fast path disables
    // them around renderRadarNodes and re-enables them afterwards
    glBindBuffer(GL_ARRAY_BUFFER, vboVerts);
    glVertexPointer(3, GL_FLOAT, 8 * sizeof(GLfloat), NULL);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboElem);
    for (size_t i = 0; i < gpuSet.size(); i++)
    {
        const GpuCmd& cmd = gpuSet[i];
        glDrawElements(cmd.mode, cmd.count, cmd.type, (const GLvoid*)cmd.offset);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
}


/******************************************************************************/

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
