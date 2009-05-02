
#ifndef TANKGAME_TANK_MACHINE_GUN_INCLUDED
#define TANKGAME_TANK_MACHINE_GUN_INCLUDED


#include <vector>
#include <string>

#include <raknet/RakNetTypes.h>

#include "Vector.h"
#include "Scheduler.h"

#ifndef DEDICATED_SERVER

#include <osg/ref_ptr>


#include "SoundSource.h"

namespace osg
{
    class MatrixTransform;
}

#endif // #ifndef DEDICATED_SERVER

#include "WeaponSystem.h"


namespace physics
{
    struct CollisionInfo;
    class OdeSimulator;
}

class Tank;
class PlayerInput;
class RigidBody;

//------------------------------------------------------------------------------
class TankMachineGun : public WeaponSystem
{ 
 public:

    virtual ~TankMachineGun();
    
 protected:
    TankMachineGun();

    virtual bool allowFiringAfterOverheat() const { return false; }
    
    void doRayTest(physics::OdeSimulator * sim);
    bool rayCollisionCallback(const physics::CollisionInfo & info);

    float ray_intersection_dist_;
    Vector hit_normal_;
    RigidBody * hit_body_;
    SystemAddress hit_player_;
    std::string hit_rigid_body_type_;
};


//------------------------------------------------------------------------------
class TankMachineGunServer : public TankMachineGun
{
 public:
    WEAPON_SYSTEM_IMPL(TankMachineGunServer);
    
 protected:

    virtual void doFire();
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class TankMachineGunClient : public TankMachineGun
{
 public:
    WEAPON_SYSTEM_IMPL(TankMachineGunClient);

    
 protected:

    TankMachineGunClient();

    virtual bool startFiring();
    virtual bool stopFiring();
    virtual void onOverheat();

    void handleTracerDistance(float dt);
    void handleHitFeedback(void*);

    hTask task_tracer_dist_;
    hTask task_hitfeedback_;
    osg::ref_ptr<SoundSource> snd_firing_;    
};

#endif    // #ifndef DEDICATED_SERVER



#endif //#ifndef TANKGAME_TANK_MACHINE_GUN_INCLUDED
