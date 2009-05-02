
#include "BeaconBoundaryServer.h"


#include "Log.h"
#include "Beacon.h"
#include "physics/OdeCollisionSpace.h"
#include "Profiler.h"

//------------------------------------------------------------------------------
BeaconBoundaryServer::BeaconBoundaryServer(physics::OdeCollisionSpace * world_space) :
    BeaconBoundary(world_space)
{
    s_log << Log::debug('i')
          << "BeaconBoundaryServer constructor\n";
}


//------------------------------------------------------------------------------
BeaconBoundaryServer::~BeaconBoundaryServer()
{
    s_log << Log::debug('d')
          << "BeaconBoundaryServer destructor\n";
}





//------------------------------------------------------------------------------
void BeaconBoundaryServer::update()
{
    PROFILE(BeaconBoundaryServer::update);
    s_log << Log::debug('l')
          << "BeaconBoundaryServer::update\n";

    // Recalculate neighborhood information
    for (BeaconContainer::iterator it = beacon_.begin();
         it != beacon_.end();
         ++it)
    {
        (*it)->clearNeighbors();
        (*it)->placeGeoms();
    }
    beacon_space_->collide();


    // Activate fixed beacon, use activation spreading information for
    // beacon connections.
    connected_beacons_.clear();
    std::set<Beacon*> active_beacons;
    for (BeaconContainer::iterator it = beacon_.begin();
         it != beacon_.end();
         ++it)
    {
        if ((*it)->getState() == BS_FIXED)
        {
            setBeaconInsideRadius(*it, active_beacons);
        }
    }

    // Now traverse all beacons which have not been set to active and
    // set them to inactive.
    for (unsigned i=0; i<beacon_.size(); ++i)
    {
        if (active_beacons.find(beacon_[i]) == active_beacons.end())
        {
            beacon_[i]->setInsideRadius(false);
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Returns an array of bools indicating whether the geom is in each
 *  team's area of influence.
 */
const bool * BeaconBoundaryServer::isInsideArea(const physics::OdeGeom * geom)
{
    assert(geom);

    memset(is_inside_, 0, sizeof(is_inside_));

    beacon_space_->collide(geom,
                           physics::CollisionCallback(this, &BeaconBoundaryServer::areaTestCollisionCallback));

    return is_inside_;
}


//------------------------------------------------------------------------------
const std::vector<std::pair<uint16_t, uint16_t> > & BeaconBoundaryServer::getConnectedBeacons() const
{
    return connected_beacons_;
}


//------------------------------------------------------------------------------
void BeaconBoundaryServer::onBeaconAdded(Beacon * beacon)
{
    beacon->getBodyGeom()->setCollisionCallback(
        physics::CollisionCallback(this, &BeaconBoundaryServer::beaconBodyCollisionCallback) );
}

//------------------------------------------------------------------------------
/**
 *  This keeps track of beacon neighboring state. If a beacon radius
 *  collides with a beacon body, mark the two as neighbors.
 */
bool BeaconBoundaryServer::beaconBodyCollisionCallback(const physics::CollisionInfo & info)
{
    if (info.type_ == physics::CT_STOP) return false;

    Beacon * beacon       = (Beacon*)                         info.this_geom_ ->getUserData();
    Beacon * other_beacon = dynamic_cast<Beacon*>((RigidBody*)info.other_geom_->getUserData());

    // Count collisions only once
    if (beacon < other_beacon) return false;

    if (beacon->getTeamId() != other_beacon->getTeamId()) return false;

    // At least one beacon must be deployed
    if (!beacon->isDeployed() && !other_beacon->isDeployed()) return false;

    // bail if colliding against other body
    if (other_beacon->getBodyGeom() == info.other_geom_) return false;
    
    // Beacons of same team: check whether LOS is given. If so,
    // add to neighboring list.
    if (checkLos(beacon, other_beacon->getPosition()))
    {        
        beacon->addNeighbor(other_beacon);
        other_beacon->addNeighbor(beacon);
    }
    
    return false;
}


//------------------------------------------------------------------------------
/**
 *  Callback for testing whether position is inside active area. If an
 *  active beacon is intersected, set is_inside_ to true.
 */
bool BeaconBoundaryServer::areaTestCollisionCallback(const physics::CollisionInfo & info)
{
    Beacon * beacon = dynamic_cast<Beacon*>((RigidBody*)info.other_geom_->getUserData());
    assert(beacon);

    if (beacon->isDeployed()) is_inside_[beacon->getTeamId()] = true;
    
    return false;
}


//------------------------------------------------------------------------------
void BeaconBoundaryServer::setBeaconInsideRadius(Beacon * b,
                                                 std::set<Beacon*> & beacons_inside)
{
    b->setInsideRadius(true);
    beacons_inside.insert(b);

    // don't use e.g. carried beacons to maintain beacon connections.
    if (!b->isDeployed()) return;
    
    // Enforce breadth-first traversal in order to have nice connections
    std::vector<Beacon*> new_inside;
    for (unsigned i=0; i<b->getNeighbor().size(); ++i)
    {
        Beacon * cur_neighbor = b->getNeighbor()[i];
        
        if (beacons_inside.find(cur_neighbor) == beacons_inside.end())
        {
            new_inside.push_back(cur_neighbor);
            beacons_inside.insert(cur_neighbor);
        }
    }
    
    for (unsigned i=0; i<new_inside.size(); ++i)
    {
        setBeaconInsideRadius(new_inside[i],
                              beacons_inside);

        // Only establish connections between deployed beacons
        if (b->isDeployed() && new_inside[i]->isDeployed())
        {
            connected_beacons_.push_back(std::make_pair(b->getId(), new_inside[i]->getId()));
        }
    }
}
