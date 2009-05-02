#ifndef TANKGAME_AIPLAYER_INCLUDED
#define TANKGAME_AIPLAYER_INCLUDED

#include <raknet/RakNetTypes.h>

class PuppetMasterServer;
class ServerPlayer;

//------------------------------------------------------------------------------
class AIPlayer
{
 public:
    AIPlayer(PuppetMasterServer * puppet_master);
    virtual ~AIPlayer();

    virtual void addPlayer(unsigned num_ai_players);
    virtual void removePlayer();

    virtual void handleLogic(float dt);

 protected:
    
    SystemAddress pid_;
    ServerPlayer * sv_player_;

    PuppetMasterServer * puppet_master_;
};

#endif
