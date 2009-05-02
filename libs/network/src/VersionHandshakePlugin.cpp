
#include "VersionHandshakePlugin.h"


#include <raknet/RakPeerInterface.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/BitStream.h>


#include "Log.h"
#include "MessageIds.h"
#include "NetworkUtils.h"



namespace network
{

/// Time after which connection is closed automatically in case
/// version handshake doesn't succeed.
const float AUTO_CLOSE_TIMEOUT = 8.0f;

//------------------------------------------------------------------------------
VersionHandshakePlugin::VersionHandshakePlugin(AcceptVersionCallbackServer cb) :
    server_side_(true),
    accept_version_server_(cb),
    interface_(NULL)
{
}


//------------------------------------------------------------------------------
VersionHandshakePlugin::VersionHandshakePlugin(AcceptVersionCallbackClient cb) :
    server_side_(false),
    accept_version_client_(cb),
    interface_(NULL)    
{
}

    
//------------------------------------------------------------------------------
void VersionHandshakePlugin::OnAttach(RakPeerInterface *peer)
{
    assert(!interface_);
    interface_ = peer;
}

//------------------------------------------------------------------------------
void VersionHandshakePlugin::OnDetach(RakPeerInterface *peer)
{
    assert(interface_ == peer);
    interface_ = NULL;
}

//------------------------------------------------------------------------------
void VersionHandshakePlugin::OnShutdown(RakPeerInterface *peer)
{
    assert(peer == interface_);
    peer->DetachPlugin(this);
}

//------------------------------------------------------------------------------
PluginReceiveResult VersionHandshakePlugin::OnReceive(RakPeerInterface *peer, Packet *packet)
{
    if (packet->length == 0) return RR_CONTINUE_PROCESSING;

    switch (packet->data[0])
    {
    case ID_NEW_INCOMING_CONNECTION:
    {
        // Incoming connection to client - this doesn't concern us,
        // ignore
        if (!server_side_)
        {
            s_log << Log::warning
                  << "Ignoring incoming connection from "
                  << packet->systemAddress
                  << " in VersionHandshakePlugin::OnReceive\n";
            return RR_CONTINUE_PROCESSING;
        }

        
        std::map<SystemAddress, HandshakeInfo>::iterator it = handshake_info_.find(packet->systemAddress);
        if (it != handshake_info_.end())
        {
            // This is the ID_NEW_INCOMING_CONNECTION we created
            // ourselves upon successful version handshake. Remove
            // handshake_info_ entry and pass it through.
            s_scheduler.removeTask(it->second.task_auto_close_, &fp_group_);
            handshake_info_.erase(it);
            return RR_CONTINUE_PROCESSING;
        } else
        {
            s_log << Log::debug('n')
                  << "new incoming connection from "
                  << packet->systemAddress
                  << "\n";
        
            handshake_info_[packet->systemAddress] = HandshakeInfo(autoClose(packet->systemAddress));
            
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        }
    }
    break;
        
    case ID_DISCONNECTION_NOTIFICATION:
    case ID_CONNECTION_LOST:

        s_log << Log::debug('n')
              << "lost connection to "
              << packet->systemAddress
              << "\n";
        
        if (server_side_)
        {
            std::map<SystemAddress, HandshakeInfo>::iterator it = handshake_info_.find(packet->systemAddress);

            // all packets for connected clients still are handled by
            // this plugin, so only act if a handshake is still in
            // progress.
            if (it != handshake_info_.end())
            {
                // remove autoclose task, or we might close a future
                // connection that comes in from the same address.
                s_scheduler.removeTask(it->second.task_auto_close_, &fp_group_);
                handshake_info_.erase(it);
            }
        }
        return RR_CONTINUE_PROCESSING;

        
    case ID_CONNECTION_REQUEST_ACCEPTED:
        
        // Ignore this on server side - it's none of our business.
        if (server_side_)
        {
            s_log << Log::warning
                  << "Ignoring ID_CONNECTION_REQUEST_ACCEPTED from "
                  << packet->systemAddress
                  << " in VersionHandshakePlugin::OnReceive().\n";
            return RR_CONTINUE_PROCESSING;
        }

        
        // our suicide after successful version handshake will stop
        // auto close.
        autoClose(packet->systemAddress);
        
        sendVersionInfo(packet->systemAddress, g_version);

        s_log << Log::debug('n')
              << "Sent version info "
              << g_version
              << " after connection request accepted from "
              << packet->systemAddress
              << "\n";
        
        return RR_STOP_PROCESSING_AND_DEALLOCATE;

    case VHPI_VERSION_MISMATCH:
    case VHPI_TYPE_MISMATCH:
    {
        std::map<SystemAddress, HandshakeInfo>::iterator it = handshake_info_.find(packet->systemAddress);
        if (it != handshake_info_.end())
        {
            s_scheduler.removeTask(it->second.task_auto_close_, &fp_group_);
            handshake_info_.erase(it);
        }
        suicideIfNeccessary();
        
        return RR_CONTINUE_PROCESSING;
    }
    
    case VHPI_VERSION_INFO_INTERNAL:
    {
        VersionInfo info;
        RakNet::BitStream stream(&packet->data[1], packet->length-1, false);

        if (!info.readFromBitstream(stream))
        {
            s_log << "Invalid version info from "
                  << packet->systemAddress
                  << ". Closing connection.\n";
            peer->CloseConnection(packet->systemAddress, true);
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        }


        ACCEPT_VERSION_CALLBACK_RESULT res;
        VersionInfo our_reported_version;
        if (server_side_)
        {
            res = accept_version_server_(info, our_reported_version);
        } else
        {
            res = accept_version_client_(info);
            our_reported_version = g_version;
        }
        if (res != AVCR_ACCEPT)
        {
            bool type_mismatch = (res == AVCR_TYPE_MISMATCH);
            
            s_log << "Closing connection to "
                  << packet->systemAddress
                  << " because of mismatching "
                  << (type_mismatch ? std::string("type ") : std::string("version "))
                  << info
                  << "\n";

            sendMismatch(packet->systemAddress, our_reported_version, type_mismatch);

            // Push packet with failure info
            RakNet::BitStream stream;
            if (type_mismatch)
            {
                stream.Write((uint8_t)VHPI_TYPE_MISMATCH);
            } else
            {
                stream.Write((uint8_t)VHPI_VERSION_MISMATCH);
            }
            info.writeToBitstream(stream);
            pushPacket(stream, packet->systemAddress);
            
            peer->CloseConnection(packet->systemAddress, true);

            suicideIfNeccessary();
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        }
        
        if (server_side_)
        {
            std::map<SystemAddress, HandshakeInfo>::iterator it = handshake_info_.find(packet->systemAddress);
            if (it != handshake_info_.end())
            {
                // remember version, send response
                it->second.info_ = info;
                sendVersionInfo(packet->systemAddress, our_reported_version);

                s_log << Log::debug('n')
                      << "Sent version info "
                      << our_reported_version
                      << " to "
                      << packet->systemAddress
                      << "\n";
            } else
            {
                s_log << Log::warning
                      << "received unexpected VHPI_VERSION_INFO_INTERNAL from "
                      << packet->systemAddress
                      << "\n";
            }

            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        } else
        {
            sendVersionAck(packet->systemAddress);

            s_log << Log::debug('n')
                  << "Sent version ack to "
                  << packet->systemAddress
                  << "\n";

            // prepare version info packet
            RakNet::BitStream stream1;
            stream1.Write((uint8_t)VHPI_VERSION_INFO);
            info.writeToBitstream(stream1);
            pushPacket(stream1, packet->systemAddress);

            // ..and before it, push ID_CONNECTION_REQUEST_ACCEPTED
            RakNet::BitStream stream2;
            stream2.Write((uint8_t)ID_CONNECTION_REQUEST_ACCEPTED);
            pushPacket(stream2, packet->systemAddress);

            // After suicide, we won't intercept the
            // ID_CONNECTION_REQUEST_ACCEPTED which we just pushed. It
            // will also terminate autoClose.
            suicideIfNeccessary();

            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        }
    }
    case VHPI_VERSION_ACK:
    {
        if (!server_side_)
        {
            s_log << Log::warning
                  << "Ignoring VHPI_VERSION_ACK from "
                  << packet->systemAddress
                  << " in VersionHandshakePlugin::OnReceive().\n";
            return RR_CONTINUE_PROCESSING;
        }
        
        std::map<SystemAddress, HandshakeInfo>::iterator it = handshake_info_.find(packet->systemAddress);
        if (it == handshake_info_.end()) 
        {
            s_log << Log::warning
                  << "Ignoring VHPI_VERSION_ACK from unknown peer "
                  << packet->systemAddress
                  << ".\n";
            return RR_CONTINUE_PROCESSING;
        }

        s_log << Log::debug('n')
              << "Got version ack from "
              << packet->systemAddress
              << "\n";


        // prepare version info packet
        RakNet::BitStream stream1;
        stream1.Write((uint8_t)VHPI_VERSION_INFO);
        it->second.info_.writeToBitstream(stream1);
        pushPacket(stream1, packet->systemAddress);

        // ..and before it, ID_NEW_INCOMING_CONNECTION
        RakNet::BitStream stream2;
        stream2.Write((uint8_t)ID_NEW_INCOMING_CONNECTION);
        pushPacket(stream2, packet->systemAddress);
        
        return RR_STOP_PROCESSING_AND_DEALLOCATE;
    }

    
    default:

        // always let through basic system messages.
        if (packet->data[0] <= ID_PONG) return RR_CONTINUE_PROCESSING;

        if (!server_side_)
        {
            // on client, block all user messages
            return RR_STOP_PROCESSING_AND_DEALLOCATE;
        } else
        {
            // on server, only block messages from peers currently
            // performing handshake.
            std::map<SystemAddress, HandshakeInfo>::iterator it = handshake_info_.find(packet->systemAddress);
            if (it != handshake_info_.end()) return RR_STOP_PROCESSING_AND_DEALLOCATE;
            else return RR_CONTINUE_PROCESSING;
        } 
    }
}


//------------------------------------------------------------------------------
void VersionHandshakePlugin::suicideIfNeccessary()
{
    if (!server_side_)
    {
        interface_->DetachPlugin(this);
        delete this;
    }
}


//------------------------------------------------------------------------------
void VersionHandshakePlugin::sendVersionInfo(const SystemAddress & dest, const VersionInfo & info)
{
    RakNet::BitStream stream;

    stream.Write((uint8_t)VHPI_VERSION_INFO_INTERNAL);
    info.writeToBitstream(stream);

    interface_->Send(   &stream,
                        MEDIUM_PRIORITY,
                        RELIABLE_ORDERED,
                        0, dest, false);
}


//------------------------------------------------------------------------------
void VersionHandshakePlugin::sendVersionAck(const SystemAddress & dest)
{
    RakNet::BitStream stream;

    stream.Write((uint8_t)VHPI_VERSION_ACK);
    interface_->Send(   &stream,
                        MEDIUM_PRIORITY,
                        RELIABLE_ORDERED,
                        0, dest, false);
}


//------------------------------------------------------------------------------
void VersionHandshakePlugin::sendMismatch(const SystemAddress & dest, const VersionInfo & info, bool type_mismatch)
{
    RakNet::BitStream stream;

    if (type_mismatch)
    {
        stream.Write((uint8_t)VHPI_TYPE_MISMATCH);
    } else
    {
        stream.Write((uint8_t)VHPI_VERSION_MISMATCH);
    }
    info.writeToBitstream(stream);

    interface_->Send(   &stream,
                        MEDIUM_PRIORITY,
                        RELIABLE_ORDERED,
                        0, dest, false);
}


//------------------------------------------------------------------------------
void VersionHandshakePlugin::pushPacket(RakNet::BitStream & stream,
                                        const SystemAddress & address)
{
    Packet * new_packet = interface_->AllocatePacket(stream.GetNumberOfBytesUsed());
    memcpy(new_packet->data, stream.GetData(), stream.GetNumberOfBytesUsed());
    new_packet->systemAddress = address;
    interface_->PushBackPacket(new_packet, true);
    
}


//------------------------------------------------------------------------------
hTask VersionHandshakePlugin::autoClose(const SystemAddress & address)
{
    return s_scheduler.addEvent(SingleEventCallback(this, &VersionHandshakePlugin::closeConnection),
                                AUTO_CLOSE_TIMEOUT,
                                new SystemAddress(address),
                                "VersionHandshakePlugin::closeConnection",
                                &fp_group_);
}


//------------------------------------------------------------------------------
/**
 *  Automatically emit VHPI_VERSION_MISMATCH after a timeout, in case
 *  version handshake doesn't succeed.
 */
void VersionHandshakePlugin::closeConnection(void * ad)
{
    const SystemAddress & address = *(const SystemAddress*)ad;

    // Shouldn't be called on server if handshake was successfully
    // completed.
    std::map<SystemAddress, HandshakeInfo>::iterator it = handshake_info_.find(address);
    assert(!server_side_ || it != handshake_info_.end());

    // Remove entry for this handshake.
    if (it != handshake_info_.end()) handshake_info_.erase(it);

    s_log << Log::warning
          << "Closing connection to "
          << address
          << " after version handshake timeout.\n";

    // Push packet with failure info
    RakNet::BitStream stream;
    stream.Write((uint8_t)VHPI_TYPE_MISMATCH);
    VersionInfo info;
    info.writeToBitstream(stream);
    pushPacket(stream, address);

    interface_->CloseConnection(address, true);

    delete &address;
}


    
}

