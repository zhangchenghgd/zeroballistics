

#include "RigidBody.h"


#include <raknet/BitStream.h>
#include <raknet/GetTime.h>

#include "physics/OdeSimulator.h"
#include "physics/OdeRigidBody.h"
#include "physics/OdeModelLoader.h"


#include "Profiler.h"
#include "NetworkCommand.h"
#include "GameState.h"
#include "ParameterManager.h"
#include "Paths.h"

float RigidBody::proxy_interpolation_speed_pos_         = 0.0f;
float RigidBody::proxy_interpolation_speed_orientation_ = 0.0f;   
float RigidBody::proxy_interpolation_speed_vel_         = 0.0f;           
float RigidBody::proxy_interpolation_speed_ang_vel_     = 0.0f;
float RigidBody::proxy_warp_threshold_                  = 0.0f;





//---------------------------------------------------------------------
RigidBody::~RigidBody()
{
    scheduleForDeletion();

    if (proxy_object_) delete proxy_object_;
    delete target_object_;
}


//------------------------------------------------------------------------------
/**
 *  Creates a proxy object which is rendered and used to interpolate
 *  to the target position specified by the server.
 */
void RigidBody::createProxy()
{
    assert(!target_object_->isStatic());

    proxy_object_ = target_object_->getSimulator()->instantiate(target_object_);
    proxy_object_->setUserData(this);
    proxy_object_->enableGravity(false);
    proxy_object_->setName(target_object_->getName() + "-proxy");

    proxy_interpolation_speed_pos_         = s_params.get<float>("physics.proxy_interpolation_speed_pos");
    proxy_interpolation_speed_orientation_ = s_params.get<float>("physics.proxy_interpolation_speed_orientation");     
    proxy_interpolation_speed_vel_         = s_params.get<float>("physics.proxy_interpolation_speed_vel");             
    proxy_interpolation_speed_ang_vel_     = s_params.get<float>("physics.proxy_interpolation_speed_ang_vel");
    proxy_warp_threshold_                  = s_params.get<float>("physics.proxy_warp_threshold");

    // Target object doesn't need collision detection anymore.
    //
    // For controlled objects, which need target collision for client
    // side prediction, this is enabled in ClientPlayer.
    target_object_->changeSpace(NULL);

    // If gravity influenced the target object, it would fall down
    // endlessly if there is a connection problem, pulling the proxy
    // down as well.
    target_object_->enableGravity(false);

    assert(!proxy_object_->isStatic());
}

//------------------------------------------------------------------------------
/**
 *  Proxy object interpolation: Wake up proxy if target is not
 *  asleep. Interpolate proxy speed, position towards target.
 *
 *  Check whether target_object_ has fallen asleep or woken up and
 *  emit observable events.
 */
void RigidBody::frameMove(float dt)
{
    if (proxy_object_)
    {
        if (!target_object_->isStatic() &&
            !target_object_->isSleeping())
        {
            if (proxy_object_->isSleeping())
            {
                // Wake up proxy if target is not asleep
                proxy_object_->setSleeping(false);
            }

            // Avoid doing proxy interpolation for sleeping
            // objects. We must take care, however, that a proxy
            // belonging to a sleeping target stays asleep
            handleProxyInterpolation();
        } else proxy_object_->setSleeping(true);
    }

    handleSleepingState();
}


//------------------------------------------------------------------------------
void RigidBody::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    GameObject::writeInitValuesToBitstream(stream);
    writeStateToBitstream(stream, OST_BOTH);

    float32_t lifetime = getLifetime();
    stream.WriteDelta(lifetime, 0.0f);
}

//------------------------------------------------------------------------------
/**
 *  Used for client-side object creation. A newly constructed object
 *  reads its initial data from the given stream and creates its stuff
 *  in game_state.
 */
void RigidBody::readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp)
{
    PROFILE(RigidBody::readInitValuesFromBitstream);
    
    GameObject::readInitValuesFromBitstream(stream, game_state, timestamp);
    
    createTargetObject(name_, game_state->getSimulator());

    if (!isStatic() && !target_object_->isClientSideOnly()) createProxy();

    init();

    emit(RBE_INITIALIZATION_FINISHED);

    readStateFromBitstream(stream, OST_BOTH, timestamp);

    float32_t lifetime = 0.0f;
    stream.ReadDelta(lifetime);
    setLifetime(lifetime);

    emit(RBE_INITIAL_POSITION_SET);
}


//------------------------------------------------------------------------------
/**
 *  Write transient state (eg position, velocity..) to the bitstream
 *  to be distributed over the network.
 */
void RigidBody::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    if (type & OST_CORE) writeCoreState(stream, true);
    GameObject::writeStateToBitstream(stream, type);
}


//------------------------------------------------------------------------------
/**
 *  Reads the object state form a raknet bitstream and assigns it to
 *  the target object.
 *
 *  \param stream The stream to read from.
 *
 *  \param type GameObject have only core state, so nothing gets
 *  read if this is OST_EXTRA.
 *
 *  \param timestamp Either 0 or a valid raknet timestamp. If
 *  non-zero, dead-reckoning is performed on the position before
 *  assigning it to the target object.
 *
 */
void RigidBody::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    // is not taken care of in readCoreState quantized decision ATM,
    // so don't allow that...
    assert(!(type & OST_CLIENT_SIDE_PREDICTION));
    
    if (type & OST_CORE)
    {
        readCoreState(stream, timestamp, true);
    }

    warpProxy(type & OST_EXTRA);

    GameObject::readStateFromBitstream(stream, type, timestamp);
}


//------------------------------------------------------------------------------
void RigidBody::handleProxyInterpolation()
{
    PROFILE(RigidBody::handleProxyInterpolation);
    
    assert(proxy_object_);
    
    // Position & Orientation
    Matrix proxy_trans  = proxy_object_ ->getTransform();
    Matrix target_trans = target_object_->getTransform();

    Vector new_pos = proxy_trans.getTranslation() +
        (target_trans.getTranslation() - proxy_trans.getTranslation()) * proxy_interpolation_speed_pos_;

    // XXXX Cheaper interpolation
    proxy_trans += (target_trans - proxy_trans) * proxy_interpolation_speed_orientation_;
            
    proxy_trans.orthonormalize();
    proxy_trans._44 = 1.0f;

    proxy_trans.getTranslation() = new_pos;
    
    proxy_object_->setTransform(proxy_trans);


    // Linear velocity
    Vector cur_v = proxy_object_->getGlobalLinearVel();
    if (cur_v.lengthSqr() > target_object_->getGlobalLinearVel().lengthSqr())
    {
        cur_v = target_object_->getGlobalLinearVel();
    } else
    {
        cur_v += (target_object_->getGlobalLinearVel() - cur_v) *
            proxy_interpolation_speed_vel_;
    }
    proxy_object_->setGlobalLinearVel(cur_v);


    // Angular velocity
    Vector cur_w = proxy_object_->getGlobalAngularVel();
    if (cur_w.lengthSqr() > target_object_->getGlobalAngularVel().lengthSqr())
    {
        cur_w = target_object_->getGlobalAngularVel();
    } else
    {
        cur_w += (target_object_->getGlobalAngularVel() - cur_w) *
            proxy_interpolation_speed_ang_vel_;
    }
    proxy_object_->setGlobalAngularVel(cur_w);
}



//------------------------------------------------------------------------------
/**
 *  Remove all geoms from their spaces. This generates CT_STOP events
 *  if those geoms currently are in contact. Because this is not done
 *  in the destructor, full type information about the RigidBody is
 *  still available inside the collisioncallbacks.
 */
void RigidBody::scheduleForDeletion()
{
    if (!scheduled_for_deletion_)
    {   
        if (proxy_object_)
        {
            for (unsigned i=0; i<proxy_object_->getGeoms().size(); ++i)
            {
                proxy_object_->getSimulator()->disableGeom(proxy_object_->getGeoms()[i]);
                proxy_object_->getGeoms()[i]->setSpace(NULL);
            }
        } else
        {
            // Wake up any bodies which are neighboring to our
            // geoms. Check for target_object_ existence as this can
            // happen in exception stack unwinding if rb body creation
            // failed.
            if (target_object_) setSleeping(false);
        }

        if (target_object_)
        {
            for (unsigned i=0; i<target_object_->getGeoms().size(); ++i)
            {
                target_object_->getSimulator()->disableGeom(target_object_->getGeoms()[i]);
                target_object_->getGeoms()[i]->setSpace(NULL);
            }
        }
    }

    GameObject::scheduleForDeletion();
}


//------------------------------------------------------------------------------
/**
 *  Sets the static state of the body, properly handling sleeping
 *  state as well.
 */
void RigidBody::setStatic(bool s)
{
    if (s==isStatic()) return;

    // handleSleepingState only works for non-static bodies, so call
    // it before going static.
    if (s)
    {
        setSleeping(true);
        handleSleepingState();
    }

    target_object_->setStatic(s);
    if (proxy_object_) proxy_object_->setStatic(s);

    // handleSleepingState only works for non-static bodies, so call
    // it after going non-static.
    if (!s) handleSleepingState();
}


//------------------------------------------------------------------------------
bool RigidBody::isStatic() const
{
    assert(target_object_);
    return target_object_->isStatic();
}

//------------------------------------------------------------------------------
bool RigidBody::isSleeping() const
{
    return target_object_->isSleeping();
}


//------------------------------------------------------------------------------
void RigidBody::setSleeping(bool s)
{
    target_object_->setSleeping(s);
    handleSleepingState();
}


//------------------------------------------------------------------------------
void RigidBody::setPosition(const Vector & pos)
{    
    target_object_->setPosition(pos);
    if (proxy_object_) proxy_object_->setPosition(pos);

    emit(RBE_POSITION_SET_EXTERNALLY);
}

//------------------------------------------------------------------------------
void RigidBody::setTransform(const Matrix & mat)
{
    target_object_->setTransform(mat);
    if (proxy_object_) proxy_object_->setTransform(mat);

    emit(RBE_POSITION_SET_EXTERNALLY);
}


//------------------------------------------------------------------------------
Vector RigidBody::getPosition() const
{
    return target_object_->getPosition();
}


//------------------------------------------------------------------------------
Matrix RigidBody::getTransform(bool proxy) const
{
    if (proxy && proxy_object_)
    {
        return proxy_object_->getTransform();
    } else
    {
        return target_object_->getTransform();
    }
}


//------------------------------------------------------------------------------
void RigidBody::setGlobalLinearVel (const Vector & v)
{
    target_object_->setGlobalLinearVel(v);
}

//------------------------------------------------------------------------------
void RigidBody::setGlobalAngularVel(const Vector & w)
{
    target_object_->setGlobalAngularVel(w);
}


//------------------------------------------------------------------------------
Vector RigidBody::getGlobalLinearVel() const
{
    return target_object_->getGlobalLinearVel();
}

//------------------------------------------------------------------------------
Vector RigidBody::getGlobalAngularVel() const
{
    return target_object_->getGlobalAngularVel();
}

//------------------------------------------------------------------------------
Vector RigidBody::getLocalLinearVel() const
{
    return target_object_->getLocalLinearVel();
}

//------------------------------------------------------------------------------
Vector RigidBody::getLocalAngularVel() const
{
    return target_object_->getLocalAngularVel();
}

//------------------------------------------------------------------------------
/**
 *  Sets the category of target (and proxy object if set_proxy is
 *  true). Only non-sensor geoms are affected!
 */
void RigidBody::setCollisionCategory(unsigned cat, bool set_proxy)
{
    target_object_->setCollisionCategory(cat);
    
    if (set_proxy && proxy_object_)
    {
        proxy_object_->setCollisionCategory(cat);
    }
}

//------------------------------------------------------------------------------
physics::OdeRigidBody * RigidBody::getTarget() const
{
    return target_object_;
}

//------------------------------------------------------------------------------
physics::OdeRigidBody * RigidBody::getProxy() const
{
    return proxy_object_;
}


//------------------------------------------------------------------------------
/**
 *  (Re)schedules onLifetimeExpired if dt != 0.
 */
void RigidBody::setLifetime(float dt)
{
    if (task_delete_ == INVALID_TASK_HANDLE)
    {
        if (dt == 0.0f) return;
        task_delete_ = s_scheduler.addEvent(SingleEventCallback(this, &RigidBody::onLifetimeExpired),
                                            dt,
                                            NULL,
                                            "RigidBody::onLifetimeExpired",
                                            &fp_group_);
    } else
    {
        if (dt == 0.0f)
        {
            s_scheduler.removeTask(task_delete_, &fp_group_);
            task_delete_ = INVALID_TASK_HANDLE;
        } else
        {
            s_scheduler.reschedule(task_delete_, dt);
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Returns the time this body still has to live, or 0.0 if no
 *  lifetime was set.
 */
float RigidBody::getLifetime() const
{
    if (task_delete_ == INVALID_TASK_HANDLE) return 0.0f;
    return s_scheduler.getExecutionDelay(task_delete_);
}


//------------------------------------------------------------------------------
/**
 *  Removes activation points of the specified type from the RB. type
 *  is a list of appendices which are used for finding the replacement
 *  body if the body is activated.
 */
void RigidBody::dealActivation(unsigned d, std::string type)
{
    if (target_object_->dealActivation(d))
    {
        emit(RBE_ACTIVATED, &type);
    }
}


//------------------------------------------------------------------------------
RegisteredFpGroup & RigidBody::getFpGroup()
{
    return fp_group_;
}


//------------------------------------------------------------------------------
RigidBody * RigidBody::create(const std::string & desc_file,
                              const std::string & type,
                              physics::OdeSimulator * simulator,
                              bool create_visual)
{
    PROFILE(RigidBody::create);
    
    std::auto_ptr<RigidBody> ret(dynamic_cast<RigidBody*>(GameObject::create(desc_file,
                                                                             type,
                                                                             create_visual)));
    
    ret->createTargetObject(desc_file, simulator);
    ret->init();

    ret->emit(RBE_INITIALIZATION_FINISHED);
    
    return ret.release();
}



//------------------------------------------------------------------------------
RigidBody::RigidBody() :
    target_object_(NULL),
    proxy_object_(NULL),
    sleeping_last_frame_(true),
    task_delete_(INVALID_TASK_HANDLE)
{
}

//------------------------------------------------------------------------------
RigidBody::RigidBody(const RigidBody & other, physics::OdeSimulator * sim) :
    GameObject(other),
    target_object_(sim->instantiate(other.target_object_)),
    proxy_object_(NULL),
    sleeping_last_frame_(true),
    task_delete_(INVALID_TASK_HANDLE)    
{
}



//------------------------------------------------------------------------------
/**
 *  Creates the ode rigid body specified by name in the given
 *  simulator.
 */
void RigidBody::createTargetObject(const std::string & name,
                                   physics::OdeSimulator * simulator)
{
    assert(simulator);

    target_object_ = s_ode_model_loader.instantiateModel(simulator, name);
    target_object_->setUserData(this);

    setLifetime(target_object_->getLifetime());
}




//------------------------------------------------------------------------------
void RigidBody::writeCoreState(RakNet::BitStream & stream, bool quantized) const
{
#ifdef DISABLE_NETWORK_OPTIMIZATIONS
    quantized = false;
#endif    

    
    Matrix m = target_object_->getTransform();
    network::writeToBitstream(stream, m, quantized);

    if (!isStatic())
    {
        Vector v;
        
        v = target_object_->getGlobalLinearVel();
        if (quantized)
        {
            stream.WriteVector(v.x_,v.y_,v.z_);
        } else
        {
            stream.Write(v.x_);
            stream.Write(v.y_);
            stream.Write(v.z_);
        }

        v = target_object_->getGlobalAngularVel();
        if (quantized)
        {
            stream.WriteVector(v.x_,v.y_,v.z_);
        } else
        {
            stream.Write(v.x_);
            stream.Write(v.y_);
            stream.Write(v.z_);
        }
    }
}


//------------------------------------------------------------------------------
void RigidBody::readCoreState(RakNet::BitStream & stream, uint32_t timestamp, bool quantized)
{
#ifdef DISABLE_NETWORK_OPTIMIZATIONS
    quantized = false;
#endif    

    Matrix m;
    network::readFromBitstream(stream, m, quantized);

    if (!isStatic())
    {
        Vector v,w;

        if (quantized)
        {
            stream.ReadVector(v.x_,v.y_,v.z_);
            stream.ReadVector(w.x_,w.y_,w.z_);
        } else
        {
            stream.Read(v.x_);
            stream.Read(v.y_);
            stream.Read(v.z_);
            stream.Read(w.x_);
            stream.Read(w.y_);
            stream.Read(w.z_);
        }

        // Dead-reckon without gravity for normal objects
        deadReckon(m, v, w, false, timestamp);

        target_object_->setGlobalLinearVel (v);
        target_object_->setGlobalAngularVel(w);

        // Wake up if target is moving. We cannot always simply wake
        // up and assume target is awake because this is called once
        // for the initial state transmission.
        if (target_object_->isSleeping() &&
            (v.x_ != 0.0f || v.y_ != 0.0f || v.z_ != 0.0f ||
             w.x_ != 0.0f || w.y_ != 0.0f || w.z_ != 0.0f))             
        {
            target_object_->setSleeping(false);
        }
    }
        
    target_object_->setTransform(m);
}


//------------------------------------------------------------------------------
/**
 *  Updates the proxy position if it is too far away from the target.
 *
 *  \param force If true, update proxy regardless of position.
 */
void RigidBody::warpProxy(bool force)
{
    if (!proxy_object_) return;

    float dist_sqr = (target_object_->getPosition() - proxy_object_ ->getPosition()).lengthSqr();
    if (force || (dist_sqr > proxy_warp_threshold_))
    {
        if (!proxy_object_->isStatic())
        {
            proxy_object_->setGlobalLinearVel (target_object_->getGlobalLinearVel());
            proxy_object_->setGlobalAngularVel(target_object_->getGlobalAngularVel());
        }
        proxy_object_->setTransform(target_object_->getTransform());
            
        s_log << Log::debug('n');
        if (force) s_log << "Forced ";
        s_log << "warping proxy for "
              << *this
              << " to new position ("
              << dist_sqr
              << ")\n";
    }
}


//------------------------------------------------------------------------------
/**
 *  Used to perform simple simulation for the specified time interval.
 *
 *  \param transform [in,out] The position of the body to be simulated.
 *
 *  \param v [in, out] The velocity to simulate with. If simulating
 *  with gravity, will be changed to the velocity at the end of the
 *  interval.
 *
 *  \param w The angular velocity to simulate with.
 *  \param gravity Whether to take gravity into account.
 *  \param timestamp The time to simulate from. Difference to RakNet::GetTime is duration of simulation.
 */
void RigidBody::deadReckon(Matrix & transform,
                           Vector & v,
                           const Vector & w,
                           bool gravitiy,
                           uint32_t timestamp)
{
    assert(target_object_);

    if (timestamp == 0) return;

    uint32_t cur_time = RakNet::GetTime();
    if (cur_time <= timestamp) return;
    float dt = (float)(cur_time - timestamp) * 0.001f;
    
    float step_size = 1.0f / s_params.get<float>("physics.fps");
    while (dt > 0.0f)
    {   
        v -= step_size*v*s_params.get<float>("physics.lin_dampening");
        if (gravitiy)
        {
            v.y_ -= step_size * s_params.get<float>("physics.gravity");
        }
        transform.getTranslation() += step_size * v;

        dt -= step_size;
    }

    // Angular velocity
    float w_abs = w.length();
    if (!equalsZero(w_abs))
    {
        Matrix rot;
        rot.loadRotationVector(-deg2Rad(w_abs)*dt, w);

        transform = transform.mult3x3Left(rot);
    }
}

//------------------------------------------------------------------------------
void RigidBody::handleSleepingState()
{
    if (target_object_->isStatic()) return;
    
    if (target_object_->isSleeping() && !sleeping_last_frame_)
    {
        // Going to sleep
        s_log << Log::debug('l')
              << *this
              << " has gone to sleep.\n";
        sleeping_last_frame_ = true;


        // Move to static collision space
        physics::OdeRigidBody * obj = proxy_object_ ? proxy_object_ : target_object_;
        obj->changeSpace(obj->getSimulator()->getActorSpace(), obj->getSimulator()->getStaticSpace());

        if (proxy_object_)
        {
            warpProxy(true);
            proxy_object_->setSleeping(true);
        }
        
        emit(RBE_GO_TO_SLEEP);
        
    } else if (!target_object_->isSleeping() && sleeping_last_frame_)
    {
        // Waking up
        s_log << Log::debug('l')
              << *this
              << " is now awake.\n";
        sleeping_last_frame_ = false;

        wakeupNeighbors();
        
        // Move to back to actor collision space
        physics::OdeRigidBody * obj = proxy_object_ ? proxy_object_ : target_object_;
        obj->changeSpace(obj->getSimulator()->getStaticSpace(), obj->getSimulator()->getActorSpace());
        
        emit(RBE_WAKE_UP);
    }
}


//------------------------------------------------------------------------------
void RigidBody::wakeupNeighbors()
{
    for (unsigned g=0; g<target_object_->getGeoms().size(); ++g)
    {
        target_object_->getSimulator()->getStaticSpace()->collide(
            target_object_->getGeoms()[g], physics::CollisionCallback(this, &RigidBody::wakeupNeighborsCollisionCallback));
    }
}


//------------------------------------------------------------------------------
bool RigidBody::wakeupNeighborsCollisionCallback(const physics::CollisionInfo & info)
{
    RigidBody * other_body = (RigidBody*)info.other_geom_->getUserData();
    if (other_body) other_body->setSleeping(false);
    return false;
}




//------------------------------------------------------------------------------
void RigidBody::onLifetimeExpired(void*)
{
    task_delete_ = INVALID_TASK_HANDLE;
    emit(RBE_ACTIVATED, const_cast<std::string*>(&ACTIVATE_APPENDIX_TIMEOUT));
}

