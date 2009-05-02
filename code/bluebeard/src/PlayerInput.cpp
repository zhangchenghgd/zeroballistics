
#include "PlayerInput.h"

#include <string>

#include <raknet/BitStream.h>

#include "utility_Math.h"


const float MAX_DELTA_YAW   = 14.0f;
const float MAX_DELTA_PITCH =  8.0f;


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
    stream.Write((bool)fire4_);

#ifdef DISABLE_NETWORK_OPTIMIZATIONS
    stream.Write(delta_yaw_  );
    stream.Write(delta_pitch_);
#else
    // writing delta for two reasons:
    // -) (more important) WriteCompressed doesn't reliable produce output 0 for input 0
    //    -> history replays when standing still
    // -) minor saving when player doesn't turn turret
    stream.WriteCompressedDelta(clamp(delta_yaw_   / MAX_DELTA_YAW  , -1.0f, 1.0f), 0.0f);
    stream.WriteCompressedDelta(clamp(delta_pitch_ / MAX_DELTA_PITCH, -1.0f, 1.0f), 0.0f);
#endif
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
    res &= stream.Read(b); fire4_ = (INPUT_KEY_STATE)b;

#ifdef DISABLE_NETWORK_OPTIMIZATIONS
    res &= stream.Read(delta_yaw_);
    res &= stream.Read(delta_pitch_);
#else
    // prev value always is zero
    delta_yaw_   = 0.0f;
    delta_pitch_ = 0.0f;
    res &= stream.ReadCompressedDelta(delta_yaw_);
    res &= stream.ReadCompressedDelta(delta_pitch_);
    
    delta_yaw_   *= MAX_DELTA_YAW;
    delta_pitch_ *= MAX_DELTA_PITCH;
#endif

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
    handleKeyState(fire4_);
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
