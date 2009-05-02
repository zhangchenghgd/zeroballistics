
#ifndef TANK_BEACON_BOUNDARY_SERVER_INCLUDED
#define TANK_BEACON_BOUNDARY_SERVER_INCLUDED

#include <vector>
#include <set>

#include "Vector2d.h"
#include "physics/OdeCollision.h"
#include "BeaconBoundary.h"
#include "TeamBs.h"

namespace physics
{
    class OdeCollisionSpace;
    struct CollisionInfo;
}

class Beacon;
class Observable;


//------------------------------------------------------------------------------
/**
 *  Determines the active status of beacons.
 */
class BeaconBoundaryServer : public BeaconBoundary
{
 public:
    BeaconBoundaryServer(physics::OdeCollisionSpace * world_space);
    virtual ~BeaconBoundaryServer();

    void update();

    const bool * isInsideArea(const physics::OdeGeom * geom);

    const std::vector<std::pair<uint16_t, uint16_t> > & getConnectedBeacons() const;
    
 protected:

    virtual void onBeaconAdded(Beacon * beacon);
    
    bool beaconBodyCollisionCallback  (const physics::CollisionInfo & info);

    bool areaTestCollisionCallback  (const physics::CollisionInfo & info);

    void setBeaconInsideRadius(Beacon * b,
                               std::set<Beacon*> & beacons_inside);
    
    bool is_inside_[NUM_TEAMS_BS]; ///< Used for isInsideArea queries

    std::vector<std::pair<uint16_t, uint16_t> > connected_beacons_; ///< Caches connected beacons between calls to update().
};

#endif
