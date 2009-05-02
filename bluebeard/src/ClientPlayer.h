
#ifndef TANK_PLAYER_CLIENT_INCLUDED
#define TANK_PLAYER_CLIENT_INCLUDED

#include <string>
#include <vector>
#include <map>


#include "PlayerInput.h"
#include "Datatypes.h"
#include "Player.h"

const unsigned HISTORY_SIZE = 30; // should not be much larger than
                                  // that, because of sequence number
                                  // wraparound at 255


class PuppetMasterClient;
class GameState;
class RigidBody;
class Observable;

namespace physics
{
    class OdeSimulator;
    class OdeHeightfieldGeom;
    class OdeRigidBody;
}

//------------------------------------------------------------------------------
/**
 *  Structure to remember player state in history.
 */
struct PlayerState
{
    uint8_t sequence_number_;
    std::vector<char> state_;
    PlayerInput input_;
};


//------------------------------------------------------------------------------
class RemotePlayer : public Player
{
 public:
    RemotePlayer(const SystemAddress & id) :
        Player(id) {}

    void setControllable(Controllable * controllable);
    
 protected:
    
};


//------------------------------------------------------------------------------
class LocalPlayer : public Player
{
 public:
    LocalPlayer();
    virtual ~LocalPlayer();

    void reset();

    void handleInput(const PlayerInput & input);
    void serverCorrection(uint8_t sequence_number, RakNet::BitStream & correct_state);

    void setId(const SystemAddress & id);
    void setControllable(Controllable * controllable);

    void incSequenceNumber();
    uint8_t getSequenceNumber() const;

    bool isLevelDataSet() const;
    void setLevelData(GameState * game_state);

    bool isHistoryOverflow();

    const physics::OdeSimulator * getReplaySimulator() const;

    Controllable * getReplaySimulatorControllable();
    
 protected:
    LocalPlayer(const LocalPlayer & other);

    static void advanceHistoryIndex(unsigned & index);

    void writeStateToHistory(const Controllable * c, unsigned index);
    void readStateFromHistory(Controllable * c, unsigned index);    

    void onRigidBodyDeleted(Observable* o, unsigned);
    
    uint8_t cur_sequence_number_;

    PlayerState history_[HISTORY_SIZE];
    unsigned hist_head_; ///< The oldest valid history entry.
    unsigned hist_tail_; ///< The index to fill with the next history entry.


    std::auto_ptr<physics::OdeSimulator> replay_simulator_; ///< This simulator is used
                                                            ///solely for history replay.
    std::auto_ptr<physics::OdeHeightfieldGeom> heightfield_geom_;

    Controllable * correct_controllable_;
    Controllable * test_controllable_;

    int history_size_; //XXXXX testing only

    bool history_overflow_;

    /// If a static object is deleted, we have to remove the
    /// corresponding OdeRigidBody from our replay simulator.
    std::map<RigidBody*, physics::OdeRigidBody*> static_body_; 
};


#endif
