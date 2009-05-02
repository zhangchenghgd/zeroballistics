
#ifndef RANKING_CLIENT_INTERFACE_INCLUDED
#define RANKING_CLIENT_INTERFACE_INCLUDED


#include <string>


#include <raknet/RakPeerInterface.h>


#include "RegisteredFpGroup.h"
#include "Observable.h"
#include "VersionHandshakePlugin.h"

namespace network
{

const unsigned DEFAULT_SLEEP_TIMER = 10;
const float DEFAULT_NETWORK_DT = 0.1f;
    
//------------------------------------------------------------------------------
/**
 *  Handles connecting to a server, with version handshake and
 *  optional encryption.
 */
class ClientInterface : public Observable
{
 public:
    ClientInterface();
    virtual ~ClientInterface();

    void connect(const std::vector<std::string> & hosts,
                 const std::vector<unsigned> & ports,
                 unsigned num_retries,
                 AcceptVersionCallbackClient cb,
                 unsigned sleep_timer = DEFAULT_SLEEP_TIMER,
                 float handle_network_dt = DEFAULT_NETWORK_DT,
                 const char * public_key = NULL);

 protected:

    virtual bool handlePacket(Packet * packet) = 0;
    
    void handleNetwork(float dt);
    
    RakPeerInterface * interface_;
    RegisteredFpGroup fp_group_;
};

} // namespace network

#endif
