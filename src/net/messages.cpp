#include "private/common.hpp"

int power(int a, int b)
{
    int result = 1;
    for ( int c = 0; c < b; c++ )
    {
        result *= a;
    }
    return result;
}

/*-------------------------------------------------------------------------------

messageLocalPlayers

Support function, messages all local players with the message "message"

-------------------------------------------------------------------------------*/

bool messageLocalPlayers(Uint32 type, char const * const message, ...)
{
    char str[Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH] = { 0 };

    va_list argptr;
    va_start(argptr, message);
    vsnprintf(str, Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH - 1, message, argptr);
    va_end(argptr);

    bool result = true;
    for ( int player = 0; player < barony::net::playerCapacity(); ++player )
    {
        if ( players[player]->isLocalPlayer() )
        {
            result = messagePlayerColor(player, type, 0xFFFFFFFF, str) ? result : false;
        }
    }

    return result;
}

/*-------------------------------------------------------------------------------

 messagePlayer

 Support function, messages the player number given by "player" with the
    message "message"

-------------------------------------------------------------------------------*/

bool messagePlayer(int player, Uint32 type, char const * const message, ...)
{
    if ( player < 0 || player >= barony::net::playerCapacity() )
    {
        return false;
    }
    char str[Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH] = { 0 };

    va_list argptr;
    va_start( argptr, message );
    vsnprintf( str, Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH - 1, message, argptr );
    va_end( argptr );

    strncpy(str, messageSanitizePercentSign(str, nullptr).c_str(), Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH - 1);
    str[Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH - 1] = '\0';

    return messagePlayerColor(player, type, 0xFFFFFFFF, str);
}

/*-------------------------------------------------------------------------------

messageLocalPlayersColor

Messages all local players with the message "message"
and color "color"

-------------------------------------------------------------------------------*/

bool messageLocalPlayersColor(Uint32 color, Uint32 type, char const * const message, ...)
{
    char str[Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH] = { 0 };

    va_list argptr;
    va_start(argptr, message);
    vsnprintf(str, Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH - 1, message, argptr);
    va_end(argptr);

    bool result = true;
    for ( int player = 0; player < barony::net::playerCapacity(); ++player )
    {
        if ( players[player]->isLocalPlayer() )
        {
            result = messagePlayerColor(player, type, color, str) ? result : false;
        }
    }

    return result;
}

/*-------------------------------------------------------------------------------

 messagePlayerColor

    Messages the player number given by "player" with the message "message"
    and color "color"

-------------------------------------------------------------------------------*/

bool messagePlayerColor(int player, Uint32 type, Uint32 color, char const * const message, ...)
{
    char str[Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH] = { 0 };
    va_list argptr;

    if ( message == nullptr)
    {
        return false;
    }
    if ( player < 0 || player >= barony::net::playerCapacity() )
    {
        return false;
    }

    // format the content
    va_start( argptr, message );
    vsnprintf( str, Player::MessageZone_t::ADD_MESSAGE_BUFFER_LENGTH - 1, message, argptr );
    va_end( argptr );

    // fixes crash when reading config at start of game
    if (!initialized)
    {
        printlog("%s\n", str);
        return true;
    }
    
    // don't bother printing any message if we're not in game, it just clutters the log
    if (intro) {
        return false;
    }

    // if this is for a local player, but we've disabled this message type, don't print it!
    const bool localPlayer = players[player]->isLocalPlayer();

    bool result = false;
    if ( localPlayer )
    {
        printlog("%s\n", str);
#ifdef NDEBUG
        if (type != MESSAGE_DEBUG) { 
            auto string = newString(&messages, color, completionTime, player, str);
            addMessageToLogWindow(player, string);
        }
#else
        auto string = newString(&messages, color, completionTime, player, str);
        addMessageToLogWindow(player, string);
#endif
        while ( list_Size(&messages) > MESSAGE_LIST_SIZE_CAP )
        {
            list_RemoveNode(messages.first);
        }
        if (!disable_messages && (messagesEnabled & type))
        {
            players[player]->messageZone.addMessage(color, str);
            result = true;
        }
    }
    else if ( multiplayer == SERVER )
    {
        strcpy((char*)net_packet->data, "MSGS");
        SDLNet_Write32(color, &net_packet->data[4]);
        SDLNet_Write32(type, &net_packet->data[8]);
        strcpy((char*)(&net_packet->data[12]), str);
        net_packet->address.host = net_clients[player - 1].host;
        net_packet->address.port = net_clients[player - 1].port;
        net_packet->len = 12 + strlen(str) + 1;
        sendPacketSafe(net_sock, -1, net_packet, player - 1);
    }

    // player death messages trigger this achievement
    char tempstr[256];
    for ( int c = 0; c < barony::net::playerCapacity(); c++ )
    {
        if ( client_disconnected[c] )
        {
            continue;
        }
        snprintf(tempstr, 256, Language::get(697), stats[c]->name);
        if ( !strcmp(str, tempstr) )
        {
            steamAchievementClient(player, "BARONY_ACH_NOT_A_TEAM_PLAYER");
        }
    }

    return result;
}

/*-------------------------------------------------------------------------------

 sendEntityUDP / sendEntityTCP

 Updates given entity data for given client. Server -> client functions

-------------------------------------------------------------------------------*/

