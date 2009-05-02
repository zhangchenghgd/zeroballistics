
#include "ParticleCustomShooter.h"

#include <osgParticle/ModularEmitter>

#include "UtilsOsg.h"


/// This is neccessary because physics and rendering don't go in
/// lockstep and we can only guess the velocity from the node's
/// position.
const float SHOOT_VELOCITY_TRACKING_SPEED = 0.1f;

//------------------------------------------------------------------------------
VelocityScaleShooter::VelocityScaleShooter() :
    v_scale_(0.0f)
{
}


//------------------------------------------------------------------------------
VelocityScaleShooter::VelocityScaleShooter(float v_scale) :
    v_scale_(v_scale)
{
}

//------------------------------------------------------------------------------
VelocityScaleShooter::VelocityScaleShooter( const VelocityScaleShooter& copy,
                                            const osg::CopyOp& copyop)
{
    assert(false);
}






//------------------------------------------------------------------------------
void VelocityScaleShooter::shoot(osgParticle::Particle* p) const
{
    RadialShooter::shoot(p);
    p->setVelocity(p->getVelocity() + v_*v_scale_);
}



//------------------------------------------------------------------------------
void VelocityScaleShooter::setVelocity(const osg::Vec3 & v)
{
    v_ *= 1.0f - SHOOT_VELOCITY_TRACKING_SPEED;
    v_ += v * SHOOT_VELOCITY_TRACKING_SPEED;
}
    

//------------------------------------------------------------------------------
VelocityScaleShooterCallback::VelocityScaleShooterCallback(VelocityScaleShooter * op) :
    shooter_(op),
    last_time_(0.0f)
{
}



//------------------------------------------------------------------------------
void VelocityScaleShooterCallback::operator()(osg::Node *node, osg::NodeVisitor *nv)
{    
    osgParticle::ModularEmitter * em = dynamic_cast<osgParticle::ModularEmitter*>(node);
    assert(em);
        

    if (last_time_==0.0f)
    {
        last_time_   = nv->getFrameStamp()->getSimulationTime();
    }  else
    {
        double shoot_dt = nv->getFrameStamp()->getSimulationTime()-last_time_;
        last_time_   = nv->getFrameStamp()->getSimulationTime();        

        if (!equalsZero(shoot_dt))
        {
            osg::Matrix inv;
            inv.invert(em->getPreviousLocalToWorldMatrix());
            inv.preMult(em->getLocalToWorldMatrix());

            shooter_->setVelocity(inv.getTrans() / shoot_dt);
        }
    }
    traverse(node, nv);
}
