#ifndef TANKGAME_TANKCANNON_INCLUDED
#define TANKGAME_TANKCANNON_INCLUDED

#include <string>

#include "physics/OdeCollision.h"

#include "WeaponSystem.h"


class Tank;
class PlayerInput;
class RigidBody;


//------------------------------------------------------------------------------
class TankCannonServer : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(TankCannonServer);

 protected:

    virtual void doFire();
    
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class TankCannonClient : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(TankCannonClient);

 protected:

    virtual void doFire();

};


#endif

#endif
