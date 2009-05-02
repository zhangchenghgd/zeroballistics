
#ifndef TANK_EFFECT_MANAGER_INCLUDED
#define TANK_EFFECT_MANAGER_INCLUDED

#include <osg/ref_ptr>
#include <osg/Matrix>
#include <osg/MatrixTransform>

#include "ResourceManager.h"
#include "ParticleEffect.h"



#include "BbmOsgConverter.h"
#include "SceneManager.h"

namespace osg
{
    class Group;
}

class SoundSource;


//------------------------------------------------------------------------------
/// simple struct describing a particle effect.
struct ParticleEffectDesc
{
    std::string name_;
    float delay_;
};


//------------------------------------------------------------------------------
struct SoundEffectDesc
{
    std::string name_;
    bool loop_;
};

//------------------------------------------------------------------------------
/**
 * Class holding EffectGroup blueprints, used as prototype for each
 * generated effect. EffectManager is ResourceManager for
 * EffectGroups.
 *
 **/
class EffectGroup
{
public:
    EffectGroup(const std::string & filename);
    virtual ~EffectGroup();

    const std::vector<ParticleEffectDesc> & getParticleEffects() const;
    const std::vector<SoundEffectDesc>    & getSoundEffects   () const;

private:
    void parseEffectGroup();

    std::string name_;
    std::vector<ParticleEffectDesc> particle_effects_;
    std::vector<SoundEffectDesc>    sound_effects_;
};



//------------------------------------------------------------------------------
class EffectNode : public osg::MatrixTransform
{
 public:
    META_Node(bluebeard, EffectNode);
    
    EffectNode();
    EffectNode(const EffectNode & n, const osg::CopyOp & op);
        
    virtual void fire() {}
    virtual void setEnabled(bool e) {}

    void setDelWhenDone(bool d);
    
 protected:
    bool del_when_done_;
};


//------------------------------------------------------------------------------
class SoundEffectNode : public EffectNode
{
 public:
    META_Node(bluebeard, SoundEffectNode);

    SoundEffectNode();
    SoundEffectNode(const SoundEffectNode & n, const osg::CopyOp & op);

    virtual void traverse(osg::NodeVisitor &nv);
    
    virtual void fire();
    virtual void setEnabled(bool e);

    unsigned getNumEffects() const;
    SoundSource * getEffect(unsigned n);
    
 protected:
    bool prev_enabled_;
};

//------------------------------------------------------------------------------
/**
 *  Holds a vector of particle effects and managers their deletion
 *  when they are finished.
 *
 *  Used to handle effect nodes differently when enabling / disabling
 *  through group mechanism. \see EnableGroupVisitor.
 */
class ParticleEffectNode : public EffectNode
{
 public:
    META_Node(bluebeard, ParticleEffectNode);

    ParticleEffectNode();
    ParticleEffectNode(const ParticleEffectNode & n, const osg::CopyOp & op);

    virtual ~ParticleEffectNode();

    virtual void traverse(osg::NodeVisitor &nv);

    virtual void fire();
    virtual void setEnabled(bool e);

    
    unsigned getNumEffects() const;
    ParticleEffectContainer getEffect(unsigned n);


    void cloneEmitters();
};



//------------------------------------------------------------------------------
/**
 * Singleton that loads effect group files, is ResourceManager to avoid 
 * XML parsing of the same effect over and over again
 **/
#define s_effect_manager Loki::SingletonHolder<EffectManager, Loki::CreateUsingNew, SingletonEffectManagerLifetime >::Instance()
//------------------------------------------------------------------------------
class EffectManager: public ResourceManager<EffectGroup>
{
        DECLARE_SINGLETON(EffectManager);
    public:

        typedef std::pair<osg::ref_ptr<ParticleEffectNode>,osg::ref_ptr<SoundEffectNode> > EffectPair;
            
        EffectPair createEffect(const std::string & name,
                                osg::Group * parent,
                                const Vector & offset = Vector(0.0f, 0.0f, 0.0f));

        EffectPair createEffect(const std::string & name,
                                const Vector & pos,
                                const Vector & dir,
                                bool prototype,
                                osg::Group * parent);

        EffectPair createEffectWithGroup(const std::string & effect_name,
                                         const std::string & group_name,
                                         osg::Group * parent);

        void suspendEffectCreation(bool s);
        
 protected:
        virtual ~EffectManager() {}

        EffectPair setupEffect(const std::string & name,
                               const osg::Matrix & transform,
                               bool prototype,
                               osg::Group * parent);

        bool suspend_creation_; ///< During periods when scene graph
                                ///is not updated (e.g. user switches
                                ///to main menu), do not create
                                ///particle effects, or they
                                ///simulation will get stuck when they
                                ///are all played with the first
                                ///updates.
};



//------------------------------------------------------------------------------
/**
 *  For debugging purposes only...
 */
class ToggleParticleEffectsVisitor : public osg::NodeVisitor
{
public:

    ToggleParticleEffectsVisitor(bool enable);
    virtual void apply(osg::MatrixTransform & node);
    
protected:
    bool enable_;
};

#endif

