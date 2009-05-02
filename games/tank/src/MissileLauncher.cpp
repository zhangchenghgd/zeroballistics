
#include "MissileLauncher.h"

#include <limits>

#include "Tank.h"
#include "PuppetMasterServer.h"
#include "GameState.h"
#include "AutoRegister.h"
#include "ParameterManager.h"
#include "Missile.h"
#include "GameLogicServerCommon.h"

#ifndef DEDICATED_SERVER
#include "GameLogicClientCommon.h"
#include "SoundManager.h"
#include "TankVisual.h"
#endif


#undef min
#undef max

REGISTER_CLASS(WeaponSystem, MissileLauncherServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, MissileLauncherClient);
#endif



//------------------------------------------------------------------------------
void MissileLauncherServer::doFire()
{
    WeaponSystem::doFire();    
                
    // Spawn new missile
    Missile * missile =
        (Missile*)game_logic_server_->createRigidBody(
            s_params.get<std::string>(section_ + ".projectile_desc_file"),
            "Missile");
    assert(missile);

    missile->setSection(section_);
    missile->setOwner(tank_->getOwner());
    missile->setTank(tank_);

    // set collision callback in GameLogicServer
    missile->getTarget()->setCollisionCallback(
        physics::CollisionCallback(missile, &Missile::collisionCallback) );


    Matrix trans;
    tank_->getLauncherTransform(&trans);
    missile->initPosAndVelocity(trans);

    
    tank_->applyRecoil(-trans.getZ() * s_params.get<float>(section_ + ".recoil"),
                       trans.getTranslation());



    game_logic_server_->getPuppetMaster()->addGameObject(missile);

    // Must be set after addGameObject, else gets overwritten
    missile->setCollisionCategory(CCCS_PROJECTILE, false);
}


#ifndef DEDICATED_SERVER


//------------------------------------------------------------------------------
void MissileLauncherClient::doFire()
{
    assert(game_logic_client_);

    WeaponSystem::doFire();    

    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if (tank_visual)
    {
        tank_visual->playTankSoundEffect(s_params.get<std::string>(section_ + ".fire_sound"),
                                         tank_->getPosition());
    }
    
    tank_->applyRecoil(-tank_->getTransform().getY() * s_params.get<float>(section_ + ".recoil"),
                       tank_->getTransform().getTranslation());
    
    game_logic_client_->revealOnMinimap(tank_);
}


#endif
