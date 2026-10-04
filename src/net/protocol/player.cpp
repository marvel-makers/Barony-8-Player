#include "../private/common.hpp"

/*-------------------------------------------------------------------------------

 serverUpdateEffects

   Updates the status of the EFFECTS variables (blindness, drunkenness, etc.)
 for the specified client.

-------------------------------------------------------------------------------*/

void serverUpdateEffects(int player)
{
    if ( multiplayer != SERVER || clientnum == player )
    {
        return;
    }
    if ( player <= 0 || player >= barony::net::playerCapacity() )
    {
        return;
    }
    if ( client_disconnected[player] == true || players[player]->isLocalPlayer() )
    {
        return;
    }

    strcpy((char*)net_packet->data, "UPEF");
    int numBytes = NUMEFFECTS / 8;
    for ( int i = 0; i < numBytes; ++i )
    {
        net_packet->data[4 + i] = 0;
        net_packet->data[4 + numBytes + i] = 0;
    }

    std::vector<std::pair<Uint8, Uint8>> effectStrengths;
    for (int j = 0; j < NUMEFFECTS; j++)
    {
        Uint8 effectValue = stats[player]->getEffectActive(j);
        if ( effectValue > 0 )
        {
            net_packet->data[4 + j / 8] |= power(2, j - (j / 8) * 8);
            if ( effectValue > 1 )
            {
                // effect index, then value
                effectStrengths.push_back(std::make_pair(static_cast<Uint8>(j & 0xFF), effectValue));
            }
        }
        if ( stats[player]->EFFECTS_TIMERS[j] < TICKS_PER_SECOND * 5 && stats[player]->EFFECTS_TIMERS[j] > 0 )
        {
            // use these bits to denote if duration is low.
            net_packet->data[4 + numBytes + j / 8] |= power(2, j - (j / 8) * 8);
        }
    }
    
    net_packet->data[4 + numBytes * 2] = static_cast<Uint8>(effectStrengths.size());
    net_packet->len = 4 + numBytes * 2 + 1;
    for ( auto& pair : effectStrengths )
    {
        if ( net_packet->len + 1 >= NET_PACKET_SIZE )
        {
            // no more room
            break;
        }
        net_packet->data[net_packet->len + 0] = pair.first;
        net_packet->data[net_packet->len + 1] = pair.second;
        net_packet->len += 2;
    }

    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}

/*-------------------------------------------------------------------------------

 serverUpdateHunger

    Updates the HUNGER variable for the specified client

-------------------------------------------------------------------------------*/

void serverUpdateHunger(int player)
{
    if ( multiplayer != SERVER || clientnum == player )
    {
        return;
    }
    if ( player <= 0 || player >= barony::net::playerCapacity() )
    {
        return;
    }
    if ( client_disconnected[player] == true || players[player]->isLocalPlayer() )
    {
        return;
    }

    strcpy((char*)net_packet->data, "HNGR");
    SDLNet_Write32(stats[player]->HUNGER, &net_packet->data[4]);
    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    net_packet->len = 8;
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}

/*-------------------------------------------------------------------------------

serverUpdateSexChange

Updates all clients on specified player's sex

-------------------------------------------------------------------------------*/

void serverUpdateSexChange(int player)
{
    if ( multiplayer != SERVER || !stats[player] )
    {
        return;
    }

    for ( int c = 1; c < barony::net::playerCapacity(); c++ )
    {
        if ( client_disconnected[c] || players[c]->isLocalPlayer() )
        {
            continue;
        }
        strcpy((char*)net_packet->data, "SEXU");
        net_packet->data[4] = static_cast<Uint8>(player);
        net_packet->data[5] = static_cast<Uint8>(stats[player]->sex);
        net_packet->address.host = net_clients[c - 1].host;
        net_packet->address.port = net_clients[c - 1].port;
        net_packet->len = 6;
        sendPacketSafe(net_sock, -1, net_packet, c - 1);
    }
}

/*-------------------------------------------------------------------------------

serverUpdatePlayerStats

Updates all player current HP/MP for clients

-------------------------------------------------------------------------------*/

void serverUpdatePlayerStats()
{
    if ( multiplayer != SERVER )
    {
        return;
    }
    for ( int c = 1; c < barony::net::playerCapacity(); c++ )
    {
        if ( client_disconnected[c] || players[c]->isLocalPlayer() )
        {
            continue;
        }
        strcpy((char*)net_packet->data, "STAT");
        Sint32 playerHP = 0;
        Sint32 playerMP = 0;
        for ( int i = 0; i < barony::net::playerCapacity(); ++i )
        {
            if ( stats[i] )
            {
                playerHP = static_cast<Sint16>(stats[i]->MAXHP);
                playerHP |= static_cast<Sint16>(stats[i]->HP) << 16;
                playerMP = static_cast<Sint16>(stats[i]->MAXMP);
                playerMP |= static_cast<Sint16>(stats[i]->MP) << 16;
            }
            SDLNet_Write32(playerHP, &net_packet->data[4 + i * 8]); // 4/12/20/28 data
            SDLNet_Write32(playerMP, &net_packet->data[8 + i * 8]); // 8/16/24/32 data
            playerHP = 0;
            playerMP = 0;
        }
        net_packet->address.host = net_clients[c - 1].host;
        net_packet->address.port = net_clients[c - 1].port;
        net_packet->len = 4 + 8 * barony::net::playerCapacity();
        sendPacketSafe(net_sock, -1, net_packet, c - 1);
    }
}

/*-------------------------------------------------------------------------------

serverUpdatePlayerGameplayStats

Updates given players gameplayStatistics value by given increment.

-------------------------------------------------------------------------------*/
void serverUpdatePlayerGameplayStats(int player, int gameplayStat, int changeval)
{
    if ( player < 0 || player >= barony::net::playerCapacity() )
    {
        return;
    }
    if ( client_disconnected[player] )
    {
        return;
    }
    if ( player == 0 )
    {
        if ( gameplayStat == STATISTICS_TEMPT_FATE )
        {
            if ( gameStatistics[STATISTICS_TEMPT_FATE] == -1 )
            {
                // don't change, completed task.
            }
            else
            {
                if ( changeval == 5 )
                {
                    gameStatistics[gameplayStat] = changeval;
                }
                else if ( changeval == 1 && gameStatistics[gameplayStat] > 0 )
                {
                    gameStatistics[gameplayStat] = -1;
                }
            }
        }
        else if ( gameplayStat == STATISTICS_FORUM_TROLL )
        {
            if ( changeval == AchievementObserver::FORUM_TROLL_BREAK_WALL )
            {
                int walls = gameStatistics[gameplayStat] & 0xFF;
                walls = std::min(walls + 1, 3);
                gameStatistics[gameplayStat] = gameStatistics[gameplayStat] & 0xFFFFFF00;
                gameStatistics[gameplayStat] |= walls;
            }
            else if ( changeval == AchievementObserver::FORUM_TROLL_RECRUIT_TROLL )
            {
                int trolls = (gameStatistics[gameplayStat] >> 8) & 0xFF;
                trolls = std::min(trolls + 1, 3);
                gameStatistics[gameplayStat] = gameStatistics[gameplayStat] & 0xFFFF00FF;
                gameStatistics[gameplayStat] |= (trolls << 8);
            }
            else if ( changeval == AchievementObserver::FORUM_TROLL_FEAR )
            {
                int fears = (gameStatistics[gameplayStat] >> 16) & 0xFF;
                fears = std::min(fears + 1, 3);
                gameStatistics[gameplayStat] = gameStatistics[gameplayStat] & 0xFF00FFFF;
                gameStatistics[gameplayStat] |= (fears << 16);
            }
        }
        else if ( gameplayStat == STATISTICS_POP_QUIZ_1 || gameplayStat == STATISTICS_POP_QUIZ_2 )
        {
            int spellID = changeval;
            if ( spellID >= 30 )
            {
                spellID -= 30;
                int shifted = (1 << spellID);
                gameStatistics[gameplayStat] |= shifted;
            }
            else
            {
                int shifted = (1 << spellID);
                gameStatistics[gameplayStat] |= shifted;
            }
        }
        else if ( gameplayStat == STATISTICS_FLAVORTOWN )
        {
            gameStatistics[gameplayStat] |= changeval;
        }
        else if ( gameplayStat == STATISTICS_BARDIC_INSPIRATION )
        {
            if ( changeval == 0 )
            {
                gameStatistics[gameplayStat] = 0;
            }
            else
            {
                gameStatistics[gameplayStat] += changeval;
            }
        }
        else if ( gameplayStat == STATISTICS_PARRY_TANK )
        {
            if ( changeval == 0 )
            {
                if ( gameStatistics[gameplayStat] < 20 )
                {
                    gameStatistics[gameplayStat] = 0;
                }
            }
            else
            {
                gameStatistics[gameplayStat] += changeval;
            }
        }
        else
        {
            gameStatistics[gameplayStat] += changeval;
        }
    }
    else if ( !players[player]->isLocalPlayer() )
    {
        strcpy((char*)net_packet->data, "GPST");
        SDLNet_Write32(gameplayStat, &net_packet->data[4]);
        SDLNet_Write32(changeval, &net_packet->data[8]);
        net_packet->address.host = net_clients[player - 1].host;
        net_packet->address.port = net_clients[player - 1].port;
        net_packet->len = 12;
        sendPacketSafe(net_sock, -1, net_packet, player - 1);
    }
    //messagePlayer(clientnum, MESSAGE_DEBUG, "[DEBUG]: sent: %d, %d: val %d", gameplayStat, changeval, gameStatistics[gameplayStat]);
}

void serverUpdatePlayerConduct(int player, int conduct, int value)
{
    if ( player <= 0 || player >= barony::net::playerCapacity() )
    {
        return;
    }
    if ( client_disconnected[player] || players[player]->isLocalPlayer() )
    {
        return;
    }
    strcpy((char*)net_packet->data, "COND");
    SDLNet_Write16(conduct, &net_packet->data[4]);
    SDLNet_Write16(value, &net_packet->data[6]);
    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    net_packet->len = 8;
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}

/*-------------------------------------------------------------------------------

serverUpdatePlayerLVL

Updates all player current LVL for clients

-------------------------------------------------------------------------------*/

void serverUpdatePlayerLVL()
{
    if (multiplayer != SERVER)
    {
        return;
    }

    constexpr std::size_t headerSize = 5;
    constexpr std::size_t recordSize = 5;
    const std::size_t playerCount = std::size(players);
    const std::size_t maxRecords = (NET_PACKET_SIZE - headerSize) / recordSize;
    const std::size_t records = std::min(playerCount, maxRecords);

    std::memcpy(net_packet->data, "UPLV", 4);
    net_packet->data[4] = static_cast<Uint8>(records);

    std::size_t offset = headerSize;
    for (std::size_t player = 0; player < records; ++player)
    {
        net_packet->data[offset] = static_cast<Uint8>(player);
        const Sint32 level = stats[player] ? stats[player]->LVL : 0;
        SDLNet_Write32(static_cast<Uint32>(level), &net_packet->data[offset + 1]);
        offset += recordSize;
    }
    net_packet->len = static_cast<int>(offset);

    for (std::size_t player = 1; player < playerCount; ++player)
    {
        if (client_disconnected[player] || !players[player] || players[player]->isLocalPlayer())
        {
            continue;
        }
        net_packet->address.host = net_clients[player - 1].host;
        net_packet->address.port = net_clients[player - 1].port;
        sendPacketSafe(net_sock, -1, net_packet, static_cast<int>(player - 1));
    }
}

void serverRemoveClientFollower(int player, Uint32 uidToRemove)
{
    if ( multiplayer != SERVER || player <= 0 )
    {
        return;
    }
    if ( client_disconnected[player] || players[player]->isLocalPlayer() )
    {
        return;
    }

    strcpy((char*)net_packet->data, "LDEL");
    SDLNet_Write32(uidToRemove, &net_packet->data[4]);
    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    net_packet->len = 8;
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}

void serverSendItemToPickupAndEquip(int player, Item* item)
{
    if ( multiplayer != SERVER || player <= 0 )
    {
        return;
    }
    if ( client_disconnected[player] || players[player]->isLocalPlayer() )
    {
        return;
    }

    // send the client info on the item it just picked up
    strcpy((char*)net_packet->data, "ITEQ");
    SDLNet_Write32(item->type, &net_packet->data[4]);
    SDLNet_Write32(item->status, &net_packet->data[8]);
    SDLNet_Write32(static_cast<Uint32>(item->beatitude), &net_packet->data[12]);
    SDLNet_Write32(static_cast<Uint32>(item->count), &net_packet->data[16]);
    SDLNet_Write32(item->appearance, &net_packet->data[20]);
    SDLNet_Write32(item->ownerUid, &net_packet->data[24]);
    net_packet->data[28] = item->identified;
    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    net_packet->len = 29;
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}

void serverUpdateAllyStat(int player, Uint32 uidToUpdate, int LVL, int HP, int MAXHP, int type)
{
    if ( multiplayer != SERVER || player <= 0 )
    {
        return;
    }
    if ( client_disconnected[player] || players[player]->isLocalPlayer() )
    {
        return;
    }

    strcpy((char*)net_packet->data, "NPCI");
    SDLNet_Write32(uidToUpdate, &net_packet->data[4]);
    net_packet->data[8] = static_cast<Uint8>(LVL);
    SDLNet_Write16(HP, &net_packet->data[9]);
    SDLNet_Write16(MAXHP, &net_packet->data[11]);
    net_packet->data[13] = static_cast<Uint8>(type);
    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    net_packet->len = 14;
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}

void serverUpdatePlayerSummonStrength(int player)
{
    if ( multiplayer != SERVER )
    {
        return;
    }
    if ( player <= 0 || !barony::net::validPlayer(player) )
    {
        return;
    }
    if ( client_disconnected[player] || !stats[player] || players[player]->isLocalPlayer() )
    {
        return;
    }

    strcpy((char*)net_packet->data, "SUMS");
    SDLNet_Write32(stats[player]->playerSummonLVLHP, &net_packet->data[4]);
    SDLNet_Write32(stats[player]->playerSummonSTRDEXCONINT, &net_packet->data[8]);
    SDLNet_Write32(stats[player]->playerSummonPERCHR, &net_packet->data[12]);
    SDLNet_Write32(stats[player]->playerSummon2LVLHP, &net_packet->data[16]);
    SDLNet_Write32(stats[player]->playerSummon2STRDEXCONINT, &net_packet->data[20]);
    SDLNet_Write32(stats[player]->playerSummon2PERCHR, &net_packet->data[24]);
    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    net_packet->len = 28;
    sendPacketSafe(net_sock, -1, net_packet, player - 1);
}

void serverUpdateAllyHP(int player, Uint32 uidToUpdate, int HP, int MAXHP, bool guarantee)
{
    if ( multiplayer != SERVER )
    {
        return;
    }
    if ( player <= 0 )
    {
        return;
    }
    if ( client_disconnected[player] || players[player]->isLocalPlayer() )
    {
        return;
    }

    strcpy((char*)net_packet->data, "NPCU");
    SDLNet_Write32(uidToUpdate, &net_packet->data[4]);
    SDLNet_Write16(HP, &net_packet->data[8]);
    SDLNet_Write16(MAXHP, &net_packet->data[10]);
    net_packet->address.host = net_clients[player - 1].host;
    net_packet->address.port = net_clients[player - 1].port;
    net_packet->len = 12;
    if ( !guarantee )
    {
        sendPacket(net_sock, -1, net_packet, player - 1);
    }
    else
    {
        sendPacketSafe(net_sock, -1, net_packet, player - 1);
    }
}

void sendMinimapPing(Uint8 player, Uint8 x, Uint8 y, Uint8 pingType, bool radius)
{
    if ( multiplayer == CLIENT )
    {
        // send to host to relay info.
        strcpy((char*)net_packet->data, "PMAP"); 
        net_packet->data[4] = player;
        net_packet->data[5] = x;
        net_packet->data[6] = y;
        net_packet->data[7] = pingType;
        net_packet->data[8] = radius ? 1 : 0;

        net_packet->address.host = net_server.host;
        net_packet->address.port = net_server.port;
        net_packet->len = 9;
        sendPacket(net_sock, -1, net_packet, 0);
    }
    else
    {
        for ( int c = 0; c < barony::net::playerCapacity(); c++ )
        {
            if ( client_disconnected[c] )
            {
                continue;
            }
            if ( players[c]->isLocalPlayer() )
            {
                minimapPingAdd(player, c, MinimapPing(ticks, player, x, y, radius, static_cast<MinimapPing::PingType>(pingType)));
                continue;
            }

            if ( multiplayer == SERVER )
            {
                // send to all clients.
                strcpy((char*)net_packet->data, "PMAP");
                net_packet->data[4] = player;
                net_packet->data[5] = x;
                net_packet->data[6] = y;
                net_packet->data[7] = pingType;
                net_packet->data[8] = radius ? 1 : 0;

                net_packet->address.host = net_clients[c - 1].host;
                net_packet->address.port = net_clients[c - 1].port;
                net_packet->len = 9;
                sendPacketSafe(net_sock, -1, net_packet, c - 1);
            }
        }
    }
}

void sendAllyCommandClient(int player, Uint32 uid, int command, Uint8 x, Uint8 y, Uint32 targetUid)
{
    if ( multiplayer != CLIENT )
    {
        return;
    }
    //messagePlayer(clientnum, "%d", targetUid);

    // send to host.
    strcpy((char*)net_packet->data, "ALLY");
    net_packet->data[4] = player;
    net_packet->data[5] = command;
    net_packet->data[6] = x;
    net_packet->data[7] = y;
    SDLNet_Write32(uid, &net_packet->data[8]);
    net_packet->len = 12;
    if ( targetUid != 0 )
    {
        SDLNet_Write32(targetUid, &net_packet->data[12]);
        net_packet->len = 16;
    }
    net_packet->address.host = net_server.host;
    net_packet->address.port = net_server.port;
    sendPacket(net_sock, -1, net_packet, 0);
}

