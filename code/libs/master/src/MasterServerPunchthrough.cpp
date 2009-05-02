

#include "MasterServerPunchthrough.h"



#include <raknet/MessageIdentifiers.h>
#include <raknet/RakPeerInterface.h>
#include <raknet/BitStream.h>
#include <raknet/SocketLayer.h>


#include "Scheduler.h"
#include "Log.h"
#include "ParameterManager.h"

#include "MessageIds.h"
#include "MasterServerRequest.h"


namespace network
{
namespace master
{


//------------------------------------------------------------------------------
MasterServerPunchthrough::MasterServerPunchthrough() :
    target_server_address_(UNASSIGNED_SYSTEM_ADDRESS),
    facilitator_address_  (UNASSIGNED_SYSTEM_ADDRESS),
    interface_(NULL),
    request_(NULL),
    punchthrough_port_(0)
{
}


//------------------------------------------------------------------------------
void MasterServerPunchthrough::OnAttach(RakPeerInterface *peer)
{
    NatPunchthrough::OnAttach(peer);
            
    interface_ = peer;
}



//------------------------------------------------------------------------------
PluginReceiveResult MasterServerPunchthrough::OnReceive(RakPeerInterface *peer, Packet *packet)
{
    assert(peer == interface_);
            
    if (packet->length == 0) return NatPunchthrough::OnReceive(peer, packet);
            
    switch (packet->data[0])
    {
    case ID_CONNECTION_ATTEMPT_FAILED:
        if (!punchthrough_port_) return NatPunchthrough::OnReceive(peer, packet);
        
        s_log << "Direct connect failed. Attempting NAT punchthrough...\n";
        target_server_address_.port = punchthrough_port_;
        punchthrough_port_ = 0;
        connectWithPunchThrough();

        return RR_STOP_PROCESSING_AND_DEALLOCATE;
        break;
    case ID_CONNECTION_REQUEST_ACCEPTED:
        if (packet->systemAddress == target_server_address_)
        {
            // Connection was successful, abort all tasks and close
            // connection to facilitator. We still have to stay alive
            // to intercept the "connection closed" message from the
            // master server.
            delete request_;
            request_ = NULL;
            if (facilitator_address_ != UNASSIGNED_SYSTEM_ADDRESS)
            {
                interface_->CloseConnection(facilitator_address_, true);
            }
        }
        return NatPunchthrough::OnReceive(peer, packet);
                
        break;
    case ID_NEW_INCOMING_CONNECTION:
        //  After we sent our NAT punchthrough request, the master
        //  server should connect to us.
        if (packet->systemAddress != facilitator_address_ ||
            peer->NumberOfConnections() == 2)
        {
            // not from master server, or we already are connected
            s_log << Log::debug('m')
                  << "Ignoring incoming connection from "
                  << packet->systemAddress.ToString()
                  << "\n";
            peer->CloseConnection(packet->systemAddress, true);
        } else
        {
            s_log << Log::debug('m')
                  << "incoming facilitator connection: "
                  << packet->systemAddress.ToString()
                  << ", trying to connect to "
                  << target_server_address_.ToString()
                  << " via NAT punchthrough\n";

            // remove NAT punchthrough request
            delete request_;
            request_ = NULL;            
            
            if (!NatPunchthrough::Connect(target_server_address_,
                                          0,0,
                                          facilitator_address_))
            {
                peer->CloseConnection(packet->systemAddress, true);
                throw Exception("Failed to connect to server.");
            }
        }

        return RR_STOP_PROCESSING_AND_DEALLOCATE;
                
    case ID_DISCONNECTION_NOTIFICATION:
    case ID_CONNECTION_LOST:
        // intercept disconnect message from facilitator
        if (packet->systemAddress == facilitator_address_)
        {
            s_log << Log::debug('m')
                  << "disconnected from facilitator "
                  << facilitator_address_.ToString()
                  << "\n";

            if (peer->NumberOfConnections() == 0)
            {
                // We haven't established connection to our target
                // system.
                throw Exception("Failed to connect to server.");
            } else
            {
                // Our job is done, suicide
                interface_->DetachPlugin(this);
                delete this;
                return RR_STOP_PROCESSING_AND_DEALLOCATE;
            }
        } 
        break;
    }
            
    return NatPunchthrough::OnReceive(peer, packet);
}

//------------------------------------------------------------------------------
void MasterServerPunchthrough::OnShutdown(RakPeerInterface *peer)
{
    NatPunchthrough::OnShutdown(peer);
    
    peer->DetachPlugin(this);
    delete this;
}



//------------------------------------------------------------------------------
void MasterServerPunchthrough::connect(const SystemAddress & address,
                                       unsigned internal_port)
{
    // Juggle ports for direct connection attempt
    punchthrough_port_ = address.port;
    target_server_address_ = address;
    target_server_address_.port = internal_port;

    s_log << "Attempting direct connection to "
          << target_server_address_.ToString()
          << "...\n";
    
    if (!interface_->Connect(target_server_address_.ToString(false),
                             target_server_address_.port,
                             NULL, 0, 0))
    {
        throw Exception("Connection attempt failed ");
    }
}


//------------------------------------------------------------------------------
void MasterServerPunchthrough::connectWithPunchThrough()
{
    uint8_t m = MPI_REQUEST_AUTH_TOKEN;
    request_ = new MasterServerRequest(interface_,
                                       (const char*)&m, 1,
                                       MPI_AUTH_TOKEN);
    request_->addObserver(ObserverCallbackFunUserData(this, &MasterServerPunchthrough::onTokenReceived),
                          MSRE_RECEIVED_RESPONSE,
                          &fp_group_);
    request_->addObserver(ObserverCallbackFun0(this, &MasterServerPunchthrough::onMasterUnreachable),
                          MSRE_CONTACT_FAILED,
                          &fp_group_);
    facilitator_address_ = request_->getServerAddress();
}


//------------------------------------------------------------------------------
void MasterServerPunchthrough::onTokenReceived(Observable*, void * s, unsigned)
{
    RakNet::BitStream & stream = *(RakNet::BitStream*)s;

    // ignore message id
    stream.IgnoreBytes(1);
    
    uint32_t token;
    stream.Read(token);


    RakNet::BitStream args;
    args.Write(token);
    args.Write((uint8_t)master::MPI_REQUEST_NAT_PUNCHTHROUGH);
    args.Write(target_server_address_);
    

    // request is destroyed when master server connects to us,
    // MPI_LAST should never be received -> don't install observer for
    // MSRE_RECEIVED_RESPONSE.
    request_ = new MasterServerRequest(interface_,
                                       (const char*)args.GetData(), args.GetNumberOfBytesUsed(),
                                       MPI_LAST);
    request_->addObserver(ObserverCallbackFun0(this, &MasterServerPunchthrough::onMasterUnreachable),
                          MSRE_CONTACT_FAILED,
                          &fp_group_);
}


//------------------------------------------------------------------------------
void MasterServerPunchthrough::onMasterUnreachable()
{
    throw Exception("Could not connect.");
}


} // namespace master
} // namespace network
