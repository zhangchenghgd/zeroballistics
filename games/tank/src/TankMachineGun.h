
#ifndef TANKGAME_TANK_MACHINE_GUN_INCLUDED
#define TANKGAME_TANK_MACHINE_GUN_INCLUDED


#include <vector>
#include <string>

#include <raknet/RakNetTypes.h>

#include "Vector.h"
#include "Scheduler.h"
#include "RegisteredFpGroup.h"

#ifndef DEDICATED_SERVER
#include <osg/ref_ptr>

namespace osg
{
    class MatrixTransform;
}


#endif

#include "WeaponSystem.h"


namespace physics
{
    struct CollisionInfo;
    class OdeSimulator;
}

class Tank;
class PlayerInput;
class SoundSource;
class RigidBody;

//------------------------------------------------------------------------------
class TankMachineGun : public WeaponSystem
{ 
 public:
    
    WEAPON_SYSTEM_IMPL(TankMachineGun);

    virtual ~TankMachineGun();

    virtual bool fire(bool fire);
    
 protected:

    TankMachineGun();

    void dealDamage(float dt);

    void doRayTest(physics::OdeSimulator * sim);
    
    bool rayCollisionCallback(const physics::CollisionInfo & info);
    

    float ray_intersection_dist_;
    Vector hit_normal_;
    RigidBody * hit_body_;
    SystemAddress hit_player_;
    std::string hit_rigid_body_type_;
    
#ifndef DEDICATED_SERVER

    void startFiringClient();
    void stopFiringClient();

    void handleTracerDistance(float dt);
    void handleHitFeedback(void*);

    hTask task_tracer_dist_;
    osg::ref_ptr<SoundSource> snd_firing_;

#endif    


    RegisteredFpGroup fp_group_;
    

};


#endif
