
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



//------------------------------------------------------------------------------
/// simple struct describing a particle effect.
struct ParticleEffectDesc
{
    std::string name_;
    float delay_;
};


//------------------------------------------------------------------------------
/**
 * Class holding EffectGroup blueprints, used as prototype
 * for each generated effect
 *
 **/
class EffectGroup
{
public:
    EffectGroup(const std::string & filename);
    virtual ~EffectGroup();

    const std::vector<ParticleEffectDesc> & getParticleEffects() const;

private:
    void parseEffectGroup();

    std::string name_;
    std::vector<ParticleEffectDesc> particle_effects_;
};




//------------------------------------------------------------------------------
/**
 *  Holds a vector of particle effects and managers their deletion
 *  when they are finished.
 *
 *  Used to handle effect nodes differently when enabling / disabling
 *  through group mechanism. \see EnableGroupVisitor.
 */
class EffectNode : public osg::MatrixTransform
{
 public:
    META_Node(bluebeard, EffectNode);

    EffectNode() : del_when_done_(true) {}
    EffectNode(const EffectNode & n, const osg::CopyOp & op);

    virtual ~EffectNode();

    virtual void traverse(osg::NodeVisitor &nv);

    unsigned getNumEffects() const;
    ParticleEffectContainer getEffect(unsigned n);

    void fire();
    void setEnabled(bool e);

    void cloneEmitters();

    void setDelWhenDone(bool d);
    
 protected:
    
    bool del_when_done_;
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
            
        EffectNode * createEffect(const std::string & name,
                                  const Vector & offset = Vector(0.0f, 0.0f, 0.0f));

        EffectNode * createEffect(const std::string & name,
                                  const Vector & pos,
                                  const Vector & dir,
                                  bool prototype);

        EffectNode * createEffectWithGroup(const std::string & effect_name,
                                           const std::string & group_name,
                                           osg::Group * parent);

        void suspendEffectCreation(bool s);
        
    protected:
        virtual ~EffectManager() {}

        EffectNode * setupEffect(const std::string & name,
                                 const osg::Matrix & transform,
                                 bool prototype);

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
class ToggleEffectsVisitor : public osg::NodeVisitor
{
public:

    ToggleEffectsVisitor(bool enable);
    virtual void apply(osg::MatrixTransform & node);
    
protected:
    bool enable_;
};

#endif

