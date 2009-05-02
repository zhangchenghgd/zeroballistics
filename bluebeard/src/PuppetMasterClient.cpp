
#include "PuppetMasterClient.h"

#include <tinyxml.h>


#include "physics/OdeSimulator.h"

#include "PlayerInput.h"
#include "GameState.h"
#include "NetworkCommand.h"
#include "NetworkCommandClient.h"
#include "GameLogicClient.h"
#include "Controllable.h"
#include "ClassLoader.h"
#include "GameHud.h"
#include "RigidBodyVisual.h"
#include "TinyXmlUtils.h"
#include "SceneManager.h"
#include "NetworkCommandClient.h"
#include "Scheduler.h"

// Managers for level file loading
#include "SoundManager.h"
#include "TextureManager.h"
#include "EffectManager.h"
#include "ParameterManager.h"


#include "ReaderWriterBbm.h"
#include "Paths.h"
#include "LodUpdater.h"

#include "InputHandler.h"

#include "RegEx.h"

#include "HudMessageColors.h"

#include "ObjectParts.h"



//------------------------------------------------------------------------------
PuppetMasterClient::PuppetMasterClient(RakPeerInterface * client_interface) :
    game_state_(new GameState()),
    connection_problem_(false),
    client_interface_(client_interface),
    hud_(new GameHud(this))
{

#ifdef ENABLE_DEV_FEATURES    
    s_console.addFunction("toggleRenderTargets",
                          Loki::Functor<void> (&RigidBodyVisual::toggleRenderTargets),
                          &fp_group_);
    s_console.addFunction("saveLevelResources",
                          Loki::Functor<void> (this, &PuppetMasterClient::saveLevelResources),
                          &fp_group_);
    s_console.addFunction("dumpClientSimulators",
                          ConsoleFun          (this, &PuppetMasterClient::dumpSimulatorContents),
                          &fp_group_);
    s_console.addFunction("dumpClientCollisionSpaces",
                          ConsoleFun          (this, &PuppetMasterClient::dumpSpaceContents),
                          &fp_group_);
    s_console.addFunction("fov",
                          ConsoleFun          (this, &PuppetMasterClient::setFov),
                          &fp_group_);
    s_console.addFunction("hither",
                          ConsoleFun          (this, &PuppetMasterClient::setHither),
                          &fp_group_);
    s_console.addFunction("yon",
                          ConsoleFun          (this, &PuppetMasterClient::setYon),
                          &fp_group_);
#endif    
    s_console.addFunction("name",
                          ConsoleFun          (this, &PuppetMasterClient::changePlayerName),
                          &fp_group_);

    
    // ---------- Setup collision categories ----------
    
    // We don't want to collide static level data.
    game_state_->getSimulator()->enableCategoryCollisions(CCC_STATIC, CCC_STATIC,     false);
    
    // Proxies shouldn't be able to interpenetrate level geometry.
    game_state_->getSimulator()->enableCategoryCollisions(CCC_STATIC, CCC_PROXY,      true);    
    
    // The user controlled tank must match with the server, so
    // collision against level data is neccessary.
    game_state_->getSimulator()->enableCategoryCollisions(CCC_STATIC, CCC_CONTROLLED, true);


    // Proxies need to collide.
    game_state_->getSimulator()->enableCategoryCollisions(CCC_PROXY, CCC_PROXY,       true);
    
    // Collisions are resolved by history replay, don't perform collision detection.
    game_state_->getSimulator()->enableCategoryCollisions(CCC_PROXY, CCC_CONTROLLED,  false);

    // There never should be more than one controlled object... make sure we notice...
    game_state_->getSimulator()->enableCategoryCollisions(CCC_CONTROLLED, CCC_CONTROLLED, true);

    

    // ---------- Setup input handling functions ----------
    s_scheduler.addTask(PeriodicTaskCallback(this, &PuppetMasterClient::handleInput),
                        1.0f/s_params.get<float>("client.input.send_input_fps"),
                        "PuppetMasterClient::handleInput",
                        &fp_group_);


    s_input_handler.registerInputCallback("view",
                                          input_handler::MouseInputHandler(this, &PuppetMasterClient::mouseMotion),
                                          &fp_group_);
    
    s_input_handler.registerInputCallback("Accelerate",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::up),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Decelerate",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::down),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Turn left",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::left),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Turn right",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::right),
                                          &fp_group_);

    s_input_handler.registerInputCallback("Fire Cannon",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::fire1),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Fire tractor beam",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::fire2),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Use First Skill",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::fire3),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Use boost",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::fire4),
                                          &fp_group_);


    s_input_handler.registerInputCallback("Pickup/Drop Beacon",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::action1),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Handbrake",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::action2),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Flip Tank",
                                          input_handler::ContinuousInputHandler(this, &PuppetMasterClient::action3),
                                          &fp_group_);
}


//------------------------------------------------------------------------------
PuppetMasterClient::~PuppetMasterClient()
{
    s_log << Log::debug('d')
          << "PuppetMasterClient destructor\n";

    // Destroy gamestate objects before eg beacon boundary...
    reset();
}

//------------------------------------------------------------------------------
void PuppetMasterClient::frameMove(float dt)
{
//     if (local_player_.getControllable())
//     {
//         s_log << Log::debug('b')
//               << "at start of frame " << (unsigned)local_player_.getSequenceNumber()
//               << " with forward " << local_player_.getControllable()->getPlayerInput().up_ 
//               << " with pos " << local_player_.getControllable()->getPosition().z_
//               << "\n";
//     }

    game_state_->frameMove(dt);

    // detect a connection problem
    if (connection_problem_ != local_player_.isHistoryOverflow())
    {
        connection_problem_ = local_player_.isHistoryOverflow();

        if (connection_problem_)
        {
            hud_->setStatusLine("There is a problem with your connection.");
        } else
        {
            hud_->setStatusLine("");
        }
    }

    local_player_.incSequenceNumber();
}



//------------------------------------------------------------------------------
/**
 *  Resets all data specific to the currently loaded level, but leaves
 *  intact team assignments, team scores etc. Should to the same
 *  things as PuppetMasterServer::reset.
 */
void PuppetMasterClient::reset()
{
#ifdef ENABLE_DEV_FEATURES
    saveLevelResources();
#endif
    
    s_log << Log::debug('d')
          << "PuppetMasterClient::reset()\n";
    
    for (RemotePlayerContainer::iterator it = remote_player_.begin();
         it != remote_player_.end();
         ++it)
    {
        setControllable(it->getId(), INVALID_GAMEOBJECT_ID);
    }

    setControllable(local_player_.getId(), INVALID_GAMEOBJECT_ID);
    
    s_lod_updater.reset();

    game_state_->reset();
    game_logic_.reset(NULL);

    local_player_.reset();

    hud_->setStatusLine("");
    hud_->clearMessage(NULL);

}

//------------------------------------------------------------------------------
void PuppetMasterClient::handleStringMessage(network::STRING_MESSAGE_TYPE type,
                                             const std::string & message,
                                             const SystemAddress & pid1)
{
    if (!game_logic_.get()) return;
    
    Player * player_name_changed = NULL;
    RemotePlayer * player = NULL;

    std::string complete;
    switch(type)
    {
    case network::SMT_RCON_RESPONSE:
        s_log << "rcon: " << message << "\n";
        break;
    case network::SMT_GAME_INFORMATION:
        hud_->setStatusLine(message);
        break;
    case network::SMT_CHAT:
        if (pid1 == UNASSIGNED_SYSTEM_ADDRESS)
        {
            // Server message
            s_log << "Server: "
                  << message
                  << "\n";
            hud_->addChatLine("Server: " + message, Color(0.51f, 0.87, 1.0f));
        } else
        {
            // Player message
            player = getRemotePlayer(pid1);
            if (!player)
            {
                s_log << Log::warning
                      << "Received chat msg from nonexisting player\n";
                return;
            }

            complete = player->getName() + " : " + message;
            s_log << complete << "\n";
            hud_->addChatLine(complete);
        }
        break;

    case network::SMT_PLAYER_NAME:
        player_name_changed = getRemotePlayer(pid1);

        // check if it is a remote player
        if (!player_name_changed)
        {
            //if not check if it is the local player
            if(local_player_.getId() == pid1)
            {
                player_name_changed = &local_player_;
            }
            else
            {
                s_log << Log::warning
                      << "Received name for nonexisting player "
                      << pid1
                      << "\n";
                return;
            }
        }        

        // Determine whether this is the initial transmission of
        // player data based on whether level data was already set for
        // replay sim
        if (player_name_changed != &local_player_ &&
            local_player_.isLevelDataSet())
        {
            if(player_name_changed->getName().empty())
            {
                hud_->addMessage(message + " connected.",
                                 MC_PLAYER_CONNECTED);
            } else
            {
                hud_->addMessage(player_name_changed->getName() + " changed his name to " + message,
                                 MC_PLAYER_NAME_CHANGED);
            }
        }

        player_name_changed->setName(message);
        game_logic_->onPlayerNameChanged(player_name_changed);
        break;
        
    default:
        s_log << Log::warning
              << "Received unknown string message "
              << (unsigned)type
              << "\n";
        break;
    }
}



//------------------------------------------------------------------------------
/**
 *  Sets the game object controlled by a player.  Setting the local
 *  player's controllable is the more important case.
 *
 *  Do the following things:
 *  - Check whether the object is truly a controllable.
 *
 *  - If the player is local, inform the player object of the
 *  controllable change.
 *
 *  - Else just set the player's controlled object.
 *
 *  \see LocalPlayer::setControllable
 */
void PuppetMasterClient::setControllable(const SystemAddress & id,
                                         uint16_t controllable_id)
{
    if (!game_logic_.get()) return;
    
    s_log << Log::debug('l')
          << "Setting controllable for "
          << id
          << " to "
          << controllable_id
          << "\n";
    
    Controllable * controllable = NULL;

    if (controllable_id != INVALID_GAMEOBJECT_ID)
    {
        controllable = dynamic_cast<Controllable*>(game_state_->getGameObject(controllable_id));
        if (!controllable)
        {
            s_log << Log::warning << "controllable "
                  << controllable_id << " for player "
                  << id << " is not a valid Controllable object.\n";

            return;
        }
    }

    
    if (id == local_player_.getId())
    {
        Controllable * prev_controllable = local_player_.getControllable();

        if (prev_controllable == controllable) return;
        
        local_player_.setControllable(controllable);
        game_logic_->onControllableChanged(&local_player_, prev_controllable);
    } else
    {
        RemotePlayer * player = getRemotePlayer(id);
        if (!player)
        {
            s_log << Log::warning << "Nonexisting player "
                  << id << " in PuppetMasterClient::setControllable\n";
            return;
        }

        Controllable * prev_controllable = player->getControllable();
        
        player->setControllable(controllable);
        game_logic_->onControllableChanged(player, prev_controllable);
    }
}

//------------------------------------------------------------------------------
/**
 *  This means our level is completely loaded. Now we can fill the
 *  replay sim with static objects.
 */
void PuppetMasterClient::onRequestReady()
{
    if (!game_logic_.get()) return;
    
    s_log << Log::debug('n')
          << "PuppetMasterClient::onRequestReady\n";

    local_player_.setLevelData(game_state_.get());

    game_logic_->onRequestReady();
    
    network::SimpleCmd ready(network::TPI_READY);
    ready.send(client_interface_);


    try
    {
        hud_->addMessage("Press '" + s_input_handler.getKeyForFunction("Show Help") + "' for help.", MC_HELP);
    } catch (input_handler::KeyNotRegisteredException & e)
    {
    }
}



//------------------------------------------------------------------------------
LocalPlayer * PuppetMasterClient::getLocalPlayer()
{
    return &local_player_;
}


//------------------------------------------------------------------------------
void PuppetMasterClient::addRemotePlayer(const SystemAddress & id)
{
    if (!game_logic_.get()) return;
    
    for (RemotePlayerContainer::iterator it = remote_player_.begin();
         it != remote_player_.end();
         ++it)
    {
        if (it->getId() == id)
        {
            s_log << Log::warning
                  << "Remote player added twice : "
                  << id
                  << "\n";

            return;
        }
    }
    
    remote_player_.push_back(RemotePlayer(id));
    game_logic_->addPlayer(&remote_player_.back());
}

//------------------------------------------------------------------------------
void PuppetMasterClient::deleteRemotePlayer(const SystemAddress & id)
{
    if (!game_logic_.get()) return;
    
    for (RemotePlayerContainer::iterator it = remote_player_.begin();
         it != remote_player_.end();
         ++it)
    {
        if (it->getId() == id)
        {
            hud_->addMessage(it->getName() + " has left the game.", MC_PLAYER_DISCONNECTED);
            remote_player_.erase(it);

            game_logic_->removePlayer(id);
            
            return;
        }
    }

    s_log << Log::warning << "Attempted to delete nonexisting player "
          << id << "\n";
}

//------------------------------------------------------------------------------
void PuppetMasterClient::clearRemotePlayers()
{
    remote_player_.clear();
}


//------------------------------------------------------------------------------
RemotePlayer * PuppetMasterClient::getRemotePlayer(const SystemAddress & id)
{
    for (RemotePlayerContainer::iterator it = remote_player_.begin();
         it != remote_player_.end();
         ++it)
    {
        if (it->getId() == id) return &(*it);
    }

    return NULL;
}

//------------------------------------------------------------------------------
/**
  * Convenience method to avoid iterating over local and remote players
  * on client all the time
 **/
Player * PuppetMasterClient::getLocalOrRemotePlayer(const SystemAddress & id)
{
    Player * player = getRemotePlayer(id);

    // if it's not a remote player, check the local player
    if(!player)
    {
        if(local_player_.getId() == id)
        {
            player = dynamic_cast<Player*>(&local_player_);
        }
    }

    return player;
}

//------------------------------------------------------------------------------
void PuppetMasterClient::addGameObject(GameObject * object)
{
    if (!game_logic_.get()) return;
    
    s_log << Log::debug('n')
          << "PuppetMasterClient::addGameObject for id "
          << object->getId()
          << "\n";
    
    game_state_->addGameObject(object);
    game_logic_->onGameObjectAdded(object);

    object->addObserver(ObserverCallbackFunUserData(this, &PuppetMasterClient::onLifetimeExpired),
                        RBE_ACTIVATED,
                        &fp_group_);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::deleteGameObject(uint16_t id)
{
    GameObject * object = game_state_->getGameObject(id);
    
    if (!object)
    {
        s_log << Log::debug('n')
              << "Received deleteGameObject command for nonexisting game object "
              << id
              << "\n";
        return;
    }

    s_log << Log::debug('n')
          << "Deleting GameObject "
          << *object
          <<"\n";
    
    game_state_->deleteGameObject(id);
}


//------------------------------------------------------------------------------
GameState * PuppetMasterClient::getGameState()
{
    return game_state_.get();
}

//------------------------------------------------------------------------------
GameLogicClient * PuppetMasterClient::getGameLogic()
{
    return game_logic_.get();
}

//------------------------------------------------------------------------------
const std::list<RemotePlayer> & PuppetMasterClient::getConnectedPlayers()
{
    return remote_player_;
}

//------------------------------------------------------------------------------
bool PuppetMasterClient::isConnectionProblem()
{
    return connection_problem_;
}


//------------------------------------------------------------------------------
void PuppetMasterClient::sendChat(const std::string &  msg)
{
    if(!local_player_.allowAnnoyingClientRequest())
    {
        hud_->addMessage("Blocked due to flooding.", MC_BLOCKED_FLOODING);
        return;
    }

    network::ChatCmd cmd(msg);
    cmd.NetworkCommandClient::send(client_interface_);
    hud_->addChatLine("You: " + msg, Color(1.0f,1.0f,1.0f));

    local_player_.incAnnoyingClientRequest();

    s_log << "you: " << msg << "\n";
}


//------------------------------------------------------------------------------
GameHud * PuppetMasterClient::getHud()
{
    return hud_.get();
}

//------------------------------------------------------------------------------
RakPeerInterface * PuppetMasterClient::getInterface()
{
    return client_interface_;
}

//------------------------------------------------------------------------------
void PuppetMasterClient::loadLevel(const std::string & map_name,
                                   const std::string & game_logic_type)
{
    // automatically save level resources of previous level...
#ifdef ENABLE_DEV_FEATURES
    saveLevelResources();
#endif

    
    try
    {
        loadLevelResources(map_name);
    } catch (Exception & e)
    {
        e.addHistory("PuppetMasterClient::loadLevelResources");
        s_log << Log::error << e << "\n";
    }

    game_logic_.reset(NULL); // only one logic may exist at any moment...
    try
    {
        game_logic_.reset(s_client_logic_loader.create(std::string("GameLogicClient") + game_logic_type));
    } catch (Exception & e)
    {
        throw Exception("Server is running game mode \"" + game_logic_type + "\", which is not available in this version. "
                        "Download the latest version from the game homepage.");
    }
    game_logic_->init(this);
    
    try
    {
        game_logic_->loadLevel(map_name);
    } catch (Exception & e)
    {
        e.addHistory("GameLogicClient::loadLevel(" + map_name + ", " + game_logic_type + ")");
        throw e;
    }    

    // Re-add all players to newly constructed game logic
    game_logic_->addPlayer(&local_player_);
    for (RemotePlayerContainer::iterator it = remote_player_.begin();
         it != remote_player_.end();
         ++it)
    {
        game_logic_->addPlayer(&(*it));
    }
}


//------------------------------------------------------------------------------
/**
 *  \see PuppetMasterServer::onRigidBodyActivated
 */
void PuppetMasterClient::replaceGameObject(uint16_t replaced_id,
                                           uint16_t start_replacing_id,
                                           const std::string & appendix_list)
{    
    RigidBody * replaced_object = dynamic_cast<RigidBody*>(game_state_->getGameObject(replaced_id));
    
    if (!replaced_object)
    {
        s_log << Log::error
              << "Cannot find object with id "
              << replaced_id
              << " to be replaced.\n";
        return;
    }


    uint16_t cur_id = start_replacing_id;
    std::vector<std::string> part_names = getObjectPartNames(replaced_object->getName(), appendix_list);
    std::vector<RigidBody*> parts;
    for (std::vector<std::string>::const_iterator it = part_names.begin();
         it != part_names.end();
         ++it)
    {
        RigidBody * body = RigidBody::create(*it,
                                             "RigidBody",
                                             game_state_->getSimulator(),
                                             true);
        parts.push_back(body);

        body->setTransform(replaced_object->getTransform());
        body->setSleeping(false);

        if (!body->isStatic())
        {
            body->setGlobalLinearVel (replaced_object->getGlobalLinearVel());
            body->setGlobalAngularVel(replaced_object->getGlobalAngularVel());
        }

        // Align COGs of old and new object if corresponding flag ist set
        if (body->getTarget()->isAlignCogSet())
        {
            Vector cog1 = body->getTarget()->vecToWorld(body->getTarget()->getCog());
            Vector cog2 = replaced_object->getTarget()->vecToWorld(replaced_object->getTarget()->getCog());

            body->setPosition(body->getPosition() - cog1 + cog2);
        }

        body->setId(cur_id++);
        addGameObject(body);
    }


    game_logic_->onObjectReplaced(replaced_object, parts);

    // We need to do this here, or collision shape of replaced body
    // might still influence stuff...
    deleteGameObject(replaced_object->getId());    
}


//------------------------------------------------------------------------------
/**
 *  XXXX unimplemented at the moment.
 */ 
std::string PuppetMasterClient::changePlayerName(const std::vector<std::string> & args)
{
    return "Not implemented.";    

    /*
    assert(game_logic_.get());

    std::string name = "";

    // convert std::vector of std::strings to string
    for(unsigned i=0; i != args.size(); i++)
    {
        if(!args[i].empty())
        {
            name.append(args[i].c_str());

            // recreate spaces between arguments
            if(i != args.size()-1)
            {
                name.append(" ");
            }
        }
    }

    trim(name);

    // check player name
    RegEx reg_ex(PLAYER_NAME_REGEX);
    if(reg_ex.match(name))
    {
        if(name == local_player_.getName()) return "Name remains unchanged.";
        
        network::SetPlayerDataCmd cmd(name);
        cmd.NetworkCommandClient::send(client_interface_);

        s_params.set("client.app.player_name",name);

        return "Name changed to: " + name;
    }
    else
    {
        return "Invalid player name: " + name + " Needs to be 3-20 characters. Not allowed: \\, /";
    }
    */
}


//------------------------------------------------------------------------------
std::string PuppetMasterClient::setFov(const std::vector<std::string> & args)
{
    if (args.size() != 1) return "Specify new field of view in degrees.";

    float fov = fromString<float>(args[0]);

    s_scene_manager.getCamera().setFov(deg2Rad(fov));

    return "";
}


//------------------------------------------------------------------------------
std::string PuppetMasterClient::setHither(const std::vector<std::string> & args)
{
    if (args.size() != 1) return "Specify new near plane distance.";

    float h = fromString<float>(args[0]);

    s_scene_manager.getCamera().setHither(h);

    return "";
}

//------------------------------------------------------------------------------
std::string PuppetMasterClient::setYon(const std::vector<std::string> & args)
{
    if (args.size() != 1) return "Specify new far plane distance.";

    float h = fromString<float>(args[0]);

    s_scene_manager.getCamera().setYon(h);

    return "";
}


//------------------------------------------------------------------------------
std::string PuppetMasterClient::dumpSimulatorContents(const std::vector<std::string> & args)
{
    s_log << "Dumping client simulators:\n";
    game_state_->getSimulator()->dumpContents();
    local_player_.getReplaySimulator()->dumpContents();
    
    return "";
}

//------------------------------------------------------------------------------
std::string PuppetMasterClient::dumpSpaceContents(const std::vector<std::string> & args)
{
    s_log << "Dumping client collision spaces:\n";
    game_state_->getSimulator()->getStaticSpace()->dumpContents();
    game_state_->getSimulator()->getActorSpace() ->dumpContents();
    return "";
}



//------------------------------------------------------------------------------
void PuppetMasterClient::loadLevelResources(const std::string & lvl_name)
{
    s_log << Log::debug('r')
          << "Loading resources for level "
          << lvl_name
          << "\n";

    std::string resource_file = LEVEL_PATH + lvl_name + "/resources.xml";
    
    s_texturemanager  .loadResourceSet(resource_file);
    s_soundmanager    .loadResourceSet(resource_file);
    s_effect_manager  .loadResourceSet(resource_file);
    s_model_manager   .loadResourceSet(resource_file); 
}


//------------------------------------------------------------------------------
void PuppetMasterClient::saveLevelResources() const
{
    if (!game_logic_.get()) return;
    if (game_logic_->getLevelName().empty()) return;
    
    LocalParameters resources;

    std::vector<std::string> resource_string;
    std::string name;

    for (unsigned m=0; m<4; ++m)
    {
        switch(m)
        {
        case 0: resource_string = s_texturemanager.  getLoadedResources(); name = s_texturemanager.  getName(); break;
        case 1: resource_string = s_soundmanager.    getLoadedResources(); name = s_soundmanager.    getName(); break;
        case 2: resource_string = s_effect_manager.  getLoadedResources(); name = s_effect_manager.  getName(); break;
        case 3: resource_string = s_model_manager.   getLoadedResources(); name = s_model_manager.   getName(); break;
        }
        
        for (unsigned res=0; res<resource_string.size(); ++res)
        {
            resources.set<std::vector<std::string> >("level." + name, resource_string);
        }
    }


    std::string resource_file = LEVEL_PATH + game_logic_->getLevelName() + "/resources.xml";
    TiXmlDocument doc(resource_file);
    doc.InsertEndChild(TiXmlDeclaration("1.0","","yes"));
    TiXmlElement * root = (TiXmlElement*)doc.InsertEndChild(TiXmlElement("parameters"));
    
    resources.save(root);

    if (!doc.SaveFile())
    {
        Exception e("Couldn't save ");
        e << resource_file
          << ".";
        throw e;
    }    
}


//------------------------------------------------------------------------------
void PuppetMasterClient::handleInput(float dt)
{
    if (!game_logic_.get()) return;

//    s_log << Log::debug('t') << "handleInput\n";

    float min = s_params.get<float>("client.input.mouse_sensitivity_min");
    float max = s_params.get<float>("client.input.mouse_sensitivity_max");
    float v =   s_params.get<float>("client.input.mouse_sensitivity");
    float fac = s_params.get<float>("client.input.mouse_sensitivity_pitch_factor");
    
    float sensitivity = min + (max-min)*v;
    
    cur_input_.delta_yaw_   *= sensitivity / dt;
    cur_input_.delta_pitch_ *= sensitivity*fac / dt;

    // copy input to allow input modifications inside gamelogic that only affect 
    // input in this frame
    // this is required e.g. for braking tank when beacon should be released.
    PlayerInput input_copy = cur_input_; 
    bool input_handled = game_logic_->handleInput(input_copy);

    if (local_player_.getControllable())
    {
        // Process input locally
        local_player_.handleInput(input_handled ? PlayerInput() : input_copy);

        // Send input to server, together with number of performed
        // physic steps        
        network::PlayerInputCmd cmd(local_player_.getControllable()->getPlayerInput(),
                                    local_player_.getSequenceNumber());
        cmd.send(client_interface_);

//         s_log << Log::debug('b') << "sent input \t\t\t"
//               << (unsigned)local_player_.getSequenceNumber()
//               << " with forward " << local_player_.getControllable()->getPlayerInput().up_
//               << "\n";
        
    } else if (!input_handled)
    {
        // don't do this or input state is wrong afterwards.
//        cur_input_.clear();
    }

    // Reset for next accumulation
    cur_input_.clearReleased();
    cur_input_.delta_yaw_   = 0.0f;
    cur_input_.delta_pitch_ = 0.0f;
}


//------------------------------------------------------------------------------
void PuppetMasterClient::up     (bool b)
{
    cur_input_.up_ = b;
}


//------------------------------------------------------------------------------
void PuppetMasterClient::down   (bool b)
{
    cur_input_.down_ = b;
}

//------------------------------------------------------------------------------
void PuppetMasterClient::right  (bool b)
{
    cur_input_.right_ = b;
}

//------------------------------------------------------------------------------
void PuppetMasterClient::left   (bool b)
{
    cur_input_.left_ = b;
}

//------------------------------------------------------------------------------
void PuppetMasterClient::fire1  (bool b)
{
    setKeyState(cur_input_.fire1_, b);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::fire2  (bool b)
{
    setKeyState(cur_input_.fire2_, b);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::fire3  (bool b)
{
    setKeyState(cur_input_.fire3_, b);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::fire4  (bool b)
{
    setKeyState(cur_input_.fire4_, b);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::action1(bool b)
{
    setKeyState(cur_input_.action1_, b);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::action2(bool b)
{
    setKeyState(cur_input_.action2_, b);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::action3(bool b)
{
    setKeyState(cur_input_.action3_, b);
}

//------------------------------------------------------------------------------
void PuppetMasterClient::mouseMotion(Vector2d pos, Vector2d delta)
{
    cur_input_.delta_yaw_   -= delta.x_;
    cur_input_.delta_pitch_ += delta.y_;
}


//------------------------------------------------------------------------------
void PuppetMasterClient::setKeyState(INPUT_KEY_STATE & state, bool b)
{
    if (b) state          = IKS_JUST_PRESSED;
    else
    {
        if (state == IKS_JUST_PRESSED) state = IKS_PRESSED_AND_RELEASED;
        else state = IKS_UP;
    }
}


//------------------------------------------------------------------------------
void PuppetMasterClient::onLifetimeExpired(Observable * o, void*a, unsigned )
{
    RigidBody * b = (RigidBody*)o;
    if (b->getTarget()->isClientSideOnly())
    {
        game_state_->deleteGameObject(b->getId());
    }
}
