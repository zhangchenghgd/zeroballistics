
#include "SdlApp.h"

#include <SDL/SDL.h>


#include <GL/glu.h>

#ifdef ENABLE_DEV_FEATURES
#include <GL/glut.h>
#endif

#include "Profiler.h"
#include "Exception.h"
#include "Log.h"
#include "Utils.h"
#include "Gui.h"
#include "MetaTask.h"
#include "SceneManager.h"
#include "UserPreferences.h"


//------------------------------------------------------------------------------
SdlApp::~SdlApp()
{
    s_log << Log::debug('d') << "SDL App destructor\n";
    SDL_Quit();
}

//------------------------------------------------------------------------------
/**
 *  Initializes SDL and GLEW.
 */
void SdlApp::init(int & argc, char ** argv,
                  const std::string & caption,
                  const std::string & application_section)
{
    // store cmd line args for use in application
    // skip program name i=1
    for (int i=1; i < argc; i++) 
    {
        command_line_arguments_.push_back(argv[i]);
    }

    application_section_ = application_section;
    
    width_      = s_params.get<unsigned>(application_section_ + ".app.initial_window_width");
    height_     = s_params.get<unsigned>(application_section_ + ".app.initial_window_height");
    min_fps_    = s_params.getPointer<float>(application_section_ + ".app.min_fps");
    target_fps_ = s_params.getPointer<float>(application_section_ + ".app.target_fps");
    
    getCurTime(last_time_);

    
    enableFloatingPointExceptions();

    if( SDL_Init(SDL_INIT_VIDEO) < 0 ) sdlError("Could not initialize SDL");
    else                               s_log << Log::debug('i') << "SDL initialization suceeded\n";
    
    SDL_WM_SetCaption(caption.c_str(), caption.c_str());

    SDL_Surface * window_icon = NULL; // XXX todo free surface
    window_icon = SDL_LoadBMP("data/window_icon.bmp");    
    if(window_icon) SDL_WM_SetIcon(window_icon, NULL);

    try
    {
        /// XXXX this actually is app specfic not graphics. move to param .app section?? 
        /// caution: server, client, viewer -> options
        unsigned fsaa_samples = s_params.get<unsigned>("client.graphics.fsaa_samples");
        setGlAttributes(fsaa_samples);

        createSdlSurface();
    } catch (Exception & e)
    {
        s_log << Log::warning
              << e << "\n";
        try
        {
            s_log << Log::warning
                  << SDL_GetError()
                  << "\n";
            s_log << Log::warning
                  << "Retrying with FSAA disabled...";
            setGlAttributes(0);
            createSdlSurface();
            s_log << "OK\n";
        }
        catch (Exception & e)
        {
            s_log << Log::error << "\nFailed with FSAA disabled...\n";
            s_log << " SDL error msg: " << SDL_GetError() << "\n";
            throw e;
        }
    }
    
    SDL_EnableUNICODE(1);
    SDL_ShowCursor(SDL_DISABLE);

    printGlInfo();
    s_scene_manager.checkCapabilities(true);

    enableFloatingPointExceptions(false);
    s_gui.init(width_,height_);
    enableFloatingPointExceptions();

#ifdef ENABLE_DEV_FEATURES    
    glutInit(&argc, argv);
#endif

    s_scheduler.setRenderCallback(RenderCallback(this, &SdlApp::renderCallback));
}



//------------------------------------------------------------------------------
void SdlApp::run(MetaTask * initial_task)
{
    initial_task->focus();
    
    SDL_Event event;
    
    while(!quit_)
    {
#ifdef _DEBUG
        checkForGlError();
#endif
        
        TimeValue cur_time;
        getCurTime(cur_time);
        float dt = getTimeDiff(cur_time, last_time_) * 0.001f;
        // due to timer resolution
        if(dt == 0.0f) continue;        
        last_time_ = cur_time;

        float fps = 1.0f / dt;
        ADD_LOCAL_CONSOLE_VAR(float, fps);
        
        // Cap dt to avoid accumulation
        ADD_STATIC_CONSOLE_VAR(bool, capping_dt, false);         
        if (dt > 1.0f / *min_fps_)
        {
            dt = 1.0f / *min_fps_;
            capping_dt = true;
        } else capping_dt = false;
    

        if (s_params.get<bool>(application_section_ + ".app.total_profile"))
        {
            if (capping_dt)
            {
                s_log << s_profiler.getSummary();
            } else
            {
                s_profiler.clearAll();
            }
        } else
        {
            s_profiler.frameMove();
        }

        s_scheduler.frameMove(dt);
        s_gui.frameMove(dt);

        
        while(SDL_PollEvent(&event))
        {
            switch( event.type ) 
            {
                
            case SDL_QUIT:
                quit_ = true;
                break;

            case SDL_ACTIVEEVENT:
                if (!event.active.gain)
                {
                    captureMouse(false, true);
                }
                break;
            case SDL_VIDEORESIZE:
                width_  = event.resize.w;
                height_ = event.resize.h;


                //// XXXXXX hack to make resize window on win32 machines possible
#ifdef _WIN32
                sdl_surf_ = SDL_GetVideoSurface();
                sdl_surf_->w = width_;
                sdl_surf_->h = height_;            
#else
                // old method:
                createSdlSurface();
#endif
                glViewport(0,0,width_, height_);

                s_gui.onResizeEvent(width_, height_);
              
                focused_meta_task_->onResizeEvent(width_, height_);

                break;
            case SDL_KEYDOWN:
                focused_meta_task_->onKeyDown(event.key.keysym);
                break;
            case SDL_KEYUP:
                focused_meta_task_->onKeyUp(event.key.keysym);
                break;              
            case SDL_MOUSEMOTION:
                focused_meta_task_->onMouseMotion(event.motion);
                break;
            case SDL_MOUSEBUTTONDOWN:
                focused_meta_task_->onMouseButtonDown(event.button);
                break;
            case SDL_MOUSEBUTTONUP:
                focused_meta_task_->onMouseButtonUp(event.button);
                break;

                
            }
        }

        if (*target_fps_ != 0)
        {
            getCurTime(cur_time);
            float sleep_time = 1.0f / *target_fps_ - getTimeDiff(cur_time, last_time_) * 0.001f;

            ADD_LOCAL_CONSOLE_VAR(float, sleep_time);
            
            if (sleep_time > 0.0f) SDL_Delay((unsigned)(sleep_time*1000.0f));
            else s_log << Log::debug('t') << "Frame time exceeded by " << -1000.0f*sleep_time << " ms\n";
        }        
    }

    s_log << Log::debug('d') << "After main loop\n";
}


//------------------------------------------------------------------------------
void SdlApp::printGlInfo() const
{
    std::string gl_info; 

    char * vendor     = (char*)glGetString(GL_VENDOR);
    char * renderer   = (char*)glGetString(GL_RENDERER);
    char * version    = (char*)glGetString(GL_VERSION);

    if (!vendor || !renderer || !version)
    {
        gl_info = "Couldn't determine OpenGL info.";
        s_log << gl_info << "\n";
        return;
    }
    
    gl_info += (gl_info +
                "Vendor: "                 + vendor     + "\n" +
                "Renderer: "               + renderer   + "\n" +
                "GL Version: "             + version    + "\n");

    int max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);

    int max_varying = 0;;
    glGetIntegerv(GL_MAX_VARYING_FLOATS, &max_varying);

    int max_vertex_uniforms = 0, max_fragment_uniforms = 0;
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,   &max_vertex_uniforms);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &max_fragment_uniforms);

    int max_vertex_attribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);

    gl_info += "Max. Texture Units: "  + toString(max_texture_units) + "\n";
    gl_info += "Max. Varying Floats: " + toString(max_varying) + "\n";
    gl_info += "Max. Vertex Attribs: " + toString(max_vertex_attribs) + "\n";
    gl_info += "Max. Uniforms in VS: " + toString(max_vertex_uniforms) + "\n";
    gl_info += "Max. Uniforms in FS: " + toString(max_fragment_uniforms) + "\n";
    
    gl_info += std::string("GLSL language version: ");
    char * glsl_ver = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
    gl_info += (glsl_ver ? std::string(glsl_ver) : std::string("n.a."))  + "\n";
    
    gl_info += std::string("Shader Model: ") + toString(s_scene_manager.getShaderModel()) + "\n";

    s_log << gl_info << "\n";
}

//------------------------------------------------------------------------------
void SdlApp::printGlExtensions() const
{
    std::string gl_extensions; 

    char * extensions = (char*)glGetString(GL_EXTENSIONS);

    if (!extensions)
    {
        gl_extensions = "Couldn't determine OpenGL Extensions.";
        s_log << gl_extensions << "\n";
        return;
    }

    gl_extensions = "Available extensions: \n";

    // format extensions
    Tokenizer tok(extensions, ' ');
    while (!tok.isEmpty())
    {
        gl_extensions = (gl_extensions + tok.getNextWord() + "\n");
    }

    s_log << gl_extensions << "\n";
}

//------------------------------------------------------------------------------
void SdlApp::quit()
{
    quit_ = true;
}

//------------------------------------------------------------------------------
bool SdlApp::isMouseCaptured() const
{
    return capture_mouse_;
}

//------------------------------------------------------------------------------
/**
 *  If certain gui windows are open and user input is needed or mouse capturing
 *  should be prevented for user convenience, mouse capturing can be prevented 
 *  here
 **/
void SdlApp::setAllowMouseCapturing(bool allow)
{
    allow_capture_mouse_ = allow;
}

//------------------------------------------------------------------------------
void SdlApp::captureMouse(bool capture, bool window_lost_focus)
{
    // allow uncapturing all the time, but only
    // allow capturing of the mouse if allow flag is set
    if(!capture || allow_capture_mouse_)
    {
        capture_mouse_ = capture;
        s_gui.setMouseCursorVisible(!capture);
    }
    
    // if mouse gets uncaptured move it to last known CEGUI position 
    // for user convenience. Ignore this if mouse leaves window event is
    // triggered otherwise mouse cannot leave window
    if(!capture && !window_lost_focus)
    {
        SDL_WarpMouse((Uint16)s_gui.getMouseCursorPostion().x_,
                      (Uint16)s_gui.getMouseCursorPostion().y_);
    }
}

//------------------------------------------------------------------------------
unsigned SdlApp::getScreenWidth() const
{
    return width_;
}
    
//------------------------------------------------------------------------------
unsigned SdlApp::getScreenHeight() const
{
    return height_;
}


//------------------------------------------------------------------------------
void SdlApp::setFocusedTask(MetaTask * task)
{
    focused_meta_task_ = task;
}


//------------------------------------------------------------------------------
MetaTask * SdlApp::getFocusedTask() const
{
    return focused_meta_task_;
}



//------------------------------------------------------------------------------
void SdlApp::toggleFullScreen()
{
#ifdef _WIN32  
    s_log << Log::warning
          << "Switch to fullscreen mode not available on Win32.\n";
#else
    if (!SDL_WM_ToggleFullScreen(sdl_surf_))
    {
        s_log << Log::error
              << "Failed to switch to fullscreen mode.\n";
    }
#endif    
}

//------------------------------------------------------------------------------
void SdlApp::enableKeyRepeat(bool b)
{
    if (b)
    {
        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
                            SDL_DEFAULT_REPEAT_INTERVAL);
    } else
    {
        SDL_EnableKeyRepeat(0,0);
    }
}


//------------------------------------------------------------------------------
const std::vector<std::string> & SdlApp::getCommandLineArguments() const
{
    return command_line_arguments_;
}

//------------------------------------------------------------------------------
SdlApp::SdlApp() :
    sdl_surf_(NULL), quit_(false),
    capture_mouse_(false),
    allow_capture_mouse_(true),
    width_(800),
    height_(600),
    min_fps_(NULL),
    target_fps_(NULL),
    focused_meta_task_(NULL)
{
    srand(time(NULL));
    
    s_console.addFunction("printGlInfo",
                          Loki::Functor<void>       (this, &SdlApp::printGlInfo),
                          &fp_group_);
    s_console.addFunction("printGlExtensions",
                          Loki::Functor<void>       (this, &SdlApp::printGlExtensions),
                          &fp_group_);
    s_console.addFunction("quit",
                          Loki::Functor<void>       (this, &SdlApp::quit),
                          &fp_group_);
}


//------------------------------------------------------------------------------
void SdlApp::renderCallback()
{
    focused_meta_task_->render();
}



//------------------------------------------------------------------------------
void SdlApp::createSdlSurface()
{
    unsigned surface_flags = SDL_OPENGL;
    const int bpp = 32;

    if (s_params.get<bool>(application_section_ + ".app.fullscreen"))
    {
        surface_flags |= SDL_FULLSCREEN;
    } else
    {
        surface_flags |= SDL_RESIZABLE;
    }

    sdl_surf_ = SDL_SetVideoMode(width_, height_, bpp, surface_flags );
    if (!sdl_surf_)
    {
        width_  = 800;
        height_ = 600;
        sdl_surf_ = SDL_SetVideoMode(width_, height_, bpp, surface_flags );

        if (!sdl_surf_)
        {
            sdlError("Unable to switch video mode");
        } else
        {
            s_log << Log::error
                  << "Resolution not supported, falling back to 800x600."
                  << "\n";
            
            s_params.set(application_section_ + ".app.initial_window_width",  (unsigned)width_);
            s_params.set(application_section_ + ".app.initial_window_height", (unsigned)height_);
            s_params.saveParameters(getUserConfigFile(), CONFIG_SUPERSECTION);
        }
    }


    
    if ((sdl_surf_->flags & surface_flags) != surface_flags)
    {
        s_log << Log::warning << "Couldn't get requested surface flags. Requested: "
              << (void*)surface_flags
              << ". Got: " << (void*)sdl_surf_->flags << ".\n";
    }

    // check bpp to be 32 also in windowed mode
    int red;
    int res = SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &red);

    if(res != 0 || red < 8 || (sdl_surf_->format->BitsPerPixel < (Uint8)bpp))
    {
        sdlError("32 bpp color depth required.");
    }
}

//------------------------------------------------------------------------------
void SdlApp::setGlAttributes(unsigned fsaa_samples)
{
    if (SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1) < 0)    sdlError("Could not setup stencil buffer");
    if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true) < 0) sdlError("Could not enable doublebuffering");

    if (fsaa_samples)
    {
        if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, true)         < 0) sdlError("Could not enable AntiAliasing");
        if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, fsaa_samples) < 0) sdlError("Could not enable AntiAliasing");
    } else
    {
        if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, false)        < 0) sdlError("Could not enable AntiAliasing");
        if (SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0)            < 0) sdlError("Could not enable AntiAliasing");
    }    

// Linux: Don't do this here or no glx visual will be found...        
// Win32: Also do not do this on ATI cards since catalyst 8.1, causes graphics HW not to be found
//        if (SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 0)          < 0) sdlError("Could not set HW acceleration");
}

//------------------------------------------------------------------------------
void SdlApp::sdlError(const std::string & msg) const
{
    Exception e(msg);
    char * sdl_err_msg = SDL_GetError();
    if(sdl_err_msg && strlen(sdl_err_msg)) e << " : " << sdl_err_msg;
    throw e;
}

//------------------------------------------------------------------------------
void SdlApp::checkForGlError() const
{
    GLenum error = glGetError();
    
    if (error != GL_NO_ERROR)
    {
        s_log << Log::error
              << "OpenGL error: "
              << gluErrorString(error)
              << "\n";
    }
}

