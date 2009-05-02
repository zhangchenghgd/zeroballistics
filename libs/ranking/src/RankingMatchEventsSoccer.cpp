
#include "RankingMatchEventsSoccer.h"

#include "NetworkUtils.h"

namespace network
{
namespace ranking
{

//------------------------------------------------------------------------------
MatchEventsSoccer::MatchEventsSoccer() :
    ball_possession_(0.5f),
    game_distribution_(0.5f),
    goals_blue_(0),
    goals_red_ (0)
{
}



//------------------------------------------------------------------------------
MatchEventsSoccer::~MatchEventsSoccer()
{
}

//------------------------------------------------------------------------------
void MatchEventsSoccer::setBallPossessionPercentage(float perc)
{
    ball_possession_ = perc;
}

//------------------------------------------------------------------------------
float MatchEventsSoccer::getBallPossessionPercentage() const
{
    return ball_possession_;
}


//------------------------------------------------------------------------------
void MatchEventsSoccer::setGameDistribution(float perc)
{
    game_distribution_ = perc;
}

//------------------------------------------------------------------------------
float MatchEventsSoccer::getGameDistribution() const
{
    return game_distribution_;
}



//------------------------------------------------------------------------------
void MatchEventsSoccer::setFinalGoals(uint8_t   blue, uint8_t   red)
{
    goals_blue_ = blue;
    goals_red_  = red;
}


//------------------------------------------------------------------------------
void MatchEventsSoccer::getFinalGoals(uint8_t & blue, uint8_t & red) const
{
    blue = goals_blue_;
    red  = goals_red_;
}


//------------------------------------------------------------------------------
void MatchEventsSoccer::logGoal(uint32_t user_id)
{
    logEvent(MES_GOAL);
    event_log_.Write(user_id);
}



//------------------------------------------------------------------------------
void MatchEventsSoccer::logGoalAssist(uint32_t user_id)
{
    logEvent(MES_GOAL_ASSIST);
    event_log_.Write(user_id);
}

//------------------------------------------------------------------------------
void MatchEventsSoccer::logOwnGoal(uint32_t user_id)
{
    logEvent(MES_OWN_GOAL);
    event_log_.Write(user_id);
}




//------------------------------------------------------------------------------
const std::string MatchEventsSoccer::getGameName()
{
    return std::string("Soccer");
}



//------------------------------------------------------------------------------
void MatchEventsSoccer::writeStateToBitstream (RakNet::BitStream & stream) const
{
    writeToBitstream(stream, getGameName());
    
    stream.Write(ball_possession_);
    stream.Write(game_distribution_);

    stream.Write(goals_blue_);
    stream.Write(goals_red_);

    MatchEvents::writeStateToBitstream(stream);
}


//------------------------------------------------------------------------------
void MatchEventsSoccer::readStateFromBitstream(RakNet::BitStream & stream)
{
    std::string dummy_name;

    readFromBitstream(stream, dummy_name);

    stream.Read(ball_possession_);
    stream.Read(game_distribution_);

    stream.Read(goals_blue_);
    stream.Read(goals_red_);
    
    MatchEvents::readStateFromBitstream(stream);
}



//------------------------------------------------------------------------------
void MatchEventsSoccer::parse(uint8_t event, unsigned timestamp, MatchEventsConsumer * c) const
{
    MatchEventsSoccerConsumer * consumer = (MatchEventsSoccerConsumer*)c;
    uint32_t user_id;
    
    switch (event)
    {
    case MES_GOAL:
        if (readAndValidateUserId(user_id, event))
        {
            consumer->onGoal(timestamp, user_id);
        }
        break;

    case MES_GOAL_ASSIST:
        if (readAndValidateUserId(user_id, event))
        {
            consumer->onGoalAssist(timestamp, user_id);
        }
        break;

    case MES_OWN_GOAL:
        if (readAndValidateUserId(user_id, event))
        {
            consumer->onOwnGoal(timestamp, user_id);
        }
        break;

    default:
        MatchEvents::parse(event, timestamp, c);
    }
}



}
}
