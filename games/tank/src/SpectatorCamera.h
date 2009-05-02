
#ifndef TANK_SPECTATOR_CAMERA_INCLUDED
#define TANK_SPECTATOR_CAMERA_INCLUDED


#include <raknet/RakNetTypes.h>


#include "Entity.h"
#include "RegisteredFpGroup.h"


//------------------------------------------------------------------------------
enum CAMERA_MODE
{
    CM_FREE,          // completely free camera movement
    CM_TRACKING_3RD_CONSTANT_DIR, // 3rd person, but keep constant
                                  // viewing direction (used as death
                                  // cam)
    CM_TV,            // look at target, but free camera movement
    CM_TRACKING_1ST,
    CM_TRACKING_3RD,  // 3rd person, rotate along with tank turret
    CM_LAST,
};


class GameLogicClientCommon;
class PlayerInput;
class Controllable;


//------------------------------------------------------------------------------
class SpectatorCamera : protected Entity
{
 public:
    SpectatorCamera(GameLogicClientCommon * game_logic);
    virtual ~SpectatorCamera();

    void clearMotion();
    
    void setTransform(const Matrix & trans);
    Matrix getTransform() const;
    
    void update(float dt);
    void handleInput(const PlayerInput & input);
    
    void trackPlayer(const SystemAddress & id);

    void setMode(CAMERA_MODE new_mode, bool hud_msg = false);
    CAMERA_MODE getMode() const;


    Controllable * getTargetControllable() const;
    const SystemAddress & getTargetPlayer() const;
    
    void changeTrackedPlayer(bool next);
    
 protected:

    void onTrackedObjectScheduledForDeletion();


    void moveOutsideTerrain(Matrix & trans) const;
    
    void modeFree();
    void modeTracking();
    void modeTv();
    void mode1St();
    void mode3rd();
    
    CAMERA_MODE camera_mode_;

    Vector last_target_position_; ///< Used in mode
                           ///CM_TRACKING_3RD_CONSTANT_DIR. Stores the
                           ///last position of the camera target, so
                           ///the camera can be properly updated as
                           ///the target moves along.
    Controllable * target_controllable_;
    SystemAddress  target_player_;


    GameLogicClientCommon * game_logic_;

    RegisteredFpGroup fp_group_;
};

#endif
