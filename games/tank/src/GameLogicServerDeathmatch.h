#ifndef TANK_GAMELOGIC_SERVER_DEATHMATCH_DEFINED
#define TANK_GAMELOGIC_SERVER_DEATHMATCH_DEFINED


#include "GameLogicServerCommon.h"

//------------------------------------------------------------------------------
/**
 *  Used to distinguish CustomServerCmd types.
 */
enum CUSTOM_SERVER_COMMAND_TYPE_DEATHMATCH
{
    CSCTDM_GAME_WON = CSCT_LAST
};


//------------------------------------------------------------------------------
class GameLogicServerDeathmatch : public GameLogicServerCommon
{
 public:
    GameLogicServerDeathmatch();


    virtual void init(PuppetMasterServer * master);    
    
    virtual void addPlayer(const Player * player, bool newly_connected);

    virtual void loadLevel(const std::string & name);
    

    virtual bool onLevelObjectLoaded(RigidBody * obj, const LocalParameters & params);
    


    // -------------------- Common virtual functions --------------------
    virtual Matrix getRespawnPos(const SystemAddress & pid, unsigned stage);
    virtual Tank * createNewPlayerTank(const SystemAddress & pid);

    virtual void onTankKilled(Tank * tank, const SystemAddress & killer_id);
    virtual void onProjectileHit(Projectile * projectile,
                                 RigidBody * hit_object,
                                 float hit_percentage,
                                 const physics::CollisionInfo & info);

    virtual void onTimeLimitExpired();
    virtual void onPlayerTeamChanged(Player * player);
    
    static GameLogicServer * create();    
 protected:
    
    // -------------------- Custom command send functions --------------------
    void sendGameWon(const SystemAddress & player,
                     const SystemAddress & target_id,
                     COMMAND_SEND_TYPE type);


    void handleLogic    (float dt);

    SystemAddress winner_; ///< Only valid if state is TGS_WON

    Team team_; ///< Only one dummy team to distinguish spectators from players
};


#endif
