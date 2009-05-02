
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

REGISTER_CLASS(WeaponSystem, WeaponSystem);

//------------------------------------------------------------------------------
WeaponSystem::~WeaponSystem()
{
}

//------------------------------------------------------------------------------
void WeaponSystem::reset(Tank * tank, const std::string & section)
{
    tank_    = tank;
    section_ = section;
    
    reload_time_ = 0.0f;
    ammo_  = s_params.get<unsigned>(section_ + ".max_ammo");

    if (ammo_==0) ammo_=1;
}


//------------------------------------------------------------------------------
Tank * WeaponSystem::getTank() const
{
    return tank_;
}

//------------------------------------------------------------------------------
void WeaponSystem::frameMove(float dt)
{
    if (reload_time_ != 0.0f && ammo_ != 0)
    {
        reload_time_ -= dt;
        if (reload_time_ < 0.0f) reload_time_ = 0.0f;
    }
}

//------------------------------------------------------------------------------
void WeaponSystem::writeStateToBitstream (RakNet::BitStream & stream) const
{
    stream.Write(ammo_);
}


//------------------------------------------------------------------------------
void WeaponSystem::readStateFromBitstream(RakNet::BitStream & stream)
{
    stream.Read(ammo_);
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
float WeaponSystem::getReloadPercentage() const
{
    float max_reload_time = s_params.get<float>(section_ + ".reload_time");
    if (max_reload_time == 0.0f) return 1.0f;
    else return 1.0f - reload_time_ / max_reload_time;
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
    reload_time_(0.0f),
    ammo_(0)
{
}
