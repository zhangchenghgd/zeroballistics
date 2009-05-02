

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





#endif
