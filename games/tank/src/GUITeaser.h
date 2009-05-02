
#ifndef TANK_GUITEASER_INCLUDED
#define TANK_GUITEASER_INCLUDED

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
    
 protected:

    void loadWidgets();
    void registerCallbacks();

    bool clickedScreen(const CEGUI::EventArgs& e);

    CEGUI::Window * teaser_window_;

    MainMenu * main_menu_;
};

#endif
