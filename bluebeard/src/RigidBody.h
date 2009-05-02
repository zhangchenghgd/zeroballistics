

#ifndef TANK_RIGIDBODY_INCLUDED
#define TANK_RIGIDBODY_INCLUDED


#include <map>

#include "physics/OdeCollision.h"
#include "physics/OdeRigidBody.h"

#include "GameObject.h"
#include "RegisteredFpGroup.h"
#include "Scheduler.h"

class GameState;

namespace osg
{
    class MatrixTransform;
    class Node;
}


namespace RakNet
{
    class BitStream;
}


namespace physics
{
    class OdeRigidBody;
    class OdeSimulator;
}




//------------------------------------------------------------------------------
enum RIGID_BODY_EVENT
{
    RBE_GO_TO_SLEEP = GOE_LAST,
    RBE_WAKE_UP,
    RBE_INITIALIZATION_FINISHED,
    RBE_POSITION_SET_EXTERNALLY,
    RBE_INITIAL_POSITION_SET,
    RBE_ACTIVATED,
    RBE_LAST
};


const std::string ACTIVATE_APPENDIX_TIMEOUT = "td";

//------------------------------------------------------------------------------
/**
 *  Represents a rigid body in the game. 
 */
class RigidBody : public GameObject
{

 public:
    virtual ~RigidBody();

    GAME_OBJECT_IMPL(RigidBody);
    
    void createProxy();
    
    virtual void frameMove(float dt);

    // used for object creation
    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);

    // used for state value transmission 
    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp);

    virtual void handleProxyInterpolation();


    virtual void scheduleForDeletion();
    
    // -------------------- OdeRigidBody begin --------------------

    void setStatic(bool s);
    bool isStatic() const;

    bool isSleeping() const;
    void setSleeping(bool s);
    
    void setPosition(const Vector & pos);
    void setTransform(const Matrix & mat);

    Vector getPosition() const;
    Matrix getTransform(bool proxy = false) const;


    void setGlobalLinearVel (const Vector & v);
    void setGlobalAngularVel(const Vector & w);

    Vector getGlobalLinearVel() const;
    Vector getGlobalAngularVel() const;

    Vector getLocalLinearVel() const;
    Vector getLocalAngularVel() const;
    
    void setCollisionCategory(unsigned cat, bool set_proxy);

    physics::OdeRigidBody * getTarget() const;
    physics::OdeRigidBody * getProxy() const;
    
    // -------------------- OdeRigidBody end   --------------------

    void setLifetime(float dt);
    float getLifetime() const;
    void dealActivation(unsigned d, std::string type);
    

    RegisteredFpGroup & getFpGroup();

    
    static RigidBody * create(const std::string & desc_file,
                              const std::string & type,
                              physics::OdeSimulator * simulator,
                              bool create_visual);


    
 protected:

    RigidBody();
    RigidBody(const RigidBody & other, physics::OdeSimulator * sim);

    /// Called when the object is fully constructed to let the
    /// subclasses do their initialization (e.g. acquire special
    /// entities...
    virtual void init() {}    

    void createTargetObject(const std::string & name,
                            physics::OdeSimulator * simulator);


    void writeCoreState(RakNet::BitStream & stream, bool quantized) const;
    void readCoreState(RakNet::BitStream & stream, uint32_t timestamp, bool quantized);

    void warpProxy(bool force);

    void deadReckon(Matrix & transform, Vector & v, const Vector & w, bool gravity, uint32_t timestamp);

    void handleSleepingState();

    void wakeupNeighbors();
    bool wakeupNeighborsCollisionCallback(const physics::CollisionInfo & info);

    void onLifetimeExpired(void*);
        
    physics::OdeRigidBody * target_object_; ///< The rigid body this object is
                                  ///linked to. Can be NULL. The user
                                  ///data pointer of the rigidbody is
                                  ///set to the owner GameObject.

    physics::OdeRigidBody * proxy_object_; ///< The rigid body object used for
                                 ///client side interpolation / local
                                 ///collisions. NULL on server.
    

    bool sleeping_last_frame_; ///< Whether the body was asleep last
                               ///frame. Used to detect state
                               ///transition.

    hTask task_delete_; ///< RigidBody lifetime
    
    RegisteredFpGroup fp_group_;

    static float proxy_interpolation_speed_pos_;        
    static float proxy_interpolation_speed_orientation_;
    static float proxy_interpolation_speed_vel_;        
    static float proxy_interpolation_speed_ang_vel_;    
    static float proxy_warp_threshold_;    
};


#endif
