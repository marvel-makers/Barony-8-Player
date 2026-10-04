#include "../private/common.hpp"
#include <type_traits>
#include "../transport/transport.hpp"


namespace
{
template <typename Fn>
void useItemServerCompat(Fn fn, Item* item, int client)
{
    if constexpr (std::is_invocable_v<Fn, Item*, int, Entity*, bool, bool>)
    {
        fn(item, client, nullptr, false, true);
    }
    else
    {
        fn(item, client, nullptr, false);
    }
}
}

static std::unordered_map<Uint32, void(*)()> serverPacketHandlers = {
	// keep alive
	{'KPAL', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
	}},

	// ping
	{'PING', [](){
		const int j = net_packet->data[4];
		if (j <= 0 || !barony::net::validPlayer(j) )
		{
			return;
		}
		if ( client_disconnected[j] || players[j]->isLocalPlayer() )
		{
			return;
		}
		memcpy(net_packet->data, "PING", 4);
		net_packet->address.host = net_clients[j - 1].host;
		net_packet->address.port = net_clients[j - 1].port;
		net_packet->len = 5;
		sendPacketSafe(net_sock, -1, net_packet, j - 1);
	}},

	// automated ping
	{'PNGU', []() {
		PingNetworkStatus_t::respond();
	}},

	// automated ping response
	{'PNGR', []() {
		PingNetworkStatus_t::receive();
	}},

	// network scan
	{'SCAN', [](){
	    handleScanPacket();
	}},

	// pause game
	{'PAUS', [](){
		messagePlayer(clientnum, MESSAGE_MISC, Language::get(1118), stats[net_packet->data[4]]->name);
		const int j = net_packet->data[4];
		if (!barony::net::validPlayer(j))
		{
			return;
		}
		pauseGame(2, j);
	}},

	// unpause game
	{'UNPS', [](){
		messagePlayer(clientnum, MESSAGE_MISC, Language::get(1119), stats[net_packet->data[4]]->name);
		const int j = net_packet->data[4];
		if (!barony::net::validPlayer(j))
		{
			return;
		}
		pauseGame(1, j);
	}},

	// check entity existence
	{'ENTE', [](){
		const int x = net_packet->data[4];
		if ( x <= 0 || !barony::net::validPlayer(x) )
		{
			return;
		}
		if ( players[x]->isLocalPlayer() )
		{
			return;
		}
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			return; // found entity.
		}
		// else reply with entity deleted.
		strcpy((char*)net_packet->data, "ENTD");
		SDLNet_Write32(uid, &net_packet->data[4]);
		net_packet->address.host = net_clients[x - 1].host;
		net_packet->address.port = net_clients[x - 1].port;
		net_packet->len = 8;
		sendPacketSafe(net_sock, -1, net_packet, x - 1);
	}},

	// client request item details.
	{'ITMU', [](){
		const int x = net_packet->data[4];
		if ( x <= 0 || !barony::net::validPlayer(x) )
		{
			return;
		}
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			strcpy((char*)net_packet->data, "ITMU");
			SDLNet_Write32(uid, &net_packet->data[4]);

			Uint32 itemTypeAndIdentified = ((static_cast<Uint16>(entity->skill[10]) & 0xFFFF) << 16); // type
			itemTypeAndIdentified |= (static_cast<Uint16>(entity->skill[15]) & 0xFFFF); // identified

			SDLNet_Write32(itemTypeAndIdentified, &net_packet->data[8]);

			Uint32 statusBeatitudeQuantityAppearance = 0;
			statusBeatitudeQuantityAppearance |= ((static_cast<Uint8>(entity->skill[11]) & 0xFF) << 24); // status
			statusBeatitudeQuantityAppearance |= ((static_cast<Sint8>(entity->skill[12]) & 0xFF) << 16); // beatitude
			statusBeatitudeQuantityAppearance |= ((static_cast<Uint8>(entity->skill[13]) & 0xFF) << 8); // quantity
			Uint8 appearance = entity->skill[14] % items[entity->skill[10]].variations;
			if ( entity->skill[10] == TOOL_PLAYER_LOOT_BAG )
			{
				appearance = (entity->skill[14] & 0xF) % items[entity->skill[10]].variations;
			}
			else if ( entity->skill[10] == ENCHANTED_FEATHER )
			{
				appearance = entity->skill[14] % ENCHANTED_FEATHER_MAX_DURABILITY;
			}
			else if ( entity->skill[10] == MAGICSTAFF_SCEPTER )
			{
				appearance = entity->skill[14] % MAGICSTAFF_SCEPTER_CHARGE_MAX;
			}
			statusBeatitudeQuantityAppearance |= (appearance & 0xFF); // appearance
			SDLNet_Write32(statusBeatitudeQuantityAppearance, &net_packet->data[12]);

			net_packet->len = 16;
			if ( entity->skill[10] >= 0 && entity->skill[10] < NUMITEMS )
			{
				if ( items[entity->skill[10]].category == TOME_SPELL )
				{
					SDLNet_Write16(entity->skill[14] % TOME_APPEARANCE_MAX, &net_packet->data[16]);
					net_packet->len = 18;
				}
			}

			net_packet->address.host = net_clients[x - 1].host;
			net_packet->address.port = net_clients[x - 1].port;
			sendPacketSafe(net_sock, -1, net_packet, x - 1);
		}
	}},

	// player move
	{'PMOV', [](){
		const int player = net_packet->data[4];
		if ( player < 0 || !barony::net::validPlayer(player) )
		{
			return;
		}
		client_keepalive[player] = ticks;
		if (players[player] == nullptr || players[player]->entity == nullptr)
		{
			return;
		}

		// check if the info is outdated
		if ( net_packet->data[5] != currentlevel || net_packet->data[18] != secretlevel )
		{
			return;
		}

		// get info from client
		auto dx = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6])) / 32.0;
		auto dy = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8])) / 32.0;
		auto velx = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10])) / 128.0;
		auto vely = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[12])) / 128.0;
		auto yaw = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[14])) / 128.0;
		auto pitch = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[16])) / 128.0;

		// update rotation
		players[player]->entity->yaw = yaw;
		players[player]->entity->pitch = pitch;

		// update player's internal velocity variables
		players[player]->entity->vel_x = velx; // PLAYER_VELX
		players[player]->entity->vel_y = vely; // PLAYER_VELY

		// store old coordinates
		// since this function runs more often than actPlayer runs, we need to keep track of the accumulated position in new_x/new_y
		real_t ox = players[player]->entity->x;
		real_t oy = players[player]->entity->y;
		players[player]->entity->x = players[player]->entity->new_x;
		players[player]->entity->y = players[player]->entity->new_y;

		// calculate distance
		dx -= players[player]->entity->x;
		dy -= players[player]->entity->y;
		auto dist = sqrt( dx * dx + dy * dy );

		// move player with collision detection
		real_t result = clipMove(&players[player]->entity->x, &players[player]->entity->y, dx, dy, players[player]->entity);
		if ( result < dist - .025 )
		{
			// player encountered obstacle on path
			// stop updating position on server side and send client corrected position
			const int j = net_packet->data[4];
			if ( j > 0 && j < barony::net::playerCapacity() )
			{
				strcpy((char*)net_packet->data, "PMOV");
				SDLNet_Write16(static_cast<Sint16>(players[j]->entity->x * 32), &net_packet->data[4]);
				SDLNet_Write16(static_cast<Sint16>(players[j]->entity->y * 32), &net_packet->data[6]);
				net_packet->address.host = net_clients[j - 1].host;
				net_packet->address.port = net_clients[j - 1].port;
				net_packet->len = 8;
				sendPacket(net_sock, -1, net_packet, j - 1);
			}
		}

		// clipMove sent any corrections to the client, now let's save the updated coordinates.
		players[player]->entity->new_x = players[player]->entity->x;
		players[player]->entity->new_y = players[player]->entity->y;
		// return x/y to their original state as this can update more than actPlayer and causes stuttering. use new_x/new_y in actPlayer.
		players[player]->entity->x = ox;
		players[player]->entity->y = oy;

		// update the players' head and mask as these will otherwise wait until actPlayer to update their rotation. stops clipping.
		node_t* tmpNode = nullptr;
		int bodypartNum = 0;
		for ( bodypartNum = 0, tmpNode = players[player]->entity->children.first; tmpNode; tmpNode = tmpNode->next, bodypartNum++ )
		{
			if ( bodypartNum == 0 )
			{
				// hudweapon case
				continue;
			}

			auto limb = static_cast<Entity*>(tmpNode->element);
			if ( limb )
			{
				// adjust headgear/mask yaw/pitch variations as these do not update always.
				if ( bodypartNum == 9 || bodypartNum == 10 )
				{
					limb->x = players[player]->entity->x;
					limb->y = players[player]->entity->y;
					limb->pitch = players[player]->entity->pitch;
					limb->yaw = players[player]->entity->yaw;
				}
			}
		}
	}},

	// player ghost move
	{'GMOV', []() {
		const int player = net_packet->data[4];
		if ( player < 0 || !barony::net::validPlayer(player) )
		{
			return;
		}
		client_keepalive[player] = ticks;
		if ( players[player] == nullptr || players[player]->ghost.my == nullptr )
		{
			return;
		}

		// check if the info is outdated
		if ( net_packet->data[5] != currentlevel || net_packet->data[18] != secretlevel )
		{
			return;
		}

		// get info from client
		auto dx = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6])) / 32.0;
		auto dy = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8])) / 32.0;
		auto velx = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10])) / 128.0;
		auto vely = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[12])) / 128.0;
		auto yaw = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[14])) / 128.0;
		auto pitch = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[16])) / 128.0;
		bool bounce = ((net_packet->data[19] & 1) == 1) ? true : false;
		int deactivated = (((net_packet->data[19] >> 1) & 1) == 1) ? 1 : 0;

		// update rotation
		players[player]->ghost.my->yaw = yaw;
		players[player]->ghost.my->pitch = pitch;

		// update player's internal velocity variables
		players[player]->ghost.my->vel_x = velx; // PLAYER_VELX
		players[player]->ghost.my->vel_y = vely; // PLAYER_VELY

		// store old coordinates
		// since this function runs more often than actPlayer runs, we need to keep track of the accumulated position in new_x/new_y
		real_t ox = players[player]->ghost.my->x;
		real_t oy = players[player]->ghost.my->y;
		players[player]->ghost.my->x = players[player]->ghost.my->new_x;
		players[player]->ghost.my->y = players[player]->ghost.my->new_y;

		// calculate distance
		dx -= players[player]->ghost.my->x;
		dy -= players[player]->ghost.my->y;
		auto dist = sqrt(dx * dx + dy * dy);

		// move player with collision detection
		real_t result = clipMove(&players[player]->ghost.my->x, &players[player]->ghost.my->y, dx, dy, players[player]->ghost.my);
		if ( result < dist - .025 )
		{
			// player encountered obstacle on path
			// stop updating position on server side and send client corrected position
			const int j = net_packet->data[4];
			if ( j > 0 && j < barony::net::playerCapacity() )
			{
				strcpy((char*)net_packet->data, "GMOV");
				SDLNet_Write16(static_cast<Sint16>(players[j]->ghost.my->x * 32), &net_packet->data[4]);
				SDLNet_Write16(static_cast<Sint16>(players[j]->ghost.my->y * 32), &net_packet->data[6]);
				net_packet->address.host = net_clients[j - 1].host;
				net_packet->address.port = net_clients[j - 1].port;
				net_packet->len = 8;
				sendPacket(net_sock, -1, net_packet, j - 1);
			}
		}

		// clipMove sent any corrections to the client, now let's save the updated coordinates.
		players[player]->ghost.my->new_x = players[player]->ghost.my->x;
		players[player]->ghost.my->new_y = players[player]->ghost.my->y;
		// return x/y to their original state as this can update more than actPlayer and causes stuttering. use new_x/new_y in actPlayer.
		players[player]->ghost.my->x = ox;
		players[player]->ghost.my->y = oy;

		if ( bounce )
		{
			players[player]->ghost.my->fskill[9] = Player::Ghost_t::GHOST_SQUISH_START_ANGLE / 100.f;
			playSoundEntityLocal(players[player]->ghost.my, 612 + local_rng.rand() % 3, 64);
			for ( int c = 1; c < barony::net::playerCapacity(); ++c ) // send to other players
			{
				if ( c == player || client_disconnected[c] || players[c]->isLocalPlayer() )
				{
					continue;
				}
				strcpy((char*)net_packet->data, "GHFS");
				SDLNet_Write32(players[player]->ghost.my->getUID(), &net_packet->data[4]);
				net_packet->data[8] = 9;
				SDLNet_Write16(static_cast<Sint16>(players[player]->ghost.my->fskill[9] * 256), &net_packet->data[9]);
				net_packet->address.host = net_clients[c - 1].host;
				net_packet->address.port = net_clients[c - 1].port;
				net_packet->len = 11;
				sendPacketSafe(net_sock, -1, net_packet, c - 1);
			}
		}
		if ( deactivated != players[player]->ghost.my->skill[7] )
		{
			players[player]->ghost.setActive(deactivated == 0 ? true : false);
			for ( int c = 1; c < barony::net::playerCapacity(); ++c ) // send to other players
			{
				if ( c == player || client_disconnected[c] || players[c]->isLocalPlayer() )
				{
					continue;
				}
				strcpy((char*)net_packet->data, "ENTS");
				SDLNet_Write32(players[player]->ghost.my->getUID(), &net_packet->data[4]);
				net_packet->data[8] = 7;
				SDLNet_Write32(players[player]->ghost.my->skill[7], &net_packet->data[9]);
				net_packet->address.host = net_clients[c - 1].host;
				net_packet->address.port = net_clients[c - 1].port;
				net_packet->len = 13;
				sendPacketSafe(net_sock, -1, net_packet, c - 1);
			}
		}
	}},

	{'REZZ', []() {
		// check if the info is outdated
		if ( net_packet->data[5] != currentlevel || net_packet->data[10] != secretlevel )
		{
			return;
		}

		int player = net_packet->data[4];

		if ( player < 0 || !barony::net::validPlayer(player) )
		{
			return;
		}

		if ( players[player]->entity )
		{
			return;
		}

		int x = SDLNet_Read16(&net_packet->data[6]);
		int y = SDLNet_Read16(&net_packet->data[8]);

		if ( players[player]->ghost.my )
		{
			list_RemoveNode(players[player]->ghost.my->mynode);
			players[player]->ghost.my = nullptr;
		}
		players[player]->ghost.reset();

		Entity* entity = newEntity(113, 1, map.entities, nullptr); //Player entity.
		entity->x = (x * 16) + 8;
		entity->y = (y * 16) + 8;
		entity->new_x = entity->x;
		entity->new_y = entity->y;
		entity->z = -1;
		entity->flags[INVISIBLE] = false;
		entity->flags[GENIUS] = true;
		entity->behavior = &actPlayer;
		entity->skill[2] = player;
		entity->yaw = 0.0;
		entity->sizex = 4;
		entity->sizey = 4;
		entity->focalx = limbs[HUMAN][0][0]; // 0
		entity->focaly = limbs[HUMAN][0][1]; // 0
		entity->focalz = limbs[HUMAN][0][2]; // -1.5
		entity->flags[UPDATENEEDED] = true;
		entity->flags[BLOCKSIGHT] = true;
		entity->addToCreatureList(map.creatures);
		players[player]->entity = entity;
		stats[player]->HP = stats[player]->MAXHP / 2;
	}},

	// player created ghost
	{'GHOS', []() {
		// check if the info is outdated
		if ( net_packet->data[5] != currentlevel || net_packet->data[10] != secretlevel )
		{
			return;
		}
		
		int player = net_packet->data[4];

		if ( player < 0 || !barony::net::validPlayer(player) )
		{
			return;
		}

		int x = SDLNet_Read16(&net_packet->data[6]);
		int y = SDLNet_Read16(&net_packet->data[8]);

		if ( players[player]->ghost.my )
		{
			list_RemoveNode(players[player]->ghost.my->mynode);
			players[player]->ghost.my = nullptr;
		}
		players[player]->ghost.reset();

		// deathcam
		int sprite = Player::Ghost_t::getSpriteForPlayer(player);
		Entity* entity = newEntity(sprite, 1, map.entities, nullptr); //Ghost entity.
		players[player]->ghost.my = entity;
		players[player]->ghost.uid = entity->getUID();
		entity->x = (x * 16) + 8;
		entity->y = (y * 16) + 8;
		entity->new_x = entity->x;
		entity->new_y = entity->y;
		entity->z = -4;
		entity->flags[PASSABLE] = true;
		entity->flags[INVISIBLE] = true;
		entity->flags[GENIUS] = true;
		entity->behavior = &actDeathGhost;
		entity->skill[2] = player;
		entity->yaw = 0.0;
		entity->pitch = PI / 16;
		entity->sizex = 2;
		entity->sizey = 2;
		entity->flags[UPDATENEEDED] = true;
		Compendium_t::Events_t::eventUpdateMonster(player, Compendium_t::CPDM_GHOST_SPAWNED, entity, 1);
	}},

	// tried to update
	{'NOUP', [](){
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			entity->flags[UPDATENEEDED] = false;
		}
	}},

	// client deleted entity
	/*{'ENTD', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		for ( auto node = entitiesToDelete[player].first; node != NULL; node = node->next )
		{
			auto deleteent = (deleteent_t*)node->element;
			if ( deleteent->uid == SDLNet_Read32(&net_packet->data[5]) )
			{
				list_RemoveNode(node);
				break;
			}
		}
	}},*/

	// clicked entity in range
	{'CKIR', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			client_selected[player] = entity;
			inrange[player] = true;
		}
	}},

	// tinker salvage
	{'SALV', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			if ( (entity->behavior == &actItem || entity->behavior == &actTorch || entity->behavior == &actCrystalShard) )
			{
				// auto salvage this item.
				if ( players[player] && players[player]->entity )
				{
					entity->itemAutoSalvageByPlayer = static_cast<Sint32>(players[player]->entity->getUID());
				}
			}
			client_selected[player] = entity;
			inrange[player] = true;
		}
	}},

	// clicked wall lock entity in range with key
	{'LKEY', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity && entity->behavior == &actWallLock )
		{
			if ( players[player]->entity )
			{
				client_selected[player] = entity;
				inrange[player] = true;
				if ( entity->wallLockState == Entity::WallLockStates::LOCK_NO_KEY )
				{
					if ( entity->wallLockPlayerInteracting == 0 )
					{
						entity->wallLockPlayerInteracting = players[player]->entity->getUID();
					}
					else if ( entity->wallLockPlayerInteracting == players[player]->entity->getUID() )
					{
						// client has already queued up an action, drop this interaction
						client_selected[player] = nullptr;
						inrange[player] = false;
					}
				}
			}
		}
	}},

	// clicked wall lock entity in range without key
	{'LNOK', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity && entity->behavior == &actWallLock )
		{
			if ( players[player]->entity )
			{
				client_selected[player] = entity;
				inrange[player] = true;
				if ( entity->wallLockState == Entity::WallLockStates::LOCK_NO_KEY )
				{
					if ( entity->wallLockPlayerInteracting == players[player]->entity->getUID() )
					{
						// client has already queued up an action, drop this interaction
						client_selected[player] = nullptr;
						inrange[player] = false;
					}
				}
			}
		}
	}},

	// client checked valid key for the lock
	{ 'OKEY', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity && entity->behavior == &actWallLock )
		{
			if ( entity->wallLockState == Entity::WallLockStates::LOCK_NO_KEY && net_packet->data[9] != 0 ) // success from client
			{
				Uint16 key = SDLNet_Read16(&net_packet->data[10]);
				if ( key >= WOODEN_SHIELD && key < NUMITEMS )
				{
					messagePlayer(player, MESSAGE_INTERACTION, Language::get(6378), items[key].getIdentifiedName());
					Compendium_t::Events_t::eventUpdateWorld(player, Compendium_t::CPDM_KEYLOCK_UNLOCKED_KEY, "wall locks", 1);
					Compendium_t::Events_t::eventUpdate(player, Compendium_t::CPDM_KEYLOCK_UNLOCKED_KEY, static_cast<ItemType>(key), 1);
					if ( key == KEY_IRON )
					{
						Compendium_t::Events_t::eventUpdateWorld(player, Compendium_t::CPDM_KEYLOCK_UNLOCKED_KEY_IRON, "wall locks", 1);
					}
					else if ( key == KEY_SILVER )
					{
						Compendium_t::Events_t::eventUpdateWorld(player, Compendium_t::CPDM_KEYLOCK_UNLOCKED_KEY_SILVER, "wall locks", 1);
						steamStatisticUpdateClient(player, STEAM_STAT_PREMIUM_LOOTBOX, STEAM_STAT_INT, 1);
					}
					else if ( key == KEY_GOLD )
					{
						Compendium_t::Events_t::eventUpdateWorld(player, Compendium_t::CPDM_KEYLOCK_UNLOCKED_KEY_GOLD, "wall locks", 1);
						steamStatisticUpdateClient(player, STEAM_STAT_PREMIUM_LOOTBOX, STEAM_STAT_INT, 1);
					}
					else if ( key == KEY_BRONZE )
					{
						Compendium_t::Events_t::eventUpdateWorld(player, Compendium_t::CPDM_KEYLOCK_UNLOCKED_KEY_BRONZE, "wall locks", 1);
						steamStatisticUpdateClient(player, STEAM_STAT_PREMIUM_LOOTBOX, STEAM_STAT_INT, 1);
					}
				}

				entity->wallLockState = Entity::WallLockStates::LOCK_KEY_START;
				serverUpdateEntitySkill(entity, 0);
			}
			else if ( entity->wallLockState == Entity::WallLockStates::LOCK_NO_KEY && net_packet->data[9] == 0 )
			{
				messagePlayer(player, MESSAGE_INTERACTION, Language::get(6379));
				playSoundEntity(entity, 152, 64);
			}
			entity->wallLockClientInteractDelay = 0;
			entity->wallLockPlayerInteracting = 0;
		}
	}},

	// rat feed
	{'RATF', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			achievementObserver.playerAchievements[player].rat5000secondRule.insert(uid);
			client_selected[player] = entity;
			inrange[player] = true;
		}
	}},

	// clicked entity out of range
	{'CKOR', [](){
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			client_selected[net_packet->data[4]] = entity;
			inrange[net_packet->data[4]] = false;
		}
	}},

	// disconnect
	{'DISC', [](){
	    // TODO verify packet origin
		char shortname[32];
		const int playerDisconnected = net_packet->data[4];
		if (!barony::net::validPlayer(playerDisconnected))
		{
			return;
		}
	    if (playerDisconnected == 0) {
	        // yeah right
	        return;
	    }
		stringCopy(shortname, stats[playerDisconnected]->name, sizeof(shortname), sizeof(Stat::name));
		client_disconnected[playerDisconnected] = true;
		for ( int c = 1; c < barony::net::playerCapacity(); c++ )
		{
			if ( client_disconnected[c] == true )
			{
				continue;
			}
			memcpy(net_packet->data, "DISC", 4);
			net_packet->data[4] = playerDisconnected;
			net_packet->address.host = net_clients[c - 1].host;
			net_packet->address.port = net_clients[c - 1].port;
			net_packet->len = 5;
			sendPacketSafe(net_sock, -1, net_packet, c - 1);
			messagePlayer(c, MESSAGE_MISC, Language::get(1120), shortname);
		}
		messagePlayer(clientnum, MESSAGE_MISC, Language::get(1120), shortname);
	}},

	// client callout
	{'CALL', []() {
		const int pnum = net_packet->data[4];
		if (!barony::net::validPlayer(pnum))
		{
			return;
		}
		if ( pnum != clientnum )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			Entity* entity = uidToEntity(uid);
			if ( uid != 0 )
			{
				if ( !entity )
				{
					return;
				}
			}
			CalloutMenu[pnum].lockOnEntityUid = uid;
			auto cmd = static_cast<CalloutRadialMenu::CalloutCommand>(net_packet->data[9]);
			CalloutMenu[pnum].clientCalloutHelpFlags = SDLNet_Read32(&net_packet->data[10]);
			if ( uid == 0 )
			{
				real_t x = SDLNet_Read16(&net_packet->data[14]);
				real_t y = SDLNet_Read16(&net_packet->data[16]);
				if ( CalloutMenu[pnum].createParticleCallout(x * 16.0 + 8.0, y * 16.0 + 8.0, -4, 0, cmd) )
				{
					CalloutMenu[pnum].sendCalloutText(cmd);
				}
			}
			else
			{
				Uint32 overrideUID = 0;
				if ( entity && (entity->behavior == &actPlayer || entity->behavior == &actDeathGhost) && entity->skill[2] != pnum )
				{
					if ( cmd == CalloutRadialMenu::CALLOUT_CMD_AFFIRMATIVE
						|| cmd == CalloutRadialMenu::CALLOUT_CMD_THANKS )
					{
						entity = Player::getPlayerInteractEntity(pnum);
						overrideUID = CalloutMenu[pnum].lockOnEntityUid;
					}
					else if ( cmd == CalloutRadialMenu::CALLOUT_CMD_LOOK
						|| cmd == CalloutRadialMenu::CALLOUT_CMD_NEGATIVE )
					{
						entity = Player::getPlayerInteractEntity(pnum);
					}
					else if ( cmd == CalloutRadialMenu::CALLOUT_CMD_SOUTH
						|| cmd == CalloutRadialMenu::CALLOUT_CMD_SOUTHWEST
						|| cmd == CalloutRadialMenu::CALLOUT_CMD_SOUTHEAST )
					{
						int toPlayer = CalloutMenu[pnum].getPlayerForDirectPlayerCmd(pnum, cmd);
						if ( toPlayer >= 0 )
						{
							entity = Player::getPlayerInteractEntity(pnum);
						}
					}
				}
				if ( CalloutMenu[pnum].createParticleCallout(entity, cmd, overrideUID) )
				{
					CalloutMenu[pnum].sendCalloutText(cmd);
				}
			}
		}
	}},

	// message
	{'MSGS', [](){
		const int pnum = net_packet->data[4];
		if (!barony::net::validPlayer(pnum))
		{
			return;
		}
		client_keepalive[pnum] = ticks;
		Uint32 color = SDLNet_Read32(&net_packet->data[5]);
		MessageType type = MESSAGE_CHAT; // the only kind of message you can get from a client.

		char shortname[32];
		stringCopy(shortname, stats[pnum]->name, sizeof(shortname), 22);

		char fmt[1024];
		const int len = snprintf(fmt, sizeof(fmt), "%s: %s", shortname, (char*)(&net_packet->data[9]));
		messagePlayerColor(clientnum, type, color, fmt);

		playSound(Message::CHAT_MESSAGE_SFX, 64);

		// relay message to all clients
		for ( int c = 1; c < barony::net::playerCapacity(); c++ )
		{
			if ( c == pnum || client_disconnected[c] == true || players[c]->isLocalPlayer() )
			{
				continue;
			}
			memcpy(net_packet->data, "MSGS", 4);
			SDLNet_Write32(color, &net_packet->data[4]);
			SDLNet_Write32(type, &net_packet->data[8]);
			stringCopy((char*)(&net_packet->data[12]), fmt, len + 1, sizeof(fmt));
			net_packet->address.host = net_clients[c - 1].host;
			net_packet->address.port = net_clients[c - 1].port;
			net_packet->len = 12 + len + 1;
			sendPacketSafe(net_sock, -1, net_packet, c - 1);
		}
	}},

	// spotting (examining)
	{'SPOT', [](){
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			clickDescription(player, entity);
		}
	}},

	// item drop
	{'DROP', [](){
	    const int player = net_packet->data[25];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		auto item = newItem(static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		    &stats[player]->inventory);
		dropItem(item, player);
	}},

	// item drop (greasy)
	{ 'GRES', []() {
		const int player = net_packet->data[25];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		auto item = newItem(static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
			static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
			SDLNet_Read32(&net_packet->data[12]),
			SDLNet_Read32(&net_packet->data[16]),
			SDLNet_Read32(&net_packet->data[20]),
			net_packet->data[24],
			&stats[player]->inventory);
		playerGreasyDropItem(player, item);
		if ( net_packet->data[26] == 1 )
		{
			// shield
			if ( stats[player]->shield )
			{
				if ( stats[player]->shield->node )
				{
					list_RemoveNode(stats[player]->shield->node);
				}
				else
				{
					free(stats[player]->shield);
				}
				stats[player]->shield = nullptr;
			}
		}
		else if ( net_packet->data[26] == 0 )
		{
			// weapon
			if ( stats[player]->weapon )
			{
				if ( stats[player]->weapon->node )
				{
					list_RemoveNode(stats[player]->weapon->node);
				}
				else
				{
					free(stats[player]->weapon);
				}
				stats[player]->weapon = nullptr;
			}
		}
	} },

	// duck throw
	{ 'DCKA', []() {
		const int player = net_packet->data[25];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_keepalive[player] = ticks;
		auto item = newItem(static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
			static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
			SDLNet_Read32(&net_packet->data[12]),
			SDLNet_Read32(&net_packet->data[16]),
			SDLNet_Read32(&net_packet->data[20]),
			net_packet->data[24],
			&stats[player]->inventory);
		playerThrowDuck(player, item, net_packet->data[26]);
		// shield
		if ( stats[player]->shield && stats[player]->shield->type == TOOL_DUCK )
		{
			if ( stats[player]->shield->node )
			{
				list_RemoveNode(stats[player]->shield->node);
			}
			else
			{
				free(stats[player]->shield);
			}
			stats[player]->shield = nullptr;
		}
	} },

	// item drop (on death)
	{'DIEI', [](){
	    const int player = net_packet->data[25];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		    nullptr);

		real_t x = net_packet->data[26];
		x = (x * 16) + 8;
		real_t y = net_packet->data[27];
		y = (y * 16) + 8;

		stats[0]->addItemToLootingBag(player, x, y, *item);
		if ( item->node )
		{
			list_RemoveNode(item->node);
		}
		else
		{
			free(item);
		}
		//{
		//	auto entity = newEntity(-1, 1, map.entities, nullptr); //Item entity.
		//	entity->x = net_packet->data[26];
		//	entity->x = entity->x * 16 + 8;
		//	entity->y = net_packet->data[27];
		//	entity->y = entity->y * 16 + 8;
		//	entity->flags[NOUPDATE] = true;
		//	entity->flags[PASSABLE] = true;
		//	entity->flags[INVISIBLE] = true;
		//	for ( int c = item->count; c > 0; c-- )
		//	{
		//		int qtyToDrop = 1;
		//		if ( c >= 10 && (item->type == TOOL_METAL_SCRAP || item->type == TOOL_MAGIC_SCRAP) )
		//		{
		//			qtyToDrop = 10;
		//			c -= 9;
		//		}
		//		else if ( itemTypeIsQuiver(item->type) )
		//		{
		//			qtyToDrop = item->count;
		//			c -= item->count;
		//		}
		//		dropItemMonster(item, entity, stats[player], qtyToDrop);
		//	}
		//	list_RemoveNode(entity->mynode);
		//}
	}},

	// raise/lower shield
	{'SHLD', [](){
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		stats[player]->defending = net_packet->data[5];
        for (int c = 1; c < barony::net::playerCapacity(); ++c) {
            // relay packet to other players
            if (client_disconnected[c] || c == player) {
                continue;
            }
            net_packet->address.host = net_clients[c - 1].host;
            net_packet->address.port = net_clients[c - 1].port;
            sendPacketSafe(net_sock, -1, net_packet, c - 1);
        }
	}},

	// sneaking
	{'SNEK', [](){
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		stats[player]->sneaking = net_packet->data[5];
        for (int c = 1; c < barony::net::playerCapacity(); ++c) {
            // relay packet to other players
            if (client_disconnected[c] || c == player) {
                continue;
            }
            net_packet->address.host = net_clients[c - 1].host;
            net_packet->address.port = net_clients[c - 1].port;
            sendPacketSafe(net_sock, -1, net_packet, c - 1);
        }
	}},

	// ghost sneaking
	{ 'GHOD', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		if ( players[player]->ghost.my )
		{
			players[player]->ghost.my->skill[3] = net_packet->data[5] & (1 << 0);
			players[player]->ghost.my->skill[11] = net_packet->data[5] & (1 << 1) ? 1 : 0;
			for ( int c = 1; c < barony::net::playerCapacity(); ++c ) {
				// relay packet to other players
				if ( client_disconnected[c] || c == player ) {
					continue;
				}
				net_packet->address.host = net_clients[c - 1].host;
				net_packet->address.port = net_clients[c - 1].port;
				sendPacketSafe(net_sock, -1, net_packet, c - 1);
			}
		}
	}},

	// close shop
	{'SHPC', [](){
		Entity* entity = uidToEntity(SDLNet_Read32(&net_packet->data[4]));
		if ( entity )
		{
			entity->skill[0] = 0;
			monsterMoveAside(entity, uidToEntity(entity->skill[1]));
			entity->skill[1] = 0;
		}
		return;
	}},

	// buy item from shop
	{'SHPB', [](){
		Uint32 uidnum = SDLNet_Read32(&net_packet->data[4]);
		const int client = net_packet->data[29];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		Entity* entity = uidToEntity(uidnum);
		if ( !entity )
		{
			printlog("[Shops]: warning: client %d bought item from non-existent shop! (uid=%d)\n", client, uidnum);
			return;
		}
		Stat* entitystats = entity->getStats();
		if ( !entitystats )
		{
			printlog("[Shops]: warning: client %d bought item from a \"shop\" that has no stats! (uid=%d)\n", client, uidnum);
			return;
		}
		Item* item = newItem(WOODEN_SHIELD, BROKEN, 0, 1, 0, true, nullptr);
		item->type = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[8]));
		item->status = static_cast<Status>(SDLNet_Read32(&net_packet->data[12]));
		item->beatitude = SDLNet_Read16(&net_packet->data[16]);
		item->appearance = SDLNet_Read32(&net_packet->data[20]);
		item->count = SDLNet_Read32(&net_packet->data[24]);
		item->identified = false;
		if ( net_packet->data[28] & 1 )
		{
			item->identified = true;
		}
		item->playerSoldItemToShop = false;
		if ( (net_packet->data[28] >> 4) & 1 )
		{
			item->playerSoldItemToShop = true;
		}
		item->x = static_cast<Sint8>(net_packet->data[18]);
		item->y = static_cast<Sint8>(net_packet->data[19]);
		node_t* nextnode;
		for ( auto node = entitystats->inventory.first; node != nullptr; node = nextnode )
		{
			nextnode = node->next;
			auto item2 = static_cast<Item*>(node->element);
			if ( !item2 )
			{
				continue;
			}
			if ( item2->playerSoldItemToShop != item->playerSoldItemToShop )
			{
				continue;
			}
			if ( item2->x != item->x || item2->y != item->y )
			{
				continue;
			}
			if (!itemCompare(item, item2, false, false))
			{
				printlog("[Shops]: client %d bought item from shop (uid=%d)\n", client, uidnum);
				if ( shopIsMysteriousShopkeeper(entity) )
				{
					buyItemFromMysteriousShopkeepConsumeOrb(client, *entity, *item2);
				}
				if ( itemTypeIsQuiver(item2->type) )
				{
					item2->count = 1; // so we consume it all up
				}
				consumeItem(item2, client);
				break;
			}
		}

		Sint32 buyValue = item->buyValue(client);
		entitystats->GOLD += buyValue;
		stats[client]->GOLD -= buyValue;
		stats[client]->GOLD = std::max(0, stats[client]->GOLD);
		if ( players[client] && players[client]->entity && !item->playerSoldItemToShop )
		{
			bool increaseSkill = false;
			if ( buyValue >= 100 )
			{
				increaseSkill = true;
			}
			else
			{
				if ( local_rng.rand() % 100 <= (std::max(10, buyValue)) ) // 20% to 100% from 1-100 gold
				{
					increaseSkill = true;
				}
			}

			if ( increaseSkill && entity )
			{
				if ( !strcmp(map.name, "Mages Guild") )
				{
					int increases = hamletShopkeeperSkillLimit[client][entity->getUID()];
					if ( increases >= hamletTradingSkillLimit )
					{
						increaseSkill = false;
						if ( local_rng.rand() % 2 )
						{
							messagePlayer(client, MESSAGE_HINT | MESSAGE_INTERACTION, Language::get(6868));
						}
					}
				}
			}

			if ( increaseSkill )
			{
				if ( buyValue <= 1 )
				{
					if ( stats[client]->getProficiency(PRO_TRADING) < SKILL_LEVEL_SKILLED )
					{
						players[client]->entity->increaseSkill(PRO_TRADING);
						if ( entity )
						{
							if ( !strcmp(map.name, "Mages Guild") )
							{
								hamletShopkeeperSkillLimit[client][entity->getUID()]++;
							}
						}
					}
				}
				else
				{
					players[client]->entity->increaseSkill(PRO_TRADING);
					if ( entity )
					{
						if ( !strcmp(map.name, "Mages Guild") )
						{
							hamletShopkeeperSkillLimit[client][entity->getUID()]++;
						}
					}
				}
			}
			//if ( local_rng.rand() % 2 )
			//{
			//	if ( item->buyValue(client) <= 1 )
			//	{
			//		// buying cheap items does not increase trading past basic
			//		if ( stats[client]->PROFICIENCIES[PRO_TRADING] < SKILL_LEVEL_SKILLED )
			//		{
			//			players[client]->entity->increaseSkill(PRO_TRADING);
			//		}
			//	}
			//	else
			//	{
			//		players[client]->entity->increaseSkill(PRO_TRADING);
			//	}
			//}
			//else if ( buyValue >= 150 )
			//{
			//	if ( buyValue >= 300 || local_rng.rand() % 2 )
			//	{
			//		players[client]->entity->increaseSkill(PRO_TRADING);
			//	}
			//}
		}
		free(item);
	}},

	//Remove a spell from the channeled spells list.
	{'UNCH', [](){
		const int client = net_packet->data[4];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		spell_t* thespell = getSpellFromID(SDLNet_Read32(&net_packet->data[5]));
		if (spellInList(&channeledSpells[client], thespell))
		{
			node_t *node, *nextnode;
			for (node = channeledSpells[client].first; node; node = nextnode )
			{
				nextnode = node->next;
				auto spell_search = static_cast<spell_t*>(node->element);
				if (spell_search->ID == thespell->ID)
				{
					spell_search->sustain = false;
				}
			}
		}
	}},

	// sell item to shop
	{'SHPS', [](){
		Uint32 uidnum = SDLNet_Read32(&net_packet->data[4]);
		const int client = net_packet->data[29];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		Entity* entity = uidToEntity(uidnum);
		if ( !entity )
		{
			printlog("[Shops]: warning: client %d sold item to non-existent shop! (uid=%d)\n", client, uidnum);
			return;
		}
		Stat* entitystats = entity->getStats();
		if ( !entitystats )
		{
			printlog("[Shops]: warning: client %d sold item to a \"shop\" that has no stats! (uid=%d)\n", client, uidnum);
			return;
		}

		bool identified = net_packet->data[28] == 1;
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[8])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[12])),
			SDLNet_Read16(&net_packet->data[16]),
			SDLNet_Read32(&net_packet->data[24]),
			SDLNet_Read32(&net_packet->data[20]),
			identified, nullptr);

		if ( !item )
		{
			printlog("[Shops]: client %d sold item to shop (uid=%d) but could not create item!\n", client, uidnum);
			return;
		}

		Sint32 goldValue = item->sellValue(client);
		int xout = Player::ShopGUI_t::MAX_SHOP_X;
		int yout = Player::ShopGUI_t::MAX_SHOP_Y;
		Item* itemToStackInto = nullptr;
		getShopFreeSlot(-1, &entitystats->inventory, item, xout, yout, itemToStackInto);
		if ( itemToStackInto )
		{
			itemToStackInto->count += item->count;
			itemToStackInto->playerSoldItemToShop = true;
			free(item);
			item = nullptr;
			printlog("[Shops]: client %d sold item to shop (uid=%d), added to existing item x: %d y: %d\n", client, uidnum, itemToStackInto->x, itemToStackInto->y);
		}
		else
		{
			Item* item2 = newItem(item->type, item->status, item->beatitude, item->count, item->appearance, item->identified, &entitystats->inventory);
			item2->x = xout;
			item2->y = yout;
			item2->playerSoldItemToShop = true;
			free(item);
			item = nullptr;
			printlog("[Shops]: client %d sold item to shop (uid=%d), new item stack x: %d y: %d\n", client, uidnum, item2->x, item2->y);
		}

		stats[client]->GOLD += goldValue;
		entitystats->GOLD -= goldValue;
		//if ( players[client] && players[client]->entity )
		//{
		//	if ( local_rng.rand() % 2 )
		//	{
		//		if ( goldValue <= 1 )
		//		{
		//			// selling cheap items does not increase trading past basic
		//			if ( stats[client]->PROFICIENCIES[PRO_TRADING] < SKILL_LEVEL_SKILLED )
		//			{
		//				players[client]->entity->increaseSkill(PRO_TRADING);
		//			}
		//		}
		//		else
		//		{
		//			players[client]->entity->increaseSkill(PRO_TRADING);
		//		}
		//	}
		//}
	}},

	// use item
	{'USEI', [](){
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		    &stats[client]->inventory);
		useItemServerCompat(&useItem, item, client);
	}},

	// use loot bag
	{ 'LOOT', []() {
		const int client = net_packet->data[8];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		Uint32 appearance = SDLNet_Read32(&net_packet->data[4]);
		Stat::emptyLootingBag(client, appearance);
	} },

	// equip item (as a weapon)
	{'EQUI', [](){
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		    &stats[client]->inventory);
		EquipItemResult res = equipItem(item, &stats[client]->weapon, client, false);
		if ( res == EQUIP_ITEM_SUCCESS_UPDATE_QTY
			|| res == EQUIP_ITEM_FAIL_CANT_UNEQUIP )
		{
			if ( item )
			{
				if ( item->node )
				{
					list_RemoveNode(item->node);
				}
				else
				{
					free(item);
				}
			}
		}
	}},

	// equip item (as a shield)
	{'EQUS', [](){
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		    &stats[client]->inventory);
		EquipItemResult res = equipItem(item, &stats[client]->shield, client, false);
		if ( res == EQUIP_ITEM_SUCCESS_UPDATE_QTY
			|| res == EQUIP_ITEM_FAIL_CANT_UNEQUIP )
		{
			if ( item )
			{
				if ( item->node )
				{
					list_RemoveNode(item->node);
				}
				else
				{
					free(item);
				}
			}
		}
	}},

	// consume torch item shield slot
	{ 'COOK', []() {
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
			static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
			static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
			SDLNet_Read32(&net_packet->data[12]),
			SDLNet_Read32(&net_packet->data[16]),
			SDLNet_Read32(&net_packet->data[20]),
			net_packet->data[24],
			&stats[client]->inventory);
		if ( stats[client]->shield )
		{
			// deselect shield
			if ( stats[client]->shield->node )
			{
				list_RemoveNode(stats[client]->shield->node);
			}
			else
			{
				free(stats[client]->shield);
			}
			stats[client]->shield = nullptr;
		}
		if ( item->count > 0 )
		{
			bool oldIntro = intro;
			intro = true;
			equipItem(item, &stats[client]->shield, client, false);
			intro = oldIntro;
		}
		else
		{
			if ( item->node )
			{
				list_RemoveNode(item->node);
			}
			else
			{
				free(item);
			}
		}
	} },

	// equip item (any other slot)
	{'EQUM', [](){
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		    &stats[client]->inventory);
		
		int res = -1;
		switch ( net_packet->data[27] )
		{
			case EQUIP_ITEM_SLOT_WEAPON:
				res = equipItem(item, &stats[client]->weapon, client, false);
				break;
			case EQUIP_ITEM_SLOT_SHIELD:
				res = equipItem(item, &stats[client]->shield, client, false);
				break;
			case EQUIP_ITEM_SLOT_MASK:
				res = equipItem(item, &stats[client]->mask, client, false);
				break;
			case EQUIP_ITEM_SLOT_HELM:
				res = equipItem(item, &stats[client]->helmet, client, false);
				break;
			case EQUIP_ITEM_SLOT_GLOVES:
				res = equipItem(item, &stats[client]->gloves, client, false);
				break;
			case EQUIP_ITEM_SLOT_BOOTS:
				res = equipItem(item, &stats[client]->shoes, client, false);
				break;
			case EQUIP_ITEM_SLOT_BREASTPLATE:
				res = equipItem(item, &stats[client]->breastplate, client, false);
				break;
			case EQUIP_ITEM_SLOT_CLOAK:
				res = equipItem(item, &stats[client]->cloak, client, false);
				break;
			case EQUIP_ITEM_SLOT_AMULET:
				res = equipItem(item, &stats[client]->amulet, client, false);
				break;
			case EQUIP_ITEM_SLOT_RING:
				res = equipItem(item, &stats[client]->ring, client, false);
				break;
			default:
				break;
		}

		if ( res == EQUIP_ITEM_SUCCESS_UPDATE_QTY
			|| res == EQUIP_ITEM_FAIL_CANT_UNEQUIP )
		{
			if ( item )
			{
				if ( item->node )
				{
					list_RemoveNode(item->node);
				}
				else
				{
					free(item);
				}
			}
		}
	}},

	// update appearance of item
	{ 'EQUA', []() {
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
			static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
			static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
			SDLNet_Read32(&net_packet->data[12]),
			SDLNet_Read32(&net_packet->data[16]),
			SDLNet_Read32(&net_packet->data[20]),
			net_packet->data[24],
			nullptr);

		const bool onIdentify = net_packet->data[27];
		Item* slot = nullptr;

		switch ( net_packet->data[26] )
		{
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_WEAPON:
				slot = stats[client]->weapon;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_SHIELD:
				slot = stats[client]->shield;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_MASK:
				slot = stats[client]->mask;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_HELM:
				slot = stats[client]->helmet;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_GLOVES:
				slot = stats[client]->gloves;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_BOOTS:
				slot = stats[client]->shoes;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_BREASTPLATE:
				slot = stats[client]->breastplate;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_CLOAK:
				slot = stats[client]->cloak;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_AMULET:
				slot = stats[client]->amulet;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_RING:
				slot = stats[client]->ring;
				break;
			default:
				break;
		}

		if ( slot )
		{
			if ( onIdentify )
			{
				item->identified = slot->identified;
			}
			Uint32 newAppearance = item->appearance;
			item->appearance = slot->appearance;
			if ( !itemCompare(item, slot, false, false) )
			{
				slot->appearance = newAppearance;
				if ( onIdentify )
				{
					slot->identified = true;
				}
			}
		}

		free(item);
		item = nullptr;
	} },

	// update itemType of item
	{ 'EQUT', []() {
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
			static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
			static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
			SDLNet_Read32(&net_packet->data[12]),
			SDLNet_Read32(&net_packet->data[16]),
			SDLNet_Read32(&net_packet->data[20]),
			net_packet->data[24],
			nullptr);

		Item* slot = nullptr;
		auto newType = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[27]));

		switch ( net_packet->data[26] )
		{
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_WEAPON:
				slot = stats[client]->weapon;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_SHIELD:
				slot = stats[client]->shield;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_MASK:
				slot = stats[client]->mask;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_HELM:
				slot = stats[client]->helmet;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_GLOVES:
				slot = stats[client]->gloves;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_BOOTS:
				slot = stats[client]->shoes;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_BREASTPLATE:
				slot = stats[client]->breastplate;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_CLOAK:
				slot = stats[client]->cloak;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_AMULET:
				slot = stats[client]->amulet;
				break;
			case ItemEquippableSlot::EQUIPPABLE_IN_SLOT_RING:
				slot = stats[client]->ring;
				break;
			default:
				break;
		}

		if ( slot )
		{
			if ( !itemCompare(item, slot, false, false) )
			{
				slot->type = newType;
				slot->appearance = item->appearance;
			}
		}

		free(item);
		item = nullptr;
	} },

	// apply item to entity
	{'APIT', [](){
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		nullptr);
		Entity* entity = uidToEntity(SDLNet_Read32(&net_packet->data[26]));
		if ( entity )
		{
			item->apply(client, entity);
		}
		else
		{
			printlog("warning: client applied item to entity that does not exist\n");
		}
		free(item);
	}},

	// apply item to entity
	{'APIW', [](){
		const int client = net_packet->data[25];
		if (!barony::net::validPlayer(client))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		nullptr);
		int wallx = (SDLNet_Read16(&net_packet->data[26]));
		int wally = (SDLNet_Read16(&net_packet->data[28]));
		item->applyLockpickToWall(client, wallx, wally);
		free(item);
	}},

	// attacking
	{'ATAK', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		if (players[player] && players[player]->entity)
		{
			auto type = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[7]));
			Uint32 appearance = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[11]));
			if ( stats[player]->weapon && stats[player]->weapon->type == type )
			{
				if ( type == MAGICSTAFF_SCEPTER )
				{
					stats[player]->weapon->appearance = appearance;
				}
			}
			if ( type == TOOL_DUCK )
			{
				if ( stats[player]->weapon && stats[player]->weapon->type != TOOL_DUCK )
				{
					return;
				}
			}
			if ( type == GEM_JEWEL )
			{
				if ( stats[player]->weapon && stats[player]->weapon->type != GEM_JEWEL )
				{
					return;
				}
			}
			int pose = net_packet->data[5];
			if ( pose == PLAYER_POSE_GOLEM_SMASH )
			{
				Item* tmp = stats[player]->weapon;
				stats[player]->weapon = nullptr;
				players[player]->entity->attack(pose, net_packet->data[6], nullptr);
				stats[player]->weapon = tmp;
			}
			else
			{
				players[player]->entity->attack(pose, net_packet->data[6], nullptr);
			}
		}
	}},

	//Multiplayer chest code (server).
	{'CCLS', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		if (openedChest[player])
		{
			openedChest[player]->closeChestServer();
		}
	}},

	//Multiplayer duck code (server).
	{ 'DUCK', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		const int duck = net_packet->data[5];
		players[player]->mechanics.pendingDucks.push_back(
			std::make_pair(duck, ticks + (3 + (local_rng.rand() % 30)) * TICKS_PER_SECOND));
	} },

	//The client failed some alchemy.
	{'BOOM', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		if ( players[player] && players[player]->entity )
		{
			bool protection = false;
			if ( stats[player]->mask && stats[player]->mask->type == MASK_HAZARD_GOGGLES )
			{
				bool shapeshifted = false;
				if ( stats[player]->type != HUMAN )
				{
					if ( players[player]->entity->effectShapeshift != NOTHING )
					{
						shapeshifted = true;
					}
				}
				if ( !shapeshifted )
				{
					protection = true;
					messagePlayerColor(player, MESSAGE_STATUS, makeColorRGB(0, 255, 0), Language::get(6089));
				}
			}
			spawnMagicTower(protection ? players[player]->entity : nullptr, 
				players[player]->entity->x, players[player]->entity->y, SPELL_FIREBALL, nullptr);
			players[player]->entity->setObituary(Language::get(3350));
			stats[player]->killer = KilledBy::FAILED_ALCHEMY;
		}
	}},

	//The client cast a spell.
	{'SPEL', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}

		spell_t* thespell = getSpellFromID(SDLNet_Read32(&net_packet->data[5]));
		if ( players[player] && players[player]->entity )
		{
			bool spellbookCast = net_packet->data[9] == 1;
			if ( net_packet->len > 10 )
			{
				CastSpellProps_t castSpellProps;
				castSpellProps.caster_x = (SDLNet_Read32(&net_packet->data[10]) / 256.0);
				castSpellProps.caster_y = (SDLNet_Read32(&net_packet->data[14]) / 256.0);
				castSpellProps.target_x = (SDLNet_Read32(&net_packet->data[18]) / 256.0);
				castSpellProps.target_y = (SDLNet_Read32(&net_packet->data[22]) / 256.0);
				castSpellProps.targetUID = (SDLNet_Read32(&net_packet->data[26]));
				castSpellProps.wallDir = net_packet->data[30];
				castSpellProps.optionalData = net_packet->data[31];
				castSpellProps.overcharge = net_packet->data[32];
				castSpell(players[player]->entity->getUID(), thespell, false, false, spellbookCast, &castSpellProps);
			}
			else
			{
				castSpell(players[player]->entity->getUID(), thespell, false, false, spellbookCast);
			}
		}
	}},

	//The client ghost cast a spell.
	{'GHSP', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}

		spell_t* thespell = getSpellFromID(SDLNet_Read32(&net_packet->data[5]));
		if ( players[player] && players[player]->ghost.isActive() )
		{
			castSpell(players[player]->ghost.my->getUID(), thespell, false, true);
		}
	}},

	//The client added an item to the chest.
	{'CITM', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		if ( net_packet->data[27] == 0 && !openedChest[player])
		{
			return;
		}

		Item* newitem = newItem(WOODEN_SHIELD, BROKEN, 0, 1, 0, true, nullptr);
		newitem->type = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[5]));
		newitem->status = static_cast<Status>(SDLNet_Read32(&net_packet->data[9]));
		newitem->beatitude = SDLNet_Read32(&net_packet->data[13]);
		newitem->count = SDLNet_Read32(&net_packet->data[17]);
		newitem->appearance = SDLNet_Read32(&net_packet->data[21]);
		newitem->identified = net_packet->data[25];
		bool forceNewStack = net_packet->data[26] ? true : false;
		if ( net_packet->data[27] == 0 )
		{
			Item* chestItem = openedChest[player]->addItemToChestServer(newitem, forceNewStack, nullptr);
			if ( chestItem != newitem )
			{
				free(newitem);
			}
		}
		else
		{
			Item* chestItem = Entity::addItemToVoidChestServer(player, newitem, forceNewStack, nullptr);
			if ( chestItem != newitem )
			{
				free(newitem);
			}
		}
	}},

	//The client removed an item from the chest.
	{'RCIT', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		if ( net_packet->data[27] == 0 && !openedChest[player])
		{
			return;
		}

		Item* item = newItem(WOODEN_SHIELD, BROKEN, 0, 1, 0, true, nullptr);
		item->type = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[5]));
		item->status = static_cast<Status>(SDLNet_Read32(&net_packet->data[9]));
		item->beatitude = SDLNet_Read32(&net_packet->data[13]);
		item->count = SDLNet_Read32(&net_packet->data[17]);
		item->appearance = SDLNet_Read32(&net_packet->data[21]);
		item->identified = net_packet->data[25];

		if ( net_packet->data[27] == 0 )
		{
			openedChest[player]->removeItemFromChestServer(item, item->count);
		}
		else
		{
			Entity::removeItemFromVoidChestServer(player, item, item->count);
		}
		free(item);
	}},

	// the client removed a curse on his equipment
	{'RCUR', [](){
	    Item* item;
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		switch ( net_packet->data[5] )
		{
			case 0:
				item = stats[player]->helmet;
				break;
			case 1:
				item = stats[player]->breastplate;
				break;
			case 2:
				item = stats[player]->gloves;
				break;
			case 3:
				item = stats[player]->shoes;
				break;
			case 4:
				item = stats[player]->shield;
				break;
			case 5:
				item = stats[player]->weapon;
				break;
			case 6:
				item = stats[player]->cloak;
				break;
			case 7:
				item = stats[player]->amulet;
				break;
			case 8:
				item = stats[player]->ring;
				break;
			case 9:
				item = stats[player]->mask;
				break;
			default:
				item = nullptr;
				break;
		}
		if ( item != nullptr )
		{
			item->beatitude = 0;
		}
	}},

	// the client repaired equipment or otherwise modified status of equipment.
	{'REPA', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		Item* equipment = nullptr;

		switch ( net_packet->data[5] )
		{
			case 0:
				equipment = stats[player]->weapon;
				break;
			case 1:
				equipment = stats[player]->helmet;
				break;
			case 2:
				equipment = stats[player]->breastplate;
				break;
			case 3:
				equipment = stats[player]->gloves;
				break;
			case 4:
				equipment = stats[player]->shoes;
				break;
			case 5:
				equipment = stats[player]->shield;
				break;
			case 6:
				equipment = stats[player]->cloak;
				break;
			case 7:
				equipment = stats[player]->mask;
				break;
			default:
				equipment = nullptr;
				break;
		}

		if ( !equipment )
		{
			return;
		}
		
		if ( static_cast<int>(net_packet->data[6]) > EXCELLENT )
		{
			equipment->status = EXCELLENT;
		}
		else if ( static_cast<int>(net_packet->data[6]) < BROKEN )
		{
			equipment->status = BROKEN;
		}
		equipment->status = static_cast<Status>(net_packet->data[6]);
		return;
	}},

	// the client repaired tinkering bots
	{ 'REPT', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		Item* equipment = nullptr;

		switch ( net_packet->data[5] )
		{
			case 0:
				equipment = stats[player]->weapon;
				break;
			case 1:
				equipment = stats[player]->helmet;
				break;
			case 2:
				equipment = stats[player]->breastplate;
				break;
			case 3:
				equipment = stats[player]->gloves;
				break;
			case 4:
				equipment = stats[player]->shoes;
				break;
			case 5:
				equipment = stats[player]->shield;
				break;
			case 6:
				equipment = stats[player]->cloak;
				break;
			case 7:
				equipment = stats[player]->mask;
				break;
			default:
				equipment = nullptr;
				break;
		}

		if ( !equipment )
		{
			return;
		}
		if ( !(equipment->type == TOOL_SENTRYBOT
			|| equipment->type == TOOL_SPELLBOT
			|| equipment->type == TOOL_DUMMYBOT
			|| equipment->type == TOOL_GYROBOT) )
		{
			return;
		}

		if ( static_cast<int>(net_packet->data[6]) > EXCELLENT )
		{
			equipment->status = EXCELLENT;
		}
		else if ( static_cast<int>(net_packet->data[6]) < BROKEN )
		{
			equipment->status = BROKEN;
		}
		equipment->status = static_cast<Status>(net_packet->data[6]);
		equipment->appearance = SDLNet_Read32(&net_packet->data[7]);
		return;
	} },

	// the client changed beatitude of equipment.
	{'BEAT', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		Item* equipment = nullptr;
		//messagePlayer(0, "client: %d, armornum: %d, status %d", player, net_packet->data[5], net_packet->data[6]);
		switch ( net_packet->data[5] )
		{
			case 0:
				equipment = stats[player]->weapon;
				break;
			case 1:
				equipment = stats[player]->helmet;
				break;
			case 2:
				equipment = stats[player]->breastplate;
				break;
			case 3:
				equipment = stats[player]->gloves;
				break;
			case 4:
				equipment = stats[player]->shoes;
				break;
			case 5:
				equipment = stats[player]->shield;
				break;
			case 6:
				equipment = stats[player]->cloak;
				break;
			case 7:
				equipment = stats[player]->mask;
				break;
			default:
				equipment = nullptr;
				break;
		}

		if ( !equipment )
		{
			return;
		}
		int itemType = SDLNet_Read16(&net_packet->data[7]);
		if ( static_cast<int>(equipment->type) == itemType ) // sanity check the item type is what was changed
		{
			equipment->beatitude = net_packet->data[6] - 100; // we sent the data beatitude + 100
		}
		//messagePlayer(0, "%d", equipment->beatitude);
	}},

	// client dropped gold
	{'DGLD', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		int amount = SDLNet_Read32(&net_packet->data[5]);

		if ( stats[player]->GOLD < 0 )
		{
			stats[player]->GOLD = 0;
		}
		if ( stats[player]->GOLD < amount )
		{
			amount = stats[player]->GOLD;
		}
        if ( amount <= 0 )
        {
            return;
        }
		stats[player]->GOLD -= amount;
		stats[player]->GOLD = std::max(stats[player]->GOLD, 0);
		if ( players[player] && players[player]->entity )
		{
			//Drop gold.
			playSoundEntity(players[player]->entity, 242 + local_rng.rand() % 4, 64);
			auto entity = newEntity(amount < 5 ? 1379 : 130, 0, map.entities, nullptr); // 130 = goldbag model
			entity->sizex = 4;
			entity->sizey = 4;
			entity->x = players[player]->entity->x;
			entity->y = players[player]->entity->y;
			entity->goldAmount = amount; // amount
			entity->z = 0;
			entity->vel_z = (-40 - local_rng.rand() % 5) * .01;
			entity->goldBouncing = 0;
			entity->yaw = (local_rng.rand() % 360) * PI / 180.0;
			entity->flags[PASSABLE] = true;
			entity->flags[UPDATENEEDED] = true;
			entity->behavior = &actGoldBag;
			entity->goldDroppedByPlayer = player + 1;
		}
	}},

	// client played a sound
	{'EMOT', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		const int sfx = SDLNet_Read16(&net_packet->data[5]);
		const int vol = std::min(92, static_cast<int>(net_packet->data[7]));
		if ( players[player] && players[player]->entity )
		{
			playSoundEntityLocal(players[player]->entity, sfx, vol);
			for ( int c = 1; c < barony::net::playerCapacity(); ++c )
			{
				// send to all other players
				if ( c != player && !client_disconnected[c] && !players[c]->isLocalPlayer() )
				{
					strcpy((char*)net_packet->data, "SNEL");
					SDLNet_Write16(sfx, &net_packet->data[4]);
					SDLNet_Write32(players[player]->entity->getUID(), &net_packet->data[6]);
					SDLNet_Write16(vol, &net_packet->data[10]);
					net_packet->address.host = net_clients[c - 1].host;
					net_packet->address.port = net_clients[c - 1].port;
					net_packet->len = 12;
					sendPacketSafe(net_sock, -1, net_packet, c - 1);
				}
			}
		}
	}},

	// the client asked for a level up
	{'CLVL', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		if ( players[player] && players[player]->entity )
		{
			players[player]->entity->getStats()->EXP += 100;
		}
	}},

	// the client asked for a level up
	{'CSKL', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		const int skill = net_packet->data[5];
		if ( player > 0 && barony::net::validPlayer(player) && players[player] && players[player]->entity )
		{
			if ( skill >= 0 && skill < NUMPROFICIENCIES )
			{
				players[player]->entity->increaseSkill(skill);
			}
		}
	}},

	// the client sent a minimap ping packet.
	{'PMAP', [](){
		MinimapPing newPing(ticks, net_packet->data[4], 
			net_packet->data[5], 
			net_packet->data[6],
			net_packet->data[8] ? true : false,
			static_cast<MinimapPing::PingType>(net_packet->data[7]));
		sendMinimapPing(net_packet->data[4], newPing.x, newPing.y, newPing.pingType); // relay self and to other clients.
	}},

	// the client sent a gameplayer preferences update
	{ 'GPPR', []() {
		GameplayPreferences_t::receivePacket();
	}},

	//Remove vampiric aura
	{'VAMP', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		const int spellID = SDLNet_Read32(&net_packet->data[5]);
		if ( players[player] && players[player]->entity && stats[player] )
		{
			if ( client_classes[player] == CLASS_ACCURSED &&
				stats[player]->getEffectActive(EFF_VAMPIRICAURA) && players[player]->entity->playerVampireCurse == 1 )
			{
				players[player]->entity->setEffect(EFF_VAMPIRICAURA, true, 1, true);
				messagePlayerColor(player, MESSAGE_STATUS, uint32ColorGreen, Language::get(3241));
				messagePlayerColor(player, MESSAGE_HINT, uint32ColorGreen, Language::get(3242));
				players[player]->entity->playerVampireCurse = 2; // cured.
				serverUpdateEntitySkill(players[player]->entity, 51);
				steamAchievementClient(player, "BARONY_ACH_REVERSE_THIS_CURSE");
				playSoundEntity(players[player]->entity, 402, 128);
				createParticleDropRising(players[player]->entity, 174, 1.0);
				serverSpawnMiscParticles(players[player]->entity, PARTICLE_EFFECT_RISING_DROP, 174);
			}
		}
	}},

	// the client sent a monster command.
	{'ALLY', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		const int allyCmd = net_packet->data[5];
		const Uint32 uid = SDLNet_Read32(&net_packet->data[8]);
		//messagePlayer(0, " received %d, %d, %d, %d, %d", player, allyCmd, net_packet->data[6], net_packet->data[7], uid);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			if ( net_packet->len > 12 )
			{
				Uint32 interactUid = SDLNet_Read32(&net_packet->data[12]);
				entity->monsterAllySendCommand(allyCmd, net_packet->data[6], net_packet->data[7], interactUid);
				//messagePlayer(0, "received UID of target: %d, applying...", uid);
				entity->monsterAllyInteractTarget = interactUid;
			}
			else
			{
				entity->monsterAllySendCommand(allyCmd, net_packet->data[6], net_packet->data[7]);
			}
		}
	}},

	{'IDIE', [](){
		const int playerDie = net_packet->data[4];
		if ( playerDie >= 1 && barony::net::validPlayer(playerDie) )
		{
			if ( players[playerDie] && players[playerDie]->entity )
			{
				players[playerDie]->entity->setHP(0);
			}
		}
	}},

	// use automaton food item
	{'FODA', [](){
	    const int player = net_packet->data[25];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[24],
		    &stats[player]->inventory);
		item_FoodAutomaton(item, player);
	}},

	// adorcise item
	{ 'ADOR', []() {
		const int player = net_packet->data[25];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		auto item = newItem(
			static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
			static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
			SDLNet_Read32(&net_packet->data[12]),
			SDLNet_Read32(&net_packet->data[16]),
			SDLNet_Read32(&net_packet->data[20]),
			net_packet->data[24], nullptr);
		
		real_t spawn_x = SDLNet_Read16(&net_packet->data[26]) * 16.0 + 8.0;
		real_t spawn_y = SDLNet_Read16(&net_packet->data[28]) * 16.0 + 8.0;
		bool spawned = false;
		if ( players[player]->entity )
		{
			if ( Entity* monster = spellEffectAdorcise(*players[player]->entity, spellElementMap[SPELL_ADORCISM],
				spawn_x, spawn_y, item) )
			{
				spawned = true;
			}
		}
		
		if ( !spawned )
		{
			messagePlayer(player, MESSAGE_MISC, Language::get(6578));

			// refund the item at the player, or spawn location if dead
			bool dropped = false;
			if ( players[player]->entity )
			{
				// no room to spawn!
				auto item2 = newItem(item->type,
					item->status,
					item->beatitude,
					item->count,
					item->appearance,
					item->identified,
					&stats[player]->inventory);
				dropped = dropItem(item2, player, true, true);
			}
			
			if ( !dropped )
			{
				Entity* entity = newEntity(-1, 1, map.entities, nullptr); //Item entity.
				entity->flags[INVISIBLE] = true;
				entity->flags[UPDATENEEDED] = true;
				entity->x = players[player]->player_last_x;
				entity->y = players[player]->player_last_y;
				entity->sizex = 4;
				entity->sizey = 4;
				entity->yaw = local_rng.rand() % 360 * (PI / 180.0);
				entity->vel_x = 0.0;
				entity->vel_y = 0.0;
				entity->vel_z = (-10 - local_rng.rand() % 20) * .01;
				entity->flags[PASSABLE] = true;
				entity->behavior = &actItem;
				entity->skill[10] = item->type;
				entity->skill[11] = item->status;
				entity->skill[12] = item->beatitude;
				entity->skill[13] = item->count;
				entity->skill[14] = item->appearance;
				entity->skill[15] = item->identified;
				entity->parent = 0;
				entity->itemOriginalOwner = 0;

				playSoundPos(players[player]->player_last_x, players[player]->player_last_y, 47 + local_rng.rand() % 3, 64);
			}
			messagePlayer(player, MESSAGE_MISC, Language::get(6621), item->getName());
		}

		free(item);
	} },

	// broke a mirror
	{ 'MIRR', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		if ( players[player]->entity )
		{
			if ( players[player]->entity->setEffect(EFF_BLEEDING, true, TICKS_PER_SECOND * 15, true) )
			{
				messagePlayerColor(player, MESSAGE_STATUS, 
					makeColorRGB(255, 0, 0), Language::get(701)); // you're bleeding!
			}
			playSoundEntity(players[player]->entity, 162, 64);
		}
	} },

	{ 'CMPD', []() {
		// client ack received the packet
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		const Uint8 clientSequence = net_packet->data[5];

		auto find = Compendium_t::Events_t::clientDataStrings[player].find(clientSequence);
		if ( find != Compendium_t::Events_t::clientDataStrings[player].end() )
		{
			Compendium_t::Events_t::clientDataStrings[player].erase(clientSequence);
		}
	}},

	// character change
	{ 'ASSC', []() {
		int player = net_packet->data[8];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			auto& gui = GenericGUI[player].assistShrineGUI;
			gui.savedClass = static_cast<Sint8>(net_packet->data[4]);
			gui.savedRace = static_cast<Sint8>(net_packet->data[5]);
			gui.savedSex = static_cast<Sint8>(net_packet->data[6]);
			gui.savedAppearance = static_cast<Sint8>(net_packet->data[7]);
			gui.receivedCharacterChangeOK = true;

			std::string racename = "";
			if ( gui.savedRace != RACE_HUMAN )
			{
				if ( gui.savedAppearance != 0 )
				{
					racename = Language::get(4068); // guised
					racename += ' ';
				}
			}
			racename += getMonsterLocalizedName(getMonsterFromPlayerRace(gui.savedRace)).c_str();
			camelCaseString(racename);
			std::string classname = playerClassLangEntry(gui.savedClass >= 0 ? gui.savedClass : client_classes[player], player);
			camelCaseString(classname);

			for ( int i = 0; i < barony::net::playerCapacity(); ++i )
			{
				if ( i != player )
				{
					messagePlayer(i, MESSAGE_WORLD, Language::get(6336), stats[player]->name, racename.c_str(), classname.c_str());
				}
			}

			if ( player > 0 )
			{
				// confirm receipt of class change to sender
				strcpy((char*)net_packet->data, "ASSC");
				net_packet->data[4] = static_cast<Sint8>(gui.savedClass);
				net_packet->data[5] = static_cast<Sint8>(gui.savedRace);
				net_packet->data[6] = static_cast<Sint8>(gui.savedSex);
				net_packet->data[7] = static_cast<Sint8>(gui.savedAppearance);
				net_packet->data[8] = player;
				net_packet->address.host = net_clients[player - 1].host;
				net_packet->address.port = net_clients[player - 1].port;
				net_packet->len = 9;
				sendPacketSafe(net_sock, -1, net_packet, player - 1);
			}
		}
	}},

	// client closed assist shrine
	{ 'ASCL', []() {
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* shrine = uidToEntity(uid) )
			{
				if ( achievementObserver.playerUids[player] == static_cast<Uint32>(shrine->skill[0]) )
				{
					shrine->skill[0] = 0;
					serverUpdateEntitySkill(shrine, 0);
				}
			}
		}
	}},

	// client closed cauldron
	{ 'CAUC', []() {
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* cauldron = uidToEntity(uid) )
			{
				if ( achievementObserver.playerUids[player] == static_cast<Uint32>(cauldron->skill[6]) )
				{
					cauldron->skill[6] = 0;
					serverUpdateEntitySkill(cauldron, 6);
				}
			}
		}
	} },

	// client closed workbench
	{ 'WRKC', []() {
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* workbench = uidToEntity(uid) )
			{
				if ( achievementObserver.playerUids[player] == static_cast<Uint32>(workbench->skill[6]) )
				{
					workbench->skill[6] = 0;
					serverUpdateEntitySkill(workbench, 6);
				}
			}
		}
	} },

	// client closed mailbox
	{ 'MBXC', []() {
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* mailbox = uidToEntity(uid) )
			{
				if ( achievementObserver.playerUids[player] == static_cast<Uint32>(mailbox->skill[6]) )
				{
					mailbox->skill[6] = 0;
					serverUpdateEntitySkill(mailbox, 6);
				}
			}
		}
	} },

	// client claimed some assist items
	{ 'ASSI', []() {
	int player = net_packet->data[4];
	if ( player >= 0 && barony::net::validPlayer(player) )
	{
		Sint32 claimedPts = std::max(0, static_cast<Sint32>(SDLNet_Read32(&net_packet->data[5])));
		Sint32 prevPts = stats[player]->MISC_FLAGS[STAT_FLAG_ASSISTANCE_PLAYER_PTS];
		stats[player]->MISC_FLAGS[STAT_FLAG_ASSISTANCE_PLAYER_PTS] = claimedPts;

		int totalClaimed = 0;
		for ( int i = 0; i < barony::net::playerCapacity(); ++i )
		{
			if ( !client_disconnected[i] )
			{
				totalClaimed += stats[i]->MISC_FLAGS[STAT_FLAG_ASSISTANCE_PLAYER_PTS];
				if ( i == player )
				{
					messagePlayer(i, MESSAGE_WORLD, Language::get(6356), claimedPts);
				}
				else
				{
					messagePlayer(i, MESSAGE_WORLD, Language::get(6357), stats[player]->name, claimedPts);
				}
			}
		}

		conductGameChallenges[CONDUCT_ASSISTANCE_CLAIMED] = std::max(totalClaimed, conductGameChallenges[CONDUCT_ASSISTANCE_CLAIMED]);
		for ( int i = 1; i < barony::net::playerCapacity(); ++i )
		{
			if ( !client_disconnected[i] )
			{
				serverUpdatePlayerConduct(i, CONDUCT_ASSISTANCE_CLAIMED, conductGameChallenges[CONDUCT_ASSISTANCE_CLAIMED]);
			}
		}
		GenericGUIMenu::AssistShrineGUI_t::serverUpdateStatFlagsForClients();
	}
	} },

	{ 'VOIP',[]() {
#ifdef USE_FMOD
		VoiceChat.receivePacket(net_packet);
#endif
	} },

	{ 'FXGD',[]() {
		int player = net_packet->data[4];
		if ( player >= 1 && barony::net::validPlayer(player) && !players[player]->isLocalPlayer() )
		{
			Sint32 goldSpent = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[5]));
			stats[player]->GOLD -= goldSpent;
			stats[player]->GOLD = std::max(0, stats[player]->GOLD);

			Sint32 magiccost = std::max(0, static_cast<Sint32>(SDLNet_Read32(&net_packet->data[9])));
			Sint32 prevMP = stats[player]->MP;
			if ( players[player] && players[player]->entity )
			{
				if ( magiccost > stats[player]->MP )
				{
					// damage sound/effect due to overdraw.
					strcpy((char*)net_packet->data, "SHAK");
					net_packet->data[4] = 10; // turns into .1
					net_packet->data[5] = 10;
					net_packet->address.host = net_clients[player - 1].host;
					net_packet->address.port = net_clients[player - 1].port;
					net_packet->len = 6;
					sendPacketSafe(net_sock, -1, net_packet, player - 1);
					playSoundPlayer(player, 28, 92);
				}
				players[player]->entity->drainMP(magiccost);
			}

			Uint16 spellID = SDLNet_Read16(&net_packet->data[13]);
			if ( spellID != SPELL_NONE )
			{
				if ( auto spell = getSpellFromID(spellID) )
				{
					players[player]->mechanics.baseSpellIncrementMP(prevMP - stats[player]->MP, spell->skillID);
				}
			}

			strcpy((char*)net_packet->data, "GOLD");
			SDLNet_Write32(stats[player]->GOLD, &net_packet->data[4]);
			net_packet->address.host = net_clients[player - 1].host;
			net_packet->address.port = net_clients[player - 1].port;
			net_packet->len = 8;
			sendPacketSafe(net_sock, -1, net_packet, player - 1);
		}
	}},

	{ 'SANM',[]() { // player spellcast animation
		int player = net_packet->data[4];
		int pose = net_packet->data[5];
		int charge = SDLNet_Read16(&net_packet->data[6]);
		spellcastAnimationUpdateReceive(player, pose, charge);
	} },

	{ 'OVRC', []() {
		int player = net_packet->data[4];
		if ( player >= 1 && barony::net::validPlayer(player) && !players[player]->isLocalPlayer() )
		{
			if ( players[player] && players[player]->entity && stats[player] )
			{
				cast_animation[clientnum].overcharge_init = net_packet->data[5];
			}
		}
	} },

	{ 'SPLV',[]() { // player spell level proc
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			if ( !players[player]->isLocalPlayer() )
			{
				if ( players[player]->entity )
				{
					int spellID = SDLNet_Read16(&net_packet->data[5]);
					Uint32 eventType = SDLNet_Read32(&net_packet->data[7]);
					int eventValue = SDLNet_Read32(&net_packet->data[11]);
					if ( spellID == SPELL_DETECT_FOOD )
					{
						players[player]->mechanics.updateSustainedSpellEvent(SPELL_DETECT_FOOD, eventValue * 10, 1.0, nullptr);
					}
					else
					{
						magicOnSpellCastEvent(players[player]->entity, players[player]->entity, nullptr, spellID, eventType, eventValue);
					}
				}
			}
		}
	} },

	// update breakable counter
	{ 'GBRK', []() {
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			if ( !players[player]->isLocalPlayer() )
			{
				if ( players[player]->entity )
				{
					int eventType = net_packet->data[5];
					if ( eventType == static_cast<int>(Player::PlayerMechanics_t::BreakableEvent::GBREAK_DEGRADE) )
					{
						players[player]->mechanics.incrementBreakableCounter(Player::PlayerMechanics_t::BreakableEvent::GBREAK_DEGRADE, nullptr);
					}
				}
			}
		}
	} },
};

void serverHandlePacket()
{
	if (handleSafePacket())
	{
		return;
	}

#ifdef PACKETINFO
	char packetinfo[NET_PACKET_SIZE];
	strncpy( packetinfo, (char*)net_packet->data, net_packet->len );
	packetinfo[net_packet->len] = 0;
	printlog("info: server packet: %s\n", packetinfo);
#endif

	Uint32 packetId = SDLNet_Read32(&net_packet->data[0]);

    auto find = serverPacketHandlers.find(packetId);
    if (find == serverPacketHandlers.end()) {
        // error
        printlog("Got a mystery packet: %c%c%c%c",
            static_cast<char>(net_packet->data[0]),
            static_cast<char>(net_packet->data[1]),
            static_cast<char>(net_packet->data[2]),
            static_cast<char>(net_packet->data[3]));
    } else {
        (*(find->second))(); // handle packet
    }
}

/*-------------------------------------------------------------------------------

	serverHandleMessages

	Parses messages received from clients

-------------------------------------------------------------------------------*/

void serverHandleMessages(Uint32 framerateBreakInterval)
{
    if (!directConnect)
    {
#if defined(STEAMWORKS) || defined(USE_EOS)
        auto* transport = barony::net::platformTransport();
        if (!transport)
        {
            return;
        }

        std::vector<barony::net::IncomingPacket> packets;
        transport->poll(packets);

        if (logCheckMainLoopTimers)
        {
            DebugStats.messagesT1 = std::chrono::high_resolution_clock::now();
            DebugStats.handlePacketStartLoop = true;
        }

        for (std::size_t index = 0; index < packets.size(); ++index)
        {
            const auto& packet = packets[index];
            if (packet.data.empty() || packet.data.size() > NET_PACKET_SIZE)
            {
                continue;
            }

            std::memcpy(net_packet->data, packet.data.data(), packet.data.size());
            net_packet->len = static_cast<int>(packet.data.size());
            serverHandlePacket();

            if (logCheckMainLoopTimers)
            {
                DebugStats.messagesT2WhileLoop = std::chrono::high_resolution_clock::now();
                DebugStats.handlePacketStartLoop = false;
            }

            if (!disableFPSLimitOnNetworkMessages && !frameRateLimit(framerateBreakInterval, false))
            {
                if (logCheckMainLoopTimers)
                {
                    printlog("[NETWORK]: Incoming messages exceeded given cycle time, packets remaining: %zu",
                        packets.size() - index - 1);
                }
                break;
            }
        }
#endif
        return;
    }

    while (SDLNet_UDP_Recv(net_sock, net_packet))
    {
        if (!net_packet->data[0])
        {
            continue;
        }
        serverHandlePacket();
    }
}

/*-------------------------------------------------------------------------------

	handleSafePacket()

	Handles potentially safe packets. Returns true if this is not a packet to
	be handled.

-------------------------------------------------------------------------------*/

