
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

#ifndef DEDICATED_SERVER

#include <osg/Node>
#include <osg/MatrixTransform>

#include "SceneManager.h"
#include "SoundManager.h"
#include "SoundSource.h"
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

REGISTER_CLASS(WeaponSystem, TankMachineGun);



const std::string DEAL_DAMAGE_TASK_NAME = "TankMachineGun::dealDamage";



//------------------------------------------------------------------------------
TankMachineGun::~TankMachineGun()
{
}

//------------------------------------------------------------------------------
bool TankMachineGun::fire(bool fire)
{
    if(tank_->getLocation() == CL_SERVER_SIDE)
    {
        if (fire && ammo_ > 0) 
        {
            if (fp_group_.isEmpty())
            {
                tank_->setNetworkFlagDirty();

                s_scheduler.addTask(PeriodicTaskCallback(this, &TankMachineGun::dealDamage),
                                    s_params.get<float>(section_ + ".reload_time"),
                                    DEAL_DAMAGE_TASK_NAME,
                                    &fp_group_);
            }

            return true;
        } else if (!fp_group_.isEmpty())
        {
            tank_->setNetworkFlagDirty();
    
            fp_group_.deregisterAllOfType(TaskFp());

            return false;
        }
    }
    else if (tank_->getLocation() == CL_CLIENT_SIDE)
    {
#ifndef DEDICATED_SERVER
        if (fire && ammo_ > 0)
        {
            if (fp_group_.isEmpty()) startFiringClient();

            return true;
        } else
        {
            stopFiringClient();

            return false;
        }
#endif        
    }

    return false;
}


//------------------------------------------------------------------------------
TankMachineGun::TankMachineGun()
    : ray_intersection_dist_(0.0f),
      hit_normal_(Vector(0.0f, 0.0f, 0.0f)),
      hit_body_(NULL)
#ifndef DEDICATED_SERVER
      ,task_tracer_dist_(INVALID_TASK_HANDLE)
#endif
{
}



//------------------------------------------------------------------------------
/**
 *  Server side. Cast a ray, see what's hit, deal damage (in callback fun), decrease ammo.
 */
void TankMachineGun::dealDamage(float dt)
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

    setAmmo(ammo_-1);
    if (ammo_==0) fp_group_.deregisterAllOfType(TaskFp());
}

//------------------------------------------------------------------------------
/**
 *  Used on both client and server.
 */
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



#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
void TankMachineGun::startFiringClient()
{
    s_log << Log::debug('l')
          << "startFiringClient\n";
    
    assert(snd_firing_.get() == NULL);
    assert(task_tracer_dist_ == INVALID_TASK_HANDLE);

    snd_firing_ = s_soundmanager.playLoopingEffect(s_params.get<std::string>("sfx.mg_fire"),
                                                   tank_);
    snd_firing_->addUpdateCallback(new SoundSourcePositionAndVelocityUpdater(tank_, false));
    snd_firing_->setGain(SND_FIRING_GAIN);





    // Schedule next tracer round
    task_tracer_dist_ = s_scheduler.addTask(PeriodicTaskCallback(this, &TankMachineGun::handleTracerDistance),
                                            TRACER_DIST_UPDATE_DT,
                                            "TankMachineGun::handleTracerDistance",
                                            &fp_group_);
    
    // Single-event based in order to allow for random fluctuations.
    handleHitFeedback(NULL);


    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if(tank_visual) tank_visual->enableMgEffect(true);
}


//------------------------------------------------------------------------------
void TankMachineGun::stopFiringClient()
{
    if (fp_group_.isEmpty())
    {
        assert(!snd_firing_.get());
        return;
    }
    
    assert(snd_firing_.get());
    // Will be deleted after last sample has finished playing
    snd_firing_->setLooping(false);    
    snd_firing_ = NULL;

    fp_group_.deregisterAllOfType(TaskFp());
    task_tracer_dist_ = INVALID_TASK_HANDLE;

    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    if(tank_visual) tank_visual->enableMgEffect(false);    
}


//------------------------------------------------------------------------------
/**
 *  First, cast a ray into the scene to see how far tracer must
 *  fly. Then create tracer round with the appropriate lifetime, and
 *  schedule the next tracer event.
 */
void TankMachineGun::handleTracerDistance(float dt)
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
void TankMachineGun::handleHitFeedback(void *)
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
    s_scheduler.addEvent(SingleEventCallback(this, &TankMachineGun::handleHitFeedback),
                         s_params.get<float>(section_ + ".feedback_interval"),
                         NULL,
                         "Hit Feedback",
                         &fp_group_);
}

#endif
