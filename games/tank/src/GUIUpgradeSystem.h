
#ifndef TANK_GUIUPGRADESYSTEM_INCLUDED
#define TANK_GUIUPGRADESYSTEM_INCLUDED

#include <CEGUI/CEGUI.h>

#include "Score.h"
#include "RegisteredFpGroup.h"

class PuppetMasterClient;

//------------------------------------------------------------------------------
class GUIUpgradeSystem
{
 public:
    GUIUpgradeSystem(PuppetMasterClient * puppet_master);
    virtual ~GUIUpgradeSystem();
    
    void update(const Score & score);
    
 protected:
    void loadWidgets();

    void toggleShow();

    void registerCallbacks();

    bool clickedButton1(const CEGUI::EventArgs& e);
    bool clickedButton2(const CEGUI::EventArgs& e);
    bool clickedButton3(const CEGUI::EventArgs& e);
    bool clickedCloseBtn(const CEGUI::EventArgs& e);

    bool onKeySelect(const CEGUI::EventArgs& e);

    CEGUI::Window * root_;
    
    CEGUI::Window * upgrade_window_;
    CEGUI::Window * upgrade_points_label_;
    CEGUI::ButtonBase * close_btn_;

    CEGUI::ButtonBase * button_[3];
    CEGUI::Checkbox * active_upgrades_[3][3]; ///< holds activation flags from layout
    CEGUI::Window * upgrades_desc_[3][3]; ///< holds upgrade description text

    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};


#endif
