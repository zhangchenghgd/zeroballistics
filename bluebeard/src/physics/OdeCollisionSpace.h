

#ifndef BLUEBEARD_ODE_COLLISIONSPACE_INCLUDED
#define BLUEBEARD_ODE_COLLISIONSPACE_INCLUDED

#include <set>
#include <vector>
#include <stack>

#include <ode/ode.h>

#include "Datatypes.h"
#include "physics/OdeCollision.h"


namespace physics
{

class OdeRigidBody;
class OdeGeom;

 
//------------------------------------------------------------------------------
class OdeCollisionSpace
{
 public:
    OdeCollisionSpace(const std::string & name, bool generate_start_stop_events);
    virtual ~OdeCollisionSpace();


    void collide(OdeCollisionSpace * other_space = NULL, bool check_for_stopped_collisions = true);
    void collide(const OdeGeom * geom, CollisionCallback callback);
    void collideRayMultiple(OdeRayGeom * ray, CollisionCallback callback);

    void spaceCollideCallback(dGeomID o1, dGeomID o2);

    void disableGeom(const OdeGeom * body_geom);

    dSpaceID getId() const;

    const std::string & getName() const;
    
    void dumpContents() const;

    void createQuadtreeSpace(const Vector & center, const Vector & extents, unsigned depth);
    
 protected:    

    void handlePotentialCollisions();
    void handlePotentialCollisionsSingle(const OdeGeom * single_geom, CollisionCallback callback);
    
    bool addCollidingGeoms   (OdeGeom * geom1, OdeGeom * geom2);
    bool handleCollisionEvent(OdeGeom * geom1, OdeGeom * geom2, const dContactGeom & contact_geom);


    void generateStoppedCollisionEvent(OdeGeom * geom1, OdeGeom * geom2);
    void checkForStoppedCollisions();


    unsigned mergeContacts(unsigned num_contacts,
                           dContactGeom * in,
                           dContactGeom * out);

    std::string name_;

    /// We don't want to call user callbacks in the dSpaceCollide
    /// callback function because then we wouldn't be able to
    /// add/remove geoms. Instead store potentially colliding geoms in
    /// this array and traverse it afterwards.
    ///
    /// Use a stack of vectors to allow nested collide calls.
    std::stack<std::vector<std::pair<dGeomID, dGeomID> > > potentially_colliding_geoms_; 
    
    std::set<std::pair<OdeGeom*, OdeGeom*> > prev_colliding_geoms_; ///< Geoms which collided last frame.
    std::set<std::pair<OdeGeom*, OdeGeom*> > cur_colliding_geoms_;  ///< Geoms colliding this frame. 


    ///Geoms disabled during a call to collide(). CT_IN_PROGRESS / CT_START
    ///events must not be generated for those geoms after the CT_STOP
    ///event, so ignore them if they are still pending in
    ///potentially_colliding_geoms_.
    std::vector<dGeomID> disabled_geom_; 
    /// Only remember disabled geoms while handling collisions. This
    /// avoids problems when the same geoms are re-added directly
    /// after removal (e.g. at a level restart)
    bool remember_disabled_geoms_;


    
    bool generate_start_stop_events_;

    
    dSpaceID space_id_;
    bool is_quadtree_;
};

    
} // namespace physics


#endif
