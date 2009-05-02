
#include "NetworkServer.h"

#include <raknet/RakPeerInterface.h>
#include <raknet/RakPeerInterface.h>
#include <raknet/RakNetworkFactory.h>
#include <raknet/RakNetTypes.h>
#include <raknet/RakNetStatistics.h>
#include <raknet/NatPunchthrough.h>

#include <SDL/SDL.h>

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
#include "VersionInfo.h"


using namespace network;

std::string SERVER_CONSOLE_STATE_FILE = "console_server.txt";


//------------------------------------------------------------------------------
NetworkServer::NetworkServer() :
    interface_(RakNetworkFactory::GetRakPeerInterface())    
{

#ifdef ENABLE_DEV_FEATURES    
    s_console.addFunction("printNetStatistics",
                          ConsoleFun(this, &NetworkServer::printNetStatistics),
                          &fp_group_);
#endif

    interface_->SetMTUSize(s_params.get<unsigned>("server.network.mtu_size"));
    interface_->SetOccasionalPing(true); // need this for timestamping to work

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

    interface_->DetachPlugin(nat_plugin_.get());
    interface_->Shutdown(300);
    RakNetworkFactory::DestroyRakPeerInterface(interface_);

    if (s_params.get<bool>("server.log.print_network_summary")) network::NetworkCommand::logAccountingInfo();

#ifndef DEDICATED_SERVER    
    s_console.storeState(SERVER_CONSOLE_STATE_FILE);
#endif    
}

//------------------------------------------------------------------------------
void NetworkServer::start()
{
    SocketDescriptor desc(s_params.get<unsigned>("server.settings.listen_port"), 0);

    // Do this before startup or we might connect in just the instant
    // when it isn't set...
    // We need to allocate one extra slot for the master server connection (NAT punchthrough)
    interface_->SetMaximumIncomingConnections(s_params.get<unsigned>("server.settings.max_connections") +1);
    
    bool success = interface_->Startup(s_params.get<unsigned>("server.settings.max_connections") +1,
                                       s_params.get<unsigned>("server.network.sleep_timer"),
                                       &desc, 1);    
    if (success)
    {
        s_log << "Server started, waiting for connections on port "
              << desc.port
              << ".\n";
    } else
    {
        Exception e("Failed to start server on port ");
        e << desc.port << ".";
        throw e;
    }


    nat_plugin_.reset(new NatPunchthrough);
    nat_plugin_->FacilitateConnections(false);
    interface_->AttachPlugin(nat_plugin_.get());


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
    
    s_scheduler.addFrameTask(PeriodicTaskCallback(this, &NetworkServer::handleNetwork),
                             "NetworkServer::handleNetwork",
                             &fp_group_);
}


//------------------------------------------------------------------------------
PuppetMasterServer * NetworkServer::getPuppetMaster()
{
    return puppet_master_.get();
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
void NetworkServer::handleNetwork(float dt)
{
    try
    {
        PROFILE(NetworkServer::handleNetwork);

        RakAutoPacket p(interface_);    
        while (p.receive())
        {
            if (p->length == 0) continue;

            uint8_t packet_id = p->data[0];

            if (packet_id == ID_TIMESTAMP)
            {
                size_t offset = sizeof(unsigned char) + sizeof(unsigned int);
                if (p->length <= offset) continue;
                packet_id = p->data[offset];
            }

            switch (packet_id)
            {
                // ---------- RakNet packets ----------            
            case ID_NEW_INCOMING_CONNECTION:
            {
                network::VersionInfoCmd version_info(g_version);
                version_info.send(interface_, p->systemAddress, false);
            }
                break;
            
            case ID_DISCONNECTION_NOTIFICATION:
                s_log << "Player " << p->systemAddress << " disconnected.\n";
                puppet_master_->removePlayer(p->systemAddress);
                break;
			
            case ID_CONNECTION_LOST:
                s_log << "Lost connection to  " << p->systemAddress << "\n";
                puppet_master_->removePlayer(p->systemAddress);
                break;

            
                // ---------- Own Packets ----------
            case TPI_VERSION_ACK:
                s_log << "A new player connected: " << p->systemAddress << "\n";
                if (!puppet_master_->addPlayer(p->systemAddress))
                {
                    interface_->CloseConnection(p->systemAddress, true);
                }
                break;
            case TPI_READY:
                s_log << Log::debug('n')
                      << "Received ready from "
                      << p->systemAddress
                      << "\n";
                puppet_master_->playerReady(p->systemAddress);
                break;
            default:
                std::auto_ptr<NetworkCommandClient> cmd(NetworkCommandClient::createFromPacket(packet_id, p));
                if (cmd.get()) cmd->execute(puppet_master_.get());
            }		
        }
    } catch (Exception & e)
    {
        e.addHistory("NetworkServer::handleNetwork");
        s_log << Log::error << e << "\n";
        emit(EE_EXCEPTION_CAUGHT, &e);
    }
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
