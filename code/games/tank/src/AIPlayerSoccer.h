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

enum AI_PLAYER_STATE_SOCCER
{
    APSS_IDLE,

	APSS_GO_NEAR_SOCCERBALL,
	APSS_GO_TO_OWN_SPAWNPOS,
	APSS_GO_TO_ENEMY_GOAL,
	APSS_DRAG_BALL_TOWARDS_TANK,

	APSS_RAM_ENEMY
};

//------------------------------------------------------------------------------
class AIPlayerSoccer : public AIPlayer
{
 public:
    AIPlayerSoccer(PuppetMasterServer * puppet_master);
    virtual ~AIPlayerSoccer();

    virtual void addPlayer(unsigned num_ai_players);
    virtual void removePlayer();

    virtual void handleLogic(float dt);

 protected:

	void handleIdle(float dt, PlayerInput & input, Tank * tank);

    void handleStuck(float dt, PlayerInput & input, Tank * tank);
    void handleWaypoints(float dt, PlayerInput & input, Tank * tank);
	void handleDragBall(float dt, PlayerInput & input, Tank * tank);
	void handleHitEnemy(float dt, PlayerInput & input, Tank * tank);
	void handleRamEnemy(float dt, PlayerInput & input, Tank * tank);

    void assignToTeam() const;
    
    void getNearestEnemy(Tank * tank);
    void onEnemyDestroyed();

    std::deque<WaypointServer*> ai_target_positions_;

    Controllable * enemy_;

    float stuck_dt_;
    float * projectile_inital_velocity_;
    float * gravity_;
    float * attack_range_sqr_;

    AI_PLAYER_STATE_SOCCER state_;

	Vector old_ball_pos_;  ///< used to track ball movement for route calc.

    RegisteredFpGroup fp_group_;
};

#endif
