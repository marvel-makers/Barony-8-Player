#include "../private/common.hpp"
#include "udp.hpp"

namespace barony::net
{
int sendUdp(UDPsocket socket, int channel, UDPpacket* packet) noexcept
{
    if (!socket || !packet)
    {
        return 0;
    }
    return SDLNet_UDP_Send(socket, channel, packet);
}
}
