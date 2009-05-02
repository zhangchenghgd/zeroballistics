#ifndef TANK_MAINMENU_INCLUDED
#define TANK_MAINMENU_INCLUDED


#include "MetaTask.h"
#include "RegisteredFpGroup.h"

///#define TEASER_ENABLED  /// XXXX remove teaser screen for FMS launch


struct SystemAddress;


class TankApp;
class IntegratedServer;
class SdlTestApp;
class GUIOptions;
class GUIServerBrowser;
class GUIHostMenu;

class GUIClientAuthentication;
class OggStream;
class Observable;

#ifdef TEASER_ENABLED
class GUITeaser;
#endif

namespace CEGUI
{
    class ButtonBase;
    class Window;
    class Editbox;
    class EventArgs;
}

//------------------------------------------------------------------------------
class MainMenu : public MetaTask
{
public:
    MainMenu();
    virtual ~MainMenu();

    virtual void onKeyDown(SDL_keysym sym);

    virtual void onFocusGained();
    virtual void onFocusLost();

    void showNotification(const std::string & text);
    void showMainMenu();

    void setLoadingScreenText(const std::string & text);

    void shutdownClient(void * user_data);
    void shutdownServer(void * user_data);

    bool isClientRunning();

    void stopMusic(bool fade);
    void startMusic(bool fade);

    void connectPunch(const SystemAddress & address,
                      unsigned internal_port);
    void connect(const std::string & host, unsigned port);
    void hostServer();

    bool clickedQuitBtn(const CEGUI::EventArgs& e);  ///< used by auth. window exit
protected:

    void setupAndLoad();
    void registerCallbacks();
    void loadParameterFiles();
    void loadMusic();
    
    void show(CEGUI::Window * window);
    void hide();

    bool clickedHostBtn(const CEGUI::EventArgs& e);
    bool clickedJoinBtn(const CEGUI::EventArgs& e);
    bool clickedDisconnectBtn(const CEGUI::EventArgs& e);
    bool clickedJoinOkBtn(const CEGUI::EventArgs& e);
    bool clickedJoinCancelBtn(const CEGUI::EventArgs& e);
    bool clickedNotificationOkBtn(const CEGUI::EventArgs& e);
    bool clickedContinueBtn(const CEGUI::EventArgs& e);
    bool clickedOptionsBtn(const CEGUI::EventArgs& e);
    bool clickedCreditsBtn(const CEGUI::EventArgs& e);
    bool clickedCreditsOkBtn(const CEGUI::EventArgs& e);
    bool onKeyNotificationWindow(const CEGUI::EventArgs& e);
    bool onKeyDownJoinWindow(const CEGUI::EventArgs& e);
    bool onKeyDownScanningWindow(const CEGUI::EventArgs& e);

    //-------------------- Observer callbacks --------------------
    void onExceptionCaught(Observable* ob, void*, unsigned event);

    void preConnect(const std::string & server);
    

    CEGUI::Window * main_menu_;
    CEGUI::Window * notification_window_;

    CEGUI::ButtonBase * host_btn_;
    CEGUI::ButtonBase * join_btn_;
    CEGUI::ButtonBase * quit_btn_;
    CEGUI::ButtonBase * continue_btn_;
    CEGUI::ButtonBase * disconnect_btn_;
    CEGUI::ButtonBase * options_btn_;
    CEGUI::ButtonBase * credits_btn_;
    CEGUI::ButtonBase * notification_ok_btn_;
    CEGUI::Window * notification_label_;
    CEGUI::Window * loading_screen_window_;
    CEGUI::Window * loading_screen_label_;
    CEGUI::Window * credits_window_;
    CEGUI::ButtonBase * credits_ok_btn_;

    TankApp          * client_task_;
    IntegratedServer * server_task_; 
    
    std::auto_ptr<GUIOptions> options_menu_; 
    std::auto_ptr<GUIServerBrowser> server_browser_;
    std::auto_ptr<GUIHostMenu> host_menu_;
    std::auto_ptr<GUIClientAuthentication> client_authentication_;

#ifdef TEASER_ENABLED
    std::auto_ptr<GUITeaser> teaser_screen_;
#endif

    OggStream * menu_music_;

    RegisteredFpGroup fp_group_;
};

#endif
