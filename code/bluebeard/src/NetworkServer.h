
#ifndef TANK_NETWORKCLIENT_INCLUDED
#define TANK_NETWORKCLIENT_INCLUDED



#include "GameState.h"
#include "TimeStructs.h"
#include "RegisteredFpGroup.h"

#include "Observable.h"
#include "ServerInterface.h"


class NatPunchthroughServer;
class PuppetMasterServer;


//------------------------------------------------------------------------------
class NetworkServer : public Observable, public network::ServerInterface
{

 public:
    NetworkServer();
    virtual ~NetworkServer();



    void setAuthData(uint32_t id, uint32_t session_key);
    void logon();
    
    PuppetMasterServer * getPuppetMaster();

    void start();
    
 protected:

    
    virtual bool handlePacket(Packet * packet);
    virtual std::string getDetailedConnectionInfo(const SystemAddress & address) const;
    
    void handlePhysics      (float dt);
    void handleSendGameState(float dt);

    
    std::string printNetStatistics(const std::vector<std::string> & args);

    network::ACCEPT_VERSION_CALLBACK_RESULT acceptVersionCallback(const VersionInfo & version,
                                                                  VersionInfo & reported_version);

    void onAuthorizationSuccessful(Observable*, void*, unsigned);
    void onAuthorizationFailed(Observable*, void*, unsigned);
    
    
    std::auto_ptr<PuppetMasterServer> puppet_master_;
    // TODO CM fix NAT punchthrough code
//    std::auto_ptr<NatPunchthroughServer> nat_plugin_;

    RegisteredFpGroup fp_group_;
};





#endif // TANK_NETWORKCLIENT_INCLUDED
