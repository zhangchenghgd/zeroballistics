
#ifndef TANK_BEACON_VISUAL_INCLUDED
#define TANK_BEACON_VISUAL_INCLUDED

#include "RigidBodyVisual.h"

#include "RegisteredFpGroup.h"



namespace osg
{
    class Geode;
}

class HudBar;

//------------------------------------------------------------------------------
class BeaconVisual : public RigidBodyVisual
{
 public:
    virtual ~BeaconVisual();
    VISUAL_IMPL(BeaconVisual);
    
    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);

    void destroy();
    
 protected:

    BeaconVisual();

    virtual void onModelChanged();

    void onBeaconStateChanged();
    
    osg::ref_ptr<osg::Geode> health_billboard_;
    osg::ref_ptr<HudBar>     health_bar_;

    osg::ref_ptr<osg::MatrixTransform> outer_node_;
    
    bool prev_healing_;
    
    RegisteredFpGroup fp_group_;
};





#endif
