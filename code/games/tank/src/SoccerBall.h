#ifndef FMS_SOCCER_BALL_INCLUDED
#define FMS_SOCCER_BALL_INCLUDED

#include <string>

#include "RigidBody.h"

namespace physics
{
    struct CollisionInfo;
}


class GameState;


//------------------------------------------------------------------------------
class SoccerBall : public RigidBody
{ 
 public:
    
    GAME_OBJECT_IMPL(SoccerBall);
    SoccerBall();
    virtual ~SoccerBall();

    void setGameState(GameState * game_state);
    void sendPosRelativeTo(uint16_t object_id);
    void setRelPos(const Vector & pos);

    uint16_t getRelObjectId() const;

    virtual void frameMove(float dt);

    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp);
    
 protected:

    uint16_t rel_object_id_;
    Vector rel_pos_;
    GameState * game_state_;
};


#endif
