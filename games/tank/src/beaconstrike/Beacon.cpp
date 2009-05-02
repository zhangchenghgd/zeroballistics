
#include "Beacon.h"

#include <limits>

#include <raknet/BitStream.h>

#include "physics/OdeRigidBody.h"
#include "physics/OdeSimulator.h"

#include "AutoRegister.h"
#include "NetworkCommand.h"
#include "ParameterManager.h"
#include "Scheduler.h"
#include "TeamBs.h"


#undef min
#undef max

const std::string RADIUS_GEOM = "s_radius";
const std::string BODY_GEOM   = "s_body";


//------------------------------------------------------------------------------
/**
 *  Hysteresis transition values. If health is below first value,
 *  change state. If it is above second value, change state back. 
 */
const float BEACON_HEALTH_THRESHOLD [][2] = { { 0.85f, 0.9f },
                                              { 0.3f,  0.5f } };


REGISTER_CLASS(GameObject, Beacon);

//------------------------------------------------------------------------------
Beacon::~Beacon()
{
}

//------------------------------------------------------------------------------
void Beacon::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    RigidBody::writeInitValuesToBitstream(stream);

    stream.Write((uint8_t)team_id_);
    stream.Write(state_ == BS_FIXED);
}

//------------------------------------------------------------------------------
void Beacon::readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp)
{
    RigidBody::readInitValuesFromBitstream(stream, game_state, timestamp);

    uint8_t team_id;
    stream.Read(team_id);

    bool fixed;
    stream.Read(fixed);
    if (fixed) setFixed();

    setTeamId((TEAM_ID)team_id);
}

//------------------------------------------------------------------------------
void Beacon::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    if (type & OST_EXTRA)
    {
        stream.Write(inside_radius_);
        stream.Write((uint8_t)state_);

        HitpointTracker::writeStateToBitstream(stream);
    }

    assert(isInsideRadius() || !isDeployed());

    // What gets written here depends on whether body is static -> do
    // it after deployment stuff
    RigidBody::writeStateToBitstream( stream, type);
}


//------------------------------------------------------------------------------
void Beacon::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    if (type & OST_EXTRA)
    {
        bool     new_inside_radius;
        uint8_t  new_state;
        
        stream.Read(new_inside_radius);
        stream.Read(new_state);

        HitpointTracker::readStateFromBitstream(stream);
        
        s_log << Log::debug('l')
              << "Read (in_radius, state, hitpoints) "
              << new_inside_radius
              << ", "
              << (unsigned)new_state
              << ", "
              << hitpoints_
              << " for "
              << *this
              << "\n";
        
        if (new_state == BS_FIXED)
        {
            setStatic(true);
            state_ = BS_FIXED;
            emit(BE_DEPLOYED_CHANGED);
        } else
        {
            if (state_ == BS_FIXED)
            {
                s_log << "ignoring state different from fixed for " << *this << "\n";
            } else if (new_state == BS_DEPLOYED && !new_inside_radius)
            {
                s_log << Log::warning
                      << "readStateFromBitstream says beacon is not in radius but deployed. Ignoring.\n";
            } else 
            {
                setInsideRadius(new_inside_radius);
                setDeployed(new_state == BS_DEPLOYED);
            }

            setState((BEACON_STATE)new_state);
        }
    }
    
    // What gets read here depends on whether body is static -> do
    // it after deployment stuff
    RigidBody::readStateFromBitstream(stream, type, timestamp);

    // Previous call might have changed position (e.g. at
    // initialization time), so place geoms again
    if ((type & OST_EXTRA) && isStatic())
    {
        placeGeoms();
        warpProxy(true);
    }
}


//------------------------------------------------------------------------------
void Beacon::setInsideRadius(bool a)
{
    if (inside_radius_ == a) return;

    s_log << Log::debug('l')
          << *this
          << " changed inside_radius_ from "
          << isInsideRadius()
          << " to "
          << a
          << "\n";
    
    inside_radius_ = a;
    network_state_dirty_ = true;
    
    emit (BE_INSIDE_RADIUS_CHANGED);

    if (!a)
    {
        // If we are currently hovering or rising and fall out of the
        // radius, drop back to ground. 
        if (state_ == BS_HOVERING || state_ == BS_RISING)
        {
            setSleeping(false);
            setState(BS_UNDEPLOYED);

            // This happens on server only (task_deploy_ exists only
            // on server)
            if (task_deploy_ != INVALID_TASK_HANDLE)
            {
                getTarget()->enableGravity(true);
//                 getTarget()->addGlobalForce(100.0f*getTarget()->getMass()*Vector((float)rand()/RAND_MAX, 0.0f,
//                                                                                 (float)rand()/RAND_MAX));
                s_scheduler.removeTask(task_deploy_, &fp_group_);
                task_deploy_ = INVALID_TASK_HANDLE;
            }
        }
        
        setDeployed(false);
    }
}





//------------------------------------------------------------------------------
/**
 *  Returns whether this beacon is currently connected to a fixed beacon.
 */
bool Beacon::isInsideRadius() const
{
    return inside_radius_;
}

//------------------------------------------------------------------------------
BEACON_STATE Beacon::getState() const
{
    return state_;
}


//------------------------------------------------------------------------------
void Beacon::setFixed()
{
    if (state_ == BS_FIXED) return;
    assert(state_ == BS_DISPENSED);
    
    placeGeoms();

    inside_radius_ = true;
    setState(BS_FIXED);
    setStatic(true);
}


//------------------------------------------------------------------------------
/**
 *  A deployed beacon is always static and vice versa. Thus we need no
 *  deployed_ state variable.
 */
void Beacon::setDeployed(bool s)
{
    assert(state_ != BS_FIXED);
    
    if (isDeployed() == s) return;    
    
    s_log << Log::debug('l')
          << *this
          << " changed deployed from "
          << isDeployed()
          << " to "
          << s
          << "\n";


    assert(isInsideRadius() || !s);

    // Deployed beacons should be completely upright
    if (s)
    {
        Matrix t = getTransform();
        t.getX() = Vector(1.0f, 0.0f, 0.0f);
        t.getY() = Vector(0.0f, 1.0f, 0.0f);
        t.getZ() = Vector(0.0f, 0.0f, 1.0f);
        setTransform(t);
    }
    
    setStatic(s);

    if (s) setState(BS_DEPLOYED);
    else   setState(BS_UNDEPLOYED);

    emit(BE_DEPLOYED_CHANGED);
}


//------------------------------------------------------------------------------
/**
 *  \see setDeployed
 *  Beacon is deployed in states BS_DEPLOYED and BS_FIXED.
 */
bool Beacon::isDeployed() const
{
    assert(!isStatic() ||
           state_ == BS_DEPLOYED ||
           state_ == BS_FIXED);
  
    return isStatic();
}



//------------------------------------------------------------------------------
void Beacon::setTeamId(TEAM_ID t)
{
    if (t > NUM_TEAMS_BS)
    {
        team_id_ = INVALID_TEAM_ID;
    } else team_id_ = t;
}


//------------------------------------------------------------------------------
TEAM_ID Beacon::getTeamId() const
{
    return team_id_;
}


//------------------------------------------------------------------------------
float Beacon::getRadius() const
{
    return radius_geom_->getRadius();
}

//------------------------------------------------------------------------------
void Beacon::setCarried(bool c)
{
    // Cannot carry fixed beacon
    assert(!c || state_ != BS_FIXED);
    physics::OdeRigidBody * body = proxy_object_ ? proxy_object_ : target_object_;
    
    if (c)
    {        
        setDeployed(false);

        // Beacon might be asleep, so remove either from actor or from
        // static space.
        body->changeSpace(body->getSimulator()->getActorSpace(), NULL);
        body->changeSpace(body->getSimulator()->getStaticSpace(), NULL);

        setSleeping(true);
    } else
    {   
        body->changeSpace(NULL, body->getSimulator()->getActorSpace());

        setSleeping(false);
        
        setGlobalLinearVel (Vector(0,0,0));
        setGlobalAngularVel(Vector(0,0,0));
    }

    setState(c ? BS_CARRIED : BS_UNDEPLOYED);

    emit(BE_CARRIED_CHANGED);
}


//------------------------------------------------------------------------------
/**
 *  Make beacon rise, schedule hovering.
 */
void Beacon::rise()
{
    assert(inside_radius_);
    
    setState(BS_RISING);
    
    getTarget()->enableGravity(false);
    setGlobalLinearVel(Vector(0,
                              s_params.get<float>("server.logic_beaconstrike.beacon_rise_speed"),
                              0));
    setSleeping(false);

    
    if (task_deploy_ != INVALID_TASK_HANDLE) s_scheduler.removeTask(task_deploy_, &fp_group_);
    task_deploy_ = s_scheduler.addEvent(SingleEventCallback(this, &Beacon::stopRising),
                                        s_params.get<float>("server.logic_beaconstrike.beacon_stop_hover_delay"),
                                        NULL,
                                        "Beacon::stopRising",
                                        &fp_group_);

    s_log << Log::debug('l')
          << "Hovering beacon "
          << *this
          << "\n";
}

//------------------------------------------------------------------------------
/**
 *  Enters hover state, and schedules the deployment.
 */
void Beacon::stopRising(void*)
{
    task_deploy_ = INVALID_TASK_HANDLE;
    
    if (state_ != BS_RISING) return;
    setState(BS_HOVERING);
    
    if (isInsideRadius()) setGlobalLinearVel(Vector(0,0,0));

    task_deploy_ = s_scheduler.addEvent(SingleEventCallback(this, &Beacon::deploy),
                                        s_params.get<float>("server.logic_beaconstrike.beacon_deploy_delay"),
                                        NULL,
                                        "Deploy Beacon",
                                        &fp_group_);    
}


//------------------------------------------------------------------------------
/**
 *  Called at the end of the beacon hovering phase. Re-enables
 *  gravity.
 */
void Beacon::deploy(void*)
{
    task_deploy_ = INVALID_TASK_HANDLE;

    if (state_ != BS_HOVERING)  return;
    
    getTarget()->enableGravity(true);
    setDeployed(isInsideRadius());
}


//------------------------------------------------------------------------------
void Beacon::clearNeighbors()
{
    neighbor_.clear();
}

//------------------------------------------------------------------------------
void Beacon::addNeighbor(Beacon * b)
{    
    neighbor_.push_back(b);
}


//------------------------------------------------------------------------------
std::vector<Beacon*> & Beacon::getNeighbor()
{
    return neighbor_;
}



//------------------------------------------------------------------------------
physics::OdeCCylinderGeom * Beacon::getRadiusGeom()
{
    assert(radius_geom_);
    return radius_geom_;
}

//------------------------------------------------------------------------------
physics::OdeSphereGeom * Beacon::getBodyGeom()
{
    assert(body_geom_);
    return body_geom_;
}

//------------------------------------------------------------------------------
const physics::OdeSphereGeom * Beacon::getBodyGeom() const
{
    assert(body_geom_);
    return body_geom_;
}


//------------------------------------------------------------------------------
void Beacon::setHitpoints(int hp, bool dirty_state)
{
    if(state_ == BS_FIXED) return;    

    HitpointTracker::setHitpoints(hp, dirty_state);

    if (hitpoints_ == 0) setDeployed(false);

    float health_percentage = (float)hitpoints_ / max_hitpoints_;

    // Check for downwards transitions
    if (health_percentage < BEACON_HEALTH_THRESHOLD[1][0])
    {
        if (health_state_ != BHS_CRITICAL)
        {
            emit(BE_HEALTH_CRITICAL);
            health_state_ = BHS_CRITICAL;
        }
    } else if (health_percentage < BEACON_HEALTH_THRESHOLD[0][0])
    {
        if (health_state_ == BHS_HEALTHY)
        {
            emit(BE_UNDER_ATTACK);
            health_state_ = BHS_UNDER_ATTACK;
        }
    }

    // Check for upwards transitions
    if (health_percentage > BEACON_HEALTH_THRESHOLD[0][1]) 
    {
        if (health_state_ != BHS_HEALTHY)
        {
            emit(BE_HEALTHY);
            health_state_ = BHS_HEALTHY;
        }
    } else if (health_percentage > BEACON_HEALTH_THRESHOLD[1][1]) 
    {
        if (health_state_ == BHS_CRITICAL)
        {
            // don't emit "under attack" when regenerating...
//            emit(BE_UNDER_ATTACK);
            health_state_ = BHS_UNDER_ATTACK;
        }
    } 
}


//------------------------------------------------------------------------------
BEACON_HEALTH_STATE Beacon::getHealthState() const
{
    return health_state_;
}

//------------------------------------------------------------------------------
void Beacon::handleHealth(float dt)
{
    if (state_ == BS_UNDEPLOYED)
    {
        dealWeaponDamage((unsigned)(dt*s_params.get<unsigned>("server.logic_beaconstrike.beacon_degeneration_rate")),
                         UNASSIGNED_SYSTEM_ADDRESS);
    } else if (isDeployed())
    {
        startHealing();
    }
}

//------------------------------------------------------------------------------
void Beacon::placeGeoms()
{
    Vector pos = target_object_->getPosition();
    pos.y_ = 0.0f;
    radius_geom_->setPosition(pos);
    body_geom_  ->setPosition(pos);

}


//------------------------------------------------------------------------------
void Beacon::setNetworkFlagDirty()
{
    network_state_dirty_ = true;
}


//------------------------------------------------------------------------------
Beacon::Beacon() :
    inside_radius_(true), // To avoid newly spawned beacons from hovering right away
    team_id_(INVALID_TEAM_ID),
    health_state_(BHS_HEALTHY),
    radius_geom_(NULL),
    body_geom_(NULL),
    task_deploy_    (INVALID_TASK_HANDLE),
    state_(BS_DISPENSED)
{
    getObservable()->addObserver(ObserverCallbackFun0(this, &Beacon::setNetworkFlagDirty),
                                 HTE_STATE_CHANGED,
                                 &fp_group_);

    setHealParameters(s_params.get<float>("server.logic_beaconstrike.beacon_heal_start_val"),
                      s_params.get<float>("server.logic_beaconstrike.beacon_heal_increment"));
    
    max_hitpoints_ = s_params.get<unsigned>("server.logic_beaconstrike.beacon_hitpoints");
    
}


//------------------------------------------------------------------------------
/**
 *  Detaches the radius and body geoms from the body so they can be
 *  placed manually at a constant height.
 */
void Beacon::init()
{
    // Retrieve radius geom
    assert(target_object_);
    
    body_geom_   = dynamic_cast<physics::OdeSphereGeom*>   (target_object_->getGeom(BODY_GEOM));

    if(body_geom_ == NULL)
    {
        throw Exception("Beacon is missing body geom or it is not of type sphere");
    }

    body_geom_->stopTrackingRigidbody();


    radius_geom_ = new physics::OdeCCylinderGeom(s_params.get<float>("server.logic_beaconstrike.beacon_radius"),
                                                 1000.0f);
    radius_geom_->setSensor(true);
    // make beacon upright
    Matrix rot(true);
    rot.loadCanonicalRotation(PI*0.5f, 0);
    radius_geom_->setOffset(rot);
    
    target_object_->addGeom(radius_geom_);
    radius_geom_->stopTrackingRigidbody();

    setHitpoints(max_hitpoints_);

    if (isStatic()) state_ = BS_FIXED;
}


//------------------------------------------------------------------------------
void Beacon::setState(BEACON_STATE new_state)
{
    if (state_ == new_state) return;
    network_state_dirty_ = true;

    bool emit_hover_changed   = (state_ == BS_HOVERING || new_state == BS_HOVERING);
    bool emit_carried_changed = (state_ == BS_CARRIED  || new_state == BS_CARRIED);
    
    state_ = new_state;

    setInvincible((void*)(state_ == BS_FIXED || state_ == BS_DISPENSED));
    if (!isDeployed()) stopHealing();

    if (emit_hover_changed)   emit(BE_HOVERING_CHANGED);
    if (emit_carried_changed) emit(BE_CARRIED_CHANGED);
}
