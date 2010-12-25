
#include "NetworkServer.h"

#include <raknet/RakPeerInterface.h>
#include <raknet/RakNetworkFactory.h>
#include <raknet/RakNetTypes.h>
#include <raknet/RakNetStatistics.h>
#include <raknet/NatPunchthroughServer.h>

#include "physics/OdeSimulator.h"

#include "PuppetMasterServer.h"
#include "Log.h"
#include "NetworkCommandClient.h"
#include "NetworkCommandServer.h"
#include "GameLogicServer.h"
#include "Profiler.h"
#include "VariableWatcher.h"
#include "ParameterManager.h"
#include "RakAutoPacket.h"

#include "md5.h"

#include "RankingClientLogon.h"

using namespace network;

std::string SERVER_CONSOLE_STATE_FILE = "console_server.txt";


//------------------------------------------------------------------------------
NetworkServer::NetworkServer() :
    ServerInterface(AcceptVersionCallbackServer(this, &NetworkServer::acceptVersionCallback))
{

#ifdef ENABLE_DEV_FEATURES    
    s_console.addFunction("printNetStatistics",
                          ConsoleFun(this, &NetworkServer::printNetStatistics),
                          &fp_group_);
#endif

    NetworkCommand::initAccounting(&fp_group_);
    
    interface_->SetOccasionalPing(true); // need this for timestamping to work
    interface_->SetUnreliableTimeout(UNRELIABLE_PACKET_TIMEOUT);

    puppet_master_.reset(new PuppetMasterServer(interface_));
}

//------------------------------------------------------------------------------
NetworkServer::~NetworkServer()
{
    puppet_master_->reset(); // Destruction can trigger network
                             // messages, so do this before taking our
                             // interface down

    // let puppet master deregister raknet plugin...
    puppet_master_.reset(NULL);

    // TODO CM
//    interface_->DetachPlugin(nat_plugin_.get());
    interface_->Shutdown(300);

    if (s_params.get<bool>("server.log.print_network_summary"))
    {
        network::NetworkCommand::printAndResetNetSummary(std::vector<std::string>());
    }

#ifndef DEDICATED_SERVER    
    s_console.storeState(SERVER_CONSOLE_STATE_FILE);
#endif    
}


//------------------------------------------------------------------------------
/**
 *  Used for integrated servers: user already had to log in, uid and
 *  session key are known, use those for ranking server communication.
 */
void NetworkServer::setAuthData(uint32_t id, uint32_t session_key)
{
    puppet_master_->setAuthData(id, session_key);
}

//------------------------------------------------------------------------------
/**
 *  Used for dedicated servers: log on to ranking server with user and
 *  password stored in the configuration file.
 */
void NetworkServer::logon()
{
    network::ranking::ClientLogon * logon = new network::ranking::ClientLogon(
        s_params.get<std::string>("server.settings.login_name"),
        hashString(s_params.get<std::string>("server.settings.login_passwd")));


    // add Observer event for logon object
    logon->addObserver( ObserverCallbackFunUserData(this, &NetworkServer::onAuthorizationSuccessful),
                        network::ranking::CLOE_AUTHORIZATION_SUCCESSFUL,
                        &fp_group_);
    logon->addObserver( ObserverCallbackFunUserData(this, &NetworkServer::onAuthorizationFailed),
                        network::ranking::CLOE_CONNECT_FAILED,
                        &fp_group_);
    logon->addObserver( ObserverCallbackFunUserData(this, &NetworkServer::onAuthorizationFailed),
                        network::ranking::CLOE_AUTHORIZATION_FAILED,
                        &fp_group_);

    logon->connect();

    s_log << "Trying to contact ranking server...\n";
}


//------------------------------------------------------------------------------
PuppetMasterServer * NetworkServer::getPuppetMaster()
{
    return puppet_master_.get();
}


//------------------------------------------------------------------------------
void NetworkServer::start()
{
    ServerInterface::start("Game Server",
                           s_params.get<unsigned>("server.settings.listen_port"),
                           //extra slot for the master server connection (NAT punchthrough)                           
                           s_params.get<unsigned>("server.settings.max_connections") + 1, 
                           s_params.get<unsigned>("server.network.sleep_timer"),
                           0.0f,
                           s_params.get<unsigned>("server.network.mtu_size"),
                           NULL);

//    nat_plugin_.reset(new NatPunchthroughServer);
    // TODO CM fix nat punchthrough code
//    nat_plugin_->FacilitateConnections(false);
//    interface_->AttachPlugin(nat_plugin_.get());
    
    interface_->ApplyNetworkSimulator(s_params.get<float>   ("server.network.max_bps"),
                                      s_params.get<unsigned>("server.network.min_ping"),
                                      s_params.get<unsigned>("server.network.extra_ping"));
    

    s_scheduler.addTask(PeriodicTaskCallback(this, &NetworkServer::handlePhysics),
                        1.0f / s_params.get<float>("physics.fps"),
                        "NetworkServer::handlePhysics",
                        &fp_group_);

    s_scheduler.addTask(PeriodicTaskCallback(this, &NetworkServer::handleSendGameState),
                        1.0f / s_params.get<float>("server.network.send_gamestate_fps"),
                        "NetworkServer::handleSendGameState",
                        &fp_group_);    
}


//------------------------------------------------------------------------------
bool NetworkServer::handlePacket(Packet * packet)
{
    try
    {
        uint8_t packet_id = packet->data[0];

        if (packet_id == ID_TIMESTAMP)
        {
            size_t offset = sizeof(unsigned char) + sizeof(unsigned int);
            if (packet->length <= offset) return false;
            packet_id = packet->data[offset];
        }

        switch (packet_id)
        {
            // ---------- RakNet packets ----------            
        case ID_DISCONNECTION_NOTIFICATION:
            s_log << Log::millis << " Player ";
            puppet_master_->logPlayer(packet->systemAddress);
            s_log << " disconnected. Remaining players: "
                  << interface_->NumberOfConnections()
                  << "\n";
            puppet_master_->removePlayer(packet->systemAddress);
            break;
			
        case ID_CONNECTION_LOST:
            s_log << Log::millis << " Lost connection to  ";
            puppet_master_->logPlayer(packet->systemAddress);
            s_log << ". Remaining players: "
                  << interface_->NumberOfConnections()
                  << "\n";
            puppet_master_->removePlayer(packet->systemAddress);
            break;

        case ID_NEW_INCOMING_CONNECTION:
            s_log << Log::millis << " A new player connected: "
                  << packet->systemAddress
                  << ". Number of players: "
                  << interface_->NumberOfConnections()
                  << "\n";

            if (!puppet_master_->addPlayer(packet->systemAddress))
            {
                interface_->CloseConnection(packet->systemAddress, true);
            }
            break;
            
            // ---------- Own Packets ----------
        case TPI_READY:
            s_log << Log::debug('n')
                  << "Received ready from ";
            puppet_master_->logPlayer(packet->systemAddress);
            s_log << "\n";
            puppet_master_->playerReady(packet->systemAddress);
            break;

        case VHPI_VERSION_INFO:
        {
            VersionInfo info;
            RakNet::BitStream stream(&packet->data[1], packet->length-1, false);
            info.readFromBitstream(stream);
            s_log << packet->systemAddress
                  << " has version "
                  << info
                  << "\n";
            break;
        }
                
        default:
            std::auto_ptr<NetworkCommandClient> cmd(NetworkCommandClient::createFromPacket(packet_id, packet, interface_));
            if (cmd.get())
            {
                cmd->execute(puppet_master_.get());
                return true;
            } else
            {
                return false;
            }
        }

        return true;
        
    } catch (Exception & e)
    {
        e.addHistory("NetworkServer::handleNetwork");
        s_log << Log::error << e << "\n";
        emit(EE_EXCEPTION_CAUGHT, &e);
    }
    
    return false;
}


//------------------------------------------------------------------------------
std::string NetworkServer::getDetailedConnectionInfo(const SystemAddress & address) const
{
    ServerPlayer * player = puppet_master_->getPlayer(address);
    if (!player) return "Player doesn't exist!";
    
    return (ServerInterface::getDetailedConnectionInfo(address) +
            "\n\tName: " + player->getName() +
            "\n\tPing: " + toString((unsigned)(player->getNetworkDelay() * 1000.0f)) +
            "\n\tRcon Authorized:" + toString(player->isRconAuthorized()));
}
    


//------------------------------------------------------------------------------
void NetworkServer::handlePhysics(float dt)
{
    PROFILE(NetworkServer::handlePhysics);

    s_variable_watcher.frameMove();
    
    puppet_master_->frameMove(dt);    
}


//------------------------------------------------------------------------------
void NetworkServer::handleSendGameState(float dt)
{
    PROFILE(NetworkServer::handleSendGameState);
    
    puppet_master_->sendGameState();
}


//------------------------------------------------------------------------------
std::string NetworkServer::printNetStatistics(const std::vector<std::string> & args)
{
    if (args.size() != 1 &&
        args.size() != 2)
    {
        return "Args: playerNumber, verbosity(0-2)";
    }

    unsigned num;
    std::istringstream istr(args[0]);
    istr  >> num;

    SystemAddress id = interface_->GetSystemAddressFromIndex(num);

    if (id == UNASSIGNED_SYSTEM_ADDRESS)
    {
        return "Player doesn't exist";
    }

    RakNetStatistics * str = interface_->GetStatistics(id);
    
    unsigned verbosity =0;
    if (args.size() == 2)
    {
        std::istringstream istr2(args[1]);
        istr2 >> verbosity;
    }
    

    char buf[10000]; // AARFGGHLLL!!!!!!
    StatisticsToString(str, buf, verbosity);

    s_log << "\n" << buf <<"\n";
    
    return buf;
}



//------------------------------------------------------------------------------
network::ACCEPT_VERSION_CALLBACK_RESULT NetworkServer::acceptVersionCallback(const VersionInfo & version,
                                                                             VersionInfo & reported_version)
{
    reported_version = g_version;
    
    // accept only same game, same version, client
    VersionInfo cmp_version(tolower(g_version.type_), g_version.major_, g_version.minor_);
    if (cmp_version.type_ != version.type_) return AVCR_TYPE_MISMATCH;

    return cmp_version == version ? AVCR_ACCEPT : AVCR_VERSION_MISMATCH;
}

//------------------------------------------------------------------------------
void NetworkServer::onAuthorizationSuccessful(Observable*o, void*, unsigned)
{
    s_log << "Authorization succeeded.\n";
    
    start();
    
    network::ranking::ClientLogon * logon = (network::ranking::ClientLogon*)(o);
    puppet_master_->setAuthData(logon->getUserId(),
                                logon->getSessionKey());
}


//------------------------------------------------------------------------------
void NetworkServer::onAuthorizationFailed(Observable*ob, void*r, unsigned event)
{
    std::string msg = *(std::string*)(r);
    
    s_log << Log::error << msg << "\n"
          << Log::error << "Authorization with ranking server failed. Player stats will not be updated.\n";

    start();

    puppet_master_->setAuthData(0,0);
//    s_console.executeCommand("quit");
}

