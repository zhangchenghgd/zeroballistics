

#include "ServerList.h"

#include <limits>




#include <raknet/RakNetworkFactory.h>
#include <raknet/RakNetTypes.h>
#include <raknet/MessageIdentifiers.h>
#include <raknet/BitStream.h>
#include <raknet/GetTime.h>


#include "Exception.h"
#include "Log.h"
#include "ServerInfo.h"
#include "MessageIds.h"
#include "ParameterManager.h"

#include "RakAutoPacket.h"

#include "MasterServerRequest.h"
#include "VersionInfo.h"

#undef min
#undef max

namespace network
{
namespace master
{

const float    SCAN_PING_DT      = 1.0f;
const float    HANDLE_NETWORK_DT = 0.01f; ///< Must be low or latency is wrong
const unsigned SLEEP_TIMER       = 1;
const char *   BROADCAST_IP      = "255.255.255.255";


//------------------------------------------------------------------------------
ServerList::ServerList() :
    interface_(RakNetworkFactory::GetRakPeerInterface()),
    task_ping_lan_(INVALID_TASK_HANDLE),
    lan_scan_port_(0),
    query_active_(false)
{
    SocketDescriptor desc;
    bool attempt_succeded = interface_->Startup(1, SLEEP_TIMER, &desc, 1);

    if (!attempt_succeded) throw Exception("Unable to startup network interface");

    s_scheduler.addTask(PeriodicTaskCallback(this, &ServerList::handleNetwork),
                        HANDLE_NETWORK_DT,
                        "ServerList::handleNetwork",
                        &fp_group_);    

}


//------------------------------------------------------------------------------
ServerList::~ServerList()
{
    interface_->Shutdown(0);
    RakNetworkFactory::DestroyRakPeerInterface(interface_);
}


//------------------------------------------------------------------------------
void ServerList::queryMasterServer()
{
    reset();
    
    s_log << Log::debug('m')
          << "requesting auth token from master server\n";

    RakNet::BitStream args;
    args.Write((uint8_t)MPI_REQUEST_AUTH_TOKEN);

    MasterServerRequest * req;
    try
    {
        req = new MasterServerRequest(interface_,
                                      (const char*)args.GetData(), args.GetNumberOfBytesUsed(),
                                      MPI_AUTH_TOKEN);
    } catch (Exception & e)
    {
        emit(SLE_MASTER_SERVER_UNREACHABLE);
        return;
    }

    req->addObserver(ObserverCallbackFunUserData(this, &ServerList::onTokenReceived),
                     MSRE_RECEIVED_RESPONSE, &fp_group_);
    req->addObserver(ObserverCallbackFunUserData(this, &ServerList::onMasterContactFailed),
                     MSRE_CONTACT_FAILED, &fp_group_);

    query_active_ = true;
}


//------------------------------------------------------------------------------
/**
 *  Start sending LAN broadcast pings, if any server responds,
 *  directly add it to our server list.
 */
void ServerList::queryLan(unsigned port)
{
    reset();

    lan_scan_port_ = port;
    
    task_ping_lan_ = s_scheduler.addTask(PeriodicTaskCallback(this, &ServerList::handleLanPing),
                                         SCAN_PING_DT,
                                         "ServerList::handleLanPing",
                                         &fp_group_);

    query_active_ = true;
}


//------------------------------------------------------------------------------
void ServerList::cancelQueries()
{
    s_scheduler.removeTask(task_ping_lan_, &fp_group_);
    task_ping_lan_ = INVALID_TASK_HANDLE;
    query_active_  = false;
    lan_scan_port_ = 0;
}



//------------------------------------------------------------------------------
const std::vector<ListServerInfo> & ServerList::getInfoList() const
{
    return server_info_;
}

//------------------------------------------------------------------------------
void ServerList::reset()
{
    cancelQueries();
    server_info_.clear();
}


//------------------------------------------------------------------------------
void ServerList::handleNetwork(float dt)
{
    RakAutoPacket packet(interface_);
    while (packet.receive())
    {
        // Workaround because we cannot specify 0 possible connections.
        if (packet->length != 0 && packet->data[0] == ID_NEW_INCOMING_CONNECTION)
        {
            interface_->CloseConnection(packet->systemAddress, false);
        }

        if (packet->length < 2)
        {
            s_log << Log::debug('m')
                  << "dropped invalid packet\n";
            continue;
        }

        RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
        switch (packet->data[0])
        {
        case ID_PONG:
            handleLanServerPong(packet->systemAddress, stream);
            break;
        case ID_ADVERTISE_SYSTEM:
            uint8_t type;
            stream.Read(type);
                
            switch (packet->data[1])
            {
            case MPI_SERVER_LIST:
                s_log << Log::debug('m')
                      << "got server batch\n";
                processServerBatch(stream);
                break;
            case MPI_CUSTOM_MESSAGE:
            {
                std::string msg;
                ServerInfo::readString(stream, msg);
                emit(SLE_SERVER_MESSAGE, &msg);
            }   
                break;
            default:
                s_log << "ServerList::handleNetwork: unhandled advertise packet with id "
                      << (int)packet->data[1]
                      << " from "
                      << packet->systemAddress.ToString()
                      << ".\n";
                break;
            }
            break;
        default:
            s_log << "ServerList::handleNetwork: unhandled packet with id "
                  << (int)packet->data[0]
                  << " from "
                  << packet->systemAddress.ToString()
                  << ".\n";
                
            break;
        }
    }
}


//------------------------------------------------------------------------------
void ServerList::handleLanPing(float dt)
{
    assert (query_active_);
    
    SystemAddress ad;
    ad.SetBinaryAddress(BROADCAST_IP);
    ad.port = lan_scan_port_;

    s_log << Log::debug('m')
          << "Pinging "
          << ad.ToString()
          << "\n";
        
    interface_->Ping(ad.ToString(false),
                     ad.port,
                     false);

/*
unsigned num_servers = 3;
    for (unsigned i=0; i<num_servers; ++i)
    {
        ServerInfo info;

        info.name_     = std::string(toString(i) + " Testserver ");
        info.address_.binaryAddress = rand() % num_servers;
        info.address_.port = i;
        info.max_players_ = rand() % num_servers;
        info.num_players_ = rand() % num_servers;
        info.ping_ = rand() % num_servers;
        info.level_name_ = "almrausch";
        
        server_info_.push_back(info);
    }

    emit(SLE_FOUND_SERVER);
*/
}


//------------------------------------------------------------------------------
/**
 *  Server "pong" packets are handled here.
 */
void ServerList::handleLanServerPong(const SystemAddress & server_address,
                                     RakNet::BitStream & pong)
{
    if (!query_active_) return;
    
    // If we are scanning the LAN, there might be multiple servers
    // on it, so continue scanning and ignore this pong if we
    // already got it.
    if (find(server_info_.begin(), server_info_.end(), server_address) != server_info_.end()) return;
    
    
    RakNetTime time;
    if (!pong.Read(time)) return;

    ListServerInfo info;
    if (!info.readFromBitstream(pong)) return;    

    info.address_.binaryAddress = server_address.binaryAddress; // XXXXXX don't overwrite port; better solution?
    info.ping_ = (unsigned)(RakNet::GetTime()-time);
    
    s_log// << Log::debug('l')
          << "Adding "
          << info
          << " to server list.\n";

    server_info_.push_back(info);
    
    emit(SLE_FOUND_SERVER);
}



//------------------------------------------------------------------------------
void ServerList::sendRequestServerList(uint32_t token)
{
    s_log << Log::debug('m')
          << "sent server list request.\n";
    RakNet::BitStream args;
    args.Write(token);
    args.Write((uint8_t)MPI_REQUEST_SERVER_LIST);

    args.Write(g_version.game_);
    args.Write(g_version.major_);
    args.Write(g_version.minor_);
    
    interface_->AdvertiseSystem(s_params.get<std::string>("master_server.host").c_str(),
                                s_params.get<unsigned>   ("master_server.port"),
                                (const char*)args.GetData(),
                                args.GetNumberOfBytesUsed());    
}



//------------------------------------------------------------------------------
/**
 *  Got some servers from the master server...
 */
void ServerList::processServerBatch(RakNet::BitStream & stream)
{
    if (!query_active_) return;

    ListServerInfo info;
    if (info.readFromBitstream(stream))
    {
        server_info_.push_back(info);
        emit(SLE_FOUND_SERVER);
    } else
    {
        s_log << "invalid server info received.\n";
    }
}

//------------------------------------------------------------------------------
void ServerList::onTokenReceived(Observable*, void* s, unsigned)
{
    RakNet::BitStream & stream = *(RakNet::BitStream*)s;

    // ignore message id
    stream.IgnoreBytes(1);
    
    uint32_t token;
    stream.Read(token);
    sendRequestServerList(token);    
}



//------------------------------------------------------------------------------
void ServerList::onMasterContactFailed(Observable*, void * s, unsigned)
{
    emit(SLE_MASTER_SERVER_UNREACHABLE);
}




} // namespace master
} // namespace network
