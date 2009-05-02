
#ifndef TANK_MACHINE_GUN_INCLUDED
#define TANK_MACHINE_GUN_INCLUDED


#include "InstantHitWeapon.h"

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
#include "GameLogicClientCommon.h"
#include "OsgNodeWrapper.h"

#include "TankVisual.h"

#include "ReaderWriterBbm.h"
#include "UtilsOsg.h"
#include "EffectManager.h"


const float SND_FIRING_GAIN       = 0.5f;
const float TRACER_DIST_UPDATE_DT = 0.2f;


const float TRACER_LENGTH = 0.1f; ///< XXXX This is used to correct the distance

#endif

#undef min
#undef max

REGISTER_CLASS(WeaponSystem, InstantHitWeaponServer);
#ifndef DEDICATED_SERVER
REGISTER_CLASS(WeaponSystem, InstantHitWeaponClient);
#endif


//------------------------------------------------------------------------------
InstantHitWeapon::~InstantHitWeapon()
{
}





//------------------------------------------------------------------------------
InstantHitWeapon::InstantHitWeapon() :
    ray_intersection_dist_(0.0f),
    hit_normal_(Vector(0.0f, 0.0f, 0.0f)),
    hit_body_(NULL)
{
}





//------------------------------------------------------------------------------
void InstantHitWeapon::doRayTest(physics::OdeSimulator * sim)
{
    Matrix muzzle_trans;
    tank_->getMuzzleTransform(&muzzle_trans);

    ray_intersection_dist_ = s_params.get<float>(section_ + ".range");
    physics::OdeRayGeom ray(ray_intersection_dist_);
    ray.set(muzzle_trans.getTranslation(), -muzzle_trans.getZ());

    hit_body_ = NULL;
    sim->getStaticSpace()->collide(&ray, physics::CollisionCallback(this, &InstantHitWeapon::rayCollisionCallback));
    sim->getActorSpace() ->collide(&ray, physics::CollisionCallback(this, &InstantHitWeapon::rayCollisionCallback));
}



//------------------------------------------------------------------------------
bool InstantHitWeapon::rayCollisionCallback(const physics::CollisionInfo & info)
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
void InstantHitWeaponServer::doFire()
{
    assert(game_logic_server_);
    
    // Because this is a scheduled function, the tank may have been
    // destroyed in the meantime...
    if (tank_->getOwner() == UNASSIGNED_SYSTEM_ADDRESS) return;

    doRayTest(game_logic_server_->getPuppetMaster()->getGameState()->getSimulator());

    if (hit_body_)
    {
        game_logic_server_->onInstantWeaponHit(this, hit_body_);
    }
}


#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
InstantHitWeaponClient::InstantHitWeaponClient() :
    task_tracer_dist_(INVALID_TASK_HANDLE),
    task_hitfeedback_(INVALID_TASK_HANDLE)
{
}

//------------------------------------------------------------------------------
void InstantHitWeaponClient::init(Tank * tank, const std::string & section)
{
    WeaponSystem::init(tank, section);

    
    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    assert(tank_visual);
    assert(tank_visual->getWrapperNode());
    // TODO check why this triggers with bots...
    // assert(tracer_effect_.empty());
    if(!tracer_effect_.empty()) tracer_effect_.clear();
    
    // retrieve tracer effect modeled in blender
    std::vector<osg::Node*> tracer_v =
        s_scene_manager.findNode(s_params.get<std::string>(section_ + ".tracer_effect"),
                                 tank_visual->getWrapperNode()->getOsgNode());
    
    
    for (unsigned i=0; i<tracer_v.size(); ++i)
    {
        ParticleEffectNode * n = dynamic_cast<ParticleEffectNode*>(tracer_v[i]);
        assert(n);
        tracer_effect_.push_back(n);
    }

    if (tracer_effect_.empty())
    {
        s_log << Log::warning
              << "No tracer effect found for "
              << section
              << "\n";
    }
}



//------------------------------------------------------------------------------
bool InstantHitWeaponClient::startFiring()
{
    if (!WeaponSystem::startFiring()) return false;
    
    s_log << Log::debug('l')
          << "startFiringClient\n";

    // Schedule next tracer round
    task_tracer_dist_ = s_scheduler.addTask(PeriodicTaskCallback(this, &InstantHitWeaponClient::handleTracerDistance),
                                            TRACER_DIST_UPDATE_DT,
                                            "InstantHitWeapon::handleTracerDistance",
                                            &fp_group_);
    
    // Single-event based in order to allow for random fluctuations.
    handleHitFeedback(NULL);


    TankVisual * tank_visual = (TankVisual*)tank_->getUserData();
    assert(tank_visual);

    
    EnableGroupVisitor v(s_params.get<std::string>(section_ + ".effect_group"), true);
    tank_visual->getWrapperNode()->getOsgNode()->accept(v);

    tank_visual->enableSecondaryWeaponEffect(true);



    assert(snd_firing_.get() == NULL);

    snd_firing_ = s_soundmanager.playLoopingEffect(s_params.get<std::string>(section_ + ".sound_effect"),
                                                   tank_visual->getWrapperNode()->getOsgNode());
    
    snd_firing_->setGain(SND_FIRING_GAIN);

    return true;
}


//------------------------------------------------------------------------------
bool InstantHitWeaponClient::stopFiring()
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
    if(tank_visual)
    {
        EnableGroupVisitor v(s_params.get<std::string>(section_ + ".effect_group"), false);
        tank_visual->getWrapperNode()->getOsgNode()->accept(v);

        tank_visual->enableSecondaryWeaponEffect(false);
    }

    return true;
}

//------------------------------------------------------------------------------
void InstantHitWeaponClient::onOverheat()
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
void InstantHitWeaponClient::handleTracerDistance(float dt)
{
    doRayTest(game_logic_client_->getPuppetMaster()->getGameState()->getSimulator());




    for (unsigned i=0; i < tracer_effect_.size(); ++i)
    {
        for (unsigned eff=0; eff<tracer_effect_[i]->getNumEffects(); ++eff)
        {
            osgParticle::ModularEmitter * emitter = tracer_effect_[i]->getEffect(eff).emitter_.get();

            osgParticle::RadialShooter * shooter = dynamic_cast<osgParticle::RadialShooter*>(emitter->getShooter());
            assert(shooter);
            
            float speed = shooter->getInitialSpeedRange().mid();

            emitter->getParticleSystem()->getDefaultParticleTemplate().setLifeTime(
                std::max(ray_intersection_dist_ - TRACER_LENGTH, TRACER_LENGTH)/speed);
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  Uses the current ray_intersection_dist_ and hit_normal_ to create
 *  a particle & sound effect.
 *
 *  Reschedules itself.
 */
void InstantHitWeaponClient::handleHitFeedback(void *)
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
        
        uint8_t object_hit_type;
        uint8_t weapon_hit_type;
        
        /// XXX better solution??
        if(section_ == "mg")
        {
            weapon_hit_type = WHT_MACHINE_GUN;
        } else if(section_ == "flamethrower")
        {
            weapon_hit_type = WHT_FLAME_THROWER;
        } else if(section_ == "laser")
        {
            weapon_hit_type = WHT_LASER;
        } else if (section_ == "tractor_beam")
        {
            weapon_hit_type = WHT_TRACTOR_BEAM;
        } else
        {
            weapon_hit_type = WHT_MACHINE_GUN;
            s_log << Log::warning << " Unknown weapon hit type in InstantHitWeapon.\n";
        } 

        /// Object hit by ray
        if(hit_rigid_body_type_ == "Tank")
        {
            object_hit_type = OHT_TANK;
        }
        else if(hit_rigid_body_type_ == "Water")
        {
            object_hit_type = OHT_WATER;
        } else
        {
            object_hit_type = OHT_OTHER;
        }

        RakNet::BitStream args;
        args.Write(tank_->getOwner());
        args.Write(hit_player_);
        args.Write(hit_pos);
        args.Write(hit_normal_);
        args.Write(weapon_hit_type);
        args.Write(object_hit_type);

        game_logic_client_->executeCustomCommand(CSCT_WEAPON_HIT, args);
    }

    // Schedule next hit feedback
    task_hitfeedback_ = s_scheduler.addEvent(SingleEventCallback(this, &InstantHitWeaponClient::handleHitFeedback),
                                             s_params.get<float>(section_ + ".feedback_interval"),
                                             NULL,
                                             "Hit Feedback",
                                             &fp_group_);
}

#endif // #ifndef DEDICATED_SERVER

#endif // #ifndef TANK_MACHINE_GUN_INCLUDED
