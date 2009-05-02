
#include "Entity.h"



//------------------------------------------------------------------------------
Entity::Entity() :
    Matrix(true),
    v_local_ (Vector(0,0,0)),
    v_global_(Vector(0,0,0)),
    yaw_(0.0f),
    pitch_(0.0f),
    v_yaw_(0),
    v_pitch_(0),
    target_(true),
    is_tracking_(false),
    tracking_speed_(1.0f)
{
}


//------------------------------------------------------------------------------
void Entity::setTransform(const Matrix & mat)
{
    *(Matrix*)this = mat;
    (-mat.getZ()).getYawPitch(yaw_, pitch_);
}


//------------------------------------------------------------------------------
void Entity::frameMove(float dt)
{
    if (is_tracking_)
    {
        // off course, quaternion interpolation would be nicer
        // here.... implement someday...
        
        *(Matrix*)this += (target_ - *this) * tracking_speed_ * dt;
        orthonormalize();
        _44 = 1.0f;
    } else
    {
        // calculate the world translation caused by continous local
        // movement
        Vector delta_world = transformVector(v_local_);

        // add continous movement in world coordinates
        delta_world += v_global_;
        getTranslation() += dt * delta_world;


        yaw_   += dt*v_yaw_;
        pitch_ += dt*v_pitch_;
    
        loadOrientationPart(yaw_, pitch_);
    }
}


//------------------------------------------------------------------------------
void Entity::clearMotion()
{
    (-getZ()).getYawPitch(yaw_, pitch_);
    
    is_tracking_ = false;
    
    v_local_  = Vector(0,0,0);
    v_global_ = Vector(0,0,0);

    v_yaw_   = 0;
    v_pitch_ = 0;
}


//------------------------------------------------------------------------------
bool Entity::isTracking() const
{
    return is_tracking_;
}


//------------------------------------------------------------------------------
void Entity::setTargetTransform(const Matrix & target, float tracking_speed)
{
    is_tracking_    = true;
    target_         = target;
    tracking_speed_ = tracking_speed;
}

//------------------------------------------------------------------------------
const Matrix & Entity::getTargetTransform() const
{
    return target_;
}


//------------------------------------------------------------------------------
void Entity::setAngVelocity(float yaw, float pitch)
{
    v_yaw_   = yaw;
    v_pitch_ = pitch;
}

//------------------------------------------------------------------------------
void Entity::changeOrientation(float delta_yaw, float delta_pitch)
{
    yaw_   += delta_yaw;
    pitch_ += delta_pitch;
}


//------------------------------------------------------------------------------
Vector & Entity::getLocalVelocity()
{
    return v_local_;
}

//------------------------------------------------------------------------------
Vector & Entity::getGlobalVelocity()
{
    return v_global_;
}
