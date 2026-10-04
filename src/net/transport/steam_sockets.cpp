#include "../private/common.hpp"
#include "steam_sockets.hpp"
#include "../metrics.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace barony::net
{
#ifdef STEAMWORKS
namespace
{
constexpr int kVirtualPort = 0;
std::unordered_map<HostIndex, std::uint64_t> gPeerIdentities;

std::uint64_t legacyPeerIdentity(HostIndex host)
{
    if (host >= static_cast<HostIndex>(barony::net::playerCapacity()) || !steamIDRemote[host])
    {
        return 0;
    }
    return static_cast<CSteamID*>(steamIDRemote[host])->ConvertToUint64();
}

std::uint64_t peerIdentity(HostIndex host)
{
    const auto it = gPeerIdentities.find(host);
    if (it != gPeerIdentities.end())
    {
        return it->second;
    }
    return legacyPeerIdentity(host);
}

HostIndex hostForIdentity(std::uint64_t identity)
{
    for (const auto& [host, id] : gPeerIdentities)
    {
        if (id == identity)
        {
            return host;
        }
    }
    for (HostIndex host = 0; host < static_cast<HostIndex>(barony::net::playerCapacity()); ++host)
    {
        if (legacyPeerIdentity(host) == identity)
        {
            return host;
        }
    }
    return InvalidHost;
}
}

class SteamSocketsTransport final : public ITransport
{
public:
    SteamSocketsTransport()
    {
        active_ = this;
        if (auto* utils = SteamNetworkingUtils())
        {
            utils->SetGlobalCallback_SteamNetConnectionStatusChanged(&SteamSocketsTransport::connectionStatusChanged);
        }
        ensurePollGroup();
    }

    ~SteamSocketsTransport() override
    {
        shutdown();
        if (active_ == this)
        {
            active_ = nullptr;
        }
    }

    bool send(HostIndex host, PacketView packet, Reliability reliability) override
    {
        if (!packet || !ensureConnection(host))
        {
            return false;
        }

        const auto it = byHost_.find(host);
        if (it == byHost_.end())
        {
            return false;
        }

        const int flags = reliability == Reliability::Reliable
            ? k_nSteamNetworkingSend_Reliable
            : k_nSteamNetworkingSend_UnreliableNoNagle;

        const EResult result = sockets()->SendMessageToConnection(
            it->second,
            packet.data,
            static_cast<std::uint32_t>(packet.size),
            flags,
            nullptr);

        if (result != k_EResultOK)
        {
            return false;
        }

        metrics().recordSend(packet.data, packet.size, reliability);
        return true;
    }

    void poll(std::vector<IncomingPacket>& packets) override
    {
        ensureRuntimeState();
        if (!sockets() || pollGroup_ == k_HSteamNetPollGroup_Invalid)
        {
            return;
        }

        sockets()->RunCallbacks();

        SteamNetworkingMessage_t* messages[128];
        for (;;)
        {
            const int count = sockets()->ReceiveMessagesOnPollGroup(pollGroup_, messages, 128);
            if (count <= 0)
            {
                break;
            }

            for (int i = 0; i < count; ++i)
            {
                auto* message = messages[i];
                if (!message)
                {
                    continue;
                }

                IncomingPacket packet;
                const auto userData = message->m_nConnUserData;
                packet.host = userData >= 0 ? static_cast<HostIndex>(userData) : InvalidHost;
                const auto* begin = static_cast<const std::uint8_t*>(message->m_pData);
                packet.data.assign(begin, begin + message->m_cbSize);
                metrics().recordReceive(packet.data.data(), packet.data.size());
                packets.emplace_back(std::move(packet));
                message->Release();
            }
        }
    }

    void tick() override
    {
        ensureRuntimeState();
        if (sockets())
        {
            sockets()->RunCallbacks();
        }
    }

    void shutdown() override
    {
        if (!sockets())
        {
            return;
        }

        for (const auto& [host, connection] : byHost_)
        {
            sockets()->CloseConnection(connection, 0, "network shutdown", false);
        }
        byHost_.clear();
        byConnection_.clear();

        if (listenSocket_ != k_HSteamListenSocket_Invalid)
        {
            sockets()->CloseListenSocket(listenSocket_);
            listenSocket_ = k_HSteamListenSocket_Invalid;
        }
        if (pollGroup_ != k_HSteamNetPollGroup_Invalid)
        {
            sockets()->DestroyPollGroup(pollGroup_);
            pollGroup_ = k_HSteamNetPollGroup_Invalid;
        }
    }

private:
    static SteamSocketsTransport* active_;

    static ISteamNetworkingSockets* sockets()
    {
        return SteamNetworkingSockets();
    }

    void ensureRuntimeState()
    {
        ensurePollGroup();
        if (multiplayer == SERVER)
        {
            ensureListenSocket();
        }
    }

    bool ensurePollGroup()
    {
        if (pollGroup_ != k_HSteamNetPollGroup_Invalid)
        {
            return true;
        }
        if (!sockets())
        {
            return false;
        }
        pollGroup_ = sockets()->CreatePollGroup();
        return pollGroup_ != k_HSteamNetPollGroup_Invalid;
    }

    bool ensureListenSocket()
    {
        if (listenSocket_ != k_HSteamListenSocket_Invalid)
        {
            return true;
        }
        if (!ensurePollGroup() || !sockets())
        {
            return false;
        }
        listenSocket_ = sockets()->CreateListenSocketP2P(kVirtualPort, 0, nullptr);
        return listenSocket_ != k_HSteamListenSocket_Invalid;
    }

    bool ensureConnection(HostIndex host)
    {
        ensureRuntimeState();
        if (byHost_.find(host) != byHost_.end())
        {
            return true;
        }

        const std::uint64_t steamId = peerIdentity(host);
        if (!steamId || !sockets())
        {
            return false;
        }

        SteamNetworkingIdentity identity;
        identity.Clear();
        identity.SetSteamID64(steamId);

        const HSteamNetConnection connection = sockets()->ConnectP2P(identity, kVirtualPort, 0, nullptr);
        if (connection == k_HSteamNetConnection_Invalid)
        {
            return false;
        }

        bind(host, connection);
        return true;
    }

    void bind(HostIndex host, HSteamNetConnection connection)
    {
        byHost_[host] = connection;
        byConnection_[connection] = host;
        sockets()->SetConnectionUserData(connection, static_cast<std::int64_t>(host));
        if (pollGroup_ != k_HSteamNetPollGroup_Invalid)
        {
            sockets()->SetConnectionPollGroup(connection, pollGroup_);
        }
    }

    void remove(HSteamNetConnection connection)
    {
        const auto it = byConnection_.find(connection);
        if (it == byConnection_.end())
        {
            return;
        }
        byHost_.erase(it->second);
        byConnection_.erase(it);
    }

    void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* status)
    {
        if (!status || !sockets())
        {
            return;
        }

        const auto state = status->m_info.m_eState;
        if (state == k_ESteamNetworkingConnectionState_Connecting
            && status->m_info.m_hListenSocket != k_HSteamListenSocket_Invalid)
        {
            const auto steamId = status->m_info.m_identityRemote.GetSteamID64();
            const auto host = hostForIdentity(steamId);
            if (host == InvalidHost)
            {
                sockets()->CloseConnection(status->m_hConn, 0, "unknown lobby peer", false);
                return;
            }
            if (sockets()->AcceptConnection(status->m_hConn) != k_EResultOK)
            {
                sockets()->CloseConnection(status->m_hConn, 0, "accept failed", false);
                return;
            }
            bind(host, status->m_hConn);
            return;
        }

        if (state == k_ESteamNetworkingConnectionState_Connected)
        {
            const auto existing = byConnection_.find(status->m_hConn);
            if (existing == byConnection_.end())
            {
                const auto steamId = status->m_info.m_identityRemote.GetSteamID64();
                const auto host = hostForIdentity(steamId);
                if (host != InvalidHost)
                {
                    bind(host, status->m_hConn);
                }
            }
            return;
        }

        if (state == k_ESteamNetworkingConnectionState_ClosedByPeer
            || state == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
        {
            remove(status->m_hConn);
            sockets()->CloseConnection(status->m_hConn, 0, nullptr, false);
        }
    }

    static void connectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* status)
    {
        if (active_)
        {
            active_->onConnectionStatusChanged(status);
        }
    }

    HSteamListenSocket listenSocket_ = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup pollGroup_ = k_HSteamNetPollGroup_Invalid;
    std::unordered_map<HostIndex, HSteamNetConnection> byHost_;
    std::unordered_map<HSteamNetConnection, HostIndex> byConnection_;
};

SteamSocketsTransport* SteamSocketsTransport::active_ = nullptr;
#endif

std::unique_ptr<ITransport> makeSteamTransport()
{
#ifdef STEAMWORKS
    return std::make_unique<SteamSocketsTransport>();
#else
    return {};
#endif
}

void setSteamPeerIdentity(HostIndex host, std::uint64_t steamId)
{
#ifdef STEAMWORKS
    if (steamId)
    {
        gPeerIdentities[host] = steamId;
    }
    else
    {
        gPeerIdentities.erase(host);
    }
#else
    (void)host;
    (void)steamId;
#endif
}

void clearSteamPeerIdentity(HostIndex host)
{
#ifdef STEAMWORKS
    gPeerIdentities.erase(host);
#else
    (void)host;
#endif
}
}
