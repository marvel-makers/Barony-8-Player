#pragma once

#include "../types.hpp"
#include <SDL_net.h>

namespace barony::net
{
    int sendUdp(UDPsocket socket, int channel, UDPpacket* packet) noexcept;
}