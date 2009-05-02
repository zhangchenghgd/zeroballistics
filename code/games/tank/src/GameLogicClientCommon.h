#ifndef TANK_GAMELOGIC_CLIENT_COMMON_DEFINED
#define TANK_GAMELOGIC_CLIENT_COMMON_DEFINED

#include "GameLogicClient.h"

#include <osg/ref_ptr>

#include "Scheduler.h"
#include "Score.h"
#include "RigidBody.h"
#include "PuppetMasterClient.h"
#include "Entity.h"
#include "RegisteredFpGroup.h"
#include "GameLogicServerCommon.h"
#include "SpectatorCamera.h"

#include "WaypointManagerClient.h"

class PlayerInput;
class Beacon;
class Observable;
class GameHudTank;
class GUIScore;
class GUIHelp;
class GUIUpgradeSystem;
class GUITankEquipment;
class GUITeamSelect;
class GUIMatchSummary;

namespace physics
{
    struct CollisionInfo;
}

namespace terrain
{
    class TerrainVisual;
}

namespace bbm
{
    class ObjectInfo;
}


class RigidBody;
class OsgNodeWrapper;

//------------------------------------------------------------------------------
enum INPUT_MODE
{
    IM_CONTROL_TANK,
    IM_CONTROL_CAMERA,
    IM_SELECT_SPAWN_STAGE
};


//------------------------------------------------------------------------------
enum CONTACT_CATEGORY_CONQUEROR_CLIENT
{
    CCCC_PROJECTILE = CCC_LAST,
    CCCC_HEIGHTFIELD,
    CCCC_WHEEL_RAY
};

//------------------------------------------------------------------------------
enum CUSTOM_TEAM_MESSAGE
{
    CTM_NEED_ASSISTANCE,
    CTM_ACKNOWLEDGED
};


//------------------------------------------------------------------------------
class GameLogicClientCommon : public GameLogicClient
{
 public:

    GameLogicClientCommon();
    virtual ~GameLogicClientCommon();

    virtual void init(PuppetMasterClient * master);

    virtual void addPlayer(const Player * player);
    virtual void removePlayer(const SystemAddress & pid);
    virtual void onPlayerNameChanged(Player * player);
    
    virtual bool handleInput(PlayerInput & input);
    virtual Matrix getCameraPosition();

    virtual void onControllableChanged(Player * player, Controllable * prev_controllable);
    virtual void onObjectReplaced(RigidBody * obj, const std::vector<RigidBody*> & parts);

    virtual void loadLevel(const std::string & lvl_name);
    virtual const std::string & getLevelName() const;
    
    virtual void onRequestReady() {}

    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args);

    virtual void onGameObjectAdded(GameObject * object);


    // -------------------- Common virtual functions --------------------
    virtual void createHud();
    virtual void createGuiScreens();
    virtual void onLevelObjectLoaded(const std::string & type, const bbm::ObjectInfo & info);
    virtual void onTeamAssignmentChanged(Player * player) {}
    virtual void onLocalControllableSet();
    virtual void onKill(const SystemAddress & killer,
                        const SystemAddress & killed,
                        PLAYER_KILL_TYPE kill_type,
                        bool assist);
    virtual void onLocalPlayerKilled(Player * killer);
    
    virtual void handleMinimapIcon(RigidBody * object, bool force_reveal = false) {}
    virtual void onRepopulateMinimap();

    virtual void onMatchStatsReceived(RakNet::BitStream & args);
    
    Score & getScore();

    void sendUpgradeRequest   (UPGRADE_CATEGORY category);
    void sendEquipmentChangeRequest(const std::vector<uint8_t> & equipment);
    void sendTeamChangeRequest(TEAM_ID team);
    void sendRespawnRequest   (unsigned base);

    const SystemAddress & getTrackedPlayer() const;

    void enableLocalView(bool e);

    void setInputMode(INPUT_MODE mode, bool console_msg = false);

    void revealOnMinimap(RigidBody * object);
    
 protected:

    void repopulateMinimap();

    void removeRevealed(void * obj);

    // -------------------- Input Handlers --------------------
    void toggleInputMode();

    // -------------------- Custom command handlers --------------------
    void weaponHit              (RakNet::BitStream & args);
    void updateScore            (RakNet::BitStream & args);
    void updatePlayersLatency   (RakNet::BitStream & args);
    void teamAssignment         (RakNet::BitStream & args);
    void kill                   (RakNet::BitStream & args);
    void setUpgradeLevel        (RakNet::BitStream & args);
    void setEquipment           (RakNet::BitStream & args);
    void teamMessage            (RakNet::BitStream & args);
    void statusMessage          (RakNet::BitStream & args);
    void matchStatistics        (RakNet::BitStream & args);

    bool tankCollisionCallback      (const physics::CollisionInfo & info);
    bool projectileCollisionCallback(const physics::CollisionInfo & info);
    bool bodyCollisionCallback      (const physics::CollisionInfo & info);

    void executeAllUpgrades(Controllable * controllable);
    void executeUpgradeStep(Controllable * controllable, uint8_t category, uint8_t level);
    void executeEquipmentChange(Controllable * controllable);



    // -------------------- Input Handlers --------------------
    void upgradeWeapon();
    void upgradeArmor();
    void upgradeSpeed();
    void sendTeamMessage1();
    void sendTeamMessage2();
    
    // -------------------- Console Functions --------------------
    std::string startWaypointing (const std::vector<std::string> & args);

    void sendTeamMessage(const CUSTOM_TEAM_MESSAGE & ctm);

    void updateTimeLimit(float dt);
    void decAnnoyingClientRequests(float dt);

    void updatePlayerLabel(Player * player);

    void removeFromMinimap(Observable* observable, unsigned event);

    void activateHitMarker(const SystemAddress & shooter,
                           const SystemAddress & player_hit,
                           Vector pos,
                           WEAPON_HIT_TYPE type);

    void activateHitFeedback(const SystemAddress & shooter,
                             const SystemAddress & player_hit,
                             WEAPON_HIT_TYPE type);

    void setupDeathCam(Controllable * killer,
                       Controllable * killed);

    void changeToSpawnSelectionCam(void*);

    void startRespawnCounter();
    void handleRespawnCounter(float dt);
    void blockRespawnInput();
    void unblockRespawnInput(void *);
    
    virtual Matrix selectValidSpawnStage(int delta) { return Matrix(true); }

    void gameWon();
    
    Score score_;
    
    std::auto_ptr<GameHudTank> hud_;

    std::auto_ptr<GUIScore>         gui_score_;
    std::auto_ptr<GUIHelp>          gui_help_;
    std::auto_ptr<GUIUpgradeSystem> gui_upgrade_system_;
    std::auto_ptr<GUITankEquipment> gui_tank_equipment_;
    std::auto_ptr<GUITeamSelect>    gui_teamselect_;
    std::auto_ptr<GUIMatchSummary>  gui_match_summary_;

    SpectatorCamera spectator_camera_;

    std::string lvl_name_;

    osg::ref_ptr<terrain::TerrainVisual> terrain_visual_;    
    osg::ref_ptr<OsgNodeWrapper>         sky_visual_;

    std::auto_ptr<WaypointManagerClient> waypoint_manager_;

    INPUT_MODE input_mode_;
    
    // -------------------- Respawn stuff --------------------
    bool respawn_request_sent_;
    hTask task_respawn_counter_;
    hTask task_change_to_spawn_selection_;
    unsigned respawn_time_;
    unsigned selected_spawn_stage_;
    bool show_respawn_text_;
    bool respawn_input_blocked_; ///< Block for some time after
                                   ///death to avoid accidential
                                   ///requests.


    bool game_won_;
    
    RegisteredFpGroup fp_group_;
    RegisteredFpGroup fp_group_remove_revealed_;
};


#endif
