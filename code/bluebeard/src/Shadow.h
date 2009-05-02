
#ifndef TANK_SHADOW_INCLUDED
#define TANK_SHADOW_INCLUDED


#include <vector>

#include <osg/Node>
#include <osg/NodeCallback>
#include <osg/PolygonOffset>

#include "Plane.h"
#include "Camera.h"
#include "UtilsOsg.h"
#include "RegisteredFpGroup.h"



// ATI changed its FBO orientation in latest drivers (y-flip). Draw a
// quad across half the FBO and see where the pixels land up, flip
// FBO if neccessary.
#define ATI_FBO_FLIP_WORKAROUND


class Frustum;
class Vector;
class Camera;



namespace osg
{
    class MatrixTransform;
    class Camera;
    class Uniform;
    class Texture2D;
}


#ifdef ATI_FBO_FLIP_WORKAROUND

//------------------------------------------------------------------------------
enum FLIP_FBO_STATE
{
    FFS_FALSE,
    FFS_TRUE,
    FFS_UNINITIALIZED,
    FFS_RENDERED
};

#endif

//------------------------------------------------------------------------------
class Shadow : public osg::NodeCallback
{
public:
    Shadow(osg::Group * receiver);
    
    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv);
    
    void setLightDir(const Vector & dir);

    void addReceiver(osg::Node * node);
    void addBlocker (osg::Node * node);
    void removeReceiver(osg::Node * node);
    void removeBlocker (osg::Node * node);

    void toggleShadowDebug();
    
protected:
    virtual ~Shadow();

    void initDepthTex(bool depth_comparison);
    void initShadowCamera();

    void enableDebug();
    void disableDebug();
    
    void findRelevantBlockers();
    void calcLSViewAndProjectionMatrix(osg::Matrix & p_view, osg::Matrix & p_proj);
    
    osg::Vec3 light_dir_;
    
    osg::ref_ptr<osg::Group> blocker_;
    osg::Group * receiver_;  ///< avoid cyclic ref_ptr dependency with scene_manager

    osg::ref_ptr<osg::PolygonOffset> polygon_offset_;
    osg::ref_ptr<osg::Uniform>       projective_texture_mat_;

    osg::ref_ptr<osg::Camera> shadow_camera_node_;
    osg::ref_ptr<osg::Texture2D>  depth_tex_;
    std::vector<osg::Node*>       relevant_blocker_;
    
    bool                          shadow_debug_info_;
    osg::ref_ptr<osg::Camera> debug_hud_camera_;
    osg::ref_ptr<CameraGeode>     debug_camera_frustum_render_;

    bool update_shadow_;

#ifdef ATI_FBO_FLIP_WORKAROUND    
    FLIP_FBO_STATE flip_fbo_;
#endif
    
    RegisteredFpGroup fp_group_;
};

std::vector<Plane> extrudeFrustum(const Frustum & frustum, const Vector & light_direction);

#endif
