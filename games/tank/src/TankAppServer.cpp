

#include "TankAppServer.h"

#include <osgDB/ReadFile>

#include "NetworkServer.h"

#include "physics/OdeSimulator.h"

#include "PuppetMasterServer.h"

#include "NetworkCommand.h" // for logAccountingInfo

#include "VariableWatcher.h"

#include "Profiler.h"

#include "Gui.h"
#include "GUIProfiler.h"
#include "GUIConsole.h"

#include "GameLogicServer.h" // XXX only used for rendering game logic

#include "VariableWatcherVisual.h"
#include "SdlApp.h"


const float FREE_CAM_MOVING_SPEED = 6; // PPPP


//------------------------------------------------------------------------------
TankAppServer::TankAppServer(bool create_visuals) : // MMMM
    MetaTask("TankAppServer")
{
    GameLogicServer::setCreateVisuals(create_visuals);    
    
    loadParameterFiles();

    gui_console_.reset(new GUIConsole(root_window_));
    gui_profiler_.reset(new GUIProfiler(root_window_));

#ifdef ENABLE_DEV_FEATURES    
    s_console.addFunction("toggleProfiler",
                          Loki::Functor<void> (this, &TankAppServer::toggleProfiler),
                          &fp_group_);
    s_console.addFunction("toggleWireframeMode",
                          Loki::Functor<void> (&s_scene_manager, &SceneManager::toggleWireframeMode),
                          &fp_group_);
#endif    

    if (create_visuals) s_scene_manager.init(); // MMMM



    server_.reset(new NetworkServer());

    
    server_->start();

    if (create_visuals) new VariableWatcherVisual(); // MMMM

    s_scheduler.addFrameTask(PeriodicTaskCallback(&camera_pos_, &Entity::frameMove),
                             "camera_pos_::frameMove",
                             &fp_group_);
}

//------------------------------------------------------------------------------
TankAppServer::~TankAppServer()
{      
    server_.reset(NULL);
    
    s_scene_manager.reset();
}

//------------------------------------------------------------------------------
void TankAppServer::render()
{    
    PROFILE(TankAppServer::render);
    ADD_STATIC_CONSOLE_VAR(bool, render_shapes, false);

    s_scene_manager.getCamera().setTransform(camera_pos_);
    
    if (render_shapes)
    {
        s_scene_manager.getCamera().applyTransform();
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_LIGHT1);
        float diffuse[] = {0.4, 0.4, 0.4, 1.0};
        glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse);
        float ambient[] = {0.2f, 0.2f, 0.2f, 1.0f};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        server_->getPuppetMaster()->getGameState()->getSimulator()->renderGeoms();

        glPopAttrib();
    } else
    {
       s_scene_manager.render(); 
    }

    s_gui.render();
    
    {
        PROFILE(SDL_GL_SwapBuffers);
        SDL_GL_SwapBuffers();
    }
}


//------------------------------------------------------------------------------
void TankAppServer::onKeyDown(SDL_keysym sym)
{
    if(!s_app.isMouseCaptured())
    {
        if(s_gui.onKeyDown(sym))
            return;
    }

    Vector & cur_camera_v = camera_pos_.getLocalVelocity();
    switch(sym.sym)
    {
    case SDLK_CARET:
    case SDLK_BACKQUOTE:
        if(!gui_console_->isActive())
        {
            s_app.captureMouse(false);
        }
        gui_console_->toggleShow();
        break;    
    case SDLK_a:
        cur_camera_v.x_ = -FREE_CAM_MOVING_SPEED;
        break;
    case SDLK_d:
        cur_camera_v.x_ = FREE_CAM_MOVING_SPEED;
        break;
    case SDLK_w:
        cur_camera_v.z_ = -FREE_CAM_MOVING_SPEED;
        break;
    case SDLK_s:
        cur_camera_v.z_ = FREE_CAM_MOVING_SPEED;
        break;
        
    case SDLK_LCTRL:
        cur_camera_v.y_ = -FREE_CAM_MOVING_SPEED;
        break;

        
    case SDLK_q:
        s_app.quit();
        break;
        
    default:
        break;
    }
}

//------------------------------------------------------------------------------
void TankAppServer::onKeyUp(SDL_keysym sym)
{
    if(s_gui.onKeyUp(sym))
        return;


    Vector & cur_camera_v = camera_pos_.getLocalVelocity();
    switch(sym.sym)
    {
    case SDLK_a:
    case SDLK_d:
        cur_camera_v.x_ = 0.0f;
        break;

    case SDLK_w:
    case SDLK_s:
        cur_camera_v.z_ = 0.0f;
        break;        

    case SDLK_LCTRL:
        cur_camera_v.y_ = 0.0f;
        break;
        
    default:
        break;
    }    
}

//------------------------------------------------------------------------------
void TankAppServer::onMouseButtonDown(const SDL_MouseButtonEvent & event)
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
        break;
    case SDL_BUTTON_MIDDLE:
        camera_pos_.getLocalVelocity().y_ = FREE_CAM_MOVING_SPEED;
        break;
    case SDL_BUTTON_RIGHT:
        s_app.captureMouse(false);
        break;
    }
}

//------------------------------------------------------------------------------
void TankAppServer::onMouseButtonUp(const SDL_MouseButtonEvent & event)
{
    if(!s_app.isMouseCaptured())
    {
        if(s_gui.onMouseButtonUp(event))
            return;
    }
    
    switch (event.button)
    {
    case SDL_BUTTON_MIDDLE:
        camera_pos_.getLocalVelocity().y_ = 0.0f;
        break;
    case SDL_BUTTON_RIGHT:
        break;
    }
}


//------------------------------------------------------------------------------
void TankAppServer::onMouseMotion(const SDL_MouseMotionEvent & event)
{
    if(!s_app.isMouseCaptured())
    {
        s_gui.onMouseMotion(event);
        return;
    }
    
    // Bail on WarpMouse-induced events
    if (event.x == (s_app.getScreenWidth() >>1) &&
        event.y == (s_app.getScreenHeight()>>1)) return;


    // Check mouse invert
    SDL_MouseMotionEvent tmp_event = event;
    if (!s_params.get<bool>("camera.invert_mouse")) tmp_event.yrel = -event.yrel;
    
    float rotate_speed = s_params.get<float>("camera.rotate_speed");
    camera_pos_.changeOrientation( rotate_speed * tmp_event.xrel,
                                   -rotate_speed * tmp_event.yrel);

    SDL_WarpMouse(s_app.getScreenWidth()  >> 1,
                  s_app.getScreenHeight() >> 1); 
}

//------------------------------------------------------------------------------
void TankAppServer::onResizeEvent(unsigned width, unsigned height)
{
    s_log << Log::debug('i')
          << "Resize to " << width << "x" << height << "\n";

    s_scene_manager.setWindowSize(width, height);
}

//------------------------------------------------------------------------------
void TankAppServer::toggleProfiler()
{
    gui_profiler_->toggle();
}

//------------------------------------------------------------------------------
void TankAppServer::loadParameterFiles()
{
    s_log << Log::debug('i') << "Loading Parameter files\n";

    s_params.loadParameters("config_server.xml");
    s_params.loadParameters("config_common.xml");

    s_params.loadParameters("data/config/teams.xml");
    s_params.loadParameters("data/config/weapon_systems.xml");
    s_params.loadParameters("data/config/tanks.xml");
    s_params.loadParameters("data/config/upgrade_system.xml");
    s_params.loadParameters("data/config/upgrades.xml");


    /// XXX still used on server for console, profiler
    s_params.loadParameters("data/config/hud.xml");
    s_params.loadParameters("data/config/hud_tank.xml");

    ///XXX still needs client for scene_manager stuff like shadows ...
    s_params.loadParameters("config_client.xml");

}

