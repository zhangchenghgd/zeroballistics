
#ifndef TANK_BEACON_BOUNDARY_INCLUDED
#define TANK_BEACON_BOUNDARY_INCLUDED

#include <vector>

#include "Vector2d.h"
#include "physics/OdeCollision.h"
#include "RegisteredFpGroup.h"

namespace physics
{
    class OdeCollisionSpace;
    struct CollisionInfo;
}

class Beacon;
class Observable;


//------------------------------------------------------------------------------
class BeaconBoundary
{
 public:

    typedef std::vector<Beacon*> BeaconContainer;
    
    BeaconBoundary(physics::OdeCollisionSpace * world_space);
    virtual ~BeaconBoundary();

    void addBeacon   (Beacon * beacon);
    void deleteBeacon(Beacon * beacon);

    BeaconContainer & getBeacons();
    const BeaconContainer & getBeacons() const;

    physics::OdeCollisionSpace * getWorldSpace();
    physics::OdeCollisionSpace * getBeaconSpace();
    
 protected:

    virtual void onBeaconAdded  (Beacon * beacon) {}
    virtual void onBeaconDeleted(Beacon * beacon) {}

    bool checkLos(const Beacon * b1, const Vector & pos);
    bool losCollisionCallback(const physics::CollisionInfo & info);
    
    physics::OdeCollisionSpace * world_space_;
    std::auto_ptr<physics::OdeCollisionSpace> beacon_space_;
    
    BeaconContainer beacon_;

    bool is_los_; ///< Used for line of sight test

    RegisteredFpGroup fp_group_;
};

#endif
