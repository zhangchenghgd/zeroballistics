

#ifndef MASTER_SERVER_LIST_INCLUDED
#define MASTER_SERVER_LIST_INCLUDED

// interface enumeration stuff
#ifdef _WIN32
#endif 

#include <raknet/RakPeerInterface.h>


#include "RegisteredFpGroup.h"
#include "Observable.h"
#include "Scheduler.h"
#include "Datatypes.h"
#include "ServerInfo.h"

namespace network
{
namespace master
{



//------------------------------------------------------------------------------
enum SERVER_LIST_EVENT
{
    SLE_FOUND_SERVER,
    SLE_SERVER_MESSAGE,
    SLE_MASTER_SERVER_UNREACHABLE
    // number of servers from master server etc will go here...
};



//------------------------------------------------------------------------------ 
class ListServerInfo : public ServerInfo
{
 public:
    ListServerInfo() : ping_(-1) {}
    int ping_;
};

//------------------------------------------------------------------------------
/**
 *  Finds & reports servers. Currently the only way of finding a
 *  server is by a LAN broadcast, as no master server implementation
 *  exists.
 *
 *  Runs until the first server answers to a LAN broadcast ping, or
 *  until stopped. Found servers are reported via observable event.
 */
class ServerList : public Observable
{
public:
    ServerList();
    virtual ~ServerList();

    void queryMasterServer();
    void queryLan(unsigned port);

    void cancelQueries();

    const std::vector<ListServerInfo> & getInfoList() const;
    
    void reset();
protected:

    void handleNetwork(float dt);    
    void handleLanPing(float dt);

    void handleLanServerPong(const SystemAddress & server_address,
                             RakNet::BitStream & pong);

    void sendRequestServerList(uint32_t token);

    void processServerBatch(RakNet::BitStream & stream);

    void onTokenReceived      (Observable*, void * s, unsigned);
    void onMasterContactFailed(Observable*, void * s, unsigned);

    std::vector<ListServerInfo> server_info_;
    
    RakPeerInterface * interface_;
    RegisteredFpGroup fp_group_;


    hTask task_ping_lan_;
    unsigned lan_scan_port_;

    bool query_active_;
};


} // namespace master

} // namespace network


#endif
