
#ifndef FMS_SOCCER_BALL_VISUAL_INCLUDED
#define FMS_SOCCER_BALL_VISUAL_INCLUDED


#include "RigidBodyVisual.h"


//------------------------------------------------------------------------------
class SoccerBallVisual : public RigidBodyVisual
{
 public:
    virtual ~SoccerBallVisual();
    VISUAL_IMPL(SoccerBallVisual);
};

#endif
