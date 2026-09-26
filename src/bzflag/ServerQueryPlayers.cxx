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
 * ServerQueryPlayers:
 *  Asks the selected game server directly for its player list using
 *  the BZFS protocol handshake: TCP connect, "BZFLAG\r\n\r\n" header,
 *  server replies 8-byte version + 1-byte my-id, then MsgQueryPlayers
 *  ('qp').  The server replies with a count header followed by one
 *  MsgAddPlayer ('ap') per online player carrying id, type, team,
 *  score, callsign and motto.  Callsign and team are kept; the rest is
 *  discarded.  No third-party service.
 */

// interface header
#include "ServerQueryPlayers.h"

// common implementation headers
#include "Team.h"
#include "AnsiCodes.h"
#include "TextUtils.h"
#include "version.h"
#include "network.h"
#include "Protocol.h"
#include "Pack.h"

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
#include <vector>
#include <map>


// minimum seconds between any two queries
static const float MinRetryDelay = 2.0f;
// give up on a slow server after this many seconds
static const float QueryTimeout = 8.0f;

ServerQueryPlayers::ServerQueryPlayers()
    : fd(-1), phase(Idle), serverPort(ServerPort), numTeams(0), numPlayers(0),
      lastFetch(), startTime(), active(false)
{
}

ServerQueryPlayers::~ServerQueryPlayers()
{
    if (fd >= 0)
        close(fd);
}

ServerQueryPlayers& ServerQueryPlayers::instance()
{
    static ServerQueryPlayers theInstance;
    return theInstance;
}

void ServerQueryPlayers::queryServer(const std::string& addrName)
{
    if (addrName.empty() || active)
        return;

    const double since = TimeKeeper::getCurrent() - lastFetch;
    if (since < (double)MinRetryDelay)
        return; // never hammer when paging through the list
    lastFetch = TimeKeeper::getCurrent();

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
        return;
    }

    // connection is being established (or already is, on loopback):
    // defer the header send + handshake parsing to pump() so a connect
    // still in flight can complete.  sending here would fail with
    // ENOTCONN/EALREADY on a real (non-loopback) server and kill the
    // query before it started.
    queryingAddr = addrName;
    serverAddress = addr;
    serverPort = port;
    numTeams = 0;
    numPlayers = 0;
    gotPlayers.clear();
    inBuf.clear();
    wantHandshake = true;
    phase = Connecting;
    startTime = TimeKeeper::getCurrent();
    active = true;
}

std::string ServerQueryPlayers::getPlayersString(const std::string& addrName) const
{
    std::map<std::string, std::string>::const_iterator it = serverPlayers.find(addrName);
    if (it == serverPlayers.end())
        return "";
    return it->second;
}

// pull all available stream bytes into inBuf (non-blocking)
static void drainStream(int fd, std::vector<char>& inBuf)
{
    char tmp[2048];
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

// MsgAddPlayer code and body size: id(1) type(2) team(2) wins(2) losses(2)
// tks(2) callsign(32) motto(128)
static const uint16_t MsgAddPlayerCode = 0x6170; // 'ap'
static const size_t AddPlayerBodyLen = 1 + 10 + CallSignLen + MottoLen;

void ServerQueryPlayers::pump()
{
    if (!active || fd < 0)
        return;

    // timeout
    if ((TimeKeeper::getCurrent() - startTime) > (double)QueryTimeout)
    {
        finish();
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
            finish();
            return;
        }
        // connected: send the connect header (server replies with version)
        if (send(fd, BZ_CONNECT_HEADER, (int)strlen(BZ_CONNECT_HEADER), 0) !=
                (int)strlen(BZ_CONNECT_HEADER))
        {
            finish();
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
            finish();
            return;
        }
        char idByte;
        if (!takeBytes(inBuf, &idByte, 1))
            return;
        wantHandshake = false;

        // handshake complete: send the player-list query
        char qbuf[4];
        void* buf = qbuf;
        buf = nboPackUShort(buf, 0);
        buf = nboPackUShort(buf, MsgQueryPlayers);
        if (send(fd, qbuf, sizeof(qbuf), 0) != sizeof(qbuf))
        {
            finish();
            return;
        }
        phase = WaitCounts;
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

        if (code == MsgSuperKill)
        {
            finish();
            return;
        }

        if (phase == WaitCounts)
        {
            if (code == MsgReject)
            {
                finish();
                return;
            }
            if (code != MsgQueryPlayers || len != 4)
            {
                finish();
                return;
            }
            char body[4];
            if (!takeBytes(inBuf, body, 4))
                return; // body split across TCP segments; wait for more
            const void* bbuf = body;
            uint16_t nt, np;
            bbuf = nboUnpackUShort(bbuf, nt);
            bbuf = nboUnpackUShort(bbuf, np);
            numTeams = nt;
            numPlayers = np;
            phase = WaitPackets;
            continue;
        }

        if (phase == WaitPackets)
        {
            if (code != MsgAddPlayerCode || len != AddPlayerBodyLen)
            {
                // unexpected packet (Score, TeamUpdate, ...) - skip body
                if (len > 0 && !skipBytes(inBuf, len))
                    return; // body split; wait for more stream bytes
                continue;
            }
            static char body[256];
            if (!takeBytes(inBuf, body, AddPlayerBodyLen))
                return; // body split across TCP segments; wait for more
            const void* bbuf = body + 1; // skip id
            uint16_t type, team, wins, losses, tks;
            bbuf = nboUnpackUShort(bbuf, type);
            bbuf = nboUnpackUShort(bbuf, team);
            bbuf = nboUnpackUShort(bbuf, wins);
            bbuf = nboUnpackUShort(bbuf, losses);
            bbuf = nboUnpackUShort(bbuf, tks);
            char callsign[CallSignLen + 1];
            memcpy(callsign, bbuf, CallSignLen);
            callsign[CallSignLen] = '\0';
            gotPlayers.push_back(std::make_pair((int)team,
                                                stripAnsiCodes(callsign)));
            if ((int)gotPlayers.size() >= numPlayers)
            {
                // all players arrived: render the line
                std::string line;
                for (size_t i = 0; i < gotPlayers.size(); i++)
                {
                    if (i > 0)
                        line += ANSI_STR_FG_BLACK ", ";
                    const TeamColor tc = (TeamColor)gotPlayers[i].first;
                    line += Team::getAnsiCode(tc);
                    line += gotPlayers[i].second;
                    line += ANSI_STR_RESET;
                }
                serverPlayers[queryingAddr] = line;
                finish();
                return;
            }
            continue;
        }

        // unknown phase
        finish();
        return;
    }
}

void ServerQueryPlayers::finish()
{
    if (fd >= 0)
        close(fd);
    fd = -1;
    phase = Idle;
    active = false;
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4