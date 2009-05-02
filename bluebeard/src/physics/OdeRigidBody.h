

#ifndef BLUEBEARD_ODE_RIGID_BODY_INCLUDED
#define BLUEBEARD_ODE_RIGID_BODY_INCLUDED

#include "OdeCollision.h"

namespace physics
{


class OdeSimulator;

//------------------------------------------------------------------------------
enum ENABLE_TYPE
{
    ET_BOTH,
    ET_SENSORS,
    ET_BODIES
};
 
 
//------------------------------------------------------------------------------
/**
 *  An ode body's user-data pointer points to objects of this type.
 */
class OdeRigidBody
{
    friend class OdeModelLoader;
 public:
    virtual ~OdeRigidBody();

    void setName(const std::string & name);
    const std::string & getName() const;
    
    dBodyID getId() const;

    inline bool isStatic() const { return static_; }
    void setStatic(bool s);

    void setAutoDisable(bool b);
    bool isSleeping() const;
    void setSleeping(bool s);

//    void enableCollision(bool b);
    
    const Vector & getCog() const;
    
    Matrix getTransform() const;
    Vector getPosition() const;

    Vector getGlobalLinearVel() const;
    Vector getGlobalAngularVel() const;

    Vector getGlobalLinearVelAtGlobalPos(const Vector & pos) const;
    
    Vector getLocalLinearVel() const;
    Vector getLocalAngularVel() const;


    void setTransform(const Matrix & transform);
    void setPosition(const Vector & pos);
    
    void setGlobalLinearVel(const Vector & v);
    void setGlobalAngularVel(const Vector & w);

    void setLocalLinearVel(const Vector & v);
    void setLocalAngularVel(const Vector & w);


    void addGlobalForce (const Vector & force);
    void addGlobalTorque(const Vector & torque);

    void addGlobalForceAtGlobalPos(const Vector & force, const Vector & pos);
    
    void addLocalForce (const Vector & force);
    void addLocalTorque(const Vector & torque);

    void addLocalForceAtLocalPos(const Vector & force, const Vector & pos);
    
    void setCollisionCategory(unsigned cat);

    void enableGravity(bool g);

    void setTotalMass(float m);
    float getMass() const;
    Matrix getInertiaTensor() const;
    
    
    Vector vecToWorld  (const Vector & v) const;
    Vector vecFromWorld(const Vector & v) const;

    OdeSimulator * getSimulator();

    OdeRigidBody * instantiate(dBodyID id, OdeSimulator * simulator) const;

    void addGeom(OdeGeom * geom);

    OdeGeom * getGeom     (const std::string & name);
    unsigned  getGeomIndex(const std::string & name) const;

    const std::vector<OdeGeom*> & getGeoms() const;
    std::vector<OdeGeom*>       & getGeoms();

    void      deleteGeom(unsigned index);
    OdeGeom * detachGeom(unsigned index);

    void * getUserData();
    void setUserData(void * data);

    bool setCollisionCallback(const CollisionCallback & callback,
                              const std::string & geom_name = "");
    void clearCollisionCallback(const std::string & geom_name = "");

    void changeSpace(OdeCollisionSpace * old_space,
                     OdeCollisionSpace * new_space);

    void setBelowWater(bool b);
    bool isBelowWater() const;
    
 protected:
    OdeRigidBody();

    std::string name_;
    
    bool static_;
    
    Vector cog_; ///< Ode doesn't support this, so we have to keep
                 ///track of center of gravity ourselves...
    
    std::vector<OdeGeom*> geom_;

    void * user_data_;

    dBodyID id_; ///< Zero for "blueprint" bodies and static bodies.

    bool below_water_; ///< Very crude water fake physics: if any part
                       ///of the body touches water, apply extra
                       ///dampening.
    
    OdeSimulator * simulator_;

    bool mass_initialized_;
    
 private:
    OdeRigidBody(const OdeRigidBody&);
    OdeRigidBody & operator=(const OdeRigidBody&);
};

std::ostream & operator<<(std::ostream & out, const OdeRigidBody & body);

} // namespace physics 

#endif
