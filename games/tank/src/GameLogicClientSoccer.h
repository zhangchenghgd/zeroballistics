#ifndef TANK_GAMELOGIC_CLIENT_SOCCER_DEFINED
#define TANK_GAMELOGIC_CLIENT_SOCCER_DEFINED

#include "GameLogicClientCommon.h"


#include "GameLogicServerSoccer.h"

//------------------------------------------------------------------------------
class GameLogicClientSoccer : public GameLogicClientCommon
{    
 public:

    GameLogicClientSoccer();
    
    virtual void init(PuppetMasterClient * master);

    virtual bool handleInput(PlayerInput & input);
    
    virtual void onRequestReady();
    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args);

    virtual void onGameObjectAdded(GameObject * object);
    
    // -------------------- Common virtual functions --------------------
    virtual void createGuiScreens();
    virtual void createHud();
    virtual void onLevelObjectLoaded(const std::string & type, const bbm::ObjectInfo & info);
    virtual void onTeamAssignmentChanged(Player * player);
    virtual void onLocalControllableSet();
    virtual void onLocalPlayerKilled(Player * killer);

    virtual void handleMinimapIcon(RigidBody * object, bool force_reveal = false);
    virtual void onRepopulateMinimap();

    virtual void onMatchStatsReceived(RakNet::BitStream & args);
    
    static GameLogicClient * create();    
 protected:

    physics::CONTACT_GENERATION onTankClawCollision (const physics::CollisionInfo & info);
    physics::CONTACT_GENERATION onSoccerballCollision(const physics::CollisionInfo & info);
    
    // -------------------- Custom command handlers --------------------
    void gameWon (RakNet::BitStream & args);
    void onGoalScored(RakNet::BitStream & args);
    
    Team team_[NUM_TEAMS_SOCCER];
};


#endif
