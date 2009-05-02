

#include "ProjectileVisual.h"


#include <osg/MatrixTransform>

#include "Missile.h"
#include "Projectile.h"
#include "UtilsOsg.h"
#include "AutoRegister.h"
#include "RigidBody.h"
#include "EffectManager.h"
#include "SoundSource.h"
#include "SoundManager.h"
#include "OsgNodeWrapper.h"


REGISTER_CLASS(GameObjectVisual, BallisticProjectileVisual);
REGISTER_CLASS(GameObjectVisual, MissileVisual);

const float PROJECTILE_FLY_REF_DIST = 0.8f;

//------------------------------------------------------------------------------
ProjectileVisual::~ProjectileVisual()
{
}


//------------------------------------------------------------------------------
/**
 *  Although the projectile has a spherical collision shape, we want
 *  it to always face into the moving direction.
 */
void ProjectileVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    traverse(node, nv);

    RigidBody * rb = (RigidBody*)object_;
    physics::OdeRigidBody * body = rb->getProxy() ? rb->getProxy() :rb->getTarget();
    assert(body);

    
    Matrix transform = body->getTransform();
    transform.loadOrientation(rb->getTarget()->getGlobalLinearVel(),
                              Vector(0.0f, 1.0f, 0.0f));

    osg_wrapper_->setTransform(transform);
}



//------------------------------------------------------------------------------
ProjectileVisual::ProjectileVisual()
{
}

//------------------------------------------------------------------------------
void ProjectileVisual::onModelChanged()
{
    RigidBodyVisual::onModelChanged();
    
    // particle trail
    s_effect_manager.createEffect("smoke_trail", osg_wrapper_->getOsgNode());

    // fly by sound effect
    SoundSource * snd = s_soundmanager.playLoopingEffect(s_params.get<std::string>("sfx.projectile"),
                                                         getWrapperNode()->getOsgNode());
    snd->setReferenceDistance(PROJECTILE_FLY_REF_DIST);
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void MissileVisual::onModelChanged()
{
    RigidBodyVisual::onModelChanged();
    
    // particle trail
    s_effect_manager.createEffect("missile_smoke", osg_wrapper_->getOsgNode());

    // fly by sound effect
    SoundSource * snd = s_soundmanager.playLoopingEffect(s_params.get<std::string>("sfx.projectile"),
                                                         getWrapperNode()->getOsgNode());
    snd->setReferenceDistance(PROJECTILE_FLY_REF_DIST);
}




