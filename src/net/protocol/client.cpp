#include "../private/common.hpp"
#include "../transport/transport.hpp"


static void changeLevel() {
	if ( net_packet->data[14] != 0 )
	{
		// loading a custom map name.
		char buf[128] = "";
		strcpy(buf, (char*)&net_packet->data[14]);
		loadCustomNextMap = buf;
	}

	if ( MainMenu::isCutsceneActive() )
	{
		introstage = 1; // return to normal game functionality
		pauseGame(1, false); // unpause game
	}

	// hack to fix these things from breaking everything...
	for ( int i = 0; i < barony::net::playerCapacity(); ++i )
	{
		players[i]->hud.arm = nullptr;
		players[i]->hud.weapon = nullptr;
		players[i]->hud.magicLeftHand = nullptr;
		players[i]->hud.magicRightHand = nullptr;
		players[i]->hud.magicRangefinder = nullptr;
		players[i]->ghost.reset();
		FollowerMenu[i].recentEntity = nullptr;
		FollowerMenu[i].followerToCommand = nullptr;
		FollowerMenu[i].entityToInteractWith = nullptr;
		CalloutMenu[i].closeCalloutMenuGUI();
		CalloutMenu[i].callouts.clear();
	}

	// stop all sounds
#ifdef USE_FMOD
	if ( sound_group )
	{
		sound_group->stop();
	}
	if ( soundAmbient_group )
	{
		soundAmbient_group->stop();
	}
	if ( soundEnvironment_group )
	{
		soundEnvironment_group->stop();
	}
	if ( soundNotification_group )
	{
		soundNotification_group->stop();
	}
	ensembleSounds.stopPlaying(true);
	VoiceChat.deinitRecording(false);
#elif defined USE_OPENAL
	if ( sound_group )
	{
		OPENAL_ChannelGroup_Stop(sound_group);
	}
	if ( soundAmbient_group )
	{
		OPENAL_ChannelGroup_Stop(soundAmbient_group);
	}
	if ( soundEnvironment_group )
	{
		OPENAL_ChannelGroup_Stop(soundEnvironment_group);
	}
#endif
	if ( openedChest[clientnum] )
	{
		closeChestClientside(clientnum);
	}

	int prevcurrentlevel = currentlevel;
	int prevsecretfloor = secretlevel;
	std::string prevmapname = map.name;

	// unlock some steam achievements
	if ( !secretlevel )
	{
		switch ( currentlevel )
		{
		case 0:
			steamAchievement("BARONY_ACH_ENTER_THE_DUNGEON");
			break;
		default:
			break;
		}
	}

	MainMenu::destroyMainMenu();
	movie = false;

	// setup level change
	const int newlevel = static_cast<Sint8>(net_packet->data[13]);
	printlog("Received order to change level to %d (from %d).\n", newlevel, currentlevel);
	currentlevel = newlevel;

	if ( !secretlevel )
	{
		switch ( currentlevel )
		{
			case 5:
				steamAchievement("BARONY_ACH_TWISTY_PASSAGES");
				break;
			case 10:
				steamAchievement("BARONY_ACH_JUNGLE_FEVER");
				break;
			case 15:
				steamAchievement("BARONY_ACH_SANDMAN");
				break;
			case 30:
				steamAchievement("BARONY_ACH_SPELUNKY");
				break;
			case 35:
				if ( ((completionTime / TICKS_PER_SECOND) / 60) <= 45 )
				{
					conductGameChallenges[CONDUCT_BLESSED_BOOTS_SPEED] = 1;
				}
				break;
			default:
				break;
		}
	}

	list_FreeAll(&removedEntities);
	for ( auto node = map.entities->first; node != nullptr; node = node->next )
	{
		auto entity = static_cast<Entity*>(node->element);
		auto entity2 = newEntity(entity->sprite, 1, &removedEntities, nullptr);
		entity2->setUID(entity->getUID());
	}
	for ( int i = 0; i < barony::net::playerCapacity(); ++i )
	{
		list_FreeAll(&stats[i]->FOLLOWERS);
	}

	// load next level
	darkmap = false;
	secretlevel = net_packet->data[4];
	mapseed = SDLNet_Read32(&net_packet->data[5]);
	numplayers = 0;
	entity_uids = SDLNet_Read32(&net_packet->data[9]);
	printlog("Received map seed: %d. Entity UID start: %d\n", mapseed, entity_uids);

	for ( int i = 0; i < barony::net::playerCapacity(); ++i )
	{
		minimapPings[i].clear(); // clear minimap pings
        auto& camera = players[i]->camera();
        camera.globalLightModifierActive = GLOBAL_LIGHT_MODIFIER_STOPPED;
        camera.luminance = defaultLuminance;
		players[i]->hud.followerBars.clear();
		spellcastingAnimationManager_deactivate(&cast_animation[i]);
	}
	EnemyHPDamageBarHandler::dumpCache();
	AOEIndicators_t::cleanup();
	monsterAllyFormations.reset();
	particleTimerEmitterHitEntities.clear();
	particleTimerEffects.clear();
	monsterTrapIgnoreEntities.clear();
	minimapHighlights.clear();

	// clear follower menu entities.
	FollowerMenu[clientnum].closeFollowerMenuGUI(true);
	CalloutMenu[clientnum].closeCalloutMenuGUI();

	bool died = stats[clientnum] && stats[clientnum]->HP <= 0;

    // load map file
	loading = true;
    createLevelLoadScreen(5);
    std::atomic_bool loading_done {false};
    auto loading_task = std::async(std::launch::async, [&loading_done](){
	    gameplayCustomManager.readFromFile();
		if ( gameplayCustomManager.inUse() )
		{
			conductGameChallenges[CONDUCT_MODDED] = 1;
			Mods::disableSteamAchievements = true;
		}
        updateLoadingScreen(10);

	    int checkMapHash = -1;
	    int result = physfsLoadMapFile(currentlevel, mapseed, false, &checkMapHash);
	    if (!verifyMapHash(map.filename, checkMapHash))
	    {
		    conductGameChallenges[CONDUCT_MODDED] = 1;
			Mods::disableSteamAchievements = true;
	    }
        updateLoadingScreen(50);

	    numplayers = 0;
	    assignActions(&map);
        updateLoadingScreen(55);

	    generatePathMaps();
        updateLoadingScreen(80);

        node_t *node, *nextnode;
	    for ( node = map.entities->first; node != nullptr; node = nextnode )
	    {
		    nextnode = node->next;
		    auto entity = static_cast<Entity*>(node->element);
		    if ( entity->flags[NOUPDATE] )
		    {
			    list_RemoveNode(entity->mynode);    // we're anticipating this entity data from server
		    }
	    }
        updateLoadingScreen(99);

	    loading_done = true;
	    return result;
	});
    while (!loading_done)
    {
	    doLoadingScreen();
	    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    destroyLoadingScreen();
	loading = false;
    int result = loading_task.get();

    clearChunks();
    createChunks();

	// (special) unlock temple achievement
	if ( secretlevel && currentlevel == 8 )
	{
		steamAchievement("BARONY_ACH_TRICKS_AND_TRAPS");
	}

	Player::Minimap_t::mapDetails.clear();

	if ( !secretlevel )
	{
		messagePlayer(clientnum, MESSAGE_PROGRESSION, Language::get(710), currentlevel);
	}
	else
	{
		messagePlayer(clientnum, MESSAGE_PROGRESSION, Language::get(711), map.name);
	}
	if ( !secretlevel && result )
	{
		switch ( currentlevel )
		{
			case 2:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(712));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(712)));
				break;
			case 3:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(713));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(713)));
				break;
			case 7:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(714));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(714)));
				break;
			case 8:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(715));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(715)));
				break;
			case 11:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(716));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(716)));
				break;
			case 13:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(717));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(717)));
				break;
			case 16:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(718));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(718)));
				break;
			case 18:
				messagePlayer(clientnum, MESSAGE_HINT, Language::get(719));
				Player::Minimap_t::mapDetails.push_back(std::make_pair("secret_exit_description", Language::get(719)));
				break;
			default:
				break;
		}
	}
	if ( MFLAG_DISABLETELEPORT )
	{
		Player::Minimap_t::mapDetails.push_back(std::make_pair("map_flag_disable_teleport", Language::get(2382)));
	}
	if ( MFLAG_DISABLEOPENING )
	{
		Player::Minimap_t::mapDetails.push_back(std::make_pair("map_flag_disable_opening", Language::get(2382)));
	}
	if ( MFLAG_DISABLETELEPORT || MFLAG_DISABLEOPENING )
	{
		messagePlayer(clientnum, MESSAGE_HINT, Language::get(2382));
	}
	if ( MFLAG_DISABLELEVITATION )
	{
		messagePlayer(clientnum, MESSAGE_HINT, Language::get(2383));
		Player::Minimap_t::mapDetails.push_back(std::make_pair("map_flag_disable_levitation", Language::get(2383)));
	}
	if ( MFLAG_DISABLEDIGGING )
	{
		messagePlayer(clientnum, MESSAGE_HINT, Language::get(2450));
		Player::Minimap_t::mapDetails.push_back(std::make_pair("map_flag_disable_digging", Language::get(2450)));
	}
	if ( MFLAG_DISABLEHUNGER )
	{
		Player::Minimap_t::mapDetails.push_back(std::make_pair("map_flag_disable_hunger", ""));
	}

	if ( !died )
	{
		if ( stats[clientnum]->type == MYCONID && stats[clientnum]->playerRace == RACE_MYCONID && stats[clientnum]->stat_appearance == 0
			&& stats[clientnum]->helmet && gameStatistics[STATISTICS_NO_CAP] >= 0 )
		{
			gameStatistics[STATISTICS_NO_CAP]++;
			if ( gameStatistics[STATISTICS_NO_CAP] >= 5 )
			{
				steamAchievement("BARONY_ACH_NO_CAP");
			}
		}
		if ( stats[clientnum]->getEffectActive(EFF_GROWTH) >= 2
			&& ((stats[clientnum]->type == MYCONID && stats[clientnum]->playerRace == RACE_MYCONID)
				|| (stats[clientnum]->type == DRYAD && stats[clientnum]->playerRace == RACE_DRYAD)) && stats[clientnum]->stat_appearance == 0
			&& !stats[clientnum]->helmet && gameStatistics[STATISTICS_DONT_TOUCH_HAIR] >= 0 )
		{
			gameStatistics[STATISTICS_DONT_TOUCH_HAIR]++;
			if ( gameStatistics[STATISTICS_DONT_TOUCH_HAIR] >= 25 )
			{
				steamAchievement("BARONY_ACH_DONT_TOUCH_HAIR");
			}
		}
		if ( stats[clientnum]->type == SALAMANDER && stats[clientnum]->playerRace == RACE_SALAMANDER && stats[clientnum]->stat_appearance == 0
			&& stats[clientnum]->getEffectActive(EFF_SALAMANDER_HEART) >= 3 && stats[clientnum]->getEffectActive(EFF_SALAMANDER_HEART) <= 4
			&& gameStatistics[STATISTICS_GARGOYLES_QUEST] >= 0 )
		{
			gameStatistics[STATISTICS_GARGOYLES_QUEST]++;
			if ( gameStatistics[STATISTICS_GARGOYLES_QUEST] >= 10 )
			{
				steamAchievement("BARONY_ACH_GARGOYLES_QUEST");
			}
		}
		if ( stats[clientnum]->type == SALAMANDER && stats[clientnum]->playerRace == RACE_SALAMANDER && stats[clientnum]->stat_appearance == 0
			&& stats[clientnum]->getEffectActive(EFF_SALAMANDER_HEART) >= 1 && stats[clientnum]->getEffectActive(EFF_SALAMANDER_HEART) <= 2
			&& gameStatistics[STATISTICS_FIRE_FIGHTER] >= 0 )
		{
			gameStatistics[STATISTICS_FIRE_FIGHTER]++;
			if ( gameStatistics[STATISTICS_FIRE_FIGHTER] >= 5 )
			{
				steamAchievement("BARONY_ACH_FIRE_FIGHTER");
			}
		}
		if ( stats[clientnum]->type == SALAMANDER && stats[clientnum]->playerRace == RACE_SALAMANDER && stats[clientnum]->stat_appearance == 0
			&& !stats[clientnum]->getEffectActive(EFF_SALAMANDER_HEART)
			&& gameStatistics[STATISTICS_DISCIPLINE] >= 0 )
		{
			gameStatistics[STATISTICS_DISCIPLINE]++;
			if ( gameStatistics[STATISTICS_DISCIPLINE] >= 25 )
			{
				steamAchievement("BARONY_ACH_DISCIPLINE");
			}
		}
	}

	Compendium_t::Events_t::onLevelChangeEvent(clientnum, prevcurrentlevel, prevsecretfloor, prevmapname, died);
	for ( int i = 0; i < barony::net::playerCapacity(); ++i )
	{
		players[i]->compendiumProgress.playerAliveTimeTotal = 0;
		players[i]->compendiumProgress.playerGameTimeTotal = 0;
	}

	if ( gameModeManager.allowsSaves() )
	{
		saveGame();
	}

	Compendium_t::Events_t::writeItemsSaveData();
	Compendium_t::writeUnlocksSaveData();
#ifdef LOCAL_ACHIEVEMENTS
	LocalAchievements_t::writeToFile();
#endif
	printlog("Done.\n");

	if ( !strncmp(map.name, "Mages Guild", 11) )
	{
		messagePlayer(clientnum, MESSAGE_HINT, Language::get(2599));
	}
	fadeout = false;
	fadealpha = 255;
}

static std::unordered_map<Uint32, void(*)()> clientPacketHandlers = {
	// keep alive
	{'KPAL', [](){
		client_keepalive[0] = ticks;
	}},

	// entity update
	{'ENTU', [](){
		client_keepalive[0] = ticks; // don't timeout
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			if (SDLNet_Read32(&net_packet->data[36]) < entity->lastupdateserver )
			{
				// old packet, not used
			}
			else if ( entity->behavior == &actPlayer && entity->skill[2] == clientnum )
			{
				// don't update my player
			}
			else if ( entity->behavior == &actDeathGhost && entity->skill[2] == clientnum )
			{
				// don't update my ghost
			}
			else if ( entity->flags[NOUPDATE] )
			{
				// inform the server that it tried to update a no-update entity
				strcpy((char*)net_packet->data, "NOUP");
				net_packet->data[4] = clientnum;
				SDLNet_Write32(entity->getUID(), &net_packet->data[5]);
				net_packet->address.host = net_server.host;
				net_packet->address.port = net_server.port;
				net_packet->len = 9;
				sendPacket(net_sock, -1, net_packet, 0);
			}
			else
			{
				// receive the entity
				receiveEntity(entity);
				entity->behavior = nullptr;
				clientActions(entity);
			}
			return;
		}

		for ( auto node = removedEntities.first; node != nullptr; node = node->next )
		{
			auto entity2 = static_cast<Entity*>(node->element);
			if ( entity2->getUID() == static_cast<int>(SDLNet_Read32(&net_packet->data[4])) )
			{
				return;
			}
		}

		entity = receiveEntity(nullptr);
		// IMPORTANT! Assign actions to the objects the client has control over
		clientActions(entity);

		//if ( entity->behavior == &actPlayer && entity->skill[2] >= 0 && entity->skill[2] < MAXPLAYERS ) // respawned
		//{
		//	if ( !players[entity->skill[2]]->entity )
		//	{
		//		players[entity->skill[2]]->entity = entity;
		//		if ( entity->skill[2] == clientnum )
		//		{
		//			stats[entity->skill[2]]->HP = stats[entity->skill[2]]->MAXHP;
		//			node_t* nextnode = nullptr;
		//			for ( auto node = map.entities->first; node != NULL; node = nextnode )
		//			{
		//				nextnode = node->next;
		//				auto entity2 = (Entity*)node->element;
		//				if ( entity2 )
		//				{
		//					if ( entity2->behavior == &actDeathCam && entity2->skill[2] == clientnum )
		//					{
		//						list_RemoveNode(entity2->mynode);
		//					}
		//				}
		//			}
		//		}
		//	}
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
    }},

    // sneaking
    {'SNEK', [](){
        const int player = net_packet->data[4];
        if (!barony::net::validPlayer(player))
		{
			return;
		}
        stats[player]->sneaking = net_packet->data[5];
        return;
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
		}
	}},

	// update ghost bounce
	{'GHFS', []() {
		Entity* entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			entity->fskill[net_packet->data[8]] = (SDLNet_Read16(&net_packet->data[9]) / 256.0);
			playSoundEntityLocal(entity, 612 + local_rng.rand() % 3, 64);
		}
	}},

	{'EFFE', [](){
		/*
		* Packet breakdown:
		* [0][1][2][3]: "EFFE"
		* [4][5][6][7]: Entity's UID.
		* [8][9][10][11][12][13][14][15]: Entity's effects.
		*/

		Uint32 uid = static_cast<int>(SDLNet_Read32(&net_packet->data[4]));

		Entity* entity = uidToEntity(uid);

		if ( entity )
		{
			if ( entity->behavior == &actPlayer && entity->skill[2] == clientnum )
			{
				//Don't update this client's entity! Use the dedicated function for that.
				return;
			}

			Stat *stats = entity->getStats();
			if ( !stats )
			{
				entity->giveClientStats();
				stats = entity->getStats();
				if ( !stats )
				{
					return;
				}
			}

			for ( int i = 0; i < NUMEFFECTS; ++i )
			{
				if ( net_packet->data[8 + i / 8] & power(2, i - (i / 8) * 8) )
				{
					stats->setEffectValueUnsafe(i, 1);
				}
				else
				{
					stats->clearEffect(i);
				}
			}

			int numBytes = NUMEFFECTS / 8;

			int numEffectStrengths = net_packet->data[8 + numBytes];
			int index = 0;
			while ( numEffectStrengths > 0 )
			{
				int currentIndex = 8 + numBytes + 1 + index;
				if ( currentIndex + 1 >= NET_PACKET_SIZE || (currentIndex + 1 >= net_packet->len) )
				{
					// too much data to read, abort
					break;
				}
				int effectIndex = net_packet->data[currentIndex + 0];
				Uint8 effectStrength = net_packet->data[currentIndex + 1];
				stats->setEffectValueUnsafe(effectIndex, effectStrength);
				index += 2;
				--numEffectStrengths;
			}
		}
	}},

	// update entity skill
	{'ENTS', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			entity->skill[net_packet->data[8]] = SDLNet_Read32(&net_packet->data[9]);
		}
	}},

	// update entity fskill
	{'ENFS', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			entity->fskill[net_packet->data[8]] = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[9])) / 256.0;
		}
	}},

	// update entity bodypart
	{'ENTB', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			node_t* childNode = list_Node(&entity->children, net_packet->data[8]);
			if ( childNode )
			{
				auto tempEntity = static_cast<Entity*>(childNode->element);
				tempEntity->sprite = SDLNet_Read32(&net_packet->data[9]);
				tempEntity->skill[7] = tempEntity->sprite;
				tempEntity->flags[INVISIBLE] = (net_packet->data[13] & (1 << 0)) > 0 ? true : false;
				tempEntity->flags[INVISIBLE_DITHER] = (net_packet->data[13] & (1 << 1)) > 0 ? true : false;
			}
			/*else
			{
				if ( entity->behavior == &actPlayer )
				{
					messagePlayer(clientnum, MESSAGE_DEBUG, "actPlayer !childNode: %d", net_packet->data[8]);
				}
				else
				{
					messagePlayer(clientnum, MESSAGE_DEBUG, "!childNode: %d", net_packet->data[8]);
				}
			}*/
		}
	}},

	// bodypart ids
	{'BDYI', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			node_t* childNode;
			int c;
			for ( c = 0, childNode = entity->children.first; childNode != nullptr; childNode = childNode->next, c++ )
			{
				if ( c < 1 || (c < 2 && entity->behavior == &actMonster) )
				{
					continue;
				}
				auto tempEntity = static_cast<Entity*>(childNode->element);
				if ( tempEntity )
				{
					if ( entity->behavior == &actMonster )
					{
						tempEntity->setUID(SDLNet_Read32(&net_packet->data[8 + 4 * (c - 2)]));
					}
					else
					{
						tempEntity->setUID(SDLNet_Read32(&net_packet->data[8 + 4 * (c - 1)]));
					}
				}
			}
		}
	}},

	// update entity flag
	{'ENTF', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			entity->flags[net_packet->data[8]] = net_packet->data[9];
			if ( entity->behavior == &actMonster && net_packet->data[8] == USERFLAG2 )
			{
				// we should update the flags for all bodyparts (except for human and automaton heads, don't update the other bodyparts).
				if ( !(entity->isPlayerHeadSprite() || entity->sprite == 467 || !monsterChangesColorWhenAlly(nullptr, entity)) )
				{
					int bodypart = 0;
					for ( node_t* node = entity->children.first; node != nullptr; node = node->next )
					{
						if ( bodypart >= LIMB_HUMANOID_TORSO )
						{
							auto tmp = static_cast<Entity*>(node->element);
							if ( tmp )
							{
								tmp->flags[USERFLAG2] = entity->flags[net_packet->data[8]];
							}
						}
						++bodypart;
					}
				}
			}
		}
	}},

	// player movement correction
	{'PMOV', [](){
		if ( players[clientnum] == nullptr || players[clientnum]->entity == nullptr )
		{
			return;
		}
		players[clientnum]->entity->x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4])) / 32.0;
		players[clientnum]->entity->y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6])) / 32.0;
	}},

	// player ghost movement correction
	{'GMOV', []() {
		if ( players[clientnum] == nullptr || players[clientnum]->ghost.my == nullptr )
		{
			return;
		}
		players[clientnum]->ghost.my->x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4])) / 32.0;
		players[clientnum]->ghost.my->y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6])) / 32.0;
	}},

	// update health
	{'UPHP', [](){
		if ( static_cast<Monster>(SDLNet_Read32(&net_packet->data[8])) != NOTHING )
		{
			if ( SDLNet_Read32(&net_packet->data[4]) < stats[clientnum]->HP )
			{
				cameravars[clientnum].shakex += .1;
				cameravars[clientnum].shakey += 10;
			}
			else
			{
				cameravars[clientnum].shakex += .05;
				cameravars[clientnum].shakey += 5;
			}
		}
		stats[clientnum]->HP = SDLNet_Read32(&net_packet->data[4]);
		return;
	}},

	// server sent item details.
	{'ITMU', [](){
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			Uint32 itemTypeAndIdentified = SDLNet_Read32(&net_packet->data[8]);
			Uint32 statusBeatitudeQuantityAppearance = SDLNet_Read32(&net_packet->data[12]);

			entity->skill[10] = static_cast<ItemType>((itemTypeAndIdentified >> 16) & 0xFFFF); //type
			entity->skill[15] = (itemTypeAndIdentified) & 0xFFFF;
			entity->skill[11] = static_cast<Uint8>((statusBeatitudeQuantityAppearance >> 24) & 0xFF); // status
			entity->skill[12] = static_cast<Sint8>((statusBeatitudeQuantityAppearance >> 16) & 0xFF); // beatitude
			entity->skill[13] = static_cast<Uint8>((statusBeatitudeQuantityAppearance >> 8) & 0xFF); // quantity
			entity->skill[14] = static_cast<Uint8>((statusBeatitudeQuantityAppearance) & 0xFF); // appearance
			if ( net_packet->len >= 16 )
			{
				if ( entity->skill[10] >= 0 && entity->skill[10] < NUMITEMS )
				{
					if ( items[entity->skill[10]].category == TOME_SPELL )
					{
						entity->skill[14] = SDLNet_Read16(&net_packet->data[16]);
					}
				}
			}
			entity->itemReceivedDetailsFromServer = 1;
		}
	}},

	// breakable dropped item
	{ 'BREK', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			if ( entity->behavior == &actItem )
			{
				//entity->flags[UPDATENEEDED] = true;
				if ( entity->flags[INVISIBLE] )
				{
					entity->flags[INVISIBLE] = false;
					entity->vel_x = (0.25 + .025 * (local_rng.rand() % 11)) * cos(entity->yaw);
					entity->vel_y = (0.25 + .025 * (local_rng.rand() % 11)) * sin(entity->yaw);
					entity->vel_z = (-40 - local_rng.rand() % 5) * .01;
					entity->itemContainer = 0;
					entity->z = 0.0;
					entity->itemNotMoving = 0;
					entity->itemNotMovingClient = 0;
					entity->flags[USERFLAG1] = false; // enable collision
				}
			}
			else if ( entity->behavior == &actGoldBag )
			{
				if ( entity->flags[INVISIBLE] )
				{
					entity->vel_x = (0.25 + .025 * (local_rng.rand() % 11)) * cos(entity->yaw);
					entity->vel_y = (0.25 + .025 * (local_rng.rand() % 11)) * sin(entity->yaw);
					entity->vel_z = (-40 - local_rng.rand() % 10) * .01;
					entity->goldBouncing = 0;
					entity->z = 0.0 - (local_rng.rand() % 3);
					entity->flags[INVISIBLE] = false;
				}
			}
		}
	}},

	{ 'DAED', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( Entity* shrine = uidToEntity(uid) )
		{
			if ( shrine->behavior == &::actDaedalusShrine )
			{
				daedalusShrineInteract(shrine, nullptr);
			}
		}
	}},

	// bell dropped item
	{ 'BELI', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			if ( entity->behavior == &actItem )
			{
				//entity->flags[UPDATENEEDED] = true;
				if ( entity->flags[INVISIBLE] )
				{
					playSoundEntityLocal(entity, 47 + local_rng.rand() % 3, 64);
					entity->flags[INVISIBLE] = false;
					entity->vel_x = 0.0; //(0.25 + .025 * (local_rng.rand() % 11)) * cos(entity->yaw);
					entity->vel_y = 0.0; //(0.25 + .025 * (local_rng.rand() % 11)) * sin(entity->yaw);
					entity->vel_z = (-2 - local_rng.rand() % 5) * .01;
					entity->itemContainer = 0;
					entity->z = -16;
					entity->itemNotMoving = 0;
					entity->itemNotMovingClient = 0;
					entity->flags[USERFLAG1] = false; // enable collision
				}
			}
			else if ( entity->behavior == &actGoldBag )
			{
				if ( entity->flags[INVISIBLE] )
				{
					playSoundEntityLocal(entity, 242 + local_rng.rand() % 4, 64);
					entity->vel_x = 0.0;
					entity->vel_y = 0.0;
					entity->vel_z = (-2 - local_rng.rand() % 5) * .01;
					entity->goldBouncing = 0;
					entity->z = -16;
					entity->flags[INVISIBLE] = false;
				}
			}
		}
	} },

	// ghost interact item
	{ 'GHOI', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			entity->itemNotMoving = 0;
			entity->itemNotMovingClient = 0;
			entity->flags[USERFLAG1] = false; // enable collision

			entity->x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8])) / 32.0;
			entity->y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10])) / 32.0;
			entity->z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[12])) / 32.0;

			entity->vel_x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[14])) / 32.0;
			entity->vel_y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[16])) / 32.0;
			entity->vel_z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[18])) / 32.0;
		}
	}},

	// attract item
	{ 'ATTI', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		Entity* entity = uidToEntity(uid);
		if ( entity )
		{
			entity->itemNotMoving = 0;
			entity->itemNotMovingClient = 0;
			entity->flags[USERFLAG1] = false; // enable collision
			entity->flags[UPDATENEEDED] = true;
			entity->flags[NOUPDATE] = false;

			entity->itemFollowUID = SDLNet_Read32(&net_packet->data[20]);

			entity->x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8])) / 32.0;
			entity->y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10])) / 32.0;
			entity->z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[12])) / 32.0;
			entity->new_z = entity->z;
			entity->itemLevitate = 1.0;
			entity->itemLevitateStartZ = entity->z;

			entity->vel_x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[14])) / 32.0;
			entity->vel_y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[16])) / 32.0;
			entity->vel_z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[18])) / 32.0;
		}
	} },

	// spawn an explosion
	{'EXPL', [](){
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		spawnExplosion(x, y, z);
	}},

	// spawn an explosion, custom sprite
	{'EXPS', [](){
		Uint16 sprite = SDLNet_Read16(&net_packet->data[4]);
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10]));
		spawnExplosionFromSprite(sprite, x, y, z);
	}},

	// spawn a bang sprite
	{'BANG', [](){
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		spawnBang(x, y, z);
	}},

	// spawn a gib
	{'SPGB', [](){
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		Sint16 sprite = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10]));
		Entity* gib = spawnGibClient(x, y, z, sprite);
		gib->flags[SPRITE] = net_packet->data[12] & (1 << 0);
		gib->skill[5] = net_packet->data[12] & (1 << 1); // poof
		if ( !spawn_blood && !gib->flags[SPRITE] && gib->sprite != 5 )
		{
			gib->flags[INVISIBLE] = true;
		}
	}},

	// spawn a sleep Z
	{'SLEZ', [](){
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		spawnSleepZ(x, y, z);
	}},

	// spawn a poof
	{ 'PUFF', []() {
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		Uint16 scale = SDLNet_Read16(&net_packet->data[10]);
		Entity* poof = spawnPoof(x, y, z, scale / 100.0);
	}},

	// spawn a misc sprite like the sleep Z
	{'SLEM', [](){
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		Sint16 sprite = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[10]));
		spawnFloatingSpriteMisc(sprite, x, y, z);
	}},

	// spawn magical effect particles
	{'MAGE', [](){
		Sint16 x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		Uint32 sprite = SDLNet_Read32(&net_packet->data[10]);
		spawnMagicEffectParticles(x, y, z, sprite);
	}},

	// spawn magical bell effect particles
	{ 'MAGB', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( Entity* entity = uidToEntity(uid) )
		{
			Uint32 sprite = SDLNet_Read32(&net_packet->data[8]);
			spawnMagicEffectParticlesBell(entity, sprite);
		}
	} },

	// spawn misc particle effect
	{'SPPE', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			int particleType = net_packet->data[8];
			int sprite = SDLNet_Read16(&net_packet->data[9]);
			switch ( particleType )
			{
				case PARTICLE_EFFECT_ABILITY_PURPLE:
					createParticleDot(entity);
					break;
				case PARTICLE_EFFECT_ABILITY_ROCK:
					createParticleRock(entity, sprite);
					break;
				case PARTICLE_EFFECT_SPIN:
					createParticleSpin(entity);
					break;
				case PARTICLE_EFFECT_SHATTERED_GEM:
					createParticleShatteredGem(entity->x, entity->y, 7.5, sprite, entity);
					break;
				case PARTICLE_EFFECT_SHADOW_INVIS:
					createParticleDropRising(entity, sprite, 1.0);
					break;
				case PARTICLE_EFFECT_INCUBUS_TELEPORT_STEAL:
				{
					Entity* spellTimer = createParticleTimer(entity, 80, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SHOOT_PARTICLES;
					spellTimer->particleTimerCountdownSprite = sprite;
					spellTimer->particleTimerPreDelay = 40;
				}
				break;
				case PARTICLE_EFFECT_INCUBUS_TELEPORT_TARGET:
				{
					Entity* spellTimer = createParticleTimer(entity, 40, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SHOOT_PARTICLES;
					spellTimer->particleTimerCountdownSprite = sprite;
				}
				break;
				case PARTICLE_EFFECT_SHADOW_TELEPORT:
				{
					Entity* spellTimer = createParticleTimer(entity, 40, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SHOOT_PARTICLES;
					spellTimer->particleTimerCountdownSprite = sprite;
				}
				break;
				case PARTICLE_EFFECT_SHRINE_TELEPORT:
				{
					Entity* spellTimer = createParticleTimer(entity, 200, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SHOOT_PARTICLES;
					spellTimer->particleTimerCountdownSprite = sprite;
					spellTimer->particleTimerPreDelay = 0;
				}
				break;
				case PARTICLE_EFFECT_DESTINY_TELEPORT:
				{
					Uint32 duration = SDLNet_Read32(&net_packet->data[11]);
					Entity* spellTimer = createParticleTimer(entity, duration, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SHOOT_PARTICLES;
					spellTimer->particleTimerCountdownSprite = sprite;
					spellTimer->particleTimerPreDelay = 0;
				}
				break;
				case PARTICLE_EFFECT_TELEPORT_PULL:
				{
					Entity* spellTimer = createParticleTimer(entity, 40, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SHOOT_PARTICLES;
					spellTimer->particleTimerCountdownSprite = sprite;
				}
				break;
				case PARTICLE_EFFECT_ERUPT:
					createParticleErupt(entity, sprite);
					break;
				case PARTICLE_EFFECT_VAMPIRIC_AURA:
					createParticleDropRising(entity, sprite, 0.5);
					break;
				case PARTICLE_EFFECT_RISING_DROP:
					createParticleDropRising(entity, sprite, 1.0);
					break;
				case PARTICLE_EFFECT_CHARM_MONSTER:
					createParticleCharmMonster(entity);
					break;
				case PARTICLE_EFFECT_SHADOW_TAG:
				{
					Uint32 uid = SDLNet_Read32(&net_packet->data[11]);
					createParticleShadowTag(entity, uid, 60 * TICKS_PER_SECOND);
					break;
				}
				case PARTICLE_EFFECT_PINPOINT:
				{
					Uint32 uid = SDLNet_Read32(&net_packet->data[11]);
					if ( sprite >= PINPOINT_PARTICLE_START && sprite < PINPOINT_PARTICLE_END )
					{
						if ( net_packet->len >= 23 )
						{
							int duration = SDLNet_Read32(&net_packet->data[15]);
							int spellID = SDLNet_Read32(&net_packet->data[19]);
							createParticleSpellPinpointTarget(entity, uid, sprite, duration, spellID);
						}
					}
					break;
				}
				case PARTICLE_EFFECT_REVENANT_CURSE:
				{
					int duration = SDLNet_Read32(&net_packet->data[15]);
					if ( Entity* fx = createParticleAestheticOrbit(entity, sprite, duration, PARTICLE_EFFECT_REVENANT_CURSE) )
					{
						fx->z = 7.5;
						fx->yaw = entity->yaw;
						fx->ditheringOverride = 6;
					}
					break;
				}
				case PARTICLE_EFFECT_SPELL_WEB_ORBIT:
					createParticleAestheticOrbit(entity, 863, 400, PARTICLE_EFFECT_SPELL_WEB_ORBIT);
					break;
				case PARTICLE_EFFECT_SMITE_PINPOINT:
				{
					for ( int i = 0; i < 3; ++i )
					{
						Entity* fx1 = createParticleAestheticOrbit(entity, 2401, 2 * TICKS_PER_SECOND, PARTICLE_EFFECT_SMITE_PINPOINT);
						fx1->yaw = entity->yaw + PI / 2 + 2 * i * PI / 3;
						fx1->fskill[4] = entity->x;
						fx1->fskill[5] = entity->y;
						fx1->x = entity->x;
						fx1->y = entity->y;
						fx1->fskill[6] = fx1->yaw;
						fx1->skill[3] = 0;
						if ( i != 0 )
						{
							fx1->actmagicNoLight = 1;
						}
					}
					break;
				}
				case PARTICLE_EFFECT_TURN_UNDEAD:
				{
					if ( Entity* fx1 = createParticleAestheticOrbit(entity, 2401, 3 * TICKS_PER_SECOND, PARTICLE_EFFECT_TURN_UNDEAD) )
					{
						fx1->yaw = entity->yaw;
						fx1->fskill[4] = entity->x;
						fx1->fskill[5] = entity->y;
						fx1->x = entity->x;
						fx1->y = entity->y;
						fx1->fskill[6] = fx1->yaw;
						fx1->skill[3] = 0;
					}
					break;
				}
				case PARTICLE_EFFECT_HOLY_FIRE:
				{
					int duration = SDLNet_Read32(&net_packet->data[15]);
					if ( Entity* fx = createParticleAestheticOrbit(entity, 288, duration, PARTICLE_EFFECT_HOLY_FIRE) )
					{
						fx->flags[SPRITE] = true;
						fx->flags[INVISIBLE] = true;
					}
					break;
				}
				case PARTICLE_EFFECT_DEFY_FLESH_ORBIT:
				{
					int duration = SDLNet_Read32(&net_packet->data[15]);
					if ( Entity* fx = createParticleAestheticOrbit(entity, 2363, duration, PARTICLE_EFFECT_DEFY_FLESH_ORBIT) )
					{
						fx->flags[INVISIBLE] = true;
					}
					break;
				}
				case PARTICLE_EFFECT_DEFY_FLESH:
				{
					int duration = SDLNet_Read32(&net_packet->data[15]);
					Sint32 dir = SDLNet_Read32(&net_packet->data[19]);
					if ( Entity* fx = createParticleAestheticOrbit(entity, 2363, duration, PARTICLE_EFFECT_DEFY_FLESH) )
					{
						fx->yaw = dir / 256.0;
						fx->flags[INVISIBLE] = true;

						fx->pitch = PI / 2;
						fx->fskill[0] = fx->yaw;
						fx->fskill[1] = PI / 4 - PI / 8;
						fx->fskill[2] = entity->z;
						fx->x = entity->x - 8.0 * cos(fx->yaw);
						fx->y = entity->y - 8.0 * sin(fx->yaw);
						fx->z = entity->z;
						fx->scalex = 0.0;
						fx->scaley = 0.0;
						fx->scalez = 0.0;
					}
					break;
				}
				case PARTICLE_EFFECT_PSYCHIC_SPEAR:
				{
					int duration = SDLNet_Read32(&net_packet->data[15]);
					Sint32 dir = SDLNet_Read32(&net_packet->data[19]);
					if ( Entity* fx = createParticleAestheticOrbit(entity, 2362, duration, PARTICLE_EFFECT_PSYCHIC_SPEAR) )
					{
						fx->yaw = dir / 256.0;
						//fx->skill[3] = spell->caster;
						fx->pitch = 0;// PI / 4;
						fx->fskill[0] = fx->yaw + PI / 2 + (local_rng.rand() % 6) * PI / 3;
						fx->fskill[1] = PI / 4 + PI / 8;// +(i + 1) * 2 * PI / 3;
						fx->x = entity->x - 8.0 * cos(fx->yaw);
						fx->y = entity->y - 8.0 * sin(fx->yaw);
						fx->z = entity->z;// -8.0;
						fx->scalex = 0.0;
						fx->scaley = 0.0;
						fx->scalez = 0.0;
					}
					break;
				}
				case PARTICLE_EFFECT_FOCI_LIGHT:
				{
					createParticleFociLight(entity, sprite, false);
					break;
				}
				case PARTICLE_EFFECT_FOCI_DARK:
				{
					createParticleFociDark(entity, sprite, false);
					break;
				}
				case PARTICLE_EFFECT_PORTAL_SPAWN:
				{
					Entity* spellTimer = createParticleTimer(entity, 100, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SPAWN_PORTAL;
					spellTimer->particleTimerCountdownSprite = 174;
					spellTimer->particleTimerEndAction = PARTICLE_EFFECT_PORTAL_SPAWN;
				}
				break;
				case PARTICLE_EFFECT_LICHFIRE_TELEPORT_STATIONARY:
				case PARTICLE_EFFECT_LICHICE_TELEPORT_STATIONARY:
				case PARTICLE_EFFECT_LICH_TELEPORT_ROAMING:
				{
					Entity* spellTimer = createParticleTimer(entity, 40, sprite);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SHOOT_PARTICLES;
					spellTimer->particleTimerCountdownSprite = sprite;
				}
				break;
				case PARTICLE_EFFECT_SLIME_SPRAY:
				{
					Entity* spellTimer = createParticleTimer(entity, 30, -1);
					spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_MAGIC_SPRAY;
					spellTimer->particleTimerCountdownSprite = sprite;
				}
				break;
				case PARTICLE_EFFECT_PLAYER_AUTOMATON_DEATH:
					createParticleExplosionCharge(entity, 174, 100, 0.25);
					if ( entity && entity->behavior == &actPlayer )
					{
						if ( entity->getMonsterTypeFromSprite() == AUTOMATON )
						{
							entity->playerAutomatonDeathCounter = 1;
							if ( entity->skill[2] == clientnum )
							{
								// this is me dying, setup the deathcam.
								entity->playerCreatedDeathCam = 1;
								Entity* entity = newEntity(-1, 1, map.entities, nullptr);
								entity->x = cameras[clientnum].x * 16;
								entity->y = cameras[clientnum].y * 16;
								entity->z = -2;
								entity->flags[NOUPDATE] = true;
								entity->flags[PASSABLE] = true;
								entity->flags[INVISIBLE] = true;
								entity->behavior = &actDeathCam;
								entity->skill[2] = clientnum;
								entity->yaw = cameras[clientnum].ang;
								entity->pitch = PI / 8;
								players[clientnum]->ghost.initTeleportLocations(entity->x / 16, entity->y / 16);
							}
						}
					}
					break;
				case PARTICLE_EFFECT_ENSEMBLE_OTHER_CAST:
					createEnsembleTargetParticleCircling(entity);
					break;
				case PARTICLE_EFFECT_ENSEMBLE_SELF_CAST:
					createEnsembleHUDParticleCircling(entity);
					break;
				case PARTICLE_EFFECT_IGNITE:
					createParticleIgnite(entity);
					break;
				case PARTICLE_EFFECT_SHATTER_OBJECTS:
					createParticleShatterObjects(entity);
					break;
				case PARTICLE_EFFECT_LIGHTNING_SEQ:
					floorMagicCreateLightningSequence(entity, entity->ticks + 1);
					break;
				case PARTICLE_EFFECT_STATIC_ORBIT:
				{
					Entity* fx = createParticleAestheticOrbit(entity, sprite, 2 * TICKS_PER_SECOND, PARTICLE_EFFECT_STATIC_ORBIT);
					fx->z = 7.5;
					fx->actmagicOrbitDist = 20;
					fx->actmagicNoLight = 1;
					break;
				}
				case PARTICLE_EFFECT_STATIC_MAXIMISE:
				{
					for ( int i = 0; i < 3; ++i )
					{
						Entity* fx = createParticleAestheticOrbit(entity, sprite, 2 * TICKS_PER_SECOND, PARTICLE_EFFECT_STATIC_ORBIT);
						fx->z = 7.5 - 2.0 * i;
						fx->scalex = 1.0;
						fx->scaley = 1.0;
						fx->scalez = 1.0;
						fx->actmagicOrbitDist = 20;
						fx->yaw += i * 2 * PI / 3;
						fx->actmagicNoLight = (i == 0 ? 0 : 1);
					}
					break;
				}
				case PARTICLE_EFFECT_CONTROL:
					for ( int i = 0; i < 4; ++i )
					{
						Entity* fx = spawnMagicParticle(entity);
						fx->sprite = sprite;
						fx->yaw = entity->yaw + i * PI / 2;
						fx->scalex = 0.7;
						fx->scaley = fx->scalex;
						fx->scalez = fx->scalex;
						fx->vel_x = 0.5 * cos(entity->yaw + i * PI / 2);
						fx->vel_y = 0.5 * sin(entity->yaw + i * PI / 2);
					}
					break;
				case PARTICLE_EFFECT_FLAMES:
				{
					int duration = SDLNet_Read32(&net_packet->data[15]);
					if( Entity* fx = createParticleAestheticOrbit(entity, 233, duration, PARTICLE_EFFECT_IGNITE_ORBIT))
					{
						fx->flags[SPRITE] = true;
						fx->x = entity->x;
						fx->y = entity->y;
						fx->fskill[0] = fx->x;
						fx->fskill[1] = fx->y;
						fx->vel_z = -0.05;
						fx->actmagicOrbitDist = 2;
						fx->fskill[2] = entity->yaw + (local_rng.rand() % 8) * PI / 4.0;
						fx->yaw = fx->fskill[2];
						fx->actmagicNoLight = 1;
					}
					break;
				}
				case PARTICLE_EFFECT_HEAT_ORBIT_SPIN:
				{
					Uint32 particle = SDLNet_Read32(&net_packet->data[11]);
					int duration = SDLNet_Read32(&net_packet->data[15]);
					for ( int i = 0; i < 2; ++i )
					{
						if ( Entity* fx = createParticleAestheticOrbit(entity, sprite, duration, PARTICLE_EFFECT_IGNITE_ORBIT) )
						{
							fx->flags[SPRITE] = true;
							fx->x = entity->x;
							fx->y = entity->y;
							fx->z = 7.5;
							fx->fskill[0] = fx->x;
							fx->fskill[1] = fx->y;
							fx->vel_z = -0.5;
							fx->actmagicOrbitDist = 5;
							fx->fskill[2] = entity->yaw + PI / 4.0 + i * PI;
							fx->yaw = fx->fskill[2];
							fx->fskill[4] = 0.25;
							if ( particle == 1 )
							{
								fx->lightBonus = vec4{ 0.f, 0.f, 0.f, 0.f };
								fx->actmagicNoLight = 1;
							}
						}
					}
					break;
				}
				case PARTICLE_EFFECT_SUMMON_FLAMES:
				{
					int duration = SDLNet_Read32(&net_packet->data[15]);
					for ( int i = 0; i < 3; ++i )
					{
						if ( Entity* fx = createParticleAestheticOrbit(entity, 233, duration, PARTICLE_EFFECT_IGNITE_ORBIT) )
						{
							fx->flags[SPRITE] = true;
							fx->x = entity->x;
							fx->y = entity->y;
							fx->fskill[0] = fx->x;
							fx->fskill[1] = fx->y;
							fx->z = -7.5;
							fx->vel_z = 0.25;
							fx->actmagicOrbitDist = 4;
							fx->fskill[2] = entity->yaw + (i) * 2 * PI / 3.0;
							fx->yaw = fx->fskill[2];
							fx->actmagicNoLight = 1;

						}
					}
					break;
				}
				case PARTICLE_EFFECT_BOLAS:
				{
					Uint32 duration = SDLNet_Read32(&net_packet->data[15]);
					createParticleBolas(entity, sprite, duration, nullptr);
				}
				break;
				default:
					break;
			}
		}
	}},

	// spawn misc particle effect at fixed location
	{'SPPL', [](){
		Sint16 particle_x = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[4]));
		Sint16 particle_y = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Sint16 particle_z = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		int particleType = net_packet->data[10];
		int sprite = SDLNet_Read16(&net_packet->data[11]);
		//messagePlayer(1, "recv, %d, %d, %d, type: %d", particle_x, particle_y, particle_z, particleType);
		switch ( particleType )
		{
			case PARTICLE_EFFECT_SUMMON_MONSTER:
			{
				Entity* spellTimer = createParticleTimer(nullptr, 70, sprite);
				spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SUMMON_MONSTER;
				spellTimer->particleTimerCountdownSprite = 174;
				spellTimer->particleTimerEndAction = PARTICLE_EFFECT_SUMMON_MONSTER;
				spellTimer->x = particle_x * 16.0 + 8;
				spellTimer->y = particle_y * 16.0 + 8;
				spellTimer->z = particle_z;
			}
			break;
			case PARTICLE_EFFECT_DEVIL_SUMMON_MONSTER:
			{
				Entity* spellTimer = createParticleTimer(nullptr, 70, sprite);
				spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_DEVIL_SUMMON_MONSTER;
				spellTimer->particleTimerCountdownSprite = 174;
				spellTimer->particleTimerEndAction = PARTICLE_EFFECT_SUMMON_MONSTER;
				spellTimer->x = particle_x * 16.0 + 8;
				spellTimer->y = particle_y * 16.0 + 8;
				spellTimer->z = particle_z;
			}
			break;
			case PARTICLE_EFFECT_SPELL_SUMMON:
			{
				Entity* spellTimer = createParticleTimer(nullptr, 55, sprite);
				spellTimer->particleTimerCountdownSprite = 791;
				spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_SPELL_SUMMON;
				spellTimer->particleTimerPreDelay = 40;
				spellTimer->particleTimerEndAction = PARTICLE_EFFECT_SPELL_SUMMON;
				spellTimer->x = particle_x * 16.0 + 8;
				spellTimer->y = particle_y * 16.0 + 8;
				spellTimer->z = particle_z;
			}
			break;
			case PARTICLE_EFFECT_TELEPORT_PULL_TARGET_LOCATION:
			{
				Entity* spellTimer = createParticleTimer(nullptr, 40, 593);
				spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_TELEPORT_PULL_TARGET_LOCATION;
				spellTimer->particleTimerCountdownSprite = 593;
				spellTimer->x = particle_x * 16.0 + 8;
				spellTimer->y = particle_y * 16.0 + 8;
				spellTimer->z = particle_z;
				spellTimer->flags[PASSABLE] = false;
				spellTimer->sizex = 4;
				spellTimer->sizey = 4;
			}
			break;
			case PARTICLE_EFFECT_SHATTERED_GEM:
				createParticleShatteredGem(particle_x, particle_y, 7.5, sprite, nullptr);
				break;
			case PARTICLE_EFFECT_ERUPT:
				createParticleErupt(particle_x, particle_y, sprite);
				break;
			case PARTICLE_EFFECT_BOOBY_TRAP:
				createParticleBoobyTrapExplode(nullptr, particle_x, particle_y);
				break;
			case PARTICLE_EFFECT_MISC_PUDDLE:
				spawnMiscPuddle(nullptr, particle_x, particle_y, sprite);
				break;
			case PARTICLE_EFFECT_BLOOD_BUBBLE:
			{
				for ( int i = 0; i < 4; ++i )
				{
					if ( Entity* gib = spawnGibClient(particle_x, particle_y, particle_z, 5) )
					{
						gib->sprite = 5;
					}

					Entity* fx = createParticleAestheticOrbit(nullptr, 283, 1.5 * TICKS_PER_SECOND + i * 10, PARTICLE_EFFECT_BLOOD_BUBBLE);
					real_t dir = (local_rng.rand() % 360) * PI / 180.f;
					fx->x = particle_x + 4.0 * cos(dir);
					fx->y = particle_y + 4.0 * sin(dir);
					fx->z = particle_z - (local_rng.rand() % 5);
					fx->flags[SPRITE] = true;

					fx->fskill[2] = 2 * PI * (local_rng.rand() % 10) / 10.0;
					fx->fskill[3] = 0.025; // speed osc
					fx->scalex = 0.0125;
					fx->scaley = fx->scalex;
					fx->scalez = fx->scalex;
					fx->actmagicOrbitDist = 2;
					fx->actmagicOrbitStationaryX = particle_x;
					fx->actmagicOrbitStationaryY = particle_y;
				}
				break;
			}
			case PARTICLE_EFFECT_SPORE_BOMB:
				for ( int i = 0; i < 16; ++i )
				{
					Entity* gib = spawnGibClient(particle_x, particle_y, particle_z, sprite);
					gib->sprite = sprite;
					gib->yaw = i * PI / 4 + (-2 + local_rng.rand() % 5) * PI / 64;
					gib->vel_x = 1.75 * cos(gib->yaw);
					gib->vel_y = 1.75 * sin(gib->yaw);
					gib->scalex = 0.5;
					gib->scaley = 0.5;
					gib->scalez = 0.5;
					gib->z = local_rng.uniform(8, particle_z - 4);
					gib->lightBonus = vec4(0.25, 0.25, 0.25, 0.f);
				}
				break;
			case PARTICLE_EFFECT_WINDGATE:
			{
				int duration = static_cast<int>(SDLNet_Read32(&net_packet->data[13]));
				Uint32 data = SDLNet_Read32(&net_packet->data[17]);

				int wallDir = (data & 0xF);
				int length = (data >> 4) & 0xF;
				Uint32 casterUid = SDLNet_Read32(&net_packet->data[21]);
				createWindMagic(casterUid, particle_x, particle_y, duration, wallDir, length);
				break;
			}
			case PARTICLE_EFFECT_DEMESNE_DOOR:
				createParticleDemesneDoor(particle_x, particle_y, particle_z / 256.0);
				break;
			case PARTICLE_EFFECT_NULL_PARTICLE:
			{
				Entity* fx = createParticleAestheticOrbit(nullptr, sprite, TICKS_PER_SECOND / 4, PARTICLE_EFFECT_NULL_PARTICLE);
				fx->x = particle_x;
				fx->y = particle_y;
				fx->z = particle_z;
				Sint32 dir = SDLNet_Read32(&net_packet->data[17]);
				fx->yaw = dir / 256.0;
				fx->actmagicOrbitDist = 0;
				fx->actmagicNoLight = 0;
				break;
			}
			case PARTICLE_EFFECT_AREA_EFFECT:
			{
				int radius = SDLNet_Read32(&net_packet->data[13]);
				createSpellExplosionArea(sprite, nullptr, particle_x, particle_y, particle_z, radius, 0, nullptr);
				break;
			}
			case PARTICLE_EFFECT_EARTH_ELEMENTAL_DIE:
			{
				Entity* spellTimer = createParticleTimer(nullptr, TICKS_PER_SECOND, -1);
				spellTimer->x = particle_x;
				spellTimer->y = particle_y;
				spellTimer->z = particle_z;
				spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_EARTH_ELEMENTAL_DIE;
				break;
			}
			case PARTICLE_EFFECT_DUCK_SPAWN_FEATHER:
			{
				duckSpawnFeather(sprite, particle_x, particle_y, particle_z, nullptr);
				break;
			}
			case PARTICLE_EFFECT_SABOTAGE_TRAP:
			{
				Entity* spellTimer = createParticleTimer(nullptr, TICKS_PER_SECOND, -1);
				spellTimer->x = particle_x;
				spellTimer->y = particle_y;
				spellTimer->z = particle_z;
				spellTimer->particleTimerCountdownAction = PARTICLE_TIMER_ACTION_TRAP_SABOTAGED;
				break;
			}
			case PARTICLE_EFFECT_EARTH_ELEMENTAL_SUMMON_AOE:
			{
				int radius = SDLNet_Read32(&net_packet->data[13]);
				Uint32 color = SDLNet_Read32(&net_packet->data[17]);
				if ( Entity* fx = createParticleAOEIndicator(nullptr, particle_x, particle_y, 0.0, TICKS_PER_SECOND, radius) )
				{
					fx->actSpriteFollowUID = 0;
					fx->actSpriteCheckParentExists = 0;
					if ( auto indicator = AOEIndicators_t::getIndicator(fx->skill[10]) )
					{
						indicator->indicatorColor = color;
						indicator->loop = false;
						indicator->gradient = 4;
						indicator->framesPerTick = 2;
						indicator->ticksPerUpdate = 1;
						indicator->delayTicks = 0;
					}
				}
				break;
			}
			case PARTICLE_EFFECT_BASTION_MUSHROOM:
			{
				Uint32 casterUid = SDLNet_Read32(&net_packet->data[21]);
				createMushroomSpellEffect(uidToEntity(casterUid), particle_x, particle_y);
				break;
			}
			case PARTICLE_EFFECT_METEOR_STATIONARY_ORBIT:
			{
				int duration = static_cast<int>(SDLNet_Read32(&net_packet->data[13]));
				Sint32 dir = SDLNet_Read32(&net_packet->data[17]);
				if ( Entity* fx = createParticleAestheticOrbit(nullptr, 2210, duration, PARTICLE_EFFECT_METEOR_STATIONARY_ORBIT) )
				{
					fx->x = particle_x;
					fx->y = particle_y;
					fx->z = particle_z;
					fx->yaw = (dir / 256.0) + PI / 4;
				}
				if ( Entity* fx = createParticleAestheticOrbit(nullptr, 2211, duration, PARTICLE_EFFECT_METEOR_STATIONARY_ORBIT) )
				{
					fx->x = particle_x;
					fx->y = particle_y;
					fx->z = particle_z;
					fx->yaw = (dir / 256.0) - PI / 4;
				}
				break;
			}
			default:
				break;
		}
	}},

	// enemy hp bar
	{'ENHP', [](){
		Sint16 enemy_hp = SDLNet_Read16(&net_packet->data[4]);
		Sint16 enemy_maxhp = SDLNet_Read16(&net_packet->data[6]);
		Sint16 oldhp = SDLNet_Read16(&net_packet->data[8]);
		Uint32 uid = SDLNet_Read32(&net_packet->data[10]);
		bool lowPriorityTick = false;
		DamageGib gib = DMG_DEFAULT;
		if ( EnemyHPDamageBarHandler::bDamageGibTypesEnabled )
		{
			gib = static_cast<DamageGib>((net_packet->data[14] & 0xFE) >> 1); // upper 7 bits
			if ( net_packet->data[14] & 1 )
			{
				lowPriorityTick = true;
			}
		}
		else
		{
			if ( net_packet->data[14] == 1 )
			{
				lowPriorityTick = true;
			}
		}
		char enemy_name[128] = "";
		strcpy(enemy_name, (char*)(&net_packet->data[55]));
		auto details = enemyHPDamageBarHandler[clientnum].addEnemyToList(enemy_hp,
			enemy_maxhp, oldhp, uid, enemy_name, lowPriorityTick, gib);
		if ( details )
		{
			details->enemy_statusEffects1 = SDLNet_Read32(&net_packet->data[15]);
			details->enemy_statusEffects2 = SDLNet_Read32(&net_packet->data[19]);
			details->enemy_statusEffects3 = SDLNet_Read32(&net_packet->data[23]);
			details->enemy_statusEffects4 = SDLNet_Read32(&net_packet->data[27]);
			details->enemy_statusEffects5 = SDLNet_Read32(&net_packet->data[31]);
			details->enemy_statusEffectsLowDuration1 = SDLNet_Read32(&net_packet->data[35]);
			details->enemy_statusEffectsLowDuration2 = SDLNet_Read32(&net_packet->data[39]);
			details->enemy_statusEffectsLowDuration3 = SDLNet_Read32(&net_packet->data[43]);
			details->enemy_statusEffectsLowDuration4 = SDLNet_Read32(&net_packet->data[47]);
			details->enemy_statusEffectsLowDuration5 = SDLNet_Read32(&net_packet->data[51]);
		}
	}},

	// custom damage gib (miss/healing)
	{'DMGG', [](){
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		Sint16 dmg = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[8]));
		DamageGib gib = DMG_DEFAULT;
		gib = static_cast<DamageGib>(net_packet->data[10]);
		DamageGibDisplayType displayType = DamageGibDisplayType::DMG_GIB_NUMBER;
		if ( net_packet->data[11] == 1 )
		{
			displayType = DamageGibDisplayType::DMG_GIB_MISS;
		}
		else if ( net_packet->data[11] == 2 )
		{
			displayType = DamageGibDisplayType::DMG_GIB_SPRITE;
		}
		else if ( net_packet->data[11] == 3 )
		{
			displayType = DamageGibDisplayType::DMG_GIB_GUARD;
		}
		spawnDamageGib(uidToEntity(uid), dmg, gib, displayType);
	}},

	// ping
	{'PING', [](){
		messagePlayer(clientnum, MESSAGE_MISC, Language::get(1117), (SDL_GetTicks() - pingtime));
	}},

	// automated ping
	{'PNGU', [](){
		PingNetworkStatus_t::respond();
	}},

	// automated ping response
	{'PNGR', [](){
		PingNetworkStatus_t::receive();
	}},

	// unlock steam achievement
	{'SACH', [](){
		steamAchievement((char*)(&net_packet->data[4]));
	}},

	// update steam statistic
	{'SSTA', []() {
		const int statisticNum = net_packet->data[4];
		int value = SDLNet_Read16(&net_packet->data[6]);
		steamStatisticUpdate(statisticNum, static_cast<ESteamStatTypes>(net_packet->data[5]), value);
	}},

	// update challenge counter
	{ 'CHCT', []() {
		int value = SDLNet_Read16(&net_packet->data[4]);
		int max = SDLNet_Read16(&net_packet->data[6]);
		auto challengeName = "CHALLENGE_MONSTER_KILLS";
		int eventType = net_packet->data[8];
		if ( eventType == static_cast<int>(GameModeManager_t::CurrentSession_t::ChallengeRun_t::CHEVENT_KILLS_FURNITURE) )
		{
			challengeName = "CHALLENGE_FURNITURE_KILLS";
		}
		UIToastNotificationManager.createStatisticUpdateNotification(challengeName, value, max);
	}},

	// pause game
	{'PAUS', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		messagePlayer(clientnum, MESSAGE_MISC, Language::get(1118), stats[player]->name);
		pauseGame(2, 0);
	}},

	// unpause game
	{'UNPS', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		messagePlayer(clientnum, MESSAGE_MISC, Language::get(1119), stats[player]->name);
		pauseGame(1, 0);
	}},

	// server or player shut down
	{'DISC', [](){
	    const int player = net_packet->data[4];
	    if (!barony::net::validPlayer(player))
		{
			return;
		}
		client_disconnected[player] = true;
		if (player == 0)
		{
			// server shutdown
			if (!victory)
			{
				printlog("The remote server has shut down.\n");
				MainMenu::disconnectedFromServer("The host player\nhas ended the game.");
			}
		}
	}},

	// project spirit player
	{ 'PROJ', []() {
		if ( players[clientnum] == nullptr || !players[clientnum]->entity || stats[clientnum]->HP <= 0 )
		{
			return;
		}

		players[clientnum]->ghost.initTeleportLocations(players[clientnum]->entity->x / 16, players[clientnum]->entity->y / 16);
		players[clientnum]->ghost.spawnGhost();
		//node_t* nextnode = nullptr;
		//for ( auto node = map.entities->first; node; node = nextnode )
		//{
		//	nextnode = node->next;
		//	if ( Entity* entity = (Entity*)node->element )
		//	{
		//		if ( entity->behavior == &actProjectSpiritCam && entity->skill[2] == clientnum )
		//		{
		//			entity->removeLightField();
		//			list_RemoveNode(entity->mynode);
		//		}
		//	}
		//}

		//Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		//if ( Entity* targetEntity = uidToEntity(uid) )
		//{
		//	// projectcam
		//	Entity* entity = newEntity(-1, 1, map.entities, nullptr); //Deathcam entity.
		//	entity->x = targetEntity->x;
		//	entity->y = targetEntity->y;
		//	entity->z = -2;
		//	entity->flags[NOUPDATE] = true;
		//	entity->flags[PASSABLE] = true;
		//	entity->flags[INVISIBLE] = true;
		//	entity->behavior = &actProjectSpiritCam;
		//	entity->skill[1] = targetEntity->getUID();
		//	entity->skill[2] = clientnum;
		//	entity->yaw = targetEntity->yaw;
		//	entity->pitch = PI / 8;
		//	players[clientnum]->entity->skill[3] = 2;
		//	if ( multiplayer != CLIENT )
		//	{
		//		entity_uids--;
		//	}
		//	entity->setUID(-3);
		//}
	}},

	// teleport player
	{'TELE', [](){
		if (players[clientnum] == nullptr || !Player::getPlayerInteractEntity(clientnum) )
		{
			return;
		}
		int tele_x = net_packet->data[4];
		int tele_y = net_packet->data[5];
		Sint16 degrees = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[6]));
		Entity* playerEntity = Player::getPlayerInteractEntity(clientnum);
		playerEntity->yaw = degrees * PI / 180;
		playerEntity->x = (tele_x << 4) + 8;
		playerEntity->y = (tele_y << 4) + 8;
		playerEntity->bNeedsRenderPositionInit = true;
        for (auto part : playerEntity->bodyparts) {
            part->bNeedsRenderPositionInit = true;
        }
        for (auto node = map.entities->first; node != nullptr; node = node->next) {
            auto entity = static_cast<Entity*>(node->element);
            if (entity && entity->behavior == &actSpriteNametag) {
                if (entity->parent == playerEntity->getUID()) {
                    entity->bNeedsRenderPositionInit = true;
                }
            }
        }
        temporarilyDisableDithering();
	}},

	// teleport player
	{'TELM', [](){
		if ( players[clientnum] == nullptr || !Player::getPlayerInteractEntity(clientnum) )
		{
			return;
		}
		int tele_x = net_packet->data[4];
		int tele_y = net_packet->data[5];
		int type = net_packet->data[6];
		Entity* playerEntity = Player::getPlayerInteractEntity(clientnum);
		playerEntity->x = (tele_x << 4) + 8;
		playerEntity->y = (tele_y << 4) + 8;
		playerEntity->bNeedsRenderPositionInit = true;
		for ( auto part : playerEntity->bodyparts ) {
			part->bNeedsRenderPositionInit = true;
		}

		// play sound effect
		if ( type == 0 || type == 1 )
		{
			playSoundEntityLocal(playerEntity, 96, 64);
		}
		else if ( type == 2 )
		{
			playSoundEntityLocal(playerEntity, 154, 64);
		}
		for ( auto node = map.entities->first; node != nullptr; node = node->next ) {
			auto entity = static_cast<Entity*>(node->element);
			if ( entity && entity->behavior == &actSpriteNametag ) {
				if ( entity->parent == playerEntity->getUID() ) {
					entity->bNeedsRenderPositionInit = true;
				}
			}
		}
		temporarilyDisableDithering();
	}},

	// delete entity
	{'ENTD', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			auto entity2 = newEntity(entity->sprite, 1, &removedEntities, nullptr);
			if ( entity2 )
			{
				entity2->setUID(entity->getUID());
				for ( int j = 0; j < barony::net::playerCapacity(); ++j )
				{
					if (entity == players[j]->entity )
					{
						if ( stats[j] )
						{
							for ( int effect = 0; effect < NUMEFFECTS; ++effect )
							{
								if ( effect != EFF_VAMPIRICAURA && effect != EFF_WITHDRAWAL && effect != EFF_SHAPESHIFT )
								{
									stats[j]->clearEffect(effect);
									stats[j]->EFFECTS_TIMERS[effect] = 0;
								}
							}
						}
						players[j]->entity = nullptr;
						players[j]->cleanUpOnEntityRemoval();
					}
					else if ( entity == players[j]->ghost.my )
					{
						players[j]->ghost.my = nullptr;
						players[j]->ghost.reset();
					}
				}
				if ( entity->light )
				{
					list_RemoveNode(entity->light->node);
					entity->light = nullptr;
				}
				list_RemoveNode(entity->mynode);

				// inform the server that we deleted the entity
				//strcpy((char*)net_packet->data, "ENTD");
				//net_packet->data[4] = clientnum;
				//SDLNet_Write32(entity2->getUID(), &net_packet->data[5]);
				//net_packet->address.host = net_server.host;
				//net_packet->address.port = net_server.port;
				//net_packet->len = 9;
				//sendPacket(net_sock, -1, net_packet, 0);
			}
		}
	}},

	// shake screen
	{'SHAK', [](){
		cameravars[clientnum].shakex += static_cast<Sint8>(net_packet->data[4]) / 100.f;
		cameravars[clientnum].shakey += static_cast<Sint8>(net_packet->data[5]);
	}},

	// no mana flash
	{ 'NOMP', []() {
		messagePlayer(clientnum, MESSAGE_MISC, Language::get(375));
		playSound(563, 64);
		if ( players[clientnum]->magic.noManaProcessedOnTick == 0 )
		{
			players[clientnum]->magic.flashNoMana();
		}
		if ( net_packet->len >= 8 )
		{
			if ( stats[clientnum]->defending && stats[clientnum]->shield )
			{
				auto itemType = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4]));
				if ( stats[clientnum]->shield->type == itemType )
				{
					Input& input = Input::inputs[clientnum];
					if ( input.binaryToggle("Defend") )
					{
						input.consumeBinaryToggle("Defend");
					}
				}
			}
		}
	} },

	// a torch burns out
	{'TORC', [](){
		auto itemType = static_cast<ItemType>(SDLNet_Read16(&net_packet->data[4]));
		auto itemStatus = static_cast<Status>(net_packet->data[6]);
		int qty = net_packet->data[7];
		if ( stats[clientnum]->shield && stats[clientnum]->shield->type == itemType )
		{
			stats[clientnum]->shield->status = itemStatus;
			stats[clientnum]->shield->count = qty;
			if ( stats[clientnum]->shield->count <= 0 )
			{
				Item* item = stats[clientnum]->shield;
				item->count = 1; // to be consumed below
				consumeItem(item, clientnum);
				stats[clientnum]->shield = nullptr;
			}
			else
			{
				players[clientnum]->hud.shieldSwitch = true;
			}
		}
	}},

	// update equip beatitude
	{'BEAT', []() {
		Item* equipment = nullptr;
		//messagePlayer(0, "client: %d, armornum: %d, status %d", player, net_packet->data[5], net_packet->data[6]);
		switch ( net_packet->data[5] )
		{
			case 0:
				equipment = stats[clientnum]->weapon;
				break;
			case 1:
				equipment = stats[clientnum]->helmet;
				break;
			case 2:
				equipment = stats[clientnum]->breastplate;
				break;
			case 3:
				equipment = stats[clientnum]->gloves;
				break;
			case 4:
				equipment = stats[clientnum]->shoes;
				break;
			case 5:
				equipment = stats[clientnum]->shield;
				break;
			case 6:
				equipment = stats[clientnum]->cloak;
				break;
			case 7:
				equipment = stats[clientnum]->mask;
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
	}},

	// update armor quality
	{'ARMR', [](){
	    Item* item;
		switch ( net_packet->data[4] )
		{
			case 0:
				item = stats[clientnum]->helmet;
				break;
			case 1:
				item = stats[clientnum]->breastplate;
				break;
			case 2:
				item = stats[clientnum]->gloves;
				break;
			case 3:
				item = stats[clientnum]->shoes;
				break;
			case 4:
				item = stats[clientnum]->shield;
				break;
			case 5:
				item = stats[clientnum]->weapon;
				break;
			case 6:
				item = stats[clientnum]->cloak;
				break;
			case 7:
				item = stats[clientnum]->amulet;
				break;
			case 8:
				item = stats[clientnum]->ring;
				break;
			case 9:
				item = stats[clientnum]->mask;
				break;
			default:
				item = nullptr;
				break;
		}

		int checkType = -1;
		if ( net_packet->len >= 8 )
		{
			checkType = SDLNet_Read16(&net_packet->data[6]);
		}
		if ( item != nullptr && (checkType <= -1 || (checkType >= 0 && item->type == checkType)) )
		{
			if ( item->count > 1 )
			{
				Item* pickedUp = newItem(item->type, item->status, item->beatitude, item->count - 1, item->appearance, item->identified, &stats[clientnum]->inventory);
				item->count = 1;
			}
			if ( static_cast<int>(net_packet->data[5]) > EXCELLENT )
			{
				item->status = EXCELLENT;
			}
			else if ( static_cast<int>(net_packet->data[5]) < BROKEN )
			{
				item->status = BROKEN;
				if ( net_packet->data[4] == 5 )
				{
					if ( client_classes[clientnum] == CLASS_MESMER )
					{
						if ( stats[clientnum]->weapon->type == MAGICSTAFF_CHARM )
						{
							bool foundCharmSpell = false;
							for ( node_t* spellnode = stats[clientnum]->inventory.first; spellnode != nullptr; spellnode = spellnode->next )
							{
								auto item = static_cast<Item*>(spellnode->element);
								if ( item && itemCategory(item) == SPELL_CAT )
								{
									spell_t* spell = getSpellFromItem(clientnum, item, false);
									if ( spell && spell->ID == SPELL_CHARM_MONSTER )
									{
										foundCharmSpell = true;
										break;
									}
								}
							}
							if ( !foundCharmSpell )
							{
								steamAchievement("BARONY_ACH_WHAT_NOW");
							}
						}
					}
				}
			}
			else
			{
				item->status = static_cast<Status>(net_packet->data[5]);
			}

			// spellbooks in hand crumble to nothing.
			if ( item->status == BROKEN && net_packet->data[4] == 4 && itemCategory(item) == SPELLBOOK )
			{
				consumeItem(item, clientnum);
			}
			else if ( item )
			{
				if ( players[clientnum]->isLocalPlayer() )
				{
					std::unordered_set<Uint32> appearancesOfSimilarItems;
					std::vector<Item*> itemsToReroll;
					for ( node_t* node = stats[clientnum]->inventory.first; node != nullptr; node = node->next )
					{
						auto item2 = static_cast<Item*>(node->element);
						if ( item2 && item2 != item && !itemCompare(item, item2, true) )
						{
							itemsToReroll.push_back(item2);

							// items are the same (incl. appearance!)
							// if they shouldn't stack, we need to change appearance of the new item.
							appearancesOfSimilarItems.insert(item2->appearance);
						}
					}

					for ( auto rerollItem : itemsToReroll )
					{
						Item::itemFindUniqueAppearance(rerollItem, appearancesOfSimilarItems);
						appearancesOfSimilarItems.insert(rerollItem->appearance);
					}
				}
			}
		}
	}},

	// steal armor (destroy it)
	{'STLA', [](){
	    Item* item = nullptr;
		int armornum = net_packet->data[4];
		switch ( armornum )
		{
			case 0:
				item = stats[clientnum]->helmet;
				break;
			case 1:
				item = stats[clientnum]->breastplate;
				break;
			case 2:
				item = stats[clientnum]->gloves;
				break;
			case 3:
				item = stats[clientnum]->shoes;
				break;
			case 4:
				item = stats[clientnum]->shield;
				break;
			case 5:
				item = stats[clientnum]->weapon;
				break;
			case 6:
				item = stats[clientnum]->cloak;
				break;
			case 7:
				item = stats[clientnum]->amulet;
				break;
			case 8:
				item = stats[clientnum]->ring;
				break;
			case 9:
				item = stats[clientnum]->mask;
				break;
			default:
				item = nullptr;
				break;
		}


		auto checkType = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[5]));
		auto checkStatus = static_cast<Status>(SDLNet_Read32(&net_packet->data[9]));
		Sint16 checkBeatitude = static_cast<Sint16>(SDLNet_Read32(&net_packet->data[13]));
		Sint16 checkCount = static_cast<Sint16>(SDLNet_Read32(&net_packet->data[17]));
		Uint32 checkAppearance = SDLNet_Read32(&net_packet->data[21]);
		bool checkIdentified = net_packet->data[25] == 1 ? true : false;

		if ( item )
		{
			if ( item->type == checkType
				&& item->status == checkStatus
				&& item->beatitude == checkBeatitude
				&& item->count == checkCount
				&& item->appearance == checkAppearance
				/*&& item->identified == checkIdentified*/ )
			{
				// ok
				if ( itemTypeIsQuiver(item->type) || armornum == 5 /*weapon*/ )
				{
					item->count = 0;
				}
				else
				{
					item->count--;
				}

				if ( item->count <= 0 )
				{
					Item** slot = itemSlot(stats[clientnum], item);
					if ( slot != nullptr)
					{
						*slot = nullptr;
					}
					if ( item )
					{
						list_RemoveNode(item->node);
					}
				}
				return;
			}
			else
			{
				item = nullptr;
			}
		}

		if ( !item )
		{
			for ( node_t* node = stats[clientnum]->inventory.first; node != nullptr; node = node->next )
			{
				if (auto item2 = static_cast<Item*>(node->element) )
				{
					if ( item2->type == checkType
						&& item2->status == checkStatus
						&& item2->beatitude == checkBeatitude
						&& item2->count == checkCount
						&& item2->appearance == checkAppearance
						/*&& item2->identified == checkIdentified*/ )
					{
						// next best match
						if ( itemTypeIsQuiver(item2->type) || armornum == 5 /*weapon*/ )
						{
							item2->count = 0;
						}
						else
						{
							item2->count--;
						}

						if ( item2->count <= 0 )
						{
							Item** slot = itemSlot(stats[clientnum], item2);
							if ( slot != nullptr)
							{
								*slot = nullptr;
							}
							if ( item2 )
							{
								list_RemoveNode(item2->node);
							}
						}
						return;
					}
				}
			}
		}
	}},

	// damage indicator
	{'DAMI', [](){
		DamageIndicatorHandler.insert(clientnum, SDLNet_Read32(&net_packet->data[4]),
			SDLNet_Read32(&net_packet->data[8]), net_packet->data[12] == 1 ? true : false);
	} },

	// remote vibration
	{ 'BRRR', []() {
		inputs.addRumbleForHapticType(clientnum, SDLNet_Read32(&net_packet->data[4]),
			SDLNet_Read32(&net_packet->data[8]));
	}},

		// play sound position
	{'SNDP', [](){
		playSoundPos(
		    SDLNet_Read32(&net_packet->data[4]),
		    SDLNet_Read32(&net_packet->data[8]),
		    SDLNet_Read16(&net_packet->data[12]),
		    net_packet->data[14]);
	}},

		// play sound global
	{'SNDG', [](){
		playSound(
		    SDLNet_Read16(&net_packet->data[4]),
		    net_packet->data[6]);
	}},

	// play sound notification global
	{ 'SNDN', []() {
		playSoundNotification(
			SDLNet_Read16(&net_packet->data[4]),
			net_packet->data[6]);
	} },

	// play sound entity local
	{'SNEL', [](){
		Entity* tmp = uidToEntity(SDLNet_Read32(&net_packet->data[6]));
		int sfx = SDLNet_Read16(&net_packet->data[4]);
		if ( tmp )
		{
			if ( tmp->behavior == &actPlayer && mute_player_monster_sounds )
			{
				switch ( sfx )
				{
					case 95:
					case 70:
					case 322:
					case 323:
					case 324:
					case 329:
					case 332:
					case 333:
					case 229:
					case 230:
					case 231:
					case 232:
					case 233:
					case 234:
					case 235:
					case 291:
					case 292:
					case 293:
					case 294:
					case 60:
					case 61:
					case 62:
					case 257:
					case 258:
					case 276:
					case 277:
					case 278:
					case 502:
					case 503:
					case 504:
					case 505:
					case 506:
					case 507:
					case 508:
					case 830:
					case 831:
					case 832:
					case 833:
					case 834:
					case 835:
					case 836:
					case 837:
					case 838:
					case 839:
					case 840:
					case 841:
					case 842:
					case 843:
					case 844:
					case 845:
					case 846:
					case 847:
					case 848:
						// return early, don't play monster noises from players.
						return;
					default:
						break;
				}
			}
			playSoundEntityLocal(tmp, sfx, SDLNet_Read16(&net_packet->data[10]));
		}
	}},

	// add light
	{'ALIT', [](){
        std::vector<char> data;
        const auto len = SDLNet_Read16(&net_packet->data[8]);
        data.resize(len);
        stringCopy(data.data(), (const char*)&net_packet->data[10], data.size(), len);
		addLight(
		    SDLNet_Read16(&net_packet->data[4]),
		    SDLNet_Read16(&net_packet->data[6]),
		    data.data());
	}},

	// create wall
	{'WALC', [](){
		int y = SDLNet_Read16(&net_packet->data[6]);
		int x = SDLNet_Read16(&net_packet->data[4]);
		if ( x >= 0 && x < map.width && y >= 0 && y < map.height )
		{
			map.tiles[OBSTACLELAYER + y * MAP_LAYERS + x * MAP_LAYERS * map.height] = map.tiles[y * MAP_LAYERS + x * MAP_LAYERS * map.height];
		}

		const real_t effectOffset = 2.0;
		spawnPoof(static_cast<Sint16>(x * 16.0 - effectOffset), static_cast<Sint16>(y * 16.0 - effectOffset), 8, 1.0);
		spawnPoof(static_cast<Sint16>(x * 16.0 - effectOffset), static_cast<Sint16>(y * 16.0 + 16.0 + effectOffset), 8, 1.0);
		spawnPoof(static_cast<Sint16>(x * 16.0 + 16.0 + effectOffset), static_cast<Sint16>(y * 16.0 - effectOffset), 8, 1.0);
		spawnPoof(static_cast<Sint16>(x * 16.0 + 16.0 + effectOffset), static_cast<Sint16>(y * 16.0 + 16.0 + effectOffset), 8, 1.0);
	}},

	// destroy wall
	{'WALD', [](){
		int y = SDLNet_Read16(&net_packet->data[6]);
		int x = SDLNet_Read16(&net_packet->data[4]);
		if ( x >= 0 && x < map.width && y >= 0 && y < map.height )
		{
			map.tiles[OBSTACLELAYER + y * MAP_LAYERS + x * MAP_LAYERS * map.height] = 0;
		}
	}},

	// destroy wall + ceiling
	{'WACD', [](){
		int y = SDLNet_Read16(&net_packet->data[6]);
		int x = SDLNet_Read16(&net_packet->data[4]);
		if ( x >= 0 && x < map.width && y >= 0 && y < map.height )
		{
			map.tiles[OBSTACLELAYER + y * MAP_LAYERS + x * MAP_LAYERS * map.height] = 0;
			map.tiles[(MAP_LAYERS - 1) + y * MAP_LAYERS + x * MAP_LAYERS * map.height] = 0;
		}
	}},

	// monster music
	{'MUSM', [](){
	    Uint8 assailant = net_packet->data[4];
		combat = assailant;
	}},

	// get item
	{'ITEM', [](){
		Item* item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[28],
		nullptr);
		item->ownerUid = SDLNet_Read32(&net_packet->data[24]);
		Item* pickedUp = itemPickup(clientnum, item);
		free(item);
		if ( players[clientnum] && players[clientnum]->entity )
		{
			if ( pickedUp && pickedUp->type == BOOMERANG && !stats[clientnum]->weapon && pickedUp->ownerUid == players[clientnum]->entity->getUID() )
			{
				useItem(pickedUp, clientnum);

				auto& hotbar_t = players[clientnum]->hotbar;
				auto& hotbar = hotbar_t.slots();
				if ( hotbar_t.magicBoomerangHotbarSlot >= 0 )
				{
					hotbar[hotbar_t.magicBoomerangHotbarSlot].item = pickedUp->uid;
					for ( int i = 0; i < NUM_HOTBAR_SLOTS; ++i )
					{
						if ( i != hotbar_t.magicBoomerangHotbarSlot && hotbar[i].item == pickedUp->uid )
						{
							hotbar[i].item = 0;
							hotbar[i].resetLastItem();
						}
					}
				}
			}
			else if ( pickedUp && pickedUp->type == TOOL_DUCK && !stats[clientnum]->shield )
			{
				bool shapeshifted = false;
				if ( players[clientnum] && players[clientnum]->entity && players[clientnum]->entity->effectShapeshift != NOTHING )
				{
					shapeshifted = true;
				}

				if ( !shapeshifted && !intro )
				{
					useItem(pickedUp, clientnum);

					auto& hotbar_t = players[clientnum]->hotbar;
					auto& hotbar = hotbar_t.slots();
					if ( hotbar_t.magicDuckHotbarSlot >= 0 )
					{
						hotbar[hotbar_t.magicDuckHotbarSlot].item = pickedUp->uid;
						for ( int i = 0; i < NUM_HOTBAR_SLOTS; ++i )
						{
							if ( i != hotbar_t.magicDuckHotbarSlot && hotbar[i].item == pickedUp->uid )
							{
								hotbar[i].item = 0;
								hotbar[i].resetLastItem();
							}
						}
					}
				}
			}
		}
	}},

	// unequip and remove item
	{'DROP', [](){
		Item** armor = nullptr;
		switch ( net_packet->data[4] )
		{
			case 0:
				armor = &stats[clientnum]->helmet;
				break;
			case 1:
				armor = &stats[clientnum]->breastplate;
				break;
			case 2:
				armor = &stats[clientnum]->gloves;
				break;
			case 3:
				armor = &stats[clientnum]->shoes;
				break;
			case 4:
				armor = &stats[clientnum]->shield;
				break;
			case 5:
				armor = &stats[clientnum]->weapon;
				break;
			case 6:
				armor = &stats[clientnum]->cloak;
				break;
			case 7:
				armor = &stats[clientnum]->amulet;
				break;
			case 8:
				armor = &stats[clientnum]->ring;
				break;
			case 9:
				armor = &stats[clientnum]->mask;
				break;
		}
		if ( !armor )
		{
			return;
		}
		if ( !(*armor) )
		{
			return;
		}

		if ( *armor == inputs.getUIInteraction(clientnum)->selectedItem )
		{
			inputs.getUIInteraction(clientnum)->selectedItem = nullptr;
			inputs.getUIInteraction(clientnum)->selectedItemFromChest = 0;
		}

		if ( (*armor)->count > 1 )
		{
			(*armor)->count--;
		}
		else
		{
			if ( (*armor)->node )
			{
				list_RemoveNode((*armor)->node);
			}
			else
			{
				free(*armor);
			}
		}
		*armor = nullptr;
	}},

	// get gold
	{'GOLD', [](){
		stats[clientnum]->GOLD = SDLNet_Read32(&net_packet->data[4]);
	}},

	// open shop
	{'SHOP', [](){
		players[clientnum]->closeAllGUIs(DONT_CHANGE_SHOOTMODE, CLOSEGUI_DONT_CLOSE_INVENTORY);
		players[clientnum]->openStatusScreen(GUI_MODE_SHOP, INVENTORY_MODE_ITEM, Player::GUI_t::MODULE_SHOP);

		shopkeeper[clientnum] = SDLNet_Read32(&net_packet->data[4]);
		shopkeepertype[clientnum] = net_packet->data[8];
		strcpy( shopkeepername_client[clientnum], (char*)(&net_packet->data[9]) );
		shopkeepername[clientnum] = shopkeepername_client[clientnum];
		shoptimer[clientnum] = ticks - 1;
		shopspeech[clientnum] = Language::get(194 + local_rng.rand() % 3);

		players[clientnum]->shopGUI.openShop();
		return;
	}},

	// shop item
	{'SHPI', [](){
		if ( !shopInv[clientnum] )
		{
			return;
		}

		auto type = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4]));
		auto status = static_cast<Status>(static_cast<Sint8>(net_packet->data[8]));
		Sint16 beatitude = static_cast<Sint8>(net_packet->data[9]);
		Sint16 count = net_packet->data[10];
		Uint32 appearance = SDLNet_Read32(&net_packet->data[11]);
		bool identified = static_cast<bool>(net_packet->data[15] & 1);
		bool buybackItem = static_cast<bool>((net_packet->data[15] >> 1) & 1);
		bool extraConsumable = static_cast<bool>((net_packet->data[15] >> 2) & 1);
		Uint8 requireTradingSkill = static_cast<Uint8>((net_packet->data[15] >> 4) & 0xF);
		int x = static_cast<Sint8>(net_packet->data[16]);
		int y = static_cast<Sint8>(net_packet->data[17]);
		if ( Item* item = newItem(type, status, beatitude, count, appearance, identified, shopInv[clientnum]) )
		{
			item->x = x;
			item->y = y;
			item->playerSoldItemToShop = buybackItem;
			item->itemSpecialShopConsumable = extraConsumable;
			item->itemRequireTradingSkillInShop = requireTradingSkill;
		}
	}},

	// close shop
	{'SHPC', [](){
		Uint32 id = SDLNet_Read32(&net_packet->data[4]);
		if ( id == shopkeeper[clientnum] )
		{
			closeShop(clientnum);
			players[clientnum]->closeAllGUIs(CLOSEGUI_ENABLE_SHOOTMODE, CLOSEGUI_CLOSE_ALL);
		}
	}},

	// you died
	{'UDIE', [](){
		auto killer = static_cast<KilledBy>(SDLNet_Read32(&net_packet->data[4]));
		stats[clientnum]->killer = killer;

		if (killer == KilledBy::MONSTER) {
		    if (net_packet->data[8]) { // named monster
		        char name[128];
		        Uint32 len = net_packet->data[8];
		        len = std::min(static_cast<Uint32>(sizeof(name) - 1), len);
		        memcpy(name, &net_packet->data[13], len);
		        name[len] = '\0';
		        stats[clientnum]->killer_name = name;

				auto monster = static_cast<Monster>(SDLNet_Read32(&net_packet->data[9]));
				stats[clientnum]->killer_monster = monster;
		    } else { // anonymous monster
		        auto monster = static_cast<Monster>(SDLNet_Read32(&net_packet->data[9]));
		        stats[clientnum]->killer_monster = monster;
				stats[clientnum]->killer_name = "";
		    }
		} else if (killer == KilledBy::ITEM) {
		    auto item = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[8]));
		    stats[clientnum]->killer_item = item;
		}

		if ( players[clientnum] && players[clientnum]->entity && players[clientnum]->entity->playerCreatedDeathCam != 0 )
		{
			// don't spawn deathcam
		}
		else
		{
			Entity* entity = newEntity(-1, 1, map.entities, nullptr);
			entity->x = cameras[clientnum].x * 16;
			entity->y = cameras[clientnum].y * 16;
			entity->z = -2;
			entity->flags[NOUPDATE] = true;
			entity->flags[PASSABLE] = true;
			entity->flags[INVISIBLE] = true;
			entity->behavior = &actDeathCam;
			entity->skill[2] = clientnum;
			entity->yaw = cameras[clientnum].ang;
			entity->pitch = PI / 8;
			players[clientnum]->ghost.initTeleportLocations(entity->x / 16, entity->y / 16);
		}

		//deleteSaveGame(multiplayer); // stops save scumming c: //Not here, because it'll make the game unresumable if the game crashes but not all players have died.

		players[clientnum]->closeAllGUIs(CloseGUIShootmode::CLOSEGUI_ENABLE_SHOOTMODE, CloseGUIIgnore::CLOSEGUI_CLOSE_ALL);
		players[clientnum]->bControlEnabled = false;

#ifdef SOUND
		levelmusicplaying = true;
		combatmusicplaying = false;
		fadein_increment = default_fadein_increment * 4;
		fadeout_increment = default_fadeout_increment * 4;
		playMusic(gameovermusic, false, false, false);
#endif
		combat = false;
		assailant[clientnum] = false;
		assailantTimer[clientnum] = 0;

		if ( !keepInventoryGlobal )
		{
		    node_t* nextnode;
			for ( auto node = stats[clientnum]->inventory.first; node != nullptr; node = nextnode )
			{
				nextnode = node->next;
				auto item = static_cast<Item*>(node->element);
				if ( itemCategory(item) == SPELL_CAT )
				{
					continue;    // don't drop spells on death, stupid!
				}
				if ( itemIsEquipped(item, clientnum) )
				{
					continue;
				}
				strcpy((char*)net_packet->data, "DIEI");
				SDLNet_Write32(item->type, &net_packet->data[4]);
				SDLNet_Write32(item->status, &net_packet->data[8]);
				SDLNet_Write32(static_cast<Uint32>(item->beatitude), &net_packet->data[12]);
				SDLNet_Write32(static_cast<Uint32>(item->count), &net_packet->data[16]);
				SDLNet_Write32(item->appearance, &net_packet->data[20]);
				net_packet->data[24] = item->identified;
				net_packet->data[25] = clientnum;
				net_packet->data[26] = static_cast<Uint8>(cameras[clientnum].x);
				net_packet->data[27] = static_cast<Uint8>(cameras[clientnum].y);
				net_packet->address.host = net_server.host;
				net_packet->address.port = net_server.port;
				net_packet->len = 28;
				sendPacketSafe(net_sock, -1, net_packet, 0);
			}
		}
		else
		{
			// to not soft lock at Herx
			node_t *node, *nextnode;
			for ( node = stats[clientnum]->inventory.first; node != nullptr; node = nextnode )
			{
				nextnode = node->next;
				auto item = static_cast<Item*>(node->element);
				if ( itemCategory(item) == SPELL_CAT )
				{
					continue;
				}
				if ( item->type == ARTIFACT_ORB_PURPLE || item->type == TOOL_DUCK )
				{
					Item** slot = itemSlot(stats[clientnum], item);
					if ( slot != nullptr )
					{
						*slot = nullptr;
					}

					players[clientnum]->paperDoll.updateSlots();

					strcpy((char*)net_packet->data, "DIEI");
					SDLNet_Write32(item->type, &net_packet->data[4]);
					SDLNet_Write32(item->status, &net_packet->data[8]);
					SDLNet_Write32(static_cast<Uint32>(item->beatitude), &net_packet->data[12]);
					SDLNet_Write32(static_cast<Uint32>(item->count), &net_packet->data[16]);
					SDLNet_Write32(item->appearance, &net_packet->data[20]);
					net_packet->data[24] = item->identified;
					net_packet->data[25] = clientnum;
					net_packet->data[26] = static_cast<Uint8>(cameras[clientnum].x);
					net_packet->data[27] = static_cast<Uint8>(cameras[clientnum].y);
					net_packet->address.host = net_server.host;
					net_packet->address.port = net_server.port;
					net_packet->len = 28;
					sendPacketSafe(net_sock, -1, net_packet, 0);
					list_RemoveNode(node);
				}
			}
		}

		for ( node_t* mapNode = map.creatures->first; mapNode != nullptr; mapNode = mapNode->next )
		{
			auto mapCreature = static_cast<Entity*>(mapNode->element);
			if ( mapCreature )
			{
				if ( mapCreature->monsterEntityRenderAsTelepath == 1 )
				{
					mapCreature->monsterEntityRenderAsTelepath = 0; // do a final pass to undo any telepath rendering.
				}
			}
		}
	}},

	// server forwarded a player callout
	{ 'CALL', []() {
		const int pnum = net_packet->data[4];
		if (!barony::net::validPlayer(pnum))
		{
			return;
		}
		if ( pnum != clientnum )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			Entity* entity = nullptr;
			if ( uid != 0 )
			{
				entity = uidToEntity(uid);
				if ( !entity )
				{
					return;
				}
			}
			CalloutMenu[pnum].lockOnEntityUid = uid;
			auto cmd = static_cast<CalloutRadialMenu::CalloutCommand>(net_packet->data[9]);
			CalloutMenu[pnum].clientCalloutHelpFlags = SDLNet_Read32(&net_packet->data[10]);
			if ( uid )
			{
				if ( entity )
				{
					CalloutMenu[pnum].createParticleCallout(entity, cmd);
				}
			}
			else
			{
				real_t x = SDLNet_Read16(&net_packet->data[14]);
				real_t y = SDLNet_Read16(&net_packet->data[16]);
				CalloutMenu[pnum].createParticleCallout(
					x * 16.0 + 8.0, y * 16.0 + 8.0, -4, 0, cmd);
			}
		}
	}},

	// textbox message
	{'MSGS', [](){
		Uint32 color = SDLNet_Read32(&net_packet->data[4]);
		auto type = static_cast<MessageType>(SDLNet_Read32(&net_packet->data[8]));
		auto msg = (const char*)(&net_packet->data[12]);

		if ( ticks != 1 )
		{
			const bool printed = messagePlayerColor(clientnum, type, color, "%s", msg);
			if (type == MESSAGE_CHAT && printed)
			{
				playSound(Message::CHAT_MESSAGE_SFX, 64);
			}
		}

		if ( !strcmp(msg, Language::get(1109)) ) // "you survive through your party's persistence"
		{
			// ... or lived
			stats[clientnum]->HP = stats[clientnum]->MAXHP * 0.5;
			stats[clientnum]->MP = stats[clientnum]->MAXMP * 0.5;
			stats[clientnum]->HUNGER = 500;
			for ( int c = 0; c < NUMEFFECTS; c++ )
			{
				if ( !(c == EFF_VAMPIRICAURA && stats[clientnum]->EFFECTS_TIMERS[c] == -2)
					&& c != EFF_WITHDRAWAL && c != EFF_SHAPESHIFT )
				{
					stats[clientnum]->clearEffect(c);
					stats[clientnum]->EFFECTS_TIMERS[c] = 0;
				}
			}
		}
		else if ( !strncmp(msg, Language::get(1114), 28) ) // Zap brigade music
		{
			playSoundNotification(175, 128);
		}
		else if ( (strstr(msg, Language::get(1160))) != nullptr) // <player name> bumps you
		{
			for ( int c = 0; c < barony::net::playerCapacity(); c++ )
			{
				if ( !strncmp(stats[c]->name, msg, strlen(stats[c]->name)) )
				{
					if (players[clientnum] && players[clientnum]->entity && players[c] && players[c]->entity)
					{
						double tangent = atan2(players[clientnum]->entity->y - players[c]->entity->y, players[clientnum]->entity->x - players[c]->entity->x);
						players[clientnum]->entity->vel_x += cos(tangent);
						players[clientnum]->entity->vel_y += sin(tangent);
					}
					break;
				}
			}
		}
		return;
	}},

	// update magic
	{'UPMP', [](){
		stats[clientnum]->MP = SDLNet_Read32(&net_packet->data[4]);
		return;
	}},

	// update effects flags
	{'UPEF', [](){
		int numBytes = NUMEFFECTS / 8;
		for (int c = 0; c < NUMEFFECTS; c++)
		{
			if ( net_packet->data[4 + c / 8]&power(2, c - (c / 8) * 8) )
			{
				stats[clientnum]->setEffectValueUnsafe(c, 1);
				if ( net_packet->data[4 + numBytes + c / 8] & power(2, c - (c / 8) * 8) ) // use these bits to denote if duration is low.
				{
					stats[clientnum]->EFFECTS_TIMERS[c] = 1;
				}
				else if ( stats[clientnum]->EFFECTS_TIMERS[c] > 0 )
				{
					stats[clientnum]->EFFECTS_TIMERS[c] = 0;
				}
			}
			else
			{
				stats[clientnum]->clearEffect(c);
				if ( stats[clientnum]->EFFECTS_TIMERS[c] > 0 )
				{
					stats[clientnum]->EFFECTS_TIMERS[c] = 0;
				}
			}
		}

		int numEffectStrengths = net_packet->data[4 + numBytes * 2];
		int index = 0;
		while ( numEffectStrengths > 0 )
		{
			int currentIndex = (4 + numBytes * 2 + 1) + index;
			if ( currentIndex + 1 >= NET_PACKET_SIZE || ((currentIndex + 1) >= net_packet->len) )
			{
				// too much data to read, abort
				break;
			}
			int effectIndex = net_packet->data[currentIndex + 0];
			Uint8 effectStrength = net_packet->data[currentIndex + 1];
			stats[clientnum]->setEffectValueUnsafe(effectIndex, effectStrength);
			index += 2;
			--numEffectStrengths;
		}
	}},

	// update entity stat flag
	{'ENSF', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			if ( entity->getStats() )
			{
				entity->getStats()->MISC_FLAGS[net_packet->data[8]] = SDLNet_Read32(&net_packet->data[9]);
			}
		}
	}},

	// update attributes
	{'ATTR', [](){
		stats[clientnum]->STR = (static_cast<Sint8>(net_packet->data[5]) <= -8) ? net_packet->data[5] : static_cast<Sint8>(net_packet->data[5]);
		stats[clientnum]->DEX = (static_cast<Sint8>(net_packet->data[6]) <= -8) ? net_packet->data[6] : static_cast<Sint8>(net_packet->data[6]);
		stats[clientnum]->CON = (static_cast<Sint8>(net_packet->data[7]) <= -8) ? net_packet->data[7] : static_cast<Sint8>(net_packet->data[7]);
		stats[clientnum]->INT = (static_cast<Sint8>(net_packet->data[8]) <= -8) ? net_packet->data[8] : static_cast<Sint8>(net_packet->data[8]);
		stats[clientnum]->PER = (static_cast<Sint8>(net_packet->data[9]) <= -8) ? net_packet->data[9] : static_cast<Sint8>(net_packet->data[9]);
		stats[clientnum]->CHR = (static_cast<Sint8>(net_packet->data[10]) <= -8) ? net_packet->data[10] : static_cast<Sint8>(net_packet->data[10]);
		stats[clientnum]->EXP = net_packet->data[11];
		stats[clientnum]->LVL = net_packet->data[12];
		stats[clientnum]->HP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[13]));
		stats[clientnum]->MAXHP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[15]));
		stats[clientnum]->MP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[17]));
		stats[clientnum]->MAXMP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[19]));
	}},

	// level up icon timers, sets second row of icons if double stat gain is rolled.
	{'LVLI', [](){
		// Note - set to 250 ticks, higher values will require resending/using 16 bit data.
		players[clientnum]->hud.xpBar.animateState = Player::HUD_t::AnimateStates::ANIMATE_LEVELUP_RISING;
		players[clientnum]->hud.xpBar.xpLevelups++;

		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_STR] = net_packet->data[5];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_DEX] = net_packet->data[6];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_CON] = net_packet->data[7];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_INT] = net_packet->data[8];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_PER] = net_packet->data[9];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_CHR] = net_packet->data[10];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_STR + NUMSTATS] = net_packet->data[11];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_DEX + NUMSTATS] = net_packet->data[12];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_CON + NUMSTATS] = net_packet->data[13];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_INT + NUMSTATS] = net_packet->data[14];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_PER + NUMSTATS] = net_packet->data[15];
		stats[clientnum]->PLAYER_LVL_STAT_TIMER[STAT_CHR + NUMSTATS] = net_packet->data[16];

		std::vector<LevelUpAnimation_t::LevelUp_t::StatUp_t> StatUps;
		for ( int i = 0; i < NUMSTATS; ++i )
		{
			if ( stats[clientnum]->PLAYER_LVL_STAT_TIMER[i] > 0 )
			{
				int increase = 1;
				if ( stats[clientnum]->PLAYER_LVL_STAT_TIMER[i + NUMSTATS] > 0 )
				{
					++increase;
				}
				int currentStat = 0;
				switch ( i )
				{
					case STAT_STR:
						currentStat = stats[clientnum]->STR;
						break;
					case STAT_DEX:
						currentStat = stats[clientnum]->DEX;
						break;
					case STAT_CON:
						currentStat = stats[clientnum]->CON;
						break;
					case STAT_INT:
						currentStat = stats[clientnum]->INT;
						break;
					case STAT_PER:
						currentStat = stats[clientnum]->PER;
						break;
					case STAT_CHR:
						currentStat = stats[clientnum]->CHR;
						break;
					default:
						break;
				}
				StatUps.push_back(LevelUpAnimation_t::LevelUp_t::StatUp_t(i, currentStat - increase, increase));
			}
		}
		levelUpAnimation[clientnum].addLevelUp(stats[clientnum]->LVL - 1, 1, StatUps);
	}},

	// killed a monster
	{'MKIL', [](){
		const int monster = net_packet->data[4];
		if ( monster >= 0 && monster < NUMMONSTERS )
		{
			kills[monster]++;
		}
	}},

	// update skill
	{'SKIL', [](){
	    const int pro = std::min(net_packet->data[5], static_cast<Uint8>(NUMPROFICIENCIES - 1));
		int oldSkill = stats[clientnum]->getProficiency(pro);
		stats[clientnum]->setProficiency(pro, (net_packet->data[6] & 0x7F));
		bool notify = (net_packet->data[6] & (1 << 7)) != 0;

		int statBonusSkill = getStatForProficiency(pro);

		if ( statBonusSkill >= STAT_STR )
		{
			// stat has chance for bonus point if the relevant proficiency has been trained.
			// write the last proficiency that effected the skill.
			stats[clientnum]->PLAYER_LVL_STAT_BONUS[statBonusSkill] = pro;
		}

		if ( pro == PRO_ALCHEMY )
		{
			GenericGUI[clientnum].alchemyLearnRecipeOnLevelUp(stats[clientnum]->getProficiency(pro));
		}
		if ( oldSkill < 100 )
		{
			if ( notify )
			{
				skillUpAnimation[clientnum].addSkillUp(pro, oldSkill, stats[clientnum]->getProficiency(pro) - oldSkill);
			}
		}
	}},

	//Add spell.
	{'ASPL', [](){
		addSpell(net_packet->data[5], clientnum, true);
	}},

	// update hunger
	{'HNGR', [](){
		stats[clientnum]->HUNGER = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[4]));
	}},

	// update player stat values
	{'STAT', [](){
		Sint32 buffer = 0;
		for ( int i = 0; i < barony::net::playerCapacity(); ++i )
		{
			buffer = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[4 + i * 8]));
			stats[i]->MAXHP = buffer & 0xFFFF;
			stats[i]->HP = (buffer >> 16) & 0xFFFF;
			buffer = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[8 + i * 8]));
			stats[i]->MAXMP = buffer & 0xFFFF;
			stats[i]->MP = (buffer >> 16) & 0xFFFF;
		}
	}},

	// update sex
	{'SEXU', [](){
		int player = net_packet->data[4];
		if ( player < 0 || !barony::net::validPlayer(player) || !stats[player] )
		{
			return;
		}
		stats[player]->sex = static_cast<sex_t>(net_packet->data[5]);
		//messagePlayer(clientnum, "Received player: %d sex: %d", player, stats[player]->sex);
		return;
	}},

	{'COND', [](){
		int conduct = SDLNet_Read16(&net_packet->data[4]);
		int value = SDLNet_Read16(&net_packet->data[6]);
		conductGameChallenges[conduct] = value;
		//messagePlayer(clientnum, "received %d %d, set to %d", conduct, value, conductGameChallenges[conduct]);
	}},

	// update player statistics
	{'GPST', [](){
		int gameplayStat = SDLNet_Read32(&net_packet->data[4]);
		int changeval = SDLNet_Read32(&net_packet->data[8]);
		if ( gameplayStat == STATISTICS_TEMPT_FATE )
		{
			if ( gameStatistics[STATISTICS_TEMPT_FATE] == -1 )
			{
				// don't change, completed task.
			}
			else
			{
				if ( changeval == 5 )
				{
					gameStatistics[gameplayStat] = changeval;
				}
				else if ( changeval == 1 && gameStatistics[gameplayStat] > 0 )
				{
					gameStatistics[gameplayStat] = -1;
				}
			}
		}
		else if ( gameplayStat == STATISTICS_FORUM_TROLL )
		{
			if ( changeval == AchievementObserver::FORUM_TROLL_BREAK_WALL )
			{
				int walls = gameStatistics[gameplayStat] & 0xFF;
				walls = std::min(walls + 1, 3);
				gameStatistics[gameplayStat] = gameStatistics[gameplayStat] & 0xFFFFFF00;
				gameStatistics[gameplayStat] |= walls;
			}
			else if ( changeval == AchievementObserver::FORUM_TROLL_RECRUIT_TROLL )
			{
				int trolls = (gameStatistics[gameplayStat] >> 8) & 0xFF;
				trolls = std::min(trolls + 1, 3);
				gameStatistics[gameplayStat] = gameStatistics[gameplayStat] & 0xFFFF00FF;
				gameStatistics[gameplayStat] |= (trolls << 8);
			}
			else if ( changeval == AchievementObserver::FORUM_TROLL_FEAR )
			{
				int fears = (gameStatistics[gameplayStat] >> 16) & 0xFF;
				fears = std::min(fears + 1, 3);
				gameStatistics[gameplayStat] = gameStatistics[gameplayStat] & 0xFF00FFFF;
				gameStatistics[gameplayStat] |= (fears << 16);
			}
		}
		else if ( gameplayStat == STATISTICS_POP_QUIZ_1 || gameplayStat == STATISTICS_POP_QUIZ_2 )
		{
			int spellID = changeval;
			if ( spellID >= 32 )
			{
				spellID -= 32;
				int shifted = (1 << spellID);
				gameStatistics[gameplayStat] |= shifted;
			}
			else
			{
				int shifted = (1 << spellID);
				gameStatistics[gameplayStat] |= shifted;
			}
		}
		else if ( gameplayStat == STATISTICS_FLAVORTOWN )
		{
			gameStatistics[gameplayStat] |= changeval;
		}
		else if ( gameplayStat == STATISTICS_BARDIC_INSPIRATION )
		{
			if ( changeval == 0 )
			{
				gameStatistics[gameplayStat] = 0;
			}
			else
			{
				gameStatistics[gameplayStat] += changeval;
			}
		}
		else if ( gameplayStat == STATISTICS_PARRY_TANK )
		{
			if ( changeval == 0 )
			{
				if ( gameStatistics[gameplayStat] < 20 )
				{
					gameStatistics[gameplayStat] = 0;
				}
			}
			else
			{
				gameStatistics[gameplayStat] += changeval;
			}
		}
		else
		{
			gameStatistics[gameplayStat] += changeval;
		}
		//messagePlayer(clientnum, "received: %d, %d, val: %d", gameplayStat, changeval, gameStatistics[gameplayStat]);
	}},

	// update player levels
	{'UPLV', [](){
		constexpr std::size_t headerSize = 5;
		constexpr std::size_t recordSize = 5;
		if (net_packet->len < static_cast<int>(headerSize))
		{
			return;
		}

		const std::size_t count = net_packet->data[4];
		const std::size_t required = headerSize + count * recordSize;
		if (required > static_cast<std::size_t>(net_packet->len))
		{
			return;
		}

		std::size_t offset = headerSize;
		for (std::size_t record = 0; record < count; ++record)
		{
			const std::size_t player = net_packet->data[offset];
			if (player < std::size(stats) && stats[player])
			{
				stats[player]->LVL = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[offset + 1]));
			}
			offset += recordSize;
		}
	}},

	// level change
	{'LVLC', [](){
		if ( currentlevel == net_packet->data[13] && secretlevel == net_packet->data[4] )
		{
			// the server's just doing a routine check
			return;
		}
		if ( net_packet->data[13] == 0 )
		{
			return; // dont warp back to start level
		}
		changeLevel();
	}},

	// level reminder
	{'LVLR', [](){
		changeLevel();
	}},

	// lead a monster
	{'LEAD', [](){
		auto uidnum = static_cast<Uint32*>(malloc(sizeof(Uint32)));
		*uidnum = SDLNet_Read32(&net_packet->data[4]);
		node_t* node = list_AddNodeLast(&stats[clientnum]->FOLLOWERS);
		node->element = uidnum;
		node->deconstructor = &defaultDeconstructor;
		node->size = sizeof(Uint32);

		Entity* monster = uidToEntity(*uidnum);
		if ( monster )
		{
			if ( !monster->clientsHaveItsStats )
			{
				monster->giveClientStats();
			}
			if ( monster->clientStats )
			{
                monster->clientStats->type = static_cast<Monster>(SDLNet_Read32(&net_packet->data[8]));
				if ( static_cast<Sint8>(net_packet->data[12]) == '$' )
				{
					char buf[128];
					memset(buf, 0, sizeof(buf));
					strcpy(buf, (char*)&net_packet->data[13]);
					bool found = false;
					for ( int type = 0; type < NUMMONSTERS; ++type )
					{
						if ( MonsterData_t::monsterDataEntries[type].specialNPCs.find(buf) != MonsterData_t::monsterDataEntries[type].specialNPCs.end() )
						{
							strcpy(monster->clientStats->name, MonsterData_t::monsterDataEntries[type].specialNPCs[buf].name.c_str());
							found = true;
							break;
						}
					}
					if ( !found )
					{
						strcpy(monster->clientStats->name, buf);
					}
				}
				else
				{
					strcpy(monster->clientStats->name, (char*)&net_packet->data[12]);
				}
                if ( monster->clientStats->name[0] && (!monsterNameIsGeneric(*monster->clientStats) || monster->clientStats->type == SLIME))
				{
                    Entity* nametag = newEntity(-1, 1, map.entities, nullptr);
                    nametag->x = monster->x;
                    nametag->y = monster->y;
                    nametag->z = monster->z - 6;
                    nametag->sizex = 1;
                    nametag->sizey = 1;
                    nametag->flags[NOUPDATE] = true;
                    nametag->flags[PASSABLE] = true;
                    nametag->flags[SPRITE] = true;
                    nametag->flags[UNCLICKABLE] = true;
                    nametag->flags[BRIGHT] = true;
                    nametag->behavior = &actSpriteNametag;
                    nametag->parent = monster->getUID();
                    nametag->scalex = 0.2;
                    nametag->scaley = 0.2;
                    nametag->scalez = 0.2;
                    nametag->skill[0] = clientnum;
                    nametag->skill[1] = playerColor(clientnum, colorblind_lobby, true);
                }
			}
			if ( !FollowerMenu[clientnum].recentEntity )
			{
				FollowerMenu[clientnum].recentEntity = monster;
			}
		}
	}},

	// remove a monster from followers list
	{'LDEL', [](){
		Uint32 uidnum = SDLNet_Read32(&net_packet->data[4]);
		if ( stats[clientnum] )
		{
			for ( node_t* allyNode = stats[clientnum]->FOLLOWERS.first; allyNode != nullptr; allyNode = allyNode->next )
			{
				if ( static_cast<Uint32*>(allyNode->element) && *static_cast<Uint32*>(allyNode->element) == uidnum )
				{
					if ( FollowerMenu[clientnum].recentEntity && (FollowerMenu[clientnum].recentEntity->getUID() == 0
						|| FollowerMenu[clientnum].recentEntity->getUID() == uidnum) )
					{
						FollowerMenu[clientnum].recentEntity = nullptr;
					}
					if ( FollowerMenu[clientnum].followerToCommand == uidToEntity(uidnum) )
					{
						FollowerMenu[clientnum].closeFollowerMenuGUI();
					}
					list_RemoveNode(allyNode);
					break;
				}
			}
		}
	}},

	// update client's follower data on level up or initial follow.
	{'NPCI', [](){
		Uint32 uidnum = SDLNet_Read32(&net_packet->data[4]);
		Entity* monster = uidToEntity(uidnum);
		if ( monster )
		{
			if ( !monster->clientsHaveItsStats )
			{
				monster->giveClientStats();
			}
			if ( monster->clientStats )
			{
				monster->clientStats->LVL = net_packet->data[8];
				monster->clientStats->HP = SDLNet_Read16(&net_packet->data[9]);
				monster->clientStats->MAXHP = SDLNet_Read16(&net_packet->data[11]);
				monster->clientStats->type = static_cast<Monster>(net_packet->data[13]);
			}
		}
	}},

	// update client's follower hp/maxhp data at intervals
	{'NPCU', [](){
		Uint32 uidnum = SDLNet_Read32(&net_packet->data[4]);
		Entity* monster = uidToEntity(uidnum);
		if ( monster )
		{
			if ( !monster->clientsHaveItsStats )
			{
				monster->giveClientStats();
			}
			if ( monster->clientStats )
			{
				monster->clientStats->HP = SDLNet_Read16(&net_packet->data[8]);
				monster->clientStats->MAXHP = SDLNet_Read16(&net_packet->data[10]);
			}
		}
	}},

	// bless my equipment
	{'BLES', [](){
		if ( stats[clientnum]->helmet )
		{
			stats[clientnum]->helmet->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->helmet->type, 1);
		}
		if ( stats[clientnum]->breastplate )
		{
			stats[clientnum]->breastplate->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->breastplate->type, 1);
		}
		if ( stats[clientnum]->gloves )
		{
			stats[clientnum]->gloves->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->gloves->type, 1);
		}
		if ( stats[clientnum]->shoes )
		{
			stats[clientnum]->shoes->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->shoes->type, 1);
		}
		if ( stats[clientnum]->shield )
		{
			stats[clientnum]->shield->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->shield->type, 1);
		}
		if ( stats[clientnum]->weapon )
		{
			stats[clientnum]->weapon->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->weapon->type, 1);
		}
		if ( stats[clientnum]->cloak )
		{
			stats[clientnum]->cloak->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->cloak->type, 1);
		}
		if ( stats[clientnum]->amulet )
		{
			stats[clientnum]->amulet->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->amulet->type, 1);
		}
		if ( stats[clientnum]->ring )
		{
			stats[clientnum]->ring->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->ring->type, 1);
		}
		if ( stats[clientnum]->mask )
		{
			stats[clientnum]->mask->beatitude++;
			Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->mask->type, 1);
		}
	}},

	// bless one piece of my equipment
	{'BLE1', [](){
		Uint32 chosen = SDLNet_Read32(&net_packet->data[4]);
		switch ( chosen )
		{
			case 0:
				if ( stats[clientnum]->helmet )
				{
					stats[clientnum]->helmet->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->helmet->type, 1);
				}
				break;
			case 1:
				if ( stats[clientnum]->breastplate )
				{
					stats[clientnum]->breastplate->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->breastplate->type, 1);
				}
				break;
			case 2:
				if ( stats[clientnum]->gloves )
				{
					stats[clientnum]->gloves->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->gloves->type, 1);
				}
				break;
			case 3:
				if ( stats[clientnum]->shoes )
				{
					stats[clientnum]->shoes->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->shoes->type, 1);
				}
				break;
			case 4:
				if ( stats[clientnum]->shield )
				{
					stats[clientnum]->shield->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->shield->type, 1);
				}
				break;
			case 5:
				if ( stats[clientnum]->weapon )
				{
					stats[clientnum]->weapon->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->weapon->type, 1);
				}
				break;
			case 6:
				if ( stats[clientnum]->cloak )
				{
					stats[clientnum]->cloak->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->cloak->type, 1);
				}
				break;
			case 7:
				if ( stats[clientnum]->amulet )
				{
					stats[clientnum]->amulet->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->amulet->type, 1);
				}
				break;
			case 8:
				if ( stats[clientnum]->ring )
				{
					stats[clientnum]->ring->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->ring->type, 1);
				}
				break;
			case 9:
				if ( stats[clientnum]->mask )
				{
					stats[clientnum]->mask->beatitude++;
					Compendium_t::Events_t::eventUpdate(clientnum, Compendium_t::CPDM_BLESSED_TOTAL, stats[clientnum]->mask->type, 1);
				}
				break;
			default:
				break;
		}
	}},

	// update entity appearance (sprite)
	{'ENTA', [](){
		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			entity->sprite = SDLNet_Read32(&net_packet->data[8]);
		}
	}},

	// monster summon
	{'SUMM', [](){
		auto monster = static_cast<Monster>(SDLNet_Read32(&net_packet->data[4]));
		Sint32 x = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[8]));
		Sint32 y = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[12]));
		Uint32 uid = SDLNet_Read32(&net_packet->data[16]);
		summonMonsterClient(monster, x, y, uid);
	}},

	// monster summon
	{'SUMS', [](){
		if ( stats[clientnum] )
		{
			stats[clientnum]->playerSummonLVLHP = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[4]));
			stats[clientnum]->playerSummonSTRDEXCONINT = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[8]));
			stats[clientnum]->playerSummonPERCHR = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[12]));
			stats[clientnum]->playerSummon2LVLHP = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[16]));
			stats[clientnum]->playerSummon2STRDEXCONINT = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[20]));
			stats[clientnum]->playerSummon2PERCHR = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[24]));
		}
	}},

	//Multiplayer chest code (client).
	{'CHST', [](){
		if ( openedChest[clientnum] )
		{
			//Close the chest.
			closeChestClientside(clientnum);
		}

		Entity *entity = uidToEntity(static_cast<int>(SDLNet_Read32(&net_packet->data[4])));
		if ( entity )
		{
			openedChest[clientnum] = entity; //Set the opened chest to this.
			GenericGUI[clientnum].closeGUI();
			list_FreeAll(&chestInv[clientnum]);
			chestInv[clientnum].first = nullptr;
			chestInv[clientnum].last = nullptr;
			players[clientnum]->openStatusScreen(GUI_MODE_INVENTORY, INVENTORY_MODE_ITEM);
			players[clientnum]->GUI.activateModule(Player::GUI_t::MODULE_CHEST);
			bool voidChest = net_packet->data[8] == 0 ? false : true;
			players[clientnum]->inventoryUI.chestGUI.openChest(voidChest);
		}
	}},

	//Add an item to the chest.
	{'CITM', [](){
		auto itemType = static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4]));
		auto status = static_cast<Status>(SDLNet_Read32(&net_packet->data[8]));
		Sint16 beatitude = SDLNet_Read32(&net_packet->data[12]);
		Sint16 count = SDLNet_Read32(&net_packet->data[16]);
		Uint32 appearance = SDLNet_Read32(&net_packet->data[20]);
		bool identified = false;
		if ( net_packet->data[24])   //TODO: Is this right?
		{
			identified = true;
		}
		else
		{
			identified = false;
		}
		Item* newitem = newItem(itemType, status, beatitude, count, appearance, identified, nullptr);
		bool forceNewStack = net_packet->data[25] ? true : false;
		newitem->x = static_cast<Sint8>(net_packet->data[26]);
		newitem->y = static_cast<Sint8>(net_packet->data[27]);
		addItemToChestClientside(clientnum, newitem, forceNewStack, nullptr);
	}},

	//Close the chest.
	{'CCLS', [](){
		closeChestClientside(clientnum);
	}},

	//Open up the GUI to identify an item.
	{'IDEN', [](){
		if ( net_packet->data[4] == 1 ) // spellbook
		{
			int beatitude = static_cast<Sint8>(net_packet->data[5]);
			GenericGUI[clientnum].openGUI(GUI_TYPE_ITEMFX, nullptr, beatitude, getSpellbookFromSpellID(SPELL_IDENTIFY), SPELL_IDENTIFY);
		}
		else
		{
			GenericGUI[clientnum].openGUI(GUI_TYPE_ITEMFX, nullptr, 0, SPELL_ITEM, SPELL_IDENTIFY);
		}
	}},

	// Open up the Remove Curse GUI
	{'CRCU', [](){
		//Uncurse an item
		if ( net_packet->data[4] == 1 ) // spellbook
		{
			int beatitude = static_cast<Sint8>(net_packet->data[5]);
			GenericGUI[clientnum].openGUI(GUI_TYPE_ITEMFX, nullptr, beatitude, getSpellbookFromSpellID(SPELL_REMOVECURSE), SPELL_REMOVECURSE);
		}
		else
		{
			GenericGUI[clientnum].openGUI(GUI_TYPE_ITEMFX, nullptr, 0, SPELL_ITEM, SPELL_REMOVECURSE);
		}
	}},

	{'FXSP', []() {
		int spellID = SDLNet_Read32(&net_packet->data[6]);
		if ( net_packet->data[4] == 1 ) // spellbook
		{
			int beatitude = static_cast<Sint8>(net_packet->data[5]);
			GenericGUI[clientnum].openGUI(GUI_TYPE_ITEMFX, nullptr, beatitude, getSpellbookFromSpellID(spellID), spellID);
		}
		else
		{
			GenericGUI[clientnum].openGUI(GUI_TYPE_ITEMFX, nullptr, 0, SPELL_ITEM, spellID);
		}
	}},

	//Add a spell to the channeled spells list.
	{'CHAN', [](){
		if ( auto spell = getSpellFromID(SDLNet_Read32(&net_packet->data[5])) )
		{
			if ( spell_t* thespell = copySpell(spell) )
			{
				auto node = list_AddNodeLast(&channeledSpells[clientnum]);
				node->element = thespell;
				node->size = sizeof(spell_t);
				//node->deconstructor = &spellDeconstructor_Channeled;
				node->deconstructor = &spellChanneledClientDeconstructor;
				static_cast<spell_t*>(node->element)->sustain_node = node;
			}
		}
	}},

	//Remove a spell from the channeled spells list.
	{'UNCH', [](){
		spell_t* thespell = getSpellFromID(SDLNet_Read32(&net_packet->data[5]));
		if (spellInList(&channeledSpells[clientnum], thespell))
		{
			node_t *node, *nextnode;
			for (node = channeledSpells[clientnum].first; node; node = nextnode)
			{
				nextnode = node->next;
				auto spell_search = static_cast<spell_t*>(node->element);
				if (spell_search->ID == thespell->ID)
				{
					list_RemoveNode(node);
					node = nullptr;
				}
			}
		}
	}},

	//Map the magic. I mean magic the map. I mean magically map the level (client).
	{'MMAP', [](){
		int radius = SDLNet_Read16(&net_packet->data[4]);
		int x = SDLNet_Read16(&net_packet->data[6]);
		int y = SDLNet_Read16(&net_packet->data[8]);
		spell_magicMap(clientnum, radius, x, y);
	}},

	{'MFOD', [](){
		mapFoodOnLevel(clientnum);
	}},

	{'TKIT', [](){
		GenericGUI[clientnum].tinkeringKitDegradeOnUse(clientnum);
	}},

	// leaf pile
	{ 'LEAF', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( Entity* entity = uidToEntity(uid) )
		{
			if ( net_packet->data[8] == 1 )
			{
				entity->skill[3] = static_cast<Sint32>(net_packet->data[9]);
				entity->skill[4] = static_cast<Sint32>(net_packet->data[10]);
				playSoundEntityLocal(entity, 754 + local_rng.rand() % 2, 64);
			}
			else if ( net_packet->data[8] == 2 )
			{
				entity->skill[7] = static_cast<Sint32>(net_packet->data[9]);
				entity->skill[8] = static_cast<Sint32>(net_packet->data[10]);
			}
		}
	} },

	// boss death
	{'BDTH', [](){
		for ( auto node = map.entities->first; node != nullptr; node = node->next )
		{
			auto entity = static_cast<Entity*>(node->element);
			if ( strstr(map.name, "Hell") )
			{
				if ( entity->behavior == &actWinningPortal )
				{
					//entity->flags[INVISIBLE] = false;
				}
			}
			else if ( strstr(map.name, "Boss") )
			{
				if ( entity->behavior == &actPedestalBase )
				{
					entity->pedestalInit = 1;
				}
			}
		}
		if ( strstr(map.name, "Hell") )
		{
			int x, y;
			for ( y = map.height / 2 - 1; y < map.height / 2 + 2; y++ )
			{
				for ( x = 3; x < map.width / 2; x++ )
				{
					if ( !map.tiles[y * MAP_LAYERS + x * MAP_LAYERS * map.height] )
					{
						map.tiles[y * MAP_LAYERS + x * MAP_LAYERS * map.height] = 72;
					}
				}
			}
		}
	}},

	// update svFlags
	{'SVFL', [](){
		svFlags = SDLNet_Read32(&net_packet->data[4]);
		lobbyWindowSvFlags = svFlags;
	}},

	// kick
	{'KICK', [](){
		MainMenu::timedOut();
	}},

	// win the game
	{'WING', [](){
		if ( net_packet->data[4] == 100 || net_packet->data[4] == 101 )
		{
			movie = true;
			pauseGame(2, 0);
			MainMenu::destroyMainMenu();
			MainMenu::createDummyMainMenu();
			beginFade(MainMenu::FadeDestination::Endgame);
			return;
		}


		int victoryType;
		int race = RACE_HUMAN;
		if ( stats[clientnum]->playerRace != RACE_HUMAN && stats[clientnum]->stat_appearance == 0 )
		{
			race = stats[clientnum]->playerRace;
		}

		switch ( race ) {
		default: victoryType = 3; break;
		case RACE_HUMAN: victoryType = 4; break;
		case RACE_SKELETON: victoryType = 5; break;
		case RACE_VAMPIRE: victoryType = 5; break;
		case RACE_SUCCUBUS: victoryType = 5; break;
		case RACE_GOATMAN: victoryType = 3; break;
		case RACE_AUTOMATON: victoryType = 4; break;
		case RACE_INCUBUS: victoryType = 5; break;
		case RACE_GOBLIN: victoryType = 3; break;
		case RACE_INSECTOID: victoryType = 3; break;
		case RACE_RAT: victoryType = 3; break;
		case RACE_TROLL: victoryType = 3; break;
		case RACE_SPIDER: victoryType = 3; break;
		case RACE_IMP: victoryType = 5; break;
		case RACE_DRYAD: victoryType = 4; break;
		case RACE_MYCONID: victoryType = 4; break;
		case RACE_SALAMANDER: victoryType = 4; break;
		case RACE_GREMLIN: victoryType = 5; break;
		case RACE_GNOME: victoryType = 4; break;
		}
		victory = victoryType;
	    if (net_packet->data[5] == 0) { // full ending
	        switch ( race ) {
	        default:
	        case RACE_HUMAN:
			case RACE_GNOME:
			case RACE_DRYAD:
			case RACE_MYCONID:
			case RACE_SALAMANDER:
	            MainMenu::beginFade(MainMenu::FadeDestination::EndingHuman);
	            break;
	        case RACE_AUTOMATON:
	            MainMenu::beginFade(MainMenu::FadeDestination::EndingAutomaton);
	            break;
	        case RACE_GOATMAN:
	        case RACE_GOBLIN:
	        case RACE_INSECTOID:
	            MainMenu::beginFade(MainMenu::FadeDestination::EndingBeast);
	            break;
	        case RACE_SKELETON:
	        case RACE_VAMPIRE:
	        case RACE_SUCCUBUS:
	        case RACE_INCUBUS:
			case RACE_GREMLIN:
	            MainMenu::beginFade(MainMenu::FadeDestination::EndingEvil);
	            break;
	        }
	    }
	    else if (net_packet->data[5] == 1) { // classic herx ending
			victory = 1;
	        switch ( race ) {
	        default:
	        case RACE_HUMAN:
			case RACE_GNOME:
			case RACE_DRYAD:
			case RACE_MYCONID:
			case RACE_SALAMANDER:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicEndingHuman);
	            break;
	        case RACE_AUTOMATON:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicEndingAutomaton);
	            break;
	        case RACE_GOATMAN:
	        case RACE_GOBLIN:
	        case RACE_INSECTOID:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicEndingBeast);
	            break;
	        case RACE_SKELETON:
	        case RACE_VAMPIRE:
	        case RACE_SUCCUBUS:
	        case RACE_INCUBUS:
			case RACE_GREMLIN:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicEndingEvil);
	            break;
	        }
	    }
	    else if (net_packet->data[5] == 2) { // classic baphomet ending
			victory = 2;
	        switch ( race ) {
	        default:
	        case RACE_HUMAN:
			case RACE_GNOME:
			case RACE_DRYAD:
			case RACE_MYCONID:
			case RACE_SALAMANDER:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicBaphometEndingHuman);
	            break;
	        case RACE_AUTOMATON:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicBaphometEndingAutomaton);
	            break;
	        case RACE_GOATMAN:
	        case RACE_GOBLIN:
	        case RACE_INSECTOID:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicBaphometEndingBeast);
	            break;
	        case RACE_SKELETON:
	        case RACE_VAMPIRE:
	        case RACE_SUCCUBUS:
	        case RACE_INCUBUS:
			case RACE_GREMLIN:
	            MainMenu::beginFade(MainMenu::FadeDestination::ClassicBaphometEndingEvil);
	            break;
	        }
	    }

		if ( victory > 0 )
		{
			int k = 0;
			for ( int c = 0; c < barony::net::playerCapacity(); c++ )
			{
				if ( players[c] && players[c]->entity )
				{
					k++;
				}
			}
			if ( k >= 2 )
			{
				steamAchievement("BARONY_ACH_IN_GREATER_NUMBERS");
			}
		}

	    // force game to pause
        movie = true;
		pauseGame(2, false);
	}},

	// mid game cutscene
	{'MIDG', [](){
		int race = RACE_HUMAN;
		if ( stats[clientnum]->playerRace != RACE_HUMAN && stats[clientnum]->stat_appearance == 0 )
		{
			race = stats[clientnum]->playerRace;
		}
	    if (net_packet->data[4] == 0) { // herx midpoint
	        switch ( race ) {
	        default:
	        case RACE_HUMAN:
			case RACE_GNOME:
	            MainMenu::beginFade(MainMenu::FadeDestination::HerxMidpointHuman);
	            break;
	        case RACE_AUTOMATON:
	            MainMenu::beginFade(MainMenu::FadeDestination::HerxMidpointAutomaton);
	            break;
	        case RACE_GOATMAN:
	        case RACE_GOBLIN:
	        case RACE_INSECTOID:
			case RACE_DRYAD:
			case RACE_MYCONID:
			case RACE_SALAMANDER:
	            MainMenu::beginFade(MainMenu::FadeDestination::HerxMidpointBeast);
	            break;
	        case RACE_SKELETON:
	        case RACE_VAMPIRE:
	        case RACE_SUCCUBUS:
	        case RACE_INCUBUS:
			case RACE_GREMLIN:
	            MainMenu::beginFade(MainMenu::FadeDestination::HerxMidpointEvil);
	            break;
	        }
	    }
	    else if (net_packet->data[4] == 1) { // baphomet midpoint
	        switch ( race ) {
	        default:
	        case RACE_HUMAN:
			case RACE_GNOME:
	            MainMenu::beginFade(MainMenu::FadeDestination::BaphometMidpointHuman);
	            break;
	        case RACE_AUTOMATON:
	            MainMenu::beginFade(MainMenu::FadeDestination::BaphometMidpointAutomaton);
	            break;
	        case RACE_GOATMAN:
	        case RACE_GOBLIN:
	        case RACE_INSECTOID:
			case RACE_DRYAD:
			case RACE_MYCONID:
			case RACE_SALAMANDER:
	            MainMenu::beginFade(MainMenu::FadeDestination::BaphometMidpointBeast);
	            break;
	        case RACE_SKELETON:
	        case RACE_VAMPIRE:
	        case RACE_SUCCUBUS:
	        case RACE_INCUBUS:
			case RACE_GREMLIN:
	            MainMenu::beginFade(MainMenu::FadeDestination::BaphometMidpointEvil);
	            break;
	        }
	    }

	    // force game to pause
        movie = true;
		pauseGame(2, false);
	}},

	{'PMAP', [](){
		MinimapPing newPing(ticks, net_packet->data[4],
			net_packet->data[5],
			net_packet->data[6],
			net_packet->data[8] ? true : false,
			static_cast<MinimapPing::PingType>(net_packet->data[7]));
		for ( int c = 0; c < barony::net::playerCapacity(); ++c )
		{
			if ( players[c]->isLocalPlayer() )
			{
				minimapPingAdd(newPing.player, c, newPing);
			}
		}
	}},

	// the server sent a game player preferences update
	{'GPPR', []() {
		GameplayPreferences_t::receivePacket();
	}},

	// the server requested a game player preferences update
	{'GPPU', []() {
		gameplayPreferences[clientnum].sendToServer();
	}},

	// the server sent a game config update
	{ 'GOPT', []() {
		GameplayPreferences_t::receiveGameConfig();
	} },

	{'DASH', [](){
		if ( players[clientnum] && players[clientnum]->entity && stats[clientnum] )
		{
			real_t vel = sqrt(pow(players[clientnum]->entity->vel_y, 2) + pow(players[clientnum]->entity->vel_x, 2));
			players[clientnum]->entity->monsterKnockbackVelocity = std::min(2.25, std::max(1.0, vel));
			players[clientnum]->entity->monsterKnockbackTangentDir = atan2(players[clientnum]->entity->vel_y, players[clientnum]->entity->vel_x);
			if ( vel < 0.01 )
			{
				players[clientnum]->entity->monsterKnockbackTangentDir = players[clientnum]->entity->yaw + PI;
			}
		}
	}},

	{ 'OVRC', []() {
		if ( players[clientnum] && players[clientnum]->entity && stats[clientnum] )
		{
			cast_animation[clientnum].overcharge_init = net_packet->data[4];
		}
	} },

	{ 'KINE', []() {
	if ( players[clientnum] && players[clientnum]->entity && stats[clientnum] )
	{
		real_t vel = sqrt(pow(players[clientnum]->entity->vel_y, 2) + pow(players[clientnum]->entity->vel_x, 2));
		players[clientnum]->entity->monsterKnockbackVelocity = std::min(2.25, std::max(1.0, vel));

		real_t dir = (SDLNet_Read32(&net_packet->data[4]) / 256.0);
		players[clientnum]->entity->monsterKnockbackTangentDir = dir;
	}
} },

	// get item
	{'ITEQ', [](){
		auto item = newItem(
		    static_cast<ItemType>(SDLNet_Read32(&net_packet->data[4])),
		    static_cast<Status>(SDLNet_Read32(&net_packet->data[8])),
		    SDLNet_Read32(&net_packet->data[12]),
		    SDLNet_Read32(&net_packet->data[16]),
		    SDLNet_Read32(&net_packet->data[20]),
		    net_packet->data[28],
		nullptr);
		item->ownerUid = SDLNet_Read32(&net_packet->data[24]);
		Item* pickedUp = itemPickup(clientnum, item);
		free(item);
		if ( players[clientnum] && players[clientnum]->entity && pickedUp )
		{
			bool oldIntro = intro;
			intro = true;
			useItem(pickedUp, clientnum);
			intro = oldIntro;
		}
	}},

	// update attributes from script
	{'SCRU', [](){
		if ( net_packet->data[25] )
		{
			bool clearStats = false;
			if ( net_packet->data[26] )
			{
				clearStats = true;
			}
			textSourceScript.playerClearInventory(clearStats);
		}
		stats[clientnum]->STR = (static_cast<Sint8>(net_packet->data[5]) <= -8) ? net_packet->data[5] : static_cast<Sint8>(net_packet->data[5]);
		stats[clientnum]->DEX = (static_cast<Sint8>(net_packet->data[6]) <= -8) ? net_packet->data[6] : static_cast<Sint8>(net_packet->data[6]);
		stats[clientnum]->CON = (static_cast<Sint8>(net_packet->data[7]) <= -8) ? net_packet->data[7] : static_cast<Sint8>(net_packet->data[7]);
		stats[clientnum]->INT = (static_cast<Sint8>(net_packet->data[8]) <= -8) ? net_packet->data[8] : static_cast<Sint8>(net_packet->data[8]);
		stats[clientnum]->PER = (static_cast<Sint8>(net_packet->data[9]) <= -8) ? net_packet->data[9] : static_cast<Sint8>(net_packet->data[9]);
		stats[clientnum]->CHR = (static_cast<Sint8>(net_packet->data[10]) <= -8) ? net_packet->data[10] : static_cast<Sint8>(net_packet->data[10]);
		stats[clientnum]->EXP = net_packet->data[11];
		stats[clientnum]->LVL = net_packet->data[12];
		stats[clientnum]->HP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[13]));
		stats[clientnum]->MAXHP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[15]));
		stats[clientnum]->MP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[17]));
		stats[clientnum]->MAXMP = static_cast<Sint16>(SDLNet_Read16(&net_packet->data[19]));
		stats[clientnum]->GOLD = static_cast<Sint32>(SDLNet_Read32(&net_packet->data[21]));
		for ( int i = 0; i < NUMPROFICIENCIES; ++i )
		{
			stats[clientnum]->setProficiency(i, static_cast<Sint8>(net_packet->data[27 + i]));
		}
	} },

	// update class from script
	{ 'SCRC', []() {
		int player = net_packet->data[4];
		int classnum = net_packet->data[5];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			client_classes[player] = classnum;
			bool oldIntro = intro;
			intro = true;
			initClass(player);
			intro = oldIntro;
		}
	}},

	// open fullscreen sign
	{'SIGN', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( Entity* sign = uidToEntity(uid) )
		{
			auto key = (char*)(&net_packet->data[8]);
			players[clientnum]->signGUI.openSign(key, uid);
		}
	}},

	// game restart
	{'RSTR', [](){
		svFlags = SDLNet_Read32(&net_packet->data[4]);
		uniqueGameKey = SDLNet_Read32(&net_packet->data[8]);
		local_rng.seedBytes(&uniqueGameKey, sizeof(uniqueGameKey));
		net_rng.seedBytes(&uniqueGameKey, sizeof(uniqueGameKey));
		uniqueLobbyKey = SDLNet_Read32(&net_packet->data[13]);
	    if (net_packet->data[12] == 0) {
		    loadingsavegame = 0;
			loadinglobbykey = 0;
	    }
		if ( gameModeManager.allowsSaves() )
		{
			deleteSaveGame(multiplayer);
		}
		printlog("Received order to restart game");
		MainMenu::beginFade(MainMenu::FadeDestination::GameStart);
		pauseGame(2, 0);
	}},

	// delete multiplayer save
	{'DSAV', [](){
		if ( multiplayer == CLIENT )
		{
			if ( gameModeManager.allowsSaves() )
			{
				deleteSaveGame(multiplayer);
			}
		}
	}},

	// post online hiscore
	{ 'DEND', []() {
		if ( multiplayer == CLIENT )
		{
#ifdef USE_PLAYFAB
			playfabUser.postScore(clientnum);
#endif
		}
	}},

	// text bubbles
	{'BUBL', []() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		auto type =
			static_cast<Player::WorldUI_t::WorldTooltipDialogue_t::DialogueType_t>(net_packet->data[8]);
		auto msg = (const char*)(&net_packet->data[9]);
		players[clientnum]->worldUI.worldTooltipDialogue.createDialogueTooltip(uid, type, msg);
		return;
	}},

	// shopkeeper player hostility
	{ 'SHPH', []() {
		auto wantedLevel = static_cast<ShopkeeperPlayerHostility_t::WantedLevel>(net_packet->data[4]);
		Uint16 numKills = SDLNet_Read16(&net_packet->data[5]);
		Uint16 numAggressions = SDLNet_Read16(&net_packet->data[7]);
		Uint16 numAccessories = SDLNet_Read16(&net_packet->data[9]);
		Uint32 type = SDLNet_Read32(&net_packet->data[11]);
		if ( auto hostility = ShopkeeperPlayerHostility.getPlayerHostility(clientnum, type) )
		{
			hostility->wantedLevel = wantedLevel;
			hostility->playerRace = static_cast<Monster>(type & 0xFF);
			hostility->sex = ((type >> 8) & 0x1) ? sex_t::MALE : sex_t::FEMALE;
			hostility->equipment = ((type >> 9) & 0x7F);
			hostility->numKills = static_cast<int>(numKills);
			hostility->numAggressions = static_cast<int>(numAggressions);
			hostility->numAccessories = static_cast<int>(numAccessories);
			hostility->player = clientnum;
		}
		return;
	} },

	{ 'BNTY', []() {
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			size_t numBounties = net_packet->data[5];
			int index = 6;
			achievementObserver.playerAchievements[player].bountyTargets.clear();
			while ( numBounties > 0 )
			{
				Uint32 uid = SDLNet_Read32(&net_packet->data[index]);
				achievementObserver.playerAchievements[player].bountyTargets.insert(uid);
				--numBounties;
				index += 4;
			}
		}
	}},

	{ 'BNTH', []() {
		int player = net_packet->data[4];
		if ( player >= 0 && barony::net::validPlayer(player) )
		{
			achievementObserver.playerAchievements[player].wearingBountyHat = net_packet->data[5] > 0 ? true : false;
		}
	} },

	// compendium reveal an entry
	{ 'CMPU', []() {
		int eventID = SDLNet_Read32(&net_packet->data[4]);
		if ( eventID >= Compendium_t::Events_t::kEventMonsterOffset && eventID < Compendium_t::Events_t::kEventMonsterOffset + 1000 )
		{
			auto find = Compendium_t::Events_t::monsterIDToString.find(eventID);
			if ( find != Compendium_t::Events_t::monsterIDToString.end() )
			{
				auto& unlockStatus = Compendium_t::CompendiumMonsters_t::unlocks[find->second];
				if ( unlockStatus == Compendium_t::CompendiumUnlockStatus::LOCKED_UNKNOWN )
				{
					unlockStatus = Compendium_t::CompendiumUnlockStatus::LOCKED_REVEALED_UNVISITED;
				}
			}
		}
	} },

	// compendium data update
	{ 'CMPD', []() {
		Uint8 clientSequence = net_packet->data[4];
		int sequence = net_packet->data[5];
		int numchunks = net_packet->data[6];
		if ( numchunks == 0 )
		{
			return;
		}
		char buf[512];
		stringCopy(buf, (char*)(&net_packet->data[7]), sizeof(buf), std::max(0, net_packet->len - 7));
		Compendium_t::Events_t::clientReceiveData[clientSequence][sequence] = buf;
		if ( static_cast<int>(Compendium_t::Events_t::clientReceiveData[clientSequence].size()) == numchunks )
		{
			std::string str = "";
			for ( int i = 1; i <= numchunks; ++i )
			{
				str += Compendium_t::Events_t::clientReceiveData[clientSequence][i];
			}

			rapidjson::Document d;
			d.Parse(str.c_str());
			if ( !d.HasParseError() )
			{
				if ( d.HasMember("seq") && d.HasMember("item") )
				{
					if ( d["seq"].GetInt() == clientSequence )
					{
						for ( auto itr = d["item"].MemberBegin(); itr != d["item"].MemberEnd(); ++itr )
						{
							int id = std::stoi(itr->name.GetString());
							if ( id >= 0 && id < Compendium_t::EventTags::CPDM_EVENT_TAGS_MAX )
							{
								for ( auto itr2 = itr->value.MemberBegin(); itr2 != itr->value.MemberEnd(); ++itr2 )
								{
									int itemType = std::stoi(itr2->name.GetString());
									Sint32 value = itr2->value.GetInt();
									if ( itemType >= Compendium_t::Events_t::kEventMonsterOffset && itemType < Compendium_t::Events_t::kEventMonsterOffset + 1000 )
									{
										Compendium_t::Events_t::eventUpdateMonster(0, static_cast<Compendium_t::EventTags>(id), nullptr, value, false, itemType);
										continue;
									}
									if ( itemType >= Compendium_t::Events_t::kEventWorldOffset && itemType < Compendium_t::Events_t::kEventWorldOffset + 1000 )
									{
										Compendium_t::Events_t::eventUpdateWorld(0, static_cast<Compendium_t::EventTags>(id), nullptr, value, false,
											itemType - Compendium_t::Events_t::kEventWorldOffset);
										continue;
									}
									if ( itemType >= Compendium_t::Events_t::kEventCodexOffset && itemType <= Compendium_t::Events_t::kEventCodexOffsetMax )
									{
										Compendium_t::Events_t::eventUpdateCodex(0, static_cast<Compendium_t::EventTags>(id), nullptr, value, false,
											itemType);
										continue;
									}
									if ( itemType < 0 || (itemType >= NUMITEMS && itemType < Compendium_t::Events_t::kEventSpellOffset) )
									{
										continue;
									}
									if ( itemType >= Compendium_t::Events_t::kEventSpellOffset )
									{
										Compendium_t::Events_t::eventUpdate(0, static_cast<Compendium_t::EventTags>(id), SPELL_ITEM, value, false,
											itemType - Compendium_t::Events_t::kEventSpellOffset);
									}
									else
									{
										Compendium_t::Events_t::eventUpdate(0, static_cast<Compendium_t::EventTags>(id), static_cast<ItemType>(itemType), value);
									}
								}
							}
						}
					}
				}
			}
			Compendium_t::Events_t::clientReceiveData.erase(clientSequence);

			Compendium_t::Events_t::writeItemsSaveData();
			Compendium_t::writeUnlocksSaveData();

			// reply got packet
			strcpy((char*)net_packet->data, "CMPD");
			net_packet->data[4] = clientnum;
			net_packet->data[5] = clientSequence;
			net_packet->address.host = net_server.host;
			net_packet->address.port = net_server.port;
			net_packet->len = 7;
			sendPacketSafe(net_sock, -1, net_packet, 0);
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
		if ( player == clientnum )
		{
			if ( gui.bOpen )
			{
				gui.addNotification(Language::get(6334), Language::get(6335), "", GenericGUIMenu::AssistShrineGUI_t::AssistNotification_t::NOTIF_CHARACTER_CHANGE_OK);
			}
			messagePlayer(clientnum, MESSAGE_WORLD, Language::get(6355), racename.c_str(), classname.c_str());
		}
		else
		{
			messagePlayer(clientnum, MESSAGE_WORLD, Language::get(6336), stats[player]->name, racename.c_str(), classname.c_str());
		}
	}
	}},

	{ 'ASSU', []() {
		// server sent player current assist values
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		Sint32 assistance = SDLNet_Read32(&net_packet->data[5]);
		if ( player == clientnum )
		{
			stats[player]->MISC_FLAGS[STAT_FLAG_ASSISTANCE_PLAYER_PTS]
				= std::max(assistance, stats[player]->MISC_FLAGS[STAT_FLAG_ASSISTANCE_PLAYER_PTS]);
		}
		else
		{
			stats[player]->MISC_FLAGS[STAT_FLAG_ASSISTANCE_PLAYER_PTS] = assistance;
		}
	} },

	{ 'ASSO', []() {
		// server order to open assist gui
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( auto entity = uidToEntity(uid) )
		{
			if ( entity->behavior == &::actAssistShrine )
			{
				GenericGUI[clientnum].openGUI(GUI_TYPE_ASSIST, entity);
			}
		}
	} },

	// server order to close assist shrine
	{ 'ASCL', []() {
		int player = net_packet->data[4];
		if ( player == clientnum )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* shrine = uidToEntity(uid) )
			{
				GenericGUI[clientnum].assistShrineGUI.closeAssistShrine();
			}
		}
	} },

	{ 'CAUO', []() {
		// server order to open cauldron gui
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( auto entity = uidToEntity(uid) )
		{
			if ( entity->behavior == &::actCauldron )
			{
				GenericGUI[clientnum].openGUI(GUI_TYPE_ALCHEMY, entity);
			}
		}
	} },

	// server order to close cauldron
	{ 'CAUC', []() {
		int player = net_packet->data[4];
		if ( player == clientnum )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* cauldron = uidToEntity(uid) )
			{
				GenericGUI[clientnum].alchemyGUI.closeAlchemyMenu();
			}
		}
	} },

	{ 'WRKO', []() {
		// server order to open workbench gui
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( auto entity = uidToEntity(uid) )
		{
			if ( entity->behavior == &::actWorkbench )
			{
				GenericGUI[clientnum].openGUI(GUI_TYPE_TINKERING, entity);
			}
		}
	} },

	// server order to close workbench
	{ 'WRKC', []() {
		int player = net_packet->data[4];
		if ( player == clientnum )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* cauldron = uidToEntity(uid) )
			{
				GenericGUI[clientnum].tinkerGUI.closeTinkerMenu();
			}
		}
	} },

	{ 'MBXO', []() {
		// server order to open mailbox gui
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( auto entity = uidToEntity(uid) )
		{
			if ( entity->behavior == &::actMailbox )
			{
				GenericGUI[clientnum].openGUI(GUI_TYPE_MAILBOX, entity);
			}
		}
	} },

	// server order to close mailbox
	{ 'MBXC', []() {
		int player = net_packet->data[4];
		if ( player == clientnum )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			if ( Entity* cauldron = uidToEntity(uid) )
			{
				GenericGUI[clientnum].mailboxGUI.closeMailMenu();
			}
		}
	} },

	// server order to consume key for lock
	{ 'LKEY', []() {
		const int player = net_packet->data[4];
		if (!barony::net::validPlayer(player))
		{
			return;
		}
		bool success = false;

		// reply got packet
		strcpy((char*)net_packet->data, "OKEY");
		net_packet->data[4] = clientnum;

		if ( player == clientnum )
		{
			Uint32 uid = SDLNet_Read32(&net_packet->data[5]);
			Entity* entity = uidToEntity(uid);
			if ( entity && entity->behavior == &actWallLock )
			{
				Item* key = players[clientnum]->inventoryUI.hasKeyForWallLock(*entity);
				if ( key )
				{
					SDLNet_Write16(static_cast<Uint16>(key->type), &net_packet->data[10]);
					consumeItem(key, clientnum);
					success = true;
				}
			}
		}

		net_packet->data[9] = success ? 1 : 0;
		net_packet->len = 12;
		net_packet->address.host = net_server.host;
		net_packet->address.port = net_server.port;
		sendPacketSafe(net_sock, -1, net_packet, 0);
	} },

	// server ensemble music update
	{ 'ENSM', []() {
		for ( int i = 4; i < net_packet->len; i += 4 )
		{
			Uint32 data = SDLNet_Read32(&net_packet->data[i]);
			int player = ((data & 0x7F) - 1);
			if ( player >= 0 && barony::net::validPlayer(player) )
			{
				players[player]->mechanics.ensembleDataUpdate = data;
			}
		}
	} },

	{ 'VOIP',[]() {
#ifdef USE_FMOD
		VoiceChat.receivePacket(net_packet);
#endif
	} },

	{ 'MAPT',[]() {
		int x = SDLNet_Read16(&net_packet->data[4]);
		int y = SDLNet_Read16(&net_packet->data[6]);
		Uint32 flagSet = SDLNet_Read32(&net_packet->data[8]);
		Uint32 flagRemove = SDLNet_Read32(&net_packet->data[12]);
		int layer = net_packet->data[16];
		if ( x >= 0 && x < map.width && y >= 0 && y < map.height && layer >= 0 && layer < MAP_LAYERS )
		{
			if ( flagSet )
			{
				if ( !map.tileHasAttribute(x, y, layer, flagSet) )
				{
					map.tileAttributes[layer + (y * MAP_LAYERS) + (x * MAP_LAYERS * map.height)] |= flagSet;
				}
			}
			if ( flagRemove )
			{
				if ( map.tileHasAttribute(x, y, layer, flagRemove) )
				{
					map.tileAttributes[layer + (y * MAP_LAYERS) + (x * MAP_LAYERS * map.height)] &= ~flagRemove;
				}
			}
		}
	}},

	// command spell
	{ 'COMD',[]() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		if ( Entity* target = uidToEntity(uid) )
		{
			if ( !target->clientsHaveItsStats )
			{
				target->giveClientStats();
			}
			FollowerMenu[clientnum].followerToCommand = target;
			FollowerMenu[clientnum].initfollowerMenuGUICursor(true); // set gui_mode to follower menu
		}
	} },

	{ 'FOCI',[]() {
		Uint32 uid = SDLNet_Read32(&net_packet->data[4]);
		real_t x = SDLNet_Read16(&net_packet->data[8]) / 32.0;
		real_t y = SDLNet_Read16(&net_packet->data[10]) / 32.0;
		real_t z = SDLNet_Read16(&net_packet->data[12]) / 32.0;
		real_t dir = SDLNet_Read16(&net_packet->data[14]) / 256.0;
		int sprite = SDLNet_Read16(&net_packet->data[16]);
		Uint32 seed = SDLNet_Read32(&net_packet->data[18]);
		real_t velocityBonus = SDLNet_Read16(&net_packet->data[22]) / 256.0;
		if ( Entity* gib = spawnFociGib(x, y, z, dir, velocityBonus, uid, sprite, seed) )
		{
			gib->setUID(uid);
		}
	} },

	{ 'SANM',[]() { // player spellcast animation
		int player = net_packet->data[4];
		int pose = net_packet->data[5];
		int charge = SDLNet_Read16(&net_packet->data[6]);
		spellcastAnimationUpdateReceive(player, pose, charge);
	} },

	// update breakable counter
	{ 'GBRK', []() {
		players[clientnum]->mechanics.gremlinBreakableCounter = net_packet->data[4];
	} },
};

void clientHandlePacket()
{
	if (handleSafePacket())
	{
		return;
	}

	Uint32 packetId = SDLNet_Read32(&net_packet->data[0]);

#ifdef PACKETINFO
	char packetinfo[NET_PACKET_SIZE];
	strncpy( packetinfo, (char*)net_packet->data, net_packet->len );
	packetinfo[net_packet->len] = 0;
	printlog("info: client packet: %s\n", packetinfo);
#endif
	if ( logCheckMainLoopTimers )
	{
		char packetinfo[NET_PACKET_SIZE];
		memcpy(packetinfo, net_packet->data, net_packet->len);
		packetinfo[net_packet->len] = '\0';

		char packetHeader[5];
		memcpy(packetHeader, packetinfo, 4);
		packetHeader[4] = '\0';

		std::string tmp = packetHeader;
		unsigned long hash = djb2Hash(packetHeader);
		auto find = DebugStats.networkPackets.find(hash);
		if ( find != DebugStats.networkPackets.end() )
		{
			++DebugStats.networkPackets[hash].second;
		}
		else
		{
			DebugStats.networkPackets.insert(std::make_pair(hash, std::make_pair(tmp, 0)));
			messagePlayer(clientnum, MESSAGE_DEBUG, "%s", tmp.c_str());
		}
		if ( packetId == 'ENTU' )
		{
			int sprite = 0;
			Uint32 uidpacket = SDLNet_Read32(&net_packet->data[4]);
			if ( uidToEntity(uidpacket) )
			{
				sprite = uidToEntity(uidpacket)->sprite;
				auto find = DebugStats.entityUpdatePackets.find(sprite);
				if ( find != DebugStats.entityUpdatePackets.end() )
				{
					++DebugStats.entityUpdatePackets[sprite];
				}
				else
				{
					DebugStats.entityUpdatePackets.insert(std::make_pair(sprite, 1));
				}
			}
		}
	}

    auto find = clientPacketHandlers.find(packetId);
    if (find == clientPacketHandlers.end()) {
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

	clientHandleMessages

	Parses messages received from the server

-------------------------------------------------------------------------------*/

void clientHandleMessages(Uint32 framerateBreakInterval)
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
            clientHandlePacket();

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
        clientHandlePacket();
    }
}

/*-------------------------------------------------------------------------------

	serverHandlePacket

	Called by serverHandleMessages. Does the actual handling of a packet.

-------------------------------------------------------------------------------*/

