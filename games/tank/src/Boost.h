#ifndef TANKGAME_BOOST_INCLUDED
#define TANKGAME_BOOST_INCLUDED

#include <string>

#include "physics/OdeCollision.h"

#include "WeaponSystem.h"


class Tank;
class PlayerInput;
class RigidBody;


//------------------------------------------------------------------------------
class BoostServer : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(BoostServer);

 protected:

    virtual bool startFiring();
    virtual bool stopFiring();
    virtual void onOverheat();
    
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class BoostClient : public WeaponSystem
{
 public:
    WEAPON_SYSTEM_IMPL(BoostClient);

 protected:

    virtual bool startFiring();
    virtual bool stopFiring();
    virtual void onOverheat();

};


#endif

#endif
