

#ifndef NETWORK_SERVER_INTERFACE_INCLUDED
#define NETWORK_SERVER_INTERFACE_INCLUDED


#include <string>
#include <map>

#include "VersionHandshakePlugin.h"
#include "TimeStructs.h"

class Packet;


namespace network
{


//------------------------------------------------------------------------------
/**
 *  Additional info for every connection this server has.
 */
class ConnectionInfo
{
 public:
    VersionInfo version_;
    TimeValue connect_time_; ///< timestamp when this connection was established.
};
    
//------------------------------------------------------------------------------
/**
 *  Provides a framework for network servers:
 *
 *  -) Network task
 *  -) Default logging for unhandled packets
 *  -) Default console commands
 *  -) Version handshake plugin
 */
class ServerInterface
{
 public:
    ServerInterface(AcceptVersionCallbackServer cb);
    virtual ~ServerInterface();

    
    void handleNetwork(float dt);

 protected:

    void start(const std::string & name,
               unsigned port,
               unsigned num_connections,
               unsigned sleep_timer,
               float connection_timeout,
               unsigned mtu_size,
               const char * private_key);
    
    virtual bool handlePacket(Packet * packet) = 0;
    virtual std::string getDetailedConnectionInfo(const SystemAddress & address) const;

    std::string printConnections(const std::vector<std::string>&args);
    std::string closeConnection (const std::vector<std::string>&args);
    
    std::auto_ptr<network::VersionHandshakePlugin> version_handshake_plugin_;
    
    RakPeerInterface * interface_;
    RegisteredFpGroup fp_group_;

    std::map<SystemAddress, ConnectionInfo> info_;
};


} 
 
#endif
