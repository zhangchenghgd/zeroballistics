#ifndef TANK_SCENEMANAGER_INCLUDED
#define TANK_SCENEMANAGER_INCLUDED

#include <map>

#include <osg/Node>
#include <osg/ClipNode>
#include <osg/MatrixTransform>
#include <osg/NodeVisitor>
#include <osg/Matrix>
#include <osg/Texture2D>
#include <osg/Timer>


#include <osgViewer/Viewer>

#include "Log.h"
#include "Singleton.h"
#include "Matrix.h"
#include "Observable.h"
#include "Camera.h"
#include "RegisteredFpGroup.h"


//------------------------------------------------------------------------------
enum BIN_NUMBER
{
    BN_SKY,
    BN_TERRAIN,
    BN_DEFAULT,
    BN_WATER,
    BN_TRANSPARENT, // use with "DepthSortedBin"
    BN_HUD = BN_TRANSPARENT + 100, // generous offset for particle effects
    BN_HUD_MINIMAP_MASK,
    BN_HUD_MINIMAP,
    BN_HUD_OVERLAY,
    BN_HUD_MINIMAP_ICONS,
    BN_HUD_TEXT
};

/***
 * During the pre-render traversal of the scene graph a logical AND
 * comparison of the current node mask and camera's cull mask is made. 
 * If the comparison is non-zero, the traversal will continue, if the 
 * logical AND is zero, traversal will not continue further down the scene.
***/
const osg::Node::NodeMask CAMERA_MASK_SEE_VISIBLE   = 0x00000011;
const osg::Node::NodeMask NODE_MASK_INVISIBLE       = 0x10000000; ///< must not be zero
const osg::Node::NodeMask NODE_MASK_VISIBLE         = 0x00000001;
const osg::Node::NodeMask NODE_MASK_OVERRIDE        = 0xffffffff;

const int BASE_TEX_UNIT     = 0;
const int LIGHT_TEX_UNIT    = 1;
const int SHADOW_TEX_UNIT   = 2;
const int EMISSIVE_TEX_UNIT = 3;


const unsigned TANGENT_ATTRIB    = 4;
const unsigned BI_TANGENT_ATTRIB = 5;


const std::string LOD_LVL_NAME[] = { "lod1", "lod2", "lod3" };
const unsigned NUM_LOD_LVLS = sizeof(LOD_LVL_NAME) / sizeof(std::string);


namespace osg
{
    class Fog;
}

namespace osgDB
{
    class Registry;
}


class InstanceManager;
class Shadow;


//------------------------------------------------------------------------------
enum SCENEMANAGER_OBSERVABLE_EVENT
{
    SOE_RESOLUTION_CHANGED
};

//------------------------------------------------------------------------------
struct ShaderDesc
{
    std::string filename_;
    std::string defines_;
    osg::ref_ptr<osg::Program> program_;
};



#define s_scene_manager Loki::SingletonHolder<SceneManager, Loki::CreateUsingNew, SingletonSceneManagerLifetime >::Instance()
//------------------------------------------------------------------------------
class SceneManager : public Observable
{
    DECLARE_SINGLETON(SceneManager);
    
 public:
    virtual ~SceneManager();

    void init();
    void reset();

    osg::Group * getRootNode();
    void addNode(osg::Node * node);

    void addHudNode(osg::Node * node);
    
    void render();
    osg::ref_ptr<osg::MatrixTransform> insertLocalTransformNode(const osg::ref_ptr<osg::Node> curr_node);

    std::vector<osg::Node*> findNode(const std::string & name, osg::Node * parent = NULL);

    void printCompleteNodeHierarchy();
    void printNodeHierarchy(osg::Node* node,
                            unsigned depth = 0,
                            std::vector<unsigned> print_pipe = std::vector<unsigned>());
    
    void printProgramInfoLog(const std::string & prog_name,
                             const std::string & log,
                             const std::string & vert_src,
                             const std::string & frag_src);
    
    osg::Matrix getWorldCoords(osg::Node* node);

    void setLightDir(const Vector & dir);

    void setAmbient(float a);
    float getAmbient() const;
    
    void setFogProperties(const Color & color, float offset, float factor);
    void setClearColor(const Vector & color, unsigned clear_mask = GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    
    void setWindowSize(unsigned width, unsigned height);
    void getWindowSize(unsigned & width, unsigned & height) const;

    void toggleWireframeMode();

    void scheduleNodeForDeletion(osg::Node * node);

    osg::State & getOsgState();

    osg::Program * getCachedProgram(const std::string & filename,
                                    std::string defines = "",
                                    bool * reused = NULL);


    Camera & getCamera();
    Shadow * getShadow();
    
    float getMaxSupportedAnisotropy() const;
    std::string getOpenGlRenderer() const;

    unsigned getContextId() const;
    
    unsigned calculateStats();

    float getShaderModel() const;

    osg::Group * getShadowReceiverGroup();
    osg::Group * getDefaultGroup();

    InstanceManager * getInstanceManager();


    void reloadCachedShaderPrograms();
    std::string toggleShaderDebug(const std::vector<std::string> & args);
    
    void checkCapabilities(bool verbose);
    bool areShadersEnabled() const;
    
 protected:

    void initOsgRegistry();

    void clear();
    
    void initHud();

    std::string getShaderSource(const std::string & filename) const;

    void deleteScheduledNodes();
    
    osg::ref_ptr<osg::MatrixTransform> hud_root_;       ///< Root node for HUD elements.
    osg::ref_ptr<osg::Projection>      hud_projection_; ///< Needed to adapt aspect ratio.

    osg::ref_ptr<osgViewer::Viewer> viewer_;

    osg::ref_ptr<osg::Group> root_node_;
    osg::ref_ptr<osg::Camera> osg_camera_;


    osg::ref_ptr<osg::Light> light_;
    osg::ref_ptr<Shadow> shadow_;
    osg::ref_ptr<osg::Group> receiver_; 
    osg::ref_ptr<osg::Group> default_; 

    osg::ref_ptr<osg::Uniform> camera_pos_uniform_;


    osg::ref_ptr<osg::Fog>     fog_;
    osg::ref_ptr<osg::Uniform> fog_beta_uniform_;
    osg::ref_ptr<osg::Uniform> fog_offset_uniform_;

    

    std::vector<ShaderDesc> shader_desc_; ///< Used shaders are cached here.

    Camera camera_;
    
    std::vector<osg::Node*> nodes_to_be_deleted_;

    unsigned width_;
    unsigned height_;

    std::auto_ptr<InstanceManager> instance_manager_;

    bool shaders_enabled_; ///< Set depending on available extensions.

    std::vector<unsigned> shader_debug_;
    
    RegisteredFpGroup fp_group_;
};


//------------------------------------------------------------------------------
class DeleteNodeVisitor : public osg::NodeVisitor
{
 public:
    DeleteNodeVisitor(osg::Node * delete_node);

    virtual void apply(osg::Node &searchNode);

 private:
    osg::Node * delete_node_;
};




//------------------------------------------------------------------------------
class EnableGroupVisitor : public osg::NodeVisitor
{
 public:
    EnableGroupVisitor();
    EnableGroupVisitor(const std::string & group, bool enable );
    virtual void apply(osg::Node & node);

 protected:
    std::string group_;
    bool enable_;
};



//------------------------------------------------------------------------------
class FindMaxAabbVisitor : public osg::NodeVisitor
{
public:

    FindMaxAabbVisitor();    
    virtual void apply(osg::Geode & geode);
    const osg::BoundingBox & getBoundingBox() const;
    
protected:
    osg::BoundingBox bb_;
};


//------------------------------------------------------------------------------
class DrawBillboardStyle : public osg::Drawable::DrawCallback
{
 public:
    DrawBillboardStyle(const osg::Vec3 & offset);

    virtual void drawImplementation(osg::RenderInfo& renderInfo, const osg::Drawable * drawable) const;

 protected:

     osg::Vec3 offset_;
};

//------------------------------------------------------------------------------
class HudEnableCallback : public osg::NodeCallback
{
public:
    HudEnableCallback(osg::Group * hud_root);

    virtual void operator()(osg::Node *node, osg::NodeVisitor *nv);

protected:
    osg::ref_ptr<osg::Group> hud_root_;
    bool * hud_enable_;
};


#endif
