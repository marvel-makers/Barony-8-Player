#pragma once

#include "../packet.hpp"
#include "../types.hpp"

#include <memory>
#include <vector>

namespace barony::net
{
struct IncomingPacket
{
    HostIndex host = InvalidHost;
    std::vector<std::uint8_t> data;
};

class ITransport
{
public:
    virtual ~ITransport() = default;
    virtual bool send(HostIndex host, PacketView packet, Reliability reliability) = 0;
    virtual void poll(std::vector<IncomingPacket>& packets) = 0;
    virtual void tick() = 0;
    virtual void shutdown() = 0;
};

ITransport* platformTransport();
void resetPlatformTransport();
void registerSteamPeer(HostIndex host, std::uint64_t steamId);
void clearSteamPeer(HostIndex host);
}
