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
 *  Asks the selected game server for its world database before joining
 *  (BZFS protocol handshake: TCP connect, "BZFLAG\r\n\r\n" header,
 *  server replies 8-byte version + 1-byte my-id, then MsgWantWHash
 *  ('wh')).  The server replies with an optional MsgCacheURL ('cu')
 *  followed by MsgWantWHash carrying a start byte plus the world MD5.
 *  The world database is then taken from the local world cache (the
 *  digest .bwc file every join writes there) when present, else
 *  streamed via MsgGetWorld ('gw') chunks -- a pre-Enter message the
 *  server accepts.  The uncompressed world database is unpacked with
 *  WorldBuilder and reduced to top-down obstacle outlines for
 *  HUDuiMapPreview.
 */

// interface header
#include "ServerMapPreview.h"

// common implementation headers
#include "Protocol.h"
#include "Pack.h"
#include "version.h"
#include "network.h"
#include "DirectoryNames.h"
#include "FileManager.h"
#include "md5.h"
#include "World.h"
#include "WorldBuilder.h"
#include "ObstacleMgr.h"
#include "ObstacleList.h"
#include "WallObstacle.h"
#include "BoxBuilding.h"
#include "PyramidBuilding.h"
#include "BaseBuilding.h"
#include "Teleporter.h"
#include "MeshObstacle.h"
#include "MeshFace.h"
#include "Team.h"
#include "StateDatabase.h"

// system implementation headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <map>

// minimum seconds between any two fetches for the same/another server
static const float MinRetryDelay = 2.0f;
// give up on a slow server after this many seconds (hash phase)
static const float FetchTimeout = 10.0f;
// world download gets longer, per-100kb
static const float ChunkTimeoutFactor = 10.0f;

ServerMapPreview::ServerMapPreview()
    : fd(-1), phase(Idle), serverPort(ServerPort),
      worldPtr(0), worldTotal(0), gotFirstChunk(false),
      boundMinX(0.0f), boundMaxX(0.0f), boundMinY(0.0f), boundMaxY(0.0f),
      lastFetch(), startTime(), active(false), usedNetwork(false)
{
}

ServerMapPreview::~ServerMapPreview()
{
    if (fd >= 0)
        close(fd);
}

ServerMapPreview& ServerMapPreview::instance()
{
    static ServerMapPreview theInstance;
    return theInstance;
}

void ServerMapPreview::queryServer(const std::string& addrName)
{
    if (addrName.empty() || active)
        return;

    // already have this one?
    std::map<std::string, State>::const_iterator it = serverState.find(addrName);
    if (it != serverState.end() && it->second == Ready)
        return;

    const double since = TimeKeeper::getCurrent() - lastFetch;
    if (since < (double)MinRetryDelay)
        return; // never hammer when paging through the list
    lastFetch = TimeKeeper::getCurrent();

    // if we already know the digest for this server, try the cache
    // without any network traffic at all
    std::map<std::string, std::string>::const_iterator dit = serverDigest.find(addrName);
    if (dit != serverDigest.end() && checkCache(dit->second))
    {
        serverState[addrName] = Ready;
        usedNetwork = false;
        return;
    }

    // split "host:port"
    std::string host = addrName;
    int port = ServerPort;
    const std::string::size_type colon = host.find_last_of(':');
    if (colon != std::string::npos)
    {
        port = atoi(host.substr(colon + 1).c_str());
        if (port <= 0)
            port = ServerPort;
        host = host.substr(0, colon);
    }

    // resolve the host (blocking DNS, normally cached by the list flow)
    const Address addr = Address::getHostAddress(host);
    if (addr.isAny())
        return;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return;
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((unsigned short)port);
    saddr.sin_addr = (InAddr)addr;

    // non-blocking connect
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (connect(fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0 &&
            errno != EINPROGRESS)
    {
        close(fd);
        fd = -1;
        serverState[addrName] = Failed;
        return;
    }

    // connection is being established (or already is, on loopback):
    // defer the header send + handshake parsing to pump() so a connect
    // still in flight can complete.  sending here would fail with
    // ENOTCONN/EALREADY on a real (non-loopback) server and kill the
    // fetch before it started.
    queryingAddr = addrName;
    serverAddress = addr;
    serverPort = port;
    cacheURL = "";
    digest = "";
    inBuf.clear();
    worldData.clear();
    worldPtr = 0;
    worldTotal = 0;
    gotFirstChunk = false;
    wantHandshake = true;
    phase = Connecting;
    startTime = TimeKeeper::getCurrent();
    active = true;
    usedNetwork = true;
    serverState[addrName] = Busy;
}

void ServerMapPreview::cancel()
{
    if (active)
        finish(false);
}

ServerMapPreview::State ServerMapPreview::getState(const std::string& addrName) const
{
    std::map<std::string, State>::const_iterator it = serverState.find(addrName);
    if (it == serverState.end())
        return None;
    return it->second;
}

// pull all available stream bytes into inBuf (non-blocking)
static void drainStream(int fd, std::vector<char>& inBuf)
{
    char tmp[4096];
    while (true)
    {
        const int n = recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0)
            return; // EAGAIN / error / hangup; caller times out on error
        inBuf.insert(inBuf.end(), tmp, tmp + n);
    }
}

// try to peel `want` bytes off the front of inBuf
static bool takeBytes(std::vector<char>& inBuf, void* out, size_t want)
{
    if (inBuf.size() < want)
        return false;
    memcpy(out, &inBuf[0], want);
    inBuf.erase(inBuf.begin(), inBuf.begin() + (long)want);
    return true;
}

// skip `want` bytes from the front of inBuf
static bool skipBytes(std::vector<char>& inBuf, size_t want)
{
    if (inBuf.size() < want)
        return false;
    inBuf.erase(inBuf.begin(), inBuf.begin() + (long)want);
    return true;
}

void ServerMapPreview::pump()
{
    if (!active || fd < 0)
        return;

    // timeout (longer once we are pulling a world)
    float timeout = FetchTimeout;
    if (phase == WaitChunks && gotFirstChunk)
        timeout += ChunkTimeoutFactor * (0.001f * (float)worldTotal / 1024.0f);
    if ((TimeKeeper::getCurrent() - startTime) > (double)timeout)
    {
        finish(true);
        return;
    }

    // connect in flight: poll it.  once writable, check for connect
    // error (refused/banned/unreachable) before sending the header.
    if (phase == Connecting)
    {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        const int pr = poll(&pfd, 1, 0);
        if (pr == 0)
            return; // still connecting; try again next frame
        int err = 0;
        socklen_t errLen = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errLen);
        if (err != 0 || pr < 0)
        {
            finish(true);
            return;
        }
        // connected: send the connect header (server replies with version)
        if (send(fd, BZ_CONNECT_HEADER, (int)strlen(BZ_CONNECT_HEADER), 0) !=
                (int)strlen(BZ_CONNECT_HEADER))
        {
            finish(true);
            return;
        }
        phase = ConnectHeader;
        return; // handshake handled in later pumps
    }

    // pull any fresh stream bytes
    drainStream(fd, inBuf);

    // initial handshake: 8-byte version then 1-byte my id
    if (wantHandshake)
    {
        char version[9];
        if (!takeBytes(inBuf, version, 8))
            return;
        version[8] = '\0';
        if (strncmp(version, "BZFS", 4) != 0)
        {
            finish(true);
            return;
        }
        char idByte;
        if (!takeBytes(inBuf, &idByte, 1))
            return;
        wantHandshake = false;

        // handshake complete: ask for the world hash
        char qbuf[4];
        void* buf = qbuf;
        buf = nboPackUShort(buf, 0);
        buf = nboPackUShort(buf, MsgWantWHash);
        if (send(fd, qbuf, sizeof(qbuf), 0) != sizeof(qbuf))
        {
            finish(true);
            return;
        }
        phase = WaitHash;
        return; // reply handled in later pumps
    }

    // packet loop: process everything buffered
    while (true)
    {
        char hdr[4];
        if (!takeBytes(inBuf, hdr, 4))
            return;
        uint16_t len, code;
        const void* buf = hdr;
        buf = nboUnpackUShort(buf, len);
        buf = nboUnpackUShort(buf, code);

        if (code == MsgSuperKill || code == MsgReject)
        {
            finish(true);
            return;
        }

        if (phase == WaitHash)
        {
            if (code == MsgCacheURL)
            {
                // cache URL string (nul-terminated packed string)
                static std::vector<char> urlBody;
                urlBody.resize(len);
                if (!takeBytes(inBuf, &urlBody[0], len))
                    return; // body split across TCP segments; wait
                cacheURL = &urlBody[0];
                continue;
            }
            if (code != MsgWantWHash)
            {
                // unexpected packet - skip body
                if (len > 0 && !skipBytes(inBuf, len))
                    return;
                continue;
            }
            // digest string: 't' or 'p' + 32 hex chars + nul
            static std::vector<char> hashBody;
            hashBody.resize(len);
            if (!takeBytes(inBuf, &hashBody[0], len))
                return; // body split; wait for more stream bytes
            hashBody[len - 1] = '\0';
            digest = &hashBody[0];
            gotHash(digest);
            return;
        }

        if (phase == WaitChunks)
        {
            if (code != MsgGetWorld)
            {
                // unexpected packet - skip body
                if (len > 0 && !skipBytes(inBuf, len))
                    return;
                continue;
            }
            // body: uint32 bytesLeft + chunk data
            if (len < 4)
            {
                finish(true);
                return;
            }
            const size_t chunkLen = (size_t)len - 4;
            const size_t need = worldData.size() + 4 + chunkLen;
            // accumulate the whole packet in one contiguous buffer
            static std::vector<char> body;
            body.resize(len);
            if (!takeBytes(inBuf, &body[0], len))
                return; // body split across TCP segments; wait
            uint32_t bytesLeft;
            const void* bbuf = &body[0];
            bbuf = nboUnpackUInt(bbuf, bytesLeft);
            if (!gotFirstChunk)
            {
                // total size = received so far + still to come
                worldTotal = 4 + chunkLen + bytesLeft;
                worldData.reserve(worldTotal + 64);
                gotFirstChunk = true;
            }
            worldData.insert(worldData.end(), &body[4], &body[4] + chunkLen);
            worldPtr = (uint32_t)worldData.size();
            if (bytesLeft == 0)
            {
                // complete: verify and unpack
                if (!loadWorld(&worldData[0], (unsigned int)worldData.size()))
                {
                    finish(true);
                    return;
                }
                serverState[queryingAddr] = Ready;
                finish(false);
                return;
            }
            // ask for the next chunk
            char nbuf[8];
            void* n = nbuf;
            n = nboPackUShort(n, 4);
            n = nboPackUShort(n, MsgGetWorld);
            void* nb = nbuf + 4;
            nb = nboPackUInt(nb, worldPtr);
            if (send(fd, nbuf, sizeof(nbuf), 0) != sizeof(nbuf))
            {
                finish(true);
                return;
            }
            continue;
        }

        // unknown phase
        finish(true);
        return;
    }
}

void ServerMapPreview::finish(bool failed)
{
    if (fd >= 0)
        close(fd);
    fd = -1;
    phase = Idle;
    active = false;
    if (failed && !queryingAddr.empty())
        serverState[queryingAddr] = Failed;
}

void ServerMapPreview::gotHash(const std::string& _digest)
{
    digest = _digest;
    serverDigest[queryingAddr] = digest;

    // local cache hit: no network world download needed
    if (checkCache(digest))
    {
        serverState[queryingAddr] = Ready;
        finish(false);
        return;
    }

    // no cache: start the world download (cache URL if given, else chunks)
    worldData.clear();
    worldPtr = 0;
    worldTotal = 0;
    gotFirstChunk = false;
    startTime = TimeKeeper::getCurrent(); // restart timeout for download

    if (!cacheURL.empty())
    {
        // the client's own curl world downloader handles progress + cache
        // writes, but it is wired to the joining flow; a plain threaded
        // fetch here is overkill -- just fall back to MsgGetWorld chunks,
        // which every server supports.
        cacheURL = "";
    }

    // request the first chunk
    char qbuf[8];
    void* buf = qbuf;
    buf = nboPackUShort(buf, 4);
    buf = nboPackUShort(buf, MsgGetWorld);
    buf = nboPackUInt(buf, 0);
    if (send(fd, qbuf, sizeof(qbuf), 0) != sizeof(qbuf))
    {
        finish(true);
        return;
    }
    phase = WaitChunks;
}

bool ServerMapPreview::checkCache(const std::string& _digest)
{
    if (_digest.size() < 2)
        return false;
    const std::string path = getCacheDirName() + _digest + ".bwc";
    std::istream* cached = FILEMGR.createDataInStream(path, true);
    if (!cached)
        return false;

    // get the world size
    cached->seekg(0, std::ios::end);
    const std::streampos size = cached->tellg();
    const unsigned long charSize = (unsigned long)std::streamoff(size);
    cached->seekg(0);

    // integrity: the file must hash to the digest we were given
    char* data = new char[charSize];
    cached->read(data, charSize);
    delete cached;
    MD5 md5;
    md5.update((unsigned char*)data, charSize);
    md5.finalize();
    if (md5.hexdigest() != _digest.substr(1))
    {
        delete[] data;
        remove(path.c_str());
        return false;
    }

    const bool ok = loadWorld(data, (unsigned int)charSize);
    delete[] data;
    return ok;
}

bool ServerMapPreview::loadWorld(const char* data, unsigned int length)
{
    // verify MD5 against the digest from the server (when we have one;
    // cache loads always have one)
    if (!digest.empty())
    {
        MD5 md5;
        md5.update((unsigned char*)data, length);
        md5.finalize();
        if (md5.hexdigest() != digest.substr(1))
            return false;
    }

    // unpack into a throwaway World; this populates OBSTACLEMGR
    WorldBuilder* builder = new WorldBuilder();
    if (!builder->unpack(data))
    {
        delete builder;
        return false;
    }
    World* world = builder->getWorld();
    delete builder;

    extractOutlines();

    // tear the world down again (destructor clears the managers)
    delete world;

    return true;
}

void ServerMapPreview::extractOutlines()
{
    quads.clear();
    boundMinX = boundMinY = 1e30f;
    boundMaxX = boundMaxY = -1e30f;

    // helper lambda-ish: add one rotated rectangle footprint
    struct Adder
    {
        static void addRect(std::vector<Quad>& quads,
                            const float* pos, float rot,
                            float w, float b, const float* color)
        {
            Quad q;
            const float c = cosf(rot), s = sinf(rot);
            const float wx = c * w, wy = s * w;
            const float hx = -s * b, hy = c * b;
            q.x[0] = pos[0] - wx - hx; q.y[0] = pos[1] - wy - hy;
            q.x[1] = pos[0] + wx - hx; q.y[1] = pos[1] + wy - hy;
            q.x[2] = pos[0] + wx + hx; q.y[2] = pos[1] + wy + hy;
            q.x[3] = pos[0] - wx + hx; q.y[3] = pos[1] - wy + hy;
            q.color[0] = color[0]; q.color[1] = color[1]; q.color[2] = color[2];
            quads.push_back(q);
        }
    };

    static const float boxColor[3]   = { 0.55f, 0.75f, 0.75f };
    static const float pyrColor[3]   = { 0.45f, 0.65f, 0.45f };
    static const float wallColor[3]  = { 0.35f, 0.55f, 0.55f };
    static const float teleColor[3]  = { 1.0f, 0.9f, 0.3f };

    // walls: thin lines -> draw as thin quads
    const ObstacleList& walls = OBSTACLEMGR.getWalls();
    for (int i = 0; i < (int)walls.size(); i++)
    {
        const WallObstacle& wall = *((const WallObstacle*)walls[i]);
        Adder::addRect(quads, wall.getPosition(), wall.getRotation(),
                       wall.getBreadth(), wall.getBreadth() * 0.0f + 0.5f,
                       wallColor);
    }

    // boxes
    const ObstacleList& boxes = OBSTACLEMGR.getBoxes();
    for (int i = 0; i < (int)boxes.size(); i++)
    {
        const BoxBuilding& box = *((const BoxBuilding*)boxes[i]);
        if (box.isInvisible())
            continue;
        Adder::addRect(quads, box.getPosition(), box.getRotation(),
                       box.getWidth(), box.getBreadth(), boxColor);
    }

    // pyramids
    const ObstacleList& pyrs = OBSTACLEMGR.getPyrs();
    for (int i = 0; i < (int)pyrs.size(); i++)
    {
        const PyramidBuilding& pyr = *((const PyramidBuilding*)pyrs[i]);
        Adder::addRect(quads, pyr.getPosition(), pyr.getRotation(),
                       pyr.getWidth(), pyr.getBreadth(), pyrColor);
    }

    // team bases, team colored
    const ObstacleList& bases = OBSTACLEMGR.getBases();
    for (int i = 0; i < (int)bases.size(); i++)
    {
        const BaseBuilding& base = *((const BaseBuilding*)bases[i]);
        Adder::addRect(quads, base.getPosition(), base.getRotation(),
                       base.getWidth(), base.getBreadth(),
                       Team::getRadarColor((TeamColor)base.getTeam()));
    }

    // teleporters
    const ObstacleList& teles = OBSTACLEMGR.getTeles();
    for (int i = 0; i < (int)teles.size(); i++)
    {
        const Teleporter& tele = *((const Teleporter*)teles[i]);
        Adder::addRect(quads, tele.getPosition(), tele.getRotation(),
                       tele.getWidth(), tele.getBreadth(), teleColor);
    }

    // mesh faces (top-down), dim white
    const ObstacleList& meshes = OBSTACLEMGR.getMeshes();
    for (int i = 0; i < (int)meshes.size(); i++)
    {
        const MeshObstacle* mesh = (const MeshObstacle*)meshes[i];
        const int faces = mesh->getFaceCount();
        for (int f = 0; f < faces; f++)
        {
            const MeshFace* face = mesh->getFace(f);
            if (face->getPlane()[2] <= 0.0f)
                continue; // vertical or downward faces
            const int vc = face->getVertexCount();
            if (vc < 3 || vc > 4)
                continue;
            Quad q;
            static const float meshColor[3] = { 0.6f, 0.6f, 0.6f };
            q.color[0] = meshColor[0]; q.color[1] = meshColor[1]; q.color[2] = meshColor[2];
            for (int v = 0; v < 4; v++)
            {
                const int vi = (v < vc) ? v : vc - 1;
                const float* pos = face->getVertex(vi);
                q.x[v] = pos[0];
                q.y[v] = pos[1];
            }
            quads.push_back(q);
        }
    }

    // compute bounds
    for (size_t i = 0; i < quads.size(); i++)
    {
        for (int v = 0; v < 4; v++)
        {
            if (quads[i].x[v] < boundMinX) boundMinX = quads[i].x[v];
            if (quads[i].x[v] > boundMaxX) boundMaxX = quads[i].x[v];
            if (quads[i].y[v] < boundMinY) boundMinY = quads[i].y[v];
            if (quads[i].y[v] > boundMaxY) boundMaxY = quads[i].y[v];
        }
    }
    if (quads.empty() || boundMaxX <= boundMinX || boundMaxY <= boundMinY)
    {
        // empty or degenerate: sane defaults
        boundMinX = boundMinY = -400.0f;
        boundMaxX = boundMaxY = 400.0f;
    }
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4