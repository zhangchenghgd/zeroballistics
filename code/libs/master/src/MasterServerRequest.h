
#ifndef MASTER_SERVER_REQUEST_DEFINED
#define MASTER_SERVER_REQUEST_DEFINED


#include <raknet/PluginInterface.h>

#include "RegisteredFpGroup.h"
#include "Observable.h"

namespace network
{
namespace master
{

//------------------------------------------------------------------------------
enum MASTER_SERVER_REQUEST_EVENT
{
    MSRE_RECEIVED_RESPONSE,
    MSRE_CONTACT_FAILED
};


//------------------------------------------------------------------------------
/**
 *  Sends a specified packet as "advertise system" message to the
 *  master server, and waits for a response packet with a specified
 *  ID. This is done a specified number of times, after success or
 *  failure an event is triggered.
 *
 *  This request deletes itself after success or failure, or when the
 *  used interface is shut down.
 */
class MasterServerRequest : public Observable, public PluginInterface
{
 public:
    MasterServerRequest(RakPeerInterface * rak_peer_interface,
                        const char * data, unsigned num_bytes,
                        uint8_t response_id);
    ~MasterServerRequest();

    virtual PluginReceiveResult OnReceive(RakPeerInterface *peer, Packet *packet);
    virtual void OnShutdown(RakPeerInterface *peer);

    const SystemAddress & getServerAddress() const;
    
 protected:

    void send(float dt);

    
    unsigned num_remaining_retries_;

    uint8_t response_id_; ///< The server response we are waiting for.
    
    std::vector<char*> data_; ///< The packet to send to the server.

    SystemAddress server_address_;
    RakPeerInterface * interface_;
    
    RegisteredFpGroup fp_group_;
};

}

}

#endif
