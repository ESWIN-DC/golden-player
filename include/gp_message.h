#ifndef __GP_MESSAGE_H__
#define __GP_MESSAGE_H__

#include <cstdint>

#include "gp_network.h"

enum class PlayerMsg : uint32_t {
    Server_GetStatus,
    Server_GetPing,

    Client_Accepted,
    Client_AssignID,
    Client_RegisterWithServer,
    Client_UnregisterWithServer,

    Game_AddPlayer,
    Game_RemovePlayer,
    Game_UpdatePlayer,
};

struct sPlayerDescription {
};

#endif  // __GP_MESSAGE_H__