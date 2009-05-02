
#ifndef RANKING_STATISTICS_SOCCER_INCLUDED
#define RANKING_STATISTICS_SOCCER_INCLUDED


#include <map>



#include "RankingStatistics.h"
#include "Datatypes.h"

namespace RakNet
{
    class BitStream;
}

namespace network
{
namespace ranking
{

struct ScoreStats
{
    ScoreStats() : new_score_(0), delta_(0) {}
    ScoreStats(float n, float d) : new_score_(n), delta_(d) {}
    
    float new_score_;
    float delta_;
};
    

//------------------------------------------------------------------------------
/**
 *  Contains everything a player wants to know about a match after
 *  playing.
 */   
class StatisticsSoccer : public Statistics
{
 public:
    StatisticsSoccer();

    virtual void writeToBitstream (RakNet::BitStream & stream) const;
    virtual void readFromBitstream(RakNet::BitStream & stream);

    
    uint8_t goals_blue_;
    uint8_t goals_red_;

    float32_t ball_possession_;
    float32_t game_distribution_;

    std::map<unsigned, ScoreStats> score_stats_;    
};

    
}
} 


#endif
