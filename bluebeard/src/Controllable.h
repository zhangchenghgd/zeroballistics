
#ifndef TANK_CONTROLLABLE_INCLUDED
#define TANK_CONTROLLABLE_INCLUDED

#include "RigidBody.h"

#include "PlayerInput.h"
#include "Matrix.h"

struct Color;


//------------------------------------------------------------------------------
enum CONTROLLABLE_LOCATION
{
    CL_SERVER_SIDE,
    CL_CLIENT_SIDE,
    CL_REPLAY_SIM
};


//------------------------------------------------------------------------------
/**
 *  Represents a Rigidbody that can be controlled by a player.
 */
class Controllable : public RigidBody
{
 public:
    virtual ~Controllable();

    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state);
    
    // used for state value transmission 
    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp);

    virtual bool isStateEqual(const Controllable * other) const;

    virtual Controllable * clone(physics::OdeSimulator * sim) = 0;

    void setPlayerInput(const PlayerInput & input);
    const PlayerInput & getPlayerInput() const;

    virtual void setLocallyControlled(bool b);
    bool isLocallyControlled() const;

    CONTROLLABLE_LOCATION getLocation() const;
    void setLocation(CONTROLLABLE_LOCATION l);
    
 protected:
    Controllable();
    Controllable(const Controllable & other, physics::OdeSimulator * sim);

    virtual void init();

    bool is_locally_controlled_; ///< Whether this controllable is controlled by the local player.

    CONTROLLABLE_LOCATION location_;
    
    PlayerInput input_;

};

#endif
