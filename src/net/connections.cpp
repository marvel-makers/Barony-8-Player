#include "connections.hpp"

namespace barony::net
{
ConnectionId ConnectionRegistry::upsert(HostIndex host, std::uint64_t peerIdentity)
{
    if (auto* existing = findByHost(host))
    {
        existing->peerIdentity = peerIdentity;
        return existing->id;
    }

    const auto id = nextId_++;
    Connection connection;
    connection.id = id;
    connection.host = host;
    connection.peerIdentity = peerIdentity;
    connections_.emplace(id, std::move(connection));
    byHost_[host] = id;
    return id;
}

void ConnectionRegistry::eraseByHost(HostIndex host)
{
    const auto it = byHost_.find(host);
    if (it == byHost_.end())
    {
        return;
    }
    connections_.erase(it->second);
    byHost_.erase(it);
}

void ConnectionRegistry::eraseById(ConnectionId id)
{
    const auto it = connections_.find(id);
    if (it == connections_.end())
    {
        return;
    }
    byHost_.erase(it->second.host);
    connections_.erase(it);
}

void ConnectionRegistry::clear()
{
    connections_.clear();
    byHost_.clear();
    nextId_ = 1;
}

Connection* ConnectionRegistry::findByHost(HostIndex host)
{
    const auto it = byHost_.find(host);
    return it == byHost_.end() ? nullptr : findById(it->second);
}

const Connection* ConnectionRegistry::findByHost(HostIndex host) const
{
    const auto it = byHost_.find(host);
    if (it == byHost_.end())
    {
        return nullptr;
    }
    const auto conn = connections_.find(it->second);
    return conn == connections_.end() ? nullptr : &conn->second;
}

Connection* ConnectionRegistry::findById(ConnectionId id)
{
    const auto it = connections_.find(id);
    return it == connections_.end() ? nullptr : &it->second;
}

Connection* ConnectionRegistry::findByPeer(std::uint64_t peerIdentity)
{
    for (auto& [id, connection] : connections_)
    {
        if (connection.peerIdentity == peerIdentity)
        {
            return &connection;
        }
    }
    return nullptr;
}
}
