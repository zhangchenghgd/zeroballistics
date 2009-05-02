
#include "Shield.h"

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
#include "SceneManager.h"
#include "OsgNodeWrapper.h"
#include "TankVisual.h"
#endif


#undef min
#undef max

REGISTER_CLASS(WeaponSystem, ShieldServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, ShieldClient);
#endif



//------------------------------------------------------------------------------
void ShieldServer::doFire()
{
    WeaponSystem::doFire();    

    tank_->setShield((void*)true);

    // schedule the end of shield effect
    s_scheduler.addEvent(SingleEventCallback(tank_, &Tank::setShield),
                         s_params.get<float>(section_ + ".duration"),
                         (void*)false,
                         "Tank::setShield(false)",
                         &tank_->getFpGroup());
}


#ifndef DEDICATED_SERVER


//------------------------------------------------------------------------------
void ShieldClient::doFire()
{
    assert(game_logic_client_);

    WeaponSystem::doFire();    


    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if (tank_visual)
    {
        tank_visual->playTankSoundEffect(s_params.get<std::string>("sfx.shield_effect"),
                                         tank_->getPosition());
    }


}


#endif
