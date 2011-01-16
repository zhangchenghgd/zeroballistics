#ifndef TANK_GAMELOGIC_SERVER_TEAM_DEATHMATCH_DEFINED
#define TANK_GAMELOGIC_SERVER_TEAM_DEATHMATCH_DEFINED


#include "GameLogicServerCommon.h"

//------------------------------------------------------------------------------
/**
 *  Used to distinguish CustomServerCmd types.
 */
enum CUSTOM_SERVER_COMMAND_TYPE_TEAM_DEATHMATCH
{
    CSCTTDM_GAME_WON = CSCT_LAST
};

const unsigned NUM_TEAMS_TDM = 2;

const std::string TEAM_CONFIG_TDM[] = { "team_tdm_a", "team_tdm_b" };

//------------------------------------------------------------------------------
class GameLogicServerTeamDeathmatch : public GameLogicServerCommon
{
    friend class AIPlayerTeamDeathmatch;

 public:
    GameLogicServerTeamDeathmatch();


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
                                 const physics::CollisionInfo & info,
                                 bool create_feedback);

    virtual void onTimeLimitExpired();
    
    static GameLogicServer * create();    
 protected:

    virtual AIPlayer * createAIPlayer() const;

    // -------------------- Custom command send functions --------------------
    void sendGameWon(TEAM_ID winner_team,
                     const SystemAddress & target_id,
                     COMMAND_SEND_TYPE type);


    void handleLogic    (float dt);

    TEAM_ID winner_team_; ///< Only valid if state is TGS_WON

    Team team_[NUM_TEAMS_TDM];
};


#endif
