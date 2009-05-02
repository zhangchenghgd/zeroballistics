#ifndef TANKGAME_SHIELD_INCLUDED
#define TANKGAME_SHIELD_INCLUDED

#include <string>

#include "physics/OdeCollision.h"

#include "WeaponSystem.h"


class Tank;
class PlayerInput;
class RigidBody;


//------------------------------------------------------------------------------
class ShieldServer : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(ShieldServer);

 protected:

    virtual void doFire();
    
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class ShieldClient : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(ShieldClient);

 protected:

    virtual void doFire();

};


#endif

#endif
