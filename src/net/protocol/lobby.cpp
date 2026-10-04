#include "../private/common.hpp"

NetworkingLobbyJoinRequestResult lobbyPlayerJoinRequest(int& outResult, const bool* lockedSlots)
{
    printlog("processing lobby join request\n");

	Uint32 result = static_cast<Uint32>(LobbyJoinError::Full);
	if ( strcmp(VERSION, (char*)net_packet->data + 48) ) // TODO this should be safer.
	{
		result = static_cast<Uint32>(LobbyJoinError::VersionMismatch);
	}
	else
	{
		Uint32 clientms = SDLNet_Read32(&net_packet->data[57]);
		Uint32 clientlsg = SDLNet_Read32(&net_packet->data[61]);
		Uint32 clientlobbyKey = (net_packet->len > 65) ? SDLNet_Read32(&net_packet->data[65]) : 0;
		if ( net_packet->data[56] == 0 )
		{
			// client will enter any player spot
			for ( result = 1; result < static_cast<Uint32>(barony::net::playerCapacity()); ++result )
			{
				if ( client_disconnected[result] == true && !lockedSlots[result] )
				{
					break;    // no more player slots
				}
			}
		}
		else
		{
			// client is joining a particular player spot
			result = net_packet->data[56];
			if ( !barony::net::validPlayer(static_cast<int>(result)) || !client_disconnected[result] || lockedSlots[result] )
			{
				result = static_cast<Uint32>(LobbyJoinError::Full);
			}
		}
		SaveGameInfo savegameinfo;
		if (loadingsavegame) {
			savegameinfo = getSaveGameInfo(false);
		}
		if ( clientlsg != loadingsavegame && loadingsavegame == 0 )
		{
			result = static_cast<Uint32>(LobbyJoinError::UnexpectedSave);
		}
		else if ( clientlsg == 0 && loadingsavegame != 0 )
		{
			result = static_cast<Uint32>(LobbyJoinError::MissingSave);
		}
		else if ( clientlsg != loadingsavegame )
		{
			result = static_cast<Uint32>(LobbyJoinError::IncompatibleSave);
		}
		else if ( loadingsavegame && savegameinfo.mapseed != clientms )
		{
			result = static_cast<Uint32>(LobbyJoinError::WrongLevel);
		}
		else if ( (loadingsavegame && clientlobbyKey != savegameinfo.lobbykey) )
		{
			result = static_cast<Uint32>(LobbyJoinError::LobbyKeyMismatch);
		}
	}
	outResult = result;
	if ( result >= NET_JOIN_ERROR_BASE )
	{

		net_packet->len = 8;
		memcpy(net_packet->data, "HELO", 4);
		SDLNet_Write32(result, &net_packet->data[4]); // error code for client to interpret
		printlog("sending error code %d to client.\n", result);
		if ( directConnect )
		{
			sendPacketSafe(net_sock, -1, net_packet, 0);
			return NET_LOBBY_JOIN_DIRECTIP_FAILURE;
		}
		else
		{
			return NET_LOBBY_JOIN_P2P_FAILURE;
		}
	}
	else
	{
	    const int c = result;

		// on success, client gets legit player number
		client_disconnected[c] = false;
        stringCopy(stats[c]->name, (const char*)net_packet->data + 4, sizeof(Stat::name), 32);
		client_classes[c] = (int)SDLNet_Read32(&net_packet->data[36]);
		stats[c]->sex = static_cast<sex_t>((int)SDLNet_Read32(&net_packet->data[40]));
		Uint32 raceAndAppearance = (Uint32)SDLNet_Read32(&net_packet->data[44]);
		stats[c]->stat_appearance = (raceAndAppearance & 0xFF00) >> 8;
		stats[c]->playerRace = (raceAndAppearance & 0xFF);
		net_clients[c - 1].host = net_packet->address.host;
		net_clients[c - 1].port = net_packet->address.port;
		client_keepalive[c] = ticks;

		printlog("client %d connected.\n", c);

		// send existing clients info on new client
		for ( int x = 1; x < barony::net::playerCapacity(); ++x )
		{
			if ( client_disconnected[x] || c == x )
			{
				continue;
			}
			strcpy((char*)(&net_packet->data[0]), "JOIN");
			net_packet->data[4] = c; // clientnum
			net_packet->data[5] = client_classes[c]; // class
			net_packet->data[6] = stats[c]->sex; // sex
			net_packet->data[7] = (Uint8)stats[c]->stat_appearance; // appearance
			net_packet->data[8] = (Uint8)stats[c]->playerRace; // player race
			stringCopy((char*)net_packet->data + 9, stats[c]->name, 32, sizeof(Stat::name)); // name
			net_packet->address.host = net_clients[x - 1].host;
			net_packet->address.port = net_clients[x - 1].port;
			net_packet->len = 9 + 32;
			sendPacketSafe(net_sock, -1, net_packet, x - 1);
		}
		char shortname[32] = { 0 };
		strncpy(shortname, stats[c]->name, 22);

		//newString(&lobbyChatboxMessages, 0xFFFFFFFF, "\n***   %s has joined the game   ***\n", shortname);

		// send new client their id number + info on other clients
		memcpy(net_packet->data, "HELO", 4);
		SDLNet_Write32(c, &net_packet->data[4]);
		const int snapshotChunkSize = loadingsavegame ? (6 + 32 + 6 * 10) : (6 + 32);
		if (8 + barony::net::playerCapacity() * snapshotChunkSize > NET_PACKET_SIZE)
		{
			SDLNet_Write32(static_cast<Uint32>(LobbyJoinError::SnapshotTooLarge), &net_packet->data[4]);
			net_packet->len = 8;
			outResult = static_cast<int>(LobbyJoinError::SnapshotTooLarge);
			if (directConnect)
			{
				sendPacketSafe(net_sock, -1, net_packet, 0);
				return NET_LOBBY_JOIN_DIRECTIP_FAILURE;
			}
			return NET_LOBBY_JOIN_P2P_FAILURE;
		}
		if (loadingsavegame) {
			constexpr int chunk_size = 6 + 32 + 6 * 10; // 6 bytes for player stats, 32 for name, 60 for equipment
			for ( int x = 0; x < barony::net::playerCapacity(); ++x )
			{
				net_packet->data[8 + x * chunk_size + 0] = client_disconnected[x]; // connectedness
				net_packet->data[8 + x * chunk_size + 1] = lockedSlots[x]; // locked state
				net_packet->data[8 + x * chunk_size + 2] = client_classes[x]; // class
				net_packet->data[8 + x * chunk_size + 3] = stats[x]->sex; // sex
				net_packet->data[8 + x * chunk_size + 4] = (Uint8)stats[x]->stat_appearance; // appearance
				net_packet->data[8 + x * chunk_size + 5] = (Uint8)stats[x]->playerRace; // player race

				char shortname[32];
				snprintf(shortname, sizeof(shortname), "%s", stats[x]->name);
				memcpy(net_packet->data + 8 + x * chunk_size + 6, shortname, sizeof(shortname)); // name

				const Item* player_slots[] = {
					stats[x]->helmet,
					stats[x]->breastplate,
					stats[x]->gloves,
					stats[x]->shoes,
					stats[x]->shield,
					stats[x]->weapon,
					stats[x]->cloak,
					stats[x]->amulet,
					stats[x]->ring,
					stats[x]->mask,
				};
				constexpr int num_slots = sizeof(player_slots) / sizeof(player_slots[0]);

				for (int j = 0; j < num_slots; ++j) {
					auto slot = player_slots[j];
					if (slot) {
						SDLNet_Write16((Uint16)slot->type, net_packet->data + 8 + x * chunk_size + 6 + 32 + j * 6);
						SDLNet_Write32((Uint32)slot->appearance, net_packet->data + 8 + x * chunk_size + 6 + 32 + j * 6 + 2);
					} else {
						SDLNet_Write16(0xffff, net_packet->data + 8 + x * chunk_size + 6 + 32 + j * 6);
						SDLNet_Write32(0xffffffff, net_packet->data + 8 + x * chunk_size + 6 + 32 + j * 6 + 2);
					}
				}
			}
			net_packet->len = 8 + barony::net::playerCapacity() * chunk_size;
		} else {
			constexpr int chunk_size = 6 + 32; // 6 bytes for player stats, 32 for name
			for ( int x = 0; x < barony::net::playerCapacity(); ++x )
			{
				net_packet->data[8 + x * chunk_size + 0] = client_disconnected[x]; // connectedness
				net_packet->data[8 + x * chunk_size + 1] = lockedSlots[x]; // locked state
				net_packet->data[8 + x * chunk_size + 2] = client_classes[x]; // class
				net_packet->data[8 + x * chunk_size + 3] = stats[x]->sex; // sex
				net_packet->data[8 + x * chunk_size + 4] = (Uint8)stats[x]->stat_appearance; // appearance
				net_packet->data[8 + x * chunk_size + 5] = (Uint8)stats[x]->playerRace; // player race

				char shortname[32];
				snprintf(shortname, sizeof(shortname), "%s", stats[x]->name);
				memcpy(net_packet->data + 8 + x * chunk_size + 6, shortname, sizeof(shortname)); // name
			}
			net_packet->len = 8 + barony::net::playerCapacity() * chunk_size;
		}
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		if ( directConnect )
		{
		    sendPacketSafe(net_sock, -1, net_packet, 0);
			return NET_LOBBY_JOIN_DIRECTIP_SUCCESS;
		}
		else
		{
			return NET_LOBBY_JOIN_P2P_SUCCESS;
		}
	}
}

/*-------------------------------------------------------------------------------

	receiveEntity

	receives entity data from server

-------------------------------------------------------------------------------*/

