

#ifndef TANK_PLAYER_INPUT_INCLUDED
#define TANK_PLAYER_INPUT_INCLUDED


//------------------------------------------------------------------------------
namespace RakNet
{
    class BitStream;
}


//------------------------------------------------------------------------------
/**
 *  Used to track key state during a sample interval. If the key is
 *  pressed and released again before the input is sampled, we want to
 *  get informed nevertheless.
 */
enum INPUT_KEY_STATE
{
    IKS_UP,
    IKS_DOWN,                // Pressed sometimes before the current frame.
    IKS_JUST_PRESSED,        // Pressed in the current frame.
    IKS_PRESSED_AND_RELEASED // Pressed and released before being handled in this frame. 
};

//------------------------------------------------------------------------------
class PlayerInput
{
 public:
    PlayerInput();

    void writeToBitstream (RakNet::BitStream & stream);
    bool readFromBitstream(RakNet::BitStream & stream);

    bool isNull() const;
    void clear();
    void clearReleased();
    
    bool up_;
    bool down_;
    bool left_;
    bool right_;

    INPUT_KEY_STATE fire1_;
    INPUT_KEY_STATE fire2_;
    INPUT_KEY_STATE fire3_;
    INPUT_KEY_STATE fire4_;

    INPUT_KEY_STATE action1_; // pickup beacon
    INPUT_KEY_STATE action2_; // handbrake 
    INPUT_KEY_STATE action3_; // jump

    float delta_yaw_;
    float delta_pitch_;

 protected:
    void handleKeyState(INPUT_KEY_STATE & state);
};


#endif
