#ifndef TANK_GAMELOGIC_SERVER_DEFINED
#define TANK_GAMELOGIC_SERVER_DEFINED


#include "Vector.h"
#include "RegisteredFpGroup.h"

namespace RakNet
{
    class BitStream;
}

class PuppetMasterServer;
class ServerPlayer;
class PlayerInput;
class GameObject;
struct SystemAddress;
class Player;



#define s_server_logic_loader Loki::SingletonHolder<dyn_class_loading::ClassLoader<GameLogicServer> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()




//------------------------------------------------------------------------------
class GameLogicServer
{

 public:

    GameLogicServer();
    virtual ~GameLogicServer();

    virtual void init(PuppetMasterServer * master);
    
    virtual void reset() {}

    virtual void addPlayer(const Player * player, bool newly_connected) {}
    virtual void removePlayer(const SystemAddress & pid) {}

    virtual void loadLevel(const std::string & name) {}

    virtual void onGameObjectAdded(GameObject * object) {}
    
    virtual void handleInput(ServerPlayer * player, const PlayerInput & input) {}
    
    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args) {}


    PuppetMasterServer * getPuppetMaster();
    
    static void setCreateVisuals(bool b);
    static bool isCreateVisualsEnabled();
    
    
 protected:

    PuppetMasterServer * puppet_master_;

    RegisteredFpGroup fp_group_;
    
    static bool create_visuals_;
};


#endif
