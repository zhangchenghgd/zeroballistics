
#include "TeamSoccer.h"


//------------------------------------------------------------------------------
TeamSoccer::TeamSoccer() :
    ball_possession_(0),
    ball_in_half_(0)
{
}


//------------------------------------------------------------------------------
void TeamSoccer::resetBallStats()
{
    ball_possession_ = 0;
    ball_in_half_    = 0;
}


//------------------------------------------------------------------------------
void TeamSoccer::incBallPossession()
{
    ++ball_possession_;
}

//------------------------------------------------------------------------------
void TeamSoccer::incBallInHalf()
{
    ++ball_in_half_;
}


//------------------------------------------------------------------------------
unsigned TeamSoccer::getBallPossession() const
{
    return ball_possession_;
}

//------------------------------------------------------------------------------
unsigned TeamSoccer::getBallInHalf() const
{
    return ball_in_half_;
}

