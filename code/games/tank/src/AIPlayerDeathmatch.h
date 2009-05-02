#ifndef TANKGAME_AIPLAYERDEATHMATCH_INCLUDED
#define TANKGAME_AIPLAYERDEATHMATCH_INCLUDED

#include <deque>

#include <raknet/RakNetTypes.h>

#include "AIPlayer.h"
#include "Vector.h"
#include "PlayerInput.h"
#include "RegisteredFpGroup.h"

#include "WaypointManagerServer.h"

class PuppetMasterServer;
class ServerPlayer;
class Tank;
class Controllable;

enum AI_PLAYER_STATE
{
    APS_IDLE,
    APS_DRIVING,
    APS_FIRING,
    APS_HEALING
};

//------------------------------------------------------------------------------
class AIPlayerDeathmatch : public AIPlayer
{
 public:
    AIPlayerDeathmatch(PuppetMasterServer * puppet_master);
    virtual ~AIPlayerDeathmatch();

    virtual void addPlayer(unsigned num_ai_players);
    virtual void removePlayer();

    virtual void handleLogic(float dt);

 protected:

    void handleStuck(float dt, PlayerInput & input, Tank * tank);
    void handleFire(float dt, PlayerInput & input, Tank * tank);
    void handleMineLaying(float dt, PlayerInput & input, Tank * tank, float target_yaw);
    void handleWaypoints(float dt, PlayerInput & input, Tank * tank);
    void handleHeal(float dt, PlayerInput & input, Tank * tank);

    void assignToTeam() const;
    
    void getNearestEnemy(Tank * tank);
    void onEnemyDestroyed();

    std::deque<WaypointServer*> ai_target_positions_;

    Controllable * enemy_;

    float stuck_dt_;
    float * projectile_inital_velocity_;
    float * gravity_;
    float * attack_range_sqr_;

    AI_PLAYER_STATE state_;

    RegisteredFpGroup fp_group_;
};

#endif
