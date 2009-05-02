
#include "BeaconBoundary.h"


#include "Log.h"
#include "Beacon.h"
#include "physics/OdeCollisionSpace.h"


//------------------------------------------------------------------------------
BeaconBoundary::BeaconBoundary(physics::OdeCollisionSpace * world_space) :
    world_space_(world_space),
    beacon_space_(new physics::OdeCollisionSpace("beacon", false))
{
}


//------------------------------------------------------------------------------
BeaconBoundary::~BeaconBoundary()
{
}

//------------------------------------------------------------------------------
void BeaconBoundary::addBeacon(Beacon * beacon)
{
    s_log << Log::debug('l')
          << "Adding beacon "
          << *beacon
          << " to beacon boundary.\n";
    
    beacon_.push_back(beacon);

    beacon->getRadiusGeom()->setSpace(beacon_space_.get());
    beacon->getBodyGeom()  ->setSpace(beacon_space_.get());

    onBeaconAdded(beacon);
}


//------------------------------------------------------------------------------
void BeaconBoundary::deleteBeacon(Beacon * beacon)
{
    s_log << Log::debug('l')
          << "BeaconBoundary::deleteBeacon with "
          << (*(Beacon*)beacon)
          << "\n";

    onBeaconDeleted(beacon);        
    
    for (BeaconContainer::iterator it=beacon_.begin();
         it != beacon_.end();
         ++it)
    {
        if (*it == beacon)
        {
            beacon_.erase(it);
            return;
        }
    }

    assert (false);
}

//------------------------------------------------------------------------------
BeaconBoundary::BeaconContainer & BeaconBoundary::getBeacons()
{
    return beacon_;
}

//------------------------------------------------------------------------------
const BeaconBoundary::BeaconContainer & BeaconBoundary::getBeacons() const
{
    return beacon_;
}



//------------------------------------------------------------------------------
physics::OdeCollisionSpace * BeaconBoundary::getWorldSpace()
{
    return world_space_;
}

//------------------------------------------------------------------------------
physics::OdeCollisionSpace * BeaconBoundary::getBeaconSpace()
{
    return beacon_space_.get();
}

//------------------------------------------------------------------------------
/**
 *  Check whether the given beacon can see the specified position.
 */
bool BeaconBoundary::checkLos(const Beacon * b1, const Vector & pos)
{
    is_los_ = true;

    float beacon_body_radius = b1->getBodyGeom()->getRadius();
    
    Vector dir = pos - b1->getPosition();
    float len = dir.length() - 2*beacon_body_radius;

    // If beacons are too close assume LOS
    if (len > EPSILON)
    {
        dir /= len;

        physics::OdeRayGeom ray(len);
        ray.set(b1->getPosition() + beacon_body_radius*dir, dir);

        // Sets is_los_ to false in case of collision
        world_space_->collide(
            &ray, physics::CollisionCallback(this, &BeaconBoundary::losCollisionCallback));
    }

    return is_los_;
}


//------------------------------------------------------------------------------
bool BeaconBoundary::losCollisionCallback(const physics::CollisionInfo & info)
{
    if (info.other_geom_->isSensor())
    {
        s_log << Log::error
              << "BeaconBoundary::losCollisionCallback: collision with sensor "
              << info.other_geom_->getName()
              << "\n";
        return false;
    }

    if (!info.other_geom_->isStatic()) return false;

    RigidBody * other_body = (RigidBody*)info.other_geom_->getUserData();
    if (other_body)
    {
        // Ignore beacons for LOS
        if (other_body->getType() == "Beacon") return false;
        // Ignore water plane for LOS tests
        if (other_body->getType() == "Water")  return false;
    }
    
//     s_log << Log::debug('B')
//           << info.other_geom_->getName()
//           << " is blocking LOS\n";
    is_los_ = false;

    return false;
}
