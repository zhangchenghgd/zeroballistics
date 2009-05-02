
#ifndef RANKING_STATISTICS_INCLUDED
#define RANKING_STATISTICS_INCLUDED


namespace RakNet
{
    class BitStream;
}

namespace network
{
namespace ranking
{


//------------------------------------------------------------------------------
/**
 *  Interface for game statistics which are displayed on the client's
 *  screen after the match.
 */
class Statistics
{
 public:
    virtual void writeToBitstream (RakNet::BitStream & stream) const = 0;
    virtual void readFromBitstream(RakNet::BitStream & stream)       = 0;
};

    
}
} 


#endif
