
#include "Boost.h"

#include <limits>

#include "Tank.h"
#include "PuppetMasterServer.h"
#include "GameState.h"
#include "AutoRegister.h"
#include "ParameterManager.h"
#include "GameLogicServerCommon.h"

#ifndef DEDICATED_SERVER
#include "GameLogicClientCommon.h"
#include "SoundManager.h"
#include "SceneManager.h"
#include "OsgNodeWrapper.h"
#include "TankVisual.h"
#endif


#undef min
#undef max

REGISTER_CLASS(WeaponSystem, BoostServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, BoostClient);
#endif



//------------------------------------------------------------------------------
bool BoostServer::startFiring()
{
    bool ret = WeaponSystem::startFiring();

    if(ret) tank_->activateBoost(true); 

    return ret;
}

//------------------------------------------------------------------------------
bool BoostServer::stopFiring()
{
    bool ret = WeaponSystem::stopFiring();

    if(ret) tank_->activateBoost(false); 

    return ret;
}

//------------------------------------------------------------------------------
void BoostServer::onOverheat()
{
    tank_->activateBoost(false); 
}

#ifndef DEDICATED_SERVER


//------------------------------------------------------------------------------
bool BoostClient::startFiring()
{
    bool ret = WeaponSystem::startFiring();

    if (ret)
    {
        tank_->activateBoost(true);

        TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
        if(tank_visual) tank_visual->enableBoostEffect(true);
    }

    return ret;
}

//------------------------------------------------------------------------------
bool BoostClient::stopFiring()
{
    bool ret = WeaponSystem::stopFiring();

    if (ret)
    {
        tank_->activateBoost(false);
        
        TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
        if(tank_visual) tank_visual->enableBoostEffect(false);
    }

    return ret;
}

//------------------------------------------------------------------------------
void BoostClient::onOverheat()
{
    tank_->activateBoost(false); 
}

#endif
