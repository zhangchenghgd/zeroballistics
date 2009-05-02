#include "Controllable.h"

#include <raknet/BitStream.h>

#include "physics/OdeRigidBody.h"
#include "Matrix.h"
#include "Log.h"
#include "Quaternion.h"
#include "Datatypes.h"


const float STATE_ROT_THRESHOLD   = 0.004f;
const float STATE_TRANS_THRESHOLD = 0.0003f;


//------------------------------------------------------------------------------
Controllable::~Controllable()
{
    
}

//------------------------------------------------------------------------------
void Controllable::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    RigidBody::writeInitValuesToBitstream(stream);
}


//------------------------------------------------------------------------------
/**
 *  This is called on client side object creation. Set is_server_side_
 *  to false.
 */
void Controllable::readInitValuesFromBitstream(RakNet::BitStream & stream,
                                               GameState * game_state)
{
    RigidBody::readInitValuesFromBitstream(stream, game_state);

    warpProxy(true); // This is the only time the controllable proxy
                     // is warped
}



//------------------------------------------------------------------------------
/**
 *  For our controlled object, we don't want to send quantized state
 *  because this would break client side prediction.
 */
void Controllable::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    if (type & OST_CORE)
    {
        stream.Write(input_);
        writeCoreState(stream, !(type & OST_UNQUANTIZED)); 
    }
}

//------------------------------------------------------------------------------
void Controllable::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    if (type & OST_CORE)
    {
        stream.Read(input_);

        // Avoid warping proxy of controllable.
        readCoreState(stream, timestamp);
    }
}



//------------------------------------------------------------------------------
bool Controllable::isStateEqual(const Controllable * other) const
{
    ADD_STATIC_CONSOLE_VAR(float, state_diff_trans, 0.0f);
    ADD_STATIC_CONSOLE_VAR(float, state_diff_rot, 0.0f);
    
    Matrix trans  = target_object_->getTransform();
    Matrix trans2 = other->target_object_->getTransform();

    state_diff_trans = (trans.getTranslation() - trans2.getTranslation()).lengthSqr();
    if (state_diff_trans > STATE_TRANS_THRESHOLD)
    {
        s_log << Log::debug('n')
              << "State different: state_diff_trans is "
              << state_diff_trans
              << "\t\tTTTTTTTTTT\n";
        return false; 
    }

    Quaternion qu1(trans);
    Quaternion qu2(trans2);
    state_diff_rot = (qu1-qu2).lengthSqr();
    if (state_diff_rot > STATE_ROT_THRESHOLD)
    {
        s_log << Log::debug('n')
              << "State different: state_diff_rot is "
              << state_diff_rot
              << "\t\t\tRRRRRRRRRR\n";
        return false; 
    }

    return true;
}


//------------------------------------------------------------------------------
void Controllable::setPlayerInput(const PlayerInput & input)
{
    input_ = input;
}

//------------------------------------------------------------------------------
const PlayerInput & Controllable::getPlayerInput() const
{
    return input_;
}


//------------------------------------------------------------------------------
void Controllable::setLocallyControlled(bool b)
{
    is_locally_controlled_ = b;
}

//------------------------------------------------------------------------------
bool Controllable::isLocallyControlled() const
{
    return is_locally_controlled_;
}


//------------------------------------------------------------------------------
CONTROLLABLE_LOCATION Controllable::getLocation() const
{
    return location_;
}

//------------------------------------------------------------------------------
void Controllable::setLocation(CONTROLLABLE_LOCATION l)
{
    location_ = l;
}





//------------------------------------------------------------------------------
Controllable::Controllable() :
    is_locally_controlled_(false),
    location_(CL_CLIENT_SIDE)
{ 
}

//------------------------------------------------------------------------------
Controllable::Controllable(const Controllable & other, physics::OdeSimulator * sim) :
    RigidBody(other, sim),
    is_locally_controlled_(other.is_locally_controlled_),
    location_(other.location_)
{
}


//------------------------------------------------------------------------------
void Controllable::init()
{

}

