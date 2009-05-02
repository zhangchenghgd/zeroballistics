
#include "TankMineDispenser.h"

#include <limits>

#include "Tank.h"
#include "TankMine.h"
#include "PuppetMasterServer.h"
#include "GameState.h"
#include "AutoRegister.h"
#include "ParameterManager.h"

#include "GameLogicServerCommon.h" ///< to set collision callback

#ifndef DEDICATED_SERVER
#include "GameLogicClientCommon.h"
#include "SoundManager.h"
#endif

#undef min
#undef max

REGISTER_CLASS(WeaponSystem, TankMineDispenser);


const float MINE_DROP_OFFSET = 0.7f;

//------------------------------------------------------------------------------
TankMineDispenser::TankMineDispenser()
{
}

//------------------------------------------------------------------------------
TankMineDispenser::~TankMineDispenser()
{
}


//------------------------------------------------------------------------------
bool TankMineDispenser::fire(bool fire)
{
    if (!fire ||
        reload_time_ != 0.0f ||
        ammo_ == 0) return false;
            
    if(tank_->getLocation() == CL_SERVER_SIDE)
    {
        reload_time_ = s_params.get<float>(section_ + ".reload_time");
        setAmmo(ammo_-1);
                
        // Spawn new mine
        TankMine * mine = (TankMine*)game_logic_server_->createRigidBody(
            s_params.get<std::string>(section_ + ".mine_desc_file"),
            "TankMine");
        assert(mine);
        mine->setOwner(tank_->getOwner());
        mine->setTeamId(game_logic_server_->score_.getTeamId(tank_->getOwner()));
        mine->setSection(section_);
        mine->setLifetime(s_params.get<float>(section_ + ".lifetime"));

        // set collision callback in GameLogicServer
        mine->getTarget()->setCollisionCallback(
            physics::CollisionCallback(mine, &TankMine::collisionCallback) );

        // Mine Spawn position
        Matrix initial_pos;
        initial_pos = tank_->getTransform();
        initial_pos.getTranslation() += initial_pos.getZ() * MINE_DROP_OFFSET;
        mine->setTransform(initial_pos);
        
        game_logic_server_->getPuppetMaster()->addGameObject(mine);

    } else if (tank_->getLocation() == CL_CLIENT_SIDE)
    {
#ifndef DEDICATED_SERVER
        reload_time_ = s_params.get<float>(section_ + ".reload_time");

        // only play for local player, no 3D attenuation
        if(tank_->getOwner() == game_logic_client_->getPuppetMaster()->getLocalPlayer()->getId())
        {
            s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.lay_mine"),
                                            tank_->getPosition(), 
                                            0.0f);
        }
#endif
    } else return false;

    return true;
}

