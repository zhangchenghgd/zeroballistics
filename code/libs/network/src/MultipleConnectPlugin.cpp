
#include "MultipleConnectPlugin.h"

#include <raknet/RakPeerInterface.h>
#include <raknet/MessageIdentifiers.h>


#include "NetworkUtils.h"
#include "Log.h"
#include "Exception.h"
#include "assert.h"


namespace network
{

//------------------------------------------------------------------------------
MultipleConnectPlugin::MultipleConnectPlugin() :
    had_no_free_incoming_connections_(false),
    cur_server_(0)
{
}


//------------------------------------------------------------------------------
void MultipleConnectPlugin::connect(const std::vector<std::string> & hosts,
                                    const std::vector<unsigned>    & ports,
                                    unsigned num_retries)
{
    had_no_free_incoming_connections_ = false;
    
    if (hosts.size() != ports.size())
    {
        throw Exception("host and port array size don't match in MultipleConnectPlugin::connect");
    }

    hosts_ = hosts;
    ports_ = ports;
    cur_server_  = 0;

    retries_.resize(hosts.size(), num_retries);
    
    connect(cur_server_);
}

//------------------------------------------------------------------------------
void MultipleConnectPlugin::OnRakPeerShutdown()
{
    rakPeerInterface->DetachPlugin(this);
    delete this;
}
    

//------------------------------------------------------------------------------
PluginReceiveResult MultipleConnectPlugin::OnReceive(Packet *packet)
{
    switch(packet->data[0])
    {
    case ID_CONNECTION_REQUEST_ACCEPTED:

        rakPeerInterface->DetachPlugin(this);
        delete this;
        return RR_CONTINUE_PROCESSING;
        break;
        
    case ID_NO_FREE_INCOMING_CONNECTIONS:
    case ID_CONNECTION_ATTEMPT_FAILED:

        if (packet->data[0] == ID_NO_FREE_INCOMING_CONNECTIONS)
        {
            if (retries_[cur_server_]) --retries_[cur_server_];
            had_no_free_incoming_connections_ = true;
            s_log << packet->systemAddress
                  << " has no free incoming connections. Number of retries remaining: "
                  << retries_[cur_server_]
                  << "\n";
        } else
        {
            retries_[cur_server_] = 0;
            s_log << "failed to connect to "
                  << packet->systemAddress
                  << "...\n";
        }

        // Determine next server to connect to, round-robin.
        unsigned next_server = cur_server_;
        bool found_server = false;
        do
        {
            if (++next_server == hosts_.size()) next_server=0;

            if (retries_[next_server])
            {
                found_server = true;
                cur_server_ = next_server;
                break;
            }
            
        } while(next_server != cur_server_);


        
        if (!found_server)
        {
            // we ran out of options, report failure...

            if (had_no_free_incoming_connections_)
            {
                // there were servers which had no free incoming
                // connections, make sure we report this even if the
                // current server was completely unreachable...
                packet->data[0] = ID_NO_FREE_INCOMING_CONNECTIONS;
            }

            
            // suicide...
            rakPeerInterface->DetachPlugin(this);
            delete this;
            
            return RR_CONTINUE_PROCESSING;
            
        } else
        {
            // Try again...
            connect(cur_server_);
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        }
        break;
    }

    return RR_CONTINUE_PROCESSING;
}



//------------------------------------------------------------------------------
void MultipleConnectPlugin::connect(unsigned server_index)
{
    bool ret = rakPeerInterface->Connect(hosts_[cur_server_].c_str(),
                                   ports_[cur_server_],
                                   NULL, 0, 0);

    if (!ret)
    {
        s_log << Log::error
              << "Connect() call failed for "
              << hosts_[cur_server_].c_str()
              << ":"
              << ports_[cur_server_]
              << "\n";

        // Inject ID_CONNECTION_ATTEMPT_FAILED for unified handling...
        Packet * new_packet = rakPeerInterface->AllocatePacket(1);
        new_packet->data[0]       = ID_CONNECTION_ATTEMPT_FAILED;
        new_packet->systemAddress = UNASSIGNED_SYSTEM_ADDRESS;
        rakPeerInterface->PushBackPacket(new_packet, true);
    }
}

} // namespace network
