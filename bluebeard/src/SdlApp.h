
#ifndef TANK_SDL_APP_INCLUDED
#define TANK_SDL_APP_INCLUDED

#include <string>

#include <SDL/SDL_keysym.h>
#include <SDL/SDL_events.h>


#include "TimeStructs.h"
#include "ParameterManager.h"
#include "Singleton.h"
#include "RegisteredFpGroup.h"

struct SDL_Surface;

class MetaTask;


#define s_app Loki::SingletonHolder<SdlApp, Loki::CreateUsingNew, SingletonSdlAppLifetime >::Instance()
//------------------------------------------------------------------------------
class SdlApp
{
    DECLARE_SINGLETON(SdlApp);
 public:
    virtual ~SdlApp();

    void init(int & argc, char ** argv,
              const std::string & caption,
              const std::string & application_section);

    void run(MetaTask * initial_task);
    
    void printGlInfo() const;
    void printGlExtensions() const;
    void quit();

    bool isMouseCaptured() const;
    void setAllowMouseCapturing(bool allow);
    void captureMouse(bool capture, bool window_lost_focus = false);

    unsigned getScreenWidth() const;
    unsigned getScreenHeight() const;

    void setFocusedTask(MetaTask * task);
    MetaTask * getFocusedTask() const;

    void toggleFullScreen();

    const std::vector<std::string> & getCommandLineArguments() const;
 protected:
    void renderCallback();
    
    
    void createSdlSurface();

    void setGlAttributes(unsigned fsaa_samples);
    
    void sdlError(const std::string & msg) const;

    void checkForGlError() const;
    
    
    SDL_Surface * sdl_surf_;
    bool quit_;

    TimeValue last_time_;

    bool capture_mouse_;
    bool allow_capture_mouse_;

    int width_;
    int height_;

    float * min_fps_;
    float * target_fps_;

    MetaTask * focused_meta_task_;

    std::string application_section_;

    RegisteredFpGroup fp_group_;

    std::vector<std::string> command_line_arguments_;
};


#endif // #ifndef TANK_SDL_APP_INCLUDED
