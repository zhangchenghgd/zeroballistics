#ifndef TANKGAME_TANKCANNON_INCLUDED
#define TANKGAME_TANKCANNON_INCLUDED

#include <string>

#include "physics/OdeCollision.h"

#include "WeaponSystem.h"


class Tank;
class PlayerInput;
class RigidBody;

//------------------------------------------------------------------------------
class TankCannon : public WeaponSystem
{ 
 public:
    
    WEAPON_SYSTEM_IMPL(TankCannon);

    virtual ~TankCannon();


    virtual void frameMove(float dt);

    virtual bool fire(bool fire);
    
 protected:

    TankCannon();
};


#endif
