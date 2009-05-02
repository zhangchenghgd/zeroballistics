

#ifndef TANK_PUPPETMASTER_CLIENT_DEFINED
#define TANK_PUPPETMASTER_CLIENT_DEFINED

#include <list>


#include <raknet/BitStream.h>

#include "ClientPlayer.h"
#include "GameObject.h"
#include "NetworkCommandServer.h"
#include "Console.h"

class RakPeerInterface;
class PlayerInput;
class GameState;
class GameLogicClient;
class GameHud;
class LodUpdater;

//------------------------------------------------------------------------------
/// \see OdeSimulator::renderGeoms
enum CONTACT_CATEGORY_CLIENT
{
    CCC_STATIC,
    CCC_PROXY,       ///< Interpolated proxies.
    CCC_CONTROLLED,  ///< "True" position of user controlled object.
    CCC_LAST
};



//------------------------------------------------------------------------------
/**
 *
 */
class PuppetMasterClient
{
 public:
    PuppetMasterClient(RakPeerInterface * client_interface);
    virtual ~PuppetMasterClient();    

    void frameMove(float dt);
    
    void reset();
    void handleStringMessage(network::STRING_MESSAGE_TYPE type,
                             const std::string & message,
                             const SystemAddress & pid1);
    
    LocalPlayer * getLocalPlayer();

    void setControllable(const SystemAddress & id, uint16_t controllable_id);

    void onRequestReady();

    void addRemotePlayer(const SystemAddress & id);
    void deleteRemotePlayer(const SystemAddress & id);
    void clearRemotePlayers();
    RemotePlayer * getRemotePlayer(const SystemAddress & id);

    void addGameObject(GameObject * object);
    void deleteGameObject(uint16_t id);

    GameState * getGameState();

    GameLogicClient * getGameLogic();

    const std::list<RemotePlayer> & getConnectedPlayers();

    bool isConnectionProblem();
    
    void sendChat(const std::string & msg);

    GameHud * getHud();

    RakPeerInterface * getInterface();

    void loadLevel(const std::string & map_name,
                   const std::string & game_logic_type);
    
 protected:

    std::string changePlayerName     (const std::vector<std::string> & args);
    std::string setFov               (const std::vector<std::string> & args);
    std::string setHither            (const std::vector<std::string> & args);
    std::string setYon               (const std::vector<std::string> & args);
    std::string dumpSimulatorContents(const std::vector<std::string> & args);
    std::string dumpSpaceContents    (const std::vector<std::string> & args);

    void loadLevelResources(const std::string & lvl_name);
    void saveLevelResources() const;

    void handleInput(float dt);


    // Input handling functions
    void up     (bool b);
    void down   (bool b);
    void right  (bool b);
    void left   (bool b);
    void fire1  (bool b);
    void fire2  (bool b);
    void fire3  (bool b);
    void action1(bool b);
    void action2(bool b);
    void action3(bool b);

    void mouseMotion(Vector2d pos, Vector2d delta);

    void setKeyState(INPUT_KEY_STATE & state, bool b);
    
    PlayerInput cur_input_;
    
    const std::auto_ptr<GameState> game_state_;

    std::auto_ptr<GameLogicClient> game_logic_; 

    LocalPlayer local_player_;

    typedef std::list<RemotePlayer> RemotePlayerContainer;
    RemotePlayerContainer remote_player_;

    bool connection_problem_; ///< Gets set when the player history
                              ///array overflows (no server
                              ///corrections are arriving).

    RakPeerInterface * client_interface_;

    std::auto_ptr<GameHud> hud_;

    RegisteredFpGroup fp_group_;
};

#endif
