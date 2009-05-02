
#include "ParticleCustomShooter.h"

#include <osgParticle/ModularEmitter>

#include "UtilsOsg.h"
#include "RigidBodyVisual.h"

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
    p->addVelocity(v_*v_scale_);
}



//------------------------------------------------------------------------------
void VelocityScaleShooter::setVelocity(const osg::Vec3 & v)
{
    v_ = v;
}
    

//------------------------------------------------------------------------------
VelocityScaleShooterCallback::VelocityScaleShooterCallback(VelocityScaleShooter * op) :
    shooter_(op)
{
}

//------------------------------------------------------------------------------
void VelocityScaleShooterCallback::operator()(osg::Node *node, osg::NodeVisitor *nv)
{
    traverse(node, nv);
    
    if (nv->getVisitorType() != osg::NodeVisitor::UPDATE_VISITOR) return;


    // Retrieve body velocity from update visitor user data
    if (nv->getUserData())
    {
        UpdateVisitorUserData * ud = dynamic_cast<UpdateVisitorUserData*>(nv->getUserData());
        assert(ud);
            
        v_ = ud->getVelocity();    
    } else v_ = Vector(0,0,0);
        
    osgParticle::ModularEmitter * em = dynamic_cast<osgParticle::ModularEmitter*>(node);
    assert(em);
        
    // shooter velocity is in local coordinates...
    // getWorldToLocalMatrix returns bogus results...
    osg::Matrix mat = em->getLocalToWorldMatrix();
    mat.setTrans(0,0,0);

    shooter_->setVelocity( mat * vecGl2Osg(v_) );        
}
