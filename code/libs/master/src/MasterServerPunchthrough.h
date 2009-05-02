

#ifndef MASTER_SERVER_PUNCHTHROUGH_INCLUDED
#define MASTER_SERVER_PUNCHTHROUGH_INCLUDED


#include <raknet/NatPunchthrough.h>

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
class MasterServerPunchthrough : public NatPunchthrough
{
public:
    MasterServerPunchthrough();

    virtual void OnAttach(RakPeerInterface *peer);
    virtual PluginReceiveResult OnReceive(RakPeerInterface *peer, Packet *packet);

    virtual void OnShutdown(RakPeerInterface *peer);

    void connect(const SystemAddress & address, unsigned internal_port);
    
 protected:

    void connectWithPunchThrough();

    void onTokenReceived(Observable*, void * s, unsigned);
    void onMasterUnreachable();

    SystemAddress target_server_address_; ///< Stored until nat punchthrough is requested.
    SystemAddress facilitator_address_;

    RakPeerInterface * interface_;

    MasterServerRequest * request_;

    unsigned punchthrough_port_;
    
    RegisteredFpGroup fp_group_;
};

}
}

#endif
