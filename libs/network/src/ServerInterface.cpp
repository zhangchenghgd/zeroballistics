

#include "ServerInterface.h"



#include <raknet/RakNetworkFactory.h>
#include <raknet/RakPeerInterface.h>


#include "NetworkUtils.h"
#include "Log.h"
#include "RakAutoPacket.h"
#include "MessageIds.h"



namespace network
{

//------------------------------------------------------------------------------
ServerInterface::ServerInterface(AcceptVersionCallbackServer cb) :
    version_handshake_plugin_(new network::VersionHandshakePlugin(cb)),
    interface_(RakNetworkFactory::GetRakPeerInterface())
{    
    interface_->AttachPlugin(version_handshake_plugin_.get());

    s_console.addFunction("printConnections",
                          ConsoleFun(this, &ServerInterface::printConnections),
                          &fp_group_);
    s_console.addFunction("closeConnection",
                          ConsoleFun(this, &ServerInterface::closeConnection),
                          &fp_group_);
}



//------------------------------------------------------------------------------
ServerInterface::~ServerInterface()
{
    interface_->Shutdown(0);
    RakNetworkFactory::DestroyRakPeerInterface(interface_);
}



//------------------------------------------------------------------------------
void ServerInterface::handleNetwork(float dt)
{
    RakAutoPacket packet(interface_);
    while (packet.receive())
    {
        RakNet::BitStream stream(&packet->data[1], packet->length, false);

        // store additional info about connection
        if (packet->data[0] == VHPI_VERSION_INFO)
        {
            info_[packet->systemAddress].version_.readFromBitstream(stream);
            getCurTime(info_[packet->systemAddress].connect_time_);
        } else if (packet->data[0] == ID_DISCONNECTION_NOTIFICATION ||
                   packet->data[0] == ID_CONNECTION_LOST)
        {
            std::map<SystemAddress, ConnectionInfo>::iterator it =  info_.find(packet->systemAddress);
            if (it == info_.end())
            {
                s_log << Log::warning
                      << "No connection info for "
                      << packet->systemAddress
                      << ", who just disconnected.\n";
            } else
            {
                info_.erase(it);
            }
        }
        
        if (!handlePacket(packet))
        {
            defaultPacketAction(packet, interface_);
        }
    }    
}


//------------------------------------------------------------------------------
void ServerInterface::start(const std::string & name,
                            unsigned port,
                            unsigned num_connections,
                            unsigned sleep_timer,
                            float connection_timeout,
                            unsigned mtu_size,
                            const char * private_key)
{
    assert(interface_ && !interface_->IsActive());
    
    interface_->SetMTUSize(mtu_size);
    interface_->SetMaximumIncomingConnections(num_connections);
    if (private_key) initializeSecurity(interface_, private_key, false);
    
    SocketDescriptor desc(port, 0);
    bool success = interface_->Startup(num_connections,
                                       sleep_timer,
                                       &desc, 1);
    if (success)
    {
        s_log << name
              << " started, listening on port "
              << port
              << " for "
              << num_connections
              << " connections max.\n";
    } else
    {
        Exception e;
        e << "Failed to start "
          << name
          << " on port "
          << port << ".";
        throw e;
    }
    

    s_scheduler.addFrameTask(PeriodicTaskCallback(this, &ServerInterface::handleNetwork),
                             "ServerInterface::handleNetwork",
                             &fp_group_);
}
    




//------------------------------------------------------------------------------
/**
 *  Should be overridden by servers who know more about the connection
 *  than just IP+port+version+connection time.
 */
std::string ServerInterface::getDetailedConnectionInfo(const SystemAddress & address) const
{
    std::ostringstream ret;
    ret << address;

    std::map<SystemAddress, ConnectionInfo>::const_iterator it =  info_.find(address);
    if (it == info_.end())
    {
        s_log << Log::warning
              << "missing connection info for "
              << address
              << "\n";
    } else
    {
        TimeValue cur_time;
        getCurTime(cur_time);
        
        ret << " : version "
            << it->second.version_
            << ", connected for "
            << getTimeDiff(cur_time, it->second.connect_time_)/1000.0f
            << " seconds.";
    }

    return ret.str();
}

//------------------------------------------------------------------------------
std::string ServerInterface::printConnections(const std::vector<std::string>&args)
{
    unsigned short num_connections;
    interface_->GetConnectionList(NULL, &num_connections);
    std::vector<SystemAddress> connection(num_connections);
    interface_->GetConnectionList(&connection[0], &num_connections);
    
    std::string ret;
    ret = "Number of connections: " + toString(num_connections) + "\n";
    for (unsigned i=0; i<num_connections; ++i)
    {
        ret += getDetailedConnectionInfo(connection[i]) + "\n";
    }

    return ret;
}



//------------------------------------------------------------------------------
std::string ServerInterface::closeConnection(const std::vector<std::string>&args)
{
    if (args.size() != 2) return "Usage: closeConnection IP port (use printConnections)";

    SystemAddress address;
    address.SetBinaryAddress(args[0].c_str());
    address.port = fromString<unsigned short>(args[1]);

    if (!interface_->IsConnected(address))
    {
        return args[0] + ":" + args[1] + " is not connected.\n";
    }
    
    interface_->CloseConnection(address, false);

    return "Disconnected.\n";
}



}
