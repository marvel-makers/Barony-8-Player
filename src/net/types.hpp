#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace barony::net
{
using PlayerId = std::uint16_t;
using ConnectionId = std::uint32_t;
using HostIndex = std::size_t;

constexpr PlayerId InvalidPlayer = std::numeric_limits<PlayerId>::max();
constexpr ConnectionId InvalidConnection = 0;
constexpr HostIndex InvalidHost = std::numeric_limits<HostIndex>::max();

enum class Reliability : std::uint8_t
{
    Unreliable,
    Reliable
};

enum class TrafficClass : std::uint8_t
{
    Control,
    Gameplay,
    Snapshot
};
}
