#include "net/private/common.hpp"

char last_ip[64] = "";
char last_port[64] = "";
char lobbyChatbox[LOBBY_CHATBOX_LENGTH];
list_t lobbyChatboxMessages;
bool disableMultithreadedSteamNetworking = true;
bool disableFPSLimitOnNetworkMessages = true;
