#ifndef TANK_WATER_INCLUDED
#define TANK_WATER_INCLUDED

#include "RigidBody.h"


const std::string WATER_GEOM_NAME = "water";


//------------------------------------------------------------------------------
class Water : public RigidBody
{
 public:
    virtual ~Water();

    GAME_OBJECT_IMPL(Water);
    
 protected:
    Water();

    virtual void init();
    
    bool collisionCallback(const physics::CollisionInfo & info);
};


#endif
