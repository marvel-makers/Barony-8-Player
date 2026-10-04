#pragma once

#include "types.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace barony::net
{
struct Connection
{
    ConnectionId id = InvalidConnection;
    HostIndex host = InvalidHost;
    std::uint64_t peerIdentity = 0;
    std::vector<PlayerId> players;
    bool connected = false;
};

class ConnectionRegistry
{
public:
    ConnectionId upsert(HostIndex host, std::uint64_t peerIdentity);
    void eraseByHost(HostIndex host);
    void eraseById(ConnectionId id);
    void clear();

    Connection* findByHost(HostIndex host);
    const Connection* findByHost(HostIndex host) const;
    Connection* findById(ConnectionId id);
    Connection* findByPeer(std::uint64_t peerIdentity);

private:
    ConnectionId nextId_ = 1;
    std::unordered_map<ConnectionId, Connection> connections_;
    std::unordered_map<HostIndex, ConnectionId> byHost_;
};
}
