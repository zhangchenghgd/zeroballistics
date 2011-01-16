#ifndef TANKGAME_AIPLAYERTEAMDEATHMATCH_INCLUDED
#define TANKGAME_AIPLAYERTEAMDEATHMATCH_INCLUDED

#include <deque>

#include <raknet/RakNetTypes.h>

#include "AIPlayerDeathmatch.h"
#include "Vector.h"
#include "PlayerInput.h"
#include "RegisteredFpGroup.h"

#include "WaypointManagerServer.h"

class PuppetMasterServer;
class ServerPlayer;
class Tank;
class Controllable;

//------------------------------------------------------------------------------
class AIPlayerTeamDeathmatch : public AIPlayerDeathmatch
{
 public:
    AIPlayerTeamDeathmatch(PuppetMasterServer * puppet_master);
    virtual ~AIPlayerTeamDeathmatch();

 protected:

    void assignToTeam() const;
    
    void getNearestEnemy(Tank * tank);
};

#endif
