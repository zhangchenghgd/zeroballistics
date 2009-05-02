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
class TankMineDispenserServer : public WeaponSystem
{ 
 public:
    
    WEAPON_SYSTEM_IMPL(TankMineDispenserServer);


 protected:

    virtual void doFire();
    
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class TankMineDispenserClient : public WeaponSystem
{ 
 public:
    
    WEAPON_SYSTEM_IMPL(TankMineDispenserClient);

 protected:

    virtual void doFire();
};


#endif

#endif

