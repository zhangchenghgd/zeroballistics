

#ifndef MASTER_SERVER_PUNCHTHROUGH_INCLUDED
#define MASTER_SERVER_PUNCHTHROUGH_INCLUDED


#include <raknet/NatPunchthroughClient.h>

#include "Datatypes.h"
#include "RegisteredFpGroup.h"


class Observable;

namespace network
{

namespace master
{

class MasterServerRequest;
    

//------------------------------------------------------------------------------
/**
 *  Handles NAT punchthrough communication, commits suicide on success
 *  or when interface is shut down.
 *
 *  -) client tries to connect on its own. XXXX This is not done
 *  currently! starts with NAT PT at once...  test on server whether
 *  this is neccessary, keep track of whether NAT is neccessary on
 *  master server?  still do old ping-way for servers without
 *  neccessary NAT?
 *
 *  -) client sends nat punchthrough request to master server.
 *
 *  -) master tries to connect to target server, OR reuses existing
 *     connection (reschedules shutdown)
 *
 *  -) master server tries to connect to client
 *
 *  -) on incoming connection from master server, client connects to
 *     game server via nat punchthrough, closes connection afterwards
 *
 *  -) master disconnects from game server
 */
class MasterServerPunchthrough : public NatPunchthroughClient
{
public:
    MasterServerPunchthrough();

    virtual PluginReceiveResult OnReceive(Packet *packet);

    virtual void OnRakPeerShutdown();

    void connect(const RakNetGUID& guid, const SystemAddress & address, unsigned internal_port);
    
 protected:

    void connectWithPunchThrough();

    void onTokenReceived(Observable*, void * s, unsigned);
    void onMasterUnreachable();

    //! Callable as scheduled event: we want to initate openNat as soon as the facilitator has
    //! connected to us, but when handling the ID_NEW_INCOMING_CONNECTION, the connection is not
    //! yet complete.
    void openNat(void*);

    SystemAddress target_server_address_; ///< Stored until nat punchthrough is requested.
    //! Identifier of the server we want to connect to; we got this in the server list from the
    //! master server.
    RakNetGUID    target_guid_;
    SystemAddress facilitator_address_;

    MasterServerRequest * request_;

    unsigned punchthrough_port_;
    
    RegisteredFpGroup fp_group_;
};

}
}

#endif
