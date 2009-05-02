
#include "GUIMatchSummary.h"


#include "GUIScore.h" ///< used for Score element def.
#include "Gui.h"
#include "Player.h"
#include "PuppetMasterClient.h"
#include "ParameterManager.h"
#include "SdlApp.h"

const unsigned NAME_COLUMN_ID   =   0;
const unsigned SKILL_COLUMN_ID  =   1;
const unsigned DELTA_COLUMN_ID =    2;


//------------------------------------------------------------------------------
/// Predicate for std::sort
class SortPlayersSkill
{
public:
    bool operator()(const PlayerScore & p1, const PlayerScore & p2) const
        {
            return p1.score_ > p2.score_;
        }
};

//------------------------------------------------------------------------------
GUIMatchSummary::GUIMatchSummary(PuppetMasterClient * puppet_master) :
    puppet_master_(puppet_master)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    customizeLayout();

    registerCallbacks();

    // initially invisible
    summary_window_->setVisible(false);

    enableFloatingPointExceptions();

}

//------------------------------------------------------------------------------
GUIMatchSummary::~GUIMatchSummary()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();
    wm.destroyWindow(root_);
    // lists get auto deleted by gui win mgr
}

//------------------------------------------------------------------------------
void GUIMatchSummary::show(float show)
{
    if (!summary_window_) return;

    summary_window_->setVisible((bool)show);

    if((bool)show)
    {
        s_app.captureMouse(false);
    }
}

//------------------------------------------------------------------------------
void GUIMatchSummary::loadWidgets()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("TankApp_root/");
    
    // Use parent window name as prefix to avoid name clashes
    root_ = wm.loadWindowLayout("matchsummary.layout", parent->getName());
    summary_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "matchsummary/window");
    player_skill_list_ = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "matchsummary/list");
    match_text_[0] = (CEGUI::Window*) wm.getWindow(parent->getName() + "matchsummary/match_text_left");
    match_text_[1] = (CEGUI::Window*) wm.getWindow(parent->getName() + "matchsummary/match_text_right");
    ok_btn_   = (CEGUI::ButtonBase*) wm.getWindow(parent->getName() + "matchsummary/ok_btn");

    // add score to widget tree
    parent->addChildWindow(summary_window_);

}

//------------------------------------------------------------------------------
void GUIMatchSummary::customizeLayout()
{
    player_skill_list_->setSelectionMode(CEGUI::MultiColumnList::RowSingle);
    player_skill_list_->setUserSortControlEnabled(false);
    player_skill_list_->setUserColumnSizingEnabled(false);
    player_skill_list_->setUserColumnDraggingEnabled(false);
    player_skill_list_->setWantsMultiClickEvents(true);
    player_skill_list_->setSortDirection(CEGUI::ListHeaderSegment::None);
    player_skill_list_->addColumn("Name",   NAME_COLUMN_ID,     CEGUI::UDim(0.74f,0));
    player_skill_list_->addColumn("Skill",  SKILL_COLUMN_ID,    CEGUI::UDim(0.15f,0));
    player_skill_list_->addColumn(" + / - ",  DELTA_COLUMN_ID,    CEGUI::UDim(0.10f,0));
    player_skill_list_->disable();

    // customize lists header
    player_skill_list_->getListHeader()->getSegmentFromColumn(NAME_COLUMN_ID).setProperty("Font","main_menu_small_font");
    player_skill_list_->getListHeader()->getSegmentFromColumn(SKILL_COLUMN_ID).setProperty("Font","main_menu_small_font");
    player_skill_list_->getListHeader()->getSegmentFromColumn(DELTA_COLUMN_ID).setProperty("Font","main_menu_small_font"); 
}

//------------------------------------------------------------------------------
void GUIMatchSummary::registerCallbacks()
{
    ok_btn_->subscribeEvent(CEGUI::ButtonBase::EventMouseClick, CEGUI::Event::Subscriber(&GUIMatchSummary::clickedOkBtn, this));
}

//------------------------------------------------------------------------------
bool GUIMatchSummary::clickedOkBtn(const CEGUI::EventArgs& e)
{
    show(0.0f);
    return true;
}

//------------------------------------------------------------------------------
void GUIMatchSummary::addScoreElement( CEGUI::MultiColumnList * list,
                                        const std::string & name,
                                        const std::string & skill,
                                        const std::string & delta,
                                        bool selected)
{

	unsigned row = list->addRow();
	list->setItem(new ScoreElement(name),   NAME_COLUMN_ID  , row);
	list->setItem(new ScoreElement(skill),  SKILL_COLUMN_ID , row);
	list->setItem(new ScoreElement(delta),  DELTA_COLUMN_ID , row);

    if(selected)
    {
        list->setItemSelectState(CEGUI::MCLGridRef(row, NAME_COLUMN_ID  ), selected);
        list->setItemSelectState(CEGUI::MCLGridRef(row, SKILL_COLUMN_ID ), selected);
        list->setItemSelectState(CEGUI::MCLGridRef(row, DELTA_COLUMN_ID ), selected);
    }
}

