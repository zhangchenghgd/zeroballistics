
#include "TankAppClient.h"


#include <raknet/RakNetworkFactory.h>



#include "physics/OdeSimulator.h"
#include "SoundManager.h"
#include "GameLogicClient.h"
#include "Controllable.h"

#include "VariableWatcher.h"

#include "Profiler.h"

#include "Gui.h"
#include "GUIConsole.h"
#include "GUIProfiler.h"
#include "GameHud.h"

#include "SdlApp.h"
#include "MainMenu.h"

#include "NetworkCommandClient.h"

#include "VariableWatcherVisual.h"
#include "InputHandler.h"

#include "UtilsOsg.h" // for screenshot

#include "SdlKeyNames.h" // for mouse button injection
#include "UserPreferences.h"
#include "Version.h"

#include "RakAutoPacket.h"

#include "EffectManager.h"


#include "MasterServerPunchthrough.h"

#include "VersionInfo.h"

using namespace network;

std::string CONSOLE_STATE_FILE = "console_client.txt";

//------------------------------------------------------------------------------
TankApp::TankApp(MainMenu * task) :
    MetaTask("TankApp"),
    main_menu_task_(task),
    is_chat_active_(false),
    capture_task_(INVALID_TASK_HANDLE),
    version_info_received_(false),
    interface_(RakNetworkFactory::GetRakPeerInterface())
{
    s_log << Log::debug('i') << "TankApp constructor\n";

    s_scene_manager.init();
    s_scene_manager.setClearColor(Vector(0,0,0), GL_DEPTH_BUFFER_BIT);
    
    new VariableWatcherVisual();


    gui_console_.reset(new GUIConsole(root_window_));
    gui_profiler_.reset(new GUIProfiler(root_window_));
    puppet_master_.reset(new PuppetMasterClient(interface_));


    NetworkCommand::initAccounting(&fp_group_);
    
#ifdef ENABLE_DEV_FEATURES
    s_console.addFunction("sset",
                          ConsoleFun(this, &TankApp::serverSet),
                          &fp_group_,
                          ConsoleCompletionFun(&s_console, &Console::completeVariable));
    s_console.addFunction("toggleWireframeMode",
                          Loki::Functor<void>(&s_scene_manager, &SceneManager::toggleWireframeMode),
                          &fp_group_);
    s_console.addFunction("startCapturing",
                          ConsoleFun(this, &TankApp::startCapturing),
                          &fp_group_);
#endif
    s_console.addFunction("toggleProfiler",
                          Loki::Functor<void> (this, &TankApp::toggleProfiler),
                          &fp_group_);
    s_console.addFunction("rcon",
                          ConsoleFun(this, &TankApp::rcon),
                          &fp_group_);    


    s_scheduler.addFrameTask(PeriodicTaskCallback(this, &TankApp::handleNetwork),
                             "TankAppClient::handleNetwork",
                             &fp_group_);


    s_input_handler.registerInputCallback("Talk",
                                          input_handler::SingleInputHandler(this, &TankApp::startChat),
                                          &fp_group_);

    s_input_handler.registerInputCallback("Take Screenshot",
                                          input_handler::SingleInputHandler(this, &TankApp::takeScreenshot),
                                          &fp_group_);
                                          
    
    interface_->SetMTUSize(s_params.get<unsigned>("client.network.mtu_size"));
    interface_->SetOccasionalPing(true); // need this for timestamping to work
    interface_->SetUnreliableTimeout(UNRELIABLE_PACKET_TIMEOUT);
}

//------------------------------------------------------------------------------
TankApp::~TankApp()
{
    s_log << Log::debug('d')
          << "TankApp destructor\n";

    s_console.storeState(CONSOLE_STATE_FILE);
    
    interface_->Shutdown(300);
    RakNetworkFactory::DestroyRakPeerInterface(interface_);

    if (s_params.get<bool>("client.log.print_network_summary"))
    {
        NetworkCommand::printAndResetNetSummary(std::vector<std::string>());
    }

    // Need to do this before scene manager reset, so destruction code
    // still has osg root node access.
    puppet_master_.reset(NULL);
    s_scene_manager.reset();

    s_input_handler.unloadKeymap(getUserConfigFile(), DEFAULT_KEYMAP_SUPERSECTION);
    s_input_handler.unloadKeymap(getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);

    // XXX This happen at version mismatch due to focus on request ready
    if (s_app.getFocusedTask() == this) main_menu_task_->focus();
}


//------------------------------------------------------------------------------
void TankApp::onFocusGained()
{
    s_effect_manager.suspendEffectCreation(false);
}


//------------------------------------------------------------------------------
void TankApp::onFocusLost()
{
    s_effect_manager.suspendEffectCreation(true);
}

//------------------------------------------------------------------------------
void TankApp::render()
{
    if (!puppet_master_->getGameLogic()) return;
    
    PROFILE(TankApp::renderCallback);
    
    ADD_STATIC_CONSOLE_VAR(bool, render_shapes, false);
    ADD_STATIC_CONSOLE_VAR(bool, render_replay_shapes, false);

    s_scene_manager.getCamera().setTransform(puppet_master_->getGameLogic()->getCameraPosition());    

    if (render_shapes || render_replay_shapes)
    {
        s_scene_manager.getCamera().applyTransform();
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        float diffuse[] = {0.4, 0.4, 0.4, 1.0};
        glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
        float ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        
        if (render_shapes) puppet_master_->getGameState()->getSimulator()->renderGeoms();
        else puppet_master_->getLocalPlayer()->getReplaySimulator()->renderGeoms();

        glPopAttrib();
    } 
    else
    {
        s_scene_manager.render();
    }
    
    s_gui.render();

    {
        PROFILE(SDL_GL_SwapBuffers);
        SDL_GL_SwapBuffers();
    }
}


//------------------------------------------------------------------------------
void TankApp::onKeyDown(SDL_keysym sym)
{
    if (is_chat_active_)
    {
        is_chat_active_ ^= puppet_master_->getHud()->handleChatInput(sym);
        if (!is_chat_active_) s_app.enableKeyRepeat(false);
        return;
    }
    
    // Always pass input to gui, with one exception: if console is
    // open and mouse is captured (debugging purpose of driving with
    // open console) then the input is not injected to gui
    if(!(gui_console_->isActive() && s_app.isMouseCaptured()))
    {
        if(s_gui.onKeyDown(sym))
            return;
    }

    if (s_input_handler.onKeyDown(sym.sym)) return;
    
    switch(sym.sym)
    {
#ifdef ENABLE_DEV_FEATURES        
    case SDLK_q:
        if(sym.mod & KMOD_CTRL) // quit on Ctrl+Q
        {
            s_app.quit();
        }
        break;
    case SDLK_f: /// XXXX this doesn't work currently, input is eaten nevertheless.
        if(sym.mod & KMOD_CTRL)
        {
            s_app.toggleFullScreen();
        }
        break;
#endif
    case SDLK_ESCAPE:
        s_app.captureMouse(false);
        main_menu_task_->showMainMenu();
        main_menu_task_->focus();
        break;
        
    default:
        break;
    }
}

//------------------------------------------------------------------------------
void TankApp::onKeyUp(SDL_keysym sym)
{
    if(s_gui.onKeyUp(sym))
        return;

    s_input_handler.onKeyUp(sym.sym);
}

//------------------------------------------------------------------------------
void TankApp::onMouseButtonDown(const SDL_MouseButtonEvent & event)
{
    if(!s_app.isMouseCaptured())
    {
        if(s_gui.onMouseButtonDown(event))
            return;
    }

    switch (event.button)
    {
    case SDL_BUTTON_LEFT:
        if (!s_app.isMouseCaptured()) s_app.captureMouse(true);
        else s_input_handler.onKeyDown(input_handler::KEY_LeftButton);
        break;
    case SDL_BUTTON_MIDDLE:
        s_input_handler.onKeyDown(input_handler::KEY_MiddleButton);
        break;
    case SDL_BUTTON_RIGHT:
        if (!s_app.isMouseCaptured()) s_app.captureMouse(true);
        else s_input_handler.onKeyDown(input_handler::KEY_RightButton);
        break;
    }
}

//------------------------------------------------------------------------------
void TankApp::onMouseButtonUp(const SDL_MouseButtonEvent & event)
{
    if(!s_app.isMouseCaptured())
    {
        if(s_gui.onMouseButtonUp(event))
            return;
    }

    switch (event.button)
    {
    case SDL_BUTTON_LEFT:
        s_input_handler.onKeyUp(input_handler::KEY_LeftButton);
        break;
    case SDL_BUTTON_MIDDLE:
        s_input_handler.onKeyUp(input_handler::KEY_MiddleButton);
        break;
    case SDL_BUTTON_RIGHT:
        s_input_handler.onKeyUp(input_handler::KEY_RightButton);
        break;
    }
}

//------------------------------------------------------------------------------
void TankApp::onMouseMotion(const SDL_MouseMotionEvent & event)
{
    if (!s_app.isMouseCaptured())
    {
        s_gui.onMouseMotion(event);
        return;
    }
    
    // Bail on WarpMouse-induced events
    if (event.x == (s_app.getScreenWidth()  >> 1) &&
        event.y == (s_app.getScreenHeight() >> 1)) return;


    // Check mouse invert
    SDL_MouseMotionEvent tmp_event = event;
    if (!s_params.get<bool>("camera.invert_mouse")) tmp_event.yrel = -event.yrel;

    s_input_handler.setMousePos(Vector2d(tmp_event.x,    tmp_event.y),
                                Vector2d(tmp_event.xrel, tmp_event.yrel));
    
    SDL_WarpMouse(s_app.getScreenWidth()  >> 1,
                  s_app.getScreenHeight() >> 1);   
}

//------------------------------------------------------------------------------
void TankApp::onResizeEvent(unsigned width, unsigned height)
{
    s_log << Log::debug('i')
          << "Resize to " << width << "x" << height << "\n";
    
    s_scene_manager.setWindowSize(width, height);
}


//------------------------------------------------------------------------------
bool TankApp::isConnected() const
{
    return puppet_master_->getLocalPlayer()->getId() != UNASSIGNED_SYSTEM_ADDRESS;
}


//------------------------------------------------------------------------------
/**
 *  Connect directly to host. No NAT-punchthrough is done here,
 *  address was entered manually.
 */
void TankApp::connect(const std::string & host, unsigned port)
{
    s_log << Log::debug('m')
          << "Connecting without punchthrough\n";
    
    SocketDescriptor desc;
    bool attempt_succeded = interface_->Startup(1, s_params.get<unsigned>("client.network.sleep_timer"),
                                                &desc, 1);

    if (attempt_succeded) attempt_succeded = interface_->Connect(host.c_str(), port,
                                                                 NULL, 0, 0);

    if(!attempt_succeded)
    {
        std::string text("Connection attempt to: ");
        text += host.c_str();
        text += ":" + toString(port);
        text += " failed!";

        throw Exception(text);
    }
}

//------------------------------------------------------------------------------
/**
 *  Connect to address that was received from master server. NAT
 *  punchthrough is done.
 */
void TankApp::connectPunch(const SystemAddress & address,
                           unsigned internal_port)
{
    s_log << Log::debug('m')
          << "Connecting WITH punchthrough\n";
    
    SocketDescriptor desc;
    interface_->SetMaximumIncomingConnections(1); // master server can connect to us

    if (!interface_->Startup(2, s_params.get<unsigned>("client.network.sleep_timer"),
                            &desc, 1))
    {
        std::string text("Connection attempt to: ");
        text += address.ToString();
        text += " failed!";

        throw Exception(text);
    }

    // punch plugin will commit suicide.
    master::MasterServerPunchthrough * punch = new master::MasterServerPunchthrough();
    punch->FacilitateConnections(false);
    interface_->AttachPlugin(punch);
    punch->connect(address, internal_port);
}


//------------------------------------------------------------------------------
/**
 *  Called upon successfully connecting to the server.  We now know
 *  our player id.
 *
 *  Set local player id and name, transmit name to server. Add working
 *  tasks.
 */
void TankApp::onConnectionRequestAccepted(const SystemAddress & server_id)
{
    SystemAddress external_id = interface_->GetExternalID(server_id);
    
    s_log << "We are connected to " << server_id << " and have IP "
          << external_id
          << "\n";

    interface_->ApplyNetworkSimulator(s_params.get<float>("client.network.max_bps"),
                                      s_params.get<unsigned>("client.network.min_ping"),
                                      s_params.get<unsigned>("client.network.extra_ping"));

    // ------------  temporary loading screen code start ---------
    std::string connection_text("Connecting... \nConnection accepted from: ");
    connection_text += server_id.ToString();
    connection_text += " \nLoading...";

    main_menu_task_->setLoadingScreenText(connection_text);
    main_menu_task_->render();  // XXX ugly hack
    main_menu_task_->stopMusic(false);
    // ------------  temporary loading screen code end ---------
    
    addTasks();
    
    puppet_master_->getLocalPlayer()->setId(external_id);
    puppet_master_->getLocalPlayer()->setName(s_params.get<std::string>("client.app.player_name"));
}


//------------------------------------------------------------------------------
void TankApp::onVersionInfoReceived(Packet * p)
{
    std::auto_ptr<VersionInfoCmd> version_info(
        (VersionInfoCmd*)NetworkCommandServer::createFromPacket(TPI_VERSION_INFO_SERVER, p));

    VersionInfo server_version = version_info->getVersion();

    s_log << "Server is running version "
          << server_version
          << "\n";
        
    if (server_version != g_version)
    {
        std::ostringstream strstr;

        if (server_version.game_ != g_version.game_)
        {
            strstr << "This server is running a different game.";
        } else
        {
            strstr << "Version mismatch! "
                   << "Game version: "
                   << g_version
                   << ", "
                   << "Server version: "
                   << server_version
                   << ".\nThe latest version can be downloaded from the Zero Ballistics homepage.";
        }
        throw Exception(strstr.str());
    }

    SimpleCmd cmd(TPI_VERSION_ACK);
    cmd.send(interface_, p->systemAddress, false);

    network::SetPlayerNameCmd name_cmd(s_params.get<std::string>("client.app.player_name"));
    name_cmd.send(interface_);

    version_info_received_ = true;
}




//------------------------------------------------------------------------------
void TankApp::handleNetwork(float dt)
{
    try
    {
        PROFILE(TankApp::handleNetwork);

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
        
            // Check if this is a native packet
            switch (packet_id)
            {
                // ---------- RakNet packets ----------
            case ID_CONNECTION_REQUEST_ACCEPTED:
                assert(!isConnected());
                s_log << "connection request accepted from "
                      << p->systemAddress
                      << "\n";
                onConnectionRequestAccepted(p->systemAddress);
                break;

            case ID_ALREADY_CONNECTED:
            case ID_CONNECTION_ATTEMPT_FAILED:
            case ID_CONNECTION_BANNED:
                throw Exception(std::string("Failed to connect to ") + p->systemAddress.ToString());
                break;
            case ID_NAT_TARGET_NOT_CONNECTED:
                throw Exception("Could not perform NAT punchthrough.");
                break;
			
            case ID_NO_FREE_INCOMING_CONNECTIONS:
                throw Exception("The server is full. Try connecting again at a later time.");
                break;
			
            case ID_DISCONNECTION_NOTIFICATION:
                throw Exception(std::string("You have been disconnected from ") + p->systemAddress.ToString());
                break;
            case ID_CONNECTION_LOST:
                throw Exception(std::string("Lost connection to ") + p->systemAddress.ToString());
                break;
                
                // ---------- Own Packets ----------
            case TPI_VERSION_INFO_SERVER:
                onVersionInfoReceived(p);
                break;
        
            case TPI_REQUEST_READY:
                if (acceptGameNetworkPackets())
                {
                    s_log << Log::debug('n')
                          << "Request Ready received.\n";
                
                    // Now all input handling functions should have been
                    // registered, load & apply keymap
                    s_input_handler.loadKeymap  (getUserConfigFile(), DEFAULT_KEYMAP_SUPERSECTION);
                    s_input_handler.loadKeymap  (getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);

                    puppet_master_->onRequestReady();
                    this->focus();
                }
                break;

            case TPI_RESET_GAME:
                if (acceptGameNetworkPackets())
                {
                    s_log << Log::debug('n')
                          << "Received reset command\n";
                    puppet_master_->reset();
                }
                break;

            case TPI_KICK:
                throw Exception("You have been kicked from the server.");
                break;

            default:
                std::auto_ptr<NetworkCommandServer> cmd(NetworkCommandServer::createFromPacket(packet_id, p));

                // we need the version_info_received_ before the first
                // level load or things break badly...  this can
                // happen e.g. when hosting and first loadlevel is
                // received before player is properly added
                if (cmd.get() && version_info_received_ &&
                    (acceptGameNetworkPackets() || dynamic_cast<LoadLevelCmd*>(cmd.get())))
                {
                    cmd->execute(puppet_master_.get());
                }
                break;
            }
        }
    } catch (Exception & e)
    {
        e.addHistory("TankApp::handleNetwork");
        emit(EE_EXCEPTION_CAUGHT, &e);
        throw e;
    }
}




//------------------------------------------------------------------------------
void TankApp::handlePhysics(float dt)
{
    PROFILE(TankApp::handlePhysics);

    s_variable_watcher.frameMove();

    puppet_master_->frameMove(dt);
}


//------------------------------------------------------------------------------
/**
 *  XXXX For debug tweaking: set variable on client & server
 *  simultanously
 */
std::string TankApp::serverSet(const std::vector<std::string> & args)
{
    std::string cmd_and_args("set ");

    // convert std::vector of std::strings to string
    for(unsigned i = 0; i != args.size(); i++)
    {
        cmd_and_args.append(args[i].c_str());

        // add space between arguments
        if(i != args.size()-1)
        {
            cmd_and_args.append(" ");
        }
    }
    network::RconCmd rcon_cmd(cmd_and_args);
    rcon_cmd.send(interface_);

    s_log << "ServerSet: " << cmd_and_args << "\n";
    
    return s_console.setVariable(args);
}


//------------------------------------------------------------------------------
std::string TankApp::rcon(const std::vector<std::string> & args)
{
    if (args.size() < 1) return "Invalid number of arguments.";

    std::string cmd_and_args;

    // convert std::vector of std::strings to string
    for(unsigned i = 0; i != args.size(); i++)
    {
        cmd_and_args.append(args[i].c_str());

        // add space between arguments
        if(i != args.size()-1)
        {
            cmd_and_args.append(" ");
        }
    }
    network::RconCmd rcon_cmd(cmd_and_args);
    rcon_cmd.send(interface_);

    return "";

}


//------------------------------------------------------------------------------
std::string TankApp::startCapturing(const std::vector<std::string> & args)
{
    if (!capture_name_.empty()) return std::string("Currently capturing") + capture_name_;
    
    if (args.size()!=1) return "Please specify movie name.";
    capture_name_ = args[0];
    
    capture_task_ = s_scheduler.addFrameTask(PeriodicTaskCallback(this, &TankApp::writeFrame),
                                             "writeFrame",
                                             &fp_group_);
    
    s_app.captureMouse(true);
    gui_console_->toggleShow();

    return "capturing...";
}


//------------------------------------------------------------------------------
void TankApp::startChat()
{
    is_chat_active_ = true;
    s_app.enableKeyRepeat(true);
    puppet_master_->getHud()->startChat();    
}



//------------------------------------------------------------------------------
void TankApp::takeScreenshot()
{
    if (capture_name_.empty()) saveScreenshot("screenshots/");
    else stopCapturing();
}


//------------------------------------------------------------------------------
void TankApp::stopCapturing()
{
    s_scheduler.removeTask(capture_task_, &fp_group_);
    capture_task_ = INVALID_TASK_HANDLE;
    
    s_log << "stopped capturing "
          << capture_name_
          << "\n";

    capture_name_.clear();
}


//------------------------------------------------------------------------------
void TankApp::writeFrame(float dt)
{
    saveScreenshot("movies/" + capture_name_);
}




//------------------------------------------------------------------------------
void TankApp::toggleProfiler()
{
    gui_profiler_->toggle();
}


//------------------------------------------------------------------------------
/**
 *  Registers the tasks needed to run the client. Those are:
 *
 *  - ping: Periodically ping the server to determine latency (around 1fps).
 *  - mouse sampling: Used to make mouse handling independent from input handling fps.
 *  - physics: steps the simulator.
 *  - input: samples input and sends it to server.
 */
void TankApp::addTasks()
{
    s_scheduler.addTask(PeriodicTaskCallback(this, &TankApp::handlePhysics),
                        1.0f/s_params.get<float>("physics.fps"),
                        "TankApp::handlePhysics",
                        &fp_group_);


    // Wait 1 sec for restore to be sure all vars are registered.
    s_scheduler.addEvent(SingleEventCallback(&s_console, &Console::restoreState),
                         1,
                         &CONSOLE_STATE_FILE,
                         "Console::restoreState",
                         &fp_group_);
}



//------------------------------------------------------------------------------
/**
 *  Only accept other commands after a level has been loaded to avoid
 *  initialization order mess-up and packets that are of no meaning to
 *  us yet.
 *
 *  LoadLevel is the first command the server sneds to us after
 *  version ack.
 */
bool TankApp::acceptGameNetworkPackets() const
{
    return (puppet_master_->getGameLogic() &&
            !puppet_master_->getGameLogic()->getLevelName().empty() &&
            version_info_received_);
}
