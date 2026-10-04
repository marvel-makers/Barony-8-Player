#include "../private/common.hpp"

void sendEntityTCP(Entity* entity, int c)
{
	// deprecated
}

void sendEntityUDP(Entity* entity, int c, bool guarantee)
{
	int j;

	if ( entity == nullptr)
	{
		return;
	}
	if ( client_disconnected[c] == true || players[c]->isLocalPlayer() )
	{
		return;
	}
	if ( c <= 0 )
	{
		return;
	}

	// send entity data to the client
	strcpy((char*)net_packet->data, "ENTU");
	SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
	SDLNet_Write16(static_cast<Uint16>(entity->sprite), &net_packet->data[8]);
	SDLNet_Write16(static_cast<Sint16>(entity->x * 32), &net_packet->data[10]);
	SDLNet_Write16(static_cast<Sint16>(entity->y * 32), &net_packet->data[12]);
	SDLNet_Write16(static_cast<Sint16>(entity->z * 32), &net_packet->data[14]);
	net_packet->data[16] = static_cast<Sint8>(entity->sizex);
	net_packet->data[17] = static_cast<Sint8>(entity->sizey);
	net_packet->data[18] = static_cast<Uint8>(entity->scalex * 128);
	net_packet->data[19] = static_cast<Uint8>(entity->scaley * 128);
	net_packet->data[20] = static_cast<Uint8>(entity->scalez * 128);
	SDLNet_Write16(static_cast<Sint16>(entity->yaw * 256), &net_packet->data[21]);
	SDLNet_Write16(static_cast<Sint16>(entity->pitch * 256), &net_packet->data[23]);
	SDLNet_Write16(static_cast<Sint16>(entity->roll * 256), &net_packet->data[25]);
	net_packet->data[27] = static_cast<Sint8>(entity->focalx * 8);
	net_packet->data[28] = static_cast<Sint8>(entity->focaly * 8);
	net_packet->data[29] = static_cast<Sint8>(entity->focalz * 8);
	if ( entity->behavior == &actDeathGhost )
	{
		Uint32 flags = entity->skill[2];
		flags |= ((entity->monsterSpecialState) & 0xFF) << 8;
		flags |= ((entity->skill[10]) & 0xFFFF) << 16;
		SDLNet_Write32(flags, &net_packet->data[30]);
	}
	else
	{
		SDLNet_Write32(entity->skill[2], &net_packet->data[30]);
	}
	net_packet->data[34] = 0;
	net_packet->data[35] = 0;
	for (j = 0; j < 16; j++)
	{
		if ( entity->flags[j] )
		{
			net_packet->data[34 + j / 8] |= power(2, j - (j / 8) * 8);
		}
	}
	SDLNet_Write32(ticks, &net_packet->data[36]);
	SDLNet_Write16(static_cast<Sint16>(entity->vel_x * 32), &net_packet->data[40]);
	SDLNet_Write16(static_cast<Sint16>(entity->vel_y * 32), &net_packet->data[42]);
	SDLNet_Write16(static_cast<Sint16>(entity->vel_z * 32), &net_packet->data[44]);
	net_packet->data[46] = 0;
	for ( j = 0; j < 8; j++ )
	{
		if ( entity->flags[j + 16] )
		{
			net_packet->data[46 + j / 8] |= power(2, j - (j / 8) * 8);
		}
	}
	net_packet->address.host = net_clients[c - 1].host;
	net_packet->address.port = net_clients[c - 1].port;
	net_packet->len = ENTITY_PACKET_LENGTH;

	// sometimes you want more insurance that the entity update arrives
	if ( guarantee )
	{
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
	else
	{
		sendPacket(net_sock, -1, net_packet, c - 1);
	}
	if ( entity->clientsHaveItsStats )
	{
		entity->serverUpdateEffectsForEntity(false);
	}
}

/*-------------------------------------------------------------------------------

	sendMapSeedTCP

	Sends the seed necessary to generate the next map

-------------------------------------------------------------------------------*/

void sendMapSeedTCP(int c)
{
	// deprecated
}

/*-------------------------------------------------------------------------------

	sendMapTCP

	Sends all map data to clients

-------------------------------------------------------------------------------*/

void sendMapTCP(int c)
{
	// deprecated
}

/*-------------------------------------------------------------------------------

	serverUpdateBodypartIDs

	Updates the uid numbers of all the given bodyparts for the given entity

-------------------------------------------------------------------------------*/

void serverUpdateBodypartIDs(Entity* entity)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "BDYI");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		node_t* node;
		int i;
		for ( i = 0, node = entity->children.first; node != nullptr; node = node->next, i++ )
		{
			if ( i < 1 || (i < 2 && entity->behavior == &actMonster) )
			{
				continue;
			}
			auto tempEntity = static_cast<Entity*>(node->element);
			if ( entity->behavior == &actMonster )
			{
				SDLNet_Write32(tempEntity->getUID(), &net_packet->data[8 + 4 * (i - 2)]);
			}
			else
			{
				SDLNet_Write32(tempEntity->getUID(), &net_packet->data[8 + 4 * (i - 1)]);
			}
		}
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 8 + (list_Size(&entity->children) - 2) * 4;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

/*-------------------------------------------------------------------------------

	serverUpdateEntityBodypart

	Updates the given bodypart of the given entity for all clients

-------------------------------------------------------------------------------*/

//int numPlayerBodypartUpdates = 0;
//int numMonsterBodypartUpdates = 0;
//Uint32 lastbodypartTick = 0;

void serverUpdateEntityBodypart(Entity* entity, int bodypart)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "ENTB");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		net_packet->data[8] = bodypart;
		node_t* node = list_Node(&entity->children, bodypart);
		if ( !node )
		{
			continue;
		}
		auto tempEntity = static_cast<Entity*>(node->element);
		SDLNet_Write32(tempEntity->sprite, &net_packet->data[9]);
		net_packet->data[13] = (tempEntity->flags[INVISIBLE] ? 1 : 0);
		net_packet->data[13] |= (tempEntity->flags[INVISIBLE_DITHER] ? (1 << 1) : 0);
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 14;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
	//if ( entity->behavior == &actPlayer )
	//{
	//	++numPlayerBodypartUpdates;
	//}
	//else if ( entity->behavior == &actMonster )
	//{
	//	++numMonsterBodypartUpdates;
	//}
	//if ( lastbodypartTick == 0 )
	//{
	//	lastbodypartTick = ticks;
	//}
	//if ( ticks - lastbodypartTick >= 250 )
	//{
	//	messagePlayer(0, "Bodypart updates (%ds) players: %d, monster: %d", (ticks - lastbodypartTick) / 50, numPlayerBodypartUpdates, numMonsterBodypartUpdates);
	//	lastbodypartTick = 0;
	//	numMonsterBodypartUpdates = 0;
	//	numPlayerBodypartUpdates = 0;
	//}
}

/*-------------------------------------------------------------------------------

	serverUpdateEntitySprite

	Updates the given entity's sprite for all clients

-------------------------------------------------------------------------------*/

void serverUpdateEntitySprite(Entity* entity)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "ENTA");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		SDLNet_Write32(entity->sprite, &net_packet->data[8]);
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 12;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

/*-------------------------------------------------------------------------------

	serverUpdateEntitySkill

	Updates a specific entity skill for all clients

-------------------------------------------------------------------------------*/

void serverUpdateEntitySkill(Entity* entity, int skill)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "ENTS");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		net_packet->data[8] = skill;
		SDLNet_Write32(entity->skill[skill], &net_packet->data[9]);
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 13;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

/*-------------------------------------------------------------------------------

serverUpdateEntitySkill

Updates a specific entity skill for all clients

-------------------------------------------------------------------------------*/

void serverUpdateEntityStatFlag(Entity* entity, int flag)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	if ( !entity->getStats() )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "ENSF");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		net_packet->data[8] = flag;
		SDLNet_Write32(entity->getStats()->MISC_FLAGS[flag], &net_packet->data[9]);
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 13;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

/*-------------------------------------------------------------------------------

serverUpdateEntityFSkill

Updates a specific entity fskill for all clients

-------------------------------------------------------------------------------*/

void serverUpdateEntityFSkill(Entity* entity, int fskill)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "ENFS");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		net_packet->data[8] = fskill;
		SDLNet_Write16(static_cast<Sint16>(entity->fskill[fskill] * 256), &net_packet->data[9]);
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 11;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

/*-------------------------------------------------------------------------------

serverSpawnMiscParticles

Spawns misc particle effects for all clients

-------------------------------------------------------------------------------*/

void serverSpawnMiscParticles(Entity* entity, int particleType, int particleSprite, Uint32 optionalUid, Uint32 duration, Uint32 optionalData)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "SPPE");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		net_packet->data[8] = particleType;
		SDLNet_Write16(particleSprite, &net_packet->data[9]);
		SDLNet_Write32(optionalUid, &net_packet->data[11]);
		SDLNet_Write32(duration, &net_packet->data[15]);
		SDLNet_Write32(optionalData, &net_packet->data[19]);
		net_packet->len = 23;
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

/*-------------------------------------------------------------------------------

serverSpawnMiscParticlesAtLocation

Spawns misc particle effects for all clients at given coordinates.

-------------------------------------------------------------------------------*/

void serverSpawnMiscParticlesAtLocation(Sint16 x, Sint16 y, Sint16 z, int particleType, 
	int particleSprite, Uint32 duration, Uint32 optionalData, Uint32 optionalUID)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "SPPL");
		SDLNet_Write16(x, &net_packet->data[4]);
		SDLNet_Write16(y, &net_packet->data[6]);
		SDLNet_Write16(z, &net_packet->data[8]);
		net_packet->data[10] = particleType;
		SDLNet_Write16(particleSprite, &net_packet->data[11]);
		SDLNet_Write32(duration, &net_packet->data[13]);
		SDLNet_Write32(optionalData, &net_packet->data[17]);
		SDLNet_Write32(optionalUID, &net_packet->data[21]);
		net_packet->len = 25;
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

/*-------------------------------------------------------------------------------

	serverUpdateEntityFlag

	Updates a specific entity flag for all clients

-------------------------------------------------------------------------------*/

void serverUpdateEntityFlag(Entity* entity, int flag)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "ENTF");
		SDLNet_Write32(entity->getUID(), &net_packet->data[4]);
		net_packet->data[8] = flag;
		net_packet->data[9] = entity->flags[flag];
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 10;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

void serverUpdateMapTileFlag(Sint16 x, Sint16 y, int layer, Uint32 flagSet, Uint32 flagRemove)
{
	int c;
	if ( multiplayer != SERVER )
	{
		return;
	}
	for ( c = 1; c < barony::net::playerCapacity(); c++ )
	{
		if ( client_disconnected[c] || players[c]->isLocalPlayer() )
		{
			continue;
		}
		strcpy((char*)net_packet->data, "MAPT");
		SDLNet_Write16(x, &net_packet->data[4]);
		SDLNet_Write16(y, &net_packet->data[6]);
		SDLNet_Write32(flagSet, &net_packet->data[8]);
		SDLNet_Write32(flagRemove, &net_packet->data[12]);
		net_packet->data[16] = layer;
		net_packet->address.host = net_clients[c - 1].host;
		net_packet->address.port = net_clients[c - 1].port;
		net_packet->len = 17;
		sendPacketSafe(net_sock, -1, net_packet, c - 1);
	}
}

Entity* receiveEntity(Entity* entity)
{
	bool newentity = false;
	int c;

	//TODO: Find out if this is needed.
	/*bool oldeffects[NUMEFFECTS];
	Stat* entityStats = entity->getStats();

	for ( int i = 0; i < NUMEFFECTS; ++i )
	{
		if ( !entityStats )
		{
			oldeffects[i] = 0;
		}
		else
		{
			oldeffects[i] = entityStats->EFFECTS[i];
		}
	}
	//Yes, it is necessary. I don't think I like this solution though, will try something else.
	*/

    Sint32 oldSprite = 0;
	if ( entity == nullptr )
	{
		newentity = true;
		entity = newEntity(SDLNet_Read16(&net_packet->data[8]), 0, map.entities, nullptr);
	}
	else
	{
	    oldSprite = entity->sprite;
		entity->sprite = static_cast<int>(SDLNet_Read16(&net_packet->data[8]));
	}

    // for certain monsters, we don't want to use certain bytes,
    // because voxel-animated creatures (like rats and slimes)
    // need to move vertically for their animation.
	const auto monsterType = entity->getMonsterTypeFromSprite();
	bool excludeForAnimation =
	    !newentity &&
	    entity->behavior == &actMonster &&
		(monsterType == SLIME || ((monsterType == RAT || monsterType == SCARAB) &&
	    entity->skill[8])); // MONSTER_ATTACK

	//if ( Entity::getMonsterTypeFromSprite(entity->sprite) == SPIDER )
	//{
	//	if ( arachnophobia_filter )
	//	{
	//		switch ( entity->sprite )
	//		{
	//			case 267: // spider
	//				entity->sprite = 997; // crab
	//				break;
	//			case 823: // player spider
	//				entity->sprite = 1001; // player crab
	//				break;
	//			case 1118: // shelob
	//				entity->sprite = 1189; // bubbles
	//				break;
	//			default:
	//				break;
	//		}
	//	}
	//	else
	//	{
	//		switch ( entity->sprite )
	//		{
	//			case 997: // crab
	//				entity->sprite = 997; // spider
	//				break;
	//			case 1001: // player crab
	//				entity->sprite = 823; // player spider
	//				break;
	//			case 1189: // bubbles
	//				entity->sprite = 1118; // bubbles
	//				break;
	//			default:
	//				break;
	//		}
	//	}
	//}

	if (excludeForAnimation) {
		if ( monsterType == SLIME && Entity::getMonsterTypeFromSprite(oldSprite) != SLIME )
		{
			// take this sprite as we had editor data (e.g sprite 10 or 79)
		}
		else
		{
			entity->sprite = oldSprite;
		}
	}

	if ( entity->behavior == &actItem && entity->itemFollowUID != 0 )
	{
		excludeForAnimation = true;
	}
	const bool excludeYaw =
		entity->behavior == &actMagiclightBall
		|| (entity->behavior == &actLeafPile)
		|| (entity->behavior == &actItem && entity->itemFollowUID != 0);

	entity->lastupdate = ticks;
	entity->lastupdateserver = SDLNet_Read32(&net_packet->data[36]);
	entity->setUID(static_cast<int>(SDLNet_Read32(&net_packet->data[4]))); // remember who I am
	entity->new_x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10])) / 32.0;
	entity->new_y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[12])) / 32.0;
	if (!excludeForAnimation && (newentity || monsterType != SCARAB)) {
	    entity->new_z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[14])) / 32.0;
	}
	entity->sizex = static_cast<Sint8>(net_packet->data[16]);
	entity->sizey = static_cast<Sint8>(net_packet->data[17]);
	if (newentity || monsterType != SLIME) {
	    entity->scalex = net_packet->data[18] / 128.f;
	    entity->scaley = net_packet->data[19] / 128.f;
	    entity->scalez = net_packet->data[20] / 128.f;
	}
	if ( newentity || !excludeYaw )
	{
		entity->new_yaw = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[21])) / 256.0;
	}
	entity->new_pitch = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[23])) / 256.0;
	entity->new_roll = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[25])) / 256.0;
	if ( newentity )
	{
		entity->x = entity->new_x;
		entity->y = entity->new_y;
		entity->z = entity->new_z;
		entity->yaw = entity->new_yaw;
		entity->pitch = entity->new_pitch;
		entity->roll = entity->new_roll;
	}
	entity->focalx = static_cast<Sint8>(net_packet->data[27]) / 8.0;
	entity->focaly = static_cast<Sint8>(net_packet->data[28]) / 8.0;
	if (!excludeForAnimation) {
	    entity->focalz = static_cast<Sint8>(net_packet->data[29]) / 8.0;
	}
	for (c = 0; c < 16; ++c)
	{
		if ( net_packet->data[34 + c / 8]&power(2, c - (c / 8) * 8) )
		{
			entity->flags[c] = true;
		}
	}
	for ( c = 0; c < 8; ++c ) // new flags 16-23
	{
		if ( net_packet->data[46 + c / 8] & power(2, c - (c / 8) * 8) )
		{
			entity->flags[c + 16] = true;
		}
	}
	entity->vel_x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[40])) / 32.0;
	entity->vel_y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[42])) / 32.0;
	entity->vel_z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[44])) / 32.0;

	return entity;
}

/*-------------------------------------------------------------------------------

	clientActions

	Assigns an action to a given entity based mainly on its sprite

-------------------------------------------------------------------------------*/

void clientActions(Entity* entity)
{
	int playernum;

	// this code assigns behaviors based on the sprite (model) number
	switch ( entity->sprite )
	{
	    case 1163:
	    case 1164:
	    case 1165:
	    case 1166:
	    case 1167:
	    case 1168:
	    case 1169:
		case 1631:
		case 1:
			entity->behavior = &actDoorFrame;
			break;
		case 2:
			entity->behavior = &actDoor;
			break;
		case 1162:
			entity->behavior = &actIronDoor;
			break;
		case 3:
			entity->behavior = &actTorch;
			entity->flags[NOUPDATE] = 1;
			break;
		case 160:
		case 203:
		case 212:
		case 213:
		case 214:
		case 682:
		case 681:
		case 1398:
		case 1399:
		case 1400:
			entity->flags[NOUPDATE] = true;
			break;
		case 162:
			entity->behavior = &actCampfire;
			entity->flags[NOUPDATE] = true;
			break;
		case 163:
			entity->skill[2] = static_cast<int>(SDLNet_Read32(&net_packet->data[30]));
			entity->behavior = &actFountain;
			break;
		case 174:
			if (SDLNet_Read32(&net_packet->data[30]) != 0)
			{
				entity->behavior = &actMagiclightBall; //TODO: Finish this here. I think this gets reassigned every time the entity is recieved? Make sure.
			}
			break;
		case 185:
			entity->behavior = &actSwitch;
			break;
		case 186:
			entity->behavior = &actGate;
			break;
		case 216:
		case 1790:
			entity->behavior = &actChestLid;
			break;
		case 254:
		case 255:
		case 256:
		case 257:
			entity->behavior = &actPortal;
			break;
		case 273:
			entity->behavior = &actMCaxe;
			break;
		case 278:
		case 279:
		case 280:
		case 281:
			entity->behavior = &actWinningPortal;
			break;
		case 282:
			entity->behavior = &actSpearTrap;
			break;
		case 578:
			entity->behavior = &actPowerCrystal;
			break;
		case 586:
			entity->behavior = &actSwitchWithTimer;
			break;
		case 601:
			entity->behavior = &actPedestalBase;
			break;
		case 602:
		case 603:
		case 604:
		case 605:
			entity->behavior = &actPedestalOrb;
			break;
		case 667:
		case 668:
			entity->behavior = &actBeartrap;
			break;
		case 674:
		case 675:
		case 676:
		case 677:
			entity->behavior = &actCeilingTile;
			entity->flags[NOUPDATE] = true;
			break;
		case 629:
			entity->behavior = &actColumn;
			entity->flags[NOUPDATE] = true;
			break;
		case 632:
		case 633:
			entity->behavior = &actPistonCam;
			entity->flags[NOUPDATE] = true;
			break;
		case 130:
		case 1379:
			entity->behavior = &actGoldBag;
			break;
		case 1481:
			entity->behavior = &actDaedalusShrine;
			break;
		case 1484:
			entity->behavior = &actAssistShrine;
			break;
		case 1622:
			entity->behavior = &actCauldron;
			break;
		case 1617:
			entity->behavior = &actWorkbench;
			break;
		case 1619:
		case 1620:
			entity->behavior = &actMailbox;
			break;
		case 1585:
		case 1586:
		case 1587:
		case 1588:
		case 1589:
		case 1590:
		case 1591:
		case 1592:
			// wall lock keys
			entity->behavior = &actEmpty;
			entity->flags[NOUPDATE] = true;
			break;
		case 1786:
			entity->behavior = &actGreasePuddleSpawner;
			entity->flags[NOUPDATE] = true;
			break;
		case 1151:
		case 1152:
			// wall buttons
			entity->behavior = &actEmpty;
			entity->flags[NOUPDATE] = true;
			break;
		case 1809:
			entity->behavior = &actParticleDemesneDoor;
			entity->flags[NOUPDATE] = true;
			break;
		case 1913:
			entity->behavior = &actLeafPile;
			break;
		case Player::Ghost_t::GHOST_MODEL_P1:
		case Player::Ghost_t::GHOST_MODEL_P2:
		case Player::Ghost_t::GHOST_MODEL_P3:
		case Player::Ghost_t::GHOST_MODEL_P4:
		case Player::Ghost_t::GHOST_MODEL_PX:
			// player ghosts
			playernum = 0xFF & SDLNet_Read32(&net_packet->data[30]);
			if ( playernum >= 0 && playernum < barony::net::playerCapacity() )
			{
				if ( players[playernum] )
				{
					players[playernum]->ghost.my = entity;
				}
				entity->skill[2] = playernum;
				entity->behavior = &actDeathGhost;
				if ( playernum == clientnum && multiplayer == CLIENT )
				{
					entity->flags[UPDATENEEDED] = false;
				}
				else
				{
					entity->flags[UPDATENEEDED] = true;
				}
				entity->flags[PASSABLE] = true;
				entity->flags[INVISIBLE] = true;
				entity->flags[GENIUS] = true;
				Uint32 specialFlags = (SDLNet_Read32(&net_packet->data[30]) >> 8) & 0xFFFFFF;
				if ( (specialFlags & 0xFF) )
				{
					entity->monsterSpecialState = (specialFlags & 0xFF);
				}
				if ( (specialFlags >> 8) & 0xFFFF )
				{
					int cosmeticSprite = (specialFlags >> 8) & 0xFFFF;
					entity->skill[10] = cosmeticSprite;
				}
				entity->sizex = 2;
				entity->sizey = 2;
			}
			break;
		default:
			if ( entity->isPlayerHeadSprite() )
			{
				// these are all player heads
				playernum = SDLNet_Read32(&net_packet->data[30]);
				if ( playernum >= 0 && playernum < barony::net::playerCapacity() )
				{
					if ( players[playernum] && players[playernum]->entity )
					{
						players[playernum]->entity = entity;
					}
					entity->skill[2] = playernum;
					entity->behavior = &actPlayer;
				}
			}
			break;
	}

	// if the above method failed, we check the value of skill[2] (stored in net_packet->data[30]) and assign an action based on that
	if ( entity->behavior == nullptr)
	{
		Sint32 c = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[30]));
		if ( c < 0 )
		{
			switch ( c )
			{
				case -4:
					entity->behavior = &actMonster;
					entity->skill[2] = -4;
					break;
				case -5:
					entity->behavior = &actItem;
					break;
				case -6:
					entity->behavior = &actGib;
					break;
				case -7:
					entity->behavior = &actEmpty;
					if ( entity->sprite == 989 ) // boulder_lava.vox
					{
						entity->flags[BURNABLE] = true;
					}
					break;
				case -8:
					entity->behavior = &actThrown;
					break;
				case -9:
					entity->behavior = &actLiquid;
					break;
				case -10:
					entity->behavior = &actMagiclightBall;
					break;
				case -11:
					entity->behavior = &actMagicClient;
					break;
				case -12:
					entity->behavior = &actMagicClientNoLight;
					break;
				case -13:
					entity->behavior = &actParticleSapCenter;
					break;
				case -14:
					entity->behavior = &actDecoyBox;
					break;
				case -15:
					entity->behavior = &actBomb;
					break;
				case -16:
					entity->behavior = &actBoulder;
					break;
				case -18:
					entity->behavior = &actParticleFloorMagic;
					entity->flags[NOUPDATE] = true;
					break;
				default:
					if ( static_cast<Uint8>(c & 0xFF) == 17 )
					{
						entity->arrowShotByWeapon = (c >> 8) & 0xFFF;
						int dropOffModifier = (c >> 20) & 0xF;
						entity->arrowDropOffEquipmentModifier = dropOffModifier - 8;
						entity->behavior = &actArrow;
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 19 )
					{
						entity->particleTimerDuration = (c >> 8) & 0xFFF;
						entity->particleTimerCountdownAction = (c >> 20) & 0xFF;
						entity->behavior = &actParticleTimer;
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 20 )
					{
						entity->behavior = &actParticleFloorMagic;
						entity->skill[2] = c;
						floorMagicClientReceive(entity);
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 21 )
					{
						entity->behavior = &actParticleFloorMagic;
						entity->skill[2] = c;
						entity->flags[NOUPDATE] = true;
						floorMagicClientReceive(entity);
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 22 )
					{
						entity->behavior = &actParticleWave;
						entity->skill[2] = c;
						entity->flags[NOUPDATE] = true;
						particleWaveClientReceive(entity);
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 23 )
					{
						entity->behavior = &actWind;
						entity->skill[2] = c;
						entity->flags[NOUPDATE] = true;
						particleWaveClientReceive(entity);
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 24 )
					{
						entity->behavior = &actRadiusMagic;
						entity->skill[2] = c;
						radiusMagicClientReceive(entity);
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 25 )
					{
						entity->behavior = &actColliderDecoration;
						entity->skill[2] = c;
						entity->flags[NOUPDATE] = true;
						entity->colliderDamageTypes = (c >> 8) & 0xFF;
						entity->colliderSpellEvent = (c >> 16) & 0xFF;
						Entity::colliderAssignProperties(entity, false, &map);
					}
					else if ( static_cast<Uint8>(c & 0xFF) == 26 )
					{
						entity->behavior = &actTeleporter;
						entity->skill[2] = c;
						entity->flags[NOUPDATE] = true;
						int duration = (c >> 8) & 0xFFFF;
						int dir = (c >> 24) & 0xF;
						tunnelPortalSetAttributes(entity, duration, dir);
					}
					break;
			}
		}
	}
}

/*-------------------------------------------------------------------------------

	clientHandlePacket

	Called by clientHandleMessages. Does the actual handling of a packet.

-------------------------------------------------------------------------------*/

