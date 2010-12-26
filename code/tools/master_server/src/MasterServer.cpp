
#include "MasterServer.h"


#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <raknet/MessageIdentifiers.h>
#include <raknet/RakNetworkFactory.h>
#include <raknet/BitStream.h>
#include <raknet/NatPunchthroughServer.h>

#include "ParameterManager.h"
#include "Scheduler.h"
#include "MessageIds.h"
#include "VersionInfo.h"
#include "RakAutoPacket.h"
#include "NetworkUtils.h"


namespace network
{

namespace master
{




//------------------------------------------------------------------------------
/**
 *  Servers are identified by their external IP and internal port.
 */
class AddressId : public std::unary_function<SystemAddress, bool>
{
public:
    AddressId(const SystemAddress & id) : id_(id) { }
    
    AddressId(const SystemAddress & external_address, unsigned internal_port)
        {
            id_.binaryAddress = external_address.binaryAddress;
            id_.port          = internal_port;
        }

    bool operator()(const MasterServer::GameServer & server) const
        {
            return (server.external_address_.binaryAddress == id_.binaryAddress &&
                    server.info_.address_.port             == id_.port);
        }

    const SystemAddress & getAddress() const
        {
            return id_;
        }

protected:
    SystemAddress id_;
};


//------------------------------------------------------------------------------
/**
 *  For NAT punchthrough requests, we need to search for the external
 *  ID only.
 */
class ExternalAddressEqual : public std::binary_function<MasterServer::GameServer, SystemAddress, bool>
{
public:
    bool operator()(const MasterServer::GameServer & server, const SystemAddress & ad) const
        {
            return server.external_address_ == ad;
        }
};
    
    
//------------------------------------------------------------------------------
MasterServer::MasterServer() :
    interface_(RakNetworkFactory::GetRakPeerInterface())    
{
    s_console.addFunction("listConnections",
                          ConsoleFun(this, &MasterServer::printConnections),
                          &fp_group_);
    s_console.addFunction("listServers",
                          ConsoleFun(this, &MasterServer::printServers),
                          &fp_group_);
    s_console.addFunction("populateList",
                          ConsoleFun(this, &MasterServer::generateTestData),
                          &fp_group_);


    // TODO CM MTU?
//    interface_->SetMTUSize(s_params.get<unsigned>("network.mtu_size"));
    interface_->SetUnreliableTimeout(UNRELIABLE_PACKET_TIMEOUT);    
}


//------------------------------------------------------------------------------
MasterServer::~MasterServer()
{
    interface_->DetachPlugin(nat_plugin_.get());
    interface_->Shutdown(0);
    RakNetworkFactory::DestroyRakPeerInterface(interface_);
}

//------------------------------------------------------------------------------
void MasterServer::start()
{
    interface_->SetMaximumIncomingConnections(s_params.get<unsigned>("master.max_nat_punchthrough_connections"));
    
    unsigned port = s_params.get<unsigned>("network.listen_port");
    
    SocketDescriptor desc(port, 0);
    bool success = interface_->Startup(s_params.get<unsigned>("master.max_nat_punchthrough_connections"),
                                       s_params.get<unsigned>("network.sleep_timer"),
                                       &desc, 1);
    if (success)
    {
        s_log << "Master server started.\n";
    } else
    {
        Exception e("Failed to start master server on port ");
        e << port << ".";
        throw e;
    }

    s_scheduler.addFrameTask(PeriodicTaskCallback(this, &MasterServer::handleNetwork),
                             "MasterServer::handleNetwork",
                             &fp_group_);


    nat_plugin_.reset(new NatPunchthroughServer());
    interface_->AttachPlugin(nat_plugin_.get());
}


//------------------------------------------------------------------------------
void MasterServer::handleNetwork(float dt)
{
    RakAutoPacket packet(interface_);
    while (packet.receive())
    {
        switch (packet->data[0])
        {
        case ID_CONNECTION_LOST:
        case ID_DISCONNECTION_NOTIFICATION:
            onDisconnect(packet->systemAddress);
            break;
        case ID_CONNECTION_ATTEMPT_FAILED:
            s_log << Log::debug('P')
                  << "failed to connect to "
                  << packet->systemAddress.ToString()
                  << "\n";
            onDisconnect(packet->systemAddress);
            break;
        case ID_NO_FREE_INCOMING_CONNECTIONS:
            s_log << Log::debug('P')
                  << "no free incoming connections on "
                  << packet->systemAddress.ToString()
                  << "\n";
            onDisconnect(packet->systemAddress);
            break;
        case ID_NEW_INCOMING_CONNECTION:
            s_log << Log::warning
                  << "incoming connection from "
                  << packet->systemAddress.ToString()
                  << " - closing right away\n";
            interface_->CloseConnection(packet->systemAddress, true);
            break;
        case ID_CONNECTION_REQUEST_ACCEPTED:
            onConnectionRequestAccepted(packet->systemAddress);
            break;
            
        case ID_ADVERTISE_SYSTEM:
            if (packet->data[1] == MPI_REQUEST_AUTH_TOKEN)
            {
                sendAuthToken(packet->systemAddress);
                
            } else if (packet->data[1] == MPI_REMOVE_SERVER)
            {
                
                RakNet::BitStream stream(&packet->data[2], packet->length-2, false);
                uint32_t token;
                stream.Read(token);

                uint16_t port = 0;
                stream.Read(port);

                s_log << Log::debug('H')
                      << "received MPI_REMOVE_SERVER from "
                      << packet->systemAddress.ToString()
                      << ", token "
                      << token
                      << ", internal port "
                      << port
                      << "...";
                
                AddressId id(packet->systemAddress, port);
                std::vector<GameServer>::iterator it = std::find_if(game_server_.begin(), game_server_.end(), id);

                if (it != game_server_.end() &&
                    token == it->last_token_)
                {
                    s_log << "removing.\n";
                    
                    delete (SystemAddress*)s_scheduler.removeTask(it->task_delete_, &fp_group_);
                    game_server_.erase(it);
                } else
                {
                    if (it != game_server_.end())
                    {
                        s_log << "but token is incorrect: got "
                              << token
                              << " expected "
                              << it->last_token_
                              << "\n";
                    } else s_log << "but server is not listed\n";
                }
                
            }  else if (checkAuthToken(packet))
            {
                // skip ID_ADVERTISE_SYSTEM, authentication token and
                // our packet id (6 byte total) in stream.
                RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
                uint32_t token;
                stream.Read(token);

                uint8_t id;
                stream.Read(id);

                switch (id)
                {
                case MPI_HEARTBEAT:
                    updateServer(packet->systemAddress, stream, token);
                    break;

                case MPI_REQUEST_SERVER_LIST:
                    uint8_t game, major, minor;
                    stream.Read(game);
                    stream.Read(major);
                    stream.Read(minor);
                    sendServerList(packet->systemAddress, VersionInfo(game, major, minor));
                    break;

                case MPI_REQUEST_NAT_PUNCHTHROUGH:
                    onNatPunchthroughRequested(packet->systemAddress, stream);
                    break;
                default:
                    s_log << Log::debug('N')
                          << "Unhandled packet with advertised id "
                          << (int)packet->data[5]
                          << " from "
                          << packet->systemAddress.ToString()
                          << "\n";
                }
            }
            break;
        default:
            defaultPacketAction(packet, interface_);
            break;
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Removes the given server from the server list after a certain
 *  timeout.
 */
void MasterServer::autoRemove(GameServer * server)
{
    AddressId id(server->external_address_, server->info_.address_.port);
        
    // Make sure this server is really in our list.
    assert(std::find_if(game_server_.begin(), game_server_.end(), id) != game_server_.end());
    
    if (server->task_delete_ == INVALID_TASK_HANDLE)
    {
        // The server has just joined our list, create its destruction
        // task.
        server->task_delete_ = s_scheduler.addEvent(
            SingleEventCallback(this, &MasterServer::removeServer),
            s_params.get<unsigned>("master.drop_server_delay"),
            new SystemAddress(id.getAddress()),
            std::string("MasterServer::removeServer(")+id.getAddress().ToString()+")",
            &fp_group_);
    } else
    {
        // Heartbeat, re-schedule destruction.
        s_scheduler.reschedule(server->task_delete_,
                               s_params.get<unsigned>("master.drop_server_delay"));
    }
}



//------------------------------------------------------------------------------
/**
 *  This function is scheduled to remove any servers which don't send
 *  heartbeats anymore.
 */
void MasterServer::removeServer(void * a)
{
    std::auto_ptr<SystemAddress> address((SystemAddress*)a);

    std::vector<GameServer>::iterator it = std::find_if(game_server_.begin(), game_server_.end(), AddressId(*address));
    assert (it != game_server_.end());

    if (it == game_server_.end())
    {
        s_log << Log::error
              << "removeServer called for nonexisting server "
              << address->ToString()
              << "\n";
        return;
    }
    
    s_log << Log::debug('H')
          << "Removed server "
          << address->ToString()
          << " from server list after timeout.\n";    
    
    game_server_.erase(it);
}



//------------------------------------------------------------------------------
/**
 *  Updates the server info for the specified address, creates a
 *  record if it doesn't exist yet.
 */
void MasterServer::updateServer(const SystemAddress & address,
                                RakNet::BitStream & args,
                                uint32_t token)
{
    ServerInfo info;
    if (!info.readFromBitstream(args))
    {
        s_log << Log::warning
              << "Bad server info from "
              << address.ToString()
              << ".\n";
        return;
    }
    
    // First see if a record already exists for this server. If so, we
    // need to update it.
    // search for external IP + internal port
    std::vector<GameServer>::iterator it = std::find_if(game_server_.begin(), game_server_.end(),
                                                        AddressId(address, info.address_.port));
    if (it == game_server_.end())
    {
        game_server_.push_back(GameServer());
        it = game_server_.end()-1;

        s_log << Log::debug('H')
              << "New server: "
              << info
              << " (external "
              << address.ToString()
              << ")";

        onNewServer();
    } else
    {
        s_log << Log::debug('H')
              << "Server status update: "
              << info;

        if (info.num_players_ > it->info_.num_players_) onNewPlayer();
    }

    s_log << ". Total number of servers: "
          << game_server_.size()
          << "\n";
    
    // now place the read data in our record.
    it->info_             = info;
    it->last_token_       = token; // needed to remove server...
    it->external_address_ = address;

    // Schedule the server for deletion so our list doesn't get
    // clogged with inactive servers.
    autoRemove(&(*it));    
}



//------------------------------------------------------------------------------
/**
 *  Sends a new auth token to the given address, stores the
 *  association in contact_, and schedules the association for deletion.
 */
void MasterServer::sendAuthToken(const SystemAddress & address)
{
    uint32_t token;
    
    std::vector<Contact>::iterator it = std::find(contact_.begin(), contact_.end(), address);
    if (it == contact_.end())
    {
        token = createTrueRandom();
        contact_.push_back(Contact(address, token));
    } else
    {
        token = it->auth_token_;
    }
    
    RakNet::BitStream args;

    args.Write((uint8_t)MPI_AUTH_TOKEN);
    args.Write(token);    
    
    interface_->AdvertiseSystem(address.ToString(false),
                                address.port,
                                (const char*)args.GetData(),
                                args.GetNumberOfBytesUsed());

    s_scheduler.addEvent(SingleEventCallback(this, &MasterServer::removeContact),
                         s_params.get<float>("master.clear_contact_delay"),
                         new SystemAddress(address),
                         std::string("MasterServer::removeContact(") + address.ToString() + ")",
                         &fp_group_);

    s_log << Log::debug('T')
          << "Sent auth token "
          << token
          << " to "
          << address.ToString()
          << "\n";    
}


//------------------------------------------------------------------------------
/**
 *  Returns true if we have an association of the packet's system
 *  address with the contained authentication token. Deletes this
 *  association afterwards.
 */ 
bool MasterServer::checkAuthToken(const Packet * packet)
{
    if (packet->length < 6) return false;
    RakNet::BitStream stream(&packet->data[1], sizeof(uint32_t), false);
    
    uint32_t token;
    stream.Read(token);
    
    std::vector<Contact>::iterator it = std::find(contact_.begin(), contact_.end(), packet->systemAddress);
    if (it == contact_.end())
    {
        s_log << Log::debug('T')
              << "Got auth token "
              << token
              << ", but expecting no contact from "
              << packet->systemAddress.ToString()
              << "\n";
        return false;
    }

    bool ret = it->auth_token_ == token;
    
    s_log << Log::debug('T')
          << "Got "
          << (ret ? "matching" : "MISmatching")
          << " auth token "
          << token
          << " from "
          << packet->systemAddress.ToString()
          << "\n";
    
    contact_.erase(it);
    
    return ret;
}


//------------------------------------------------------------------------------
/**
 *  If a host requests an authentication token but doesn't place any
 *  requests subsequently, it is removed form our contact list after a
 *  timeout.
 */
void MasterServer::removeContact(void * a)
{
    std::auto_ptr<SystemAddress> address((SystemAddress*)a);
    
    std::vector<Contact>::iterator it = std::find(contact_.begin(), contact_.end(), *address);
    if (it != contact_.end())
    {
        s_log << Log::debug('T')
              << "Timeout for contact "
              << address->ToString()
              << "\n";
    
        contact_.erase(it);
    }
}


//------------------------------------------------------------------------------
/**
 *  If any member of a nat punchthrough pair disconnects, remove other
 *  connection and nat punchthrough entry.
 */
void MasterServer::onDisconnect(const SystemAddress & address)
{
    s_log << Log::debug('P')
          << "connection to "
          << address.ToString()
          << " lost. Number of connections: "
          << interface_->NumberOfConnections()
          << "\n";


    // Cancel automatic disconnect task
    std::map<SystemAddress, hTask>::iterator it = task_disconnect_.find(address);
    if (it != task_disconnect_.end())
    {
        SystemAddress* ad = (SystemAddress*)s_scheduler.removeTask(it->second, &fp_group_);
        delete ad;
        task_disconnect_.erase(it);
    }


    // Remove nat_punchthrough_request_ entries, close connection to
    // other guy
    for (std::vector<NatRequest>::iterator it = nat_punchthrough_request_.begin();
         it != nat_punchthrough_request_.end();
         /* do nothing */)
    {
        if (it->server_ == address)
        {
            // cannot do anything with client after server
            // disconnected...
            if (interface_->IsConnected(it->client_, false))
            {
                interface_->CloseConnection(it->client_, true);
            } else
            {
                // client must be notified of failed attempt to
                // connect to server / server disconnect.
                //
                // this is signaled by the subsequent disconnect (when
                // client connects and we don't know what to do with
                // it...)
                interface_->Connect(it->client_.ToString(false),
                                    it->client_.port,
                                    0,0,0);
            }

            // See whether the server we just couldn't connect to
            // still is in our server list
            std::vector<GameServer>::iterator server_it =
                std::find_if(game_server_.begin(), game_server_.end(), std::bind2nd(ExternalAddressEqual(), it->server_));
            if (server_it != game_server_.end())
            {
                // assume server has gone off-line, remove from
                // server list.
                
                // remove timeout task first
                delete (SystemAddress*)s_scheduler.removeTask(server_it->task_delete_, &fp_group_);
                    
                game_server_.erase(server_it);
                s_log << Log::debug('P')
                      << "Removing "
                      << it->server_.ToString()
                      << " from server list after unsuccessful connection attempt.\n";
            }
            
            nat_punchthrough_request_.erase(it);
            
        } else if (it->client_ == address)
        {
            SystemAddress server = it->server_;
            nat_punchthrough_request_.erase(it);
            disconnectIfNoPendingPunches(server);
        } else
        {
            ++it;
        }
    }
}



//------------------------------------------------------------------------------
void MasterServer::onConnectionRequestAccepted(const SystemAddress & address)
{
    // make sure this connection will be closed again
    task_disconnect_[address] = s_scheduler.addEvent(SingleEventCallback(this, &MasterServer::closeConnection),
                                                     s_params.get<float>("master.max_connection_duration"),
                                                     new SystemAddress(address),
                                                     std::string("MasterServer::closeConnection(")
                                                     + address.ToString() + ")",
                                                     &fp_group_);
                         

    bool handled_connection = false;
    for (std::vector<NatRequest>::iterator it = nat_punchthrough_request_.begin();
         it != nat_punchthrough_request_.end();
         /* do nothing */)
    {
        if (it->server_ == address)
        {
            handled_connection = true;
            // Our connection attempt to the game server succeeded. As a
            // next step, we must connect to the client, which will
            // initiate NAT punchthrough if connection to the master
            // server is established.
            s_log << Log::debug('P')
                  << "server "
                  << address.ToString()
                  << " connected, connecting to client " << it->client_.ToString() << "\n";
        
            if (!interface_->Connect(it->client_.ToString(false),
                                     it->client_.port,
                                     0,0,0))
            {
                nat_punchthrough_request_.erase(it);
                disconnectIfNoPendingPunches(address);
                s_log << "FAILED!\n";
            } else
            {
                ++it;
            }
        } else if (it->client_ == address)
        {
            // now the client can initiate NAT punchthrough,
            // should disconnect afterwards.
            s_log << Log::debug('P')
                  << "connected to client "
                  << address.ToString()
                  << ", which requested NAT punchthrough to "
                  << it->server_.ToString()
                  << "\n";

            return;
        } else ++it;
    }

    if (!handled_connection)
    {
        s_log << Log::debug('P')
              << "connected to "
              << address.ToString()
              << " but don't know what to do with connection. disconnecting.\n";
        interface_->CloseConnection(address, true);
    } else
    {
        s_log << Log::debug('P')
              << "Number of current connections: "
              << interface_->NumberOfConnections()
              << "\n";

    }
}




//------------------------------------------------------------------------------
void MasterServer::onNatPunchthroughRequested(const SystemAddress & address,
                                              RakNet::BitStream & stream)
{
    SystemAddress server_address;
    stream.Read(server_address);

    
    s_log << Log::debug('P')
          << "Got nat punchthrough request for "
          << server_address.ToString()
          << " from "
          << address.ToString()
          << "\n";

    // don't handle this if request was already received
    if (std::find(nat_punchthrough_request_.begin(),
                  nat_punchthrough_request_.end(),
                  NatRequest(server_address, address)) != nat_punchthrough_request_.end())
    {
        s_log << Log::debug('P')
              << "already handling it...\n";
        return;
    }

    // check whether this server is in our server list
    std::vector<GameServer>::iterator server_it =
        std::find_if(game_server_.begin(), game_server_.end(), std::bind2nd(ExternalAddressEqual(), server_address));
    if (server_it == game_server_.end())
    {
        s_log << Log::debug('P')
              << "server is not known to us.\n";
        // connect & immediately disconnect to notify client
        interface_->Connect(address.ToString(false),
                            address.port,
                            0,0,0);
                            return;
    }
    
    

    if (interface_->IsConnected(server_address))
    {
        // We already are connected to the target server, reschedule
        // autodisconnect, and go to next step immediately (connect to
        // client)
        if (!interface_->Connect(address.ToString(false),
                                 address.port,
                                 0,0,0))
        {
            s_log << Log::debug('P')
                  << "FAILED to connect to requesting client "
                  << address.ToString()
                  << "\n";
            return;
        }

        std::map<SystemAddress, hTask>::iterator it = task_disconnect_.find(server_address);
        if (it != task_disconnect_.end())
        {
            s_scheduler.reschedule(it->second, s_params.get<float>("master.max_connection_duration"));
        }
        
        s_log << Log::debug('P')
              << "re-using existing connection to "
              << server_address.ToString()
              << "\n";
        
    } else
    {
        interface_->Connect(server_address.ToString(false),
                            server_address.port,
                            0,0,0);
    }

    nat_punchthrough_request_.push_back(NatRequest(server_address, address));
}


//------------------------------------------------------------------------------
void MasterServer::disconnectIfNoPendingPunches(const SystemAddress & server)
{
    for (std::vector<NatRequest>::iterator it = nat_punchthrough_request_.begin();
         it != nat_punchthrough_request_.end();
         ++it)
    {
        if (it->server_ == server) return;
    }

    interface_->CloseConnection(server, true);
}



//------------------------------------------------------------------------------
/**
 *  There is a time limit for any connection...
 */
void MasterServer::closeConnection(void * a)
{
    SystemAddress address = *((SystemAddress*)a);
    delete (SystemAddress*)a;

    std::map<SystemAddress, hTask>::iterator it = task_disconnect_.find(address);
    assert(it != task_disconnect_.end());
    task_disconnect_.erase(it);

    if (!interface_->IsConnected(address)) return;
    
    s_log << Log::debug('P')
          << "timeout for connection to "
          << address.ToString()
          << "\n";
    
    interface_->CloseConnection(address, true);
    onDisconnect(address);
    
}


//------------------------------------------------------------------------------
void MasterServer::sendServerList(const SystemAddress & address, const VersionInfo & client_version) const
{
    s_log << Log::debug('L')
          << address.ToString()
          << " requested a server list and has version "
          << client_version
          << ".\n";
    
    RakNet::BitStream args;

    bool greater_version_available = false;
    
    for (std::vector<GameServer>::const_iterator it = game_server_.begin();
         it != game_server_.end();
         ++it)
    {
        ServerInfo info = it->info_;
        const VersionInfo & server_version = info.version_;
        
        // bail immediately if game is different
        if (tolower(server_version.type_) != client_version.type_)
        {
            continue;
        }

        // See if client has lower version than this server, if
        // so, notify client.
        if (server_version.major_ > client_version.major_ ||
            (server_version.major_ == client_version.major_ &&
             server_version.minor_ > client_version.minor_))
        {
            greater_version_available = true;
            continue;
        }


        
        // Now we need to determine whether to send the local ip, and
        // whether nat punchthrough needs to be done....
        if (address.binaryAddress == it->external_address_.binaryAddress)
        {
            // Same subnet, send local IP
            s_log << Log::debug('L')
                  << "client is in same subnet as "
                  << it->external_address_.ToString()
                  << "\n";
        } else 
        {
            // report both external port (for NAT punchthrough) and
            // internal port ( for initial direct connection attempt)
            info.internal_port_ = it->info_.address_.port;

            // workaround: if game server and master server reside on
            // the same host, external IP will be 127.0.0.1, but
            // internal IP will be correct external one -> report
            // "internal" one.
            if (it->external_address_.binaryAddress == 0x0100007f)
            {
                info.address_       = it->info_.address_;
                s_log << Log::debug('L')
                      << "Reporting "
                      << info
                      << " instead of localhost.\n";
            } else
            {
                info.address_       = it->external_address_;

                s_log << Log::debug('L')
                      << "Reporting "
                      << info
                      << "\n";
            }
        }
        
        args.Write((uint8_t)MPI_SERVER_LIST);
        info.writeToBitstream(args);
        interface_->AdvertiseSystem(address.ToString(false),
                                    address.port,
                                    (const char*)args.GetData(),
                                    args.GetNumberOfBytesUsed());
        args.Reset();
    }

    
    if (greater_version_available)
    {
        s_log << "Notifying client of newer version.\n";
        args.Write((uint8_t)MPI_CUSTOM_MESSAGE);

        ServerInfo::writeString(args,
                                "There is a new version of ZB available. "
                                "Go to zeroballistics.com to download it.");
        
        interface_->AdvertiseSystem(address.ToString(false),
                                    address.port,
                                    (const char*)args.GetData(),
                                    args.GetNumberOfBytesUsed());
    }
}


//------------------------------------------------------------------------------
std::string MasterServer::printConnections (const std::vector<std::string>&args)
{
    s_log << "\nrequested NAT punchthroughs: \n";
    for (std::vector<NatRequest>::iterator it = nat_punchthrough_request_.begin();
         it != nat_punchthrough_request_.end();
         ++it)
    {
        s_log << it->server_.ToString()
              << " from "
              << it->client_.ToString()
              << "\n";
    }

    s_log << "Connections: \n";

    unsigned short num_connections;
    interface_->GetConnectionList(NULL, &num_connections);
    std::vector<SystemAddress> connections(num_connections);
    interface_->GetConnectionList(&connections[0], &num_connections);
    for (unsigned i=0; i<connections.size(); ++i)
    {
        s_log << connections[i].ToString() << "\n";
    }

    return "";
}


//------------------------------------------------------------------------------
std::string MasterServer::printServers(const std::vector<std::string>&)
{
    if (game_server_.empty()) return "No servers listed";
    
    std::ostringstream str;

    for (std::vector<GameServer>::const_iterator it =  game_server_.begin();
        it != game_server_.end();
        ++it)
    {
        str << it->info_
            << " (external "
            << it->external_address_.ToString()
            << ")\n";
    }
    
    return str.str();
}


//------------------------------------------------------------------------------
std::string MasterServer::generateTestData(const std::vector<std::string>&args)
{
    RakNet::BitStream stream;

    for (unsigned i=0; i<4000; ++i)
    {
        ServerInfo info;

        info.name_     = std::string("Testserver ") + toString(i);
        info.address_.binaryAddress = i;
        info.address_.port = i;
        
        info.writeToBitstream(stream);
        
        updateServer(info.address_, stream, 0);
        stream.Reset();
    }

    return "added lots of servers.";
}


//------------------------------------------------------------------------------
void MasterServer::onNewPlayer()
{
    try
    {
        system(s_params.get<std::string>("master.new_player_cmd").c_str());
    } catch (Exception & e) {}
}

//------------------------------------------------------------------------------
void MasterServer::onNewServer()
{
//     try
//     {
//         system(s_params.get<std::string>("master.new_server_cmd").c_str());
//     } catch (Exception & e) {}
}


} // namespace master

}

