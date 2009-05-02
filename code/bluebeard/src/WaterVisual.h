

#ifndef TANK_WATER_VISUAL_INCLUDED
#define TANK_WATER_VISUAL_INCLUDED



#include "RigidBodyVisual.h"


#include "RegisteredFpGroup.h"


namespace osg
{
    class Camera;
    class Uniform;
    class Texture2D;
    class ClipNode;
}

//------------------------------------------------------------------------------
class WaterVisual : public RigidBodyVisual
{
 public:
    virtual ~WaterVisual();
    VISUAL_IMPL(WaterVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);


    
 protected:
    WaterVisual();

    virtual void onModelChanged();

    void toggleReflections();

    osg::ref_ptr<osg::Camera>       reflection_camera_;
    osg::ref_ptr<osg::Texture2D>    reflection_tex_;

    osg::ref_ptr<osg::Uniform>      projective_texture_mat_;
    
    float water_height_;
    bool reflections_enabled_;

    RegisteredFpGroup fp_group_;
};





#endif
