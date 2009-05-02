
#include "Score.h"


#include "NetworkCommand.h"
#include "Log.h"
#include "Player.h"
#include "ParameterManager.h"

//------------------------------------------------------------------------------
PlayerScore::PlayerScore(const Player * player, Team * team) :
    score_(0),
    kills_(0),
    deaths_(0),
    shots_(0),
    hits_(0),
    upgrade_points_(0),
    player_(player),
    team_(team)
{
    // upgrades
    for(uint8_t cat = 0; cat < NUM_UPGRADE_CATEGORIES; cat++)
    {
        active_upgrades_[cat] = 0;
    }

    // equipment
    for(uint8_t slot = 0; slot < NUM_EQUIPMENT_SLOTS; slot++)
    {
        active_equipment_[slot] = 0;
    }
}


//------------------------------------------------------------------------------
PlayerScore::~PlayerScore()
{
}

//------------------------------------------------------------------------------
void PlayerScore::setTeam(Team * team)
{
    team_ = team;
}


//------------------------------------------------------------------------------
const Player * PlayerScore::getPlayer() const
{
    return player_;
}


//------------------------------------------------------------------------------
Team * PlayerScore::getTeam() const
{
    return team_;
}

//------------------------------------------------------------------------------
void PlayerScore::reset()
{
    score_ = 0;
    kills_  = 0;
    deaths_ = 0;
    shots_ = 0;
    hits_ = 0;
    upgrade_points_ = 0;

    // upgrades
    for(uint8_t cat = 0; cat < NUM_UPGRADE_CATEGORIES; cat++)
    {
        active_upgrades_[cat] = 0;
    }

    // equipment
    for(uint8_t slot = 0; slot < NUM_EQUIPMENT_SLOTS; slot++)
    {
        active_equipment_[slot] = 0;
    }
}

//------------------------------------------------------------------------------
/**
 *  \return Returns true if specified \param category can be upgraded:
 *          including check for enough upgrade points and locked categories
 */
bool PlayerScore::isUpgradePossible(UPGRADE_CATEGORY category) const
{
   
    if(upgrade_points_ >= getNeededUpgradePoints(category) && 
       !isUpgradeCategoryLocked(category))
    {
        return true;
    } else
    {
        return false;
    }

}

//------------------------------------------------------------------------------
/**
 *  \return The upgrade points needed to achieve the next level in the
 *  specified category.
 */
unsigned PlayerScore::getNeededUpgradePoints(UPGRADE_CATEGORY category) const
{
    // try/catch necessary, param for upgrade.cost 3+ does not exist
    try
    {
        return s_params.get<unsigned>("upgrade.cost." +
                                      UPGRADES[category] +
                                      toString(active_upgrades_[category]+1));
    } catch (ParamNotFoundException & e)
    {
        return (unsigned)-1;
    }
}

//------------------------------------------------------------------------------
/**
 *  \return Returns true if the provided UC is locked for upgrade,
 *          two other categories already have been chosen.
 */
bool PlayerScore::isUpgradeCategoryLocked(UPGRADE_CATEGORY category) const
{
    return false;
}

//------------------------------------------------------------------------------
bool PlayerScore::operator==(const SystemAddress & id) const
{
    assert(player_);
    return player_->getId() == id;
}


//------------------------------------------------------------------------------
TeamScore::TeamScore(Team * team) :
    score_(0),
    team_(team)
{
}

//------------------------------------------------------------------------------
TeamScore::~TeamScore()
{
}

//------------------------------------------------------------------------------
Team * TeamScore::getTeam() const
{
    assert(team_);
    return team_;
}


//------------------------------------------------------------------------------
Score::Score() : time_left_(0.0f)
{
}


//------------------------------------------------------------------------------
Score::~Score()
{
}
    
//------------------------------------------------------------------------------
void Score::resetPlayerScores()
{
    for (std::map<SystemAddress, PlayerScore>::iterator it = player_score_.begin();
         it != player_score_.end();
         ++it)
    {
        it->second.reset();
    }
}
 
//------------------------------------------------------------------------------
void Score::resetTeamAssignments()
{
    for (std::map<SystemAddress, PlayerScore>::iterator it = player_score_.begin();
         it != player_score_.end();
         ++it)
    {
        assignToTeam(it->first, INVALID_TEAM_ID);
    }
}

//------------------------------------------------------------------------------
/**
 *  Adds the player. No team is assigned per default.
 */
PlayerScore * Score::addPlayer(const Player * p)
{
    assert(player_score_.find(p->getId()) == player_score_.end());
    assert(p->getId() != UNASSIGNED_SYSTEM_ADDRESS);

    s_log << Log::debug('l')
          << "adding player "
          << p->getId()
          << " to score.\n";

    std::pair<std::map<SystemAddress, PlayerScore>::iterator, bool > ins;
    ins = player_score_.insert(std::make_pair(p->getId(), PlayerScore(p, NULL)));

    return &ins.first->second;
}



//------------------------------------------------------------------------------
PlayerScore * Score::getPlayerScore(const SystemAddress & id)
{
    std::map<SystemAddress, PlayerScore>::iterator it = player_score_.find(id);

    if (it == player_score_.end())
    {
        return NULL;
    } else
    {
        return &player_score_.find(id)->second;
    }
}

//------------------------------------------------------------------------------
const PlayerScore * Score::getPlayerScore(const SystemAddress & id) const
{
    std::map<SystemAddress, PlayerScore>::const_iterator it = player_score_.find(id);

    if (it == player_score_.end())
    {
        return NULL;
    } else
    {
        return &player_score_.find(id)->second;
    }
}


//------------------------------------------------------------------------------
void Score::removePlayer(const SystemAddress & id)
{
    assert(player_score_.find(id) != player_score_.end());

    player_score_.erase(player_score_.find(id));
}


//------------------------------------------------------------------------------
void Score::addTeam(Team * team)
{
    assert(team->getId() == team_score_.size());
    
    team_score_.push_back(TeamScore(team));
}


//------------------------------------------------------------------------------
unsigned Score::getNumTeams() const
{
    return team_score_.size();
}


//------------------------------------------------------------------------------
TeamScore * Score::getTeamScore(TEAM_ID id)
{
    if (id <= team_score_.size()) return &team_score_[id];
    else return NULL;
}

//------------------------------------------------------------------------------
const TeamScore * Score::getTeamScore(TEAM_ID id) const
{
    if (id <= team_score_.size()) return &team_score_[id];
    else return NULL;
}

//------------------------------------------------------------------------------
void Score::assignToTeam(const SystemAddress & pid, TEAM_ID team_id)
{
    std::map<SystemAddress, PlayerScore>::iterator it = player_score_.find(pid);

    if (it == player_score_.end())
    {
        s_log << Log::warning
              << "assignToTeam called for nonexisting player"
              << pid
              << "\n";
        return;
    }

    Team * team = team_id == INVALID_TEAM_ID ? NULL : team_score_[team_id].getTeam();

    it->second.setTeam(team);

    s_log << Log::debug('l')
          << "Player "
          << pid
          << " was assigned to team ";
    if (team) s_log << team->getName();
    else      s_log << "NONE";
    s_log << "\n";
}


//------------------------------------------------------------------------------
Team * Score::getTeam(const SystemAddress & player) const
{
    std::map<SystemAddress, PlayerScore>::const_iterator it = player_score_.find(player);

    if (it == player_score_.end()) return NULL;
    else return it->second.getTeam();
}


//------------------------------------------------------------------------------
Team * Score::getTeam(TEAM_ID id)
{
    TeamScore * sc = getTeamScore(id);
    if (sc) return sc->getTeam();
    else return NULL;
}


//------------------------------------------------------------------------------
/**
 *  Convenience function. Returns the team id the specified player
 *  belongs to or INVALID_TEAM_ID.
 */
TEAM_ID Score::getTeamId(const SystemAddress & player) const
{
    Team * t = getTeam(player);
    return t ? t->getId() : INVALID_TEAM_ID;
}


//------------------------------------------------------------------------------
std::vector<PlayerScore> Score::getPlayers() const
{
    std::vector<PlayerScore> players;

    for (std::map<SystemAddress, PlayerScore>::const_iterator it = player_score_.begin();
         it != player_score_.end();
         ++it)
    {
        players.push_back(it->second);
    }

    return players;
}

//------------------------------------------------------------------------------
/**
 *  Finds the team with the least number of players.
 */
TEAM_ID Score::getSmallestTeam() const
{    
    std::vector<unsigned> num_players(team_score_.size(), 0);
    for (std::map<SystemAddress, PlayerScore>::const_iterator it = player_score_.begin();
         it != player_score_.end();
         ++it)
    {
        if (it->second.getTeam() == NULL) continue;
        ++num_players[it->second.getTeam()->getId()];
    }

    return (TEAM_ID)(std::min_element(num_players.begin(), num_players.end()) - num_players.begin());
}

//------------------------------------------------------------------------------
void Score::setTimeLeft(float32_t t)
{
    time_left_ = t;
}


//------------------------------------------------------------------------------
float32_t Score::getTimeLeft() const
{
    return time_left_;
}
