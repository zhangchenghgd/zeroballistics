
#ifndef RANKING_MATCH_EVENTS_SOCCER_INCLUDED
#define RANKING_MATCH_EVENTS_SOCCER_INCLUDED


#include "RankingMatchEvents.h"


namespace network
{

namespace ranking
{


//------------------------------------------------------------------------------
enum MATCH_EVENTS_SOCCER
{
    MES_GOAL        = 6,
    MES_GOAL_ASSIST = 7,
    MES_OWN_GOAL    = 8
};


//------------------------------------------------------------------------------
class MatchEventsSoccerConsumer : public MatchEventsConsumer
{
 public:
    virtual void onGoal      (unsigned timestamp, uint32_t id) = 0;
    virtual void onGoalAssist(unsigned timestamp, uint32_t id) = 0;
    virtual void onOwnGoal   (unsigned timestamp, uint32_t id) = 0;
};




//------------------------------------------------------------------------------
class MatchEventsSoccer : public MatchEvents
{
 public:
    MatchEventsSoccer();
    virtual ~MatchEventsSoccer();

    void setBallPossessionPercentage(float perc);
    float getBallPossessionPercentage() const;

    void setGameDistribution(float perc);
    float getGameDistribution() const;

    void setFinalGoals(uint8_t   blue, uint8_t   red);
    void getFinalGoals(uint8_t & blue, uint8_t & red) const;

    void logGoal      (uint32_t user_id);
    void logGoalAssist(uint32_t user_id);
    void logOwnGoal   (uint32_t user_id);
    
    static const std::string getGameName();

    virtual void writeStateToBitstream (RakNet::BitStream & stream) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream);

 protected:


    virtual void parse(uint8_t event, unsigned timestamp, MatchEventsConsumer * c) const;

    float32_t ball_possession_;
    float32_t game_distribution_;

    uint8_t goals_blue_;
    uint8_t goals_red_;
};



 
}

}

#endif
