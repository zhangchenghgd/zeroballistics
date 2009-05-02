
#include "MetaTask.h"

#ifdef _WIN32
#include "Datatypes.h" // needed for gl.h
#endif

#include <CEGUI/CEGUI.h>

#include <GL/gl.h>

#include "SdlApp.h"
#include "Gui.h"
#include "Log.h"


//------------------------------------------------------------------------------
MetaTask::MetaTask(const std::string & name) :
    name_(name)
{
    s_log << Log::debug('i')
          << "MetaTask \""
          << name_
          << "\" constructor\n";
    
    enableFloatingPointExceptions(false);
    root_window_ = CEGUI::WindowManager::getSingleton().createWindow(
                           "DefaultWindow", name + "_root/");
    enableFloatingPointExceptions();
    
}


//------------------------------------------------------------------------------
MetaTask::~MetaTask()
{
    s_log << Log::debug('d')
          << "MetaTask \""
          << name_
          << "\" destructor\n";
    
    CEGUI::WindowManager::getSingleton().destroyWindow(root_window_);
    CEGUI::WindowManager::getSingleton().cleanDeadPool();
}

//------------------------------------------------------------------------------
const std::string & MetaTask::getName() const
{
    return name_;
}

//------------------------------------------------------------------------------
void MetaTask::focus()
{
    if (s_app.getFocusedTask())
    {
        s_log << Log::debug('M')
              << "Switching MetaTask: "
              << s_app.getFocusedTask()->getName()
              << " -> "
              << name_
              << "\n";

        s_app.getFocusedTask()->onFocusLost();
    } else
    {
        s_log << Log::debug('M')
              << "Focusing initial task: "
              << name_
              << "\n";
    }
    
    s_app.setFocusedTask(this);

    enableFloatingPointExceptions(false);
    CEGUI::System::getSingleton().setGUISheet(root_window_);
    enableFloatingPointExceptions();

    onResizeEvent(s_app.getScreenWidth(),
                  s_app.getScreenHeight());

    onFocusGained();
}


//------------------------------------------------------------------------------
void MetaTask::render()
{
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );
    s_gui.render();
    SDL_GL_SwapBuffers();
}


//------------------------------------------------------------------------------
void MetaTask::onKeyDown(SDL_keysym sym)
{
    s_gui.onKeyDown(sym);
}


//------------------------------------------------------------------------------
void MetaTask::onKeyUp(SDL_keysym sym)
{
    s_gui.onKeyUp(sym);
}


//------------------------------------------------------------------------------
void MetaTask::onMouseMotion(const SDL_MouseMotionEvent & event)
{
    s_gui.onMouseMotion(event);
}


//------------------------------------------------------------------------------
void MetaTask::onMouseButtonDown(const SDL_MouseButtonEvent & event)
{
    s_gui.onMouseButtonDown(event);
}


//------------------------------------------------------------------------------
void MetaTask::onMouseButtonUp(const SDL_MouseButtonEvent & event)
{
    s_gui.onMouseButtonUp(event);    
}


//------------------------------------------------------------------------------
void MetaTask::onResizeEvent(unsigned width, unsigned height)
{
    s_gui.onResizeEvent(width,
                        height);
}

//------------------------------------------------------------------------------
RegisteredFpGroup * MetaTask::getFpGroup()
{
    return &fp_group_;
}
