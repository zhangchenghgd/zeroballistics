
#include "GUIScore.h"

#include "Gui.h"
#include "Player.h"
#include "PuppetMasterClient.h"
#include "ParameterManager.h"

#include "InputHandler.h"

const unsigned NAME_COLUMN_ID   =   0;
const unsigned SCORE_COLUMN_ID  =   1;
const unsigned LEVEL_COLUMN_ID =    2;
const unsigned KILLS_COLUMN_ID =    3;
const unsigned DEATHS_COLUMN_ID =   4;
const unsigned PING_COLUMN_ID   =   5;


//------------------------------------------------------------------------------
/// Predicate for std::sort
class SortPlayers
{
public:
    bool operator()(const PlayerScore & p1, const PlayerScore & p2) const
        {
            return p1.score_ > p2.score_;
        }
};

//------------------------------------------------------------------------------
GUIScore::GUIScore(PuppetMasterClient * puppet_master) :
    puppet_master_(puppet_master)
{
    enableFloatingPointExceptions(false);

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();

    /// XXX hack because root window is stored inside metatask, no easy way to retrieve it
    CEGUI::Window * parent = wm.getWindow("TankApp_root/");
    
    // Use parent window name as prefix to avoid name clashes
    root_ = wm.loadWindowLayout("gamescore.layout", parent->getName());
    score_window_ = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/window");
    spectator_list_ = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "gamescore/spectators/list");
    lists_available_[0] = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "gamescore/list1");
    lists_available_[1] = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "gamescore/list2");
    lists_available_[2] = (CEGUI::MultiColumnList*) wm.getWindow(parent->getName() + "gamescore/big_list");
    team_name_[0] = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/teamname1");
    team_name_[1] = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/teamname2");
    team_score_label_[0] = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/label_teamscore1");
    team_score_label_[1] = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/label_teamscore2");
    team_score_value_[0] = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/text_teamscore1");
    team_score_value_[1] = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/text_teamscore2");
    acc_   = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/acc_value");
    eff_   = (CEGUI::Window*) wm.getWindow(parent->getName() + "gamescore/eff_value");


    for(unsigned c=0; c < NUM_SCORE_LISTS; c++)
    {
        // set init values for score multicolumn list
        lists_available_[c]->setSelectionMode(CEGUI::MultiColumnList::RowSingle);
        lists_available_[c]->setUserSortControlEnabled(false);
        lists_available_[c]->setUserColumnSizingEnabled(false);
        lists_available_[c]->setUserColumnDraggingEnabled(false);
        lists_available_[c]->setWantsMultiClickEvents(true);
        lists_available_[c]->setSortDirection(CEGUI::ListHeaderSegment::None);
        lists_available_[c]->addColumn("Name",   NAME_COLUMN_ID,     CEGUI::UDim(0.50f,0));
        lists_available_[c]->addColumn("Score",  SCORE_COLUMN_ID,    CEGUI::UDim(0.10f,0));
        lists_available_[c]->addColumn("Level",  LEVEL_COLUMN_ID,    CEGUI::UDim(0.10f,0));
        lists_available_[c]->addColumn("Kills",  KILLS_COLUMN_ID,    CEGUI::UDim(0.10f,0));
        lists_available_[c]->addColumn("Deaths", DEATHS_COLUMN_ID,   CEGUI::UDim(0.10f,0));
        lists_available_[c]->addColumn("Ping",   PING_COLUMN_ID,     CEGUI::UDim(0.099f,0));
        
        // customize lists header
        lists_available_[c]->getListHeader()->getSegmentFromColumn(0).setProperty("Font","main_menu_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(1).setProperty("Font","main_menu_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(2).setProperty("Font","main_menu_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(3).setProperty("Font","main_menu_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(4).setProperty("Font","main_menu_font");
        lists_available_[c]->getListHeader()->getSegmentFromColumn(5).setProperty("Font","main_menu_font");
        lists_available_[c]->disable();
        lists_available_[c]->setVisible(false);
    }

    // set init values for spectator multicolumn list
    spectator_list_->setSelectionMode(CEGUI::MultiColumnList::RowSingle);
    spectator_list_->setUserSortControlEnabled(false);
    spectator_list_->setUserColumnSizingEnabled(false);
    spectator_list_->setUserColumnDraggingEnabled(false);
    spectator_list_->setWantsMultiClickEvents(true);
    spectator_list_->setSortDirection(CEGUI::ListHeaderSegment::None);
    spectator_list_->addColumn("Name",   0,     CEGUI::UDim(0.88f, 0));
    spectator_list_->addColumn("Ping",   1,     CEGUI::UDim(0.119f,0));
    
    // customize list header
    spectator_list_->getListHeader()->getSegmentFromColumn(0).setProperty("Font","main_menu_font");
    spectator_list_->getListHeader()->getSegmentFromColumn(1).setProperty("Font","main_menu_font");
    spectator_list_->disable();



    // add score to widget tree
    parent->addChildWindow(score_window_);

    score_window_->setVisible(false);
    score_window_->setEnabled(false);

    enableFloatingPointExceptions();


    s_input_handler.registerInputCallback("Show Score",
                                          input_handler::ContinuousInputHandler(this, &GUIScore::show),
                                          &fp_group_);
}

//------------------------------------------------------------------------------
GUIScore::~GUIScore()
{
    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();
    wm.destroyWindow(root_);
    // lists get auto deleted by gui win mgr
}

//------------------------------------------------------------------------------
void GUIScore::setTeams(const std::vector<Team*> & teams)
{

    if (teams.size() == 1)
    {
        // Name & Color teams
        team_name_[0]->setText(teams[0]->getName());
        team_name_[0]->setProperty("TextColours","tl:FFDD0000 tr:FFDD0000 bl:FFDD0000 br:FFDD0000");

        lists_used_.clear();
        lists_used_.push_back(lists_available_[2]);

        // set visibilities
        team_name_[1]->setVisible(false);
        lists_available_[0]->setVisible(false);
        lists_available_[1]->setVisible(false);
        lists_available_[2]->setVisible(true);

        // hide team scores
        for(unsigned c=0; c < NUM_SCORE_TEAMS; c++)
        {
            team_score_label_[c]->setVisible(false);
            team_score_value_[c]->setVisible(false);
        }

    }
    else if (teams.size() == 2)
    {
        // Name & Color teams
        team_name_[0]->setText(teams[0]->getName());
        team_name_[1]->setText(teams[1]->getName());

        team_name_[0]->setProperty("TextColours","tl:FF6CCCF4 tr:FF6CCCF4 bl:FF6CCCF4 br:FF6CCCF4");
        team_name_[1]->setProperty("TextColours","tl:FFDD0000 tr:FFDD0000 bl:FFDD0000 br:FFDD0000");

        lists_used_.clear();
        lists_used_.push_back(lists_available_[0]);
        lists_used_.push_back(lists_available_[1]);

        // set visibilities
        team_name_[1]->setVisible(true);
        lists_available_[0]->setVisible(true);
        lists_available_[1]->setVisible(true);
        lists_available_[2]->setVisible(false);

        // show team scores
        for(unsigned c=0; c < NUM_SCORE_TEAMS; c++)
        {
            team_score_label_[c]->setVisible(true);
            team_score_value_[c]->setVisible(true);
        }

        team_score_label_[0]->setProperty("TextColours","tl:FF6CCCF4 tr:FF6CCCF4 bl:FF6CCCF4 br:FF6CCCF4");
        team_score_label_[1]->setProperty("TextColours","tl:FFDD0000 tr:FFDD0000 bl:FFDD0000 br:FFDD0000");
        team_score_value_[0]->setProperty("TextColours","tl:FF6CCCF4 tr:FF6CCCF4 bl:FF6CCCF4 br:FF6CCCF4");
        team_score_value_[1]->setProperty("TextColours","tl:FFDD0000 tr:FFDD0000 bl:FFDD0000 br:FFDD0000");

    }
    else
    {
        s_log << Log::warning << "GUIScore cannot handle Team size: " << teams.size() << "\n";
    }
}


//------------------------------------------------------------------------------
void GUIScore::update(const Score & score)
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
    std::sort(players.begin(), players.end(), SortPlayers());
    
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

                // calc statistic values
                float acc = 0.0f;
                float eff = 0.0f;
                if(players[i].shots_ > 0) acc = ((float)players[i].hits_ / (float)players[i].shots_) * 100.0f;
                if(players[i].kills_ > 0)
                {
                    eff = (float)players[i].kills_ / (players[i].kills_ + players[i].deaths_) * 100.0f;
                }
                acc_->setText(toString((unsigned)acc) + "%");
                eff_->setText(toString((unsigned)eff) + "%");                    
            }
            else
            {
                set_selected = false;
            }

            // calc upgrade level of each player
            unsigned level = 0;
            for(unsigned c=0; c < NUM_UPGRADE_CATEGORIES; c++)
            {
                level += (unsigned)players[i].active_upgrades_[c];
            }

            addScoreElement(lists_used_[team_id],
                            players[i].getPlayer()->getName(),
                            toString(players[i].score_),
                            toString(level),
                            toString(players[i].kills_),
                            toString(players[i].deaths_),
                            toString(players[i].ping_),
                            set_selected);  
            
        }
        else
        {
            // treat local player different, if spectator also clear eff,acc values
            if(players[i].getPlayer() == puppet_master_->getLocalPlayer())
            {
                acc_->setText("-");
                eff_->setText("-"); 
                set_selected = true;
            }

            else
            {
                set_selected = false;
            }
            
                addSpectatorElement(players[i].getPlayer()->getName(),
                                    toString(players[i].ping_),
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
void GUIScore::show(float show)
{
    if (!score_window_) return;

    score_window_->setVisible((bool)show);
}

//------------------------------------------------------------------------------
void GUIScore::addScoreElement( CEGUI::MultiColumnList * list,
                                const std::string & name,
                                const std::string & score,
                                const std::string & level,
                                const std::string & kills,
                                const std::string & deaths,
                                const std::string & ping,
                                bool selected)
{

	unsigned row = list->addRow();
	list->setItem(new ScoreElement(name),   NAME_COLUMN_ID  , row);
	list->setItem(new ScoreElement(score),  SCORE_COLUMN_ID , row);
	list->setItem(new ScoreElement(level),  LEVEL_COLUMN_ID , row);
    list->setItem(new ScoreElement(kills),  KILLS_COLUMN_ID, row);
    list->setItem(new ScoreElement(deaths), DEATHS_COLUMN_ID, row);
    list->setItem(new ScoreElement(ping),   PING_COLUMN_ID  , row);

    if(selected)
    {
        list->setItemSelectState(CEGUI::MCLGridRef(row, NAME_COLUMN_ID  ), selected);
        list->setItemSelectState(CEGUI::MCLGridRef(row, SCORE_COLUMN_ID ), selected);
        list->setItemSelectState(CEGUI::MCLGridRef(row, LEVEL_COLUMN_ID ), selected);
        list->setItemSelectState(CEGUI::MCLGridRef(row, KILLS_COLUMN_ID ), selected);
        list->setItemSelectState(CEGUI::MCLGridRef(row, DEATHS_COLUMN_ID), selected);
        list->setItemSelectState(CEGUI::MCLGridRef(row, PING_COLUMN_ID  ), selected);
    }
}

//------------------------------------------------------------------------------
void GUIScore::addSpectatorElement( const std::string & name,
                                    const std::string & ping,
                                    bool selected)
{
	unsigned row = spectator_list_->addRow();
	spectator_list_->setItem(new ScoreElement(name),  0, row);
	spectator_list_->setItem(new ScoreElement(ping),  1, row);

    if(selected)
    {
        spectator_list_->setItemSelectState(CEGUI::MCLGridRef(row, 0), selected);
        spectator_list_->setItemSelectState(CEGUI::MCLGridRef(row, 1), selected);
    }
}
