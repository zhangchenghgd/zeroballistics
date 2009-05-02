
#include "GUIClientAuthentication.h"

#include "MainMenu.h"
#include "Utils.h"

#include "Gui.h"
#include "RankingClientLogon.h"
#include "UserPreferences.h"
#include "ParameterManager.h" ///< XXX only used to set player_name to username
#include "md5.h"
#include "SdlApp.h"

#include "Ranking.h"

#include "SDL/SDL_syswm.h"

const char * CREATE_ACCOUNT_PAGE = "http://www.fullmetalsoccer.com/register.php";


//------------------------------------------------------------------------------
GUIClientAuthentication::GUIClientAuthentication(MainMenu * main_menu) :
    user_changed_credentials_(false),
    main_menu_(main_menu),
    id_         (network::ranking::INVALID_USER_ID),
    session_key_(network::ranking::INVALID_SESSION_KEY)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    registerCallbacks();

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
GUIClientAuthentication::~GUIClientAuthentication()
{
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::show()
{
    auth_window_->setVisible(true);
    auth_window_->activate();
    auth_message_->setText("");
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::hide()
{
    auth_window_->setVisible(false);
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::getAuthData(uint32_t & user_id,
                                          uint32_t & session_key)
{
    user_id     = id_;
    session_key = session_key_;
}


//------------------------------------------------------------------------------
void GUIClientAuthentication::registerCallbacks()
{
    connect_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIClientAuthentication::clickedConnectBtn, this));
    exit_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIClientAuthentication::clickedExitBtn, this));
    create_account_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIClientAuthentication::clickedCreateAccountBtn, this));

    username_editbox_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIClientAuthentication::onKeyUsernameEditbox, this));
    user_password_editbox_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIClientAuthentication::onKeyPasswordEditbox, this));
    remember_password_checkbox_->subscribeEvent(CEGUI::Checkbox::EventCheckStateChanged, CEGUI::Event::Subscriber(&GUIClientAuthentication::onRememberPasswordChanged, this));


    auth_window_->subscribeEvent(CEGUI::Window::EventKeyDown, CEGUI::Event::Subscriber(&GUIClientAuthentication::onKeyDownAuthWindow, this));
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::loadWidgets()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("MainMenu_root/");

    if(!parent)
    {
        throw Exception("GUIClientAuthentication is missing the root window.");
    }
    
    // Use parent window name as prefix to avoid name clashes
    wm.loadWindowLayout("clientauthentication.layout", parent->getName());
    auth_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "clientauthentication/window");
    auth_message_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "clientauthentication/auth_message_label");

    username_editbox_ = (CEGUI::Editbox*)wm.getWindow(parent->getName() + "clientauthentication/username_editbox");
    user_password_editbox_ = (CEGUI::Editbox*)wm.getWindow(parent->getName() + "clientauthentication/password_editbox");
    remember_password_checkbox_ = (CEGUI::Checkbox*)wm.getWindow(parent->getName() + "clientauthentication/remember_password_checkbox");

    connect_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "clientauthentication/connect_btn");
    exit_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "clientauthentication/exit_btn");
    create_account_btn_ = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "clientauthentication/create_account_btn");

    // add options to widget tree
    parent->addChildWindow(auth_window_);

    user_password_editbox_->setTextMasked(true);

    auth_window_->setVisible(false);

    // prefill username from client config file
    try
    {
        username_editbox_->setText(s_params.get<std::string>("player.username"));
    } catch(ParamNotFoundException e)
    {}

    remember_password_checkbox_->setSelected(s_params.get<bool>("player.remember_password"));
    if(remember_password_checkbox_->isSelected())
    {
        // prefill with placeholder char
        user_password_editbox_->setText("++++++");
    }
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::enableUserInput(bool enable)
{
    username_editbox_->setEnabled(enable);
    user_password_editbox_->setEnabled(enable);
    create_account_btn_->setEnabled(enable);
    connect_btn_->setEnabled(enable); 
    remember_password_checkbox_->setEnabled(enable);
}

//------------------------------------------------------------------------------
bool GUIClientAuthentication::clickedConnectBtn(const CEGUI::EventArgs& e)
{

    /// XXX developer shortcut to skip auth.
#ifdef ENABLE_DEV_FEATURES  
    CEGUI::MouseEventArgs* mouse_events =(CEGUI::MouseEventArgs*)&e;
    if(mouse_events->sysKeys == CEGUI::Control)
    {
        hide();
        main_menu_->showMainMenu();
        return true;
    }
#endif

    // real user handling
    enableUserInput(false);

    // fetch username
    std::string username = username_editbox_->getText().c_str();

    // handle user passwd. depending on passwd remembered or not
    std::string user_password = "";
    if(user_changed_credentials_ || !remember_password_checkbox_->isSelected())
    {
       user_password = user_password_editbox_->getText().c_str();
       user_password = hashString(user_password);
    }
    else
    {
       user_password = s_params.get<std::string>("player.password");
    }


    /// save user credentials entered
    s_params.set<std::string>("player.username", username);

    if(user_changed_credentials_ && remember_password_checkbox_->isSelected())
    {
        s_params.set<std::string>("player.password", user_password);
    }

    s_params.set<bool>("player.remember_password", remember_password_checkbox_->isSelected());
    s_params.set<std::string>("client.app.player_name", username);
    s_params.saveParameters(getUserConfigFile(), CONFIG_SUPERSECTION);



    network::ranking::ClientLogon * logon = new network::ranking::ClientLogon(username, user_password); 

    // add Observer event for logon object
    logon->addObserver( ObserverCallbackFunUserData(this, &GUIClientAuthentication::onConnectFailed),
                        network::ranking::CLOE_CONNECT_FAILED,
                        &fp_group_);

    // add Observer event for logon object
    logon->addObserver( ObserverCallbackFunUserData(this, &GUIClientAuthentication::onAuthorizationSuccessful),
                        network::ranking::CLOE_AUTHORIZATION_SUCCESSFUL,
                        &fp_group_);

    // add Observer event for logon object
    logon->addObserver( ObserverCallbackFunUserData(this, &GUIClientAuthentication::onAuthorizationFailed),
                        network::ranking::CLOE_AUTHORIZATION_FAILED,
                        &fp_group_);

    auth_message_->setText("Connecting...");
    logon->connect();

    return true;
}

//------------------------------------------------------------------------------
bool GUIClientAuthentication::clickedExitBtn(const CEGUI::EventArgs& e)
{
    hide();
    main_menu_->clickedQuitBtn(e);
    return true;
}

//------------------------------------------------------------------------------
bool GUIClientAuthentication::clickedCreateAccountBtn(const CEGUI::EventArgs& e)
{
#ifdef _WIN32

    /// minimize window
    HWND game_window = NULL;

    SDL_SysWMinfo wm_info;
    SDL_VERSION(&wm_info.version);

    if (SDL_GetWMInfo(&wm_info)) 
    { 
        game_window = wm_info.window;
    }

    if(game_window)
    {
        ShowWindow(game_window, SW_MINIMIZE);
    }


    // launch browser
    SHELLEXECUTEINFO SHInfo = {0};

    SHInfo.cbSize = sizeof (SHELLEXECUTEINFO);
    SHInfo.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    SHInfo.lpVerb = "open";
    SHInfo.lpFile = CREATE_ACCOUNT_PAGE;
    SHInfo.lpParameters = NULL;
    SHInfo.nShow = SW_SHOWNORMAL;

    if(!ShellExecuteEx(&SHInfo)) //execute cmd
    {
        char buf[256];
        DWORD dwErrorCode = GetLastError();
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, 256, 0);



        std::string msg = std::string("Error - go to ") + CREATE_ACCOUNT_PAGE;
        auth_message_->setText(msg.c_str());

        s_log << Log::error
              << "Failed to launch browser. Please navigate manually to: "
              << CREATE_ACCOUNT_PAGE
              << "\n System error: " << buf << "\n";

    }
#else

    int ret = system((std::string("sensible-browser ") + CREATE_ACCOUNT_PAGE).c_str());
    if (ret != 0)
    {
        std::string msg = std::string("Error - go to ") + CREATE_ACCOUNT_PAGE;
        auth_message_->setText(msg.c_str());
        
        s_log << Log::error
              << "Failed to launch browser. Please navigate manually to "
              << CREATE_ACCOUNT_PAGE
              << "\n";
    }
#endif
    
    return true;
}

//------------------------------------------------------------------------------
bool GUIClientAuthentication::onKeyDownAuthWindow(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
    case CEGUI::Key::Escape: // exit from options on Escape Key
          //  clickedExitBtn(e);
            return true;
    case CEGUI::Key::Return: // 
            clickedConnectBtn(e);
            return true;
    default:
        return false;
    }

    return false;
}

//------------------------------------------------------------------------------
bool GUIClientAuthentication::onKeyUsernameEditbox(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::Return:  
            clickedConnectBtn(e);
            return true;
        case CEGUI::Key::Tab: 
            user_password_editbox_->activate();
            return true;
        default:
            user_changed_credentials_ = true;
            return false;
    }
}

//------------------------------------------------------------------------------
bool GUIClientAuthentication::onKeyPasswordEditbox(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
        case CEGUI::Key::Return:  
            clickedConnectBtn(e);
            return true;
        case CEGUI::Key::Tab: 
            username_editbox_->activate();
            return true;
        default:
            user_changed_credentials_ = true;
            return false;
    }
}

//------------------------------------------------------------------------------
bool GUIClientAuthentication::onRememberPasswordChanged(const CEGUI::EventArgs& e)
{
    // if user deselects remember, clear the passwd. field
    if(!remember_password_checkbox_->isSelected())
    {
        user_password_editbox_->setText("");
    }

    return true;
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::onConnectFailed(Observable*, void* m, unsigned )
{
    const std::string & msg = *((const std::string*)m);
    
    auth_message_->setText(msg.c_str());
    enableUserInput(true);
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::onAuthorizationFailed(Observable * obsv, void* data, unsigned)
{
    std::string msg = *(std::string*)(data);
    auth_message_->setText(msg.c_str());
    enableUserInput(true);
}

//------------------------------------------------------------------------------
void GUIClientAuthentication::onAuthorizationSuccessful(Observable * obsv, void* data, unsigned)
{
    auth_message_->setText("Authorization successful.");

    network::ranking::ClientLogon * logon = (network::ranking::ClientLogon*)(obsv);

    id_          = logon->getUserId();
    session_key_ = logon->getSessionKey();

    enableUserInput(true);
    hide();

#ifndef _WIN32    
    s_app.toggleFullScreenIfEnabled();
#endif
    
    main_menu_->showMainMenu();
}
