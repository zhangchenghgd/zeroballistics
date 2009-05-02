
#include "WeaponSystem.h"


#include <raknet/BitStream.h>

#include "Tank.h"
#include "PlayerInput.h"
#include "ParameterManager.h"
#include "AutoRegister.h"

#undef min
#undef max


const std::string WEAPONS_PARAM_FILE = "data/config/weapon_systems.xml";

GameLogicServerCommon * WeaponSystem::game_logic_server_ = NULL;
GameLogicClientCommon * WeaponSystem::game_logic_client_ = NULL;

REGISTER_CLASS(WeaponSystem, WeaponSystemServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, WeaponSystemClient);
#endif

//------------------------------------------------------------------------------
WeaponSystem::~WeaponSystem()
{
}


//------------------------------------------------------------------------------
Tank * WeaponSystem::getTank() const
{
    return tank_;
}


//------------------------------------------------------------------------------
bool WeaponSystem::handleInput(bool fire)
{
    if (fire && canFire())
    {
        if (allowFiringAfterOverheat() || !prev_firing_)
        {
            startFiring();
        }
    } else 
    {
        stopFiring();
    }

    prev_firing_ = fire;
    
    return isFiring();
}



//------------------------------------------------------------------------------
void WeaponSystem::init(Tank * tank, const std::string & section)
{
    tank_    = tank;
    section_ = section;
    
    ammo_  = std::max(1u, s_params.get<unsigned>(section_ + ".max_ammo"));
}



//------------------------------------------------------------------------------
void WeaponSystem::frameMove(float dt)
{
    if (isFiring())
    {
        heating_ += dt * s_params.get<float>(section_ + ".heating_per_sec");
        checkForOverheating();
    } else
    {
        if (!isOverheated())
        {
            heating_ -= dt * s_params.get<float>(section_ + ".cooldown_per_sec");
        }
    }
}


//------------------------------------------------------------------------------
float WeaponSystem::getCooldownStatus() const
{
    return heating_;
}



//------------------------------------------------------------------------------
void WeaponSystem::writeStateToBitstream (RakNet::BitStream & stream) const
{
    stream.Write(ammo_);

    stream.Write(isFiring());

    stream.Write(send_fire_);
    send_fire_ = false;

    stream.Write(send_overheated_);
    send_overheated_ = false;    
}


//------------------------------------------------------------------------------
void WeaponSystem::readStateFromBitstream(RakNet::BitStream & stream)
{
    stream.Read(ammo_);

    bool new_is_firing;
    bool overheated;
    bool single_shot;

    stream.Read(new_is_firing);
    stream.Read(single_shot);
    stream.Read(overheated);

    // determine based on input and other state variables whether to
    // fire for CSP
    if (tank_->isLocallyControlled() && tank_->getLocation() == CL_CLIENT_SIDE) return;
    
    if (new_is_firing)
    {
        startFiring();
    } else
    {
        stopFiring();

        if (single_shot)
        {
            fire(0.0f);
            blockFiring();
        }
    }
    
    if (overheated && !isOverheated())
    {
        heating_ = 1.0f;
        checkForOverheating();
    }    
}


//------------------------------------------------------------------------------
void WeaponSystem::setAmmo(unsigned ammo)
{
    unsigned max_ammo = s_params.get<unsigned>(section_ + ".max_ammo");
    if (max_ammo == 0) return;
    
    unsigned new_ammo = std::min(max_ammo, ammo);

    if (new_ammo == ammo_) return;

    tank_->setNetworkFlagDirty();
    
    ammo_ = new_ammo;    
}


//------------------------------------------------------------------------------
unsigned WeaponSystem::getAmmo() const
{
    return ammo_;
}


//------------------------------------------------------------------------------
float WeaponSystem::getDamage() const
{
    return s_params.get<unsigned>(section_ + ".damage");
}



//------------------------------------------------------------------------------
void WeaponSystem::setGameLogicServer(GameLogicServerCommon * logic)
{
    game_logic_server_ = logic;
}

//------------------------------------------------------------------------------
void WeaponSystem::setGameLogicClient(GameLogicClientCommon * logic)
{
    game_logic_client_ = logic;
}

//------------------------------------------------------------------------------
WeaponSystem::WeaponSystem() :
    tank_(NULL),

    ammo_(0),
    heating_(0.0f),
    prev_firing_(false),
    
    send_fire_(false),
    send_overheated_(false),
    
    task_fire_                (INVALID_TASK_HANDLE),
    task_reset_overheated_    (INVALID_TASK_HANDLE),
    task_reset_firing_blocked_(INVALID_TASK_HANDLE)
{
}



//------------------------------------------------------------------------------
bool WeaponSystem::startFiring()
{
    if (isOverheated()) return false;
    if (isFiring()) return false;

    tank_->setNetworkFlagDirty();
    task_fire_ = s_scheduler.addTask(PeriodicTaskCallback(this, &WeaponSystem::fire),
                                     s_params.get<float>(section_ + ".fire_dt"),
                                     "WeaponSystem::fire",
                                     &fp_group_);

    return true;
}


//------------------------------------------------------------------------------
bool WeaponSystem::stopFiring()
{
    if (!isFiring()) return false;

    tank_->setNetworkFlagDirty();
    s_scheduler.removeTask(task_fire_, &fp_group_);
    task_fire_ = INVALID_TASK_HANDLE;

    blockFiring();
    
    return true;
}


//------------------------------------------------------------------------------
void WeaponSystem::fire(float dt)
{
    if (isBlocked()) return;
    
    doFire();    

    heating_ += s_params.get<float>(section_ + ".heating_per_shot");
    checkForOverheating();

    setAmmo(ammo_-1);

    send_fire_ = true;

    if (!canFire()) stopFiring();
}


//------------------------------------------------------------------------------
/**
 *  The heating of the shot must not exceed 1.0 in total heat, or shot
 *  cannot be fired.
 */
bool WeaponSystem::canFire() const
{
    if (isOverheated()) return false;
    if (ammo_ == 0)     return false;
    if (isBlocked())    return false;
    
    return heating_  + s_params.get<float>(section_ + ".heating_per_shot") <= 1.0f;
}


//------------------------------------------------------------------------------
bool WeaponSystem::isFiring() const
{
    assert(task_fire_ == INVALID_TASK_HANDLE || task_reset_overheated_ == INVALID_TASK_HANDLE);
    
    return task_fire_ != INVALID_TASK_HANDLE;
}

//------------------------------------------------------------------------------
bool WeaponSystem::isOverheated() const
{
    return task_reset_overheated_ != INVALID_TASK_HANDLE;
}

//------------------------------------------------------------------------------
bool WeaponSystem::isBlocked() const
{
    return task_reset_firing_blocked_ != INVALID_TASK_HANDLE;
}



//------------------------------------------------------------------------------
void WeaponSystem::checkForOverheating()
{
    heating_ = clamp(heating_, 0.0f, 1.0f);
    
    if (isOverheated()) return;
    
    if (heating_ != 1.0f) return;

    stopFiring();
    
    onOverheat();
    tank_->setNetworkFlagDirty();
    send_overheated_ = true;
    task_reset_overheated_ = s_scheduler.addEvent(SingleEventCallback(this, &WeaponSystem::resetOverheated),
                                                  s_params.get<float>(section_ + ".cooldown_time"),
                                                  NULL,
                                                  "WeaponSystem::resetOverheated",
                                                  &fp_group_);    
}


//------------------------------------------------------------------------------
void WeaponSystem::resetOverheated(void *)
{
    assert(isOverheated());
    task_reset_overheated_ = INVALID_TASK_HANDLE;
}


//------------------------------------------------------------------------------
void WeaponSystem::blockFiring()
{
    if (isBlocked())
    {
        s_scheduler.reschedule(task_reset_overheated_, s_params.get<float>(section_ + ".fire_dt"));
    } else
    {
        task_reset_firing_blocked_ = s_scheduler.addEvent(SingleEventCallback(this, &WeaponSystem::resetBlocked),
                                                          s_params.get<float>(section_ + ".fire_dt"),
                                                          NULL,
                                                          "WeaponSystem::resetBlocked",
                                                          &fp_group_);
    }
}

//------------------------------------------------------------------------------
void WeaponSystem::resetBlocked(void *)
{
    task_reset_firing_blocked_ = INVALID_TASK_HANDLE;
}
