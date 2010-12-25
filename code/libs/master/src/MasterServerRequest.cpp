
#include "MasterServerRequest.h"


#include <raknet/RakPeerInterface.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/SocketLayer.h>
#include <raknet/BitStream.h>


#include "ParameterManager.h"
#include "Scheduler.h"


namespace network
{

namespace master
{


const unsigned NUM_RETRIES = 10;
const float    RETRY_DELAY = 0.5f;


//------------------------------------------------------------------------------
MasterServerRequest::MasterServerRequest(RakPeerInterface * rak_peer_interface,
                                         const char * data, unsigned num_bytes,
                                         uint8_t response_id) :
    num_remaining_retries_(NUM_RETRIES),
    response_id_(response_id),
    data_(num_bytes)
{
    memcpy(&data_[0], data, num_bytes);
    
    // Lookup for master server
    const char * host = SocketLayer::DomainNameToIP(
                    s_params.get<std::string>("master_server.host").c_str());
    if (host == NULL) throw Exception("Unknown host for master server: " +
                                      s_params.get<std::string>("master_server.host"));

    server_address_.SetBinaryAddress(host);
    server_address_.port = s_params.get<unsigned>("master_server.port");
    assert(server_address_ != UNASSIGNED_SYSTEM_ADDRESS);

    s_scheduler.addTask(PeriodicTaskCallback(this, &MasterServerRequest::send),
                        RETRY_DELAY,
                        "MasterServerRequest::send",
                        &fp_group_);


    rak_peer_interface->AttachPlugin(this);
}


//------------------------------------------------------------------------------
MasterServerRequest::~MasterServerRequest()
{
    rakPeerInterface->DetachPlugin(this);
}


//------------------------------------------------------------------------------
PluginReceiveResult MasterServerRequest::OnReceive(Packet *packet)
{
    if (packet->systemAddress != server_address_ ||    
        packet->length < 2                       ||
        packet->data[0] != ID_ADVERTISE_SYSTEM   || 
        packet->data[1] != response_id_)
    {
        return RR_CONTINUE_PROCESSING;
    }

    RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
    emit(MSRE_RECEIVED_RESPONSE, &stream);
    delete this;

    return RR_STOP_PROCESSING_AND_DEALLOCATE;
}


//------------------------------------------------------------------------------
void MasterServerRequest::OnShutdown(RakPeerInterface *peer)
{
    peer->DetachPlugin(this);
    delete this;
}

//------------------------------------------------------------------------------
const SystemAddress & MasterServerRequest::getServerAddress() const
{
    return server_address_;
}


//------------------------------------------------------------------------------
void MasterServerRequest::send(float dt)
{
    if (num_remaining_retries_-- == 0)
    {
        emit(MSRE_CONTACT_FAILED);
        delete this;
    } else
    {
        rakPeerInterface->AdvertiseSystem(server_address_.ToString(false),
                                    server_address_.port,
                                    (const char*)&data_[0],
                                    data_.size());
    }
}



}
}
