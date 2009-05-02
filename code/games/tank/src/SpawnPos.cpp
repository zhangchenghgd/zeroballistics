

#include "SpawnPos.h"


#include "physics/OdeCollision.h"
#include "physics/OdeRigidBody.h"
#include "physics/OdeCollisionSpace.h"


#include "TerrainData.h"



//------------------------------------------------------------------------------
/**
 *  \param geom A geom which has been detached from its body. SpawnPos
 *  claims ownership.
 *
 *  \param terrain Used to align the spawning position with the
 *  underlying terrain.
 */
SpawnPos::SpawnPos(physics::OdeGeom * geom,
                   const terrain::TerrainData * terrain,
                   const std::string & team_name) :
    geom_(geom),
    occupied_(false),
    space_(NULL),
    team_name_(team_name)
{
    assert(geom && geom->getBody() == NULL);
    space_ = geom->getSpace();
    assert(space_);
    geom->setSpace(NULL);

    if (terrain)
    {
        // Align spawn pos with terrain
        Matrix spawn_pos = geom->getTransform();

        float h;
        Vector n;
        terrain->getHeightAndNormal(spawn_pos.getTranslation().x_,
                                    spawn_pos.getTranslation().z_,
                                    h,n);
    
        spawn_pos.getTranslation().y_ = h;
    
        spawn_pos.loadOrientation(n, spawn_pos.getZ());
        Matrix rot(true);
        rot.loadCanonicalRotation(PI*0.5f, 0);
    
        spawn_pos = spawn_pos.mult3x3(rot);

        geom->setTransform(spawn_pos);
    }
}


//------------------------------------------------------------------------------
SpawnPos::~SpawnPos()
{
}





//------------------------------------------------------------------------------
bool SpawnPos::isOccupied() const
{
    occupied_ = false;
    space_->collide(geom_.get(),
                    physics::CollisionCallback(this, &SpawnPos::collisionCallback));
    return occupied_;
}


//------------------------------------------------------------------------------
Matrix SpawnPos::getTransform() const
{
    return geom_->getTransform();
}

//------------------------------------------------------------------------------
void SpawnPos::setTeamName(const std::string & team_name)
{
    team_name_ = team_name;
}

//------------------------------------------------------------------------------
const std::string & SpawnPos::getTeamName() const
{
    return team_name_;
}

//------------------------------------------------------------------------------
/**
 *  Only dynamic objects with a sensor geom can block a spawning
 *  position.
 */
bool SpawnPos::collisionCallback(const physics::CollisionInfo & info) const
{
    assert(info.type_ == physics::CT_SINGLE);

    if (info.other_geom_->getBody() == NULL)     return false;
    if (info.other_geom_->getBody()->isStatic()) return false;

    occupied_ = true;
    
    return false;
}
