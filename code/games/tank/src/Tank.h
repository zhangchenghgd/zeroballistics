#ifndef TANKGAME_TANK_INCLUDED
#define TANKGAME_TANK_INCLUDED


#include <string>

#include "Vector.h"
#include "Matrix.h"
#include "Controllable.h"
#include "Log.h"
#include "ParameterManager.h"
#include "HitpointTracker.h"

namespace terrain
{
    class TerrainData;
}

namespace osg
{
    class MatrixTransform;
}

namespace RakNet
{
    class BitStream;
}

namespace physics
{
    struct CollisionInfo;
}

class PlayerInput;
class WeaponSystem;

const unsigned NUM_WEAPON_SLOTS = 4;


//------------------------------------------------------------------------------
class Wheel
{
 public:
    Wheel();

    bool collisionCallback     (const physics::CollisionInfo & info);
    
    float radius_;
    
    physics::CollisionInfo collision_info_;

    Vector pos_; ///< Position of lowest wheel point relative to tank.

    float prev_penetration_; ///< Used to "seed" the ray length for bicubic interpolation.

    // Cached params...
    float steer_factor_;
    bool driven_;
    bool handbraked_;

 protected:
    float calcRayPenetration(const physics::CollisionInfo & info);
};


//------------------------------------------------------------------------------
class Tank : public Controllable, public HitpointTracker
{
 public:

    virtual ~Tank();

    GAME_OBJECT_IMPL(Tank);

    virtual void frameMove(float dt);    

    // used for object creation
    virtual void writeInitValuesToBitstream (RakNet::BitStream & stream) const;
    virtual void readInitValuesFromBitstream(RakNet::BitStream & stream, GameState * game_state, uint32_t timestamp);

    // used for state value transmission 
    virtual void writeStateToBitstream (RakNet::BitStream & stream, unsigned type) const;
    virtual void readStateFromBitstream(RakNet::BitStream & stream, unsigned type, uint32_t timestamp);

    virtual void handleProxyInterpolation();
    
    virtual void setLocallyControlled(bool b);

    virtual Tank * cloneForReplay(physics::OdeSimulator * sim) const;
    
    void getMuzzleTransform(Matrix * res);
    void getLauncherTransform(Matrix * res);

    void getTargetTurretPos(float & yaw, float & pitch)const;
    void getProxyTurretPos(float & yaw, float & pitch)const;

    bool isStateEqual(const Controllable * other) const;

    RigidBody * getCarriedObject() const;
    bool pickupObject(RigidBody * object);
    void dropObject();

    void applyRecoil(const Vector & force, const Vector & pos);

    WeaponSystem ** getWeaponSystems();

    void setNetworkFlagDirty();

    void positionCarriedObject();

    bool isBraking() const;

    void setHasRamUpgrade(bool b);
    bool hasRamUpgrade()       const;

    float getMaxSpeed() const;
    
    const std::vector<Wheel> & getWheel() const;
    float getProxySteerAngle() const;

    const LocalParameters & getParams() const;

    void loadParameters(const std::string & filename, const std::string & super_section = "");

    bool isBeaconActionExecuted() const;
    void setBeaconActionExecuted(bool v);

    void setWheelCategory(unsigned wheel_category);

    void activateBoost(bool activate);

    static void setTerrainData(const terrain::TerrainData * data);
    static const terrain::TerrainData * getTerrainData();
    
 protected:

    Tank();
    Tank(const Tank & other, physics::OdeSimulator * sim);
    virtual void init();

    void frameMoveTurret(float dt, bool proxy);
    void frameMoveTires(float dt);
    
    void initializeWheels();
    void initializeTurret();
    
    void createDummyWeaponSystems();

    void handleExtraDampening();

    void enableStabilization(void*);
    
    void parameterLoadCallback(const std::string & key,
                               const std::string & value);


    void doTerrainWheelCollision(Wheel & wheel, const Matrix & tank_transform, bool bicubic);

    bool pickupRayCollisionCallback(const physics::CollisionInfo & info);

    float target_steer_angle_; ///< Steering angle for all wheels. Gets
                                /// multiplied with steering_factor for each wheel.
    float proxy_steer_angle_;
    
    float target_yaw_;
    float target_pitch_;
    float proxy_yaw_;
    float proxy_pitch_;
    float last_target_global_pitch_; ///< Used for turret stabilization. pitch
                           ///and yaw are adjusted to maintain this
                           ///viewing direction. dir is in world coordinates
    float last_proxy_global_pitch_;
    

    Vector turret_pos_; ///< Pivot point of turret. Determined at initialization time.
    Vector barrel_pos_; ///< Barrel pos relative to turret. Determined at initialization time.
    Matrix launcher_pos_; ///< Position of missile launcher. Determined at initialization time.


    RigidBody * carried_object_;
    unsigned num_carried_object_geoms_;
    bool pickup_los_given_;

    bool is_braking_;
    
    std::vector<Wheel> wheel_;

    WeaponSystem * weapon_system_[NUM_WEAPON_SLOTS]; // This is NULL for replay sim tank

    bool turret_stabilization_enabled_; ///< Recoil causes disablement of stabilization
    bool doing_stabilization_; ///< If tank is not upright or
                               ///stabilization is disabled due to
                               ///recoil this is false. If
                               ///stabilization is off, directly use
                               ///target_pitch_ for state comparison,
                               ///else use last global pitch.

    LocalParameters params_;

    bool beacon_action_executed_;


    // ---------- Cached Parameters ----------

    // Delta values
    float max_speed_;

    // updateWheelPos
    float steer_speed_;
    float max_steer_angle_;
    float steer_retreat_factor_;

    // frameMoveTires
    float suspension_k_;
    float suspension_d_;
    float force_dependent_slip_;
    float static_mu_lat_brake_;
    float static_mu_lat_;
    float static_mu_long_brake_;
    float static_mu_long_acc_;
    float engine_brake_mu_;
    float rolling_mu_;
    float inv_gravity_;


    // frameMoveTurret
    float turret_stabilization_threshold_;
    float max_barrel_pitch_;
    float min_barrel_pitch_;
    float max_yaw_speed_;
    float max_barrel_yaw_;
    float max_pitch_speed_;
    
    /// for boost use
    bool boost_activated_;
    float prev_max_speed_; ///< Used to remember old values when using boost
    float prev_static_mu_long_acc_; ///< Used to remember old values when using boost

    static const terrain::TerrainData * terrain_data_; ///< Used for wheel collision.
};


#endif
