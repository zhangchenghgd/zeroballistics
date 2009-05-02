
#ifndef TOOLS_MASTER_SERVER_INCLUDED
#define TOOLS_MASTER_SERVER_INCLUDED



#include <map>

#include <raknet/RakPeerInterface.h>
#include <raknet/RakNetDefines.h>

#include "ServerInfo.h"
#include "RegisteredFpGroup.h"
#include "Scheduler.h"


class RakPeerInterface;
class NatPunchthrough;
class VersionInfo;


#ifdef __BITSTREAM_NATIVE_END
#error __BITSTREAM_NATIVE_END defined!
#endif


namespace network
{

namespace master
{

class ServerInfo;


//------------------------------------------------------------------------------ 
class NatRequest
{
 public:
    NatRequest(const SystemAddress & server, const SystemAddress & client) :
        server_(server), client_(client) {}

    bool operator==(const NatRequest & other)
        {
            return (server_ == other.server_ &&
                    client_ == other.client_);
        }
    
    SystemAddress server_;
    SystemAddress client_;
};
 
//------------------------------------------------------------------------------
/**
 *  Master server debug classes:
 *
 *  T - token stuff
 *  P - nat punchthrough stuff
 *  H - server heartbeats
 *  L - client serverlist requests
 *  N - network (unhandled packets etc)
 */
class MasterServer
{

 public:
    MasterServer();
    virtual ~MasterServer();

    void start();
    
 
    //-------------------------------------------------------------------
    /**
     *  Everybody who wants to talk to the master server first needs
     *  to answer a challenge to avoid IP spoofing. Upon request, the
     *  master server sends an authentication token, which the other
     *  system has to return. It then is authenticated for the next
     *  query.
     */
    struct Contact
    {
        Contact(const SystemAddress & ad, uint32_t token) :
            address_(ad),
        auth_token_(token) {}
        
        SystemAddress address_;
        uint32_t auth_token_;

        bool operator==(const SystemAddress & ad) { return ad == address_; }
    };

    //-------------------------------------------------------------------
    /**
     *  An entry in our server list.
     *
     *  Servers are stored with their external IP but their internal
     *  port. This is because the external port can be different for
     *  every heartbeat, but the internal port stays fixed.
     */
    struct GameServer
    {
        GameServer() : task_delete_(INVALID_TASK_HANDLE) {}
        
        /// Servers which do not send heartbeat messages are deleted
        /// after a certain timeout.
        hTask task_delete_;
        ServerInfo info_;
        
        SystemAddress external_address_; ///< Server address we got the packet from.

        uint32_t last_token_;
    };

protected:


    void handleNetwork(float dt);

    
    void autoRemove  (GameServer * server);
    void removeServer(void * address);
    void updateServer(const SystemAddress & address,
                      RakNet::BitStream & args,
                      uint32_t token);


    
    uint32_t createAuthToken() const;
    void sendAuthToken (const SystemAddress & address);
    bool checkAuthToken(const Packet * packet);
    void removeContact(void * address);


    
    void onDisconnect(const SystemAddress & address);
    void onConnectionRequestAccepted(const SystemAddress & address);

    void onNatPunchthroughRequested(const SystemAddress & address,
                                    RakNet::BitStream & stream);
    void disconnectIfNoPendingPunches(const SystemAddress & server);


    void closeConnection(void * a);

    void sendServerList(const SystemAddress & address,
                        const VersionInfo & client_version) const;
    


    // -------------------- Test functions --------------------
    std::string printConnections (const std::vector<std::string>&args);
    std::string printServers     (const std::vector<std::string>&args);
    std::string generateTestData (const std::vector<std::string>&args);


    void onNewPlayer();
    void onNewServer();

    std::vector<Contact>    contact_;
    std::vector<GameServer> game_server_;

    
    RakPeerInterface * interface_;
    RegisteredFpGroup fp_group_;


    std::vector<NatRequest> nat_punchthrough_request_;
    std::map<SystemAddress, hTask> task_disconnect_;

    std::auto_ptr<NatPunchthrough> nat_plugin_;
};


}

}

#endif // TOOLS_MASTER_SERVER_INCLUDED
