
#ifndef TANK_HITPOINT_TRACKER_INCLUDED
#define TANK_HITPOINT_TRACKER_INCLUDED

#include <vector>
#include <string>

#include "Datatypes.h"
#include "Scheduler.h"
#include "RegisteredFpGroup.h"

namespace RakNet
{
    class BitStream;
}

struct SystemAddress;


//------------------------------------------------------------------------------
enum HITPOINT_TRACKER_EVENT
{
    HTE_STATE_CHANGED
};


//------------------------------------------------------------------------------
class HitpointTracker
{
 public:
    HitpointTracker();
    HitpointTracker(const HitpointTracker & other);
    virtual ~HitpointTracker();

    void addStateObserver(ObserverCallbackFun0 fun);
    
    void writeStateToBitstream (RakNet::BitStream & stream) const;
    void readStateFromBitstream(RakNet::BitStream & stream);
    
    virtual void setHitpoints(int hp, bool dirty_state = true);
    unsigned     getHitpoints() const;

    void     setMaxHitpoints(unsigned max_hp);
    unsigned getMaxHitpoints() const;

    void setInvincible(void * i);
    bool isInvincible() const;
    
    void dealWeaponDamage   (int amount,            const SystemAddress & damage_dealer);
    void dealCollisionDamage(float collision_speed, float max_speed,
                             const SystemAddress & damage_dealer, float factor = 1.0f);
    SystemAddress getTopAssistant(const SystemAddress & killer, float & percentage) const;

    void setHealParameters(float start_val, float delta);
    void setCollisionDamageParameters(float speed_threshold,
                                      float min_damage,
                                      float max_damage);

    void startHealing();
    void stopHealing();
    bool isGainingHealth() const;
    
 protected:

    void enableCollisionDamage(void*);

    void heal(float dt);

    void addAssistDamage(unsigned amount, const SystemAddress & damage_dealer);

    Observable observable_;

    uint32_t max_hitpoints_;
    uint32_t hitpoints_;

    float heal_increment_;
    hTask task_healing_;

    bool is_invincible_;

    bool collision_damage_enabled_;
    float cd_speed_threshold_;
    float cd_min_damage_;
    float cd_max_damage_;

    float heal_increment_start_val_;
    float heal_increment_delta_;
    
    std::vector<std::pair<SystemAddress, unsigned> > damage_dealt_;
    
    RegisteredFpGroup fp_group_tracker_; ///< Avoid name ambiguities
};

#endif
