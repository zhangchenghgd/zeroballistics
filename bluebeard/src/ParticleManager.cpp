
#include "ParticleManager.h"

#include <osg/observer_ptr>

#include <osgParticle/Particle>
#include <osgParticle/ParticleProcessor>
#include <osgParticle/ModularEmitter>
#include <osgParticle/ModularProgram>

#include <osg/Vec3>
#include <osg/Node>

#include "SceneManager.h"
#include "EffectManager.h"


//------------------------------------------------------------------------------
/**
 *  Does the job of removing particle systems which have run out of particles.
 */
class SystemDeactivationUpdater : public osg::NodeCallback
{
public:
    virtual void operator()(osg::Node *node, osg::NodeVisitor *nv)
        {
            s_particle_manager.deactivateScheduledSystems();
        }
};



//------------------------------------------------------------------------------
ParticleManager::ParticleManager() : 
    ResourceManager<ParticleEffect>("particle_effects"),
    effects_disabled_(false)
{
#ifdef ENABLE_DEV_FEATURES
    s_console.addFunction("toggleParticleSystemUpdate",
                          Loki::Functor<void>(this, &ParticleManager::toggleParticleSystemUpdate),
                          &fp_group_);
#endif
}

//------------------------------------------------------------------------------
void ParticleManager::init()
{

    particle_system_updater_ = new osgParticle::ParticleSystemUpdater;
    particle_system_updater_->setName("ParticleSystemUpdater");

    geode_ = new osg::Geode();
    geode_->setName("ParticleEffectsGeode");

    group_ = new osg::Group();
    group_->setName("ParticleEffectsGroup");
    group_->addChild(particle_system_updater_.get());
    group_->addChild(geode_.get());
    group_->setUpdateCallback(new SystemDeactivationUpdater());

    s_scene_manager.getRootNode()->addChild(group_.get());
}

//------------------------------------------------------------------------------
void ParticleManager::reset()
{
    particle_system_updater_ = NULL;
    group_                   = NULL;
    geode_                   = NULL;
}


//------------------------------------------------------------------------------
ParticleEffectContainer ParticleManager::createEffect(const std::string & name,
                                                      bool activate)
{
    ParticleEffectContainer ret = getResource(name)->createParticleEffect();

    if (activate) activateSystem(ret.emitter_->getParticleSystem());
    
    return ret;
}


//------------------------------------------------------------------------------
unsigned ParticleManager::getNumParticles() const
{
    unsigned ret=0;
    
    for(unsigned c=0; c < particle_system_updater_->getNumParticleSystems(); c++)
    {
        ret += (particle_system_updater_->getParticleSystem(c)->numParticles() -
                particle_system_updater_->getParticleSystem(c)->numDeadParticles());
    }

    return ret;
}


//------------------------------------------------------------------------------
void ParticleManager::activateSystem(osgParticle::ParticleSystem * system)
{
    // If system is scheduled for deactivation, remove that schedule.
    std::set<osg::ref_ptr<osgParticle::ParticleSystem> >::iterator it =
        scheduled_system_.find(system);
    if (it != scheduled_system_.end()) scheduled_system_.erase(it);

    // bail if already activated.
    if (particle_system_updater_->containsParticleSystem(system)) return;

    
    // add particle system to the scene graph and updater
    particle_system_updater_->addParticleSystem(system);
    geode_                  ->addDrawable      (system);
    
    // set the render order of the particle effect, younger effects placed in front of older ones
    // this also allows ordering of particle effects inside particle groups
    unsigned render_order = particle_system_updater_->getParticleSystemIndex(system);
    system->getStateSet()->setRenderBinDetails(BN_TRANSPARENT + render_order, "DepthSortedBin");    
}



//------------------------------------------------------------------------------
bool ParticleManager::isSystemActive(osgParticle::ParticleSystem * system) const
{
    return (particle_system_updater_.get() &&
            particle_system_updater_->containsParticleSystem(system));
}


//------------------------------------------------------------------------------
void ParticleManager::scheduleForDeactivation(osgParticle::ParticleSystem * system)
{
    if (!isSystemActive(system)) return;
    scheduled_system_.insert(system);
}


//------------------------------------------------------------------------------
ParticleManager::~ParticleManager()
{
}

//------------------------------------------------------------------------------
void ParticleManager::deactivateScheduledSystems()
{    
    std::set<osg::ref_ptr<osgParticle::ParticleSystem> >::iterator it = scheduled_system_.begin();
    while (it != scheduled_system_.end())
    {
        osgParticle::ParticleSystem * sys = it->get();
        
        if (sys->areAllParticlesDead())
        {
            if (particle_system_updater_.get() &&
                particle_system_updater_->containsParticleSystem(sys))
            {
                particle_system_updater_->removeParticleSystem(sys);
                geode_                  ->removeDrawable      (sys);
            }

            scheduled_system_.erase(it++);
        } else ++it;
    }
} 






//------------------------------------------------------------------------------
/**
 *  For debug purposes only.
 */
void ParticleManager::toggleParticleSystemUpdate()
{
    effects_disabled_ ^= 1;
    
    if (effects_disabled_)
    {
        group_->removeChild(geode_.get());

        ToggleEffectsVisitor v(false);
        s_scene_manager.getRootNode()->accept(v);

        s_effect_manager.suspendEffectCreation(true);
        
    } else
    {
        group_->addChild(geode_.get());

        ToggleEffectsVisitor v(true);
        s_scene_manager.getRootNode()->accept(v);


        s_effect_manager.suspendEffectCreation(false);
    }
}



