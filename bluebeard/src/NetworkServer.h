
#ifndef TANK_NETWORKCLIENT_INCLUDED
#define TANK_NETWORKCLIENT_INCLUDED



#include "GameState.h"
#include "TimeStructs.h"
#include "RegisteredFpGroup.h"

#include "Observable.h"


class NatPunchthrough;
class RakPeerInterface;
class PuppetMasterServer;


//------------------------------------------------------------------------------
class NetworkServer : public Observable
{

 public:
    NetworkServer();
    virtual ~NetworkServer();

    void start();
    
    PuppetMasterServer * getPuppetMaster();
    
 protected:

    void handlePhysics      (float dt);
    void handleSendGameState(float dt);
    void handleNetwork      (float dt);
    
    std::string printNetStatistics(const std::vector<std::string> & args);


    std::auto_ptr<PuppetMasterServer> puppet_master_;

    RakPeerInterface * interface_;
    RegisteredFpGroup fp_group_;

    std::auto_ptr<NatPunchthrough> nat_plugin_;
};





#endif // TANK_NETWORKCLIENT_INCLUDED
