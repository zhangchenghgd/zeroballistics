
#ifndef TANK_MACHINE_GUN_INCLUDED
#define TANK_MACHINE_GUN_INCLUDED


#include "TankMachineGun.h"

#include <limits>


#include "physics/OdeSimulator.h"


#include "Tank.h"
#include "Projectile.h"
#include "PuppetMasterServer.h"
#include "GameState.h"
#include "AutoRegister.h"
#include "ParameterManager.h"
#include "PuppetMasterClient.h"
#include "GameLogicServerCommon.h"
#include "TankMine.h"
#include "Water.h"

#ifndef DEDICATED_SERVER

#include <osg/Node>
#include <osg/MatrixTransform>

#include "SceneManager.h"
#include "SoundManager.h"
#include "SoundSourceCallbacks.h"
#include "GameLogicClientCommon.h"
#include "OsgNodeWrapper.h"

#include "TankVisual.h"

#include "ReaderWriterBbm.h"
#include "UtilsOsg.h"


const float SND_FIRING_GAIN       = 0.5f;
const float TRACER_DIST_UPDATE_DT = 0.2f;

#endif

#undef min
#undef max

REGISTER_CLASS(WeaponSystem, TankMachineGunServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, TankMachineGunClient);
#endif


//------------------------------------------------------------------------------
TankMachineGun::~TankMachineGun()
{
}


//------------------------------------------------------------------------------
TankMachineGun::TankMachineGun() :
    ray_intersection_dist_(0.0f),
    hit_normal_(Vector(0.0f, 0.0f, 0.0f)),
    hit_body_(NULL)
{
}





//------------------------------------------------------------------------------
void TankMachineGun::doRayTest(physics::OdeSimulator * sim)
{
    Matrix muzzle_trans;
    tank_->getMuzzleTransform(&muzzle_trans);

    ray_intersection_dist_ = s_params.get<float>(section_ + ".range");
    physics::OdeRayGeom ray(ray_intersection_dist_);
    ray.set(muzzle_trans.getTranslation(), -muzzle_trans.getZ());

    hit_body_ = NULL;
    sim->getStaticSpace()->collide(&ray, physics::CollisionCallback(this, &TankMachineGun::rayCollisionCallback));
    sim->getActorSpace() ->collide(&ray, physics::CollisionCallback(this, &TankMachineGun::rayCollisionCallback));
}



//------------------------------------------------------------------------------
bool TankMachineGun::rayCollisionCallback(const physics::CollisionInfo & info)
{
    // Ignore water plane on server so we can still shoot e.g. mines
    // below water
    if (tank_->getLocation() == CL_SERVER_SIDE &&
        info.other_geom_->getName() == WATER_GEOM_NAME) return false;
        
    RigidBody * cur_hit = (RigidBody*) info.other_geom_->getUserData();

    
    // Find closest hit that is not our own tank
    if (cur_hit != tank_ &&
        (hit_body_ == NULL || info.penetration_ < ray_intersection_dist_))
    {
        ray_intersection_dist_ = info.penetration_;
        hit_normal_            = info.n_;
        hit_body_              = cur_hit;
        if(cur_hit)
        {
            hit_rigid_body_type_   = cur_hit->getType();
            hit_player_            = cur_hit->getType() == "Tank" ? cur_hit->getOwner() : UNASSIGNED_SYSTEM_ADDRESS;
        }
        else
        {
            hit_rigid_body_type_   = "";
            hit_player_            = UNASSIGNED_SYSTEM_ADDRESS;
        }
    }
    
    return false;
}


//------------------------------------------------------------------------------
/**
 *  Cast a ray, see what's hit, deal damage (in callback fun), decrease ammo.
 */
void TankMachineGunServer::doFire()
{
    assert(game_logic_server_);
    
    // Because this is a scheduled function, the tank may have been
    // destroyed in the meantime...
    if (tank_->getOwner() == UNASSIGNED_SYSTEM_ADDRESS) return;

    doRayTest(game_logic_server_->getPuppetMaster()->getGameState()->getSimulator());

    if (hit_body_)
    {
        game_logic_server_->onMachinegunHit(this, hit_body_);
    }
}


#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
TankMachineGunClient::TankMachineGunClient() :
    task_tracer_dist_(INVALID_TASK_HANDLE),
    task_hitfeedback_(INVALID_TASK_HANDLE)
{
}



//------------------------------------------------------------------------------
bool TankMachineGunClient::startFiring()
{
    if (!WeaponSystem::startFiring()) return false;
    
    s_log << Log::debug('l')
          << "startFiringClient\n";
    
    assert(snd_firing_.get() == NULL);

    snd_firing_ = s_soundmanager.playLoopingEffect(s_params.get<std::string>("sfx.mg_fire"),
                                                   tank_);
    snd_firing_->addUpdateCallback(new SoundSourcePositionAndVelocityUpdater(tank_, false));
    snd_firing_->setGain(SND_FIRING_GAIN);





    // Schedule next tracer round
    task_tracer_dist_ = s_scheduler.addTask(PeriodicTaskCallback(this, &TankMachineGunClient::handleTracerDistance),
                                            TRACER_DIST_UPDATE_DT,
                                            "TankMachineGun::handleTracerDistance",
                                            &fp_group_);
    
    // Single-event based in order to allow for random fluctuations.
    handleHitFeedback(NULL);


    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if(tank_visual) tank_visual->enableMgEffect(true);

    return true;
}


//------------------------------------------------------------------------------
bool TankMachineGunClient::stopFiring()
{
    if (!WeaponSystem::stopFiring())
    {
        assert(task_hitfeedback_ == INVALID_TASK_HANDLE);
        assert(!snd_firing_.get());
        return false;
    }
    
    assert(snd_firing_.get());
    // Will be deleted after last sample has finished playing
    snd_firing_->setLooping(false);    
    snd_firing_ = NULL;

    s_scheduler.removeTask(task_tracer_dist_, &fp_group_);
    task_tracer_dist_ = INVALID_TASK_HANDLE;

    s_scheduler.removeTask(task_hitfeedback_, &fp_group_);
    task_hitfeedback_ = INVALID_TASK_HANDLE;

    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if(tank_visual) tank_visual->enableMgEffect(false);

    return true;
}

//------------------------------------------------------------------------------
void TankMachineGunClient::onOverheat()
{
    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if (tank_visual)
    {
        tank_visual->playTankSoundEffect(s_params.get<std::string>("sfx.mg_overheating"),
                                         tank_->getPosition());
    }
}



//------------------------------------------------------------------------------
/**
 *  First, cast a ray into the scene to see how far tracer must
 *  fly. Then create tracer round with the appropriate lifetime, and
 *  schedule the next tracer event.
 */
void TankMachineGunClient::handleTracerDistance(float dt)
{
    doRayTest(game_logic_client_->getPuppetMaster()->getGameState()->getSimulator());

    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    assert(tank_visual);
    tank_visual->setMgFireDistance(ray_intersection_dist_);
}


//------------------------------------------------------------------------------
/**
 *  Uses the current ray_intersection_dist_ and hit_normal_ to create
 *  a particle & sound effect.
 *
 *  Reschedules itself.
 */
void TankMachineGunClient::handleHitFeedback(void *)
{
    assert(game_logic_client_);
    assert(tank_);

    doRayTest(game_logic_client_->getPuppetMaster()->getGameState()->getSimulator());
    
    // Only create feedback if something was hit actually
    if (ray_intersection_dist_ != s_params.get<float>(section_ + ".range"))
    {
        Matrix muzzle_trans;
        tank_->getMuzzleTransform(&muzzle_trans);

        Vector hit_pos = muzzle_trans.getTranslation() - ray_intersection_dist_*muzzle_trans.getZ();
        
        uint8_t type;
        if(hit_rigid_body_type_ == "Tank")
        {
            type = WHT_MACHINE_GUN_HIT_TANK;
        }
        else if(hit_rigid_body_type_ == "Water")
        {
            type = WHT_MACHINE_GUN_HIT_WATER;
        } else
        {
            type = WHT_MACHINE_GUN_HIT_OTHER;
        }

        RakNet::BitStream args;
        args.Write(tank_->getOwner());
        args.Write(hit_player_);
        args.Write(hit_pos);
        args.Write(hit_normal_);
        args.Write(type);

        game_logic_client_->executeCustomCommand(CSCT_WEAPON_HIT, args);
    }

    // Schedule next hit feedback
    task_hitfeedback_ = s_scheduler.addEvent(SingleEventCallback(this, &TankMachineGunClient::handleHitFeedback),
                                             s_params.get<float>(section_ + ".feedback_interval"),
                                             NULL,
                                             "Hit Feedback",
                                             &fp_group_);
}

#endif // #ifndef DEDICATED_SERVER

#endif // #ifndef TANK_MACHINE_GUN_INCLUDED
