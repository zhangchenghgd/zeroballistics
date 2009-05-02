#ifndef BLUEBEARD_META_TASK_INCLUDED
#define BLUEBEARD_META_TASK_INCLUDED


#include <string>

#include <SDL/SDL_events.h>
#include <SDL/SDL_keysym.h>



#include "RegisteredFpGroup.h"

namespace CEGUI
{
    class Window;
}

class SdlTestApp;

//------------------------------------------------------------------------------
class MetaTask
{
    
 public:
    MetaTask(const std::string & name);
    virtual ~MetaTask();

    const std::string & getName() const;
    
    void focus();

    virtual void onFocusGained() {};
    virtual void onFocusLost()   {};
    
    virtual void render();
    virtual void onKeyDown(SDL_keysym sym);
    virtual void onKeyUp(SDL_keysym sym);
    virtual void onMouseMotion(const SDL_MouseMotionEvent & event);
    virtual void onMouseButtonDown(const SDL_MouseButtonEvent & event);
    virtual void onMouseButtonUp(const SDL_MouseButtonEvent & event);
    virtual void onResizeEvent(unsigned width, unsigned height);

    RegisteredFpGroup * getFpGroup();
    
 protected:

    std::string name_;
    
    CEGUI::Window * root_window_;
    
    RegisteredFpGroup fp_group_;
};



#endif
