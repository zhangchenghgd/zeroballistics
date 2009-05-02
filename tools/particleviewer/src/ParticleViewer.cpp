

#include "ParticleViewer.h"

#include <osgParticle/ModularEmitter>

#include <boost/filesystem.hpp>

#include "Paths.h"
#include "VariableWatcher.h"
#include "InputHandler.h"

#include "Gui.h"
#include "GUIConsole.h"

#include "Scheduler.h"
#include "VariableWatcherVisual.h"
#include "SdlApp.h"

#include "SdlKeyNames.h" // for mouse button injection
#include "UserPreferences.h"

#include "ParticleManager.h"
#include "ReaderWriterBbm.h"
#include "TextureManager.h"

#include "LodUpdater.h"

#include "EffectManager.h"

#include "CoordinateAxes.h"

const std::string VIEWER_KEYMAP_SUPERSECTION = "viewer_keymap";

//------------------------------------------------------------------------------
ParticleViewer::ParticleViewer() :
    MetaTask("ParticleViewer")
{    
    loadParameterFiles();

    gui_console_.reset(new GUIConsole(root_window_));

    s_console.addFunction("toggleWireframeMode",
                          Loki::Functor<void> (&s_scene_manager, &SceneManager::toggleWireframeMode),
                          &fp_group_);

    s_console.addFunction("addEffect",
                          ConsoleFun(this, &ParticleViewer::addEffect),
                          &fp_group_,
                          ConsoleCompletionFun(this, &ParticleViewer::loadEffectsCompletionFun));

    s_console.addFunction("removeEffect",
                          ConsoleFun(this, &ParticleViewer::removeEffect),
                          &fp_group_,
                          ConsoleCompletionFun(this, &ParticleViewer::loadEffectsCompletionFun));


    s_console.addFunction("reloadEffects",
                          Loki::Functor<void> (this, &ParticleViewer::reloadEffects),
                          &fp_group_);

    s_console.addFunction("listEffectsLoaded",
                          Loki::Functor<void> (this, &ParticleViewer::listEffectsLoaded),
                          &fp_group_);

    s_console.addFunction("saveCameraOrientation",
                          Loki::Functor<void> (this, &ParticleViewer::saveCameraOrientation),
                          &fp_group_);

    s_scene_manager.init(); // MMMM


    new VariableWatcherVisual();

    s_scheduler.addFrameTask(PeriodicTaskCallback(&camera_, &Entity::frameMove),
                             "camera_::frameMove",
                             &fp_group_);

    registerInputHandlerFunctions();

    camera_.setTransform(s_params.get<Matrix>("viewer.prefs.default_cam"));
    s_scene_manager.setClearColor(s_params.get<Vector>("viewer.prefs.default_background_color"));


    // add coord axes node
    coord_axes_ = new osg::Geode();
    setupCoordAxes(coord_axes_.get());
    s_scene_manager.addNode(coord_axes_.get());
}

//------------------------------------------------------------------------------
ParticleViewer::~ParticleViewer()
{      
    s_input_handler.unloadKeymap(getUserConfigFile(), DEFAULT_KEYMAP_SUPERSECTION);
    s_input_handler.unloadKeymap(getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);
    s_input_handler.unloadKeymap(VIEWER_CONFIG_FILE, VIEWER_KEYMAP_SUPERSECTION);
    s_scene_manager.reset();
}

//------------------------------------------------------------------------------
void ParticleViewer::render()
{
    s_scene_manager.getCamera().setTransform(camera_);    

    s_scene_manager.render();

    s_gui.render();

    /// count particles
    int num_particles = s_particle_manager.getNumParticles();

    ADD_LOCAL_CONSOLE_VAR(int, num_particles);

    s_variable_watcher.frameMove();

    SDL_GL_SwapBuffers();
}


//------------------------------------------------------------------------------
void ParticleViewer::onKeyDown(SDL_keysym sym)
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
            s_app.toggleFullScreenIfEnabled();
        }
        break;
        
    default:
        break;
    }
}

//------------------------------------------------------------------------------
void ParticleViewer::onKeyUp(SDL_keysym sym)
{
    if(s_gui.onKeyUp(sym))
        return;

    s_input_handler.onKeyUp(sym.sym);
}

//------------------------------------------------------------------------------
void ParticleViewer::onMouseButtonDown(const SDL_MouseButtonEvent & event)
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
void ParticleViewer::onMouseButtonUp(const SDL_MouseButtonEvent & event)
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
void ParticleViewer::onMouseMotion(const SDL_MouseMotionEvent & event)
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
void ParticleViewer::onResizeEvent(unsigned width, unsigned height)
{
    s_log << Log::debug('i')
          << "Resize to " << width << "x" << height << "\n";
    
    s_scene_manager.setWindowSize(width, height);
}

//------------------------------------------------------------------------------
/**
 *  Called when an effect has expired. Immediately reloads new effect.
 */
void ParticleViewer::objectDeleted(void * obj)
{
    s_scheduler.addEvent(SingleEventCallback(this, &ParticleViewer::restartEffect),
                         0.0f,
                         obj,
                         "ParticleViewer::restartEffect",
                         &fp_group_);
}



//------------------------------------------------------------------------------
std::vector<std::string> ParticleViewer::loadEffectsCompletionFun(const std::vector<std::string>& args) const
{
    
    using namespace boost::filesystem;

    std::vector<std::string> ret;
    if (args.size() > 1) return ret;

    path effect_path(EFFECTS_PATH);
    for (directory_iterator it(effect_path);
         it != directory_iterator();
         ++it)
    {
        std::string name = it->path().leaf();
        std::size_t p = name.rfind(".xml");
        if (p != name.length()-4) continue;
        
        ret.push_back(name.substr(0, p));
    }

    if (args.size()==1) Console::filterPrefix(args[0], ret);
    
    return ret;
}


//------------------------------------------------------------------------------
void ParticleViewer::restartEffect(void* eff)
{
    try
    {
        s_texturemanager.  unloadAllResources();
        s_particle_manager.unloadAllResources();
        s_effect_manager.  unloadAllResources();
    
        std::vector<osg::Group*>::iterator it = find(effects_.begin(), effects_.end(), eff);
        assert(it != effects_.end());

        unsigned ind = it - effects_.begin();
        
        osg::ref_ptr<ParticleEffectNode> new_effect = s_effect_manager.createEffect(effects_list_[ind], NULL).first;
        *it = new_effect.get();
        new_effect->addObserver(this);
        s_scene_manager.addNode(new_effect.get());
    } catch (Exception &e )
    {
        s_log << "Failed to reload effect: "
              << e
              << "\n";

        effects_list_.clear();
        effects_.clear();
    }    
}


//------------------------------------------------------------------------------
void ParticleViewer::loadEffect(const std::string & name)
{
    // check if effect is already loaded
    std::vector<std::string>::const_iterator it;
    for(it = effects_list_.begin(); it != effects_list_.end(); it++)
    {
        if((*it) == name)
        {
            throw Exception("Effect: " + name + " already loaded.");
        }
    }


    osg::ref_ptr<ParticleEffectNode> effect_node = s_effect_manager.createEffect(name, NULL).first;
    assert(effect_node.get());

    s_scene_manager.addNode(effect_node.get());

    effect_node->addObserver(this);
    effects_.push_back(effect_node.get());
    effects_list_.push_back(name);

    if(gui_console_->isActive()) gui_console_->toggleShow(); // comfort func
    s_app.captureMouse(true);   // comfort func
}


//------------------------------------------------------------------------------
void ParticleViewer::registerInputHandlerFunctions()
{
    s_input_handler.registerInputCallback("Pickup/Drop Beacon",
                                          input_handler::ContinuousInputHandler(this, &ParticleViewer::up),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Drop Mine",
                                          input_handler::ContinuousInputHandler(this, &ParticleViewer::down),
                                          &fp_group_);

    s_input_handler.registerInputCallback("Accelerate",
                                          input_handler::ContinuousInputHandler(this, &ParticleViewer::forward),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Decelerate",
                                          input_handler::ContinuousInputHandler(this, &ParticleViewer::backward),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Turn left",
                                          input_handler::ContinuousInputHandler(this, &ParticleViewer::left),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Turn right",
                                          input_handler::ContinuousInputHandler(this, &ParticleViewer::right),
                                          &fp_group_);

    s_input_handler.registerInputCallback("view",
                                          input_handler::MouseInputHandler(this, &ParticleViewer::handleMouseMotion),
                                          &fp_group_);

    s_input_handler.registerInputCallback("refresh",
                                          input_handler::SingleInputHandler(this, &ParticleViewer::reloadEffects),
                                          &fp_group_);

    s_input_handler.registerInputCallback("loadCameraOrientation",
                                          input_handler::SingleInputHandler(this, &ParticleViewer::loadCameraOrientation),
                                          &fp_group_);
    s_input_handler.registerInputCallback("Take Screenshot",
                                          input_handler::SingleInputHandler(this, &ParticleViewer::takeScreenshot),
                                          &fp_group_);

    s_input_handler.registerInputCallback("Toggle Coord Axes",
                                          input_handler::SingleInputHandler(this, &ParticleViewer::toggleCoordAxes),
                                          &fp_group_);


    // Now all input handling functions should have been
    // registered, load & apply keymap
    s_input_handler.loadKeymap  (getUserConfigFile(), DEFAULT_KEYMAP_SUPERSECTION);
    s_input_handler.loadKeymap  (getUserConfigFile(), CONFIGURABLE_KEYMAP_SUPERSECTION);
    s_input_handler.loadKeymap  (VIEWER_CONFIG_FILE, VIEWER_KEYMAP_SUPERSECTION);
}

//------------------------------------------------------------------------------
void ParticleViewer::loadParameterFiles()
{
    s_log << Log::debug('i') << "Loading Parameter files\n";
}


//------------------------------------------------------------------------------
void ParticleViewer::up     (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.y_ = s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.y_ = 0;
}

//------------------------------------------------------------------------------
void ParticleViewer::down   (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.y_ = -s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.y_ = 0;
}

//------------------------------------------------------------------------------
void ParticleViewer::forward     (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.z_ = -s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.z_ = 0;
}

//------------------------------------------------------------------------------
void ParticleViewer::backward   (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.z_ = s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.z_ = 0;
}

//------------------------------------------------------------------------------
void ParticleViewer::right  (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.x_ = s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.x_ = 0;
}

//------------------------------------------------------------------------------
void ParticleViewer::left   (bool b)
{
    Vector & cur_camera_v = camera_.getLocalVelocity();
    if(b)
        cur_camera_v.x_ = -s_params.get<float>("camera.move_speed");
    else
        cur_camera_v.x_ = 0;
}


//------------------------------------------------------------------------------
void ParticleViewer::handleMouseMotion(Vector2d pos, Vector2d delta)
{
    float rotate_speed = s_params.get<float>("camera.rotate_speed");
    camera_.changeOrientation( rotate_speed * delta.x_,
                               -rotate_speed * delta.y_);
}

//------------------------------------------------------------------------------
void ParticleViewer::takeScreenshot()
{
    saveScreenshot("screenshots/");    
}


//------------------------------------------------------------------------------
std::string ParticleViewer::addEffect(const std::vector<std::string> & args)
{
    std::string effect = "";

    if(!args.empty() && !args[0].empty())
    {
        loadEffect(args[0]);
        return std::string("Successfully loaded: ") + args[0];
    } else return "invalid arguments.";
}

//------------------------------------------------------------------------------
std::string ParticleViewer::removeEffect(const std::vector<std::string> & args)
{
    std::string msg = "";
    std::string effect = "";

    if(!args.empty() && !args[0].empty())
    {
        effect = args[0];

        std::vector<std::string>::iterator it;
        for(it = effects_list_.begin(); it != effects_list_.end(); it++)
        {
            if((*it) == effect)
            {
                effects_list_.erase(it);
                msg = "Removed: " + effect;
                reloadEffects();  // actually clear and reset with remaining effects
                break;
            }
        }
    }
    else
    {
        msg = "invalid arguments.";
    }

    return msg;
}


//------------------------------------------------------------------------------
/***
 *  clearing and reloading particle and texture resources,
 *  changes in particle effect description files take effect
 **/
void ParticleViewer::reloadEffects()
{
    for (unsigned i=0; i<effects_.size(); ++i)
    {
        effects_[i]->removeObserver(this);
        
        DeleteNodeVisitor dn(effects_[i]);
        s_scene_manager.getRootNode()->accept(dn);
    }

    s_texturemanager.  unloadAllResources();
    s_particle_manager.unloadAllResources();
    s_effect_manager.  unloadAllResources();
    
    effects_.clear();
    std::vector<std::string> cur_effects;
    cur_effects.swap(effects_list_);
    
    std::vector<std::string>::const_iterator it;
    for(it = cur_effects.begin(); it != cur_effects.end(); it++)
    {
        loadEffect(*it);
    }
}

//------------------------------------------------------------------------------
void ParticleViewer::listEffectsLoaded()
{
    std::string list_effects = "";

        std::vector<std::string>::iterator it;
        for(it = effects_list_.begin(); it != effects_list_.end(); it++)
        {
            list_effects += (*it) + "\n";
        }

    s_console.print(list_effects);
}

//------------------------------------------------------------------------------
void ParticleViewer::saveCameraOrientation() const
{
    s_params.set<Matrix>("viewer.prefs.default_cam", camera_);
    s_params.saveParameters(VIEWER_CONFIG_FILE);
}

//------------------------------------------------------------------------------
void ParticleViewer::loadCameraOrientation()
{
    camera_.setTransform(s_params.get<Matrix>("viewer.prefs.default_cam"));
}

//------------------------------------------------------------------------------
void ParticleViewer::toggleCoordAxes()
{
    if(coord_axes_->getNodeMask() != NODE_MASK_INVISIBLE)
    {
        coord_axes_->setNodeMask(NODE_MASK_INVISIBLE);
    }
    else
    {
        coord_axes_->setNodeMask(NODE_MASK_VISIBLE);
    }
}
