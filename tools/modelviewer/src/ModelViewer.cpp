

#include "ModelViewer.h"

#include <boost/filesystem.hpp>


#include <osg/Material>

#include "VariableWatcher.h"
#include "InputHandler.h"

#include "Gui.h"
#include "GUIConsole.h"

#include "Scheduler.h"
#include "VariableWatcherVisual.h"
#include "SdlApp.h"

#include "SdlKeyNames.h" // for mouse button injection
#include "UserPreferences.h"

#include "Paths.h"

#include "ReaderWriterBbm.h"
#include "TextureManager.h"
#include "EffectManager.h"
#include "ParticleManager.h"

#include "LodUpdater.h"
#include "UtilsOsg.h"
#include "BbmOsgConverter.h"
#include "InstancedGeometry.h"


const std::string VIEWER_KEYMAP_SUPERSECTION = "viewer_keymap";


//------------------------------------------------------------------------------
/**
 *  Get all groups from a model for autocompletion.
 */
class GetAllGroupsVisitor : public osg::NodeVisitor
{
 public:
    GetAllGroupsVisitor() :
        osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
        {
            setNodeMaskOverride(NODE_MASK_OVERRIDE);            
        }

    virtual void apply (osg::Node & node)
        {
            addGroups(dynamic_cast<NodeUserData*>(node.getUserData()));
            traverse(node);
        }    

    std::vector<std::string> getGroups() const
        {
            std::vector<std::string> ret;
            copy(groups_.begin(), groups_.end(), back_inserter(ret));
            return ret;
        }
    
 protected:

    void addGroups(const NodeUserData * ud)
        {
            if (!ud) return;

            for (unsigned i=0; i<ud->groups_.size(); ++i)
            {
                for (unsigned j=0; j<ud->groups_[i].size(); ++j)
                {
                    groups_.insert(ud->groups_[i][j]);
                }
            }
        }
    
    std::set<std::string> groups_;
};


//------------------------------------------------------------------------------
ModelViewer::ModelViewer() :
    MetaTask("ModelViewer"),
    current_model_name_("")
{    
    loadParameterFiles();

    gui_console_.reset(new GUIConsole(root_window_));

    s_console.addFunction("toggleWireframeMode",
                          Loki::Functor<void> (&s_scene_manager, &SceneManager::toggleWireframeMode),
                          &fp_group_);

    s_console.addFunction("loadModel",
                          ConsoleFun(this, &ModelViewer::loadModel),
                          &fp_group_,
                          ConsoleCompletionFun(this, &ModelViewer::loadModelCompletionFun));

    s_console.addFunction("reloadModel",
                          Loki::Functor<void> (this, &ModelViewer::reloadModel),
                          &fp_group_);

    s_console.addFunction("activateGroups",
                          ConsoleFun(this, &ModelViewer::activateGroups),
                          &fp_group_,
                          ConsoleCompletionFun(this, &ModelViewer::groupsCompletionFun));

    s_console.addFunction("deactivateGroups",
                          ConsoleFun(this, &ModelViewer::deactivateGroups),
                          &fp_group_,
                          ConsoleCompletionFun(this, &ModelViewer::groupsCompletionFun));

    s_console.addFunction("setLodLevel",
                          ConsoleFun(this, &ModelViewer::setLodLevel),
                          &fp_group_);

    s_console.addFunction("toggleLodUpdater",
                          Loki::Functor<void> (this, &ModelViewer::toggleLodUpdater),
                          &fp_group_);

    s_console.addFunction("saveCameraOrientation",
                          Loki::Functor<void> (this, &ModelViewer::saveCameraOrientation),
                          &fp_group_);

    s_console.addFunction("setLightDirFromCamera",
                          Loki::Functor<void> (this, &ModelViewer::setLightDirFromCamera),
                          &fp_group_);

    s_console.addFunction("setAmbient",
                          ConsoleFun(this, &ModelViewer::setAmbient),
                          &fp_group_);

    s_scene_manager.init();

    // Uniform needed for grass rendering...
    osg::Uniform * draw_dist_uniform =
        s_scene_manager.getRootNode()->getOrCreateStateSet()->getOrCreateUniform("instance_draw_dist",
                                                                                 osg::Uniform::FLOAT,
                                                                                 1);
    draw_dist_uniform->setElement(0, 1000.0f);

    
    new VariableWatcherVisual();

    s_scheduler.addFrameTask(PeriodicTaskCallback(&camera_, &Entity::frameMove),
                             "camera_::frameMove",
                             &fp_group_);

    registerInputHandlerFunctions();


    camera_.setTransform(s_params.get<Matrix>("viewer.prefs.default_cam"));

    s_scene_manager.setAmbient(s_params.get<float>("viewer.prefs.default_ambient"));
    s_scene_manager.setClearColor(s_params.get<Vector>("viewer.prefs.default_background_color"));

    

    // load any command line model
    loadModel(s_app.getCommandLineArguments());

}

//------------------------------------------------------------------------------
ModelViewer::~ModelViewer()
{
    s_input_handler.unloadKeymap(getUserConfigFile(), DEFAULT_KEYMAP_SUPERSECTION);
    s_input_handler.unloadKeymap(getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);
    s_input_handler.unloadKeymap(VIEWER_CONFIG_FILE, VIEWER_KEYMAP_SUPERSECTION);
    s_scene_manager.reset();
}

//------------------------------------------------------------------------------
void ModelViewer::render()
{
    s_scene_manager.getCamera().setTransform(camera_);    

    s_scene_manager.render();

    s_gui.render();

    s_variable_watcher.frameMove();

    SDL_GL_SwapBuffers();
}


//------------------------------------------------------------------------------
void ModelViewer::onKeyDown(SDL_keysym sym)
{
    // Always pass input to gui, with one exception: if console is
    // open and mouse is captured (debugging purpose of driving with
    // open console) then the input is not injected to gui
    if(!(gui_console_->isActive() && s_app.isMouseCaptured()))
    {
        if(s_gui.onKeyDown(sym))
            return;
    }

    if (s_input_handler.onKeyDown(sym.sym)) return;
    
    switch(sym.sym)
    {
    case SDLK_q:
        if(sym.mod & KMOD_CTRL) // quit on Ctrl+Q
        {
            s_app.quit();
        }
        break;
    case SDLK_f: /// XXXX this doesn't work currently, input is eaten nevertheless.
        if(sym.mod & KMOD_CTRL)
        {
            s_app.toggleFullScreen();
        }
        break;
        
    default:
        break;
    }
}

//------------------------------------------------------------------------------
void ModelViewer::onKeyUp(SDL_keysym sym)
{
    if(s_gui.onKeyUp(sym))
        return;

    s_input_handler.onKeyUp(sym.sym);
}

//------------------------------------------------------------------------------
void ModelViewer::onMouseButtonDown(const SDL_MouseButtonEvent & event)
{
    if(!s_app.isMouseCaptured())
    {
        if(s_gui.onMouseButtonDown(event))
            return;
    }

    switch (event.button)
    {
    case SDL_BUTTON_LEFT:
        if (!s_app.isMouseCaptured()) s_app.captureMouse(true);
        else s_input_handler.onKeyDown(input_handler::KEY_LeftButton);
        break;
    case SDL_BUTTON_MIDDLE:
        s_input_handler.onKeyDown(input_handler::KEY_MiddleButton);
        break;
    case SDL_BUTTON_RIGHT:
        if (s_app.isMouseCaptured()) s_app.captureMouse(false);
        else s_input_handler.onKeyDown(input_handler::KEY_RightButton);
        break;
    }
}

//------------------------------------------------------------------------------
void ModelViewer::onMouseButtonUp(const SDL_MouseButtonEvent & event)
{
    if(!s_app.isMouseCaptured())
    {
        if(s_gui.onMouseButtonUp(event))
            return;
    }

    switch (event.button)
    {
    case SDL_BUTTON_LEFT:
        s_input_handler.onKeyUp(input_handler::KEY_LeftButton);
        break;
    case SDL_BUTTON_MIDDLE:
        s_input_handler.onKeyUp(input_handler::KEY_MiddleButton);
        break;
    case SDL_BUTTON_RIGHT:
        s_input_handler.onKeyUp(input_handler::KEY_RightButton);
        break;
    }
}

//------------------------------------------------------------------------------
void ModelViewer::onMouseMotion(const SDL_MouseMotionEvent & event)
{
    if (!s_app.isMouseCaptured())
    {
        s_gui.onMouseMotion(event);
        return;
    }
    
    // Bail on WarpMouse-induced events
    if (event.x == (s_app.getScreenWidth()  >> 1) &&
        event.y == (s_app.getScreenHeight() >> 1)) return;


    // Check mouse invert
    SDL_MouseMotionEvent tmp_event = event;
    if (!s_params.get<bool>("camera.invert_mouse")) tmp_event.yrel = -event.yrel;

    s_input_handler.setMousePos(Vector2d(tmp_event.x,    tmp_event.y),
                                Vector2d(tmp_event.xrel, tmp_event.yrel));
    
    SDL_WarpMouse(s_app.getScreenWidth()  >> 1,
                  s_app.getScreenHeight() >> 1);   
}

//------------------------------------------------------------------------------
void ModelViewer::onResizeEvent(unsigned width, unsigned height)
{
    s_log << Log::debug('i')
          << "Resize to " << width << "x" << height << "\n";
    
    s_scene_manager.setWindowSize(width, height);
}

//------------------------------------------------------------------------------
void ModelViewer::registerInputHandlerFunctions()
{
    s_input_handler.registerInputCallback("Pickup/Drop Beacon",
                                          input_handler::ContinuousInputHandler(this, &ModelViewer::up),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Drop Mine",
                                          input_handler::ContinuousInputHandler(this, &ModelViewer::down),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Accelerate",
                                          input_handler::ContinuousInputHandler(this, &ModelViewer::forward),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Decelerate",
                                          input_handler::ContinuousInputHandler(this, &ModelViewer::backward),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Turn left",
                                          input_handler::ContinuousInputHandler(this, &ModelViewer::left),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Turn right",
                                          input_handler::ContinuousInputHandler(this, &ModelViewer::right),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Handbrake",
                                          input_handler::ContinuousInputHandler(this, &ModelViewer::speedup),
                                          &fp_group_);

    
    s_input_handler.registerInputCallback("view",
                                          input_handler::MouseInputHandler(this, &ModelViewer::handleMouseMotion),
                                          &fp_group_);

    s_input_handler.registerInputCallback("refresh",
                                          input_handler::SingleInputHandler(this, &ModelViewer::reloadModel),
                                          &fp_group_);

    s_input_handler.registerInputCallback("loadCameraOrientation",
                                          input_handler::SingleInputHandler(this, &ModelViewer::loadCameraOrientation),
                                          &fp_group_);

    s_input_handler.registerInputCallback("Take Screenshot",
                                          input_handler::SingleInputHandler(this, &ModelViewer::takeScreenshot),
                                          &fp_group_);
    

    // Now all input handling functions should have been
    // registered, load & apply keymap
    s_input_handler.loadKeymap  (getUserConfigFile(), DEFAULT_KEYMAP_SUPERSECTION);
    s_input_handler.loadKeymap  (getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);
    s_input_handler.loadKeymap  (VIEWER_CONFIG_FILE, VIEWER_KEYMAP_SUPERSECTION);
}

//------------------------------------------------------------------------------
void ModelViewer::loadParameterFiles()
{
    s_log << Log::debug('i') << "Loading Parameter files\n";
}




//------------------------------------------------------------------------------
void ModelViewer::up     (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.y_ = s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.y_ = 0;
}

//------------------------------------------------------------------------------
void ModelViewer::down   (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.y_ = -s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.y_ = 0;
}

//------------------------------------------------------------------------------
void ModelViewer::forward (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.z_ = -s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.z_ = 0;
}


//------------------------------------------------------------------------------
void ModelViewer::backward(bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.z_ = s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.z_ = 0;
}

//------------------------------------------------------------------------------
void ModelViewer::right  (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.x_ = s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.x_ = 0;
}

//------------------------------------------------------------------------------
void ModelViewer::left   (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.x_ = -s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.x_ = 0;
}


//------------------------------------------------------------------------------
void ModelViewer::speedup(bool b)
{
    if (b) camera_.getLocalVelocity() *= 20.0f;
    else camera_.getLocalVelocity()   /= 20.0f;
}


//------------------------------------------------------------------------------
void ModelViewer::handleMouseMotion(Vector2d pos, Vector2d delta)
{
    float rotate_speed = s_params.get<float>("camera.rotate_speed");
    camera_.changeOrientation( rotate_speed * delta.x_,
                               -rotate_speed * delta.y_);
}


//------------------------------------------------------------------------------
void ModelViewer::takeScreenshot()
{
    saveScreenshot("screenshots/");    
}


//------------------------------------------------------------------------------
std::string ModelViewer::loadModel(const std::vector<std::string> & args)
{
    if(args.size() != 1) return "Need model name as argument";
    
    if (current_model_ != NULL)
    {
        current_model_->removeFromScene();
        current_model_ = NULL;
    }

    // Remove previous data & state
    s_scene_manager.reloadCachedShaderPrograms();
    s_model_manager.   unloadAllResources();
    s_effect_manager.  unloadAllResources();
    s_particle_manager.unloadAllResources();
    s_scene_manager.getInstanceManager()->reset();
    s_texturemanager.  unloadAllResources();
    

    // Respect changes in config files
    s_params.loadParameters("config_client.xml");
    s_params.loadParameters("config_common.xml");
    s_params.loadParameters(getUserConfigFile(), CONFIG_SUPERSECTION);
    
                           
    // strip model name to plain file name
    boost::filesystem::path full_model_name;
    full_model_name = args[0];
    current_model_name_ = full_model_name.leaf();
    current_model_name_ = current_model_name_.substr(0, current_model_name_.rfind(".bbm"));

    current_model_ = ReaderWriterBbm::loadModel(current_model_name_);
    current_model_->addToScene(); 


    
    if(gui_console_->isActive()) gui_console_->toggleShow(); // comfort func
    s_app.captureMouse(true);   // comfort func

    // activate default groups
    activateGroups(s_params.get<std::vector<std::string> >("viewer.prefs.groups"));


    osg::ref_ptr<osg::Material> material = new osg::Material;

    material->setAmbient  (osg::Material::FRONT_AND_BACK, osg::Vec4(1,1,1,1));
    material->setDiffuse  (osg::Material::FRONT_AND_BACK, osg::Vec4(1,1,1,1));
    material->setSpecular (osg::Material::FRONT_AND_BACK, osg::Vec4(0,0,0,0));
    material->setShininess(osg::Material::FRONT_AND_BACK, 0);

    if (current_model_->getOsgNode())
    {
        current_model_->getOsgNode()->getOrCreateStateSet()->setAttribute(material.get());
    }

    return "Successfully loaded: " + current_model_name_;
}


//------------------------------------------------------------------------------
std::vector<std::string> ModelViewer::loadModelCompletionFun(const std::vector<std::string>& args) const
{
    
    using namespace boost::filesystem;

    std::vector<std::string> ret;
    if (args.size() > 1) return ret;

    path model_path(MODEL_PATH);
    for (directory_iterator it(model_path);
         it != directory_iterator();
         ++it)
    {
        std::string name = it->path().leaf();
        std::size_t p = name.rfind(".bbm");
        if (p != name.length()-4) continue;
        
        ret.push_back(name.substr(0, p));
    }

    if (args.size()==1) Console::filterPrefix(args[0], ret);
    
    return ret;
}



//------------------------------------------------------------------------------
void ModelViewer::reloadModel()
{
    if(current_model_name_.empty()) return;    

    std::vector<std::string> m;
    m.push_back(current_model_name_);
    loadModel(m);
}

//------------------------------------------------------------------------------
std::string ModelViewer::activateGroups(const std::vector<std::string> & args)
{
    return setGroups(args, true);
}

//------------------------------------------------------------------------------
std::string ModelViewer::deactivateGroups(const std::vector<std::string> & args)
{
    return setGroups(args, false);
}

//------------------------------------------------------------------------------
std::string ModelViewer::setGroups(const std::vector<std::string> & args, bool enable)
{
    if(!current_model_) return "No model loaded.";

    for(unsigned c=0; c< args.size(); c++)
    {
        if(current_model_->getOsgNode() && !args[c].empty())
        {
            EnableGroupVisitor v(args[c], enable);
            current_model_->getOsgNode()->accept(v);
        }
    }

    return "";
}

//------------------------------------------------------------------------------
std::vector<std::string> ModelViewer::groupsCompletionFun(const std::vector<std::string>&args) const
{
    std::vector<std::string> ret;
    
    if (args.size() > 1) return ret;
    
    osg::MatrixTransform * root = current_model_->getOsgNode();
    if (!root) return ret;

    GetAllGroupsVisitor v;
    root->accept(v);
    ret = v.getGroups();

    if (args.size() == 1) Console::filterPrefix(args[0], ret);

    return ret;
}


//------------------------------------------------------------------------------
std::string ModelViewer::setLodLevel(const std::vector<std::string> & args)
{
    if(!current_model_) return "No model loaded.";

    if(!args.empty() && !args[0].empty())
    {
       unsigned lod_level = 0;
       lod_level = fromString<unsigned>(args[0]);

       if((lod_level >= 0) && (lod_level <= NUM_LOD_LVLS))
       {
            current_model_->setLodLevel(lod_level);
       }
    }

    return "";
}

//------------------------------------------------------------------------------
void ModelViewer::toggleLodUpdater() const
{
    s_lod_updater.enable(!s_lod_updater.isEnabled());

    s_log << "LodUpdater is now " << (s_lod_updater.isEnabled() ? "On" : "Off")
          << "\n";
}

//------------------------------------------------------------------------------
void ModelViewer::saveCameraOrientation() const
{
    s_params.set<Matrix>("viewer.prefs.default_cam", camera_);
    s_params.saveParameters(VIEWER_CONFIG_FILE);
}

//------------------------------------------------------------------------------
void ModelViewer::loadCameraOrientation()
{
    camera_.setTransform(s_params.get<Matrix>("viewer.prefs.default_cam"));
}

//------------------------------------------------------------------------------
void ModelViewer::setLightDirFromCamera() const
{
    Vector view_dir = -camera_.getZ();
    view_dir.x_ += 0.01f; // little offset to allow correct shadow calc.
    s_scene_manager.setLightDir(view_dir);
}

//------------------------------------------------------------------------------
std::string ModelViewer::setAmbient(const std::vector<std::string> & args)
{
    if(!args.empty() && !args[0].empty())
    {
       float ambient = 0.0f;
       ambient = fromString<float>(args[0]);

       s_scene_manager.setAmbient(ambient);
    }

    return "";
}
