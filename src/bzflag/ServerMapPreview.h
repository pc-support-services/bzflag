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
 * ServerMapPreview:
 *  Shows a top-down outline of the map on the selected server before
 *  joining.  Uses the BZFS protocol handshake (same as
 *  ServerQueryPlayers): TCP connect + BZFLAG header, then MsgWantWHash
 *  ('wh').  The server replies with an optional MsgCacheURL and the MD5
 *  digest of the world database.  If the world is already in the local
 *  world cache (the <digest>.bwc file every join writes there) it is
 *  loaded straight away; otherwise it is pulled from the cache URL or
 *  streamed chunk-by-chunk via MsgGetWorld ('gw') -- both allowed
 *  pre-Enter.  The unpacked obstacle lists (walls, boxes, pyramids,
 *  bases, teleporters, mesh faces) are then drawn top-down by
 *  HUDuiMapPreview in the ServerMenu.
 */

#ifndef __SERVERMAPPREVIEW_H__
#define __SERVERMAPPREVIEW_H__

// system interface headers
#include <string>
#include <vector>
#include <map>

// common interface headers
#include "TimeKeeper.h"
#include "Address.h"


class ServerMapPreview
{
public:
    static ServerMapPreview& instance();

    // kick off an async preview fetch for this server ("host:port");
    // throttled and ignored while a fetch is already running
    void queryServer(const std::string& addrName);

    // drop any pending fetch (server selection changed)
    void cancel();

    // pump the in-progress fetch; call from the menu callback each frame
    void pump();

    // true while a fetch is running
    bool isActive() const
    {
        return active;
    }

    // render states for the menu readout
    enum State
    {
        None = 0,       // nothing cached, nothing running
        Busy,           // fetching hash / world
        Ready,          // world loaded, outline available
        Failed          // server did not deliver a usable world
    };

    State getState(const std::string& addrName) const;

    // obstacle footprints extracted from the world database
    struct Quad
    {
        float x[4], y[4];   // corners, counter-clockwise
        float color[3];
    };

    // outline data for rendering (empty until Ready)
    const std::vector<Quad>& getQuads() const
    {
        return quads;
    }

    // world bounds for scaling the preview
    void getBounds(float& minX, float& maxX, float& minY, float& maxY) const
    {
        minX = boundMinX;
        maxX = boundMaxX;
        minY = boundMinY;
        maxY = boundMaxY;
    }

private:
    ServerMapPreview();
    ~ServerMapPreview();

    void finish(bool failed);
    void gotHash(const std::string& digest);
    bool checkCache(const std::string& digest);
    bool loadWorld(const char* data, unsigned int length);
    void extractOutlines();

    enum Phase
    {
        Idle = 0,
        Connecting,     // non-blocking connect in progress
        ConnectHeader,  // waiting for BZFS#### + my id
        WaitHash,       // waiting for MsgWantWHash reply
        WaitChunks      // collecting MsgGetWorld chunks
    };

    int         fd;
    Phase       phase;
    std::string queryingAddr;   // "host:port" being fetched
    Address     serverAddress;
    int         serverPort;

    std::string cacheURL;       // from MsgCacheURL, may be empty
    std::string digest;         // server hex digest ('t'/'p' + md5)

    std::vector<char> inBuf;    // persistent TCP read buffer
    bool        wantHandshake;  // still consuming version/id bytes

    // world download state
    std::vector<char> worldData;    // accumulated world database
    uint32_t    worldPtr;           // bytes received so far
    uint32_t    worldTotal;         // total size (from first chunk)
    bool        gotFirstChunk;

    // unpacked outlines
    std::vector<Quad> quads;
    float       boundMinX, boundMaxX, boundMinY, boundMaxY;

    // per-server state cache: "host:port" -> state
    std::map<std::string, State> serverState;
    std::map<std::string, std::string> serverDigest; // addr -> digest

    TimeKeeper  lastFetch;
    TimeKeeper  startTime;

    bool        active;
    bool        usedNetwork;    // last fetch pulled over the network
};

#endif //__SERVERMAPPREVIEW_H__

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4