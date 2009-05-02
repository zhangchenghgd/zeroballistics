#ifndef TANK_GAMELOGIC_SERVER_BEACON_STRIKE_DEFINED
#define TANK_GAMELOGIC_SERVER_BEACON_STRIKE_DEFINED


#include "../GameLogicServerCommon.h"

#include "TeamBs.h"

class Beacon;
class BeaconBoundaryServer;
class SpawnStageServer;

//------------------------------------------------------------------------------
/**
 *  Used to distinguish CustomServerCmd types.
 */
enum CUSTOM_SERVER_COMMAND_TYPE_BEACON_STRIKE
{
    CSCTBS_PICKUP_BEACON = CSCT_LAST,
    CSCTBS_DROP_BEACON,
    CSCTBS_BEACON_DESTROYED,
    CSCTBS_BEACON_CONNECTIONS,
    CSCTBS_BASE_CONQUERED,
    CSCTBS_GAME_WON
};


//------------------------------------------------------------------------------
class GameLogicServerBeaconstrike : public GameLogicServerCommon
{
 public:
    GameLogicServerBeaconstrike();
    
    virtual void init(PuppetMasterServer * master);    

    virtual void reset();
    
    virtual void handleInput(ServerPlayer * player, const PlayerInput & input);

    virtual void addPlayer(const Player * player, bool newly_connected);
    virtual void removePlayer(const SystemAddress & pid);

    virtual void loadLevel(const std::string & name);

    virtual void onGameObjectAdded(GameObject * object);

    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args);    

    // -------------------- Common virtual functions --------------------
    virtual bool onLevelObjectLoaded(RigidBody * obj, const LocalParameters & params);

    virtual Matrix getRespawnPos(const SystemAddress & pid, unsigned stage);
    virtual Tank * createNewPlayerTank(const SystemAddress & pid);

    virtual void onTankKilled(Tank * tank, const SystemAddress & killer_id);
    virtual void onProjectileHit(Projectile * projectile,
                                 RigidBody * hit_object,
                                 float hit_percentage,
                                 const physics::CollisionInfo & info);
    virtual void onMachinegunHit(TankMachineGun * gun,
                                 RigidBody * hit_object);
    virtual void onTimeLimitExpired();
    virtual void onPlayerTeamChanged(Player * player);

    
    static GameLogicServer * create();    
    
    
 protected:
    void onBeaconAdded(Beacon * beacon);
    void onBeaconScheduledForDeletion(Observable * beacon,     unsigned event);
    void onBeaconDeployedChanged     (Observable * observable, unsigned event);
    void onBeaconInRadiusChanged     (Observable * observable, unsigned event);

    void handleLogic    (float dt);
    void fillBeaconQueue(float dt);


    void killBeacon(Beacon * beacon, const SystemAddress & killer);

    void pickupBeacon(Tank * tank);
    void dropBeacon(Tank * tank);

    void updateBoundary(bool broadcast_connections);


    // -------------------- Custom command send functions --------------------
    void sendPickupBeacon     (uint16_t tank_id, uint16_t beacon_id,
                               const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);
    void sendDropBeacon       (uint16_t tank_id);
    void sendBeaconDestroyed  (uint16_t beacon_id);
    void sendBeaconConnections(const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);
    void sendStageConquered   (uint8_t base,
                               const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);
    void sendGameWon          (TEAM_ID winner_team,
                               const SystemAddress & target_id,
                               COMMAND_SEND_TYPE type);


    
    
    ServerTeam team_[NUM_TEAMS_BS];
    
    std::auto_ptr<BeaconBoundaryServer> beacon_boundary_;

    /// Need pointer here because of registered collisioncallback...
    std::vector<SpawnStageServer*> spawn_stage_;

    TEAM_ID winner_team_; ///< Only valid if state_ is TGS_WON
    
};

#endif
