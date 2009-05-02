#ifndef TEAM_SOCCER_INCLUDED
#define TEAM_SOCCER_INCLUDED


#include "Team.h"




const unsigned NUM_TEAMS_SOCCER = 2;

const std::string TEAM_CONFIG_SOCCER[] = { "team_soccer_blue", "team_soccer_red" };

const TEAM_ID TEAM_ID_BLUE = 0;
const TEAM_ID TEAM_ID_RED  = 1;



//------------------------------------------------------------------------------
class TeamSoccer : public Team
{
 public:

    TeamSoccer();

    void resetBallStats();
    
    void incBallPossession();
    void incBallInHalf();
    
    unsigned getBallPossession() const;
    unsigned getBallInHalf() const;
    
 protected:
    /// Intervals in which this team possessed the ball.
    unsigned ball_possession_;
    /// Intervals in which the ball was in this team's half of the field.
    unsigned ball_in_half_;
};


#endif
