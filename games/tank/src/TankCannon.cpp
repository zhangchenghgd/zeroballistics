
#include "TankCannon.h"

#include <limits>

#include "Tank.h"
#include "PuppetMasterServer.h"
#include "GameState.h"
#include "AutoRegister.h"
#include "ParameterManager.h"
#include "Projectile.h"
#include "GameLogicServerCommon.h"

#ifndef DEDICATED_SERVER
#include "GameLogicClientCommon.h"
#include "SoundManager.h"
#include "TankVisual.h"
#endif


#undef min
#undef max

REGISTER_CLASS(WeaponSystem, TankCannonServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, TankCannonClient);
#endif


const float MAX_LATENCY_CORRECTION_DT = 0.5f;


//------------------------------------------------------------------------------
void TankCannonServer::doFire()
{
    WeaponSystem::doFire();    
                
    // Spawn new projectile
    Projectile * projectile =
        (Projectile*)game_logic_server_->createRigidBody(
            s_params.get<std::string>(section_ + ".projectile_desc_file"),
            "BallisticProjectile");
    assert(projectile);

    projectile->setSection(section_);
    projectile->setAdditionalDamage(tank_->getParams().get<unsigned>("tank.additional_projectile_damage"));
    projectile->setOwner(tank_->getOwner());            

    // set collision callback in GameLogicServer
    projectile->getTarget()->setCollisionCallback(
        physics::CollisionCallback(projectile, &Projectile::collisionCallback) );



    Matrix muzzle_transform;
    tank_->getMuzzleTransform(&muzzle_transform);

    tank_->applyRecoil(muzzle_transform.getZ() * s_params.get<float>(section_ + ".recoil"),
                       muzzle_transform.getTranslation());

    Vector tank_vel = tank_->getGlobalLinearVel();


    // XXXX disabled for soccer, can cause tank to fire through ball.
//     // Correction for network delay.
//     ServerPlayer * p = game_logic_server_->getPuppetMaster()->getPlayer(tank_->getOwner());
//     if (p) muzzle_transform.getTranslation() += tank_vel * std::min(p->getTotalInputDelay(), MAX_LATENCY_CORRECTION_DT);

    projectile->setTransform(muzzle_transform);                
    projectile->setGlobalLinearVel(-muzzle_transform.getZ() * s_params.get<float>(section_ + ".muzzle_velocity") +
                                   tank_vel);

    game_logic_server_->getPuppetMaster()->addGameObject(projectile);

    // Must be set after addGameObject, else gets overwritten
    projectile->setCollisionCategory(CCCS_PROJECTILE, false);
}


#ifndef DEDICATED_SERVER


//------------------------------------------------------------------------------
void TankCannonClient::doFire()
{
    assert(game_logic_client_);

    WeaponSystem::doFire();
    
    Matrix muzzle_transform;
    tank_->getMuzzleTransform(&muzzle_transform);

    tank_->applyRecoil(muzzle_transform.getZ() * s_params.get<float>(section_ + ".recoil"),
                       muzzle_transform.getTranslation());
    
    // muzzle flash
    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if(tank_visual) tank_visual->fireEffect(s_params.get<std::string>(section_ + ".fire_sound"));

    game_logic_client_->revealOnMinimap(tank_);
}


#endif
