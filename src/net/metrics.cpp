#include "metrics.hpp"

namespace barony::net
{
namespace
{
NetworkMetrics gMetrics;
}

std::uint32_t NetworkMetrics::packetId(const void* data, std::size_t size) noexcept
{
    if (!data || size < 4)
    {
        return 0;
    }
    const auto* b = static_cast<const std::uint8_t*>(data);
    return (static_cast<std::uint32_t>(b[0]) << 24)
        | (static_cast<std::uint32_t>(b[1]) << 16)
        | (static_cast<std::uint32_t>(b[2]) << 8)
        | static_cast<std::uint32_t>(b[3]);
}

void NetworkMetrics::recordSend(const void* data, std::size_t size, Reliability reliability)
{
    ++sent_.packets;
    sent_.bytes += size;
    if (reliability == Reliability::Reliable)
    {
        ++reliableSent_;
    }
    auto& counters = byPacketId_[packetId(data, size)];
    ++counters.packets;
    counters.bytes += size;
}

void NetworkMetrics::recordReceive(const void* data, std::size_t size)
{
    ++received_.packets;
    received_.bytes += size;
    auto& counters = byPacketId_[packetId(data, size)];
    ++counters.packets;
    counters.bytes += size;
}

void NetworkMetrics::recordRetry()
{
    ++retries_;
}

void NetworkMetrics::reset()
{
    sent_ = {};
    received_ = {};
    reliableSent_ = 0;
    retries_ = 0;
    byPacketId_.clear();
}

NetworkMetrics& metrics() noexcept
{
    return gMetrics;
}
}
