
#ifndef TANK_PARTICLE_CUSTOM_SHOOTER_INCLUDED
#define TANK_PARTICLE_CUSTOM_SHOOTER_INCLUDED


#include <osgParticle/RadialShooter>

#include "Camera.h"
#include "SceneManager.h"
#include "RigidBody.h"

using namespace osgParticle;




//------------------------------------------------------------------------------
/**
 *  This shooter is valid only for global effects.
 *
 *  If velocity scale is set to 1, the particles will inherit the
 *  exact translation of the emitter at the time they are spawned. If
 *  it is set to 2, the particles will inherit double the emitter's
 *  translation when they are spawned. 3 is triple the translation,
 *  etc.
 */
class VelocityScaleShooter : public RadialShooter
{
 public:
    VelocityScaleShooter();
    VelocityScaleShooter(float v_scale);

    VelocityScaleShooter( const VelocityScaleShooter& copy,
                          const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);

    META_Object(bluebeard, VelocityScaleShooter);


    virtual void shoot(osgParticle::Particle* p) const;

    void setVelocity(const osg::Vec3 & v);
    
 protected:
    osg::ref_ptr<osgParticle::Shooter> nested_shooter_;
    
    osg::Vec3 v_;
    float v_scale_;
};


//------------------------------------------------------------------------------
/**
 *  VelocityScaleShooter works by determining the difference in
 *  position of the underlying emitter.
 *
 *  This class supplies the neccessary information to the
 *  shooter. During the cull traversal of the emitter, it informs the
 *  shooter of the emitter's position.
 */
class VelocityScaleShooterCallback : public osg::NodeCallback
{
 public:
    VelocityScaleShooterCallback(VelocityScaleShooter * op);
    
    virtual void operator()(osg::Node *node, osg::NodeVisitor *nv);
    
 protected:
    
    osg::ref_ptr<VelocityScaleShooter> shooter_;

    Vector v_;
};



#endif
