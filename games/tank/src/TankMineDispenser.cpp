
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
#include "TankVisual.h"
#endif

#undef min
#undef max

REGISTER_CLASS(WeaponSystem, TankMineDispenserServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, TankMineDispenserClient);
#endif

const float MINE_DROP_OFFSET = 0.7f;





//------------------------------------------------------------------------------
void TankMineDispenserServer::doFire()
{
    WeaponSystem::doFire();
                
    // Spawn new mine
    TankMine * mine = (TankMine*)game_logic_server_->createRigidBody(
        s_params.get<std::string>(section_ + ".mine_desc_file"),
        "TankMine");
    assert(mine);
    mine->setOwner(tank_->getOwner());
    mine->setTeamId(game_logic_server_->getScore().getTeamId(tank_->getOwner()));
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
}


#ifndef DEDICATED_SERVER


//------------------------------------------------------------------------------
void TankMineDispenserClient::doFire()
{
    WeaponSystem::doFire();

    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if (tank_visual)
    {
        tank_visual->playTankSoundEffect(s_params.get<std::string>("sfx.lay_mine"),
                                         tank_->getPosition());
    }
}

#endif

