#ifndef TANKGAME_TANKMINEDISPENSER_INCLUDED
#define TANKGAME_TANKMINEDISPENSER_INCLUDED

#include <string>

#include "WeaponSystem.h"

class Tank;
class PlayerInput;


namespace physics
{
    struct CollisionInfo;
}


//------------------------------------------------------------------------------
class TankMineDispenser : public WeaponSystem
{ 
 public:
    
    WEAPON_SYSTEM_IMPL(TankMineDispenser);

    virtual ~TankMineDispenser();

    virtual bool fire(bool fire);

 protected:

    TankMineDispenser();

};


#endif
