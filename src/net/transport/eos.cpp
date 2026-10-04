#include "../private/common.hpp"
#include "eos.hpp"
#include "../metrics.hpp"

namespace barony::net
{
#ifdef USE_EOS
class EosTransport final : public ITransport
{
public:
    bool send(HostIndex host, PacketView packet, Reliability reliability) override
    {
        if (!packet || host > static_cast<HostIndex>(std::numeric_limits<int>::max()))
        {
            return false;
        }
        auto peer = EOS.P2PConnectionInfo.getPeerIdFromIndex(static_cast<int>(host));
        if (!peer)
        {
            return false;
        }
        EOS.SendMessageP2P(peer, packet.data, static_cast<int>(packet.size), reliability == Reliability::Reliable);
        metrics().recordSend(packet.data, packet.size, reliability);
        return true;
    }

    void poll(std::vector<IncomingPacket>& packets) override
    {
        if (!EOS.CurrentUserInfo.isValid())
        {
            return;
        }

        EOS_ProductUserId remoteId = nullptr;
        while (EOS.HandleReceivedMessages(&remoteId))
        {
            if (!remoteId || !net_packet || net_packet->len <= 0 || !net_packet->data[0])
            {
                continue;
            }

            IncomingPacket packet;
            packet.data.assign(net_packet->data, net_packet->data + std::min<int>(net_packet->len, NET_PACKET_SIZE));
            metrics().recordReceive(packet.data.data(), packet.data.size());
            packets.emplace_back(std::move(packet));
        }
    }

    void tick() override
    {
        if (EOS.PlatformHandle)
        {
            EOS_Platform_Tick(EOS.PlatformHandle);
        }
        if (EOS.ServerPlatformHandle)
        {
            EOS_Platform_Tick(EOS.ServerPlatformHandle);
        }
    }

    void shutdown() override {}
};
#endif

std::unique_ptr<ITransport> makeEosTransport()
{
#ifdef USE_EOS
    return std::make_unique<EosTransport>();
#else
    return {};
#endif
}
}
