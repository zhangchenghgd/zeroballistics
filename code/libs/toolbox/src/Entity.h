
#ifndef LIB_ENTITY_INCLUDED
#define LIB_ENTITY_INCLUDED

#include "Matrix.h"



//------------------------------------------------------------------------------
/**
 *  A matrix supplemented by interpolation and movement functions.
 */
class Entity : public Matrix
{
 public:
    Entity();

    void setTransform(const Matrix & mat);
    void frameMove(float dt);
    void clearMotion();

    bool isTracking() const;
    void setTargetTransform(const Matrix & target, float tracking_speed);
    const Matrix & getTargetTransform() const;
    
    void setAngVelocity(float yaw, float pitch);
    void changeOrientation(float delta_yaw, float delta_pitch);
    
    Vector & getLocalVelocity();
    Vector & getGlobalVelocity();
    
 protected:

    Vector v_local_;  ///< The entity is continuously
                                     ///moved by this amount.
    Vector v_global_; ///< The entity is continuously
                                     ///moved by
                                     ///this amount.
    
    float yaw_;        ///< The current rotation around the y-axis.
    float pitch_;      ///< The current rotation around the local x-axis.

    float v_yaw_;
    float v_pitch_;

    Matrix target_;
    bool is_tracking_;
    float tracking_speed_;
};


#endif // #ifndef LIB_ENTITY_INCLUDED
