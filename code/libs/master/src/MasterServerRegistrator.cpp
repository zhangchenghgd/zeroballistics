

#include "MasterServerRegistrator.h"


#include <raknet/RakNetworkFactory.h>
#include <raknet/RakPeerInterface.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/SocketLayer.h>


#include "Scheduler.h"
#include "ParameterManager.h"

#include "MessageIds.h"

#include "RakAutoPacket.h"



namespace network
{

namespace master
{

    
//------------------------------------------------------------------------------
MasterServerRegistrator::MasterServerRegistrator() :
    master_server_address_(UNASSIGNED_SYSTEM_ADDRESS),
    last_token_(0)
{
}


//------------------------------------------------------------------------------
MasterServerRegistrator::~MasterServerRegistrator()
{
}





//------------------------------------------------------------------------------
/**
 *  Connects to the master server. As soon as the connection is
 *  established, reports the new server information, instantly closes
 *  the connection again, and sends heartbeats periodically.
 */
void MasterServerRegistrator::sendServerInfo(const ServerInfo & info)
{
    cur_info_ = info;
    sendAuthTokenRequest(0.0f);
}

//------------------------------------------------------------------------------
void MasterServerRegistrator::OnDetach ()
{
    RakNet::BitStream args;
    args.Write((uint8_t)MPI_REMOVE_SERVER);
    args.Write(last_token_); // to avoid fake removal messages, authenticate with last token.
    args.Write(cur_info_.address_.port); // master needs to know internal port to identify this server
    rakPeerInterface->AdvertiseSystem(master_server_address_.ToString(false),
                                master_server_address_.port,
                                (const char*)args.GetData(),
                                args.GetNumberOfBytesUsed());

    fp_group_.deregisterAllOfType(TaskFp());
}


//------------------------------------------------------------------------------
PluginReceiveResult MasterServerRegistrator::OnReceive (Packet *packet)
{
    if (packet->length == 0) return RR_CONTINUE_PROCESSING;

    switch (packet->data[0])
    {
    case ID_CONNECTION_LOST:
    case ID_DISCONNECTION_NOTIFICATION:
        if (packet->systemAddress == master_server_address_)
        {
            s_log << Log::debug('m')
                  << "MasterServerRegistrator : connection to master server "
                  << packet->systemAddress.ToString()
                  << " closed\n";
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        } else return RR_CONTINUE_PROCESSING;
        break;
    case ID_NEW_INCOMING_CONNECTION:
        if (packet->systemAddress == master_server_address_)
        {
            s_log << Log::debug('m')
                  << "MasterServerRegistrator : incoming connection from master server "
                  << packet->systemAddress.ToString()
                  << "\n";
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        } else return RR_CONTINUE_PROCESSING;
        break;
    case ID_ADVERTISE_SYSTEM:
    {
        // We need at least two IDs: ADVERTISE and our own.
        if (packet->length < 2 ) return RR_CONTINUE_PROCESSING;
                
        RakNet::BitStream stream(&packet->data[2], packet->length-2, false);

        switch (packet->data[1])
        {
        case MPI_AUTH_TOKEN:
            master_server_address_ = packet->systemAddress;
            uint32_t token;
            stream.Read(token);
            s_log << Log::debug('m')
                  << "Got auth token "
                  << token
                  << "\n";
            last_token_ = token;
            sendHeartbeat(token);
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        default:
            return RR_CONTINUE_PROCESSING;
        }
    }
    break;
    default:
        return RR_CONTINUE_PROCESSING;
    }
}


//------------------------------------------------------------------------------
void MasterServerRegistrator::sendAuthTokenRequest(float dt)
{
    if (master_server_address_ == UNASSIGNED_SYSTEM_ADDRESS)
    {
        // Lookup for master server
        const char * host = SocketLayer::DomainNameToIP(
                        s_params.get<std::string>("master_server.host").c_str());
        if (host == NULL) throw Exception("Unknown host for master server: " +
                                          s_params.get<std::string>("master_server.host"));

        master_server_address_.SetBinaryAddress(host);
        master_server_address_.port = s_params.get<unsigned>("master_server.port");

        assert(master_server_address_ != UNASSIGNED_SYSTEM_ADDRESS);
        
        // now that we have at least the master server IP, schedule heartbeat
        s_scheduler.addTask(
            PeriodicTaskCallback(this, &MasterServerRegistrator::sendAuthTokenRequest),
            s_params.get<unsigned>("master_server.heartbeat_interval"),
            "MasterServerRegistrator::sendHeartbeat",
            &fp_group_);        
    }


    
    RakNet::BitStream args;
    args.Write((uint8_t)MPI_REQUEST_AUTH_TOKEN);
    rakPeerInterface->AdvertiseSystem(master_server_address_.ToString(false),
                                master_server_address_.port,
                                (const char*)args.GetData(),
                                args.GetNumberOfBytesUsed());
}



//------------------------------------------------------------------------------
void MasterServerRegistrator::sendHeartbeat(uint32_t token)
{
    RakNet::BitStream stream;
    stream.Write(token);
    stream.Write((uint8_t)MPI_HEARTBEAT);
    cur_info_.writeToBitstream(stream);
                
    rakPeerInterface->AdvertiseSystem(master_server_address_.ToString(false),
                                master_server_address_.port,
                                (const char*)stream.GetData(),
                                stream.GetNumberOfBytesUsed());
}

} // namespace master

} // namespace network
