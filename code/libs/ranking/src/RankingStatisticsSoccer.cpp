
#include "RankingStatisticsSoccer.h"


#include <raknet/BitStream.h>


namespace network
{
namespace ranking
{


//------------------------------------------------------------------------------
StatisticsSoccer::StatisticsSoccer()
{
}
    


//------------------------------------------------------------------------------
void StatisticsSoccer::writeToBitstream (RakNet::BitStream & stream) const
{
    stream.Write(goals_blue_);
    stream.Write(goals_red_);
    stream.Write(ball_possession_);
    stream.Write(game_distribution_);

    stream.Write((uint32_t)score_stats_.size());

    for (std::map<uint32_t, ScoreStats>::const_iterator it = score_stats_.begin();
         it != score_stats_.end();
         ++it)
    {
        stream.Write((uint32_t)it->first);
        stream.Write((float32_t)it->second.new_score_);
        stream.Write((float32_t)it->second.delta_);
    }
}



//------------------------------------------------------------------------------
void StatisticsSoccer::readFromBitstream(RakNet::BitStream & stream)
{
    stream.Read(goals_blue_);
    stream.Read(goals_red_);
    stream.Read(ball_possession_);
    stream.Read(game_distribution_);

    uint32_t num, id;
    float32_t delta, new_score;

    stream.Read(num);

    for (unsigned i=0; i<num; ++i)
    {
        stream.Read(id);
        stream.Read(new_score);
        stream.Read(delta);
        score_stats_[id] = ScoreStats(new_score, delta);
    }
}    



}
}
