
#ifndef TANK_SCORE_INCLUDED
#define TANK_SCORE_INCLUDED


#include <map>


#include "Team.h"

class Player;
class Team;
struct SystemAddress;
 
const unsigned KILL_VALUE = 1;
const unsigned KILL_SCORE_VALUE = 2;
const unsigned KILL_ASSIST_SCORE_VALUE = 1;
const unsigned BEACON_DESTROY_VALUE = 2;
const unsigned BEACON_DESTROY_ASSIST_VALUE = 1;

const unsigned KILL_UPGRADE_VALUE = 2;
const unsigned KILL_ASSIST_UPGRADE_VALUE = 1;

    
//------------------------------------------------------------------------------
const std::string UPGRADES[] = {
    "weapon",
    "armor", 
    "speed" 
};

//------------------------------------------------------------------------------
enum UPGRADE_CATEGORY
{
    UC_WEAPON,
    UC_ARMOR,
    UC_SPEED
};

const unsigned NUM_UPGRADE_CATEGORIES = sizeof(UPGRADES) / sizeof(std::string);

//------------------------------------------------------------------------------
const std::string EQUIPMENT_SLOTS[] = {
    "primary_weapon",
    "secondary_weapon", 
    "first_skill",
    "second_skill"
};

const unsigned NUM_EQUIPMENT_SLOTS = sizeof(EQUIPMENT_SLOTS) / sizeof(std::string);

//------------------------------------------------------------------------------
class PlayerScore
{
 public:
    PlayerScore(const Player * player, Team * team);
    virtual ~PlayerScore();

    void setTeam(Team * team);
    
    const Player * getPlayer() const;
    Team * getTeam() const;

    void reset();

    bool isUpgradePossible(UPGRADE_CATEGORY category) const;
    unsigned getNeededUpgradePoints(UPGRADE_CATEGORY category) const;
    bool isUpgradeCategoryLocked(UPGRADE_CATEGORY category) const;

    bool operator==(const SystemAddress & id) const;
    
    int16_t  score_;

    uint16_t kills_;
    uint16_t deaths_;

    uint16_t goals_;    ///< XXX this is soccer only, implement new score someday
    uint16_t assists_;  ///< XXX this is soccer only

    uint16_t shots_;  ///< for statistics only
    uint16_t hits_;   ///< for statistics only

    uint16_t upgrade_points_;

    uint8_t active_upgrades_[NUM_UPGRADE_CATEGORIES];

    uint8_t active_equipment_[NUM_EQUIPMENT_SLOTS];
    
 protected:


    const Player * player_;
    Team * team_;
};


//------------------------------------------------------------------------------
class TeamScore
{
 public:
    TeamScore(Team * team);
    virtual ~TeamScore();

    Team * getTeam() const;
    
    int score_;
    
 protected:
    Team * team_;
};

//------------------------------------------------------------------------------
class Score
{
 public:
    Score();
    virtual ~Score();
    
    void resetPlayerScores();
    void resetTeamAssignments();
    
    PlayerScore * addPlayer(const Player * p);
    PlayerScore * getPlayerScore(const SystemAddress & id);
    const PlayerScore * getPlayerScore(const SystemAddress & id) const;
    void removePlayer(const SystemAddress & id);

    void addTeam(Team * team);
    unsigned getNumTeams() const;
    TeamScore * getTeamScore(TEAM_ID id);
    const TeamScore * getTeamScore(TEAM_ID id) const;

    void assignToTeam(const SystemAddress & pid, TEAM_ID team_id);

    Team *  getTeam  (const SystemAddress & player) const;
    Team *  getTeam  (TEAM_ID id);
    TEAM_ID getTeamId(const SystemAddress & player) const;

    std::vector<PlayerScore> getPlayers() const;

    TEAM_ID getSmallestTeam() const;
    unsigned getTeamSize(TEAM_ID tid) const;

    void setTimeLeft(float32_t t);
    float32_t getTimeLeft() const;
    
 protected:
    std::map<SystemAddress, PlayerScore> player_score_;
    std::vector<TeamScore> team_score_;

    float32_t time_left_;
};

#endif
