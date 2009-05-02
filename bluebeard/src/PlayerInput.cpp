
#include "PlayerInput.h"

#include <string>

#include <raknet/BitStream.h>

#include "utility_Math.h"




//------------------------------------------------------------------------------
PlayerInput::PlayerInput()
{
    memset(this, 0, sizeof(PlayerInput));
}

//------------------------------------------------------------------------------
void PlayerInput::writeToBitstream (RakNet::BitStream & stream)
{
    stream.Write(up_);
    stream.Write(down_);
    stream.Write(left_);
    stream.Write(right_);

    stream.Write((bool)action1_);
    stream.Write((bool)action2_);
    stream.Write((bool)action3_);

    stream.Write((bool)fire1_);
    stream.Write((bool)fire2_);
    stream.Write((bool)fire3_);

    stream.Write(delta_yaw_);
    stream.Write(delta_pitch_);
}

//------------------------------------------------------------------------------
bool PlayerInput::readFromBitstream(RakNet::BitStream & stream)
{
    bool res = true;

    res &= stream.Read(up_);
    res &= stream.Read(down_);
    res &= stream.Read(left_);
    res &= stream.Read(right_);

    bool b;
    
    res &= stream.Read(b); action1_ = (INPUT_KEY_STATE)b;
    res &= stream.Read(b); action2_ = (INPUT_KEY_STATE)b;
    res &= stream.Read(b); action3_ = (INPUT_KEY_STATE)b;

    res &= stream.Read(b); fire1_ = (INPUT_KEY_STATE)b;
    res &= stream.Read(b); fire2_ = (INPUT_KEY_STATE)b;
    res &= stream.Read(b); fire3_ = (INPUT_KEY_STATE)b;

    res &= stream.Read(delta_yaw_);
    res &= stream.Read(delta_pitch_);
    
    return res;
}

//------------------------------------------------------------------------------
/**
 *  True if no player input is present.
 */
bool PlayerInput::isNull() const
{
    const static PlayerInput null_input;
    return memcmp(this, &null_input, sizeof(PlayerInput)) == 0;
}

//------------------------------------------------------------------------------
void PlayerInput::clear()
{
    memset(this, 0, sizeof(PlayerInput));
}


//-----------------------------------------------------------------------------------------
/**
 *  Set all keys marked as released to off.
 */
void PlayerInput::clearReleased()
{
    handleKeyState(action1_);
    handleKeyState(action2_);
    handleKeyState(action3_);

    handleKeyState(fire1_);
    handleKeyState(fire2_);
    handleKeyState(fire3_);
}

//------------------------------------------------------------------------------
/**
 *  
 */
void PlayerInput::handleKeyState(INPUT_KEY_STATE & state)
{
    if      (state == IKS_JUST_PRESSED)         state = IKS_DOWN;
    else if (state == IKS_PRESSED_AND_RELEASED) state = IKS_UP;
}
