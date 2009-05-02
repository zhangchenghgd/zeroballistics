#ifndef TANK_GAMELOGIC_CLIENT_TEAM_DEATHMATCH_DEFINED
#define TANK_GAMELOGIC_CLIENT_TEAM_DEATHMATCH_DEFINED

#include "GameLogicClientCommon.h"


#include "GameLogicServerTeamDeathmatch.h"


class GUITeamSelect;

//------------------------------------------------------------------------------
class GameLogicClientTeamDeathmatch : public GameLogicClientCommon
{    
 public:

    GameLogicClientTeamDeathmatch();
    
    virtual void init(PuppetMasterClient * master);

    virtual bool handleInput(PlayerInput & input);

    virtual void onRequestReady();
    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args);
    
    // -------------------- Common virtual functions --------------------
    virtual void onLevelObjectLoaded(const std::string & type, const bbm::ObjectInfo & info);
    virtual void onTeamAssignmentChanged(Player * player);
    virtual void onLocalControllableSet();
    virtual void onLocalPlayerKilled(Player * killer);

    virtual void handleMinimapIcon(RigidBody * object, bool force_reveal = false);
    
    static GameLogicClient * create();    
 protected:

    // -------------------- Custom command handlers --------------------
    void gameWon (RakNet::BitStream & args);

    Team team_[NUM_TEAMS_TDM];

    std::auto_ptr<GUITeamSelect> gui_teamselect_;
};


#endif
