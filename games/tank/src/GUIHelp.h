
#ifndef TANK_GUIHELP_INCLUDED
#define TANK_GUIHELP_INCLUDED

#include <CEGUI/CEGUI.h>


#include "RegisteredFpGroup.h"

//------------------------------------------------------------------------------
class GUIHelp
{
 public:
    GUIHelp();
    virtual ~GUIHelp();
    
    bool clickedTab1Btn(const CEGUI::EventArgs& e);
    bool clickedCloseBtn(const CEGUI::EventArgs& e);

 protected:

    void toggleShow();

    void registerCallbacks();

    CEGUI::Window * root_;
    
    CEGUI::Window * help_window_;
    CEGUI::Window * tab1_window_;
    CEGUI::ButtonBase * tab1_btn_;
    CEGUI::ButtonBase * close_btn_;

    RegisteredFpGroup fp_group_;
};


#endif
