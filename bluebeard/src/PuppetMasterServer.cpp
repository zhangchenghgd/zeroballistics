
#include "PuppetMasterServer.h"

#include <boost/filesystem.hpp>

#include <raknet/RakNetTypes.h>
#include <raknet/GetTime.h>
#include <raknet/RakPeerInterface.h>

#include "GameState.h"
#include "NetworkCommandServer.h"
#include "GameLogicServer.h"
#include "ClassLoader.h"
#include "ParameterManager.h"

#include "physics/OdeSimulator.h"

#include "MasterServerRegistrator.h"

#include "Paths.h" // for level autocompletion

#include "RegEx.h"
#include "VersionInfo.h"

//------------------------------------------------------------------------------
PuppetMasterServer::PuppetMasterServer(RakPeerInterface * server_interface) :
    game_state_(new GameState()),
    interface_(server_interface)
{
    try
    {
        master_server_registrator_.reset(new network::master::MasterServerRegistrator());
        server_interface->AttachPlugin(master_server_registrator_.get());
    } catch (Exception & e)
    {
        e.addHistory("PuppetMasterServer::PuppetMasterServer");
        s_log << Log::warning
              << e
              << "\n";
    }
    

    s_console.addFunction("loadLevel",           ConsoleFun(this, &PuppetMasterServer::loadLevelConsole),
                          &fp_group_,
                          ConsoleCompletionFun(this, &PuppetMasterServer::loadLevelConsoleCompletion));
    s_console.addFunction("listConnections",     ConsoleFun(this, &PuppetMasterServer::listConnections),
                          &fp_group_);
    s_console.addFunction("kick",                ConsoleFun(this, &PuppetMasterServer::kick),
                          &fp_group_);
    s_console.addFunction("say",                 ConsoleFun(this, &PuppetMasterServer::say),
                          &fp_group_);

}

//------------------------------------------------------------------------------
PuppetMasterServer::~PuppetMasterServer()
{
    interface_->DetachPlugin(master_server_registrator_.get());
    assert(game_state_->getGameObjects().empty());
    game_state_.reset(NULL);
}

//------------------------------------------------------------------------------
/**
 *  Resets all data specific to the currently loaded level, but leaves
 *  intact team assignments, team scores etc. Should to the same
 *  things as PuppetMasterClient::reset.
 */
void PuppetMasterServer::reset()
{
    level_name_ = "";
    logic_type_ = "";
    
    for (PlayerContainer::iterator it = player_.begin();
         it != player_.end();
         ++it)
    {
        it->setControllable(NULL);
    }

    game_state_->reset();
    game_logic_.reset(NULL);


    // Send reset cmd to every player
    network::SimpleCmd cmd_reset(network::TPI_RESET_GAME);
    cmd_reset.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);
}

//------------------------------------------------------------------------------
/**
 *  First removes all GameObjects scheduled for deletion from our
 *  gamestate.
 *
 *  Then steps all players, sending correcting states to the clients
 *  when neccessary.
 *
 *  Finally steps game logic and game state.
 */
void PuppetMasterServer::frameMove(float dt)
{
    deleteScheduledObjects();

    if (!player_.empty() && player_.begin()->getControllable())
    {
        /*
        s_log << Log::debug('b')
              << "at start of frame with forward "
              << player_.begin()->getControllable()->getPlayerInput().up_
              << " with pos "
              << player_.begin()->getControllable()->getPosition().z_
              << "\n";
        */
    }


    ADD_STATIC_CONSOLE_VAR(bool, sending_correction, false);
    sending_correction = false;
    for (PlayerContainer::iterator it = player_.begin();
         it != player_.end();
         ++it)
    {
        if (!it->getControllable()) continue;

        uint8_t seq_number;
        if (it->handleInput(seq_number))
        {
            // We must send the correction for the returned sequence
            // number
            
//             s_log << Log::debug('b')
//                   << "Sending correction for state \t\t\t\t"
//                   << (unsigned)seq_number
//                   << " with position " << it->getControllable()->getPosition().z_
//                   << "\n";
            sending_correction = true;
            
            network::SetControllableStateCmd controllable_state_cmd(seq_number,
                                                                    it->getControllable());
            controllable_state_cmd.send(interface_, it->getId(), false);            
        }
    }

    game_state_->frameMove(dt);
}

//------------------------------------------------------------------------------
/**
 *  Called when a client input packet reaches the server.
 *
 *  After sanity checks (does player exist, drop packets received
 *  twice, drop out-of-order packets) enqueue the input for
 *  simulation.
 *
 *  \param id The originating client's id.
 *  \param input The player input at the time the packet was sent.
 *  \param sequence_number The running number of the packet.
 */
void PuppetMasterServer::handleInput(const SystemAddress & id,
                                     const PlayerInput & input,
                                     uint8_t sequence_number,
                                     uint32_t timestamp)
{
    ServerPlayer * player = getPlayer(id);

    if (!player)
    {
        s_log << Log::warning
              << "GameState::inputReceived: Tried to set input for nonexisting player "
              << id << "\n";
        return;
    }

    uint32_t cur_time = RakNet::GetTime();
    if (cur_time > timestamp)
    {
        player->setNetworkDelay((float)(cur_time - timestamp) * 0.001f);
    }


    if(player->getControllable() == NULL)
    {
        s_log << Log::debug('n')
              << "input from client still arriving, controllable == NULL \n";
        return;   
    }

    game_logic_->handleInput(player, input);

    player->enqueueInput(sequence_number, input);
}


//------------------------------------------------------------------------------
/**
 *  Broadcasts the current game state to all connected clients.
 *
 *
 */
void PuppetMasterServer::sendGameState()
{
    ADD_STATIC_CONSOLE_VAR(bool, send_gamestate, true);
    if (!send_gamestate) return;

    
    // Traverse all game objects and send their state
    for (GameState::GameObjectContainer::const_iterator it =
             game_state_->getGameObjects().begin();
         it != game_state_->getGameObjects().end();
         ++it)
    {   
        RigidBody * rigid_body = dynamic_cast<RigidBody*>(it->second); /// XXXX ugly dynamic cast here??

        if (!rigid_body)
        {
            s_log << Log::error
                  << "Not a rigidbody in PuppetMasterServer::sendGameState!! go: "
                  << *it->second
                  << "\n";
            continue;
        }

        if (it->second->isDirty())
        {
            // Send extra state to all players
            s_log << Log::debug('n')
                  << "Sending extra state for "
                  << *rigid_body
                  << "\n";
            network::SetGameObjectStateCmd cmd_extra(it->second, OST_EXTRA);
            cmd_extra.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);

            it->second->clearDirty();
        }
            
        // Skip sleeping objects.
        if (rigid_body->isSleeping()) continue;
        
        // Don't send object state to owner, as this would mess up
        // client side prediction. This will be handled with a
        // SetControllableStateCmd.
        SystemAddress exclude_id = UNASSIGNED_SYSTEM_ADDRESS;
        if (dynamic_cast<Controllable*>(it->second)) exclude_id = it->second->getOwner();

        network::SetGameObjectStateCmd cmd_core(it->second, OST_CORE);
        sendNetworkCommand(cmd_core, exclude_id, CST_BROADCAST_READY);
    }
}



//------------------------------------------------------------------------------
/**
 *  Sets the object controlled by a particular player. Sends the
 *  change to the client.
 */
void PuppetMasterServer::setControllable(const SystemAddress & id,
                                         Controllable * controllable)
{
    s_log << Log::debug('l')
          << "Executing setControllable for "
          << id
          << " to ";
    if (controllable) s_log << *controllable;
    else s_log << "NULL";
    s_log << "\n";
    
    ServerPlayer * player = getPlayer(id);
    if (!player)
    {
        s_log << Log::warning << "Couldn't find player "
              << id << " in PuppetMasterServer::setControllable\n";
        return;
    }

    if (controllable && player->getControllable() == controllable)
    {
        s_log << Log::warning
              << "Controllable "
              << *controllable
              << " was already set in PuppetMasterServer::setControllable()\n";
        return;
    }
    
    network::SetControllableCmd set_cmd(id, controllable);
    set_cmd.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);

    // Called *after* sending the network command to avoid changing
    // object id of controllable
    player->setControllable(controllable);
}


//------------------------------------------------------------------------------
/**
 *  Transmits the current gamestate to the newly connected
 *  player. Player must send a ready message to complete the
 *  connection.
 *
 *  \return False if for some reason players cannot / should not
 *  connect at the moment (e.g. no loaded level)
 */
bool PuppetMasterServer::addPlayer(const SystemAddress & pid)
{
    // We need to do this manually because there is one additional
    // open slot for master server conenctions.
    if (player_.size() >= s_params.get<unsigned>("server.settings.max_connections"))
    {
        s_log << Log::warning
              << "Rejecting player because server is full.\n";
        
        char m = ID_NO_FREE_INCOMING_CONNECTIONS;
        interface_->Send(&m, 1,
                         MEDIUM_PRIORITY, RELIABLE,
                         0, pid, false);
        return false;
    }
    
    assert(game_logic_.get());
    
    if (level_name_.empty())
    {
        s_log << Log::warning
              << "Rejecting player because no level is loaded.\n";
        return false;
    }

    // If the player connected before he could still be around as
    // zombie, so delete him.
    removePlayer(pid);
    
    // Send level name to new client -> precache loading
    network::LoadLevelCmd level_name_cmd(level_name_, logic_type_);
    level_name_cmd.send(interface_, pid, false);
    
    
    // Send complete gamestate to newly connected player
    for (GameState::GameObjectContainer::const_iterator it =
             game_state_->getGameObjects().begin();
         it != game_state_->getGameObjects().end();
         ++it)
    {
        network::CreateGameObjectCmd create_object_cmd(it->second);    
        create_object_cmd.send(interface_, pid, false);
    }

    // Send existing players to new player
    for (PlayerContainer::iterator it = player_.begin();
        it != player_.end();
        ++it)
    {
        network::CreatePlayerCmd cmd(it->getId());
        cmd.send(interface_, pid, false);

        if (it->getControllable())
        {
            network::SetControllableCmd set_controllable_cmd(it->getId(),
                                                             it->getControllable());
            set_controllable_cmd.send(interface_, pid, false);
        }

        network::StringMessageCmd name_cmd(network::SMT_PLAYER_NAME,
                                           it->getName(),
                                           it->getId());
        name_cmd.send(interface_, pid, false);        
    }

    player_.push_back(ServerPlayer(pid));

    // Inform all others of the new player
    network::CreatePlayerCmd create_player_cmd(pid);
    create_player_cmd.send(interface_, pid, true);

    game_logic_->addPlayer(&player_.back(), true);

    updateServerInfo();

    return true;
}

//------------------------------------------------------------------------------
void PuppetMasterServer::requestPlayerReady(const SystemAddress & pid)
{
    s_log << Log::debug('l')
          << "Requesting player ready from "
          << pid
          << "\n";
    
    ServerPlayer * player = getPlayer(pid);
    assert(player);
    
    player->incNeededReadies();

    network::SimpleCmd cmd(network::TPI_REQUEST_READY);
    cmd.send(interface_, pid, false);
    
}


//------------------------------------------------------------------------------
void PuppetMasterServer::playerReady(const SystemAddress & pid)
{
    ServerPlayer * player = getPlayer(pid);
    if (!player)
    {
        s_log << Log::warning
              << "Received player ready for nonexisting player "
              << pid
              << "\n";
    }

    if (player->getNeededReadies() == 0)
    {
        s_log << Log::warning
              << "Received player ready for already ready player "
              << pid
              << ".\n";
    }

    player->decNeededReadies();
    if (player->getNeededReadies() != 0)
    {
        s_log << Log::debug('n')
              << "Player "
              << pid
              << " still needs "
              << player->getNeededReadies()
              << " readies.\n";

        return;
    }
}


//------------------------------------------------------------------------------
/**
 *  Inform all clients that a player left.
 *  Inform the game logic that a player left.
 *
 *  Removes player from rcon authorization list.
 *
 */
void PuppetMasterServer::removePlayer(const SystemAddress & pid)
{
    assert(game_logic_.get());
    
    ServerPlayer * player = getPlayer(pid);

    if (!player)
    {
        s_log << Log::debug('n') << "Removed nonexisting player " << pid << "\n";
        return;
    }
     
    game_logic_->removePlayer(pid);

    network::DeletePlayerCmd delete_player_cmd(pid);
    delete_player_cmd.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);
    
    PlayerContainer::iterator it;
    for (it = player_.begin(); it != player_.end(); it++)
    {
        if(it->getId() == pid)
        {
            player_.erase(it);
            updateServerInfo();    
            return;
        }
    }
}

//------------------------------------------------------------------------------
ServerPlayer * PuppetMasterServer::getPlayer(const SystemAddress & id)
{
    for (PlayerContainer::iterator it = player_.begin();
         it != player_.end();
         ++it)
    {
        if (it->getId() == id) return &(*it);
    }

    return NULL;
}


//------------------------------------------------------------------------------
PuppetMasterServer::PlayerContainer & PuppetMasterServer::getPlayers()
{
    return player_;
}

//------------------------------------------------------------------------------
void PuppetMasterServer::setPlayerName(const SystemAddress & id, const std::string & name)
{
    if (level_name_.empty()) return;
   
    RegEx reg_ex(PLAYER_NAME_REGEX);

    ServerPlayer * player = getPlayer(id);
    if (!player)
    {
        s_log << Log::warning << "Couldn't find player "
              << id
              << " in PuppetMasterServer::setPlayerName()\n";
        
        return;
    }

    // new player sends first name, handle different
    if (player->getName().empty())
    {
        // check existing player name
        if(reg_ex.match(name))
        {
            player->setName(name);
        }
        else
        {
            player->setName("Player");
        }

        s_log << "New player has name "
              << player->getName()
              << "\n";

    }
    else // existing player name change
    {
        // check if names are the same
        if(player->getName() == name) return;

        // check requested name
        if(reg_ex.match(name))
        {
            s_log << player->getName()
                  << " changed his name to "
                  << name
                  << "\n";

            player->setName(name);
        }
    }

    network::StringMessageCmd cmd(network::SMT_PLAYER_NAME, player->getName(), id);
    cmd.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);
}


//------------------------------------------------------------------------------
GameState * PuppetMasterServer::getGameState()
{
    return game_state_.get();
}

//------------------------------------------------------------------------------
GameLogicServer * PuppetMasterServer::getGameLogic()
{
    assert(game_logic_.get());
    
    return game_logic_.get();
}



//------------------------------------------------------------------------------
/**
 *   Checks players rcon passwords and authorizes them,
 *   executes rcon commands directly over console.executeCommand
 */
void PuppetMasterServer::rcon(const SystemAddress & id, std::string & cmd_and_args)
{
    std::string rcon_return_value;
    std::string passwd_cmd = "passwd";
    PlayerContainer::iterator it;

    try
    {
        // parse rcon for passwd cmd
        Tokenizer tokenizer(cmd_and_args.c_str(),' ');
        if (tokenizer.getNextWord() == passwd_cmd)
        {
            if (tokenizer.getRemainingString() == s_params.get<std::string>("server.puppet_master.rcon_passwd"))
            {
                // loop through connected players
                for(it = player_.begin();it != player_.end(); it++)
                {
                    // if player is found, set rcon authorized true
                    if(it->getId() == id)
                    {  
                        it->setRconAuthorized(true);
                        s_log << " player: " << id << " added to rcon list\n";
                        network::StringMessageCmd resp(network::SMT_RCON_RESPONSE,"authorization successful.",
                                                               UNASSIGNED_SYSTEM_ADDRESS);
                        resp.send(interface_,id,false);
                        return;
                    }
                }
            }
            else
            {
                network::StringMessageCmd resp(network::SMT_RCON_RESPONSE,"wrong password, authorization failed.",
                                                       UNASSIGNED_SYSTEM_ADDRESS);
                resp.send(interface_,id,false);
                return;
            }
        }
    }
    catch (Exception & e)
    {
        network::StringMessageCmd resp(network::SMT_RCON_RESPONSE,"No password specified.", UNASSIGNED_SYSTEM_ADDRESS);
        resp.send(interface_,id,false);
        s_log << Log::error << e << "\n";
        return;
    }

    // see if player is already authorized
    for(it = player_.begin();it != player_.end(); it++)
    {
        // if player is authorized execute rcon cmd
        if(it->getId() == id && it->isRconAuthorized())
        {   
            rcon_return_value = s_console.executeCommand(cmd_and_args.c_str());

            // print cmd and return values of command 
            s_log << "rcon: " << cmd_and_args.c_str() << " executed from player: " << id << "\n";
            s_log << "rcon: " << rcon_return_value.c_str() << "\n";

            // and send response to client
            network::StringMessageCmd resp(network::SMT_RCON_RESPONSE, rcon_return_value, UNASSIGNED_SYSTEM_ADDRESS);
            resp.send(interface_,id,false);
            return;
        }
    }
    
    network::StringMessageCmd resp(network::SMT_RCON_RESPONSE, "You are not authorized to execute rcon commands.\n"
                                           "Set password with rcon passwd.", UNASSIGNED_SYSTEM_ADDRESS);
    resp.send(interface_,id,false);
}


//------------------------------------------------------------------------------
/**
 *  Relays chat commands.
 */
void PuppetMasterServer::chat(const SystemAddress & id, std::string & msg)
{
    ServerPlayer * player = getPlayer(id);
    if (player)
    {
        if(!player->allowAnnoyingClientRequest())
        {
            s_log << "blocking flooder: " << player->getName() << ": " << msg << "\n";
            return;
        }

        s_log << Log::millis << "server chat relay: " << player->getName() << ": " << msg << "\n";
        player->incAnnoyingClientRequest();
    }
    else
    {
        s_log << Log::warning << "Relaying chat for unknown player "
              << id
              << "\n";
    }

    network::StringMessageCmd cmd(network::SMT_CHAT, msg, id);
    cmd.send(interface_, id, true);

}


//------------------------------------------------------------------------------
/**
 *  Adds the object to gamestate, sends creation command to clients,
 *  registers this as collision handler
 */
void PuppetMasterServer::addGameObject(GameObject * object)
{
    assert(game_logic_.get());
    
    game_state_->addGameObject(object);
    game_logic_->onGameObjectAdded(object);

    
    network::CreateGameObjectCmd create_cmd(object);
    create_cmd.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);


    object->addObserver(ObserverCallbackFun2(this, &PuppetMasterServer::sendReliableState),
                        RBE_GO_TO_SLEEP,
                        &fp_group_);
    

    s_log << Log::debug('n')
          << "PuppetMasterServer::addGameObject executed. " << *object
          << ", owner: " << object->getOwner()
          << "\n";    
}

//------------------------------------------------------------------------------
bool PuppetMasterServer::existsPlayer(const SystemAddress & id) const
{
    for (PlayerContainer::const_iterator it = player_.begin();
         it != player_.end();
         ++it)
    {
        if (it->getId() == id) return true;
    }

    return false;
}



//------------------------------------------------------------------------------
/**
 *  In order to make this function registrable as scheduler event
 *  callback, takes a pointer to a new'ed HostOptions struct, which
 *  will be deleted here.
 */
void PuppetMasterServer::loadLevel(void * opts)
{
    reset();

    HostOptions options = *(HostOptions*)opts;
    delete (HostOptions*)opts;

    game_logic_.reset(s_server_logic_loader.create(std::string("GameLogicServer") + options.game_logic_type_));
    game_logic_->init(this);
    
    level_name_ = options.level_name_;
    logic_type_ = options.game_logic_type_;

    s_log << "Loading "
          << level_name_
          << " ("
          << logic_type_
          << ")\n";
    
    // Send level name to new client -> precache loading
    network::LoadLevelCmd level_name_cmd(level_name_, options.game_logic_type_);
    level_name_cmd.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);

    try
    {
        game_logic_->loadLevel(level_name_);
    } catch (Exception & e)
    {
        reset();
        e.addHistory("PuppetMasterServer::loadMap(" + level_name_ + ", " + options.game_logic_type_ + ")");
        throw e;
    }


    // re-add existing players to logic
    for (PlayerContainer::const_iterator it = player_.begin();
         it != player_.end(); ++it)
    {
        game_logic_->addPlayer(&(*it), false);
    }

    
    updateServerInfo();

    emit(PMOE_LEVEL_LOADED);
}

//------------------------------------------------------------------------------
/**
 *  \param id Same meaning as in raknet (target for single, exclude
 *  for broadcast).
 */
void PuppetMasterServer::sendNetworkCommand(network::NetworkCommandServer & cmd,
                                            const SystemAddress & id,
                                            COMMAND_SEND_TYPE type)
{
    switch(type)
    {
    case CST_SINGLE:
        cmd.send(interface_, id, false);
        break;
    case CST_BROADCAST_ALL:
        cmd.send(interface_, id, true);
        break;
    case CST_BROADCAST_READY:
        for (PlayerContainer::iterator cur_player = player_.begin();
             cur_player != player_.end();
             ++cur_player)
        {            
            if (cur_player->getNeededReadies() != 0 ) continue;
            if (cur_player->getId()            == id) continue;

            cmd.send(interface_, cur_player->getId(), false);
        }
    }
}

//------------------------------------------------------------------------------
void PuppetMasterServer::onGameFinished()
{
    emit(PMOE_GAME_FINISHED);
}


//------------------------------------------------------------------------------
/**
 *  Gets called when a rigidbody goes to sleep. Send one last reliable state.
 */
void PuppetMasterServer::sendReliableState(Observable* observable, unsigned event)
{
    assert(event == RBE_GO_TO_SLEEP);
    assert(dynamic_cast<RigidBody*>(observable));

    RigidBody * rigid_body = (RigidBody*)observable;

    s_log << Log::debug('l')
          << "Sending last reliable state for "
          << *rigid_body
          << "\n";
    
    // OST_BOTH makes packet reliable
    network::SetGameObjectStateCmd cmd_core((GameObject*)rigid_body, OST_BOTH);

    // This needs to go to ALL players, even the ones not ready.
    cmd_core.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);
}


//------------------------------------------------------------------------------
/**
 *  Deletes all objects flagged previously for deletion, sending the
 *  deletion command over the network as well.
 */
void PuppetMasterServer::deleteScheduledObjects()
{
    std::vector<GameObject*> objects_to_be_deleted;

    // Traverse all game objects and find the ones that are scheduled for deletion
    for (GameState::GameObjectContainer::const_iterator it =
             game_state_->getGameObjects().begin();
         it != game_state_->getGameObjects().end();
         ++it)
    {
        if (it->second->isScheduledForDeletion())
        {
            objects_to_be_deleted.push_back(it->second);          
        }
    }

    // actually delete the scheduled objects
    for (std::vector<GameObject*>::const_iterator it =
             objects_to_be_deleted.begin();
         it != objects_to_be_deleted.end();
         ++it)
    {
        uint16_t id_to_delete = (*it)->getId();
        
        game_state_->deleteGameObject(id_to_delete);

        network::DeleteGameObjectCmd delete_cmd(id_to_delete);
        delete_cmd.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);
    }
}

//------------------------------------------------------------------------------
/**
 *  Console function for loading level.
 */
std::string PuppetMasterServer::loadLevelConsole(const std::vector<std::string> & args)
{
    HostOptions * options = new HostOptions;

    if (args.size() >= 1) options->level_name_ = args[0];
    else                  options->level_name_ = level_name_;

    if (args.size() == 2) options->game_logic_type_ = args[1];
    else                  options->game_logic_type_ = logic_type_;

    loadLevel(options);

    if (args.size() != 1) return "Reloaded current level";
    else return std::string("Loaded level ") + args[0];
}

//------------------------------------------------------------------------------
std::vector<std::string> PuppetMasterServer::loadLevelConsoleCompletion(const std::vector<std::string> & args)
{
    using namespace boost::filesystem;
    std::vector<std::string> ret;

    try
    {
        if (args.size() > 1) return ret;

        path lvl_path(LEVEL_PATH);
        for (directory_iterator it(lvl_path);
             it != directory_iterator();
             ++it)
        {
            std::string name = it->path().leaf();        
            ret.push_back(name);
        }

        if (args.size()==1) Console::filterPrefix(args[0], ret);
    
        return ret;
        
    } catch (basic_filesystem_error<path> & be)
    {
        return ret;
    }
}


//------------------------------------------------------------------------------
std::string PuppetMasterServer::listConnections (const std::vector<std::string> & args)
{
    if (!args.empty())   return "Function takes no arguments.\n";
    if (player_.empty()) return "No players are connected at the moment.";
    
    std::ostringstream strstr;
    for (PlayerContainer::const_iterator it = player_.begin();
         it != player_.end();
         ++it)
    {
        strstr << it->getId().ToString()
               << ":\n\tName: "
               << it->getName()
               << "\n\tPing: "
               << (unsigned)(it->getNetworkDelay() * 1000.0f)
               << "\n\tRcon Authorized:"
               << it->isRconAuthorized()
               << "\n";
    }

    return strstr.str();
}


//------------------------------------------------------------------------------
std::string PuppetMasterServer::kick(const std::vector<std::string> & args)
{
    if (args.size() != 2) return "Usage: kick IP port (use listConnections)";

    SystemAddress kicked_address;
    kicked_address.SetBinaryAddress(args[0].c_str());
    kicked_address.port = fromString<unsigned short>(args[1]);

    if (!interface_->IsConnected(kicked_address))
    {
        return args[0] + ":" + args[1] + " is not connected.\n";
    }

    network::SimpleCmd cmd_reset(network::TPI_KICK);
    cmd_reset.send(interface_, kicked_address, false);
    
    interface_->CloseConnection(kicked_address, true);
    return "";
}


//------------------------------------------------------------------------------
std::string PuppetMasterServer::say(const std::vector<std::string> & args)
{
    if (args.empty()) return "";

    std::string msg = args[0];
    for (unsigned i=1; i<args.size(); ++i)
    {
        msg += " " + args[i];
    }

    network::StringMessageCmd cmd(network::SMT_CHAT, msg, UNASSIGNED_SYSTEM_ADDRESS);
    cmd.send(interface_, UNASSIGNED_SYSTEM_ADDRESS, true);

    return "";
}


//------------------------------------------------------------------------------
void PuppetMasterServer::updateServerInfo()
{
    network::master::ServerInfo info;

    info.name_          = s_params.get<std::string>("server.settings.name");
    info.version_       = g_version;
    info.level_name_    = level_name_;
    info.game_mode_     = logic_type_;    
    info.num_players_   = player_.size();
    info.max_players_   = s_params.get<unsigned>("server.settings.max_connections");
    info.address_.port  = s_params.get<unsigned>("server.settings.listen_port");


    std::vector<std::string> interfaces = network::enumerateInterfaces();
    if (interfaces.empty())
    {
        s_log << Log::error
              << "Could not enumerate network interfaces\n";
    } else
    {
        if (interfaces.size() > 1)
        {
            s_log << Log::warning
                  << interfaces.size()
                  << " network interfaces found. Expect trouble...\n";
        }
        info.address_.SetBinaryAddress(interfaces[0].c_str());
    }
    
    // Update our offline ping reply in case we get polled by LAN
    // clients
    RakNet::BitStream args;
    info.writeToBitstream(args);
    
    interface_->SetOfflinePingResponse((const char*)args.GetData(),
                                              args.GetNumberOfBytesUsed());


    try
    {
        // Update the master server information.
        master_server_registrator_->sendServerInfo(info);
    } catch (Exception & e)
    {
        s_log << Log::warning
              << "Error contacting master server: "
              << e
              << "\n";
    }
}
