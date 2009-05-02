

#include "WaterVisual.h"

#include "Paths.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "OsgNodeWrapper.h"

#include "UtilsOsg.h"

#include "RigidBody.h"

#include <osg/Geometry> ///used for textured quad

const unsigned CLIP_PLANE_NUM = 0;


/// Avoid rendering the water plane when drawing the reflections by
/// culling it via nodemasks.
const osg::Node::NodeMask REFLECTION_CAMERA_MASK    = 0x00000001;
const osg::Node::NodeMask WATER_NODE_MASK           = 0x00000010;

//------------------------------------------------------------------------------
WaterVisual::~WaterVisual()
{
    if (reflection_camera_.get())
    {
        DeleteNodeVisitor delete_camera(reflection_camera_.get());
        s_scene_manager.getRootNode()->accept(delete_camera);
    }    
}


//------------------------------------------------------------------------------
void WaterVisual::operator() (osg::Node *node, osg::NodeVisitor *nv)
{
    RigidBodyVisual::operator()(node, nv);

    // bail out if reflections are not enabled
    if(!reflections_enabled_) return;

    osg::Matrix view_matrix, projection_matrix;
    Camera & camera = s_scene_manager.getCamera();
    water_height_ = osg_wrapper_->getPosition().y_;

    /// XXX set projection matrix only once and when changed?
    projection_matrix.set(matGl2Osg(camera.getPerspectiveTransform())); 
    reflection_camera_->setProjectionMatrix(projection_matrix);

    // if camera is above water plane
    if(camera.getPos().y_ > water_height_)
    {
        osg::Vec3 eye_pos = osg::Vec3(  camera.getPos().x_,
                                        (-camera.getPos().y_)+2*water_height_,
                                        camera.getPos().z_);

        osg::Vec3 eye_dir = osg::Vec3(camera.getPos().x_-camera.getTransform().getZ().x_,
                                      (-camera.getPos().y_)+2*water_height_+camera.getTransform().getZ().y_,
                                      camera.getPos().z_-camera.getTransform().getZ().z_);

        osg::Vec3 eye_up = osg::Vec3(   camera.getTransform().getY().x_,
                                        -camera.getTransform().getY().y_,
                                        camera.getTransform().getY().z_);


        view_matrix.makeLookAt(eye_pos, eye_dir, eye_up);
        reflection_camera_->setViewMatrix(view_matrix);
    }
    else // if camera is below water plane
    {  
        view_matrix.set(matGl2Osg(camera.getTransform().getAffineInverse()));
        reflection_camera_->setViewMatrix(view_matrix);    
    }


    osg::Matrixf MVPT = osg_wrapper_->getOsgNode()->getMatrix() * view_matrix * projection_matrix *
                       osg::Matrix::translate(1.0,1.0,1.0) *
                       osg::Matrix::scale(0.5,0.5,0.5);

    projective_texture_mat_->set(MVPT);

    /// XXX if water height is set in onModelChanged, this is not needed anymore
    osg::Plane plane(0.0,1.0,0.0, -water_height_);
    clip_node_->getClipPlane(CLIP_PLANE_NUM)->setClipPlane(plane);

}




//------------------------------------------------------------------------------
WaterVisual::WaterVisual() :
    water_height_(0.0),
    reflections_enabled_(false)
{
#ifdef ENABLE_DEV_FEATURES    
    s_console.addFunction("toggleReflections",
                          Loki::Functor<void>  (this, &WaterVisual::toggleReflections),
                          &fp_group_);
#endif

    reflections_enabled_ = s_params.get<bool>("client.graphics.reflections");
}


//------------------------------------------------------------------------------
void WaterVisual::onModelChanged()
{
    RigidBodyVisual::onModelChanged();


    Texture * dudv_tex   = s_texturemanager.getResource(BASE_TEX_PATH + "water_dudv.dds");
    Texture * normal_tex = s_texturemanager.getResource(BASE_TEX_PATH + "water_normals.dds");


    // get geometry-node stateset of water visual, to replace original texture 
    // and set all other textures needed
    osg::Geode * geode = dynamic_cast<osg::Geode*>(osg_wrapper_->getOsgNode()->getChild(0));
    assert(geode);

    osg::Drawable * water_geometry = geode->getDrawable(0);
    assert(water_geometry);

    osg::StateSet * water_stateset = water_geometry->getOrCreateStateSet();

    water_stateset->setTextureAttribute(1, dudv_tex->getOsgTexture(), osg::StateAttribute::ON);
    water_stateset->setTextureAttribute(2, normal_tex->getOsgTexture(), osg::StateAttribute::ON);

    water_stateset->setMode(GL_BLEND, osg::StateAttribute::ON); // allow water transparency
    water_stateset->setRenderBinDetails(BN_WATER, "RenderBin");

    osg::Program * program;
    if(reflections_enabled_)
    {
        std::string defines = "#define REFLECTIONS\n";
        program = s_scene_manager.getCachedProgram("water", defines); 
    }
    else
    {
        program = s_scene_manager.getCachedProgram("water"); 
    }


    water_stateset->setAttribute(program, osg::StateAttribute::ON);

    water_stateset->addUniform(new osg::Uniform("reflection_texture", 0));
    water_stateset->addUniform(new osg::Uniform("dudv_texture", 1));
    water_stateset->addUniform(new osg::Uniform("normal_texture", 2));

    projective_texture_mat_ = new osg::Uniform(osg::Uniform::FLOAT_MAT4, "proj_tex_mat");
    water_stateset->addUniform(projective_texture_mat_.get());

    /// XXXX otherwise the traverse method is not called?!?!?
    osg_wrapper_->getOsgNode()->setUpdateCallback(this);


    //---------------------------------------------------------
    // bail out, if reflections are not enabled
    if(!reflections_enabled_) return;

    //---------------- Reflection texture ------------
    reflection_tex_ = new osg::Texture2D;
    reflection_tex_->setTextureSize(1024,1024);
    reflection_tex_->setInternalFormat(GL_RGBA);
    reflection_tex_->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);
    reflection_tex_->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);
    reflection_tex_->setWrap(osg::Texture2D::WRAP_S,osg::Texture2D::CLAMP_TO_BORDER);
    reflection_tex_->setWrap(osg::Texture2D::WRAP_T,osg::Texture2D::CLAMP_TO_BORDER);
    reflection_tex_->setBorderColor(osg::Vec4(1.0f,1.0f,1.0f,1.0f));


    //------------ Reflection Camera ----------------------
    reflection_camera_ = new osg::Camera;
    reflection_camera_->setName("WaterReflectionCamera");

    reflection_camera_->setClearColor(osg::Vec4(0.1f,0.1f,0.3f,1.0f));
    reflection_camera_->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    reflection_camera_->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
    reflection_camera_->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
    reflection_camera_->setViewport(0, 0,
                                     reflection_tex_->getTextureWidth(),
                                     reflection_tex_->getTextureHeight());

    reflection_camera_->setRenderOrder(osg::Camera::PRE_RENDER);
    reflection_camera_->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
    reflection_camera_->attach(osg::Camera::COLOR_BUFFER, reflection_tex_.get());

    reflection_camera_->setClearMask(GL_DEPTH_BUFFER_BIT);


    /// XXXX water height should be set here, but transform information is not 
    /// available yet, possible solutions?  is now set in node operator()
    //water_height_ = rb->getTarget()->getPosition().y_;

    /// the node mask is set, so that the reflection camera does not render the
    /// waterplane itself, which causes ugly artifacts in the refl. texture.
    int inheritance_mask = reflection_camera_->getInheritanceMask();
    inheritance_mask &= ~(osg::CullSettings::CULL_MASK);
    reflection_camera_->setInheritanceMask(inheritance_mask);
    reflection_camera_->setCullMask(REFLECTION_CAMERA_MASK);
    osg_wrapper_->getOsgNode()->setNodeMask(WATER_NODE_MASK);

    // add clip node with clip plane, to avoid rendering anything below water surface
    clip_node_ = new osg::ClipNode();
    clip_node_->setName("Water Clip Node XZ plane");
    clip_node_->addClipPlane(new osg::ClipPlane(CLIP_PLANE_NUM,0.0,1.0,0.0, -water_height_));

    // setup node hierarchy
    clip_node_->addChild(s_scene_manager.getShadowReceiverGroup());
    clip_node_->addChild(s_scene_manager.getDefaultGroup());
    reflection_camera_->addChild(clip_node_);
    s_scene_manager.getRootNode()->addChild(reflection_camera_.get());


    // replace static water texture with reflection texture
    water_stateset->setTextureAttribute(0, reflection_tex_.get(), osg::StateAttribute::ON);



    // ---------------- Debug cam for refl. texture -------------------------------------
    /*
    osg::ref_ptr<osg::Geometry> geom = osg::createTexturedQuadGeometry(osg::Vec3(0,0,0),
                                                                       osg::Vec3(100.0,0.0,0.0),
                                                                       osg::Vec3(0.0,100.0,0.0));
    geom->getOrCreateStateSet()->setTextureAttributeAndModes(0,reflection_tex_.get(), osg::StateAttribute::ON);
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING,osg::StateAttribute::OFF);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(geom.get());
    
    osg::Camera * debug_hud_camera_ = new osg::Camera;

    debug_hud_camera_->setProjectionMatrix(osg::Matrix::ortho2D(0,100,0,100));
    debug_hud_camera_->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    debug_hud_camera_->setViewMatrix(osg::Matrix::identity());
    debug_hud_camera_->setViewport(50,50,100,100);
    debug_hud_camera_->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // draw subgraph after main camera view.
    debug_hud_camera_->setRenderOrder(osg::Camera::POST_RENDER);
    debug_hud_camera_->addChild(geode.get());
    s_scene_manager.getRootNode()->addChild(debug_hud_camera_);
    */
   
}


//------------------------------------------------------------------------------
void WaterVisual::toggleReflections()
{
    if (!s_params.get<bool>("client.graphics.reflections")) return;

    reflections_enabled_ ^= 1;

    if (reflections_enabled_)
    {
        s_scene_manager.getRootNode()->addChild(reflection_camera_.get());        
    } else
    {
        DeleteNodeVisitor d(reflection_camera_.get());
        s_scene_manager.getRootNode()->accept(d);
    }
}
