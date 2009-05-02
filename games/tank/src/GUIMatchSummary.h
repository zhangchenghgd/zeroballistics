
#ifndef TANK_GUIMATCHSUMMARY_INCLUDED
#define TANK_GUIMATCHSUMMARY_INCLUDED

#include <CEGUI/CEGUI.h>

#include "GUIOptions.h" ///< used for GUI constants
#include "Score.h"
#include "RegisteredFpGroup.h"

class PuppetMasterClient;

//------------------------------------------------------------------------------
class GUIMatchSummary
{
 public:
    GUIMatchSummary(PuppetMasterClient * puppet_master);
    virtual ~GUIMatchSummary();
      
    virtual void show(float show);
    
 protected:

    virtual void loadWidgets();
    virtual void customizeLayout();
    virtual void registerCallbacks();

    bool clickedOkBtn(const CEGUI::EventArgs& e);

    void addScoreElement(CEGUI::MultiColumnList * list,
                         const std::string & name,
                         const std::string & skill,
                         const std::string & delta,
                         bool selected);

    CEGUI::Window * root_;

    CEGUI::Window * summary_window_;
    CEGUI::Window * match_text_[2];
    CEGUI::ButtonBase * ok_btn_;

    CEGUI::MultiColumnList * player_skill_list_;

    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};


#endif
