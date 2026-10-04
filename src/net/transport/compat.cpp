#include "../private/common.hpp"
#include "transport.hpp"
#include "udp.hpp"
#include "../metrics.hpp"
#include "../types.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace
{
constexpr Uint8 kUnassignedPlayer = 0xff;

void destroySafePacket(void* data)
{
    auto* pending = static_cast<packetsend_t*>(data);
    if (!pending)
    {
        return;
    }
    SDLNet_FreePacket(pending->packet);
    std::free(pending);
}

bool validPacket(const UDPpacket* packet)
{
    return packet && packet->data && packet->len > 0 && packet->len <= NET_PACKET_SIZE;
}

bool useLegacyLobbyTransport()
{
    // MainMenu's online-lobby receive loop still consumes the legacy Steam/EOS
    // P2P queues. Do not switch those sends to the gameplay transport until
    // the game has actually left the intro/main-menu state.
    return !directConnect && intro;
}

int sendLegacyLobbyPacket(UDPpacket* packet, int hostnum, bool reliable)
{
    if (!validPacket(packet) || hostnum < 0
        || hostnum >= barony::net::playerCapacity())
    {
        return 0;
    }

    if (LobbyHandler.getP2PType() == LobbyHandler_t::LobbyServiceType::LOBBY_STEAM)
    {
#ifdef STEAMWORKS
        if (!steamIDRemote[hostnum])
        {
            return 0;
        }
        const auto mode = reliable ? k_EP2PSendReliable : k_EP2PSendUnreliable;
        return SteamNetworking()->SendP2PPacket(
            *static_cast<CSteamID*>(steamIDRemote[hostnum]),
            packet->data,
            static_cast<std::uint32_t>(packet->len),
            mode,
            0) ? 1 : 0;
#else
        return 0;
#endif
    }

    if (LobbyHandler.getP2PType() == LobbyHandler_t::LobbyServiceType::LOBBY_CROSSPLAY)
    {
#ifdef USE_EOS
        auto peer = EOS.P2PConnectionInfo.getPeerIdFromIndex(hostnum);
        if (!peer)
        {
            return 0;
        }
        EOS.SendMessageP2P(peer, packet->data, packet->len, reliable);
        return 1;
#else
        return 0;
#endif
    }

    return 0;
}
}

void packetDeconstructor(void* data)
{
    destroySafePacket(data);
}

void pollNetworkForShutdown()
{
    if (directConnect && multiplayer && !(SDL_GetTicks() % 25))
    {
        int processed = 0;
        for (node_t* node = safePacketsSent.first, *next = nullptr; node; node = next)
        {
            next = node->next;
            auto* pending = static_cast<packetsend_t*>(node->element);
            if (!pending || !pending->packet)
            {
                list_RemoveNode(node);
                continue;
            }

            barony::net::sendUdp(pending->sock, pending->channel, pending->packet);
            barony::net::metrics().recordRetry();
            ++pending->tries;
            if (pending->tries >= MAXTRIES)
            {
                list_RemoveNode(node);
            }
            if (++processed >= MAXDELETES)
            {
                break;
            }
        }
    }

    if (auto* transport = barony::net::platformTransport())
    {
        transport->tick();
    }

#ifdef STEAMWORKS
    SteamAPI_RunCallbacks();
#endif
}

int sendPacket(UDPsocket sock, int channel, UDPpacket* packet, int hostnum, bool tryReliable)
{
    if (!validPacket(packet) || hostnum < 0)
    {
        return 0;
    }

    if (directConnect)
    {
        const int result = barony::net::sendUdp(sock, channel, packet);
        if (result)
        {
            barony::net::metrics().recordSend(
                packet->data,
                static_cast<std::size_t>(packet->len),
                tryReliable ? barony::net::Reliability::Reliable : barony::net::Reliability::Unreliable);
        }
        return result;
    }

    if (useLegacyLobbyTransport())
    {
        const int result = sendLegacyLobbyPacket(packet, hostnum, tryReliable);
        if (result)
        {
            barony::net::metrics().recordSend(
                packet->data,
                static_cast<std::size_t>(packet->len),
                tryReliable ? barony::net::Reliability::Reliable : barony::net::Reliability::Unreliable);
        }
        return result;
    }

    auto* transport = barony::net::platformTransport();
    if (!transport)
    {
        return 0;
    }

    const barony::net::PacketView view{
        packet->data,
        static_cast<std::size_t>(packet->len)
    };
    const auto reliability = tryReliable
        ? barony::net::Reliability::Reliable
        : barony::net::Reliability::Unreliable;
    return transport->send(static_cast<barony::net::HostIndex>(hostnum), view, reliability) ? 1 : 0;
}

Uint32 packetnum = 0;

int sendPacketSafe(UDPsocket sock, int channel, UDPpacket* packet, int hostnum)
{
    if (!validPacket(packet) || hostnum < 0)
    {
        return 0;
    }

    if (!directConnect)
    {
        if (useLegacyLobbyTransport())
        {
            const int result = sendLegacyLobbyPacket(packet, hostnum, true);
            if (result)
            {
                barony::net::metrics().recordSend(
                    packet->data,
                    static_cast<std::size_t>(packet->len),
                    barony::net::Reliability::Reliable);
            }
            return result;
        }

        auto* transport = barony::net::platformTransport();
        if (!transport)
        {
            return 0;
        }
        const barony::net::PacketView view{
            packet->data,
            static_cast<std::size_t>(packet->len)
        };
        return transport->send(
            static_cast<barony::net::HostIndex>(hostnum),
            view,
            barony::net::Reliability::Reliable) ? 1 : 0;
    }

    constexpr int safeHeaderSize = 9;
    if (packet->len + safeHeaderSize > NET_PACKET_SIZE)
    {
        printlog("[NET]: reliable UDP packet too large: %d bytes", packet->len);
        return 0;
    }

    auto* pending = static_cast<packetsend_t*>(std::calloc(1, sizeof(packetsend_t)));
    if (!pending)
    {
        return 0;
    }

    pending->packet = SDLNet_AllocPacket(NET_PACKET_SIZE);
    if (!pending->packet)
    {
        std::free(pending);
        return 0;
    }

    pending->hostnum = hostnum;
    pending->sock = sock;
    pending->channel = channel;
    pending->packet->channel = channel;
    pending->packet->address = packet->address;
    pending->packet->len = packet->len + safeHeaderSize;

    std::memcpy(pending->packet->data, "SAFE", 4);
    pending->packet->data[4] = (receivedclientnum || multiplayer != CLIENT)
        ? static_cast<Uint8>(clientnum)
        : kUnassignedPlayer;
    SDLNet_Write32(packetnum, &pending->packet->data[5]);
    std::memcpy(pending->packet->data + safeHeaderSize, packet->data, packet->len);

    pending->num = packetnum++;
    pending->tries = 0;

    node_t* node = list_AddNodeFirst(&safePacketsSent);
    node->element = pending;
    node->deconstructor = &destroySafePacket;

    const int result = barony::net::sendUdp(sock, channel, pending->packet);
    if (result)
    {
        barony::net::metrics().recordSend(
            packet->data,
            static_cast<std::size_t>(packet->len),
            barony::net::Reliability::Reliable);
    }
    return result;
}
