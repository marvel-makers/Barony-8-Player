#include "private/common.hpp"

namespace
{
constexpr Uint8 kUnassignedPlayer = 0xff;
}

bool handleSafePacket()
{
    if (!directConnect || !net_packet || net_packet->len < 4)
    {
        return false;
    }

    const Uint32 packetId = SDLNet_Read32(&net_packet->data[0]);
    if (packetId == 'GOTP')
    {
        if (net_packet->len < 9)
        {
            return true;
        }
        const Uint32 acknowledged = SDLNet_Read32(&net_packet->data[5]);
        for (node_t* node = safePacketsSent.first; node; node = node->next)
        {
            const auto* packet = static_cast<packetsend_t*>(node->element);
            if (packet && packet->num == acknowledged)
            {
                list_RemoveNode(node);
                break;
            }
        }
        return true;
    }

    if (packetId != 'SAFE')
    {
        return false;
    }

    if (net_packet->len < 9)
    {
        return true;
    }

    const Uint8 sender = net_packet->data[4];
    if (sender == kUnassignedPlayer)
    {
        return false;
    }
    if (sender >= std::size(safePacketsReceivedMap))
    {
        return true;
    }

    const Uint32 sequence = SDLNet_Read32(&net_packet->data[5]);
    auto& received = safePacketsReceivedMap[sender];

    if (ticks > 60 * TICKS_PER_SECOND && ticks % (TICKS_PER_SECOND / 2) == 0)
    {
        const Uint32 cutoff = ticks - 60 * TICKS_PER_SECOND;
        for (auto it = received.begin(); it != received.end();)
        {
            if (it->second < cutoff)
            {
                it = received.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    const bool duplicate = received.find(sequence) != received.end();
    if (!duplicate)
    {
        received.emplace(sequence, ticks);
    }

    const int originalLength = net_packet->len;
    const int senderPlayer = sender;

    std::memcpy(net_packet->data, "GOTP", 4);
    net_packet->data[4] = static_cast<Uint8>(clientnum);
    SDLNet_Write32(sequence, &net_packet->data[5]);
    net_packet->len = 9;

    if (multiplayer == CLIENT)
    {
        net_packet->address.host = net_server.host;
        net_packet->address.port = net_server.port;
        sendPacket(net_sock, -1, net_packet, 0);
    }
    else if (senderPlayer > 0 && static_cast<std::size_t>(senderPlayer - 1) < std::size(players) - 1)
    {
        net_packet->address.host = net_clients[senderPlayer - 1].host;
        net_packet->address.port = net_clients[senderPlayer - 1].port;
        sendPacket(net_sock, -1, net_packet, senderPlayer - 1);
    }

    if (duplicate)
    {
        return true;
    }

    const int payloadLength = originalLength - 9;
    if (payloadLength <= 0)
    {
        net_packet->len = 0;
        return true;
    }

    std::memmove(net_packet->data, net_packet->data + 9, static_cast<std::size_t>(payloadLength));
    net_packet->len = payloadLength;
    return false;
}
