
#ifndef TANK_SERVER_PLAYER_INCLUDED
#define TANK_SERVER_PLAYER_INCLUDED

#include <string>
#include <vector>
#include <deque>


#include "PlayerInput.h"
#include "Datatypes.h"
#include "Controllable.h"
#include "Player.h"

struct SystemAddress;

//------------------------------------------------------------------------------
class ServerPlayer : public Player
{
 public:
    ServerPlayer(const SystemAddress & id);
    ServerPlayer(const ServerPlayer & other);
    virtual ~ServerPlayer();

    bool handleInput(uint8_t & seq_number);
    
    void setControllable(Controllable * controllable);
    
    void enqueueInput(uint8_t seq_number, const PlayerInput & input);
    float getTotalInputDelay() const;

    void setRconAuthorized(bool authorized);
    bool isRconAuthorized() const;

    unsigned getNeededReadies() const;
    void incNeededReadies();
    void decNeededReadies();

    
    
 protected:

    std::deque<std::pair<uint8_t, PlayerInput > > input_;

    bool send_correction_;  ///< Whether to send a correction after
                            ///the current input is done. Corrections
                            ///are sent for every second input
                            ///handled.
    
    unsigned cur_input_steps_; ///< Number of physic steps performed
                               ///with the input in input_[0].
    bool deque_overflow_; ///< If this is true, the next physics step
                          ///takes input_[0] as beginning of a new
                          ///current input.
    
    float total_delay_; ///< network + input buffer delay

    bool rcon_authorized_; ///< Is set true when player provided correct
                            ///rcon password

    uint8_t num_needed_readies_; ///< The client sends "ready" after
                                 ///he has completed loading a
                                 ///level. Only after this is he fully
                                 ///connected. If the level is changed
                                 ///while the client is still loading,
                                 ///it is essential of keeping track
                                 ///how many "ready" signals are yet
                                 ///to be transmitted.

    unsigned steps_input_deque_overfull_; ///< If the input deque is
                                          ///larger than 1 in size for
                                          ///a given amount of steps,
                                          ///drop input sooner to get
                                          ///deque size down.

    bool deque_underflow_; ///< Exists solely for debugging purposes.
    unsigned deque_size_;  ///< Exists solely for debugging purposes.
};

#endif
