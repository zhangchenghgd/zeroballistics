#ifndef TANK_GAMELOGIC_CLIENT_BEACON_STRIKE_DEFINED
#define TANK_GAMELOGIC_CLIENT_BEACON_STRIKE_DEFINED

#include "../GameLogicClientCommon.h"


#include "TeamBs.h"

class SpawnStageClient;

//------------------------------------------------------------------------------
class GameLogicClientBeaconstrike : public GameLogicClientCommon
{   
 public:

    GameLogicClientBeaconstrike();

    virtual void init(PuppetMasterClient * master);

    virtual bool handleInput(PlayerInput & input);

    virtual void loadLevel(const std::string & lvl_name);
    
    virtual void onRequestReady();
    
    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args);

    virtual void onGameObjectAdded(GameObject * object);


    // -------------------- Common virtual functions --------------------
    virtual void onLevelObjectLoaded(const std::string & type, const bbm::ObjectInfo & info);
    virtual void onTeamAssignmentChanged(Player * player);
    virtual void onLocalControllableSet();
    virtual void onKill(const SystemAddress & killer,
                        const SystemAddress & killed,
                        PLAYER_KILL_TYPE kill_type,
                        bool assist);

    virtual void handleMinimapIcon(RigidBody * object, bool force_reveal = false);
    virtual void onRepopulateMinimap();    

    virtual Matrix selectValidSpawnStage(int delta);
    
    static GameLogicClient * create();    

 protected:

    void assignToStages();
    
    // -------------------- Custom command handlers --------------------
    void pickupBeacon           (RakNet::BitStream & args);
    void dropBeacon             (RakNet::BitStream & args);
    void beaconExplosion        (RakNet::BitStream & args);
    void beaconConnections      (RakNet::BitStream & args);
    void stageConquered         (RakNet::BitStream & args);
    void gameWon                (RakNet::BitStream & args);


    void onBeaconDeployedChanged     (Observable* observable, unsigned event);
    void onBeaconScheduledForDeletion(Observable* observable, unsigned event);
    void onBeaconHealthChanged       (Observable* observable, unsigned event);

    void updateBoundary(float dt);

    

    
    std::vector<ClientTeam> team_;
    
    std::vector<SpawnStageClient> stage_;
    
    /// When loading the level, it is yet unclear which stage the
    /// camera positions belong to (they are assigned to the nearest
    /// beacon). Store them here intermediately.
    std::vector<Matrix> unassigned_camera_position_;
};


#endif
