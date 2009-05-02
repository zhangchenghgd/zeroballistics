#ifndef TANKGAME_TANKMINE_INCLUDED
#define TANKGAME_TANKMINE_INCLUDED

#include <string>

#include "RigidBody.h"
#include "GameLogicServerCommon.h"

namespace physics
{
    struct CollisionInfo;
}


//------------------------------------------------------------------------------
class TankMine : public RigidBody
{ 
 public:
    
    GAME_OBJECT_IMPL(TankMine);
    virtual ~TankMine();

    virtual void frameMove(float dt);

    // used for object creation
    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);

    static void setGameLogicServer(GameLogicServerCommon * logic_server); 

    bool collisionCallback(const physics::CollisionInfo & info);

    void setSection(const std::string & section);
    void setTeamId(TEAM_ID tid);
    TEAM_ID getTeamId() const;
    void setLifetime(float lifetime);

    void explode(const SystemAddress & hit_player,
                 WEAPON_HIT_TYPE feedback_type);
    
 protected:

    TankMine();

    static GameLogicServerCommon * game_logic_server_;

    float lifetime_;

    TEAM_ID team_id_;

    std::string section_;
};


#endif
