
#include "Tank.h"

#include <cmath>

#include <raknet/BitStream.h>

#include "physics/OdeRigidBody.h"
#include "physics/OdeCollision.h"
#include "physics/OdeSimulator.h"

#include "Water.h"
#include "PlayerInput.h"
#include "utility_Math.h"
#include "Quaternion.h"
#include "Log.h"
#include "AutoRegister.h"
#include "WeaponSystem.h"
#include "Scheduler.h"
#include "Profiler.h"
#include "VariableWatcher.h"

#include "GameLogicServerCommon.h"


#include "Paths.h"
#include "TerrainData.h"

#undef min
#undef max

REGISTER_CLASS(GameObject, Tank);


const terrain::TerrainData * Tank::terrain_data_ = NULL;

const std::string WHEEL_NAME = "s_wheel";

const float MUZZLE_OFFSET = 0.0f; ///< Offset for spawning position of projectile (server side only)

//------------------------------------------------------------------------------
Wheel::Wheel() :
    radius_(0),
    pos_(0,0,0),
    prev_penetration_(0.0f),
    steer_factor_(0.0f),
    driven_(false),
    handbraked_(false)
{
}


//------------------------------------------------------------------------------
bool Wheel::collisionCallback(const physics::CollisionInfo & info)
{
    PROFILE(Wheel::collisionCallback);


    // This should happen only for replay simulator calls...  We don't
    // want to bother with all the wheel and heightfield category
    // stuff in replay sim as well, so just ignore the collision and
    // accept the performance penalty.
    if (info.other_geom_->getType() == physics::GT_HEIGHTFIELD) return false;
    
    if (info.type_                  == physics::CT_STOP) return false;
    if (info.other_geom_->getName() == VOID_GEOM_NAME)   return false;
    if (info.other_geom_->getName() == WATER_GEOM_NAME)  return false;

    float cur_penetration = calcRayPenetration(info);
    if (cur_penetration > collision_info_.penetration_)
    {
        collision_info_ = info;
        collision_info_.penetration_ = cur_penetration;
    }

    return false;
}



//------------------------------------------------------------------------------
/**
 *  The ray used for collision detection is of length 2*radius. The
 *  penetration returned is actually the distance from ray origin to
 *  contact point, so we have to do a little calculation here to find
 *  the actual penetration.
 */
float Wheel::calcRayPenetration(const physics::CollisionInfo & info)
{
    const Vector & up = info.this_geom_->getBody()->getTransform().getY();
    float penetration = vecDot(&up, &info.n_) * (2*radius_ - info.penetration_);

    // Limit wheel penetration to a fraction of the wheel radius.
    return std::min(penetration, radius_*0.3f);

}


//------------------------------------------------------------------------------
Tank::~Tank()
{
    for(unsigned w=0; w < NUM_WEAPON_SLOTS; w++)
    {
        delete weapon_system_[w];
    }
}


//------------------------------------------------------------------------------
void Tank::frameMove(float dt)
{
    setSleeping(false);
    
    PROFILE(Tank::frameMove);
    
    Controllable::frameMove(dt);

    bool firing = false;
    
    firing |= weapon_system_[0]->fire(input_.fire1_);
    firing |= weapon_system_[1]->fire(input_.fire2_);
    firing |= weapon_system_[2]->fire(input_.fire3_);

    if (firing)
    {
        stopHealing();
        setInvincible(false);
    }
    
    
    for(unsigned w=0; w < NUM_WEAPON_SLOTS; w++)
    {
        weapon_system_[w]->frameMove(dt);
    }

    if (is_locally_controlled_ && getLocation() == CL_CLIENT_SIDE)
    {
        frameMoveTurret(dt, true);
    }
    frameMoveTurret(dt, false);

    handleExtraDampening();

    ADD_STATIC_CONSOLE_VAR(bool, calc_tire_physics, true);
    if (calc_tire_physics) frameMoveTires(dt);

    main_reload_time_ -= dt;
    if (main_reload_time_ < 0.0f) main_reload_time_ = 0.0f;


    // object is positioned by visal on client side. Avoid redundant
    // positioning
    if (location_ == CL_SERVER_SIDE) positionCarriedObject();
}

//------------------------------------------------------------------------------
void Tank::writeInitValuesToBitstream (RakNet::BitStream & stream) const
{
    Controllable::writeInitValuesToBitstream(stream);
}

//------------------------------------------------------------------------------
void Tank::readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state)
{
    Controllable::readInitValuesFromBitstream(stream, game_state);


    s_console.addVariable("stabilization_enabled", &turret_stabilization_enabled_, &fp_group_);
}

//------------------------------------------------------------------------------
/**
 *  Need to write all values affecting history replay.
 */
void Tank::writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const
{
    Controllable::writeStateToBitstream(stream, type);


    if (type & OST_CORE)
    {
        stream.Write(doing_stabilization_); // required for client
                                            // side prediction test
                                            // controllable...
        stream.Write(target_yaw_);
        stream.Write(main_reload_time_);
        
        if (!doing_stabilization_) stream.Write(target_pitch_);
        else                       stream.Write(last_target_global_pitch_);        

        stream.Write(steer_angle_);
    }

    if (type & OST_EXTRA)
    {
        HitpointTracker::writeStateToBitstream(stream);

        for(unsigned w=0; w < NUM_WEAPON_SLOTS; w++)
        {
            weapon_system_[w]->writeStateToBitstream(stream);
        }
    }
}

//------------------------------------------------------------------------------
void Tank::readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp)
{
    bool can_fire = !input_.fire1_ && main_reload_time_ == 0.0f;
    
    Controllable::readStateFromBitstream(stream, type, timestamp);

    if (type & OST_CORE)
    {
        stream.Read(doing_stabilization_);
        
        stream.Read(target_yaw_);
        stream.Read(main_reload_time_);

        if (!doing_stabilization_) stream.Read(target_pitch_);
        else                       stream.Read(last_target_global_pitch_);

        stream.Read(steer_angle_);
    }

    if (type & OST_EXTRA)
    {
        HitpointTracker::readStateFromBitstream(stream);

        for(unsigned w=0; w < NUM_WEAPON_SLOTS; w++)
        {
            weapon_system_[w]->readStateFromBitstream(stream);
        }
    }

    // XXXX hack to avoid missing fire feedback from other
    // clients. When input is sent for the remote tank, its reloading
    // time is already different from zero. better ways: react to
    // projectile creation? send firing as own command?
    if (!is_locally_controlled_     &&
        location_ == CL_CLIENT_SIDE &&
        can_fire                    &&
        input_.fire1_               &&
        main_reload_time_ != 0.0f)
    {
        float t = main_reload_time_;

        main_reload_time_ = 0.0f;
        weapon_system_[0]->fire(input_.fire1_);
        main_reload_time_ = t;
    } 
}

//------------------------------------------------------------------------------
void Tank::handleProxyInterpolation()
{
    Controllable::handleProxyInterpolation();

    float turret_proxy_interpolation_factor = params_.get<float>("tank.turret_proxy_interpolation_factor");

    if (is_locally_controlled_ && doing_stabilization_)
    {
        last_proxy_global_pitch_ += (last_target_global_pitch_ -
                                     last_proxy_global_pitch_) * turret_proxy_interpolation_factor;
    } else
    {
        proxy_pitch_   += (target_pitch_   - proxy_pitch_)   * turret_proxy_interpolation_factor;
    }

    // issues with 2pi periodicity and interpolation...
    if      (target_yaw_ - proxy_yaw_ > PI) proxy_yaw_ += 2*PI;
    else if (proxy_yaw_ - target_yaw_ > PI) proxy_yaw_ -= 2*PI;
    proxy_yaw_   += (target_yaw_   - proxy_yaw_)   * turret_proxy_interpolation_factor;
}



//------------------------------------------------------------------------------
void Tank::setLocallyControlled(bool b)
{
    Controllable::setLocallyControlled(b);
    
    if (!b)
    {
        // stop firing when set to uncontrolled, to avoid stuck effects
        for(unsigned w=0; w < NUM_WEAPON_SLOTS; w++)
        {
            weapon_system_[w]->fire(false);
        }
    }
}



//------------------------------------------------------------------------------
void Tank::getMuzzleTransform(Matrix * res)
{
    Matrix tank = target_object_->getTransform();
    
    Matrix pitch, yaw;
    pitch.loadCanonicalRotation(-target_pitch_, 0);
    yaw.  loadCanonicalRotation( target_yaw_,   1);


    yaw.getTranslation()   = turret_pos_;
    pitch.getTranslation() = barrel_pos_;
    
    Matrix off_muzzle(true);
    off_muzzle.getTranslation().z_ = -MUZZLE_OFFSET;
    
    *res = tank * yaw * pitch * off_muzzle;
}


//------------------------------------------------------------------------------
void Tank::getProxyTurretPos(float & yaw, float & pitch) const
{
    yaw   = proxy_yaw_;
    pitch = proxy_pitch_;
}

//------------------------------------------------------------------------------
bool Tank::isStateEqual(const Controllable * other) const
{
    ADD_STATIC_CONSOLE_VAR(float, state_diff_yaw, 0.0f);
    ADD_STATIC_CONSOLE_VAR(float, state_diff_pitch, 0.0f);

    
    assert(dynamic_cast<const Tank*>(other));
    Tank * tank = (Tank*)other;


    state_diff_yaw   = abs(target_yaw_   - tank->target_yaw_);

    if (doing_stabilization_)
    {
        state_diff_pitch = abs(last_target_global_pitch_ - tank->last_target_global_pitch_);
    } else
    {
        state_diff_pitch = abs(target_pitch_ - tank->target_pitch_);
    }

    
    if (state_diff_yaw   > 0.001)
    {
        s_log << Log::debug('n')
              << "State different: state_diff_yaw is "
              << state_diff_yaw
              << "\t\tYYYYYYY\n";
        return false; // PPPP
    }
    if (state_diff_pitch > 0.001)
    {
        s_log << Log::debug('n')
              << "State different: state_diff_pitch is "
              << state_diff_pitch
              << "\t\tPPPPPPP\n";
        return false;
    }
    
    return Controllable::isStateEqual(other);
}


//------------------------------------------------------------------------------
float Tank::getMainGunReloadTime() const
{
    return main_reload_time_;
}

//------------------------------------------------------------------------------
void Tank::setMainGunReloadTime(float t)
{
    main_reload_time_ = t;
}

//------------------------------------------------------------------------------
RigidBody * Tank::getCarriedObject() const
{
    return carried_object_;
}

//------------------------------------------------------------------------------
/**
 *  Causes the object to be positioned in front of the tank every
 *  frame.
 *
 *  Adds all non-sensor geoms from object to the tank's
 *  body. Collision callbacks for tank are not installed on the picked
 *  up object!
 */
bool Tank::pickupObject(RigidBody * object)
{
    assert(!carried_object_);
    
    carried_object_ = object;
    Vector docking_offset = params_.get<Vector>("tank.docking_pos");

    if (getLocation() == CL_SERVER_SIDE)
    {
        // First we have to check whether the LOS to the object is given.

        // Use turret pos because tank position will likely be below terrain...
        Vector tank_pos    = target_object_->getPosition() + target_object_->vecToWorld(turret_pos_);
        Vector docking_pos = target_object_->getPosition() + target_object_->vecToWorld(docking_offset);
        Vector ab = docking_pos - tank_pos;

        pickup_los_given_ = true;
    
        physics::OdeRayGeom ray(ab.length());
        ray.set(tank_pos, ab);
        target_object_->getSimulator()->getStaticSpace()->collide(
            &ray, physics::CollisionCallback(this, &Tank::pickupRayCollisionCallback));
        if (pickup_los_given_)
        {
            target_object_->getSimulator()->getActorSpace()->collide(
                &ray, physics::CollisionCallback(this, &Tank::pickupRayCollisionCallback));
        }

        if (!pickup_los_given_)
        {
            carried_object_ = NULL;
            return false;
        }
    }
    
    s_log << Log::debug('l')
          << *this
          << " now carries "
          << *object
          << "\n";

    physics::OdeRigidBody * obj_body  = object->getProxy() ? object->getProxy() : object->getTarget();
    physics::OdeRigidBody * this_body = getProxy()         ? getProxy()         : getTarget();

    Matrix offset(true);
    offset.getTranslation() = docking_offset;
    
    for (unsigned g=0; g<obj_body->getGeoms().size(); ++g)
    {
        if (obj_body->getGeoms()[g]->isSensor()) continue;
        
        physics::OdeGeom * clone = obj_body->getGeoms()[g]->instantiate();

        clone->setName(clone->getName() + "-clone");
        clone->setMass(0.0f);
        clone->setOffset(offset);
        clone->setSpace(this_body->getSimulator()->getActorSpace());
        
        this_body->addGeom(clone);

        clone->setCategory(obj_body->getGeoms()[g]->getCategory());

        ++num_carried_object_geoms_;
    }

    return true;
}


//------------------------------------------------------------------------------
void Tank::dropObject()
{
    assert(carried_object_);

    positionCarriedObject();
    
    s_log << Log::debug('l')
          << *this
          << " dropped "
          << *carried_object_
          << "\n";
    
    carried_object_ = NULL;

    physics::OdeRigidBody * this_body = getProxy() ? getProxy() : getTarget();
    for (unsigned i=0; i< num_carried_object_geoms_; ++i)
    {
        delete this_body->detachGeom(this_body->getGeoms().size()-1);
    }
    num_carried_object_geoms_ = 0;
}


//------------------------------------------------------------------------------
void Tank::applyRecoil(const Vector & force, const Vector & pos)
{
    target_object_->addGlobalForceAtGlobalPos(
        force*s_params.get<float>("physics.fps"),
        pos);

    turret_stabilization_enabled_ = false;
    s_scheduler.addEvent(SingleEventCallback(this, &Tank::enableStabilization),
                         params_.get<float>("tank.turret_stabilization_disable_time"),
                         NULL,
                         "Tank::enableStabilization",
                         &fp_group_);
}

//------------------------------------------------------------------------------
WeaponSystem ** Tank::getWeaponSystems()
{
    return &weapon_system_[0];
}

//------------------------------------------------------------------------------
void Tank::setNetworkFlagDirty()
{
    network_state_dirty_ = true;
}

//------------------------------------------------------------------------------
void Tank::positionCarriedObject()
{
    if (!carried_object_) return;

    Matrix target_transform = getTransform(proxy_object_ != NULL);
    Vector & pos = target_transform.getTranslation();

    Vector docking_pos = params_.get<Vector>("tank.docking_pos");

    pos += docking_pos.x_ * target_transform.getX();
    pos += docking_pos.y_ * target_transform.getY();
    pos += docking_pos.z_ * target_transform.getZ();

    carried_object_->setTransform(target_transform);
}


//------------------------------------------------------------------------------
bool Tank::isBraking() const
{
    return is_braking_;
}


//------------------------------------------------------------------------------
bool Tank::hasRamUpgrade() const
{
    return params_.get<bool>("tank.ram_upgrade");
}

//------------------------------------------------------------------------------
bool Tank::hasMineProtection() const
{
    return params_.get<bool>("tank.mine_protection");
}

//------------------------------------------------------------------------------
bool Tank::hasInertialDampener() const
{
    return params_.get<bool>("tank.inertial_dampener");
}

//------------------------------------------------------------------------------
float Tank::getMaxSpeed() const
{
    return max_speed_;
}


//------------------------------------------------------------------------------
const std::vector<Wheel> & Tank::getWheel() const
{
    return wheel_;
}

//------------------------------------------------------------------------------
float Tank::getSteerAngle() const
{
    return steer_angle_;
}

//------------------------------------------------------------------------------
const LocalParameters & Tank::getParams() const
{
    return params_;
}

//------------------------------------------------------------------------------
void Tank::loadParameters(const std::string & filename, const std::string & super_section)
{
    ParameterLoadCallback callback(this, &Tank::parameterLoadCallback);

    try
    {
        
        params_.loadParameters(filename, super_section, &callback);
        setCollisionDamageParameters(params_.get<float>("tank.collision_damage_speed_threshold"),
                                     params_.get<float>("tank.collision_min_damage"),
                                     params_.get<float>("tank.collision_max_damage"));
        
    } catch (Exception & e)
    {
        e.addHistory("Tank::loadParameters(" + filename + ", " + super_section + ")");
        throw e;
    }


    steer_speed_          = params_.get<float>("tank.steer_speed");
    max_steer_angle_      = params_.get<float>("tank.max_steer_angle");    
    steer_retreat_factor_ = params_.get<float>("tank.steer_retreat_factor");
    suspension_k_         = params_.get<float>("tank.suspension_k");
    suspension_d_         = params_.get<float>("tank.suspension_d");
    force_dependent_slip_ = params_.get<float>("tank.force_dependent_slip");
    static_mu_lat_brake_  = params_.get<float>("tank.static_mu_lat_brake");
    static_mu_lat_        = params_.get<float>("tank.static_mu_lat");
    static_mu_long_brake_ = params_.get<float>("tank.static_mu_long_brake");
    static_mu_long_acc_   = params_.get<float>("tank.static_mu_long_acc");
    engine_brake_mu_      = params_.get<float>("tank.engine_brake_mu");
    rolling_mu_           = params_.get<float>("tank.rolling_mu");


    turret_stabilization_threshold_ = params_.get<float>("tank.turret_stabilization_threshold");
    max_barrel_pitch_     = params_.get<float>("tank.max_barrel_pitch");
    min_barrel_pitch_     = params_.get<float>("tank.min_barrel_pitch");
    max_yaw_speed_        = params_.get<float>("tank.max_yaw_speed");
    max_barrel_yaw_       = params_.get<float>("tank.max_barrel_yaw");
    max_pitch_speed_      = params_.get<float>("tank.max_pitch_speed");
    
    inv_gravity_ = 1.0f / s_params.get<float>("physics.gravity");

    
}

//------------------------------------------------------------------------------
bool Tank::isBeaconActionExecuted() const
{
    return beacon_action_executed_;
}

//------------------------------------------------------------------------------
void Tank::setBeaconActionExecuted(bool v)
{
    beacon_action_executed_ = v;
}


//------------------------------------------------------------------------------
/**
 *  Assigns all target wheel rays to the specified category.
 */
void Tank::setWheelCategory(unsigned wheel_category)
{
    for (unsigned geom=0; geom < target_object_->getGeoms().size(); ++geom)
    {
        physics::OdeGeom * cur_geom = target_object_->getGeoms()[geom];
        std::string name = cur_geom->getName();

        // We are only interested in wheel geoms
        if (name.find(WHEEL_NAME) != 0) continue;
        
        cur_geom->setCategory(wheel_category, target_object_->getSimulator());
    }
}

//------------------------------------------------------------------------------
void Tank::setTerrainData(const terrain::TerrainData * data)
{
    terrain_data_ = data;
}

//------------------------------------------------------------------------------
const terrain::TerrainData * Tank::getTerrainData()
{
    return terrain_data_;
}

//------------------------------------------------------------------------------
Tank::Tank() :
    steer_angle_(0.0f),
    target_yaw_  (0.0f),
    target_pitch_(0.0f),
    proxy_yaw_   (0.0f),
    proxy_pitch_ (0.0f),
    last_target_global_pitch_(0.0f),
    last_proxy_global_pitch_(0.0f),
    main_reload_time_(0.0f),
    carried_object_(NULL),
    num_carried_object_geoms_(0),
    pickup_los_given_(false),
    is_braking_(false),
    turret_stabilization_enabled_(true),
    doing_stabilization_(true),
    beacon_action_executed_(false),
    max_speed_(0.0f)
{
    addStateObserver(ObserverCallbackFun0(this, &Tank::setNetworkFlagDirty));
    setHealParameters(s_params.get<float>("server.logic.tank_heal_start_val"),
                      s_params.get<float>("server.logic.tank_heal_increment"));
    
    memset(weapon_system_, 0, sizeof(weapon_system_));

    s_console.addVariable("doing_stabilization", &doing_stabilization_, &fp_group_);

}


//------------------------------------------------------------------------------
Tank::Tank(const Tank & other, physics::OdeSimulator * sim) :
    Controllable(other, sim),
    steer_angle_(other.steer_angle_),
    target_yaw_(other.target_yaw_),
    target_pitch_(other.target_pitch_),
    proxy_yaw_(other.proxy_yaw_),
    proxy_pitch_(other.proxy_pitch_),
    last_target_global_pitch_(other.last_target_global_pitch_),
    last_proxy_global_pitch_(other.last_proxy_global_pitch_),
    
    turret_pos_(other.turret_pos_),
    barrel_pos_(other.barrel_pos_),

    main_reload_time_(other.main_reload_time_),
    carried_object_(NULL),
    num_carried_object_geoms_(0),
    pickup_los_given_(false),

    is_braking_     (other.is_braking_),

    turret_stabilization_enabled_(other.turret_stabilization_enabled_),
    doing_stabilization_(other.doing_stabilization_),
    beacon_action_executed_(false),

    max_speed_(0.0f)
{
    addStateObserver(ObserverCallbackFun0(this, &Tank::setNetworkFlagDirty));
    
    memset(weapon_system_, 0, sizeof(weapon_system_));

    loadParameters(CONFIG_PATH + "tanks.xml");
    loadParameters(CONFIG_PATH + "tanks.xml", name_); // XXXX replace this by include directive

    initializeWheels();
    initializeWeaponSystems();
}



//------------------------------------------------------------------------------
void Tank::init()
{
    loadParameters(CONFIG_PATH + "tanks.xml");
    loadParameters(CONFIG_PATH + "tanks.xml", name_); // XXXX replace this by include directive

    setHitpoints(max_hitpoints_);

    initializeTurret();
    initializeWheels();
    initializeWeaponSystems();
    
    Controllable::init();
}


//------------------------------------------------------------------------------
void Tank::frameMoveTurret(float dt, bool proxy)
{
    float *pitch, *yaw;
    float *last_global_pitch;
    physics::OdeRigidBody * body;
    
    if (proxy)
    {
        assert(proxy_object_);
        
        pitch             = &proxy_pitch_;
        yaw               = &proxy_yaw_;
        last_global_pitch = &last_proxy_global_pitch_;
        body              = proxy_object_;
    } else
    {
        pitch             = &target_pitch_;
        yaw               = &target_yaw_;
        last_global_pitch = &last_target_global_pitch_;
        body              = target_object_;
    }


    // Turret Stabilization:
    //
    // Constraint one: viewing direction must have the given global pitch (alpha is angle to up vector)
    // Constraint two: viewing direction must have the given local yaw.
    //
    // Result: Two circles on the unit sphere, their intersection is the solution.
    

    // First transform constraint one into local coordinates. Rotate
    // by -yaw to align constraint two with yz-plane.
    Matrix rot_yaw(true);
    rot_yaw.loadCanonicalRotation(-*yaw, 1);
    Vector y_glob_rot = rot_yaw.transformVector(body->vecFromWorld(Vector(0,1,0)));

    // only do stabilization if we are mostly upright
    if (y_glob_rot.y_ >= turret_stabilization_threshold_ && 
        turret_stabilization_enabled_ &&
        params_.get<bool>("tank.turret_stabilization"))
    {
        doing_stabilization_ = true;
        // desired angle to global y axis
        float cos_alpha = cosf(0.5*PI - *last_global_pitch);

        // now find the intersection of a*y+b*z=c (desired angle of
        // solution to global x axis (dot product)) with the circle
        // y*y + z*z = 1 (unit length of solution).
        const float & a = y_glob_rot.y_;
        const float & b = y_glob_rot.z_;
        const float & c = cos_alpha;

        float inv_a_sqr = 1.0f / (a*a);
    
        float u =    b*b*inv_a_sqr +1;
        float v = -2*b*c*inv_a_sqr;
        float w =    c*c*inv_a_sqr -1;

        float s = v*v - 4*u*w;
        if ( s > EPSILON)
        {
            float z = (0.5*(-v-sqrtf(s))/u); // we only want the solution with negative z (heading forward)
            float y = (c-b*z)/a;

            *pitch = atan2f(y,-z); // angle of solution to xz plane is our new pitch
        }
    } else
    {
        doing_stabilization_ = false;

        // global pitch will change without user input as tank moves
        // across terrain, update the new global target pitch
        Matrix r(true);
        r.loadOrientationPart(-*yaw, -*pitch);
        Vector target_heading_global = body->vecToWorld(-r.getZ());
        *last_global_pitch = atan2f(target_heading_global.y_, sqrtf(target_heading_global.x_*target_heading_global.x_ +
                                                                    target_heading_global.z_*target_heading_global.z_));
    }
    
    float max_barrel_pitch = max_barrel_pitch_;
    float min_barrel_pitch = min_barrel_pitch_;
    float prev_pitch = *pitch;
    if (input_.delta_yaw_ || input_.delta_pitch_)
    {
        // input_.delta_yaw_,input_.delta_pitch_ are the target angular
        // velocity of the turret.
        float max_yaw_speed    = max_yaw_speed_;
        float max_barrel_yaw   = max_barrel_yaw_;    
        float max_pitch_speed  = max_pitch_speed_;

        input_.delta_yaw_   = clamp(input_.delta_yaw_,   -max_yaw_speed,   max_yaw_speed);
        input_.delta_pitch_ = clamp(input_.delta_pitch_, -max_pitch_speed, max_pitch_speed);
        
        *yaw   += input_.delta_yaw_   * dt;
        *pitch += input_.delta_pitch_ * dt;
        
        *yaw = normalizeAngle(*yaw);

        if (max_barrel_yaw != 0)
        {
            if (*yaw > PI) *yaw -= 2*PI;
            *yaw = clamp(*yaw, -max_barrel_yaw, max_barrel_yaw);
        }
    }
    
    *pitch = clamp(*pitch, min_barrel_pitch, max_barrel_pitch);
    *last_global_pitch += *pitch - prev_pitch;
}

//------------------------------------------------------------------------------
/**
 *  Updates wheel rotation and adds contact joints for tire ray
 *  contact points.
 */
void Tank::frameMoveTires(float dt)
{
    // Handle steering angle
    // Return to center position very quickly
    if (sign(steer_angle_) != input_.left_ - input_.right_)
    {
        steer_angle_ *= steer_retreat_factor_;
    }
    if (input_.left_ - input_.right_)
    {        
        steer_angle_ += dt * steer_speed_ * (input_.left_ - input_.right_);

        if (abs(steer_angle_) > max_steer_angle_)
        {
            steer_angle_ = sign(steer_angle_) * max_steer_angle_;
        }
    }


    float dir_vel = -target_object_->getLocalLinearVel().z_;

    is_braking_ = ((input_.up_ - input_.down_) > 0 && dir_vel < 0) ||
                  ((input_.up_ - input_.down_) < 0 && dir_vel > 0);
    

//--------------------------------------------------------------------------------
    // Don't calc full tire physics for uncontrolled objects on
    // client.
    if (getLocation() != CL_SERVER_SIDE && !is_locally_controlled_) return;
//--------------------------------------------------------------------------------
    
    Matrix tank_transform = getTransform();
    Vector dir = -tank_transform.getZ();

    // Calc erp and cfm from suspension spring parameters (see ODE docs)
    float cfm = 1.0f / (dt* suspension_k_ + suspension_d_);
    float erp = dt* suspension_k_*cfm;

    dContact contact;
    memset(&contact, 0, sizeof(contact));

    contact.surface.mode =
        dContactSoftERP | dContactSoftCFM |
        dContactFDir1 | dContactApprox1 | dContactSlip2 |
        dContactMu2;
    
    contact.surface.soft_erp = erp;
    contact.surface.soft_cfm = cfm;


    // Force-dependent-slip is proportional to long. moving speed
    contact.surface.slip2 = abs(dir_vel) * force_dependent_slip_;

    
    for (unsigned w=0; w<wheel_.size(); ++w)
    {
        bool braking = is_braking_ || (wheel_[w].handbraked_ && input_.action2_);

        doTerrainWheelCollision(wheel_[w], tank_transform, true);

        if (wheel_[w].collision_info_.penetration_ != 0.0f)
        {
            contact.surface.mu2 = (braking ? static_mu_lat_brake_ : static_mu_lat_) * inv_gravity_;

            Vector fdir = dir - wheel_[w].collision_info_.n_ * vecDot(&dir, &wheel_[w].collision_info_.n_);
            fdir.normalize();

            Vector right;
            vecCross(&right, &wheel_[w].collision_info_.n_, &fdir);

            float wheel_steer_angle = steer_angle_ * wheel_[w].steer_factor_;
            fdir = cosf(wheel_steer_angle) * fdir + sinf(wheel_steer_angle) * right;
            
            contact.fdir1[0] = fdir.x_;
            contact.fdir1[1] = fdir.y_;
            contact.fdir1[2] = fdir.z_;

            if (braking)
            {
                contact.surface.mode &= ~dContactMotion1;

                contact.surface.mu = static_mu_long_brake_ * inv_gravity_;
            } else if (wheel_[w].driven_)
            {
                if (input_.up_ != input_.down_)
                {
                    contact.surface.mode |= dContactMotion1;
                    contact.surface.motion1 = (input_.up_ - input_.down_) * max_speed_;
                
                    contact.surface.mu = static_mu_long_acc_ * inv_gravity_;
                } else
                {
                    contact.surface.mode &= ~dContactMotion1;

                    contact.surface.mu = engine_brake_mu_ * inv_gravity_;
                }
            } else
            {
                // Cannot set zero here or fdir is ignored
                contact.surface.mode &= ~dContactMotion1;

                contact.surface.mu = rolling_mu_ * inv_gravity_;
            }

            contact.geom.pos[0]    = wheel_[w].collision_info_.pos_.x_;
            contact.geom.pos[1]    = wheel_[w].collision_info_.pos_.y_;
            contact.geom.pos[2]    = wheel_[w].collision_info_.pos_.z_;
            
            contact.geom.normal[0] = wheel_[w].collision_info_.n_.x_;
            contact.geom.normal[1] = wheel_[w].collision_info_.n_.y_;
            contact.geom.normal[2] = wheel_[w].collision_info_.n_.z_;
            
            contact.geom.depth     = wheel_[w].collision_info_.penetration_;
            contact.geom.g1        = wheel_[w].collision_info_.this_geom_->getId();


            target_object_->getSimulator()->addContactJoint(contact, target_object_, NULL);
        }
        
        // Reset penetration for next frame.
        wheel_[w].collision_info_.penetration_ = 0.0f;
    }
}

//------------------------------------------------------------------------------
void Tank::initializeWheels()
{
    std::vector<float> steer_factor       = params_.get<std::vector<float> >("tank.steer_factor");
    std::vector<bool>  wheel_driven       = params_.get<std::vector<bool>  >("tank.wheel_driven");
    std::vector<bool>  wheel_handbraked   = params_.get<std::vector<bool>  >("tank.wheel_handbraked");

    if (steer_factor.size() != wheel_driven.size() ||
        steer_factor.size() != wheel_handbraked.size())        
    {
        throw Exception("Params steer_factor, wheel_driven, wheel_handbraked, calc_wheel_physics have different length");
    }
    
    wheel_.resize(steer_factor.size());    
    
    for (unsigned geom=0; geom < target_object_->getGeoms().size(); ++geom)
    {
        physics::OdeGeom * cur_geom = target_object_->getGeoms()[geom];
        std::string name = cur_geom->getName();

        // We are only interested in wheel geoms
        if (name.find(WHEEL_NAME) != 0) continue;
        
        name = name.substr(WHEEL_NAME.size());

        std::istringstream strstr(name);
        unsigned number;
        strstr >> number;
        --number; // Numbering starts with 1 in modeler

        if (cur_geom->getType() != physics::GT_RAY) throw Exception("Wheels must be of type ray.\n");
        if (number >= wheel_.size()) throw Exception("wheel_driven, steer_factor entries don't exist for all wheels.\n");
        if (wheel_[number].radius_ != 0) throw Exception("Wheel exists more than once.\n");


        physics::OdeRayGeom * cur_wheel_geom = (physics::OdeRayGeom*)cur_geom;        

        // Only server side sim and local tank on player have their
        // target geoms still in a space, so we can always set this.
        cur_wheel_geom->setCollisionCallback(physics::CollisionCallback(&wheel_[number], &Wheel::collisionCallback));

        
        wheel_[number].radius_ = cur_wheel_geom->getLength() * 0.5;

        wheel_[number].collision_info_.this_geom_ = cur_wheel_geom;
        wheel_[number].pos_ = cur_wheel_geom->getTransform().getTranslation() + target_object_->getCog();
        wheel_[number].pos_.y_ -= cur_wheel_geom->getLength();


        // Copy params
        wheel_[number].steer_factor_ = steer_factor    [number];
        wheel_[number].driven_       = wheel_driven    [number];
        wheel_[number].handbraked_   = wheel_handbraked[number];
    }


    // Make sure all wheel geoms were set
    for (unsigned w=0; w<wheel_.size(); ++w)
    {
        if (wheel_[w].collision_info_.this_geom_ == NULL)
        {
            throw Exception("Number of wheel params doesn't match number of wheels in model, or"
                            " wheels are not correctly ordered.");
        }
        
    }
}

//------------------------------------------------------------------------------
void Tank::initializeTurret()
{
    bool found_turret = false;
    bool found_barrel = false;
    
    for (int geom=0; geom<(int)target_object_->getGeoms().size(); ++geom)
    {
        physics::OdeGeom * cur_geom = target_object_->getGeoms()[geom];
        if (cur_geom->getName() == "turret")
        {
            turret_pos_ = cur_geom->getTransform().getTranslation();
            found_turret = true;
        } else if (cur_geom->getName() == "barrel")
        {
            barrel_pos_ = cur_geom->getTransform().getTranslation();
            found_barrel = true;
        } else continue;

        target_object_->deleteGeom(geom);
        if (proxy_object_) proxy_object_->deleteGeom(geom);

        geom-=1; // indices have shifted because of deletion
    }

    if (!found_barrel || !found_turret)
    {
        throw Exception("Tank model is missing turret and/or barrel");
    }

    // We want relative positions
    barrel_pos_ -= turret_pos_;
    turret_pos_ += target_object_->getCog(); // Geoms are relative to COG
}

//------------------------------------------------------------------------------
void Tank::initializeWeaponSystems()
{
    // Check for uninitialized weapon slots
    for (unsigned w=0; w<NUM_WEAPON_SLOTS; ++w)
    {
        if (!weapon_system_[w])
        {
            s_log << Log::warning
                  << "Weapon slot "
                  << w
                  << " was not initialized. Creating dummy weapon.\n";
            weapon_system_[w] = s_weapon_system_loader.create("WeaponSystem");
            weapon_system_[w]->reset(this, "dummy_weapon_system");
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  Apply strong dampening if tank is aloft to avoid counterintuitive
 *  bouncing behaviour.
 */
void Tank::handleExtraDampening()
{
    for (unsigned w=0; w<wheel_.size(); ++w)
    {
        // Bail if any wheel has contact
        if (wheel_[w].prev_penetration_ != 0.0f)
        {
            return;
        }
    }

    // Apply angular dampening
    Vector w = target_object_->getLocalAngularVel();
    w = target_object_->getInertiaTensor().transformVector(w);
    target_object_->addLocalTorque(-params_.get<float>("tank.aloft_ang_dampening") * w);


    // Apply linear dampening to get nose down while flying
    Vector p = target_object_->getCog();
    p.z_ += params_.get<float>("tank.drag_point_offset");
    
    Vector v = target_object_->getLocalLinearVel();
    target_object_->addLocalForceAtLocalPos(-v*params_.get<float>("tank.aloft_lin_dampening"), p);
}


//------------------------------------------------------------------------------
/**
 *  Applying recoil only looks good if stabilization is disabled. Here
 *  it is re-enabled.
 */
void Tank::enableStabilization(void*)
{
    turret_stabilization_enabled_ = true;
}



//------------------------------------------------------------------------------
/**
 *  Used as substitute for "event-driven" parameter loading
 */
void Tank::parameterLoadCallback(const std::string & key,
                                 const std::string & value)
{
    if (key == "tank.weapon_slot")
    {
        std::vector<std::string> slot_info = params_.get<std::vector<std::string> >(key);

        if (slot_info.size() != 3)
        {
            Exception e;
            e << "Incorrect weapon slot format :"
              << key
              << ": "
              << value;
            throw e;
        }
        
        unsigned slot = fromString<unsigned>(slot_info[0]);
        if (slot >= NUM_WEAPON_SLOTS)
        {
            Exception e;
            e << "Incorrect weapon slot: "
              << slot;
            throw e;
        }

        // Replace weapon system if neccessary
        if (!weapon_system_[slot] ||
            slot_info[1] != weapon_system_[slot]->getType())
        {
            delete weapon_system_[slot];
            weapon_system_[slot] = s_weapon_system_loader.create(slot_info[1]);
        }

        // Now load the new parameters into system
        weapon_system_[slot]->reset(this, slot_info[2]);
    } else if (key == "tank.delta_max_speed")
    {
        max_speed_ += params_.get<float>(key);
    } else if (key == "tank.delta_max_hitpoints")
    {
        int delta = params_.get<int>(key);
        max_hitpoints_ += delta;
        setHitpoints(max_hitpoints_);
    }
}


//------------------------------------------------------------------------------
void Tank::doTerrainWheelCollision(Wheel & wheel, const Matrix & tank_transform, bool bicubic)
{
    terrain_data_->collideRay(wheel.collision_info_,
                              tank_transform.transformPoint(wheel.pos_),
                              tank_transform.getY(),
                              wheel.prev_penetration_,
                              bicubic);    

    wheel.prev_penetration_ = wheel.collision_info_.penetration_;
}


//------------------------------------------------------------------------------
/**
 *  Used to check whether LOS to the object about to be picked up is
 *  given. The object to be picked up is ignored and must already be
 *  stored in carried_object_.
 *
 *  Sets pickup_los_given_ to false if hitting anything other than
 *  water, the tank or the object to pick up.
 */
bool Tank::pickupRayCollisionCallback(const physics::CollisionInfo & info)
{
    assert(info.type_ == physics::CT_SINGLE);

    physics::OdeRigidBody * body = info.other_geom_->getBody();
    if (body)
    {
        RigidBody * rb = (RigidBody*)body->getUserData();
        
        if (rb->getType() == "Water") return false;
        if (rb == this)               return false;
        if (rb == carried_object_)    return false;
    }

    pickup_los_given_ = false;
    return false;
}


