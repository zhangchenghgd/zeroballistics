#ifndef TANK_GAMELOGIC_SERVER_SOCCER_DEFINED
#define TANK_GAMELOGIC_SERVER_SOCCER_DEFINED


#include "GameLogicServerCommon.h"
#include "RankingMatchEventsSoccer.h"
#include "TeamSoccer.h"

const std::string RB_SOCCERBALL = "soccerball";


//------------------------------------------------------------------------------
/**
 *  Used to distinguish CustomServerCmd types.
 */
enum CUSTOM_SERVER_COMMAND_TYPE_SOCCER
{
    CSCTS_GAME_WON = CSCT_LAST,
    CSCTS_GOAL
};

//------------------------------------------------------------------------------
/**
 *  Used to distinguish type of goal.
 */
enum GOAL_TYPE_SOCCER
{
    GTS_NORMAL_GOAL,
    GTS_OWN_GOAL,
    GTS_DEFLECTED_GOAL  ///< account assistant
};

class SoccerBall;

//------------------------------------------------------------------------------
class GameLogicServerSoccer : public GameLogicServerCommon
{
    friend class AIPlayerSoccer;

 public:
    GameLogicServerSoccer();


    virtual void init(PuppetMasterServer * master);    
    
    virtual void addPlayer(const Player * player, bool newly_connected);

    virtual void loadLevel(const std::string & name);
    
    virtual void onGameObjectAdded(GameObject * object);

    virtual void createMatchEventlog();    
    
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
    virtual void onInstantWeaponHit(InstantHitWeapon * gun,
                                    RigidBody * hit_object);

    virtual void onTimeLimitExpired();
    virtual void onPlayerTeamChanged(ServerPlayer * player);
    
    static GameLogicServer * create();    
 protected:

    virtual AIPlayer * createAIPlayer() const;

    virtual void doHandleLogic(float dt);

    virtual void onMatchStart();

    std::string restartMatch(const std::vector<std::string> & args); ///< make this virtual abstract in
                                                                     ///< GLSC so that each logic has to impl. it

    void healTanks(float dt);
    
    // -------------------- Custom command send functions --------------------
    void sendGameWon(TEAM_ID winner_team,
                     const SystemAddress & target_id,
                     COMMAND_SEND_TYPE type);
    void sendGoalCmd(TEAM_ID goal_scoring_team,
                     const SystemAddress & goalgetter,
                     const SystemAddress & assist,
                     unsigned type_of_goal,
                     const Vector & pos);
    
    bool onSoccerballSensorCollision(const physics::CollisionInfo & info);
    physics::CONTACT_GENERATION onSoccerballCollision      (const physics::CollisionInfo & info);
    physics::CONTACT_GENERATION onTankClawCollision        (const physics::CollisionInfo & info);

    void handleGoal(TEAM_ID goal_scoring_team);

    void sendBallAbsolute(void*);
    
    void spawnSoccerball(void* );
    void onSoccerBallDestroyed();

    void handleBall(float dt);

    void pushBallContact(const SystemAddress & address);
    
    network::ranking::MatchEventsSoccer * getMatchEvents();

    void clearAssistant(void*);

    float intervalsToPercentage(unsigned i0, unsigned i1) const;

    
    TEAM_ID winner_team_; ///< Only valid if state is TGS_WON

    TeamSoccer team_[NUM_TEAMS_SOCCER];

    Matrix spawn_pos_ball_;
	Vector goal_pos_[NUM_TEAMS_SOCCER];

    SoccerBall * soccer_ball_;
    hTask task_spawn_soccer_ball_;

    SystemAddress last_ball_contact_[2];

    hTask task_clear_assistant_;

    hTask task_send_absolute_;
};


#endif
