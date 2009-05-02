

#ifndef TANK_PROJECTILE_VISUAL_INCLUDED
#define TANK_PROJECTILE_VISUAL_INCLUDED


#include "RigidBodyVisual.h"


//------------------------------------------------------------------------------
class ProjectileVisual : public RigidBodyVisual
{
 public:
    virtual ~ProjectileVisual();
    VISUAL_IMPL(ProjectileVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);


 protected:
    ProjectileVisual();
    
    virtual void onModelChanged();
};


//------------------------------------------------------------------------------
class BallisticProjectileVisual : public ProjectileVisual
{
 public:
    VISUAL_IMPL(BallisticProjectileVisual);
};

//------------------------------------------------------------------------------
class MissileVisual : public ProjectileVisual
{
 public:
    VISUAL_IMPL(MissileVisual);

 protected:
    
    virtual void onModelChanged();
};


#endif
