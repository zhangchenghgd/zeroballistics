#ifndef TANK_GAMELOGIC_CLIENT_DEFINED
#define TANK_GAMELOGIC_CLIENT_DEFINED

#include <string>

#include "Matrix.h"
#include "RegisteredFpGroup.h"

class PuppetMasterClient;
class PlayerInput;
class Controllable;
class Player;
class RigidBody;


namespace RakNet
{
    class BitStream;
}

struct SystemAddress;
class GameObject;

#define s_client_logic_loader Loki::SingletonHolder<dyn_class_loading::ClassLoader<GameLogicClient> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()


//------------------------------------------------------------------------------
class GameLogicClient
{

 public:

    GameLogicClient();
    virtual ~GameLogicClient();

    virtual void init(PuppetMasterClient * master);
    
    virtual void reset() {}


    virtual void addPlayer(const Player * player) {}
    virtual void removePlayer(const SystemAddress & pid) {}
    virtual void onPlayerNameChanged(Player * player) {}
    
    virtual bool handleInput(PlayerInput & input) = 0;
    virtual Matrix getCameraPosition()            = 0;

    virtual void onControllableChanged(Player * player, Controllable * prev_controllable) {}

    virtual void loadLevel(const std::string & lvl_name) {}
    virtual const std::string & getLevelName() const = 0;
    
    virtual void onGameObjectAdded(GameObject * object) {}

    /// Called after loading a level to allow last preparations
    virtual void onRequestReady() {}
    
    virtual void executeCustomCommand(unsigned type, RakNet::BitStream & args) {}

    PuppetMasterClient * getPuppetMaster();
    
 protected:

    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};

#endif
