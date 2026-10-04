#pragma once

#include "../../main.hpp"
#include "../../draw.hpp"
#include "../../game.hpp"
#include "../../stat.hpp"
#include "../../net.hpp"
#include "../../messages.hpp"
#include "../../entity.hpp"
#include "../../files.hpp"
#include "../../monster.hpp"
#include "../../interface/interface.hpp"
#include "../../magic/magic.hpp"
#include "../../engine/audio/sound.hpp"
#include "../../items.hpp"
#include "../../shops.hpp"
#include "../../menu.hpp"
#include "../../scores.hpp"
#include "../../collision.hpp"
#include "../../paths.hpp"
#ifdef STEAMWORKS
#include <steam/steam_api.h>
#include "../../steam.hpp"
#endif
#include "../../player.hpp"
#include "../../colors.hpp"
#include "../../mod_tools.hpp"
#include "../../lobbies.hpp"
#include "../../ui/MainMenu.hpp"
#include "../../ui/LoadingScreen.hpp"
#include "../../ui/GameUI.hpp"
#include "../../interface/ui.hpp"
#ifdef USE_PLAYFAB
#include "playfab.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <iterator>
#include <limits>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>




namespace barony::net
{
inline int playerCapacity() noexcept
{
    return std::size(players);
}

inline bool validPlayer(const int player) noexcept
{
    return player >= 0 && player < playerCapacity();
}

inline bool validRemotePlayer(const int player) noexcept
{
    return player > 0
        && validPlayer(player)
        && players[player]
        && !client_disconnected[player]
        && !players[player]->isLocalPlayer();
}

inline int hostIndexForPlayer(const int player) noexcept
{
    return player - 1;
}
}
