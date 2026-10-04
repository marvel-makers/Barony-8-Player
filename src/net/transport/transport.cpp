#include "../private/common.hpp"
#include "transport.hpp"
#include "eos.hpp"
#include "steam_sockets.hpp"

namespace barony::net
{
namespace
{
std::unique_ptr<ITransport> gTransport;
LobbyHandler_t::LobbyServiceType gService = LobbyHandler_t::LobbyServiceType::LOBBY_DISABLE;

std::unique_ptr<ITransport> makeTransport(LobbyHandler_t::LobbyServiceType service)
{
    if (service == LobbyHandler_t::LobbyServiceType::LOBBY_STEAM)
    {
#ifdef STEAMWORKS
        return makeSteamTransport();
#else
        return {};
#endif
    }
    if (service == LobbyHandler_t::LobbyServiceType::LOBBY_CROSSPLAY)
    {
#ifdef USE_EOS
        return makeEosTransport();
#else
        return {};
#endif
    }
    return {};
}
}

ITransport* platformTransport()
{
    if (directConnect)
    {
        return nullptr;
    }

    const auto service = LobbyHandler.getP2PType();
    if (!gTransport || service != gService)
    {
        if (gTransport)
        {
            gTransport->shutdown();
        }
        gService = service;
        gTransport = makeTransport(service);
    }
    return gTransport.get();
}

void resetPlatformTransport()
{
    if (gTransport)
    {
        gTransport->shutdown();
        gTransport.reset();
    }
    gService = LobbyHandler_t::LobbyServiceType::LOBBY_DISABLE;
}

void registerSteamPeer(HostIndex host, std::uint64_t steamId)
{
#ifdef STEAMWORKS
    setSteamPeerIdentity(host, steamId);
#else
    (void)host;
    (void)steamId;
#endif
}

void clearSteamPeer(HostIndex host)
{
#ifdef STEAMWORKS
    clearSteamPeerIdentity(host);
#else
    (void)host;
#endif
}
}
