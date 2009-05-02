
#ifndef TANK_PARTICLE_MANAGER_INCLUDED
#define TANK_PARTICLE_MANAGER_INCLUDED

#include <osg/ref_ptr>
#include <osgParticle/ParticleSystemUpdater>

#include "ResourceManager.h"
#include "ParticleEffect.h"
#include "RegisteredFpGroup.h"

namespace osg
{
    class Group;
    class MatrixTransform;
}



#define s_particle_manager Loki::SingletonHolder<ParticleManager, Loki::CreateUsingNew, SingletonParticleManagerLifetime >::Instance()
//------------------------------------------------------------------------------
class ParticleManager : public ResourceManager<ParticleEffect>, public osg::NodeCallback
{
    DECLARE_SINGLETON(ParticleManager);

public:

    friend class SystemDeactivationUpdater;
    
    void init();
    void reset();

    ParticleEffectContainer createEffect(const std::string & name, bool activate);

    unsigned getNumParticles() const;


    void activateSystem         (osgParticle::ParticleSystem * system);
    bool isSystemActive         (osgParticle::ParticleSystem * system) const;
    void scheduleForDeactivation(osgParticle::ParticleSystem * system);
    
protected:

    virtual ~ParticleManager();

    void deactivateScheduledSystems();

    void toggleParticleSystemUpdate();


    osg::ref_ptr<osgParticle::ParticleSystemUpdater> particle_system_updater_;
    osg::ref_ptr<osg::Group> group_;
    osg::ref_ptr<osg::Geode> geode_;

    std::set<osg::ref_ptr<osgParticle::ParticleSystem> > scheduled_system_;

    bool effects_disabled_;
    
    RegisteredFpGroup fp_group_;
};


#endif
