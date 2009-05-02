#ifndef TANK_GAMELOGIC_SERVER_COMMON_DEFINED
#define TANK_GAMELOGIC_SERVER_COMMON_DEFINED


#include <set>

#include <raknet/RakNetTypes.h>

#include "GameLogicServer.h"
#include "Score.h"
#include "Matrix.h"
#include "Scheduler.h"
#include "PuppetMasterServer.h"
#include "ParameterManager.h"

#include "RankingStatistics.h"

#ifndef DEDICATED_SERVER
#include <osg/ref_ptr>
#endif


class RigidBody;
class PuppetMasterServer;
class Tank;
class Projectile;
class InstantHitWeapon;
class Controllable;
class Observable;
class SpawnPos;

const std::string VOID_GEOM_NAME = "void";

const float DEC_ANNOYING_CLIENT_REQUEST_PERIOD = 4.0f;
const float SEND_PLAYERS_LATENCY_FPS = 1.0f;

const std::string TANK_VOLUME_NAME = "s_tank_volume";
const std::string TANK_BODY_NAME   = "s_body";

// Only for standalone server with graphics
namespace terrain
{
    class TerrainVisual;
}

namespace physics
{
    class OdeRigidBody;
    class OdeGeom;
    class OdeCollisionSpace;
    struct CollisionInfo;
}

//------------------------------------------------------------------------------
/**
 *  Used to distinguish CustomServerCmd types.
 */
enum CUSTOM_SERVER_COMMAND_TYPE
{
    CSCT_WEAPON_HIT,
    CSCT_SCORE_UPDATE,
    CSCT_PLAYERS_LATENCY_UPDATE,
    CSCT_TEAM_ASSIGNMENT,
    CSCT_KILL,
    CSCT_EXECUTE_UPGRADE,
    CSCT_EQUIPMENT_UPDATE,
    CSCT_TEAM_MESSAGE,
    CSCT_STATUS_MESSAGE,
    CSCT_MATCH_STATISTICS,
    CSCT_LAST
};


//------------------------------------------------------------------------------
enum WEAPON_HIT_TYPE
{
    WHT_PROJECTILE,
    WHT_PROJECTILE_SPLASH,
    WHT_PROJECTILE_SPLASH_INDIRECT,
    WHT_MINE,
    WHT_MISSILE,
    WHT_MACHINE_GUN,   ///< these are not transmitted from sv to cl,
    WHT_FLAME_THROWER,  ///< but are used to trigger client side effects
    WHT_LASER,
    WHT_TRACTOR_BEAM
};

//------------------------------------------------------------------------------
enum OBJECT_HIT_TYPE
{
    OHT_OTHER,  ///< any object
    OHT_TANK,
    OHT_BEACON,
    OHT_WATER
};


//------------------------------------------------------------------------------
enum PLAYER_KILL_TYPE
{
    PKT_UNKNOWN1, ///< kill is used on beacon destr. currently!?
    PKT_SUICIDE, ///< happens on void plane, on team change, ...
    PKT_WEAPON1, ///< Splash
    PKT_WEAPON2, ///< Armor Piercing
    PKT_WEAPON3, ///< Heavy Impact
    PKT_WEAPON4, ///< Soccer Cannon
    PKT_MINE, 
    PKT_MISSILE, 
    PKT_MACHINE_GUN,
    PKT_FLAME_THROWER,
    PKT_LASER,
    PKT_COLL_DAMAGE,
    PKT_WRECK_EXPLOSION
};


//------------------------------------------------------------------------------
enum CUSTOM_CLIENT_COMMAND_TYPE
{
    CCCT_REQUEST_TEAM_CHANGE,
    CCCT_REQUEST_UPGRADE,
    CCCT_REQUEST_RESPAWN,
    CCCT_TEAM_MESSAGE,
    CCCT_REQUEST_EQUIPMENT_CHANGE
};

//------------------------------------------------------------------------------
enum TANK_GAME_STATE
{
    TGS_BEFORE_MATCH_START,
    TGS_IN_PROGRESS,
    TGS_WON
};

//------------------------------------------------------------------------------
enum CONTACT_CATEGORY_CONQUEROR_SERVER
{
    CCCS_STATIC,
    CCCS_DYNAMIC,
    CCCS_PROJECTILE,
    CCCS_HEIGHTFIELD,
    CCCS_WHEEL_RAY
};






//------------------------------------------------------------------------------
class GameLogicServerCommon : public GameLogicServer
{
    // Weapon system classes are considered an extension to the game
    // logic.
    friend class TankMine;

 public:
    GameLogicServerCommon();
    virtual ~GameLogicServerCommon();

    virtual void init(PuppetMasterServer * master);
    
    virtual void handleInput(ServerPlayer * player, const PlayerInput & input);

    virtual void addPlayer(const Player * player, bool newly_connected);
    virtual void removePlayer(const SystemAddress & pid);

    virtual void loadLevel(const std::string & name);

    virtual void onGameObjectAdded(GameObject * object);

    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args);    

    virtual void createMatchEventlog();

    virtual void onMatchStatsReceived(const network::ranking::Statistics & stats);



    // -------------------- Common virtual functions --------------------
    virtual bool onLevelObjectLoaded(RigidBody * obj, const LocalParameters & params);

    virtual Matrix getRespawnPos(const SystemAddress & pid, unsigned stage) = 0;
    virtual Tank * createNewPlayerTank(const SystemAddress & pid) = 0;
    
    virtual void onTankKilled(Tank * tank, const SystemAddress & killer_id) {}
    virtual void onProjectileHit(Projectile * projectile,
                                 RigidBody * hit_object,
                                 float hit_percentage,
                                 const physics::CollisionInfo & info,
                                 bool create_feedback);
    virtual void onInstantWeaponHit(InstantHitWeapon * gun,
                                    RigidBody * hit_object);
    virtual void onTimeLimitExpired() {}
    virtual void onPlayerTeamChanged(ServerPlayer * player);
    
    RigidBody * createRigidBody(const std::string & desc_file,
                                const std::string & type);

    const Score & getScore() const;

    virtual void recreateAIPlayers();
 protected:

    void handleLogic(float dt);
    virtual void doHandleLogic(float dt);

    virtual void onMatchStart();
    
    bool voidCollisionCallback        (const physics::CollisionInfo & info);
    bool tankBodyCollisionCallback    (const physics::CollisionInfo & info);


    void startTimeLimit(float limit);
    void updateTimeLimit(float dt);
    void decAnnoyingClientRequests(float dt);
    void sendPlayersLatency(float dt);    

    void kill(PLAYER_KILL_TYPE kill_type,
              Controllable * controllable, 
              SystemAddress killer_id = UNASSIGNED_SYSTEM_ADDRESS);
    

    void executeUpgrade(Controllable * controllable);
    void executeUpgrade(Controllable * controllable, UPGRADE_CATEGORY category, uint8_t level);
    void executeEquipmentChange(Controllable * controllable);


    Matrix getRandomSpawnPos(const std::vector<SpawnPos*> & possible_spawn_positions) const;
    void allowRespawn(void *);
    void respawn(const SystemAddress & pid, unsigned stage);
    
    void gameWon();




    void handleAssist(const SystemAddress & assistant,
                      const SystemAddress & killed,
                      float percentage,
                      unsigned score_points,
                      unsigned upgrade_points,
                      bool respect_teams);
                      
    

    // -------------------- Custom command send functions --------------------
    void sendWeaponHit        (const SystemAddress & shooter,                       
                               const SystemAddress & player_hit,
                               const physics::CollisionInfo & info, 
                               WEAPON_HIT_TYPE type,
                               OBJECT_HIT_TYPE object_hit);
    void sendScoreUpdate      (const SystemAddress & player_id,
                               const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);
    void sendTeamAssignment   (const SystemAddress & player_id,
                               const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);
    void sendKill             (const SystemAddress & killer_id, 
                               const SystemAddress & killed_id,
                               PLAYER_KILL_TYPE kill_type,
                               bool assist);
    void sendExecuteUpgrade   (const SystemAddress & upgrading_player_id,
                               UPGRADE_CATEGORY category, 
                               const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);
    void sendEquipment        (const SystemAddress & player_id,
                               const SystemAddress & target_id,
                               bool execute,
                               COMMAND_SEND_TYPE type);
    void sendStatusMessage    (const std::string & message,
                               const Color & color,
                               const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);
    void sendMatchStats       (const network::ranking::Statistics & stats);

    // -------------------- Custom command handlers --------------------
    void onTeamChangeRequested     (RakNet::BitStream & args);
    void onUpgradeRequested        (RakNet::BitStream & args);
    void onRespawnRequested        (RakNet::BitStream & args);
    void onEquipmentChangeRequested(RakNet::BitStream & args);
    virtual void onTeamMessage     (RakNet::BitStream & args);


    // -------------------- Console Functions --------------------
    std::string dumpSimulatorContents(const std::vector<std::string> & args);
    std::string dumpSpaceContents    (const std::vector<std::string> & args);
    std::string addPoints            (const std::vector<std::string> & args);
    std::string setTimeLimit         (const std::vector<std::string> & args);
    std::string startMatch           (const std::vector<std::string> & args);
    std::string addBot               (const std::vector<std::string> & args);
    std::string removeBot            (const std::vector<std::string> & args);

    void dealTankTankCollisionDamage(Tank * dealer, Tank * victim, float collision_speed,
                                     bool dealer_ram_active, bool victim_ram_active);
    bool isRamUpgradeActive(const Tank * t1, const RigidBody * other, const Vector & v_rel) const;
    
    virtual AIPlayer * createAIPlayer() const = 0;
        
    SpawnPos * extractSpawnPos(RigidBody * obj, const LocalParameters & params);
    
    Score score_;

    std::auto_ptr<physics::OdeCollisionSpace> sensor_space_;
    
    TANK_GAME_STATE state_;

    /// We need to keep track of which players are currently scheduled
    /// for a respawn. This determines how requested team changes are
    /// handled (non-respawning players are spawned immediately,
    /// respawning players only when their respawn is due).
    ///
    /// int stores selected base or -1 if respawn was not yet requested.
    std::map<SystemAddress, int> respawning_player_;


    std::vector<SpawnPos*> spawn_pos_; 
    
    hTask task_update_time_limit_;
    
#ifndef DEDICATED_SERVER
    osg::ref_ptr<terrain::TerrainVisual> terrain_visual_;
#endif
};



#endif
