
#ifndef BLUEBEARD_MASTER_CLIENT_INCLUDED
#define BLUEBEARD_MASTER_CLIENT_INCLUDED


#include <raknet/BitStream.h>
#include <raknet/PluginInterface2.h>

#include "ServerInfo.h"

#include "Scheduler.h"



class RakPeerInterface;


namespace network
{

namespace master
{

    
//------------------------------------------------------------------------------
/**
 *  Registers the server with the master server and periodically sends
 *  heartbeat messages.
 */
class MasterServerRegistrator : public PluginInterface2
{
 public:
    MasterServerRegistrator();
    virtual ~MasterServerRegistrator();

    void sendServerInfo(const ServerInfo & info);


    virtual void OnDetach();

    virtual PluginReceiveResult OnReceive (Packet *packet);
    
 protected:

    void sendAuthTokenRequest(float dt);

    void sendHeartbeat(uint32_t token);

    ServerInfo cur_info_;

    RegisteredFpGroup fp_group_;

    SystemAddress master_server_address_;

    uint32_t last_token_; ///< used for remove server message. this is
                          ///the only case where we don't have to
                          ///request a token beforehand.
};


} // namespace master


}

#endif
