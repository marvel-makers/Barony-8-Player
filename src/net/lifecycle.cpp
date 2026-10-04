#include "private/common.hpp"
#include "transport/transport.hpp"

void closeNetworkInterfaces()
{
    printlog("closing network interfaces...\n");
    receivedclientnum = false;

    barony::net::resetPlatformTransport();

    if (net_packet)
    {
        SDLNet_FreePacket(net_packet);
        net_packet = nullptr;
    }
    if (net_clients)
    {
        std::free(net_clients);
        net_clients = nullptr;
    }
    if (net_sock)
    {
        SDLNet_UDP_Close(net_sock);
        net_sock = nullptr;
    }
    if (net_tcpclients)
    {
        for (int c = 0; c < barony::net::playerCapacity(); ++c)
        {
            if (net_tcpclients[c])
            {
                SDLNet_TCP_Close(net_tcpclients[c]);
            }
        }
        std::free(net_tcpclients);
        net_tcpclients = nullptr;
    }
    if (net_tcpsock)
    {
        SDLNet_TCP_Close(net_tcpsock);
        net_tcpsock = nullptr;
    }
    if (tcpset)
    {
        SDLNet_FreeSocketSet(tcpset);
        tcpset = nullptr;
    }

#ifdef STEAMWORKS
    for (int c = 0; c < barony::net::playerCapacity(); ++c)
    {
        barony::net::clearSteamPeer(static_cast<barony::net::HostIndex>(c));
        if (steamIDRemote[c])
        {
            cpp_Free_CSteamID(steamIDRemote[c]);
            steamIDRemote[c] = nullptr;
        }
    }
#endif
}
