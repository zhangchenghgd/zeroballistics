
#include "ClientInterface.h"

#include <raknet/RakNetworkFactory.h>
#include <raknet/BitStream.h>


#include "ParameterManager.h"
#include "Scheduler.h"

#include "RakAutoPacket.h"
#include "NetworkUtils.h"
#include "MultipleConnectPlugin.h"

namespace network
{

//------------------------------------------------------------------------------
ClientInterface::ClientInterface() :
    interface_(RakNetworkFactory::GetRakPeerInterface())
{
}
    
//------------------------------------------------------------------------------
ClientInterface::~ClientInterface()
{
    interface_->Shutdown(300);
    RakNetworkFactory::DestroyRakPeerInterface(interface_);    
}


//------------------------------------------------------------------------------
void ClientInterface::connect(const std::vector<std::string> & hosts,
                              const std::vector<unsigned> & ports,
                              unsigned num_retries,
                              AcceptVersionCallbackClient cb,                 
                              unsigned sleep_timer,
                              float handle_network_dt,
                              const char * public_key)
{
    assert(interface_ && !interface_->IsActive());
    
    interface_->AttachPlugin(new VersionHandshakePlugin(cb));

    /* TODO CM fix encryption?
    if (public_key)
    {
        initializeSecurity(interface_, public_key, true);
    }
    */

    SocketDescriptor desc;
    if (!interface_->Startup(1, sleep_timer, &desc, 1))
    {
        throw Exception("Could not start up interface in  ClientInterface::connect.");
    }

    MultipleConnectPlugin * connect_plugin = new MultipleConnectPlugin();
    interface_->AttachPlugin(connect_plugin);
    connect_plugin->connect(hosts, ports,
                            num_retries);

    s_scheduler.addTask(PeriodicTaskCallback(this, &ClientInterface::handleNetwork),
                        handle_network_dt,
                        "ClientInterface::handleNetwork",
                        &fp_group_);    
}



//------------------------------------------------------------------------------
void ClientInterface::handleNetwork(float dt)
{
    RakAutoPacket packet(interface_);
    while (interface_ && packet.receive())
    {
        if (!handlePacket(packet))
        {
            defaultPacketAction(packet, interface_);
        }
    }       
}

    
}
