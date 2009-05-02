
#include "SceneManager.h"

#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT   /// due to old gl headers on windows
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#include <limits>

#include <osg/Fog>

#include <osg/Depth>
#include <osg/LightSource>
#include <osg/CullSettings>
#include <osgDB/Registry>
#include <osg/TexEnv>
#include <osg/BlendFunc>
#include <osg/CopyOp>
#include <osg/AlphaFunc>
#include <osg/CullFace>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/Projection>
#include <osg/PolygonMode>
#include <osg/GLExtensions>

#include <osgParticle/ModularEmitter>

#include <osgUtil/Statistics>
#include <osgViewer/Renderer> ///< used in calcStats

#include "Camera.h"
#include "ReaderWriterBbm.h"
#include "Profiler.h"
#include "UtilsOsg.h"
#include "Shadow.h"
#include "ParticleManager.h"
#include "BbmOsgConverter.h"


#include "SoundManager.h"
#include "Paths.h"
#include "InstancedGeometry.h"

#include "EffectManager.h"
#include "TextureManager.h"

#undef min
#undef max

//------------------------------------------------------------------------------
class ElapsedTimeCallback : public osg::Uniform::Callback
{
public:
    ElapsedTimeCallback() {};

    virtual void operator () (osg::Uniform * uniform, osg::NodeVisitor * nv)
    {
        uniform->set((float)nv->getFrameStamp()->getReferenceTime());
    };

};


//------------------------------------------------------------------------------
SceneManager::SceneManager() :
    width_(100),
    height_(100),
    instance_manager_(new InstanceManager)
{    
#ifdef ENABLE_DEV_FEATURES
    s_console.addFunction("printOsgTree",
                          Loki::Functor<void>(this, &SceneManager::printCompleteNodeHierarchy),
                          &fp_group_);
    s_console.addFunction("reloadShaders",
                          Loki::Functor<void>(this, &SceneManager::reloadCachedShaderPrograms),
                          &fp_group_);
    s_console.addFunction("toggleProgramDebug",
                          ConsoleFun(this, &SceneManager::toggleShaderDebug),
                          &fp_group_);
#endif
    // set notify level
    osg::setNotifyLevel((osg::NotifySeverity)s_params.get<unsigned>("client.log.osg_notify_level"));

    viewer_ = new osgViewer::Viewer;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> gw = viewer_->setUpViewerAsEmbeddedInWindow(0,0,width_,height_);
    viewer_->setSceneData(NULL); //  OOOO Workaround for osg / openthreads bug? OOOO
    viewer_->realize();
    osg_camera_ = viewer_->getCamera();
    
    initOsgRegistry();
}

//------------------------------------------------------------------------------
SceneManager::~SceneManager()
{
    s_log << Log::debug('d') << "SceneManager destructor\n";

    clear(); // controlled destruction order...
}

//------------------------------------------------------------------------------
/**
 *  Counterpart to reset().
 */
void SceneManager::init()
{
    /// needs valid contextId. XXXX Is in init because it sets param
    /// values if requirements are not met.
    checkCapabilities(false);
    
    root_node_ = new osg::Group();

    root_node_->setName("RootNode");
    root_node_->getOrCreateStateSet()->setAttributeAndModes(new osg::CullFace(osg::CullFace::BACK),
                                                            osg::StateAttribute::ON);

    viewer_->setSceneData(root_node_.get());

    // We want to use our own near & far plane for culling   
    osg_camera_->setComputeNearFarMode( osgUtil::CullVisitor::DO_NOT_COMPUTE_NEAR_FAR );
   // ->setRenderStage(new osgUtil::RenderStage(osgUtil::RenderBin::SORT_FRONT_TO_BACK));
    osg_camera_->setCullingMode(osg::CullStack::VIEW_FRUSTUM_CULLING); //| osg::CullStack::SMALL_FEATURE_CULLING );
    //osg_camera_->setSmallFeatureCullingPixelSize(5);

    // prepare scene viewer to support cull masks
    int inheritance_mask = osg_camera_->getInheritanceMask();
    inheritance_mask &= ~(osg::CullSettings::CULL_MASK);
    osg_camera_->setInheritanceMask(inheritance_mask);
  
    osg_camera_->setCullMask(CAMERA_MASK_SEE_VISIBLE); 

    root_node_->getOrCreateStateSet()->addUniform(new osg::Uniform("base_texture",     BASE_TEX_UNIT));
    root_node_->getOrCreateStateSet()->addUniform(new osg::Uniform("light_texture",    LIGHT_TEX_UNIT));
    root_node_->getOrCreateStateSet()->addUniform(new osg::Uniform("shadow_texture",   SHADOW_TEX_UNIT));
    root_node_->getOrCreateStateSet()->addUniform(new osg::Uniform("emissive_texture", EMISSIVE_TEX_UNIT));
    camera_pos_uniform_ = new osg::Uniform("camera_pos", osg::Vec3(0,0,0));
    root_node_->getOrCreateStateSet()->addUniform(camera_pos_uniform_.get());

    osg::Uniform * elapsed_time = new osg::Uniform("elapsed_time", 0.0f);
    elapsed_time->setUpdateCallback(new ElapsedTimeCallback());
    root_node_->getOrCreateStateSet()->addUniform(elapsed_time);
    

    default_ = new osg::Group();
    default_->setName("DefaultGroup");
    root_node_->addChild(default_.get());

    
    // Create and hold here to avoid cyclic dependency
    receiver_ = new osg::Group();
    receiver_->setName("ShadowReceiverGroup");
    root_node_->addChild(receiver_.get());

    if (s_params.get<bool>("client.shadows.enabled")) shadow_ = new Shadow(receiver_.get());

    // Lighting
    light_ = new osg::Light;
    light_->setLightNum(0);

    osg::ref_ptr<osg::LightSource> light_source = new osg::LightSource;
    light_source->setName("LightSource");
    light_source->setLight(light_.get());
    root_node_->addChild(light_source.get());

    // Lighting is done completely in shaders, so disable it here
    root_node_->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    
    // Default light dir, must happen after light and shadow initialization
    setLightDir(Vector(0.1,-1,0.1));
    
    initHud();
    
    s_particle_manager.init();


    fog_ = new osg::Fog;
    fog_offset_uniform_ = new osg::Uniform("fog_offset", 10.0f);
    fog_beta_uniform_   = new osg::Uniform("fog_beta", 0.001f);

    root_node_->getOrCreateStateSet()->setAttribute(fog_.get());
    root_node_->getStateSet()->addUniform(fog_offset_uniform_.get());
    root_node_->getStateSet()->addUniform(fog_beta_uniform_.get());
}



//------------------------------------------------------------------------------
void SceneManager::reset()
{
    clear();

    s_particle_manager.reset();

    viewer_ = new osgViewer::Viewer;
    viewer_->setUpViewerAsEmbeddedInWindow(0,0,width_,height_);
    
    viewer_->realize();
    osg_camera_ = viewer_->getCamera();
}


//------------------------------------------------------------------------------
osg::Group * SceneManager::getRootNode()
{
    return root_node_.get();
}


//------------------------------------------------------------------------------
void SceneManager::addNode(osg::Node * node)
{
    RootNodeUserData data;
    if (node->getUserData())
    {
        assert(dynamic_cast<RootNodeUserData*>(node->getUserData()));
        data = *((RootNodeUserData*)node->getUserData());
    }
    
    if (shadow_.get() && (data.flags_ & bbm::BNO_SHADOW_RECEIVER))
    {
        shadow_->addReceiver(node);        
    } else
    {
        default_->addChild(node);
    }
    
    if (shadow_.get() && (data.flags_ & bbm::BNO_SHADOW_BLOCKER))
    {
        shadow_->addBlocker(node);
    }
}


//------------------------------------------------------------------------------
void SceneManager::addHudNode(osg::Node * node)
{
    hud_root_->addChild(node);
}


//------------------------------------------------------------------------------
void SceneManager::render()
{
    deleteScheduledNodes();
    
    PROFILE(SceneManager::render);

    s_soundmanager.setListenerTransform(camera_.getTransform());
    
    camera_.applyTransform(); // needed in order to update frustum etc.

    // XXX move proj somewhere else, but take care on parameter change?
    Matrix pers = camera_.getPerspectiveTransform();
    osg_camera_->getProjectionMatrix().set(pers);

    Matrix view = camera_.getTransform().getAffineInverse();
    osg_camera_->getViewMatrix().set(view);

    camera_pos_uniform_->set(osg::Vec3(camera_.getPos().x_,
                                       camera_.getPos().y_,
                                       camera_.getPos().z_));
    
  
    glPushAttrib( GL_ALL_ATTRIB_BITS );
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    float sim_time = viewer_->getViewerFrameStamp()->getSimulationTime();
    sim_time += s_scheduler.getLastFrameDt() * s_params.get<float>("physics.time_scale");

#ifdef ENABLE_DEV_FEATURES
    ADD_STATIC_CONSOLE_VAR(bool, render_all, true);
    if (render_all)
    {
        PROFILE(render_osg_frame);
        viewer_->frame(sim_time);
    }
#else
    {
        PROFILE(render_osg_frame);
        viewer_->frame(sim_time);
    }
#endif
    

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glPopAttrib();


    /// XXX needs to be set before setClientActiveTextureUnit(0)
    /// somewhere inside osg a texcoordpointer is still set, which
    /// leads to an glDrawElements error outside of osg
    osg_camera_->getGraphicsContext()->getState()->disableTexCoordPointersAboveAndIncluding(0);

    // Hmm.. seems to be changed by osg and not set back
    osg_camera_->getGraphicsContext()->getState()->setClientActiveTextureUnit(0);    

    deleteScheduledNodes();

#ifdef ENABLE_DEV_FEATURES
    calculateStats();
#endif
}

//------------------------------------------------------------------------------
/**
 *  Insert a matrix transform node below the given \param parent. All
 *  children of parent are moved below this new local transform node.
 *
 *  \return The inserted node.
 */
osg::ref_ptr<osg::MatrixTransform> SceneManager::insertLocalTransformNode(const osg::ref_ptr<osg::Node> parent)
{
    if (!parent.get()) return NULL;

    osg::Group * group = parent->asGroup();
    if (!group)
    {
        Exception e;
        e << "Tried to insert local transform below non-group node "
          << parent->getName()
          << "\n";
        throw e;
    }
    
    osg::ref_ptr<osg::MatrixTransform> new_local_transform_node = new osg::MatrixTransform;
    new_local_transform_node->setName(group->getName() + "-LocalTransform");

    unsigned num_children = group->getNumChildren();
    for (unsigned i=0 ; i < num_children; i++)
    {
        // add children of group to new node
        new_local_transform_node->addChild( group->getChild(i) );           
    }

    group->removeChildren(0,num_children);              // remove all children from group        
    group->addChild(new_local_transform_node.get());    // add new transform node as child to group
    return new_local_transform_node;
}


//------------------------------------------------------------------------------
/**
 *  Recursively search children of the specified node for all
 *  occurences of nodes with the given name.
 */
std::vector<osg::Node*> SceneManager::findNode(const std::string& name, osg::Node * cur_node)
{
    std::vector<osg::Node*> ret;

    if (!cur_node) cur_node = root_node_.get();
    
    if (cur_node->getName() == name) ret.push_back(cur_node);

    osg::Group* group = cur_node->asGroup();
    if ( group ) 
    {
        for (unsigned int i = 0 ; i < group->getNumChildren(); i ++)
        {
            std::vector<osg::Node*> f = findNode(name, group->getChild(i));
            std::copy(f.begin(), f.end(), back_inserter(ret));

        }
    }

    return ret;
}

//------------------------------------------------------------------------------
void SceneManager::printCompleteNodeHierarchy()
{
    printNodeHierarchy(root_node_.get());
}

//------------------------------------------------------------------------------
void SceneManager::printNodeHierarchy(osg::Node* node, unsigned depth, std::vector<unsigned> print_pipe)
{
    if (!node)
    {
        s_log << " empty leaf node\n";
        return;
    }

    std::vector<unsigned>::iterator pipe_it = print_pipe.begin();
    for(unsigned a=0; a<depth; a++)
    {
        if (((pipe_it != print_pipe.end()) && *pipe_it == a) ||
            a == depth-1)
        {
            s_log << "|";

            if (pipe_it != print_pipe.end()) ++pipe_it;
        } else
        {
            s_log << " ";
        }

        if (a != depth-1) s_log << "  ";
    }
    if (depth != 0) s_log << "- ";

    std::string name = node->getName();
    if (name.empty()) name = "<unnamed>";
    s_log << name << " (" << node->className();
    if (node->getNodeMask() == NODE_MASK_INVISIBLE) s_log << " INV";
    s_log << ")";

    if (node->getStateSet()) s_log << " (stateset: " << node->getStateSet() << ")";

    // If node is a Geode, output drawables as well
    osg::Geode * geode = dynamic_cast<osg::Geode*>(node);
    if (geode)
    {
        s_log << " <> ";
        for (unsigned d=0; d<geode->getNumDrawables(); d++)
        {
            s_log << geode->getDrawable(d)->className() << " " << geode->getDrawable(d)->getName();
            if (geode->getDrawable(d)->getStateSet())
            {
                s_log << "(stateset:" <<  geode->getDrawable(d)->getStateSet() << ") ";
            }
        }
    }
    s_log << "\n";

    osg::Group* cur_group = node->asGroup();
    if (cur_group) 
    {
        print_pipe.push_back(depth);
        for (unsigned int i = 0 ; i < cur_group->getNumChildren(); i ++)
        {
            if (i == cur_group->getNumChildren()-1) print_pipe.pop_back();
            printNodeHierarchy(cur_group->getChild(i), depth+1, print_pipe);
        }
    } 
}

//------------------------------------------------------------------------------
/**
 *  Prints info log and source code with line numbers for the
 *  specified shader.
 */
void SceneManager::printProgramInfoLog(const std::string & prog_name,
                                       const std::string & log,
                                       const std::string & vert_src,
                                       const std::string & frag_src)
{
    s_log << "Shader " << prog_name << " infolog:\n"
          << log
          << "\n";

    const unsigned MAX_LINE_SIZE = 500;
    char buf[MAX_LINE_SIZE];

    {
        s_log << prog_name << ".vert: \n\n";
        unsigned line = 1;
        std::istringstream istr(vert_src);
        while (istr.getline(buf, MAX_LINE_SIZE, '\n'))
        {
            s_log << "(" << line++ << ") " << buf << "\n";
        }
    }

    {
        s_log << "\n" << prog_name << ".frag: \n\n";
        unsigned line = 1;
        std::istringstream istr(frag_src);
        while (istr.getline(buf, MAX_LINE_SIZE, '\n'))
        {
            s_log << "(" << line++ << ") " << buf << "\n";
        }
    }
}



//------------------------------------------------------------------------------
/**
 *  Given a valid node placed in a scene under a transform, return the
 *  world coordinates in an osg::Matrix.
 *
 *  Result is arbitrary if node has more than one parent.
 */
//------------------------------------------------------------------------------
osg::Matrix SceneManager::getWorldCoords(osg::Node * node) 
{
    assert(node);
    assert(!node->getParentalNodePaths().empty());
    return osg::computeLocalToWorld(node->getParentalNodePaths()[0]);
}


//------------------------------------------------------------------------------
void SceneManager::setLightDir(const Vector & dir)
{
    light_->setPosition(osg::Vec4(-dir.x_,
                                  -dir.y_,
                                  -dir.z_,
                                  0.0));

    if (shadow_.get()) shadow_->setLightDir(dir);
}

//------------------------------------------------------------------------------
void SceneManager::setAmbient(float a)
{
    light_->setAmbient(osg::Vec4(a,a,a, 1.0f));    
}

//------------------------------------------------------------------------------
float SceneManager::getAmbient() const
{
    return light_->getAmbient()[0];
}


//------------------------------------------------------------------------------
void SceneManager::setFogProperties(const Color & color, float offset, float factor)
{
    fog_->setColor(osg::Vec4(color.r_, color.g_, color.b_, 1.0f));
    fog_offset_uniform_->set(offset);
    fog_beta_uniform_->set(factor);
}

//------------------------------------------------------------------------------
void SceneManager::setClearColor(const Vector & color, unsigned clear_mask)
{
    osg_camera_->setClearColor(osg::Vec4(color.x_, color.y_, color.z_, 1.0f));
    osg_camera_->setClearMask(clear_mask);
}

//------------------------------------------------------------------------------
/**
 *  Sets the viewport size used by the HUD camera.
 *
 *  In order to avoid distortions, adapt projection to aspect ratio.
 *
 *  The screen looks like this for the hud:
 *
 *  _________
 * 1| |    | |
 *  | |    | |
 *  | |    | |
 *  | |    | |
 *  | |    | |
 * 0__________
 * -a 0    1 1+a
 *
 *
 *  where a is diff from the code below and depends on the monitor's
 *  aspect ratio and the area in the middle is square.
 */
void SceneManager::setWindowSize(unsigned width, unsigned height)
{
    width_  = width;
    height_ = height;
    
    float diff;
    if (width > height)
    {
        diff = 0.5f * ((float)width / height - 1.0f);
        hud_projection_->setMatrix(osg::Matrix::ortho2D(-diff, 1.0f + diff,0,1));
    } else
    {
        diff = 0.5f * ((float)height / width - 1.0f);
        hud_projection_->setMatrix(osg::Matrix::ortho2D(0,1,-diff,1.0f + diff));

        s_log << Log::warning
              << "Current aspect ratio not properly supported\n";
    }

    camera_.setViewportSize(width, height);
    osg_camera_->setViewport(0, 0, width, height);

    emit(SOE_RESOLUTION_CHANGED);
}

//------------------------------------------------------------------------------
void SceneManager::getWindowSize(unsigned & width, unsigned & height) const
{
    width  = width_;
    height = height_;
}


//------------------------------------------------------------------------------
void SceneManager::toggleWireframeMode()
{
    osg::StateSet * state_set = root_node_->getOrCreateStateSet();
    osg::PolygonMode * cur_mode = (osg::PolygonMode*)state_set->getAttribute(osg::StateAttribute::POLYGONMODE);

    if (cur_mode && cur_mode->getMode(osg::PolygonMode::FRONT_AND_BACK) == osg::PolygonMode::LINE)
    {
        cur_mode->setMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::FILL);
    } else
    {
        state_set->setAttribute(new osg::PolygonMode(osg::PolygonMode::FRONT_AND_BACK, osg::PolygonMode::LINE));
        osg_camera_->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    }
}

//------------------------------------------------------------------------------
void SceneManager::scheduleNodeForDeletion(osg::Node * node)
{
    if(node)
    {
        nodes_to_be_deleted_.push_back(node);
    }
}

//------------------------------------------------------------------------------
osg::State & SceneManager::getOsgState()
{
    return *(osg_camera_->getGraphicsContext()->getState());
}


//------------------------------------------------------------------------------
/**
 *  First checks whether a shader with the given preprocessor defines
 *  has been loaded before. If not, loads the shader from the
 *  specified file, using the specified preprocessor defines, and
 *  caches it for subsequent calls.
 *
 *  \param filename The name of the shader (residing in
 *  SHADER_PATH). ".vert" and ".frag" will be appended.
 *
 *  \param defines will be prepended to the shader source.
 *
 *  \param reused Whether a new shader was created or an existing one
 *  was reused.
 */
osg::Program * SceneManager::getCachedProgram(const std::string & filename,
                                              std::string defines,
                                              bool * reused)
{
#ifdef ENABLE_DEV_FEATURES
    bool b = false;
    try
    {
        b = s_params.get<bool>("dev.disable_shaders");
    } catch (ParamNotFoundException & e){}
    if (b &&
        defines.find("INSTANCE") == std::string::npos) return new osg::Program();
#endif
    
    // XXX move most of this stuff to default defines, control from
    // appropriate location? needs to be done before first model is
    // loaded (resource manager)....
    
    defines = "#version 110\n" + defines;
    defines += ("#define INSTANCE_BATCH_SIZE " +
                toString(s_params.get<unsigned>("client.graphics.instance_batch_size")) +
                "\n");

    defines += ("#define NUM_VEC3S_PER_INSTANCE " +
                toString(NUM_VEC3S_PER_INSTANCE) +
                "\n");

    defines += ("#define NUM_INSTANCE_LAYERS " +
                toString(s_params.get<unsigned>("instances.num_layers")) +
                "\n");


    defines += ("#define SHADOW_MAP_DT " +
                toString(2.0f / (float)s_params.get<unsigned>("client.shadows.map_size")) +
                "\n");
    

    /// \see InstancePlacer
    unsigned cell_resolution = s_params.get<unsigned>("instances.cell_resolution");
    float cell_size          = s_params.get<float>   ("instances.base_draw_distance") / cell_resolution;
    float base_draw_dist     = cell_size*((cell_resolution-1)>>1);
    defines += "#define BASE_DRAW_DIST " + toString(base_draw_dist) + "\n";


    // HACKY: determine whether to do PCF based on shadow map resolution.
    unsigned shadow_map_size = s_params.get<unsigned>("client.shadows.map_size");
    if (shadow_map_size >= 1024) defines += "#define PERCENTAGE_CLOSER_FILTERING";
    
    
    for (unsigned i=0; i<shader_desc_.size(); ++i)
    {
        if (filename == shader_desc_[i].filename_ &&
            defines  == shader_desc_[i].defines_)
        {
            s_log << Log::debug('r')
                  << "Reusing shader "
                  << filename
                  << "\n";

            osg::Program * ret = shader_desc_[i].program_.get();

            if (reused) *reused = true;
            return ret;
        }
    }

    if (reused) *reused = false;

    s_log << Log::debug('r')
          << "Creating shader "
          << filename
          << "\n";
    
    ShaderDesc new_desc;

    new_desc.filename_ = filename;
    new_desc.defines_  = defines;
    new_desc.program_  = new osg::Program;
    new_desc.program_->setName(filename);
    
    std::string vert_source = defines + getShaderSource(filename + ".vert");
    osg::Shader * vert_shader = new osg::Shader(osg::Shader::VERTEX, vert_source);
    vert_shader->setName(filename+".vert");
    new_desc.program_->addShader(vert_shader);
    
    std::string frag_source = defines + getShaderSource(filename + ".frag");
    osg::Shader * frag_shader = new osg::Shader(osg::Shader::FRAGMENT, frag_source);
    frag_shader->setName(filename+".frag");
    new_desc.program_->addShader(frag_shader);
    
    new_desc.program_->compileGLObjects(getOsgState());

    if (!new_desc.program_->getPCP(getOsgState().getContextID())->isLinked())
    {
        std::string log;
        new_desc.program_->getGlProgramInfoLog(getOsgState().getContextID(), log);
        printProgramInfoLog(filename, log, vert_source, frag_source);
        
        Exception e("Failed to compile shader \"");
        e << filename
          << "\". You might try lowering the quality settings or installing a new graphics driver.";
        throw e;
    }

    shader_desc_.push_back(new_desc);
    
    return new_desc.program_.get();
}



//------------------------------------------------------------------------------
Camera & SceneManager::getCamera()
{
    return camera_;
}

//------------------------------------------------------------------------------
Shadow * SceneManager::getShadow()
{
    return shadow_.get();
}


//------------------------------------------------------------------------------
float SceneManager::getMaxSupportedAnisotropy() const
{
    if (!osg::isGLExtensionSupported(getContextId(), "GL_EXT_texture_filter_anisotropic")) return 1.0f;
    
    GLfloat max_supported_anisotropy;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_supported_anisotropy);
    return max_supported_anisotropy;
}

//------------------------------------------------------------------------------
std::string SceneManager::getOpenGlRenderer() const
{
  const char * renderer = (const char *) glGetString(GL_RENDERER);
  const char * vendor = (const char *) glGetString(GL_VENDOR);

  if (strstr(vendor, "NVIDIA") != NULL)        return "nvidia";
  if (strstr(vendor, "ATI") != NULL)           return "ati";
  if (strstr(renderer, "GDI Generic") != NULL) return "microsoft_software";
  if (strstr(renderer, "Mesa") != NULL)        return "mesa_software";
  return "Unknown renderer";
}


//------------------------------------------------------------------------------
unsigned SceneManager::getContextId() const
{
    return osg_camera_->getGraphicsContext()->getState()->getContextID();    
}

//------------------------------------------------------------------------------
/**
 *  \return The number of rendered triangles
 */
unsigned SceneManager::calculateStats()
{
    osgUtil::Statistics stats;
    stats.setType(osgUtil::Statistics::STAT_PRIMS);

    osg::ref_ptr<osgViewer::Renderer> renderer = 
        dynamic_cast<osgViewer::Renderer*>(osg_camera_->getRenderer());

    osgUtil::SceneView * scene_view = renderer->getSceneView(0);
    
    assert(scene_view);
    scene_view->getStats(stats);

    unsigned num_drawables = stats.numDrawables;
    ADD_LOCAL_CONSOLE_VAR(unsigned, num_drawables);
    
    unsigned int num_triangles = 0;
    osgUtil::Statistics::PrimitiveCountMap::iterator primitive_iter;
    for(primitive_iter=stats.GetPrimitivesBegin();
        primitive_iter!=stats.GetPrimitivesEnd(); 
        ++primitive_iter)
    {
        num_triangles += primitive_iter->second;
    }
    ADD_LOCAL_CONSOLE_VAR(unsigned, num_triangles);


    
    unsigned num_particles = s_particle_manager.getNumParticles();
    ADD_LOCAL_CONSOLE_VAR(unsigned, num_particles);
    
    return num_triangles;
}


//------------------------------------------------------------------------------
/**
 *  http://ogltotd.blogspot.com/2006_12_01_archive.html
 */
float SceneManager::getShaderModel() const
{
    if (osg::isGLExtensionSupported(getContextId(), "GL_EXT_gpu_shader4")) return 4.0f;

    if (osg::isGLExtensionSupported(getContextId(), "GL_NV_vertex_program3"    ) ||
        osg::isGLExtensionSupported(getContextId(), "GL_ATI_shader_texture_lod")) return 3.0f;

    if (osg::isGLExtensionSupported(getContextId(), "GL_ARB_fragment_program") ||
        osg::isGLExtensionSupported(getContextId(), "GL_ARB_fragment_shader" )) return 2.0f;

    if (osg::isGLExtensionSupported(getContextId(), "GL_ATI_fragment_shader"     ) ||
        osg::isGLExtensionSupported(getContextId(), "GL_ATI_text_fragment_shader")) return 1.4f;

    if (osg::isGLExtensionSupported(getContextId(), "GL_ARB_vertex_program") ||
        osg::isGLExtensionSupported(getContextId(), "GL_ARB_vertex_shader" )) return 1.1f;

    if (osg::isGLExtensionSupported(getContextId(), "GL_NV_texture_shader"    ) ||
             osg::isGLExtensionSupported(getContextId(), "GL_NV_register_combiners")) return 1.1f;
       
    return 0.0f;
}

//------------------------------------------------------------------------------
osg::Group * SceneManager::getShadowReceiverGroup()
{
    return receiver_.get();
}

//------------------------------------------------------------------------------
osg::Group * SceneManager::getDefaultGroup()
{
    return default_.get();
}

//------------------------------------------------------------------------------
InstanceManager * SceneManager::getInstanceManager()
{
    return instance_manager_.get();
}

//------------------------------------------------------------------------------
void SceneManager::reloadCachedShaderPrograms()
{
    std::string debug_defines;
    for (std::vector<unsigned>::const_iterator it = shader_debug_.begin();
         it != shader_debug_.end();
         ++it)
    {
        debug_defines += "#define XXX_DEBUG_" + toString(*it) + "\n";
    }
    
    for (std::vector<ShaderDesc>::iterator it = shader_desc_.begin();
         it != shader_desc_.end();
         ++it)
    {
        while (it->program_->getNumShaders()) it->program_->removeShader(it->program_->getShader(0));
        it->program_->releaseGLObjects(&getOsgState());

        
        std::string vert_source = debug_defines + it->defines_ + getShaderSource(it->filename_ + ".vert");
        osg::Shader * vert_shader = new osg::Shader(osg::Shader::VERTEX, vert_source);
        vert_shader->setName(it->filename_+".vert");
        it->program_->addShader(vert_shader);
    
        std::string frag_source = debug_defines + it->defines_ + getShaderSource(it->filename_ + ".frag");
        osg::Shader * frag_shader = new osg::Shader(osg::Shader::FRAGMENT, frag_source);
        frag_shader->setName(it->filename_+".frag");
        it->program_->addShader(frag_shader);
        
        it->program_->compileGLObjects(getOsgState());
    }
}

//------------------------------------------------------------------------------
std::string SceneManager::toggleShaderDebug(const std::vector<std::string> & args)
{
    if (args.size() != 1) return "Need debug number as arg";

    unsigned n = fromString<unsigned>(args[0]);
    std::vector<unsigned>::iterator it = find(shader_debug_.begin(), shader_debug_.end(), n);

    std::string ret;
    if (it == shader_debug_.end())
    {
        ret = "Enabled shader debug " + args[0];
        shader_debug_.push_back(n);
    } else
    {
        ret = "Disabled shader debug " + args[0];
        shader_debug_.erase(it);
    }

    reloadCachedShaderPrograms();

    return ret;
}




//------------------------------------------------------------------------------
void SceneManager::checkCapabilities(bool verbose)
{
    float shader_model = getShaderModel();
    if (shader_model < 2.0f)
    {
        std::string error_msg   ("Shader Model 2.0 is required, either your graphics card");
                    error_msg += " or driver supports only Shader Model ";
                    error_msg += shader_model ? toString(shader_model) : std::string("1.0 or lower");
                    error_msg += " \nThe Program cannot continue.";

        throw Exception(error_msg);
    }

    GLint max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
    if (max_texture_units < 6)
    {
        std::string error_msg   ("Your graphics card or driver supports only less ");
                    error_msg += "than 6 texture units. The Program cannot continue.";

        throw Exception(error_msg);
    }
    
    if (!osg::isGLExtensionSupported(getContextId(), "GL_ARB_vertex_buffer_object"))
    {
        std::string error_msg   ("Either your graphics card or driver do not support the ARB ");
                    error_msg += "Vertex Buffer Object Extension. The Program cannot continue.";

        throw Exception(error_msg);
    }

    // Check GLSL version 110
    unsigned major=0, minor=0;
    const char * shader_version_str = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    if (shader_version_str)
    {
        std::string str = shader_version_str;
        //remove trailing info string
        str = str.substr(0, str.find(' '));
        Tokenizer tok(str.c_str(), '.');
        major = fromString<unsigned>(std::string(tok.getNextWord()));
        minor = fromString<unsigned>(std::string(tok.getNextWord()));
    }
    if (major <= 1 && minor < 10)
    {
        Exception e("At least GLSL version 1.10 is required to run this program, but only ");
        e << major << "." << minor << " is present.\nThe Program cannot continue.";
        throw e;
    }


    


    // ---------- Check for restrictions that are not fatal ----------
    
    if(!osg::isGLExtensionSupported(getContextId(), "GL_EXT_framebuffer_object"))
    {
        if (verbose)
        {
            s_log << Log::warning
                  << "EXT Framebuffer Object Extension is not supported. "
                  << "Disabling shadows and high quality water.\n";
        }
        s_params.set("client.shadows.enabled", false);

        unsigned cur_qual = s_params.get<unsigned>("client.graphics.water_quality");
        s_params.set("client.graphics.water_quality", std::min(cur_qual, 1u));
        
    } else
    {
        if(!osg::isGLExtensionSupported(getContextId(), "GL_ARB_shadow"))
        {
            if (verbose)
            {
                s_log << Log::warning
                      << "ARB Shadow Extension is not supported. Disabling shadows.\n";
            }
            s_params.set("client.shadows.enabled", false);
        }
    }
}


//------------------------------------------------------------------------------
void SceneManager::initOsgRegistry()
{
    osgDB::Registry::instance()->addReaderWriter(new ReaderWriterBbm());
    
    osgDB::ReaderWriter::Options * o;
    o = osgDB::Registry::instance()->getOptions();
    if (!o) o = new osgDB::ReaderWriter::Options;

    o->setObjectCacheHint(osgDB::ReaderWriter::Options::CACHE_ALL);
    o->setOptionString("dds_flip");

    osgDB::Registry::instance()->setOptions(o);   

#ifdef _WIN32
    // retrieve the application path
    char app_path[MAX_PATH] = "";
    std::string app_directory;

    ::GetModuleFileName(0, app_path, sizeof(app_path) - 1);

    // extract directory
    app_directory = app_path;
    app_directory = app_directory.substr(0, app_directory.rfind("\\"));

    // set local application path as library search path (for dll loading)
    osgDB::FilePathList & list = osgDB::Registry::instance()->getLibraryFilePathList();
    list.push_front(app_directory);
#endif

}



//------------------------------------------------------------------------------
void SceneManager::clear()
{
    viewer_             = NULL;

    fog_                = NULL;
    fog_offset_uniform_ = NULL;
    fog_beta_uniform_   = NULL;
    
    camera_pos_uniform_ = NULL;
    osg_camera_         = NULL; 
    root_node_          = NULL;

    receiver_           = NULL;
    light_              = NULL;
    shadow_             = NULL;

    default_            = NULL;

    hud_root_           = NULL;
    hud_projection_     = NULL;
    instance_manager_->reset();
}

//------------------------------------------------------------------------------
void SceneManager::initHud()
{
    hud_root_ = new osg::MatrixTransform;
    hud_root_->setName("HUD root");
    hud_root_->setReferenceFrame(osg::Transform::ABSOLUTE_RF);

    hud_projection_ = new osg::Projection;
    hud_projection_->setMatrix(osg::Matrix::ortho2D(0,0.5,0,1)); // only temporary, gets overridden by setWindowSize
    hud_projection_->setName("HUD projection");
    hud_projection_->addChild(hud_root_.get());    

    osg::ref_ptr<osg::StateSet> state_set = hud_projection_->getOrCreateStateSet();    
    state_set->setMode(GL_BLEND, osg::StateAttribute::ON);
    state_set->setAttribute(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    // Many pixels will be dropped altogether, so enable alpha test
    state_set->setMode(GL_ALPHA_TEST, osg::StateAttribute::ON);
    state_set->setAttribute(new osg::AlphaFunc(osg::AlphaFunc::GREATER, 0.0f));

    state_set->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    state_set->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    state_set->setRenderBinDetails( BN_HUD, "RenderBin");
    
    hud_projection_->setUpdateCallback(new HudEnableCallback(hud_root_.get()));

    root_node_->addChild(hud_projection_.get());
}




//------------------------------------------------------------------------------
std::string SceneManager::getShaderSource(const std::string & filename) const
{    
    std::ifstream source_file((SHADER_PATH + filename).c_str(), std::ios::binary);
    if(!source_file)
    {
        s_log << Log::warning
              << "Could not open shader file "
              << filename
              << "\n";
        return "";
    }

    std::string ret;
    std::string line;

    while(getline(source_file, line))
    {
        if (line.find("#include") == 0)
        {
            std::size_t de1 = line.find("\"");
            std::size_t de2 = line.rfind("\"");

            if (de1 == std::string::npos ||
                de2 == std::string::npos)
            {
                ret += line + "\n";
            } else ret += getShaderSource(line.substr(de1+1, de2-de1-1));
        } else ret += line + "\n";
    }

    return ret;
}


//------------------------------------------------------------------------------
/**
 *  Actually remove scheduled nodes from SceneGraph
 */
void SceneManager::deleteScheduledNodes()
{
    for(unsigned i=0; i < nodes_to_be_deleted_.size(); i++)
    {
        DeleteNodeVisitor delete_node(nodes_to_be_deleted_[i]);
        s_scene_manager.getRootNode()->accept(delete_node);      
    }
    nodes_to_be_deleted_.clear(); 
}



//------------------------------------------------------------------------------
// Constructor that accepts node pointer argument
// Initializes node to be deleted to node provided
// set the traversal mode to TRAVERSE_ALL_CHILDREN
DeleteNodeVisitor::DeleteNodeVisitor(osg::Node * delete_node) : 
    osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), 
    delete_node_(delete_node) 
{
    // Make sure nodes which have their nodemask set to 0 are deleted
    // as well.
    setNodeMaskOverride(NODE_MASK_OVERRIDE);
} 

//------------------------------------------------------------------------------
// The 'apply' method for 'node' type instances.
// go through all nodes and search for a child that matches the node to be deleted
// if a match is found, remove the child from its parent (deletion should be done
// through osg_ref pointer).
void DeleteNodeVisitor::apply(osg::Node &searchNode) 
{
    osg::Group * search_node_parent = searchNode.asGroup();
    if(search_node_parent)
    {
        for(unsigned int i=0; i<search_node_parent->getNumChildren(); ++i)
        {
            if(search_node_parent->getChild(i) == delete_node_)
            {
                search_node_parent->removeChild(i);
            }
        }

    }
   
   traverse(searchNode); 
}



//------------------------------------------------------------------------------
EnableGroupVisitor::EnableGroupVisitor() :
    osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
    enable_(false)
{
    setNodeMaskOverride(NODE_MASK_OVERRIDE);
}


//------------------------------------------------------------------------------
EnableGroupVisitor::EnableGroupVisitor(const std::string & group, bool enable ) :
    osg::NodeVisitor(TRAVERSE_ALL_CHILDREN),
    group_(group), enable_(enable)
{
    setNodeMaskOverride(NODE_MASK_OVERRIDE);
}


//------------------------------------------------------------------------------
void EnableGroupVisitor::apply(osg::Node & node)
{
    traverse(node);

    NodeUserData * data = dynamic_cast<NodeUserData*>(node.getUserData());
    if (!data) return;

    if (!group_.empty()) data->enableGroup(group_, enable_);
  
    EffectNode * eff = dynamic_cast<EffectNode*>(&node);
    if (eff)
    {
        // bail if currently changed group doesn't affect this node
        if (!data->isGroupMember(group_)) return;
        eff->setEnabled(data->isActive());
    } else
    {
//        s_log << data->isActive() << "\n";
        node.setNodeMask(data->isActive() ? NODE_MASK_VISIBLE : NODE_MASK_INVISIBLE);        
    }
}

//------------------------------------------------------------------------------
FindMaxAabbVisitor::FindMaxAabbVisitor() :
    osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
{
    // Make sure nodes which have their nodemask set to 0 are
    // visited as well.
    setNodeMaskOverride(NODE_MASK_OVERRIDE);
}


//------------------------------------------------------------------------------
void FindMaxAabbVisitor::apply(osg::Geode & geode)
{
    bb_.expandBy(geode.getBoundingBox());
    traverse(geode);
}
    

//------------------------------------------------------------------------------
const osg::BoundingBox & FindMaxAabbVisitor::getBoundingBox() const
{
    return bb_;
}


//------------------------------------------------------------------------------
/**
 *  \param offset Offset for the billboard (usally y)
 */
DrawBillboardStyle::DrawBillboardStyle(const osg::Vec3 & offset) :
    offset_(offset)
{
}
    

//------------------------------------------------------------------------------
void DrawBillboardStyle::drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable * drawable) const
{
    osg::Matrix inv_view_rotation = renderInfo.getState()->getInitialInverseViewMatrix();
    inv_view_rotation.setTrans(0,0,0);            

    // offset_ is given in world space. We need to know the
    // direction of the offset in camera space in order to
    // cancel the camera rotation.
    osg::Vec3 offset = inv_view_rotation.postMult(offset_);

    // The billboard needs to be rotated along the z-axis so
    // it stays upright in world space.
    //
    // Rotate up direction to projection of offset onto xy-plane.
    osg::Matrix mv;
    mv.makeRotate(osg::Vec3(0,1,0), osg::Vec3(offset.x(), offset.y(), 0));
    mv.setTrans(offset + renderInfo.getState()->getModelViewMatrix().getTrans());
            
                        
    glPushAttrib(GL_TRANSFORM_BIT); // push matrix mode
    glMatrixMode(GL_MODELVIEW);          
    glPushMatrix(); // push the MV matrix to place the billboard in world coords, 
    // without knowing about placement in scene graph
    glLoadMatrix(mv.ptr()); // load the new MV matrix to actually place the billboard

    drawable->drawImplementation(renderInfo);

    glPopMatrix(); // restore old state
    glPopAttrib();
}

//------------------------------------------------------------------------------
HudEnableCallback::HudEnableCallback(osg::Group * hud_root) :
        hud_root_(hud_root),
        hud_enable_(NULL)
{ 
    hud_enable_ = s_params.getPointer<bool>("hud.enable");
}

//------------------------------------------------------------------------------
void HudEnableCallback::operator()(osg::Node *node, osg::NodeVisitor *nv)
{
    traverse(node, nv);     
    
    if(*hud_enable_)
    {
        hud_root_->setNodeMask(NODE_MASK_VISIBLE);
    } else
    {
        hud_root_->setNodeMask(NODE_MASK_INVISIBLE);
    }
    
}
