#ifndef TANKGAME_MISSILE_LAUNCHER_INCLUDED
#define TANKGAME_MISSILE_LAUNCHER_INCLUDED

#include <string>

#include "physics/OdeCollision.h"

#include "WeaponSystem.h"


class Tank;
class PlayerInput;
class RigidBody;


//------------------------------------------------------------------------------
class MissileLauncherServer : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(MissileLauncherServer);

 protected:

    virtual void doFire();
    
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class MissileLauncherClient : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(MissileLauncherClient);

 protected:

    virtual void doFire();

};


#endif

#endif
