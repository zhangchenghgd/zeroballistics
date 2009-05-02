
#include "GUIScoreSoccer.h"

#include "Gui.h"
#include "Player.h"
#include "PuppetMasterClient.h"
#include "ParameterManager.h"


//------------------------------------------------------------------------------
/// Predicate for std::sort
class SortPlayersGoals
{
public:
    bool operator()(const PlayerScore & p1, const PlayerScore & p2) const
        {
            return p1.goals_ > p2.goals_;
        }
};

//------------------------------------------------------------------------------
GUIScoreSoccer::GUIScoreSoccer(PuppetMasterClient * puppet_master) :
    GUIScore(puppet_master)
{
    enableFloatingPointExceptions(false);

    loadWidgets();

    customizeLayout();

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
GUIScoreSoccer::~GUIScoreSoccer()
{    
}

//------------------------------------------------------------------------------
void GUIScoreSoccer::update(const Score & score)
{
    enableFloatingPointExceptions(false);

    std::vector<PlayerScore> players = score.getPlayers();

    // clear score table
    spectator_list_->resetList();
    for(unsigned c=0; c < lists_used_.size(); c++)
    {
        lists_used_[c]->resetList();
    }

    // sort the players descending to kills
    std::sort(players.begin(), players.end(), SortPlayersGoals());
    
    bool set_selected = false;

    // fill score table
    for(unsigned i=0;i<players.size();i++)
    {
        if(players[i].getTeam()) // team can be NULL on join
        {
            TEAM_ID team_id = players[i].getTeam()->getId();
        
            // treat local player different
            if(players[i].getPlayer() == puppet_master_->getLocalPlayer())
            {
                set_selected = true;                 
            }
            else
            {
                set_selected = false;
            }

            bool player_dead = players[i].getPlayer()->getControllable() == NULL;

            addScoreElement(lists_used_[team_id],
                            players[i].getPlayer()->getName() + (player_dead ? "     (DEAD)" : ""),
                            toString(players[i].goals_),
                            toString(players[i].assists_),
                            toString(players[i].kills_),
                            toString(players[i].deaths_),
                            toString((unsigned)(players[i].getPlayer()->getNetworkDelay()*1000.0f)),
                            set_selected);  
            
        }
        else
        {
            // treat local player different, if spectator also clear eff,acc values
            if(players[i].getPlayer() == puppet_master_->getLocalPlayer())
            {
                set_selected = true;
            }

            else
            {
                set_selected = false;
            }
            
                addSpectatorElement(players[i].getPlayer()->getName(),
                                    toString((unsigned)(players[i].getPlayer()->getNetworkDelay()*1000.0f)),
                                    set_selected); 
        }
    }


    // fill team scores
    for(unsigned c=0; c < score.getNumTeams(); c++)
    {
        int score_team = score.getTeamScore(c)->score_;
        team_score_value_[c]->setText(toString( score_team ));
    }

    enableFloatingPointExceptions(true);
}

//------------------------------------------------------------------------------
void GUIScoreSoccer::loadWidgets()
{

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("TankApp_root/");

    acc_label_   = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/acc_text");
    eff_label_   = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/eff_text");


    // make accuracy/efficency labels invisible in soccer mode
    acc_->setVisible(false);
    eff_->setVisible(false);
    acc_label_->setVisible(false);
    eff_label_->setVisible(false);

}

//------------------------------------------------------------------------------
void GUIScoreSoccer::customizeLayout()
{
    for(unsigned c=0; c < NUM_SCORE_LISTS; c++)
    {
        // customize lists header
        lists_available_[c]->getListHeader()->getSegmentFromColumn(0).setProperty("Font","main_menu_small_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(1).setProperty("Font","main_menu_small_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(2).setProperty("Font","main_menu_small_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(3).setProperty("Font","main_menu_small_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(4).setProperty("Font","main_menu_small_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(5).setProperty("Font","main_menu_small_font");

        lists_available_[c]->getListHeader()->getSegmentFromColumn(0).setText("Name");      
        lists_available_[c]->getListHeader()->getSegmentFromColumn(1).setText("Goals");      
        lists_available_[c]->getListHeader()->getSegmentFromColumn(2).setText("Assists");      
        lists_available_[c]->getListHeader()->getSegmentFromColumn(3).setText("Fouls");      
        lists_available_[c]->getListHeader()->getSegmentFromColumn(4).setText("Decomp.");      

        lists_available_[c]->disable();
        lists_available_[c]->setVisible(false);
    }


    // customize spect. list header
    spectator_list_->getListHeader()->getSegmentFromColumn(0).setProperty("Font","main_menu_small_font");
    spectator_list_->getListHeader()->getSegmentFromColumn(1).setProperty("Font","main_menu_small_font");
    spectator_list_->disable();


}
