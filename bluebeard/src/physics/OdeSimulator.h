

#ifndef BLUEBEARD_ODE_SIMULATOR_INCLUDED
#define BLUEBEARD_ODE_SIMULATOR_INCLUDED

#include <list>

#include <ode/ode.h>


#include "Datatypes.h"
#include "physics/OdeCollision.h"
#include "physics/OdeCollisionSpace.h"

namespace physics
{

class OdeRigidBody;
class OdeGeom;


 
//------------------------------------------------------------------------------
class OdeSimulator
{
 public:
    OdeSimulator(const std::string & name);
    virtual ~OdeSimulator();

    void frameMove(float dt);

    void renderGeoms() const;

    OdeRigidBody * instantiate(const OdeRigidBody * blueprint);
    void removeBody(const OdeRigidBody * body);

    void addContinuousGeom(OdeContinuousGeom * g);
    void removeContinuousGeom(OdeContinuousGeom * g);

    void enableCategoryCollisions(unsigned cat1, unsigned cat2, bool b);
    void enableCategoryCollisions(unsigned cat, bool b);
    uint32_t getCollideFlags(unsigned category) const;


    void addContactJoint(const dContact & contact, const OdeRigidBody * body1, const OdeRigidBody * body2);

    OdeCollisionSpace * getStaticSpace();
    OdeCollisionSpace * getActorSpace();

    dWorldID getWorldId() const;

    bool isEmpty() const;

    void dumpContents() const;
    
 protected:
   
    void handleBodyVelocities();

    void handleContinousGeoms();

    std::string name_;
    
    std::list<OdeRigidBody*> body_;


    uint32_t category_collide_flag_[32]; ///< For each category,
                                         ///stores categories it
                                         ///collides with. This
                                         ///information is used when
                                         ///setting a body's contact
                                         ///group.

    std::vector<OdeContinuousGeom*> cont_geom_; ///< Continuous geoms
                                                ///need their
                                                ///frameMove to be
                                                ///called
    
    dWorldID world_id_;
    OdeCollisionSpace * static_space_;
    OdeCollisionSpace * actor_space_;

    dJointGroupID contact_group_id_;

    // ---------- Cached Params ----------
    float lin_dampening_;
    float ang_dampening_;

    static unsigned instance_count_;
};

    
} // namespace physics


#endif
