#pragma once

#include "transport.hpp"

#include <memory>

namespace barony::net
{
std::unique_ptr<ITransport> makeSteamTransport();
void setSteamPeerIdentity(HostIndex host, std::uint64_t steamId);
void clearSteamPeerIdentity(HostIndex host);
}
