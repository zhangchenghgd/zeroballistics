
#ifndef TANK_PARTICLE_CUSTOM_OPERATOR_INCLUDED
#define TANK_PARTICLE_CUSTOM_OPERATOR_INCLUDED


#include <osgParticle/Operator>

#include "Camera.h"
#include "SceneManager.h"

#include "RigidBody.h"

using namespace osgParticle;

//------------------------------------------------------------------------------
class AlignWithVelocityVectorOperator : public Operator 
{
    public:
        AlignWithVelocityVectorOperator() {}
        AlignWithVelocityVectorOperator( const AlignWithVelocityVectorOperator& copy,
                                         const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY) { assert(false); };

        META_Object(bluebeard, AlignWithVelocityVectorOperator);

        virtual void operate(Particle* P, double dt);
        
    protected:
        virtual ~AlignWithVelocityVectorOperator() {}
};


//------------------------------------------------------------------------------
class LocalEffectOperator : public Operator
{
 public:
    LocalEffectOperator() {}

    LocalEffectOperator( const LocalEffectOperator& copy,
                         const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY) { assert (false); };
        
    META_Object(bluebeard, LocalEffectOperator);

    virtual void operate     (Particle* p, double dt);
    virtual void beginOperate(Program * program);
    
 protected:
    osg::Matrix transform_;
};


#endif
