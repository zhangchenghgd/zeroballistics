
#ifndef TANKGAME_INSTANT_HIT_WEAPON_INCLUDED
#define TANKGAME_INSTANT_HIT_WEAPON_INCLUDED


#include <vector>
#include <string>

#include <raknet/RakNetTypes.h>


#include "WeaponSystem.h"

#include "Vector.h"
#include "Scheduler.h"

#ifndef DEDICATED_SERVER

#include <osg/ref_ptr>

#include "SoundSource.h"

class ParticleEffectNode;

namespace osg
{
    class MatrixTransform;
}

#endif // #ifndef DEDICATED_SERVER






namespace physics
{
    struct CollisionInfo;
    class OdeSimulator;
}

class Tank;
class PlayerInput;
class RigidBody;

//------------------------------------------------------------------------------
class InstantHitWeapon : public WeaponSystem
{ 
 public:

    virtual ~InstantHitWeapon();
    
 protected:
    InstantHitWeapon();

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
class InstantHitWeaponServer : public InstantHitWeapon
{
 public:
    WEAPON_SYSTEM_IMPL(InstantHitWeaponServer);
    
 protected:

    virtual void doFire();
};

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
class InstantHitWeaponClient : public InstantHitWeapon
{
 public:
    WEAPON_SYSTEM_IMPL(InstantHitWeaponClient);

    virtual void init(Tank * tank, const std::string & section);
    
 protected:

    InstantHitWeaponClient();

    virtual bool startFiring();
    virtual bool stopFiring();
    virtual void onOverheat();

    void handleTracerDistance(float dt);
    void handleHitFeedback(void*);



    
    hTask task_tracer_dist_;
    hTask task_hitfeedback_;
    osg::ref_ptr<SoundSource> snd_firing_;

    std::vector<ParticleEffectNode*> tracer_effect_;    
};

#endif    // #ifndef DEDICATED_SERVER



#endif //#ifndef TANKGAME_TANK_MACHINE_GUN_INCLUDED
