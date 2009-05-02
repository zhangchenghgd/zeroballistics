
#include "TankCannon.h"

#include <limits>

#include "Tank.h"
#include "Projectile.h"
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

REGISTER_CLASS(WeaponSystem, TankCannon);

//------------------------------------------------------------------------------
TankCannon::TankCannon()
{
}

//------------------------------------------------------------------------------
TankCannon::~TankCannon()
{
}


//------------------------------------------------------------------------------
void TankCannon::frameMove(float dt)
{
    WeaponSystem::frameMove(dt);

    // see Tank::main_reload_time_
    if (ammo_ == 0) tank_->setMainGunReloadTime(s_params.get<float>(section_ + ".reload_time"));
    reload_time_ = tank_->getMainGunReloadTime();
}

//------------------------------------------------------------------------------
bool TankCannon::fire(bool fire)
{
    if (!fire ||
        tank_->getMainGunReloadTime() != 0.0f ||
        ammo_ == 0) return false;


    // This is done for server, client and replay simulator!
    Matrix muzzle_transform;
    tank_->getMuzzleTransform(&muzzle_transform);
    Vector muzzle_dir = -muzzle_transform.getZ();

    tank_->applyRecoil(-muzzle_dir * s_params.get<float>(section_ + ".recoil"),
                       muzzle_transform.getTranslation());
    
    tank_->setMainGunReloadTime(s_params.get<float>(section_ + ".reload_time"));
    
    if(tank_->getLocation() == CL_SERVER_SIDE)
    {
        setAmmo(ammo_-1);
                
        // Spawn new projectile
        Projectile * projectile =
            (Projectile*)game_logic_server_->createRigidBody(
                s_params.get<std::string>(section_ + ".projectile_desc_file"),
                "Projectile");
        assert(projectile);

        projectile->setSection(section_);
        projectile->setAdditionalDamage(tank_->getParams().get<unsigned>("tank.additional_projectile_damage"));
        projectile->setOwner(tank_->getOwner());            

        // set collision callback in GameLogicServer
        projectile->getTarget()->setCollisionCallback(
            physics::CollisionCallback(projectile, &Projectile::collisionCallback) );


        Vector tank_vel = tank_->getGlobalLinearVel();

        // Correction for network delay.
        ServerPlayer * p = game_logic_server_->getPuppetMaster()->getPlayer(tank_->getOwner());
        if (p) muzzle_transform.getTranslation() += tank_vel * p->getTotalInputDelay();

        projectile->setTransform(muzzle_transform);                
        projectile->setGlobalLinearVel(muzzle_dir * s_params.get<float>(section_ + ".muzzle_velocity") + tank_vel);

        game_logic_server_->getPuppetMaster()->addGameObject(projectile);

        // Must be set after addGameObject, else gets overwritten
        projectile->setCollisionCategory(CCCS_PROJECTILE, false);
            
    } else if (tank_->getLocation() == CL_CLIENT_SIDE)
    {
#ifndef DEDICATED_SERVER
        assert(game_logic_client_);

        // muzzle flash
        TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
        if(tank_visual) tank_visual->fireEffect(s_params.get<std::string>(section_ + ".fire_sound"));

        game_logic_client_->revealOnMinimap(tank_);
#endif        
    } else return false;

    return true;
}
