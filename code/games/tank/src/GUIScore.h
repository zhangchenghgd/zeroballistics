
#ifndef TANK_GUISCORE_INCLUDED
#define TANK_GUISCORE_INCLUDED

#include <CEGUI/CEGUI.h>

#include "GUIOptions.h" ///< used for GUI constants
#include "Score.h"
#include "RegisteredFpGroup.h"

const unsigned NUM_SCORE_TEAMS = 2;
const unsigned NUM_SCORE_LISTS = 3;

class PuppetMasterClient;

//------------------------------------------------------------------------------
class GUIScore
{
 public:
    GUIScore(PuppetMasterClient * puppet_master);
    virtual ~GUIScore();

    virtual void setTeams(const std::vector<Team*> & teams);
    
    virtual void update(const Score & score);    
    virtual void show(float show);
    
 protected:

    virtual void loadWidgets();
    virtual void customizeLayout();
    virtual void registerCallbacks();


    void addScoreElement(CEGUI::MultiColumnList * list,
                         const std::string & name,
                         const std::string & score,
                         const std::string & level,
                         const std::string & kills,
                         const std::string & deaths,
                         const std::string & ping,
                         bool selected);

    void addSpectatorElement(const std::string & name,
                             const std::string & ping,
                             bool selected);

    CEGUI::Window * root_;

    CEGUI::Window * score_window_;

    CEGUI::MultiColumnList * spectator_list_;
    CEGUI::MultiColumnList * lists_available_[NUM_SCORE_LISTS];
    std::vector<CEGUI::MultiColumnList*> lists_used_;

    CEGUI::Window * team_name_[NUM_SCORE_TEAMS];
    CEGUI::Window * team_score_label_[NUM_SCORE_TEAMS];
    CEGUI::Window * team_score_value_[NUM_SCORE_TEAMS];

    CEGUI::Window * acc_;
    CEGUI::Window * eff_;

    PuppetMasterClient * puppet_master_;

    RegisteredFpGroup fp_group_;
};


//------------------------------------------------------------------------------
/*** \brief Specialized sub-class for ListboxItemWindow that represents a  
 *          Key Name/Value Element inside a listbox
 **/
class ScoreElement : public CEGUI::ListboxTextItem
{
public:
    ScoreElement(const std::string & text) : ListboxTextItem(text)
    {
        setSelectionBrushImage(LOOK_N_FEEL, "MultiListSelectionBrush");
        setFont(LISTBOX_ITEM_FONT_SMALL);
        setSelectionColours(LISTBOX_ITEM_SELECTION_COLOR);
        setTextColours(LISTBOX_ITEM_TEXT_COLOR);
    }
};


#endif
