

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
    request_(NULL),
    punchthrough_port_(0)
{
}


//------------------------------------------------------------------------------
PluginReceiveResult MasterServerPunchthrough::OnReceive(Packet *packet)
{
    if (packet->length == 0) return NatPunchthroughClient::OnReceive(packet);

    s_log << packet->data[0] << "\n";
    switch (packet->data[0])
    {
    case ID_CONNECTION_ATTEMPT_FAILED:
        if (punchthrough_port_ == 0) return NatPunchthroughClient::OnReceive(packet);
        
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
                rakPeerInterface->CloseConnection(facilitator_address_, true);
            }
        }
        return NatPunchthroughClient::OnReceive(packet);
                
        break;
    case ID_NEW_INCOMING_CONNECTION:
        //  After we sent our NAT punchthrough request, the master
        //  server should connect to us.
        if (packet->systemAddress != facilitator_address_ ||
            rakPeerInterface->NumberOfConnections() == 2)
        {
            // not from master server, or we already are connected
            s_log << Log::debug('m')
                  << "Ignoring incoming connection from "
                  << packet->systemAddress.ToString()
                  << "\n";
            rakPeerInterface->CloseConnection(packet->systemAddress, true);
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
            
            s_scheduler.addEvent(SingleEventCallback(this, &MasterServerPunchthrough::openNat),
                                 0, 0, "openNat", &fp_group_);
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

            if (rakPeerInterface->NumberOfConnections() == 0)
            {
                // We haven't established connection to our target
                // system.
                throw Exception("Failed to connect to server.");
            } else
            {
                // Our job is done, suicide
                rakPeerInterface->DetachPlugin(this);
                delete this;
                return RR_STOP_PROCESSING_AND_DEALLOCATE;
            }
        } 
        break;
    case ID_NAT_PUNCHTHROUGH_SUCCEEDED:
        s_log << "Nat punchthrough succeeded. Now attempting direct connection to "
              << packet->systemAddress.ToString(true) << "\n";
        rakPeerInterface->Connect(packet->systemAddress.ToString(false), packet->systemAddress.port,
                                  0, 0);

        // Our job is done, suicide
        rakPeerInterface->DetachPlugin(this);
        delete this;

        return RR_STOP_PROCESSING_AND_DEALLOCATE;
    }
            
    return NatPunchthroughClient::OnReceive(packet);
}

//------------------------------------------------------------------------------
void MasterServerPunchthrough::OnRakPeerShutdown()
{
    rakPeerInterface->DetachPlugin(this);
    delete this;
}



//------------------------------------------------------------------------------
void MasterServerPunchthrough::connect(const RakNetGUID& guid,
                                       const SystemAddress & address,
                                       unsigned internal_port)
{
    // Juggle ports for direct connection attempt
    punchthrough_port_ = address.port;
    target_server_address_ = address;
    target_server_address_.port = internal_port;
    target_guid_ = guid;

//      testing code to immendiately do NAT punchthrough attempt, without first trying the direct
//      connection.

        target_server_address_.port = punchthrough_port_;
        punchthrough_port_ = 0;
        connectWithPunchThrough();
        return;


    s_log << "Attempting direct connection to "
          << target_server_address_.ToString()
          << "...\n";
    
    if (!rakPeerInterface->Connect(target_server_address_.ToString(false),
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
    request_ = new MasterServerRequest(rakPeerInterface,
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
    request_ = new MasterServerRequest(rakPeerInterface,
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

//------------------------------------------------------------------------------
void MasterServerPunchthrough::openNat(void*)
{
    s_log << "MasterServerPunchthrough::openNat " << target_guid_.ToString() << " " << facilitator_address_.ToString(true) << "\n";
    if (!OpenNAT(RakNetGUID()/*target_guid_*/, facilitator_address_))
    {
        rakPeerInterface->CloseConnection(facilitator_address_, true);
        s_log << Log::error << "OpenNAT call failed.\n";
    }
}


} // namespace master
} // namespace network
