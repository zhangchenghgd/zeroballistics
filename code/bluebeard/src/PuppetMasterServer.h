

#ifndef TANK_PUPPETMASTER_SERVER_DEFINED
#define TANK_PUPPETMASTER_SERVER_DEFINED

#include <raknet/RakNetTypes.h>


#include "ServerPlayer.h"
#include "GameObject.h"
#include "RegisteredFpGroup.h"
#include "Observable.h"
#include "ServerAnnouncer.h"

class RakPeerInterface;
class GameState;
class GameLogicServer;


namespace network
{
    class NetworkCommandServer;
}


namespace network
{
namespace master
{
    class MasterServerRegistrator;
}
} 

//------------------------------------------------------------------------------
/**
 *  We need more fine-grained control of whom to send packets, because
 *  we don't want to spam players currently connecting with
 *  unimportant gamestate updates.
 */
enum COMMAND_SEND_TYPE
{
    CST_SINGLE,
    CST_BROADCAST_ALL,
    CST_BROADCAST_READY
};


//------------------------------------------------------------------------------
class HostOptions
{
 public:
    HostOptions() {}
    HostOptions(const std::string & level,
                const std::string & logic) :
        level_name_(level),
        game_logic_type_(logic) {}
    
    std::string level_name_;
    std::string game_logic_type_;
};



//------------------------------------------------------------------------------
enum PUPPET_MASTER_OBSERVABLE_EVENT
{
    PMOE_GAME_FINISHED,
    PMOE_LEVEL_LOADED,
    PMOE_PLAYER_JOINED,
    PMOE_AUTH_DATA_SET
};


//------------------------------------------------------------------------------
/**
 *
 */
class PuppetMasterServer : public Observable
{
 public:
    typedef std::list<ServerPlayer> PlayerContainer;

    PuppetMasterServer(RakPeerInterface * server_interface);
    virtual ~PuppetMasterServer();

    void reset();
    
    void frameMove(float dt);

    void handleInput(const SystemAddress & id,
                     const PlayerInput & input,
                     uint8_t sequence_number,
                     uint32_t timestamp);

    void sendGameState();
    
    void setControllable(const SystemAddress & id, Controllable * controllable);


    bool addPlayer   (const SystemAddress & pid);
    void removePlayer(const SystemAddress & pid);

    void requestPlayerReady(const SystemAddress & pid);
    void playerReady (const SystemAddress & pid);
    
    ServerPlayer * getPlayer(const SystemAddress & id);
    PlayerContainer & getPlayers();

    void setPlayerData(const SystemAddress & id,
                       const std::string & name,
                       uint32_t ranking_id,
                       uint32_t session_key);
    
    GameState * getGameState();
    GameLogicServer * getGameLogic();

    void rcon(const SystemAddress & id, std::string & cmd_and_args);
    void chat(const SystemAddress & id, std::string & msg);

    void addGameObject   (GameObject * object, bool send_creation_command = true);

    bool existsPlayer(const SystemAddress & id) const;

    void loadLevel(void * opts);
    
    void sendNetworkCommand(network::NetworkCommandServer & cmd,
                            const SystemAddress & id = UNASSIGNED_SYSTEM_ADDRESS,
                            COMMAND_SEND_TYPE type = CST_BROADCAST_ALL);

    void onGameFinished();

    void logPlayer(const SystemAddress & a);
    
    void sendServerMessage(const std::string & msg, const SystemAddress & dest = UNASSIGNED_SYSTEM_ADDRESS);

    void onRigidBodyActivated(Observable*, void* a, unsigned);

    const std::string & getLevelName();

    void setAuthData(uint32_t id, uint32_t key);

	RakPeerInterface * getRakPeerInterface();

 protected:

    void onStatsTransmissionFinished(Observable* ob, void* a, unsigned ev);
    void onStatsTransmissionFailed  (Observable*, void* a, unsigned);

    void updatePlayerAuthData(const ServerPlayer * player) const;

    void sendReliableState(Observable* rigid_body, unsigned event);

    void deleteScheduledObjects();


    // -------------------- Console Functions --------------------
    std::string loadLevelConsole     (const std::vector<std::string> & args);
    std::vector<std::string> loadLevelConsoleCompletion(const std::vector<std::string> & args);
    
    std::string kick                 (const std::vector<std::string> & args);
    std::string say                  (const std::vector<std::string> & args);

    void updateServerInfo();

    std::string getUniquePlayerName(const std::string original_name,
                                    const SystemAddress & player) const;
    
    void replaceObject(RigidBody * obj, const std::string & appendix_list);

    
    std::string level_name_;
    std::string logic_type_;
    
    std::auto_ptr<GameState> game_state_;
    
    PlayerContainer player_;
    
    RakPeerInterface * interface_;

    std::auto_ptr<GameLogicServer> game_logic_;
    std::auto_ptr<network::master::MasterServerRegistrator> master_server_registrator_;

    ServerAnnouncer announcer_;

    uint32_t user_id_;
    uint32_t session_key_;
    
    RegisteredFpGroup fp_group_;
};

#endif

