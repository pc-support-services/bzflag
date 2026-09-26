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
 *  the BZFS protocol handshake: TCP connect + BZFLAG header, then
 *  MsgQueryPlayers ('qp').  The server replies with a count header
 *  followed by one MsgAddPlayer ('ap') per online player carrying
 *  id, type, team, score, callsign and motto.  Callsign and team are
 *  kept; everything else is discarded.  No third-party service.
 */

#ifndef __SERVERQUERYPLAYERS_H__
#define __SERVERQUERYPLAYERS_H__

// system interface headers
#include <string>
#include <map>
#include <vector>

// common interface headers
#include "TimeKeeper.h"
#include "Address.h"


class ServerQueryPlayers
{
public:
    static ServerQueryPlayers& instance();

    // kick off an async query for this server ("host:port");
    // throttled and ignored while a query is already running
    void queryServer(const std::string& addrName);

    // rendered player line for a server, "" when nothing cached
    std::string getPlayersString(const std::string& addrName) const;

    // pump the in-progress query; call from the menu callback each frame
    void pump();

    // true while a query is running
    bool isActive() const
    {
        return active;
    }

private:
    ServerQueryPlayers();
    ~ServerQueryPlayers();

    void finish();

    enum Phase
    {
        Idle = 0,
        Connecting,     // non-blocking connect in progress
        ConnectHeader,  // waiting for BZFS#### + my id
        WaitCounts,     // waiting for MsgQueryPlayers reply header
        WaitPackets     // collecting MsgAddPlayer packets
    };

    int         fd;
    Phase       phase;
    std::string queryingAddr;   // "host:port" being queried
    Address     serverAddress;
    int         serverPort;

    int         numTeams;
    int         numPlayers;
    std::vector<std::pair<int, std::string> > gotPlayers; // (team, callsign)

    std::vector<char> inBuf;    // persistent TCP read buffer
    bool        wantHandshake;  // still consuming version/id bytes

    std::map<std::string, std::string> serverPlayers; // "host:port" -> line
    TimeKeeper  lastFetch;
    TimeKeeper  startTime;

    bool        active;
};

#endif //__SERVERQUERYPLAYERS_H__

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4