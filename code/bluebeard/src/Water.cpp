
#include "Water.h"

#include <raknet/BitStream.h>


#include "physics/OdeCollision.h"
#include "physics/OdeRigidBody.h"


//------------------------------------------------------------------------------
Water::~Water()
{
}

//------------------------------------------------------------------------------
Water::Water() 
{
}

//------------------------------------------------------------------------------
void Water::init()
{
    std::vector<physics::OdeGeom*> & geom = target_object_->getGeoms();

    for (unsigned g=0; g<geom.size(); ++g)
    {
        geom[g]->setCollisionCallback(physics::CollisionCallback(this, &Water::collisionCallback));
        geom[g]->setName(WATER_GEOM_NAME);
    }
}

//------------------------------------------------------------------------------
/**
 *  Water plane should never collide with anything.
 */
bool Water::collisionCallback(const physics::CollisionInfo & info)
{
    physics::OdeRigidBody * body = info.other_geom_->getBody();
    if (body) body->setBelowWater(info.type_ != physics::CT_STOP);
    
    return false;
}
