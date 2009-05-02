

#ifndef TANK_RIGID_BODY_VISUAL_INCLUDED
#define TANK_RIGID_BODY_VISUAL_INCLUDED



#include "GameObjectVisual.h"
#include "Datatypes.h"
#include "Vector.h"

namespace terrain
{
    class TerrainDataClient;
}

namespace osg
{
    class MatrixTransform;
    class Material;
    class BlendColor;
}



//------------------------------------------------------------------------------
/**
 *  Stores the velocity of the underlying rigid body for everything
 *  below a RigidBodyVisual osg node.
 */
class UpdateVisitorUserData : public osg::Referenced
{
 public:
    UpdateVisitorUserData(const Vector & velocity) : velocity_(velocity) {}

    const Vector & getVelocity() const { return velocity_; }
    
 protected:
    Vector velocity_;
};


//------------------------------------------------------------------------------
class RigidBodyVisual : public GameObjectVisual
{
 public:
    virtual ~RigidBodyVisual();
    VISUAL_IMPL(RigidBodyVisual);

    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv);

    virtual Matrix getTrackingPos(const Vector & offset);    
    
    void updateTransform();
    
    static void setTerrainData(const terrain::TerrainDataClient * terrain_data_);
    static void toggleRenderTargets();
    
 protected:
    RigidBodyVisual();

    virtual void onModelChanged();
    virtual void onGameObjectSet();

    
    void onRigidBodyInit();
    void updateTargetVisual(void*);

    void onInitialPositionSet();
    
    void setBrightnessFromTerrain();

    void updateAlphaFadeout();
    
    osg::ref_ptr<osg::Material> material_; ///< Used to set lm-based lighting
    osg::ref_ptr<osg::BlendColor> blend_color_;
    
    osg::ref_ptr<osg::MatrixTransform> target_visual_; ///< Only for debug reasons, only if render_targets is on

    static const terrain::TerrainDataClient * terrain_data_;
    static bool render_targets_;
};





#endif
