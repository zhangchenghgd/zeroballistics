
#include "EffectManager.h"

#include <tinyxml.h>


#include <osgParticle/ModularEmitter>
#include <osgParticle/ModularProgram>
#include <osgParticle/ConnectedParticleSystem>


#include "SceneManager.h"
#include "ParticleManager.h"
#include "SoundManager.h"
#include "SoundSource.h"
#include "Paths.h"
#include "TinyXmlUtils.h"

const std::string FILENAME_EXTENSION = ".xml";





//------------------------------------------------------------------------------
EffectGroup::EffectGroup(const std::string & filename) :
    name_(filename)
{
    parseEffectGroup();
}

//------------------------------------------------------------------------------
EffectGroup::~EffectGroup()
{

}

//------------------------------------------------------------------------------
const std::vector<ParticleEffectDesc> & EffectGroup::getParticleEffects() const
{
    return particle_effects_;
}

//------------------------------------------------------------------------------
const std::vector<SoundEffectDesc>    & EffectGroup::getSoundEffects   () const
{
    return sound_effects_;
}

//------------------------------------------------------------------------------
void EffectGroup::parseEffectGroup()
{
    using namespace tinyxml_utils;

    TiXmlDocument xml_doc;
    TiXmlHandle root_handle = getRootHandle(EFFECTS_PATH + name_ + FILENAME_EXTENSION, xml_doc);

    if (std::string("EffectGroup") != root_handle.ToElement()->Value())
    {
        Exception e;
        e << (EFFECTS_PATH + name_ + FILENAME_EXTENSION) << " is not a valid effects xml file.\n";
        throw e;
    }

    // loop through ParticleEffect Elements
    for (TiXmlElement* child = root_handle.ToElement()->FirstChildElement("ParticleEffect");
         child;
         child=child->NextSiblingElement("ParticleEffect"))
    {
        ParticleEffectDesc pe_desc;
        pe_desc.name_ = getAttributeString(child, "name");
        pe_desc.delay_ = getAttributeFloat(child, "delay");

        particle_effects_.push_back(pe_desc);

        // Pre-cache particle effect
        s_particle_manager.getResource(pe_desc.name_, false);
    }


    // loop through SoundEffect Elements
    for (TiXmlElement* child = root_handle.ToElement()->FirstChildElement("SoundEffect");
         child;
         child=child->NextSiblingElement("SoundEffect"))
    {
        SoundEffectDesc desc;
        desc.name_ = getAttributeString(child, "name");
        desc.loop_ = getAttributeBool  (child, "loop");

        sound_effects_.push_back(desc);

        // Pre-cache sound effect
        s_soundmanager.getResource(desc.name_, false);
    }

}


//------------------------------------------------------------------------------
EffectNode::EffectNode() :
    del_when_done_(true)
{
}


//------------------------------------------------------------------------------
EffectNode::EffectNode(const EffectNode & n, const osg::CopyOp & op) :
    osg::MatrixTransform(n,op),
    del_when_done_(false)
{
}

//------------------------------------------------------------------------------
void EffectNode::setDelWhenDone(bool d)
{
    del_when_done_ = d;
}



//------------------------------------------------------------------------------
SoundEffectNode::SoundEffectNode() :
    prev_enabled_(false)
{
}


//------------------------------------------------------------------------------
SoundEffectNode::SoundEffectNode(const SoundEffectNode & n, const osg::CopyOp & op) :
    EffectNode(n, op),
    prev_enabled_(false)
{
}


//------------------------------------------------------------------------------
/**
 *  Commit suicide if no sound effects left.
 */
void SoundEffectNode::traverse(osg::NodeVisitor &nv)
{
    EffectNode::traverse(nv);
    if (nv.getVisitorType() != osg::NodeVisitor::CULL_VISITOR) return;
    
    if (getNumChildren() == 0)
    {
        assert(getNumParents() == 1);
        assert(getParent(0)->asGroup());

        s_log << Log::debug('S')
              << getName()
              << " SoundEffectNode suicide\n";
        // Cannot directly delete in traversal....
        s_scene_manager.scheduleNodeForDeletion(this);        
    }
}



//------------------------------------------------------------------------------
void SoundEffectNode::fire()
{
    setEnabled(false);
    setEnabled(true);
}

//------------------------------------------------------------------------------
void SoundEffectNode::setEnabled(bool e)
{
    if (e == prev_enabled_) return;
    prev_enabled_ = e;
    
    for (unsigned i=0; i<getNumChildren(); ++i)
    {
        SoundSource * source = dynamic_cast<SoundSource*>(getChild(i));
        assert(source);
        
        source->play(e);
    }
}


//------------------------------------------------------------------------------
unsigned SoundEffectNode::getNumEffects() const
{
    return getNumChildren();
}


//------------------------------------------------------------------------------
SoundSource * SoundEffectNode::getEffect(unsigned n)
{
    assert(dynamic_cast<SoundSource*>(getChild(n)));
    return (SoundSource*)getChild(n);
}



//------------------------------------------------------------------------------
ParticleEffectNode::ParticleEffectNode()
{
}


//------------------------------------------------------------------------------
/**
 *  Our effect_ struct must hold pointers to the correct emitter and
 *  program.
 */
ParticleEffectNode::ParticleEffectNode(const ParticleEffectNode & n, const osg::CopyOp & op) :
    EffectNode(n, op)
{
    NodeUserData * data = dynamic_cast<NodeUserData*>(getUserData());
    bool start_disabled = !data->isActive();
    
    // num children must be even (em+pr)
    assert(!(getNumChildren() & 1));
    for (unsigned i=0; i<getNumChildren()>>1; ++i)
    {
        osgParticle::ModularEmitter * em = dynamic_cast<osgParticle::ModularEmitter*>(getChild(2*i  ));
        osgParticle::ModularProgram * p  = dynamic_cast<osgParticle::ModularProgram*>(getChild(2*i+1));
        assert(em); assert(p);

        // Need to manually clone particle system...
        osgParticle::ParticleSystem * clone = new osgParticle::ParticleSystem(*em->getParticleSystem());
        em->setParticleSystem(clone);
        p ->setParticleSystem(clone);

        // This effect is created by cloning a prototype model. It
        // will be enabled by enabling a corresponding group.
        if (start_disabled)
        {
            em->setEnabled(false);
        }
    }
    
    fire();
}

//------------------------------------------------------------------------------
/**
 *  Particle systems must live independently from effect node so they
 *  can finish after emitter and program are dead.
 */
ParticleEffectNode::~ParticleEffectNode()
{
    // trigger removal of particle system by Particle Manager removal
    // callback after effect has finished
    for (unsigned i=0; i<getNumChildren()>>1; ++i)
    {
        osgParticle::Emitter * em = dynamic_cast<osgParticle::Emitter*>(getChild(2*i));
        assert(em);
        s_particle_manager.scheduleForDeactivation(em->getParticleSystem());
    }
}


//------------------------------------------------------------------------------
/**
 *  Remove any effect which have completed their lifespan. Delete self
 *  if no active effects are left.
 */
void ParticleEffectNode::traverse(osg::NodeVisitor &nv)
{
    EffectNode::traverse(nv);
    
    if (nv.getVisitorType() != osg::NodeVisitor::CULL_VISITOR) return;
    
    float dist = nv.getDistanceToEyePoint(osg::Vec3(0,0,0),false);

    assert(!(getNumChildren() & 1));

    for (int c=0; c < (int)getNumChildren()/2; ++c)
    {
        assert(dynamic_cast<osgParticle::ModularEmitter*>(getChild(2*c)));
        osgParticle::ModularEmitter * em = (osgParticle::ModularEmitter*)getChild(2*c);
        
        // LOD looks bad for connected systems...
        if (!dynamic_cast<osgParticle::ConnectedParticleSystem*>(em->getParticleSystem()))
        {
            em->getParticleSystem()->setLevelOfDetail(
                1+(int)(dist * s_params.get<float>("client.graphics.particle_lod_scale")));
        }
        
        // remove effects that have spent their lifetime
        //
        // an endless particle effect isn't neccessarily alive!
        if (em->getParticleSystem()->areAllParticlesDead() && 
            ((!em->isEndless() && !em->isAlive()) || !em->isEnabled()))
        {            
            s_particle_manager.scheduleForDeactivation(em->getParticleSystem());

            if (del_when_done_)
            {
                bool r = removeChild(2*c,2);
                UNUSED_VARIABLE(r);
                assert(r);

                --c; // fix for deletion
            }
        }
    }

    // If all effects of this node are history, commit suicide.
    if (getNumChildren() == 0)
    {
        assert(getNumParents() == 1);
        assert(getParent(0)->asGroup());

        // Cannot directly delete in traversal....
        s_scene_manager.scheduleNodeForDeletion(this);
    }
}



//------------------------------------------------------------------------------
void ParticleEffectNode::fire()
{
    NodeUserData * data = dynamic_cast<NodeUserData*>(getUserData());
    if (!data->isActive()) return;

    for (unsigned i=0; i<getNumEffects(); ++i)
    {
        ParticleEffectContainer ec = getEffect(i);
        ec.emitter_->setEnabled(true);
        s_particle_manager.activateSystem(ec.emitter_->getParticleSystem());        
    }    
}

//------------------------------------------------------------------------------
/**
 *  Non-periodic effects are handled differently from periodic ones:
 *
 *  N.P.: must be activated explicitly by a call to fire(). Is only
 *  deactivated here
 *
 *  P.: Is both activated and deactivated here.
 */
void ParticleEffectNode::setEnabled(bool enable)
{
    for (unsigned i=0; i<getNumEffects(); ++i)
    {
        ParticleEffectContainer ec = getEffect(i);

        if (enable)
        {
            s_particle_manager.activateSystem(ec.emitter_->getParticleSystem());
        }
        
        if (ec.emitter_->getLifeTime() != 0.0f)
        {
            if (!enable) ec.emitter_->setEnabled(false);
        } else if (ec.emitter_->isEnabled() != enable)
        {
            ec.emitter_->setEnabled(enable);
        }
    }
}

//------------------------------------------------------------------------------
unsigned ParticleEffectNode::getNumEffects() const
{
    assert(!(getNumChildren()&1));
    return getNumChildren()>>1;
}

//------------------------------------------------------------------------------
ParticleEffectContainer ParticleEffectNode::getEffect(unsigned n)
{    
    ParticleEffectContainer ret;

    assert(getNumChildren()/2 > n);
    if (getNumChildren()/2 <= n) return ret;
    
    ret.emitter_ = dynamic_cast<osgParticle::ModularEmitter*>(getChild(2*n  ));
    ret.program_ = dynamic_cast<osgParticle::ModularProgram*>(getChild(2*n+1));

    assert(ret.emitter_.get() && ret.program_.get());
    
    return ret;
}




//------------------------------------------------------------------------------
void ParticleEffectNode::cloneEmitters()
{
    // emitters are at even positions
    for (unsigned i=0; i<getNumChildren(); i+=2)
    {
        osgParticle::Emitter * em = dynamic_cast<osgParticle::Emitter*>(getChild(i));
        assert(em);
        osgParticle::Emitter * clone =
            dynamic_cast<osgParticle::Emitter*>(em->clone(osg::CopyOp(osg::CopyOp::DEEP_COPY_ALL)));

        clone->setParticleSystem((osgParticle::ParticleSystem*)em->getParticleSystem()->clone(osg::CopyOp::DEEP_COPY_ALL));
        
        setChild(i, clone);        
    }    
}





//------------------------------------------------------------------------------
EffectManager::EffectManager() : 
    ResourceManager<EffectGroup>("effect_loader"),
    suspend_creation_(false)
{
}

//------------------------------------------------------------------------------
/**
 *  Creates a particle effect offset by the specified position with an
 *  osg matrix transform.
 */
EffectManager::EffectPair EffectManager::createEffect(const std::string & name,
                                                      osg::Group * parent,
                                                      const Vector & offset)
{
    osg::Matrix mat;
    mat.makeTranslate(offset.x_, offset.y_, offset.z_);

    return setupEffect(name, mat, false, parent);
}


//------------------------------------------------------------------------------
EffectManager::EffectPair EffectManager::createEffect(const std::string & name,
                                                      const Vector & pos,
                                                      const Vector & dir,
                                                      bool prototype,
                                                      osg::Group * parent)
{
    /// rotate effect to point into direction of "dir" Vector, but
    /// retain Y as up vector for modular emitter
    osg::Matrix mat;
    mat.makeRotate(osg::Vec3(0,1,0),osg::Vec3(dir.x_,dir.y_,dir.z_));
    mat.setTrans(osg::Vec3(pos.x_,pos.y_,pos.z_));

    return setupEffect(name, mat, prototype, parent);
}

//------------------------------------------------------------------------------
EffectManager::EffectPair EffectManager::createEffectWithGroup(const std::string & effect_name,
                                                               const std::string & group_name,
                                                               osg::Group * parent)
{
    bool cur_suspend = suspend_creation_;
    suspend_creation_ = false;
    
    EffectPair effect = createEffect(effect_name, parent);
    effect.first ->setDelWhenDone(false);
    effect.second->setDelWhenDone(false);
    
    NodeUserData * ud = new NodeUserData;
    ud->groups_.push_back(std::vector<std::string>(1, group_name));
    ud->addLodsIfNeccessary();
    
    effect.first ->setUserData(ud);
    effect.second->setUserData(ud);

    suspend_creation_ = cur_suspend;

    return effect;
}


//------------------------------------------------------------------------------
void EffectManager::suspendEffectCreation(bool s)
{
    suspend_creation_ = s;
}




//------------------------------------------------------------------------------
/**
 *  \param activate_system Whether to update the particle
 *  system. False for prototypes.
 */
EffectManager::EffectPair EffectManager::setupEffect(const std::string & name,
                                                     const osg::Matrix & t,
                                                     bool prototype,
                                                     osg::Group * parent)
{
    ParticleEffectNode * pe_effect_node = new ParticleEffectNode();
    pe_effect_node->setName(name);
    pe_effect_node->setMatrix(t);

    SoundEffectNode * snd_effect_node = new SoundEffectNode();
    snd_effect_node->setName(name + "-snd");
    snd_effect_node->setMatrix(t);
    
    // Will get deleted on first traversal because it has no children.
    if (suspend_creation_ && !prototype)
    {
        return std::make_pair(pe_effect_node, snd_effect_node);
    }
    
    EffectGroup * effect_group = getResource(name);
    assert(effect_group);
    std::vector<ParticleEffectDesc> particle_effects = effect_group->getParticleEffects();

    // create all listed particle effects and add them to the container
    if (parent) parent->addChild(pe_effect_node);
    for(unsigned c=0; c < particle_effects.size(); c++)
    {
        ParticleEffectContainer new_effect = s_particle_manager.createEffect(particle_effects[c].name_,
                                                                             !prototype);
        // set the delay time for this effect
        new_effect.emitter_->setStartTime(particle_effects[c].delay_);

        pe_effect_node->addChild(new_effect.emitter_.get());
        pe_effect_node->addChild(new_effect.program_.get());        
    }




    
    // Create all sound effects for effect
    if (parent) parent->addChild(snd_effect_node);
    for (unsigned s=0; s<effect_group->getSoundEffects().size(); ++s)
    {
        SoundSource * source = new SoundSource(effect_group->getSoundEffects()[s].name_,
                                               effect_group->getSoundEffects()[s].loop_ ? SSCF_LOOP : 0);

        // must happen before play or pos for culling will be
        // incorrect.
        snd_effect_node->addChild(source);

        if (!prototype) source->play();
    }
    
    return std::make_pair(pe_effect_node, snd_effect_node);
}





//------------------------------------------------------------------------------
ToggleParticleEffectsVisitor::ToggleParticleEffectsVisitor(bool enable) :
    osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
    enable_(enable)
{
    setNodeMaskOverride(NODE_MASK_OVERRIDE);
}


//------------------------------------------------------------------------------
void ToggleParticleEffectsVisitor::apply(osg::MatrixTransform & node)
{
    traverse(node);
            
    ParticleEffectNode * eff = dynamic_cast<ParticleEffectNode*>(&node);
    if (!eff) return;

    NodeUserData * data = dynamic_cast<NodeUserData*>(node.getUserData());
    if (!data) return;

    bool groups_active = data->isActive();
            
    for (unsigned i=0; i<eff->getNumEffects(); ++i)
    {
        if (eff->getEffect(i).emitter_->isEnabled() == (enable_ && groups_active)) continue;
                
        eff->getEffect(i).emitter_->setEnabled(enable_ && groups_active);
    }
}
