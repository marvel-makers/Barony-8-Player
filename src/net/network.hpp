#pragma once

#include "connections.hpp"
#include "metrics.hpp"
#include "transport/transport.hpp"

#include <vector>

namespace barony::net
{
class Network
{
public:
    ITransport* transport();
    void poll(std::vector<IncomingPacket>& packets);
    void tick();
    void shutdown();

    ConnectionRegistry& connections() noexcept { return connections_; }
    NetworkMetrics& statistics() noexcept { return metrics(); }

private:
    ConnectionRegistry connections_;
};

Network& network() noexcept;
}
