
#ifndef TANK_GUITEAMSELECT_INCLUDED
#define TANK_GUITEAMSELECT_INCLUDED

#include <CEGUI/CEGUI.h>

#include "RegisteredFpGroup.h"

class PuppetMasterClient;
class Team;

const unsigned NUM_TEAM_BUTTONS = 2;

//------------------------------------------------------------------------------
class GUITeamSelect
{
 public:
    GUITeamSelect(PuppetMasterClient * puppet_master,
                  const std::vector<Team*> & teams);
    virtual ~GUITeamSelect();
    
    void show(bool show);
    
 protected:

    void toggleShow();
    
    void registerCallbacks();

    bool onKeySelect(const CEGUI::EventArgs& e);
    bool clickedBtn1(const CEGUI::EventArgs& e);
    bool clickedBtn2(const CEGUI::EventArgs& e);
    bool clickedBtn3(const CEGUI::EventArgs& e);
    bool clickedWindow(const CEGUI::EventArgs& e);

    CEGUI::Window * root_;

    CEGUI::Window * menu_window_;
    CEGUI::ButtonBase * menu_btn_[NUM_TEAM_BUTTONS];
    CEGUI::Window * menu_btn_text_[NUM_TEAM_BUTTONS];

    CEGUI::ButtonBase * menu_btn_spec_;

    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};


#endif
