
#include "Water.h"

#include <raknet/BitStream.h>


#include "physics/OdeCollision.h"


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
    return false;
}
