
#ifndef TANK_GUITEASER_INCLUDED
#define TANK_GUITEASER_INCLUDED

#ifdef TEASER_ENABLED

#include <CEGUI/CEGUI.h>

#include "ParameterManager.h"

class MainMenu;

//------------------------------------------------------------------------------
class GUITeaser
{
 public:
    GUITeaser(MainMenu * main_menu);
    virtual ~GUITeaser();
    
    void show();
    void hide();
    
 protected:

    void loadWidgets();
    void registerCallbacks();

    bool clickedScreen(const CEGUI::EventArgs& e);

    CEGUI::Window * teaser_window_;

    MainMenu * main_menu_;
};

#endif // TEASER_ENABLED

#endif
