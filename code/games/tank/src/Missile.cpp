

#include "Missile.h"

#include "AutoRegister.h"
#include "Tank.h"


#include "physics/OdeCollision.h"
#include "physics/OdeSimulator.h"



#include "GameLogicServerCommon.h"
#include "GameState.h"
#include "Scheduler.h"
#include "WeaponSystem.h"

#undef min
#undef max

REGISTER_CLASS(GameObject, Missile);



const float AIM_RAY_SEGMENT_LENGTH = 10.0f;


//------------------------------------------------------------------------------
Missile::~Missile()
{
}

//------------------------------------------------------------------------------
void Missile::frameMove(float dt)
{
    Projectile::frameMove(dt);


    updateTarget(dt);


    Vector cur_dir = getGlobalLinearVel();
    cur_dir.normalize();

    Vector target_dir = target_ - getPosition();
    float l = target_dir.length();
    if (equalsZero(l)) return;
    target_dir /= l;

    float alpha = acosf(vecDot(&cur_dir, &target_dir));

    if (alpha > agility_*dt )
    {
        Matrix m;
        Vector axis;
        vecCross(&axis, &target_dir, &cur_dir);
        if (!equalsZero(axis.length()))
        {
            m.loadRotationVector(agility_*dt, axis);
            target_dir = m.transformVector(cur_dir);
        } else target_dir = cur_dir;
    }
    setGlobalLinearVel(target_dir*speed_);    
}



//------------------------------------------------------------------------------
void Missile::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    Projectile::writeInitValuesToBitstream(stream);

    stream.Write(speed_);
    stream.Write(agility_);
}


//------------------------------------------------------------------------------
void Missile::readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp)
{
    Projectile::readInitValuesFromBitstream(stream, game_state, timestamp);

    stream.Read(speed_);
    stream.Read(agility_);
}


//------------------------------------------------------------------------------
void Missile::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    if (type == OST_BOTH)
    {
        Projectile::writeStateToBitstream(stream, type);
    } else
    {
        // orientation doesn't matter for missile
        Vector v;
        v = getPosition();
        stream.WriteVector(v.x_,v.y_,v.z_);
        
        v = target_object_->getGlobalLinearVel();
        stream.WriteVector(v.x_,v.y_,v.z_);
    }
    
    stream.WriteVector(target_.x_, target_.y_, target_.z_);
}


//------------------------------------------------------------------------------
void Missile::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    if (type == OST_BOTH)
    {
        Projectile::readStateFromBitstream(stream, type, timestamp);
    } else
    {
        Vector v;
        stream.ReadVector(v.x_,v.y_,v.z_);
        setPosition(v);
        
        stream.ReadVector(v.x_,v.y_,v.z_);
        setGlobalLinearVel(v);
    }
    
    stream.ReadVector(target_.x_, target_.y_, target_.z_);
}


//------------------------------------------------------------------------------
void Missile::setTank(Tank * tank)
{
    tank_ = tank;
    tank_->addObserver(ObserverCallbackFun0(this, &Missile::onTankDeleted),
                       GOE_SCHEDULED_FOR_DELETION,
                       &fp_group_);

    assert(!section_.empty());
    speed_   = s_params.get<float>(section_ + ".muzzle_velocity");
    agility_ = s_params.get<float>(section_ + ".agility");    

    s_scheduler.addEvent(SingleEventCallback(this, &Missile::enableAiming),
                         s_params.get<float>(section_ + ".aiming_delay"),
                         NULL,
                         "Missile::enableAiming",
                         &fp_group_);

    if (!WeaponSystem::weapon_system_debug_)
    {
        s_scheduler.addEvent(SingleEventCallback(this, &Missile::destroy),
                             s_params.get<float>(section_ + ".range") / s_params.get<float>(section_ + ".muzzle_velocity"),
                             NULL,
                             "Missile::destroy",
                             &fp_group_);
    }


    max_num_segments_ = (unsigned)std::ceil(s_params.get<float>(section_ + ".range") / AIM_RAY_SEGMENT_LENGTH);
}


//---------------------------------------------------------------------------------------
void Missile::initPosAndVelocity(const Matrix & t)
{
    setTransform(t);
    setGlobalLinearVel(-t.getZ() * s_params.get<float>(section_ + ".muzzle_velocity"));
    target_ = t.getTranslation() - s_params.get<float>(section_ + ".range")*t.getZ();
}




//------------------------------------------------------------------------------
void Missile::updateTarget(float dt)
{
    if (!tank_ || !game_logic_server_ || !aiming_enabled_) return;

    physics::OdeRayGeom ray(AIM_RAY_SEGMENT_LENGTH);

    Matrix muzzle_trans;
    tank_->getMuzzleTransform(&muzzle_trans);

    Vector origin = muzzle_trans.getTranslation();
    Vector dir    = -muzzle_trans.getZ();
    
    ray.set(origin + dir*AIM_RAY_SEGMENT_LENGTH*cur_ray_segment_, dir);
        
    physics::OdeCollisionSpace * space1 =
        game_logic_server_->getPuppetMaster()->getGameState()->getSimulator()->getStaticSpace();
    physics::OdeCollisionSpace * space2 =
        game_logic_server_->getPuppetMaster()->getGameState()->getSimulator()->getActorSpace();
    assert(space1);
    assert(space2);


    penetration_depth_ = 0.0f;
    space1->collideRayMultiple(&ray, physics::CollisionCallback(this, &Missile::rayCollisionCallback));
    space2->collideRayMultiple(&ray, physics::CollisionCallback(this, &Missile::rayCollisionCallback));

    if (penetration_depth_ == 0.0f)
    {
        if (++cur_ray_segment_ >= max_num_segments_)
        {
            cur_ray_segment_ = 0;
            target_ = origin + dir*AIM_RAY_SEGMENT_LENGTH*max_num_segments_;
        } 
    } else
    {
        cur_ray_segment_ = 0;
    }
}


//------------------------------------------------------------------------------
void Missile::enableAiming(void*)
{
    aiming_enabled_ = true;
}


//------------------------------------------------------------------------------
void Missile::destroy(void*)
{
    assert(game_logic_server_);
    physics::CollisionInfo info;
    info.pos_ = getPosition();
    info.n_ = Vector(0.0f, 1.0f, 0.0f);
    game_logic_server_->onProjectileHit(this, NULL,
                                        1.0f,
                                        info,
                                        true);

    scheduleForDeletion();
}


//------------------------------------------------------------------------------
void Missile::onTankDeleted()
{
    tank_ = NULL;
}


//------------------------------------------------------------------------------
bool Missile::rayCollisionCallback(const physics::CollisionInfo & info)
{
    if (info.other_geom_->getUserData() == tank_) return false;
    // penetration for ray is distance to origin!!
    if (penetration_depth_ == 0.0f || info.penetration_ < penetration_depth_)
    {
        penetration_depth_ = info.penetration_;
        target_ = info.pos_;
    }
    
    return false;
}




//------------------------------------------------------------------------------
Missile::Missile() :
    tank_(NULL),
    target_(0.0f, 0.0f ,0.0f),
    penetration_depth_(0.0f),
    aiming_enabled_(false),
    cur_ray_segment_(0)
{
}

