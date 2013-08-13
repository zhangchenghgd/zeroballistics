

#include "MainMenu.h"


#include "Gui.h"

#include "Log.h" // XXX testing

#include "IntegratedServer.h"
#include "TankAppClient.h"
#include "SdlApp.h"
#include "Utils.h"
#include "GUIOptions.h"
#include "GUIServerBrowser.h"
#include "GUIHostMenu.h"
//#include "GUIClientAuthentication.h"
#include "UserPreferences.h"
#include "ParameterManager.h"
#include "ServerInfo.h"
#include "OggStream.h"
#include "NetworkServer.h"
#include "PuppetMasterServer.h"

#ifdef TEASER_ENABLED
#include "GUITeaser.h"
#endif


//------------------------------------------------------------------------------
MainMenu::MainMenu() :
    MetaTask("MainMenu"),
    client_task_(NULL),
    server_task_(NULL),
    menu_music_(NULL)
{
    loadParameterFiles();
    
    enableFloatingPointExceptions(false);

    setupAndLoad();

    registerCallbacks();

    enableFloatingPointExceptions();

    loadMusic();    


    s_console.addObserver(ObserverCallbackFunUserData(this, &MainMenu::onExceptionCaught),
                          EE_EXCEPTION_CAUGHT,
                          &fp_group_);
    s_scheduler.addObserver(ObserverCallbackFunUserData(this, &MainMenu::onExceptionCaught),
                            EE_EXCEPTION_CAUGHT,
                            &fp_group_);
    
//    clickedHostBtn(CEGUI::EventArgs()); // development shortcut to game...
}

//------------------------------------------------------------------------------
MainMenu::~MainMenu()
{
    s_log << Log::debug('d')
          << "MainMenu destructor\n";
    
    shutdownClient(NULL);
    shutdownServer(NULL);

    delete menu_music_;
}

//------------------------------------------------------------------------------
void MainMenu::setupAndLoad()
{
    CEGUI::ImagesetManager::getSingleton().createImageset("MainmenuBackground.imageset","imagesets");

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    try
    {
        // load the mainmenu font
        CEGUI::FontManager::getSingleton().createFont("MainMenu.font");
        CEGUI::FontManager::getSingleton().createFont("MainMenuSmall.font");

        // Use parent window name as prefix to avoid name clashes
        wm.loadWindowLayout("mainmenu.layout", root_window_->getName());
        wm.loadWindowLayout("notification.layout", root_window_->getName());
        wm.loadWindowLayout("background.layout", root_window_->getName());
        wm.loadWindowLayout("loading.layout", root_window_->getName());
        wm.loadWindowLayout("credits.layout", root_window_->getName());

        CEGUI::Window * background = (CEGUI::Window*) wm.getWindow(root_window_->getName() + "mainmenu/main");
        main_menu_ = (CEGUI::Window*) wm.getWindow(root_window_->getName() + "mainmenu/main_menu");
        host_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/host_btn");
        join_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/join_btn");
        quit_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/quit_btn");
        continue_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/continue_btn");
        disconnect_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/disconnect_btn");
        options_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/options_btn");
        credits_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/credits_btn");
        notification_window_ = (CEGUI::Window*) wm.getWindow(root_window_->getName() + "mainmenu/notification");
        notification_ok_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/notification/ok_btn");
        notification_label_ = (CEGUI::Window*) wm.getWindow(root_window_->getName() + "mainmenu/notification/label");
        loading_screen_window_ = (CEGUI::Window*) wm.getWindow(root_window_->getName() + "mainmenu/loading_screen");
        loading_screen_label_ = (CEGUI::Window*) wm.getWindow(root_window_->getName() + "mainmenu/loading_screen/label");
        credits_window_ = (CEGUI::Window*) wm.getWindow(root_window_->getName() + "mainmenu/credits/window");
        credits_ok_btn_ = (CEGUI::ButtonBase*) wm.getWindow(root_window_->getName() + "mainmenu/credits/ok_btn");

        // add main menu windows to widget tree
        root_window_->addChildWindow(background);
        root_window_->addChildWindow(main_menu_);
        root_window_->addChildWindow(notification_window_);
        root_window_->addChildWindow(loading_screen_window_);
        root_window_->addChildWindow(credits_window_);
        
    }
    catch (CEGUI::Exception& e)
    {
        throw Exception(std::string("Main Menu is missing a widget! ") + e.getMessage().c_str());
    }

    // create Options Menu
    options_menu_.reset(new GUIOptions(this));

    // create Server Browser Gui
    server_browser_.reset(new GUIServerBrowser(this));

    // create Host Menu Gui
    host_menu_.reset(new GUIHostMenu(this));

#ifdef TEASER_ENABLED
    // create Teaser Screen
    teaser_screen_.reset(new GUITeaser(this));
#endif

    // create Client Auth. screen
//    client_authentication_.reset(new GUIClientAuthentication(this));

    continue_btn_->setVisible(false);
    disconnect_btn_->setVisible(false);
    notification_window_->setVisible(false);
    loading_screen_window_->setVisible(false);
    credits_window_->setVisible(false);

//     // make client auth. the first screen visible
//     hide();
//     client_authentication_->show();
}

//------------------------------------------------------------------------------
/**
*  register callbacks for menu buttons
**/
void MainMenu::registerCallbacks()
{
    host_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedHostBtn, this));
    quit_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedQuitBtn, this));
    join_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedJoinBtn, this));
    credits_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedCreditsBtn, this));
    disconnect_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedDisconnectBtn, this));
    notification_ok_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedNotificationOkBtn, this));
    continue_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedContinueBtn, this));
    options_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedOptionsBtn, this));
    notification_window_->subscribeEvent(CEGUI::Editbox::EventKeyDown, CEGUI::Event::Subscriber(&MainMenu::onKeyNotificationWindow, this));
    credits_ok_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&MainMenu::clickedCreditsOkBtn, this));
}


//------------------------------------------------------------------------------
void MainMenu::loadParameterFiles()
{
    s_log << Log::debug('i') << "Loading Parameter files\n";

    s_params.loadParameters("data/config/teams.xml");
    s_params.loadParameters("data/config/upgrade_system.xml");
    s_params.loadParameters("data/config/weapon_systems.xml");
    s_params.loadParameters("data/config/skills.xml");

    s_params.loadParameters("data/config/hud.xml");

    s_params.loadParameters("data/config/sounds.xml");
}

//------------------------------------------------------------------------------
void MainMenu::loadMusic()
{
    try
    {
        if(!s_soundmanager.existsDevice()) return;

        menu_music_ = new OggStream(s_params.get<std::string>("client.music.menu"), SSCF_LOOP);
    }
    catch (Exception & e)
    {
        // file could not be loaded, invalid ogg or stuff like that
        // player must live without music
        s_log << Log::warning << e.getTotalErrorString() << "\n";

        menu_music_ = NULL;
    }
}

//------------------------------------------------------------------------------
/**
*  \param window 
*      make window visible and active it, hide everything else
*
*  frist, hide all windows -> makes handling with multiple menus easier
*  second, check for parameter window and activate appropriate windows
*
**/
void MainMenu::show(CEGUI::Window * window)
{
    hide();

    // activate specfic window:
    if(window == main_menu_)
    {
        main_menu_->setVisible(true);
        main_menu_->activate();

        if(client_task_)
        {
            host_btn_->setVisible(false);
            join_btn_->setVisible(false);
            disconnect_btn_->setVisible(true);
            continue_btn_->setVisible(true);
        }
        else
        {            
            disconnect_btn_->setVisible(false);
            continue_btn_->setVisible(false);
            host_btn_->setVisible(true);
            join_btn_->setVisible(true);
        }
        return;
    }

    if(window == notification_window_)
    {
        notification_window_->setVisible(true);        
        notification_window_->activate();
        return;
    }

    if(window == loading_screen_window_)
    {
        loading_screen_window_->setVisible(true);        
        return;
    }

    if(window == credits_window_)
    {
        credits_window_->setVisible(true);        
        return;
    }

    s_log << Log::warning << " Unhandled MainMenu::show call\n";

}

//------------------------------------------------------------------------------
void MainMenu::hide()
{
    // hide everything
    main_menu_->setVisible(false);
    notification_window_->setVisible(false);
    loading_screen_window_->setVisible(false);
    credits_window_->setVisible(false);

    // hide all other gui members
    options_menu_->hide();
    server_browser_->hide();
    host_menu_->hide();
//    client_authentication_->hide();

#ifdef TEASER_ENABLED
    teaser_screen_->hide();
#endif


}

//------------------------------------------------------------------------------
void MainMenu::onKeyDown(SDL_keysym sym)
{
    switch (sym.sym)
    {
    case SDLK_ESCAPE:
        // on Escape key, see if gui consumes the input.
        // if not, switch to client task, if available
        if(!s_gui.onKeyDown(sym) && client_task_ && client_task_->isConnected())
        {
            client_task_->focus();
        }                   
        break;
    default:
        s_gui.onKeyDown(sym);
        break;
    }
}

//------------------------------------------------------------------------------
void MainMenu::onFocusGained()
{
    if(s_params.get<bool>("client.music.enabled")) startMusic(true);
    s_app.enableKeyRepeat(true);
}

//------------------------------------------------------------------------------
void MainMenu::onFocusLost()
{
    stopMusic(true);
    s_app.enableKeyRepeat(false);
}

//------------------------------------------------------------------------------
void MainMenu::showNotification(const std::string & text)
{
    enableFloatingPointExceptions(false);    
    
    notification_label_->setText(text.c_str());
    show(notification_window_);

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
void MainMenu::showMainMenu()
{
    show(main_menu_);
}

//------------------------------------------------------------------------------
void MainMenu::setLoadingScreenText(const std::string & text)
{
    enableFloatingPointExceptions(false);
    loading_screen_label_->setText(text.c_str());
    show(loading_screen_window_);
    enableFloatingPointExceptions(true);
}

//------------------------------------------------------------------------------
/**
 *  this method is registered as task from TankAppClient, to delete TankAppClient
 *  avoiding errors if TankAppClient is deleting itself immediately
 */
void MainMenu::shutdownClient(void * user_data)
{
    delete client_task_;
    client_task_ = NULL;

    // This can happen if we lose the connection to the own server
    // somehow (e.g. another server is started on the same computer),
    // or if the client is shut down by an exception
    if (server_task_) shutdownServer(NULL);    
}

//------------------------------------------------------------------------------
void MainMenu::shutdownServer(void * user_data)
{
    delete server_task_;
    server_task_ = NULL;
}

//------------------------------------------------------------------------------
bool MainMenu::isClientRunning()
{
    return client_task_ != NULL;
}

//------------------------------------------------------------------------------
void MainMenu::stopMusic(bool fade)
{
    if(menu_music_)
    {
        menu_music_->pause(fade);
    }
}

//------------------------------------------------------------------------------
void MainMenu::startMusic(bool fade)
{
    if(menu_music_)
    {
        menu_music_->play(fade);
    }
}

//------------------------------------------------------------------------------
void MainMenu::connectPunch(const RakNetGUID& guid,
                            const SystemAddress & address,
                            unsigned internal_port)
{
    try
    {
        preConnect(address.ToString());
        client_task_->connectPunch(guid, address, internal_port);
    } catch (Exception & e)
    {
        onExceptionCaught(NULL, &e, EE_EXCEPTION_CAUGHT);        
    }
}

//------------------------------------------------------------------------------
void MainMenu::connect(const std::string & host, unsigned port)
{
    try
    {
        preConnect(host + ":" + toString(port));
        client_task_->connect(host, port);
    } catch (Exception & e)
    {
        onExceptionCaught(NULL, &e, EE_EXCEPTION_CAUGHT);        
    }
}

//------------------------------------------------------------------------------
void MainMenu::hostServer()
{
    assert(!client_task_ && !server_task_);

    try
    {
        connect("localhost", s_params.get<unsigned>("server.settings.listen_port"));

        // do loading for server after changing to loading screen...
        uint32_t id=0, key=0;
//        client_authentication_->getAuthData(id, key);
        server_task_ = new IntegratedServer(id, key);
        server_task_->getServer()->addObserver(ObserverCallbackFunUserData(this, &MainMenu::onExceptionCaught),
                                               EE_EXCEPTION_CAUGHT,
                                               &fp_group_);

        server_task_->start();

    } catch (Exception & e)
    {
        e.addHistory("MainMenu::clickedHostBtn");
        s_log << Log::error
              << e
              << "\n";
        onExceptionCaught(NULL, &e, EE_EXCEPTION_CAUGHT);
    }
}

//------------------------------------------------------------------------------
bool MainMenu::clickedQuitBtn(const CEGUI::EventArgs& e)
{
#ifdef TEASER_ENABLED    
    hide();
    teaser_screen_->show();
#else
    s_app.quit();
#endif
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::clickedHostBtn(const CEGUI::EventArgs& e)
{  
    // show server browser
    hide();
    host_menu_->show();
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::clickedJoinBtn(const CEGUI::EventArgs& e)
{
    // show server browser
    hide();
    server_browser_->show();
    return true;
}

//------------------------------------------------------------------------------
/**
*  At the moment on disconnect a running server is also closed.
*
**/
bool MainMenu::clickedDisconnectBtn(const CEGUI::EventArgs& e)
{
    shutdownClient(NULL);
    shutdownServer(NULL);

    show(main_menu_);
    return true;
}



//------------------------------------------------------------------------------
bool MainMenu::clickedJoinCancelBtn(const CEGUI::EventArgs& e)
{
    show(main_menu_);
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::clickedNotificationOkBtn(const CEGUI::EventArgs& e)
{
    show(main_menu_);
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::clickedContinueBtn(const CEGUI::EventArgs& e)
{
    assert(client_task_);
    client_task_->focus();
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::clickedOptionsBtn(const CEGUI::EventArgs& e)
{
    hide();
    options_menu_->show();
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::clickedCreditsBtn(const CEGUI::EventArgs& e)
{
    show(credits_window_);
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::clickedCreditsOkBtn(const CEGUI::EventArgs& e)
{
    show(main_menu_);
    return true;
}

//------------------------------------------------------------------------------
bool MainMenu::onKeyNotificationWindow(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::Return:  
            clickedNotificationOkBtn(e);
            return true;
        case CEGUI::Key::Space: 
            clickedNotificationOkBtn(e);
            return true;
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
bool MainMenu::onKeyDownJoinWindow(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
    case CEGUI::Key::Escape: // exit from join menu on Escape Key
            clickedJoinCancelBtn(e);
            return true;
    default:
        return false;
    }

    return false;
}

//------------------------------------------------------------------------------
void MainMenu::onExceptionCaught(Observable* ob, void* ex, unsigned event)
{
    assert(event == EE_EXCEPTION_CAUGHT);
    
    const Exception & e = *(Exception*)ex;    

    s_app.captureMouse(false);

    showNotification("ERROR: " +
                     e.getMessage());

    // Cannot delete right away because control flow might be
    // anywhere...
    s_scheduler.addEvent(SingleEventCallback(this, &MainMenu::shutdownClient),
                         0.0f,
                         NULL,
                         "MainMenu::shutdownClient",
                         &fp_group_);
}


//------------------------------------------------------------------------------
void MainMenu::preConnect(const std::string & server)
{    
    // We have no smart way of loading things yet, so music update
    // will occur too infrequently during loading -> stop music
    stopMusic(false);

    // We have no smart way of loading things yet, so mouse cannot move
    // on loading screen, disable it therefore
    s_app.captureMouse(true);
        
    // XXXX temporary loading screen
    setLoadingScreenText("Connecting to " + server + "...");
    render(); // XXX extremly ugly hack to show loading screen immediately

    // Create client. Successful connection will focus tankapp.
    uint32_t id=0, key=0;
//     client_authentication_->getAuthData(id, key);
    client_task_ = new TankApp(this, id, key);
    client_task_->addObserver(ObserverCallbackFunUserData(this, &MainMenu::onExceptionCaught),
                              EE_EXCEPTION_CAUGHT,
                              &fp_group_);
}


