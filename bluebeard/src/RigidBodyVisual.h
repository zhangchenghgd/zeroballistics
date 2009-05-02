

#ifndef TANK_RIGID_BODY_VISUAL_INCLUDED
#define TANK_RIGID_BODY_VISUAL_INCLUDED



#include "GameObjectVisual.h"
#include "Datatypes.h"


namespace terrain
{
    class TerrainDataClient;
}

namespace osg
{
    class MatrixTransform;
    class Material;
}

//------------------------------------------------------------------------------
class RigidBodyVisual : public GameObjectVisual
{
 public:
    virtual ~RigidBodyVisual();
    VISUAL_IMPL(RigidBodyVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);

    void updateTransform();
    
    static void setTerrainData(const terrain::TerrainDataClient * terrain_data_);
    static void toggleRenderTargets();
    
 protected:
    RigidBodyVisual();

    virtual void onModelChanged();
    virtual void onGameObjectSet();
    
    void onRigidBodyInit();
    void updateTargetVisual(void*);

    void setBrightnessFromTerrain();
    
    osg::ref_ptr<osg::Material> material_; ///< Used to set lm-based lighting
    
    osg::ref_ptr<osg::MatrixTransform> target_visual_; ///< Only for debug reasons, only if render_targets is on

    static const terrain::TerrainDataClient * terrain_data_;
    static bool render_targets_;
};





#endif
