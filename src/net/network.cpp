#include "network.hpp"

namespace barony::net
{
namespace
{
Network gNetwork;
}

ITransport* Network::transport()
{
    return platformTransport();
}

void Network::poll(std::vector<IncomingPacket>& packets)
{
    if (auto* active = transport())
    {
        active->poll(packets);
    }
}

void Network::tick()
{
    if (auto* active = transport())
    {
        active->tick();
    }
}

void Network::shutdown()
{
    resetPlatformTransport();
    connections_.clear();
}

Network& network() noexcept
{
    return gNetwork;
}
}
