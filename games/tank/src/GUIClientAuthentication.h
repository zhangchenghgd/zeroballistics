
#ifndef TANK_GUICLIENTAUTHENTICATION_INCLUDED
#define TANK_GUICLIENTAUTHENTICATION_INCLUDED

#include <CEGUI/CEGUI.h>

#include "RegisteredFpGroup.h"

class MainMenu;
class Observable;

//------------------------------------------------------------------------------
class GUIClientAuthentication
{
 public:
    GUIClientAuthentication(MainMenu * main_menu);
    virtual ~GUIClientAuthentication();
    
    void show();
    void hide();

    void getAuthData(uint32_t & user_id, uint32_t & session_key);
    
 protected:

    void registerCallbacks();
    void loadWidgets();
    void enableUserInput(bool enable);

    bool clickedConnectBtn(const CEGUI::EventArgs& e);
    bool clickedExitBtn(const CEGUI::EventArgs& e);
    bool clickedCreateAccountBtn(const CEGUI::EventArgs& e);
    bool onKeyDownAuthWindow(const CEGUI::EventArgs& e);
    bool onKeyUsernameEditbox(const CEGUI::EventArgs& e);
    bool onKeyPasswordEditbox(const CEGUI::EventArgs& e);
    bool onRememberPasswordChanged(const CEGUI::EventArgs& e);
    
    //-------------------- Observer callbacks --------------------
    void onConnectFailed(Observable*, void*, unsigned);
    void onAuthorizationFailed(Observable * obsv, void* data, unsigned);
    void onAuthorizationSuccessful(Observable*, void*, unsigned);


    CEGUI::Window * auth_window_;
    CEGUI::Window * auth_message_;
    CEGUI::ButtonBase * exit_btn_;
    CEGUI::ButtonBase * connect_btn_;
    CEGUI::ButtonBase * create_account_btn_;

    CEGUI::Checkbox * remember_password_checkbox_;

    CEGUI::Editbox * username_editbox_;
    CEGUI::Editbox * user_password_editbox_;

    bool user_changed_credentials_;

    MainMenu * main_menu_;

    uint32_t id_;
    uint32_t session_key_;

    RegisteredFpGroup fp_group_;
};

#endif
