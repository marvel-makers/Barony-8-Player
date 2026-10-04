#include "../private/common.hpp"

void deleteMultiplayerSaveGames()
{
    if ( multiplayer != SERVER )
    {
        return;
    }

    if ( !gameModeManager.allowsSaves() )
    {
        return;
    }

    //Only delete saves if no players are left alive.
    bool lastAlive = true;

    //const int playersAtStartOfMap = numplayers;
    //int currentPlayers = 0;
    //for ( std::size_t i = 0; i < std::size(players); ++i )
    //{
    //  if ( !client_disconnected[i] )
    //  {
    //      ++currentPlayers;
    //  }
    //}

    //if ( currentPlayers != playersAtStartOfMap )
    //{
    //  return;
    //}

    for ( std::size_t i = 0; i < std::size(players); ++i )
    {
        const Stat* stat = nullptr;
        if ( players[i] && players[i]->entity && (stat = players[i]->entity->getStats()) && stat->HP > 0)
        {
            lastAlive = false;
        }
    }
    if ( !lastAlive )
    {
        return;
    }

    deleteSaveGame(multiplayer); // stops save scumming c:

    for ( int i = 1; i < barony::net::playerCapacity(); ++i )
    {
        if ( client_disconnected[i] )
        {
            continue;
        }
        strcpy((char *)net_packet->data,"DSAV"); //Delete save game.
        net_packet->address.host = net_clients[i - 1].host;
        net_packet->address.port = net_clients[i - 1].port;
        net_packet->len = 4;
        sendPacketSafe(net_sock, -1, net_packet, i - 1);
    }
}

void handleScanPacket() {
    if (directConnect) {
        const Uint32 hostname_len = static_cast<Uint32>(strlen(MainMenu::getHostname()));
        SDLNet_Write32(hostname_len, &net_packet->data[4]);
        for (int c = 0; c < hostname_len; ++c) {
            net_packet->data[8 + c] = MainMenu::getHostname()[c];
        }
        const Uint32 offset = 8 + hostname_len;
        int numplayers = 0;
        for (int c = 0; c < barony::net::playerCapacity(); ++c) {
            if (!client_disconnected[c]) {
                ++numplayers;
            }
        }
        SDLNet_Write32(numplayers, &net_packet->data[offset]);
        net_packet->data[offset + 4] = intro ? 0 : 1;
        SDLNet_Write32(svFlags, &net_packet->data[offset + 5]);
        net_packet->len = offset + 9;
        sendPacket(net_sock, -1, net_packet, 0);
    }
}

PingNetworkStatusStore PingNetworkStatus;
bool PingNetworkStatus_t::bEnabled = false;
int PingNetworkStatus_t::pingLimitGreen = 100;
int PingNetworkStatus_t::pingLimitYellow = 150;
int PingNetworkStatus_t::pingLimitOrange = 250;
bool PingNetworkStatus_t::pingHUDDisplayGreen = false;
bool PingNetworkStatus_t::pingHUDDisplayYellow = false;
bool PingNetworkStatus_t::pingHUDDisplayOrange = true;
bool PingNetworkStatus_t::pingHUDDisplayRed = true;
bool PingNetworkStatus_t::pingHUDShowOKBriefly = true;
bool PingNetworkStatus_t::pingHUDShowNumericValue = false;
void PingNetworkStatus_t::reset()
{
    for ( std::size_t i = 0; i < std::size(players); ++i )
    {
        PingNetworkStatus[i].clear();
    }
}

void PingNetworkStatus_t::receive()
{
    const int player = net_packet->data[4];
    if ( player < 0 || static_cast<std::size_t>(player) >= std::size(players) )
    {
        return;
    }
    auto& p = PingNetworkStatus[player];
    const Uint32 seq = SDLNet_Read32(&net_packet->data[5]);

    const auto find = p.pings.find(seq);
    if ( find != p.pings.end() )
    {
        if ( seq > p.lastSequence )
        {
            p.lastPingtime = SDL_GetTicks() - find->second; // update our displayed ping value
            p.lastSequence = seq;
            //messagePlayer(clientnum, MESSAGE_DEBUG, "[%d]: %4d ms", player, p.lastPingtime);
        }
        p.pings.erase(seq);

    }
    std::vector<Uint32> toErase;
    for ( auto& keypair : p.pings )
    {
        if ( keypair.first < seq )
        {
            toErase.push_back(keypair.first);
        }
    }
    for ( auto& key : toErase )
    {
        p.pings.erase(key);
    }
}

void PingNetworkStatus_t::respond()
{
    const int player = net_packet->data[4];
    if ( player < 0 || static_cast<std::size_t>(player) >= std::size(players) )
    {
        return;
    }

    strcpy((char*)net_packet->data, "PNGR");
    net_packet->data[4] = clientnum;
    net_packet->len = 9;

    if ( multiplayer == CLIENT )
    {
        net_packet->address.host = net_server.host;
        net_packet->address.port = net_server.port;
        sendPacketSafe(net_sock, -1, net_packet, 0);
    }
    else if ( multiplayer == SERVER )
    {
        if ( player > 0 )
        {
            net_packet->address.host = net_clients[player - 1].host;
            net_packet->address.port = net_clients[player - 1].port;
            sendPacketSafe(net_sock, -1, net_packet, player - 1);
        }
    }
}

void PingNetworkStatus_t::saveDisplayMillis(bool forceUpdate)
{
    if ( oldestSequenceTicks > 0 )
    {
        displayMillisImmediate = std::max(SDL_GetTicks() - oldestSequenceTicks, lastPingtime);
    }
    else
    {
        displayMillisImmediate = lastPingtime;
    }
    if ( (ticks % (TICKS_PER_SECOND * 2) == 0) || displayMillis == 0 || forceUpdate )
    {
        displayMillis = displayMillisImmediate;
    }
}

void PingNetworkStatus_t::update()
{
    if ( !bEnabled )
    {
        reset();
        return;
    }
    if ( !(multiplayer == CLIENT || multiplayer == SERVER) )
    {
        reset();
        return;
    }
    if ( !net_packet ) { return; }

    if ( true )
    {
        bool updated = false;
        if ( multiplayer == CLIENT ) 
        {
            if ( PingNetworkStatus[0].needsUpdate || (ticks % (5 * TICKS_PER_SECOND) == 0) )
            {
                updated = true;
                PingNetworkStatus[0].needsUpdate = false;

                strcpy((char*)net_packet->data, "PNGU");
                net_packet->data[4] = clientnum;
                net_packet->len = 9;

                ++PingNetworkStatus[0].sequence;
                SDLNet_Write32(PingNetworkStatus[0].sequence, &net_packet->data[5]);
                net_packet->address.host = net_server.host;
                net_packet->address.port = net_server.port;
                sendPacketSafe(net_sock, -1, net_packet, 0);

                PingNetworkStatus[0].pings[PingNetworkStatus[0].sequence] = SDL_GetTicks();
            }
        }
        else if ( multiplayer == SERVER ) 
        {
            for ( std::size_t i = 1; i < std::size(players); ++i ) 
            {
                if ( client_disconnected[i] ) 
                {
                    continue;
                }

                if ( PingNetworkStatus[i].needsUpdate || (ticks % (5 * TICKS_PER_SECOND) == 0) )
                {
                    updated = true;
                    PingNetworkStatus[i].needsUpdate = false;

                    strcpy((char*)net_packet->data, "PNGU");
                    net_packet->data[4] = clientnum;
                    net_packet->len = 9;

                    ++PingNetworkStatus[i].sequence;
                    SDLNet_Write32(PingNetworkStatus[i].sequence, &net_packet->data[5]);
                    net_packet->address.host = net_clients[i - 1].host;
                    net_packet->address.port = net_clients[i - 1].port;
                    sendPacketSafe(net_sock, -1, net_packet, i - 1);

                    PingNetworkStatus[i].pings[PingNetworkStatus[i].sequence] = SDL_GetTicks();
                }
            }
        }

        if ( updated )
        {
            for ( std::size_t i = 0; i < std::size(players); ++i )
            {
                auto& p = PingNetworkStatus[i];
                while ( p.pings.size() >= 10 )
                {
                    Uint32 minSequence = 0;
                    for (const auto& keypairs : p.pings )
                    {
                        if ( minSequence == 0 )
                        {
                            minSequence = keypairs.first;
                        }
                        else if ( keypairs.first < minSequence )
                        {
                            minSequence = keypairs.first;
                        }
                    }

                    if ( minSequence > 0 )
                    {
                        p.pings.erase(minSequence);
                    }
                }
            }
        }
    }

    for ( std::size_t i = 0; i < std::size(players); ++i )
    {
        auto& p = PingNetworkStatus[i];
        if ( client_disconnected[i] )
        {
            p.clear();
            return;
        }
        Uint32 minSequence = 0;
        for (const auto& keypairs : p.pings )
        {
            if ( minSequence == 0 )
            {
                minSequence = keypairs.first;
            }
            else if ( keypairs.first < minSequence )
            {
                minSequence = keypairs.first;
            }

        }
        if ( minSequence > 0 )
        {
            p.oldestSequenceTicks = p.pings[minSequence];
        }
        else
        {
            p.oldestSequenceTicks = 0;
        }

        p.saveDisplayMillis();
    }
}