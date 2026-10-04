#pragma once

#include "types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace barony::net
{
struct PacketCounters
{
    std::uint64_t packets = 0;
    std::uint64_t bytes = 0;
};

class NetworkMetrics
{
public:
    void recordSend(const void* data, std::size_t size, Reliability reliability);
    void recordReceive(const void* data, std::size_t size);
    void recordRetry();

    const PacketCounters& sent() const noexcept { return sent_; }
    const PacketCounters& received() const noexcept { return received_; }
    std::uint64_t reliableSent() const noexcept { return reliableSent_; }
    std::uint64_t retries() const noexcept { return retries_; }
    const std::unordered_map<std::uint32_t, PacketCounters>& byPacketId() const noexcept { return byPacketId_; }

    void reset();

private:
    static std::uint32_t packetId(const void* data, std::size_t size) noexcept;

    PacketCounters sent_;
    PacketCounters received_;
    std::uint64_t reliableSent_ = 0;
    std::uint64_t retries_ = 0;
    std::unordered_map<std::uint32_t, PacketCounters> byPacketId_;
};

NetworkMetrics& metrics() noexcept;
}
