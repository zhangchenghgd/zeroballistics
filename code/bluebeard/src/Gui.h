
#ifndef TANK_GUI_DEFINED
#define TANK_GUI_DEFINED

#include <string>
#include <vector>

#include <CEGUI/CEGUI.h>

#include <SDL/SDL_keysym.h>
#include <SDL/SDL_events.h>

#include "Singleton.h"
#include "Vector2d.h"

namespace CEGUI
{
    class System;
    class OpenGLRenderer;
}

const unsigned ACTIVE_WINDOW_STACK_SIZE = 1;

#define s_gui Loki::SingletonHolder<Gui, Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()
//------------------------------------------------------------------------------
class Gui
{
    DECLARE_SINGLETON(Gui);

 public:
    virtual ~Gui();

    void frameMove(float dt);
    void init(int width, int height);
    void render();

    void setMouseCursorVisible(bool v);
    Vector2d getMouseCursorPostion() const;

    bool onKeyDown(SDL_keysym sym);
    bool onKeyUp(SDL_keysym sym);
    bool onMouseButtonDown(const SDL_MouseButtonEvent & event);
    bool onMouseButtonUp(const SDL_MouseButtonEvent & event);
    bool onMouseMotion(const SDL_MouseMotionEvent & event);
    void onResizeEvent(int w, int h);

    bool onGuiSheetChanged(const CEGUI::EventArgs &ea);
    bool onWindowActivated(const CEGUI::EventArgs &ea);
    bool onWindowDeactivated(const CEGUI::EventArgs &ea);
    bool onWindowDestructed(const CEGUI::EventArgs &ea);

    SDLKey CEGUIKeyToSDLKey(CEGUI::uint key);

    void subscribeActivationEvents(CEGUI::Window * win);
 protected:

    CEGUI::uint SDLKeyToCEGUIKey(SDLKey key);

    std::auto_ptr<CEGUI::OpenGLRenderer> renderer_;
    std::auto_ptr<CEGUI::System> cegui_system_;

    CEGUI::FontManager * font_manager_; ///< deleted by CEGUI

    float width_;
    float height_;
    float aspect_width_; ///< this is used to keep the gui window
                         ///< at an 4/3 aspect at any resolution

    std::vector<CEGUI::Window*> activated_windows_;
    CEGUI::Window * deactivated_window_;
};

#endif
