

#ifndef TANK_SERVER_APP_INCLUDED
#define TANK_SERVER_APP_INCLUDED

#ifndef DEDICATED_SERVER
#include <osg/Node>
#include <osgDB/ReadFile>


#include "SceneManager.h"
#endif


#include "MetaTask.h"

#include "RegisteredFpGroup.h"
#include "Entity.h"

class GUIConsole;
class GUIProfiler;
class NetworkServer;

//------------------------------------------------------------------------------
class TankAppServer : public MetaTask
{
public:
    TankAppServer(bool create_visuals); // MMMM
    virtual ~TankAppServer();

    virtual void render();
    
    virtual void onKeyDown(SDL_keysym sym);
    virtual void onKeyUp(SDL_keysym sym);
    virtual void onMouseButtonDown(const SDL_MouseButtonEvent & event);
    virtual void onMouseButtonUp(const SDL_MouseButtonEvent & event);
    virtual void onMouseMotion(const SDL_MouseMotionEvent & event);
    virtual void onResizeEvent(unsigned width, unsigned height);

 protected:

    void toggleProfiler();

    void loadParameterFiles();
    
    std::auto_ptr<NetworkServer> server_;
    
    std::auto_ptr<GUIConsole> gui_console_;
    std::auto_ptr<GUIProfiler> gui_profiler_;

    RegisteredFpGroup fp_group_;

    Entity camera_pos_;
};



#endif
