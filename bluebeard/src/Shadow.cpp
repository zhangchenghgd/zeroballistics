
#include "Shadow.h"

#include <limits>


#include <osg/Vec3>
#include <osg/Node>
#include <osg/ClipNode>
#include <osg/MatrixTransform>
#include <osg/NodeVisitor>
#include <osg/Matrix>
#include <osg/Geometry> ///used for textured quad
#include <osg/CullFace>
#include <osg/Geode>
#include <osg/Polytope>

#include "SceneManager.h"
#include "Frustum.h"
#include "Vector.h"
#include "ParameterManager.h"



const float MAX_SHADOW_BLOCKER_DISTANCE = 15.0f;
    
#undef min
#undef max


//------------------------------------------------------------------------------
/**
 *  This non traversing update callback is set on the blocker group, so that
 *  nodes below the blocker are not traversed twice a frame, because they are 
 *  already traversed during the update of the default or recevier group.
 **/
class NonTraversingBlocker : public osg::NodeCallback
{
public:
    NonTraversingBlocker() {}
    
    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {}
};



//------------------------------------------------------------------------------
Shadow::Shadow(osg::Group * receiver) :
    light_dir_(0,-1,0),
    receiver_(receiver),
    shadow_debug_info_(false),
    update_shadow_(true)
#ifdef ATI_FBO_FLIP_WORKAROUND
    ,flip_fbo_(FFS_UNINITIALIZED)
#endif
{
    s_log << Log::debug('i')
          << "Shadow constructor\n";
    

#ifdef ENABLE_DEV_FEATURES
    s_console.addFunction("toggleShadowDebug",
                          Loki::Functor<void>(this, &Shadow::toggleShadowDebug),
                          &fp_group_);
#endif

    s_console.addVariable("update_shadow", &update_shadow_, &fp_group_);
    

    blocker_ = new osg::Group();    
    blocker_ ->setName("ShadowBlockerGroup");
    blocker_->setUpdateCallback(new NonTraversingBlocker());

    projective_texture_mat_ = new osg::Uniform("proj_texture_mat",osg::Matrix());
    receiver_->getOrCreateStateSet()->addUniform(projective_texture_mat_.get());
    receiver_->setUpdateCallback(this);
    
    
    initDepthTex(true);
    initShadowCamera();
}

//------------------------------------------------------------------------------
/**
 *  Calculates the new shadow projection matrix and the shadow
 *  camera's view and projection matrix.
 */
void Shadow::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!update_shadow_) return;
    
    // first update subgraph to make sure objects are all moved into
    // postion
    traverse(node,nv);
    
#ifdef ATI_FBO_FLIP_WORKAROUND
    if (flip_fbo_ == FFS_UNINITIALIZED)
    {
        if (depth_tex_->getTextureObject(s_scene_manager.getOsgState().getContextID()))
        {
            flip_fbo_ = FFS_RENDERED;
        }
        return;
        
    } else if (flip_fbo_ == FFS_RENDERED)
    {
        osg::Texture::TextureObject * obj = depth_tex_->getTextureObject(s_scene_manager.getOsgState().getContextID());

        std::vector<float> tex_data(obj->_width * obj->_height, 0.0f);
        
        obj->bind();
        glGetTexImage(GL_TEXTURE_2D,
                      0,
                      GL_DEPTH_COMPONENT,
                      GL_FLOAT,
                      &tex_data[0]);

        
        flip_fbo_ = tex_data[0] > 0.5f ? FFS_TRUE : FFS_FALSE;

        // remove test quad, re-enable blocker geometry
        assert(shadow_camera_node_->getNumChildren() == 2);
        shadow_camera_node_->removeChild(1,1);
        assert(shadow_camera_node_->getChild(0) == blocker_.get());
        blocker_->setNodeMask(NODE_MASK_VISIBLE);
        
        return;
    }
#endif

    polygon_offset_->setFactor(s_params.get<float>("client.shadows.polygon_offset_factor"));
    polygon_offset_->setUnits(s_params.get<float>("client.shadows.polygon_offset_units"));
    

    findRelevantBlockers();
    
    if (relevant_blocker_.empty())
    {
        // Don't spend time rendering no objects
        // hide blocker objects so that shadow cam renders nothing
        blocker_->setNodeMask(NODE_MASK_INVISIBLE); 
        return;
    } else
    {
        blocker_->setNodeMask(NODE_MASK_VISIBLE);
    }


    
    // do lispsm calculations
    osg::Matrix p_view, p_proj;
    calcLSViewAndProjectionMatrix(p_view, p_proj);

#ifdef ATI_FBO_FLIP_WORKAROUND
    if (flip_fbo_ == FFS_TRUE)
    {
        osg::Matrix flip;
        flip(1,1) *= -flip(1,1);
        shadow_camera_node_->setProjectionMatrix(p_proj*flip);
    } else
#endif
    {
        shadow_camera_node_->setProjectionMatrix(p_proj);
    }    
    shadow_camera_node_->setViewMatrix(p_view);

    
    // get inverse MV matrix for the texcoord generation in the vertex shader
    Matrix temp_mv;
    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_MODELVIEW);
    glGetFloatv(GL_MODELVIEW_MATRIX, temp_mv );
    glPopAttrib();
    temp_mv.invertAffine();
    
    
    // set texture projection matrix, includes inverse View Matrix of
    // current camera to get the world coordinates in the vertex
    // shader for the eye linear texture projection
    //
    // inverse View * ModelView * Projection * TextureScale
    osg::Matrix iVMVPT = (matGl2Osg(temp_mv) *
                          p_view *
                          p_proj *
                          osg::Matrix::translate(1.0f, 1.0f, 1.0f) *
                          osg::Matrix::scale    (0.5f, 0.5f, 0.5f));
    projective_texture_mat_->set(iVMVPT);
}

//------------------------------------------------------------------------------
void Shadow::setLightDir(const Vector & dir)
{
    light_dir_.set(dir.x_, dir.y_, dir.z_);
    light_dir_.normalize();
}

//------------------------------------------------------------------------------
void Shadow::addReceiver(osg::Node * node)
{
    receiver_->addChild(node);
}


//------------------------------------------------------------------------------
void Shadow::addBlocker (osg::Node * node)
{
    blocker_->addChild(node);
}

//------------------------------------------------------------------------------
void Shadow::removeReceiver(osg::Node * node)
{
    DeleteNodeVisitor nv(node);
    receiver_->accept(nv);
}


//------------------------------------------------------------------------------
void Shadow::removeBlocker (osg::Node * node)
{
    DeleteNodeVisitor nv(node);
    blocker_->accept(nv);
}



//------------------------------------------------------------------------------
void Shadow::toggleShadowDebug()
{
    shadow_debug_info_ ^= 1;
    
    if(shadow_debug_info_)
    {
        initDepthTex(false);
        initShadowCamera();
        
        enableDebug();        
    } else
    {
        disableDebug();

        initDepthTex(true);
        initShadowCamera();
    }
}


//------------------------------------------------------------------------------
Shadow::~Shadow()
{
    s_log << Log::debug('d')
          << "Shadow destructor\n";
}


//------------------------------------------------------------------------------
void Shadow::initDepthTex(bool depth_comparison)
{
    depth_tex_ = new osg::Texture2D;
    depth_tex_->setTextureSize(s_params.get<unsigned>("client.shadows.map_size"),
                               s_params.get<unsigned>("client.shadows.map_size"));

    depth_tex_->setInternalFormat(GL_DEPTH_COMPONENT);
    depth_tex_->setShadowComparison(depth_comparison);
    depth_tex_->setShadowTextureMode(osg::Texture::LUMINANCE);
    depth_tex_->setFilter(osg::Texture2D::MIN_FILTER,osg::Texture2D::LINEAR);
    depth_tex_->setFilter(osg::Texture2D::MAG_FILTER,osg::Texture2D::LINEAR);
    depth_tex_->setWrap(osg::Texture2D::WRAP_S,osg::Texture2D::CLAMP_TO_BORDER);
    depth_tex_->setWrap(osg::Texture2D::WRAP_T,osg::Texture2D::CLAMP_TO_BORDER);
    depth_tex_->setBorderColor(osg::Vec4(1.0f,1.0f,1.0f,1.0f));


    receiver_->getOrCreateStateSet()->setTextureAttribute(SHADOW_TEX_UNIT,
                                                          depth_tex_.get());
}


//------------------------------------------------------------------------------
/**
 *  Sets up the camera that renders to the depth texture. The depth
 *  texture must already have been initialized.
 *
 *  \param group The osg group which contains the shadow caster
 *  objects to be rendered by the camera.
 */
void Shadow::initShadowCamera()
{
    if (shadow_camera_node_.get())
    {
        DeleteNodeVisitor nv(shadow_camera_node_.get());
        s_scene_manager.getRootNode()->accept(nv);
    }

    
    assert(blocker_.get());
    assert(depth_tex_.get());
    
    shadow_camera_node_ = new osg::Camera;
    shadow_camera_node_->setName("ShadowCamera");

    shadow_camera_node_->setCullMask(CAMERA_MASK_SEE_VISIBLE);
    shadow_camera_node_->setClearMask(GL_DEPTH_BUFFER_BIT);
    shadow_camera_node_->setComputeNearFarMode(osg::Camera::DO_NOT_COMPUTE_NEAR_FAR);
    shadow_camera_node_->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
    shadow_camera_node_->setViewport(0, 0,
                                     depth_tex_->getTextureWidth(),
                                     depth_tex_->getTextureHeight());



    osg::StateSet * camera_stateset = shadow_camera_node_->getOrCreateStateSet();

    // Don't waste time applying shaders
    camera_stateset->setAttribute(new osg::Program(), osg::StateAttribute::OVERRIDE);


    /// polygon offset as bias method
    /// bias = factor * dz + r * units
    /// factor sets the influence of the depth slope and units sets the minimal bias value
    polygon_offset_ = new osg::PolygonOffset;
    polygon_offset_->setFactor(s_params.get<float>("client.shadows.polygon_offset_factor")); 
    polygon_offset_->setUnits (s_params.get<float>("client.shadows.polygon_offset_units")); 
    
    camera_stateset->setAttribute(polygon_offset_.get(),  osg::StateAttribute::OVERRIDE);
    camera_stateset->setMode     (GL_POLYGON_OFFSET_FILL, osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);

    camera_stateset->setAttribute(new osg::CullFace(osg::CullFace::FRONT), osg::StateAttribute::OVERRIDE);
    camera_stateset->setMode     (GL_CULL_FACE, osg::StateAttribute::OVERRIDE | osg::StateAttribute::ON);


    // set the camera to render before the main camera.
    shadow_camera_node_->setRenderOrder(osg::Camera::PRE_RENDER);

    // tell the camera to use OpenGL frame buffer object where supported.
    shadow_camera_node_->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

    // attach the texture and use it as the depth buffer.
    shadow_camera_node_->attach(osg::Camera::DEPTH_BUFFER, depth_tex_.get());

    s_scene_manager.getRootNode()->addChild(shadow_camera_node_.get());

    shadow_camera_node_->addChild(blocker_.get());


#ifdef ATI_FBO_FLIP_WORKAROUND
    float s = (float)s_params.get<unsigned>("client.shadows.map_size")*0.5f;
    
    osg::Geometry * quad = new osg::Geometry();

    osg::Vec3Array & vb = *(new osg::Vec3Array(4));
    
    vb[0] = osg::Vec3( s,  0, -0.5);
    vb[1] = osg::Vec3( s, -s, -0.5);
    vb[2] = osg::Vec3(-s, -s, -0.5);
    vb[3] = osg::Vec3(-s,  0, -0.5);

    quad->setVertexArray(&vb);
    quad->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));

    osg::Geode * g = new osg::Geode;
    g->addDrawable(quad);

    // order is important here, do this after blocker geometry
    // addition
    shadow_camera_node_->addChild(g);

    // don't render ordinary blocker geometry until test quad was
    // evaluated
    blocker_->setNodeMask(0);
#endif
}


//------------------------------------------------------------------------------
void Shadow::enableDebug()
{
    assert(!debug_camera_frustum_render_.get());
    
    //add camera frustum renderer
    debug_camera_frustum_render_ = new CameraGeode(shadow_camera_node_.get());
    debug_camera_frustum_render_->setName("Depth tex debug camera");
    debug_camera_frustum_render_->getOrCreateStateSet()->setRenderBinDetails(BN_DEFAULT, "RenderBin");
    
    osg::ref_ptr<osg::Geometry> geom = osg::createTexturedQuadGeometry(osg::Vec3(0,0,0),
                                                                       osg::Vec3(100.0,0.0,0.0),
                                                                       osg::Vec3(0.0,100.0,0.0));
    geom->getOrCreateStateSet()->setTextureAttributeAndModes(0, depth_tex_.get(), osg::StateAttribute::ON);    
    geom->getOrCreateStateSet()->setMode(GL_LIGHTING,osg::StateAttribute::OFF);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(geom.get());
    
    debug_hud_camera_ = new osg::Camera;

    debug_hud_camera_->setProjectionMatrix(osg::Matrix::ortho2D(0,100,0,100));
    debug_hud_camera_->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    debug_hud_camera_->setViewMatrix(osg::Matrix::identity());
    debug_hud_camera_->setViewport(50,50,100,100);
    debug_hud_camera_->setClearMask(GL_DEPTH_BUFFER_BIT);
    // draw subgraph after main camera view.
    debug_hud_camera_->setRenderOrder(osg::Camera::POST_RENDER);
    debug_hud_camera_->addChild(geode.get());


    s_scene_manager.getRootNode()->addChild(debug_camera_frustum_render_.get());
    s_scene_manager.getRootNode()->addChild(debug_hud_camera_.get());
}


//------------------------------------------------------------------------------
void Shadow::disableDebug()
{
    assert(debug_camera_frustum_render_.get());
    
    DeleteNodeVisitor nv1(debug_camera_frustum_render_.get());
    DeleteNodeVisitor nv2(debug_hud_camera_.get());

    s_scene_manager.getRootNode()->accept(nv1);
    s_scene_manager.getRootNode()->accept(nv2);

    assert(debug_hud_camera_           ->referenceCount() == 1);
    assert(debug_camera_frustum_render_->referenceCount() == 1);

    debug_hud_camera_            = NULL;
    debug_camera_frustum_render_ = NULL;
}


//------------------------------------------------------------------------------
/**
 *  Finds all blockers which are contained in the view volume extruded
 *  towards the light source.
 */
void Shadow::findRelevantBlockers()
{
    // add frustum extruded planes to polytope
    std::vector<Plane> frustum_planes = extrudeFrustum(s_scene_manager.getCamera().getFrustum(),
                                                       Vector(light_dir_.x(),
                                                              light_dir_.y(),
                                                              light_dir_.z()));
    osg::Polytope polytope;
    for(unsigned int p=0;p< frustum_planes.size(); ++p)
    {
        polytope.add(osg::Plane(osg::Vec3(  frustum_planes[p].normal_.x_,
                                            frustum_planes[p].normal_.y_,
                                            frustum_planes[p].normal_.z_),
                                            frustum_planes[p].d_));
    }    
    polytope.flip();


    osg::Vec3 camera_pos = vecGl2Osg(s_scene_manager.getCamera().getPos());
    relevant_blocker_.clear();
    for(unsigned int i=0; i < blocker_->getNumChildren(); ++i)
    {         
        osg::MatrixTransform * blocker_node = blocker_->getChild(i)->asTransform()->asMatrixTransform();
        assert(blocker_node);

        //check if blocker is inside frustum
        if(polytope.contains(blocker_node->getBound()))
        {   
            // calculate distance from blocker to camera and add only blocker inside
            // max shadow blocker distance
            osg::Vec3 vp = blocker_node->getMatrix().getTrans() - camera_pos;
            if (vp.length() < MAX_SHADOW_BLOCKER_DISTANCE)
            {
                relevant_blocker_.push_back(blocker_node);
            }
        }
    }
}


//------------------------------------------------------------------------------
/**
 *
 */
void Shadow::calcLSViewAndProjectionMatrix(osg::Matrix & p_view, osg::Matrix & p_proj)
{
    const Camera camera   = s_scene_manager.getCamera();
    const Frustum frustum = camera.getFrustum();
    
    // First, retrieve the camera's position and view direction
    osg::Vec3 view_dir = vecGl2Osg( -camera.getTransform().getZ());
    osg::Vec3 eye_pos  = vecGl2Osg(camera.getPos());


    // gamma is the angle between light dir and camera dir.
    float cos_gamma = view_dir*light_dir_;
    float sin_gamma = sqrtf(abs(1.0-sqr(cos_gamma)));
        
    // Construct preliminary light space coordinate system:
    // - y points toward light,
    // - z is in the plane spanned by light direction and view vector
    // - x is perpendicular to those two.
    //  Origin is still at world center.
    osg::Vec3 light_view_y = -light_dir_;
    osg::Vec3 light_view_x, light_view_z;
    if (equalsZero(sin_gamma))
    {
        // light_dir_ and view_dir don't span a plane, construct
        // light space by using camera up vector
        light_view_x = light_dir_ ^ vecGl2Osg(camera.getTransform().getY());
    } else
    {
        light_view_x = light_dir_ ^ view_dir;
    }
    light_view_x.normalize();
    light_view_z = light_view_x ^ -light_dir_;
    assert(view_dir * light_view_z < EPSILON);

    

    // As a next step, calculate the position of the projection
    // center in lightspace.
    //
    // Its x-coordinate is determined by transforming the eye position
    // to lightspace.
    //
    // The y-coordinate is the center of the y-extents of the volume B
    // which encompasses the view frustum and all potential blockers.
    //
    // The z-cordinate is a distance of n_opt from the point of B
    // furthest along the positive z-axis.
    

    // Determine the extents along the y-axis of the frustum in light
    // space, determine maximum z value of near points of the view
    // frustum and minimal z value of far points of the view frustum
    float y_max  = -std::numeric_limits<float>::max();
    float y_min  =  std::numeric_limits<float>::max();
    float z_max  = -std::numeric_limits<float>::max();
    float z_min  =  std::numeric_limits<float>::max();

    for (unsigned b=0; b<relevant_blocker_.size(); ++b)
    {
        float r = relevant_blocker_[b]->getBound().radius() * sqrtf(3.0f);
        float y = relevant_blocker_[b]->getBound().center() * light_view_y;
        float z = relevant_blocker_[b]->getBound().center() * light_view_z;
        z_min = std::min(z_min, z-r);
        z_max = std::max(z_max, z+r);
        y_min = std::min(y_min, y-r);
        y_max = std::max(y_max, y+r);
    }

    // Now find the near plane distance for the light space
    // projection.
    //
    // For this we first compute sine of the angle between camera
    // view direction and light direction. The closer this angle
    // is to zero, the more the projection should behave like
    // uniform shadow mapping -> the near distance should go to
    // infinity.
    float n_opt;
    float d = z_max - z_min;
    if (equalsZero(sin_gamma))
    {
        n_opt = frustum.getYon();
    } else
    {
        n_opt = (frustum.getHither() + sqrtf(frustum.getHither()*MAX_SHADOW_BLOCKER_DISTANCE))/sin_gamma;
    }

    // We know the far plane distance of the light space projection
    // now, too
    float f = n_opt + d;

    
    osg::Vec3 p_center(light_view_x * eye_pos,
                       0.5f * (y_max + y_min),
                       z_max + n_opt);



    // Now transform p_center back into world space and construct the
    // view matrix
    osg::Matrix ls_to_world(light_view_x[0], light_view_x[1], light_view_x[2], 0,
                            light_view_y[0], light_view_y[1], light_view_y[2], 0,
                            light_view_z[0], light_view_z[1], light_view_z[2], 0,
                            0, 0, 0, 1);
    osg::Vec3 p_center_world = p_center * ls_to_world;
    osg::Vec3 target = p_center_world-light_view_z;

    p_view.makeLookAt(p_center_world, target, light_view_y);


    // We still have to find the parameters for the frustum planes. To
    // this end, project all bounding volumes and find their extents.
    osg::Matrix temp_p_proj;
    temp_p_proj.makeFrustum(-1,1,-1,1,n_opt,f);
    
    //transform bounding sphere of each blocker to light space and create aabb from it
    osg::BoundingBox p_aabb;
    
    osg::Matrix p_tmp_total = p_view * temp_p_proj;
    osg::BoundingBox object_aabb;
    for(unsigned i=0; i<relevant_blocker_.size(); i++)
    {
        object_aabb.init();
        object_aabb.expandBy(relevant_blocker_[i]->getBound());
        for(unsigned c=0; c<8; c++)
        {
            p_aabb.expandBy(object_aabb.corner(c) * p_tmp_total);
        }
    }

    assert(p_aabb.zMin() >= -1.0f);
    

    p_proj.makeFrustum( p_aabb.xMin(),
                        p_aabb.xMax(),
                        p_aabb.yMin(),
                        p_aabb.yMax(),
                        n_opt, f);
        
        
    // rotate the light space projection around x axis to make the 
    // projection plane parallel to x/z (as described in paper p.73)
    osg::Matrix rot_x;
    rot_x.makeRotate(osg::DegreesToRadians(-90.0),osg::Vec3(1.0,0.0,0.0));

    p_proj = p_proj * rot_x;
}




//------------------------------------------------------------------------------
/**
 *  Calculates the convex polytope that is formed by extruding the
 *  specified frustum towards a point(light) at infinity.
 *
 *  First determine all frustum faces that are facing away from the
 *  light; they must be in the result. Then, based on the result,
 *  determine all silhouette edges of the frustum and build planes
 *  collinear with those edges and the light direction.
 * 
 *  \param frustum The frustum to extrude.
 *  
 *  \param light_direction The light shines into this direction. The
 *  frustum is extruded to (-light_direction).
 *
 *  \return A vector of planes. Their normals point out of the
 *  extruded volume. Planes are either collinear with the light
 *  direction or are frustum planes.
 */
std::vector<Plane> extrudeFrustum(const Frustum & frustum, const Vector & light_direction)
{
    std::vector<Plane> ret;
    bool facing_light[6];

    for (unsigned i=0; i<6; ++i)
    {        
        facing_light[i] = vecDot(&light_direction, &frustum.getPlane()[i].normal_) < 0.0f;
        if (!facing_light[i])
        {
            ret.push_back(frustum.getPlane()[i]);

            if (i == FP_FAR)
            {
                ret.back().d_ += frustum.getYon() - MAX_SHADOW_BLOCKER_DISTANCE;
            }
        }
    }

//    return ret;

    // Determine silhouette edges
    FRUSTUM_PLANE plane_1 [4]    = {FP_NEAR, FP_LEFT, FP_RIGHT, FP_FAR}; // Planes to test: near, left, right, far
    FRUSTUM_PLANE plane_2 [4][4] = {{FP_LEFT, FP_RIGHT, FP_TOP, FP_BOTTOM},
                                    {FP_TOP, FP_BOTTOM},
                                    {FP_TOP, FP_BOTTOM},
                                    {FP_LEFT, FP_RIGHT, FP_TOP, FP_BOTTOM}}; // Planes to test against
    
    unsigned num_to_test[4] = {4,2,2,4};

    // Neighboring edges for planes except near&far
    unsigned edges[4][2] = {{FE_TOP_LEFT,     FE_BOTTOM_LEFT},  // left plane:   top left, bottom left edge
                            {FE_BOTTOM_RIGHT, FE_TOP_RIGHT},  // right plane:  bottom right, top right edge
                            {FE_TOP_LEFT,     FE_TOP_RIGHT},  // top plane:    top left, top right edge
                            {FE_BOTTOM_LEFT,  FE_BOTTOM_RIGHT}}; // bottom plane: bottom left, bottom right edge
    
    for (unsigned i=0; i<4; ++i)
    {
        for (unsigned j=0; j<num_to_test[i]; ++j)
        {
            FRUSTUM_PLANE p1 = plane_1[i];
            FRUSTUM_PLANE p2 = plane_2[i][j];

            // skip non-silhouette edges
            if (facing_light[p1] == facing_light[p2]) continue;

            if (p1>p2) std::swap(p1,p2);
                
            // Now we need to construct a plane collinear to this edge
            // and extending towards the light
            
            float dist;      // dist from eyepoint to point on plane: either hither or yon
            unsigned edge;   // index of the edge which leads to point on plane
            Vector edge_dir; // Edge direction of the silhouette edge.

            if (p1 == 0)
            {
                // One plane is the near plane.
                dist = frustum.getHither();
                edge = edges[p2-1][0]; // pick any edge neighboring the other plane

                vecCross(&edge_dir,
                         &frustum.getPlane()[p1].normal_,
                         &frustum.getPlane()[p2].normal_);
                    
            } else if (p2 == 5)
            {
                // One plane is the far plane.
                dist = MAX_SHADOW_BLOCKER_DISTANCE;
                edge = edges[p1-1][0];  // pick any edge neighboring the other plane

                vecCross(&edge_dir,
                         &frustum.getPlane()[p1].normal_,
                         &frustum.getPlane()[p2].normal_);

            } else 
            {
                // Neither near or far plane are neighbors of the edge.
                
                dist = MAX_SHADOW_BLOCKER_DISTANCE;

                // Find an edge that is neighbor to both planes
                if ((edges[p1-1][0] == edges[p2-1][0]) ||
                    (edges[p1-1][0] == edges[p2-1][1]))
                {
                    edge = edges[p1-1][0];
                } else
                {
                    assert(edges[p1-1][1] == edges[p2-1][0] ||
                           edges[p1-1][1] == edges[p2-1][1]);
                    edge = edges[p1-1][1];
                }

                edge_dir = frustum.getEdgeDir()[edge];
            }

            // Now calculate the point on the new plane and the new
            // plane's normal
            Vector p = frustum.getEyePos() + frustum.getEdgeDir()[edge] * dist;
            Vector n;
            vecCross(&n, &light_direction, &edge_dir);

            // Make sure n points outside our volume. Check whether a
            // point inside the frustum is on the right side of our
            // plane.
            Vector point_inside = frustum.getEyePos() +
                frustum.getHither()*(frustum.getEdgeDir()[FE_TOP_LEFT] +
                                     frustum.getEdgeDir()[FE_BOTTOM_RIGHT]);

            Vector diff = point_inside - p;
            if (vecDot(&n, &diff) > 0.0f) n *= -1.0f;
                
            Plane plane;
            plane.create(p, n);

            ret.push_back(plane);

        }
    }
    
    return ret;
}
