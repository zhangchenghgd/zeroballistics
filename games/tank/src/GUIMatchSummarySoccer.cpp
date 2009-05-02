
#include "GUIMatchSummarySoccer.h"


#include "GUIScore.h" ///< used for Score element def.
#include "Gui.h"
#include "Player.h"
#include "PuppetMasterClient.h"
#include "ParameterManager.h"

#include "InputHandler.h"

const unsigned NAME_COLUMN_ID   =   0;
const unsigned SKILL_COLUMN_ID  =   1;
const unsigned DELTA_COLUMN_ID =    2;


//------------------------------------------------------------------------------
GUIMatchSummarySoccer::GUIMatchSummarySoccer(PuppetMasterClient * puppet_master) :
    GUIMatchSummary(puppet_master)
{
    enableFloatingPointExceptions(false);

    customizeLayout();

    enableFloatingPointExceptions();

}

//------------------------------------------------------------------------------
GUIMatchSummarySoccer::~GUIMatchSummarySoccer()
{
}

//------------------------------------------------------------------------------
void GUIMatchSummarySoccer::setMatchSummaryText(const std::string & left_text,
                                                const std::string & right_text)
{
    match_text_[0]->setText(left_text);
    match_text_[1]->setText(right_text);
}

//------------------------------------------------------------------------------
void GUIMatchSummarySoccer::addSkillElement(const std::string & name,
                     const std::string & skill,
                     const std::string & delta,
                     bool selected)
{
    addScoreElement(player_skill_list_, name, skill, delta, selected);
}

//------------------------------------------------------------------------------
void GUIMatchSummarySoccer::customizeLayout()
{
}



