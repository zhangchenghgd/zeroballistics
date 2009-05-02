
#include "HitpointTracker.h"

#include <limits>

#include <raknet/RakNetTypes.h>
#include <raknet/BitStream.h>


#include "Console.h"
#include "Log.h"
#include "NetworkCommand.h" // for logging operator<<
#include "ParameterManager.h"
#include "Matrix.h"
#include "Vector.h"


#undef min
#undef max

const float COLLISION_DAMAGE_DISABLE_TIME = 0.1f;

const float HEAL_DT = 0.1f;


//------------------------------------------------------------------------------
HitpointTracker::HitpointTracker() :
    max_hitpoints_(0),
    hitpoints_(0),
    heal_increment_(0.0f),
    task_healing_(INVALID_TASK_HANDLE),
    is_invincible_(true),
    collision_damage_enabled_(true),
    cd_speed_threshold_(0.0f),
    cd_min_damage_(0.0f),
    cd_max_damage_(0.0f),
    armor_front_factor_(1.0f),
    armor_rear_factor_(1.0f),
    heal_increment_start_val_(0.0f),
    heal_increment_delta_(0.0f)
{
    s_console.addVariable("heal_increment", &heal_increment_, &fp_group_tracker_);
}



//------------------------------------------------------------------------------
HitpointTracker::HitpointTracker(const HitpointTracker & other) :
    max_hitpoints_(0), // delta values are added -> 0!
    hitpoints_(other.hitpoints_),
    heal_increment_(0.0f),
    task_healing_(INVALID_TASK_HANDLE),
    is_invincible_(other.is_invincible_),
    collision_damage_enabled_(other.collision_damage_enabled_),
    cd_speed_threshold_(other.cd_speed_threshold_),
    cd_min_damage_(other.cd_min_damage_),
    cd_max_damage_(other.cd_max_damage_),
    armor_front_factor_(other.armor_front_factor_),
    armor_rear_factor_(other.armor_rear_factor_),
    heal_increment_start_val_(other.heal_increment_start_val_),
    heal_increment_delta_(other.heal_increment_delta_)
{
}



//------------------------------------------------------------------------------
HitpointTracker::~HitpointTracker()
{
}


//------------------------------------------------------------------------------
void HitpointTracker::writeStateToBitstream (RakNet::BitStream & stream) const
{
    stream.Write(hitpoints_);
    stream.Write(is_invincible_);
    stream.Write(isGainingHealth());
}

//------------------------------------------------------------------------------
void HitpointTracker::readStateFromBitstream(RakNet::BitStream & stream)
{
    uint32_t new_hp;
    bool is_gaining_health;
    
    stream.Read(new_hp);
    stream.Read(is_invincible_);
    stream.Read(is_gaining_health);
    
    setHitpoints(new_hp);
    if (is_gaining_health)
    {
        startHealing();
        heal_increment_ = 1;
    } else
    {
        stopHealing();
    }
}
    
//------------------------------------------------------------------------------
void HitpointTracker::setHitpoints(int hp, bool dirty_state)
{
    hp = std::max(0, hp);
    hp = std::min(max_hitpoints_, (unsigned)hp);

    if ((unsigned)hp == hitpoints_) return;
  
    hitpoints_ = hp;

    if (hitpoints_ == max_hitpoints_) damage_dealt_.clear();

    if (dirty_state) observable_.emit(HTE_STATE_CHANGED);
}


//------------------------------------------------------------------------------
unsigned HitpointTracker::getHitpoints() const
{
    return hitpoints_;
}



//------------------------------------------------------------------------------
void HitpointTracker::setMaxHitpoints(unsigned max_hp)
{
    max_hitpoints_ = max_hp;
}

//------------------------------------------------------------------------------
unsigned HitpointTracker::getMaxHitpoints() const
{
    return max_hitpoints_;    
}


//------------------------------------------------------------------------------
/**
 *  To allow scheduling as event, takes a void* as param.
 */
void HitpointTracker::setInvincible(void * i)
{
    is_invincible_       = (bool)i;

    if (is_invincible_) setHitpoints(max_hitpoints_);
    
    observable_.emit(HTE_STATE_CHANGED);
}


//------------------------------------------------------------------------------
bool HitpointTracker::isInvincible() const
{
    return is_invincible_;    
}
    
//------------------------------------------------------------------------------
/**
 *  Deals the specified amount of damage if tank is not currently
 *  invincible.
 */
void HitpointTracker::dealWeaponDamage(int amount, const SystemAddress & damage_dealer, HIT_LOCATION location)
{
    if (amount <= 0) return;
    if (is_invincible_) return;
    if (hitpoints_ == 0) return;

    if      (location == HL_FRONT) amount = (int)(armor_front_factor_*amount);
    else if (location == HL_REAR ) amount = (int)(armor_rear_factor_ *amount);
    
    stopHealing();
    addAssistDamage(std::min((unsigned)amount,hitpoints_), damage_dealer);
    setHitpoints((int)hitpoints_ - amount);
}


//------------------------------------------------------------------------------
void HitpointTracker::dealCollisionDamage(float collision_speed, float max_speed,
                                          const SystemAddress & damage_dealer,
                                          float factor)
{
    if (!collision_damage_enabled_) return;
    if (is_invincible_) return;
    if (hitpoints_ == 0) return;

    if (collision_speed < cd_speed_threshold_) return;

    stopHealing();
    
    float speed_percentage;
    if (collision_speed > max_speed) speed_percentage = 1.0f;
    else speed_percentage = (collision_speed - cd_speed_threshold_) / (max_speed - cd_speed_threshold_);
 
    int damage = (int)round(factor*(cd_min_damage_ +
                                    speed_percentage * (cd_max_damage_ - cd_min_damage_)));


    addAssistDamage(std::min((unsigned)damage, hitpoints_), damage_dealer);
    setHitpoints((int)hitpoints_ - damage);

    // Disable collision damage at the end of this frame (there may be
    // collisions pending this frame), re-enable it after a given time
    s_scheduler.addEvent(SingleEventCallback(this, &HitpointTracker::enableCollisionDamage),
                         0.0f,
                         (void*)false,
                         "HitpointTracker::enableCollisionDamage",
                         &fp_group_tracker_);
    s_scheduler.addEvent(SingleEventCallback(this, &HitpointTracker::enableCollisionDamage),
                         COLLISION_DAMAGE_DISABLE_TIME,
                         (void*)true,
                         "HitpointTracker::enableCollisionDamage",
                         &fp_group_tracker_);
}


//------------------------------------------------------------------------------
/**
 *  \param killer The player that did the kill is ignored for finding
 *  the top assistant.
 *
 *  \param percentage [out] The percentage of the killer's damage the
 *  top assistant did.
 *
 *  \return The id of the top assistant.
 */
SystemAddress HitpointTracker::getTopAssistant(const SystemAddress & killer, float & percentage) const
{
    unsigned killer_damage = 0;
    
    unsigned max_damage = 0;
    SystemAddress top_assistant = UNASSIGNED_SYSTEM_ADDRESS;
    
    for (unsigned i=0; i<damage_dealt_.size(); ++i)
    {
        if (damage_dealt_[i].first == killer)
        {
            killer_damage = damage_dealt_[i].second;
        } else
        {
            if (damage_dealt_[i].second > max_damage)
            {
                max_damage    = damage_dealt_[i].second;
                top_assistant = damage_dealt_[i].first;
            }
        }
    }

    if (killer_damage) percentage = (float)max_damage / killer_damage;
    else percentage = 0.0f;

    s_log << Log::debug('l')
          << "top assistant "
          << top_assistant
          << " has done "
          << percentage*100.0f
          << "% of the killer's ("
          << killer
          << ") damage.\n";
    
    return top_assistant;    
}


//------------------------------------------------------------------------------
void HitpointTracker::setHealParameters(float start_val, float delta)
{
    heal_increment_start_val_ = start_val;
    heal_increment_delta_     = delta;
}


//------------------------------------------------------------------------------
void HitpointTracker::setCollisionDamageParameters(float speed_threshold,
                                                   float min_damage,
                                                   float max_damage)
{
    cd_speed_threshold_ = speed_threshold;
    cd_min_damage_      = min_damage;
    cd_max_damage_      = max_damage;
}


//------------------------------------------------------------------------------
void HitpointTracker::setArmorParams(float front_factor, float rear_factor)
{
    armor_front_factor_ = front_factor;
    armor_rear_factor_  = rear_factor;
}

//------------------------------------------------------------------------------
void HitpointTracker::startHealing()
{
    if (task_healing_ != INVALID_TASK_HANDLE)   return;
    if (hitpoints_ == 0)                        return;
    if (hitpoints_ == (unsigned)max_hitpoints_) return;

    task_healing_ = s_scheduler.addTask(PeriodicTaskCallback(this, &HitpointTracker::heal),
                                        HEAL_DT,
                                        "HitpointTracker::heal",
                                        &fp_group_tracker_);

    heal_increment_ = heal_increment_start_val_;
}


//------------------------------------------------------------------------------
void HitpointTracker::stopHealing()
{
    if (task_healing_ == INVALID_TASK_HANDLE) return;

    if (isGainingHealth()) observable_.emit(HTE_STATE_CHANGED);

    heal_increment_ = heal_increment_start_val_;
    
    s_scheduler.removeTask(task_healing_, &fp_group_tracker_);
    task_healing_ = INVALID_TASK_HANDLE;

}


//------------------------------------------------------------------------------
bool HitpointTracker::isGainingHealth() const
{
    return (int)heal_increment_ > 0;
}

//------------------------------------------------------------------------------
Observable * HitpointTracker::getObservable()
{
    return &observable_;
}

//------------------------------------------------------------------------------
HIT_LOCATION HitpointTracker::calcHitLocation(const Matrix & hit_object, const Vector & source)
{
    Vector v = source - hit_object.getTranslation();
    v.normalize();
    float cos_a = vecDot(&v, &hit_object.getZ());

    if (-cos_a > s_params.get<float>("server.logic.armor_cone_cos")) return HL_FRONT;
    if ( cos_a > s_params.get<float>("server.logic.armor_cone_cos")) return HL_REAR;
    return HL_DEFAULT;
}


//------------------------------------------------------------------------------
/**
 *  To avoid excessive and seemingly random collision damage, disable
 *  it for a short time after first impact. Here it is re-enabled
 *  again.
 */
void HitpointTracker::enableCollisionDamage(void*b)
{
    collision_damage_enabled_ = (bool)b;
}


//------------------------------------------------------------------------------
void HitpointTracker::heal(float dt)
{
    bool prev_gaining_health = isGainingHealth();
    
    heal_increment_ +=  dt * heal_increment_delta_;
    
    if (isGainingHealth())
    {
        if (!prev_gaining_health) observable_.emit(HTE_STATE_CHANGED);

        setHitpoints(hitpoints_ + (int)heal_increment_, false);
    }

    if (hitpoints_ == (unsigned)max_hitpoints_)
    {
        stopHealing();
    }
}



//------------------------------------------------------------------------------
void HitpointTracker::addAssistDamage(unsigned amount, const SystemAddress & damage_dealer)
{
    unsigned i;
    for (i=0; i<damage_dealt_.size(); ++i)
    {
        if (damage_dealt_[i].first == damage_dealer) break;
    }

    if (i==damage_dealt_.size())
    {
        damage_dealt_.push_back(std::make_pair(damage_dealer, amount));
    } else
    {
        damage_dealt_[i].second += amount;
    }
}


