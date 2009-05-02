

#ifndef TANK_SPAWN_POS_INCLUDED
#define TANK_SPAWN_POS_INCLUDED


#include "Matrix.h"

namespace terrain
{
    class TerrainData;
}

namespace physics
{
    class OdeGeom;
    class OdeCollisionSpace;
    struct CollisionInfo;
}


//------------------------------------------------------------------------------
class SpawnPos
{
 public:
    SpawnPos(physics::OdeGeom * geom,
             const terrain::TerrainData * terrain,
             const std::string & team_name);
    ~SpawnPos();
    
    bool isOccupied() const;
    Matrix getTransform() const;

    void setTeamName(const std::string & team_name);
    const std::string & getTeamName() const;

 protected:

    bool collisionCallback(const physics::CollisionInfo & info) const;

    std::auto_ptr<physics::OdeGeom> geom_;
    mutable bool occupied_; ///< Just to store result from callback.
    physics::OdeCollisionSpace * space_;

    std::string team_name_;
};


#endif
