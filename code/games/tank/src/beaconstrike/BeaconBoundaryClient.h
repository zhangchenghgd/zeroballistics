
#ifndef TANK_BEACON_BOUNDARY_CLIENT_INCLUDED
#define TANK_BEACON_BOUNDARY_CLIENT_INCLUDED


#include <vector>
#include <map>
#include <set>

#include <osg/ref_ptr>



#include "BeaconBoundary.h"


#include "Datatypes.h"


namespace osg
{
    class Geode;
    class Group;
    class Geometry;
    class Node;
}

class Beacon;
class BeaconConnection;
struct BoundarySegment;

//------------------------------------------------------------------------------
struct BeaconInfo
{
    /// Boundary segments corresponding to this beacon. geometry is
    /// owned by BeaconBoundaryClient::geode_.
    std::vector<osg::Geometry*> geometry_;
    std::vector<osg::Geometry*> outline_geometry_;

    /// Neighbors of this beacon after the last update call. Neighbors
    /// potentially affect the portion of the boundary this beacons is
    /// responsible for.
    std::vector<Beacon*> neighbor_;
};




//------------------------------------------------------------------------------
struct WorldIntersectionPoint
{
    WorldIntersectionPoint(const Vector & p, const Vector & n) : pos_(p),n_(n) {}
    Vector pos_;
    Vector n_;
};



//------------------------------------------------------------------------------
/**
 *  Creates a boundary visualization based on the active status of the
 *  beacons.
 */
class BeaconBoundaryClient : public BeaconBoundary
{
 public:

    BeaconBoundaryClient(physics::OdeCollisionSpace * world_space,
                         const std::string & tex_file);
    virtual ~BeaconBoundaryClient();

    void setDrawLosHints(bool b);
    void setUpdateOutline(bool b);
    
    void update();


    void updateBeaconConnections(std::vector<std::pair<Beacon*, Beacon*> > & connections);


    osg::Node * getOutlineNode() const;
    
 protected:
    
    virtual void onBeaconAdded  (Beacon * beacon);
    virtual void onBeaconDeleted(Beacon * beacon);

    void onBeaconDeployedChanged     (Observable* observable, unsigned event);
    
    
    void initOsgStates(const std::string & tex_file);


    void calculateIntervals(const Beacon * beacon,
                            std::vector<std::pair<float,float> > & intervals);
    
    void createSegments(const Beacon * beacon,
                        std::vector<std::pair<float,float> > & intervals,
                        std::vector<BoundarySegment> & segments,
                        std::vector<std::vector<Vector2d> > & outline);
    
    void updateSegments(std::vector<unsigned> & active_segments,
                        std::vector<BoundarySegment> & segments);

    void createDrawables(const Beacon * beacon,
                         const std::vector<BoundarySegment> & segments,
                         const std::vector<std::vector<Vector2d> > & outline);
                         
    
    bool worldRayCollisionCallback    (const physics::CollisionInfo & info);
    bool beaconRadiusCollisionCallback(const physics::CollisionInfo & info);

    void removeBeacon (Beacon * b);
    void clearGeometry(BeaconInfo & info);
    
    /// Used for checking a single vertical ray for world collisions.    
    std::vector<WorldIntersectionPoint> world_intersection_points_; 


    std::map<const Beacon*, BeaconInfo> beacon_info_; ///< Additional info for every beacon in superclass beacon_ array.
    
    std::set<Beacon*> newly_deployed_beacon_;    
    std::set<Beacon*> dirty_beacon_;
    
    std::vector<BeaconInfo> deactivated_beacon_;
    

    osg::ref_ptr<osg::Geode> geode_; ///< 3d beacon boundary segments are below this geode.
    osg::ref_ptr<osg::Group> connection_group_; ///< Beacon Energy connections are in this group.
    

    osg::ref_ptr<osg::Geode> outline_geode_;    ///< Outline used for minimap is below this geode.

    bool update_outline_;

    bool draw_los_hints_;
};


#endif

