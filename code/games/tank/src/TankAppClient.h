
#ifndef TANK_TANKAPP_INCLUDED
#define TANK_TANKAPP_INCLUDED


#include <raknet/RakPeerInterface.h>


#include "MetaTask.h"

#include "PlayerInput.h"
#include "SoundManager.h"

#include "GameState.h"
#include "PuppetMasterClient.h"
#include "SceneManager.h"
#include "RegisteredFpGroup.h"
#include "Scheduler.h"
#include "Observable.h"
#include "NetworkUtils.h"
#include "VersionInfo.h"
#include "VersionHandshakePlugin.h"


class RakClient;
class NetworkServer;
class GUIConsole;
class GUIProfiler;
class MainMenu;


//------------------------------------------------------------------------------
class TankApp : public MetaTask, public Observable
{
 public:
    TankApp(MainMenu * task,
            uint32_t player_id,
            uint32_t session_key);
    virtual ~TankApp();
    
    //-------------------- MetaTask part --------------------
    virtual void onFocusGained();
    virtual void onFocusLost();

    
    virtual void render();
    virtual void onKeyDown(SDL_keysym sym);
    virtual void onKeyUp(SDL_keysym sym);
    virtual void onMouseButtonDown(const SDL_MouseButtonEvent & event);
    virtual void onMouseButtonUp(const SDL_MouseButtonEvent & event);
    virtual void onMouseMotion(const SDL_MouseMotionEvent & event);
    virtual void onResizeEvent(unsigned width, unsigned height);



    bool isConnected() const;
    
    void connect(const std::string & host, unsigned port);
    void connectPunch(const RakNetGUID& guid,
                      const SystemAddress & address,
                      unsigned internal_port);
    
 protected:


    //-------------------- Network Functions --------------------
    void onConnectionRequestAccepted(const SystemAddress & player_id);
    bool onVersionInfoReceived(Packet * p, bool mismatch);
    
    //-------------------- Scheduler Callbacks --------------------
    void handleNetwork(float dt);
    void handlePhysics(float dt);




    //-------------------- Console functions --------------------
    std::string serverSet     (const std::vector<std::string> & args);
    std::string rcon          (const std::vector<std::string> & args);
    std::string startCapturing(const std::vector<std::string> & args);



    void startChat();
    void takeScreenshot();
    
    void stopCapturing();
    void writeFrame(float dt);

    void toggleProfiler();

    void addTasks();

    bool acceptGameNetworkPackets() const;

    network::ACCEPT_VERSION_CALLBACK_RESULT acceptVersionCallback(const VersionInfo & version);
    
    std::auto_ptr<PuppetMasterClient> puppet_master_;

    std::auto_ptr<GUIConsole> gui_console_;
    std::auto_ptr<GUIProfiler> gui_profiler_;

    MainMenu * main_menu_task_;

    bool is_chat_active_;

    std::string capture_name_;
    hTask capture_task_;
    
    RakPeerInterface * interface_;
    RegisteredFpGroup fp_group_;

    network::NetworkStatistics net_stats_;

    uint32_t player_id_;
    uint32_t session_key_;
};


#endif // TANK_TANKAPP_INCLUDED
