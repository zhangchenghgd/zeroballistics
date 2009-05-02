

#include "GameLogicServerBeaconstrike.h"

#include <limits>

#include "physics/OdeSimulator.h"


#include "AutoRegister.h"
#include "Beacon.h"
#include "BeaconBoundaryServer.h"
#include "GameState.h"
#include "SpawnStageServer.h"
#include "NetworkCommandServer.h"
#include "TerrainData.h"
#include "../Tank.h"
#include "../Projectile.h"
#include "../WeaponSystem.h"
#include "../TankMachineGun.h"
#include "../TankMine.h"
#include "../SpawnPos.h"


#ifndef DEDICATED_SERVER
#include "TerrainVisual.h"
#include "TerrainDataClient.h"
#include "RigidBodyVisual.h"
#endif

#undef min
#undef max

#ifdef ENABLE_DEV_FEATURES
REGISTER_CLASS(GameLogicServer, GameLogicServerBeaconstrike);
#endif

const float BEACON_CONQUERED_HEIGHT_OFFSET = 1.5f;

/// Name of the geom which defines the beacon dispenser spawn volume.
const char * BEACON_SPAWN_POS_NAME = "b_spawn_pos";


//------------------------------------------------------------------------------
GameLogicServerBeaconstrike::GameLogicServerBeaconstrike() :
    winner_team_(INVALID_TEAM_ID)
{
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::init(PuppetMasterServer * master)
{
    GameLogicServerCommon::init(master);

    for (unsigned t=0; t<NUM_TEAMS_BS; ++t)
    {
        team_[t].setId(t);
        team_[t].setConfigName(TEAM_CONFIG_BS[t]);
        score_.addTeam(&team_[t]);
    }

    physics::OdeSimulator * sim = puppet_master_->getGameState()->getSimulator();
    beacon_boundary_.reset(new BeaconBoundaryServer(sim->getStaticSpace()));    
    
}

//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::reset()
{
    GameLogicServerCommon::reset();

    for (unsigned i=0; i<spawn_stage_.size(); ++i)
    {
        delete spawn_stage_[i];
    }
    spawn_stage_.clear();



    for (unsigned t=0; t<NUM_TEAMS_BS; ++t)
    {
        team_[t].reset();
    }    
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::handleInput(ServerPlayer * player, const PlayerInput & input)
{
    GameLogicServerCommon::handleInput(player, input);
    
    
    Tank * tank = dynamic_cast<Tank*>(player->getControllable());
    assert(tank != NULL);

    // pickup / drop beacon
    if (input.action1_)
    {
        if (!tank->getCarriedObject() && !tank->isBeaconActionExecuted())
        {
            pickupBeacon(tank);
            if(tank->getCarriedObject()) tank->setBeaconActionExecuted(true); //pickup successful
        }

        if (tank->getCarriedObject() && !tank->isBeaconActionExecuted())
        {
            Vector velocity = tank->getTarget()->getLocalLinearVel();
            if(velocity.length() < s_params.get<float>("server.logic_beaconstrike.beacon_dropping_speed_threshold"))
            {                    
                    dropBeacon(tank);
                    if(!tank->getCarriedObject()) tank->setBeaconActionExecuted(true); //drop successful
            } 
        }
    }
    else
    {
        tank->setBeaconActionExecuted(false);
    }
}

//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::addPlayer(const Player * player, bool newly_connected)
{
    GameLogicServerCommon::addPlayer(player, newly_connected);

    
    if (newly_connected)
    {
        // transmit the current scores and team assignments to a newly
        // connected player
        PuppetMasterServer::PlayerContainer::const_iterator it;
        PuppetMasterServer::PlayerContainer pc = puppet_master_->getPlayers();
    
        for(it = pc.begin(); it != pc.end(); it++)
        {
            if (it->getId() != player->getId())
            {
                // transmit carried beacons
                Tank * tank = dynamic_cast<Tank*>(it->getControllable());
                if (tank &&
                    tank->getCarriedObject())
                {
                    sendPickupBeacon(tank->getId(),
                                     tank->getCarriedObject()->getId(),
                                     player->getId(),
                                     CST_SINGLE);
                }
            }
        }

        // Send current beacon connections
        sendBeaconConnections(player->getId(), CST_SINGLE);


        if (state_ == TGS_WON)
        {
            sendGameWon(winner_team_, player->getId(), CST_SINGLE);
        }
        
        puppet_master_->requestPlayerReady(player->getId());    
    }
}



//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::removePlayer(const SystemAddress & pid)
{
    assert(puppet_master_->getPlayer(pid));
    
    Tank * tank = dynamic_cast<Tank*>(puppet_master_->getPlayer(pid)->getControllable());
    
    if (tank)
    {
        if (tank->getCarriedObject()) dropBeacon(tank);
    }

    GameLogicServerCommon::removePlayer(pid);
}

//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::loadLevel(const std::string & name)
{
    GameLogicServerCommon::loadLevel(name);

    winner_team_ = INVALID_TEAM_ID;
    
    if (spawn_stage_.empty())
    {
        s_log << Log::warning
              << "No conquerables\n";
    }        

    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerBeaconstrike::handleLogic),
                        1.0f / s_params.get<float>("server.logic.logic_fps"),
                        "GameLogicServerBeaconstrike::handleLogic",
                        &fp_group_);    
}

//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onGameObjectAdded(GameObject * object)
{
    GameLogicServerCommon::onGameObjectAdded(object);

    if (object->getType() == "Beacon")
    {
        onBeaconAdded((Beacon*)object);
    }    
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::executeCustomCommand(unsigned type, RakNet::BitStream & args)
{
    GameLogicServerCommon::executeCustomCommand(type, args);
}


//------------------------------------------------------------------------------
/**
 *  \return True if the object is used and should not be added to ordinary gamestate
 *  and transmitted to clients, false otherwise.
 */
bool GameLogicServerBeaconstrike::onLevelObjectLoaded(RigidBody * obj,
                                                      const LocalParameters & params)
{
    unsigned stage_no = 1;
    std::string role;
    std::string team;

    // We only are interested if the "role" property is set
    try
    {
        role = params.get<std::string>("logic.role");
    } catch (ParamNotFoundException & e)
    {
        return false;
    }


    // Get additional info....
    try
    {
        team = params.get<std::string>("logic.team");
    } catch (ParamNotFoundException & e) {}
    try
    {
        stage_no = params.get<unsigned>("logic.stage");
        // numbering starts with 1 in Grome
        if (stage_no == 0)
        {
            s_log << Log::error
                  << "stage numbering starts with 1 in Grome!\n";
        } else
        {
            --stage_no;
        }
    } catch (ParamNotFoundException & e) {}

    if (team != "attacker" && team != "defender" && !team.empty())
    {
        s_log << Log::error
              << "Invalid team: " << team
              << " for body "
              << *obj << "\n";
        return false;
    }
    TEAM_ID team_id = team == "attacker" ? TEAM_ID_ATTACKER : TEAM_ID_DEFENDER;

    // Create as many stages as neccessary to accomodate the new
    // stage.
    if (stage_no >= spawn_stage_.size()) 
    {
        unsigned old_size = spawn_stage_.size();
        spawn_stage_.resize(std::max((unsigned)spawn_stage_.size(), stage_no+1), NULL);

        for (unsigned i=old_size; i<spawn_stage_.size(); ++i)
        {
            spawn_stage_[i] = new SpawnStageServer();
        }
    }    
    SpawnStageServer * stage = spawn_stage_[stage_no];



    


    SpawnPos * spawn_pos = extractSpawnPos(obj, params);
    if (spawn_pos)
    {
        stage->addPlayerSpawnPos(spawn_pos, team_id);
        return true;
    } else if (role == "dispenser")
    {
        unsigned i = obj->getTarget()->getGeomIndex(BEACON_SPAWN_POS_NAME);

        if (i == (unsigned)-1)
        {
            s_log << Log::error << "Beacon dispenser has no geom " << BEACON_SPAWN_POS_NAME << "\n";
        } else
        {
            physics::OdeGeom * geom = obj->getTarget()->getGeoms()[i];
            obj->getTarget()->detachGeom(i);
            geom->setSpace(sensor_space_.get());
            
            stage->setBeaconSpawnPos(std::auto_ptr<SpawnPos>(new SpawnPos(geom, NULL)));
        }
    } else if (role == "fixed_beacon")
    {
        if (obj->getType() != "Beacon")
        {
            s_log << Log::error
                  << obj->getName()
                  << " has role \"fixed beacon\" but is no Beacon.\n";
            return false;
        }

        Beacon * beacon = (Beacon*)obj;

        if (team == "defender")
        {
            beacon->setTeamId(TEAM_ID_DEFENDER);
            stage->setBeacon(beacon);
        } else
        {
            beacon->setTeamId(TEAM_ID_ATTACKER);
        }
        beacon->setFixed();
    }

    return false;
}


//------------------------------------------------------------------------------
Matrix GameLogicServerBeaconstrike::getRespawnPos(const SystemAddress & pid, unsigned stage)
{
    if (spawn_stage_.empty())
    {
        throw Exception("No conquerable base");
    }

    ServerTeam * team = (ServerTeam*)score_.getTeam(pid);
    if (!team)
    {
        Exception e;
        e << pid
          << " is assigned to no team.\n";
        throw e;
    }

    if (team->getId() != spawn_stage_[stage]->getOwnerTeam() ||
        stage >= spawn_stage_.size())
    {
        Exception e("Ignoring respawn for ");
        e <<  pid << " at invalid stage.";
        throw e;
    }


    return getRandomSpawnPos(spawn_stage_[stage]->getSpawnPositions(team->getId()));
}

//------------------------------------------------------------------------------
Tank * GameLogicServerBeaconstrike::createNewPlayerTank(const SystemAddress & pid)
{
    ServerTeam * team = (ServerTeam*)score_.getTeam(pid);
    
    RigidBody * rb = createRigidBody(team->getTankName(), "Tank");
    return dynamic_cast<Tank*>(rb);
}




//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onTankKilled(Tank * tank, const SystemAddress & killer_id)
{
    assert(killer_id != tank->getOwner()); // suicide is always "killed by UNASSIGNED_SYSTEM_ADDRESS"
    
    // destroy beacon if tank gets destroyed
    if (tank->getCarriedObject())
    {
        assert(dynamic_cast<Beacon*>(tank->getCarriedObject()));

        Beacon * b = (Beacon*)tank->getCarriedObject();
        b->setCarried(false);
        tank->dropObject();
        sendDropBeacon(tank->getId());

        killBeacon(b, killer_id);
    }


    // Fill beacon queue
    if (score_.getTeamId(tank->getOwner()) == TEAM_ID_DEFENDER)
    {
        team_[TEAM_ID_ATTACKER].fillBeaconQueue(
            s_params.get<float>("server.logic_beaconstrike.beacon_queue_increment_per_kill"));
    }



    // Update player score
    PlayerScore * killed_score = score_.getPlayerScore(tank->getOwner());
    assert(killed_score);

    ++killed_score->deaths_;
    if (killer_id == UNASSIGNED_SYSTEM_ADDRESS)
    {
        // Suicide - reduce score by one, upgrade points by one
        --killed_score->score_;
        if (killed_score->upgrade_points_) --killed_score->upgrade_points_;
    } else 
    {
        PlayerScore * killer_score = score_.getPlayerScore(killer_id);

        // killing player doesn't neccessarily exist on server
        // anymore...
        if (killer_score)
        {
            bool teamkill = killer_score->getTeam() == killed_score->getTeam();
            
            if (teamkill)
            {
                --killer_score->score_;
            } else
            {
                // Ordinary kill - add to score
                killer_score->score_          += KILL_SCORE_VALUE;
                killer_score->kills_          += 1;
                killer_score->upgrade_points_ += KILL_UPGRADE_VALUE;
                
                // if killing strong opponent, more than 2 upgrade levels above, killer gets
                // twice the upgrade points, vice versa on weak opponent
                int killed_num_upgrades =   (killed_score->active_upgrades_[0] + 
                                             killed_score->active_upgrades_[1] + 
                                             killed_score->active_upgrades_[2]);
                int killer_num_upgrades =   (killer_score->active_upgrades_[0] + 
                                             killer_score->active_upgrades_[1] + 
                                             killer_score->active_upgrades_[2]);

                if((killed_num_upgrades - killer_num_upgrades) >=  2) killer_score->upgrade_points_ += KILL_UPGRADE_VALUE;
                if((killed_num_upgrades - killer_num_upgrades) <= -2) killer_score->upgrade_points_ -= KILL_UPGRADE_VALUE;

                // Additional bonus if destroyed tank carried a beacon
                if (tank->getCarriedObject())                         killer_score->upgrade_points_ += BEACON_DESTROY_VALUE;
            }

            sendScoreUpdate(killer_id, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
        }
    }
    sendScoreUpdate(tank->getOwner(), UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);

    // Kill damage assist
    float assist_damage_percentage;
    SystemAddress assist_killer_id = tank->getTopAssistant(killer_id, assist_damage_percentage);
    handleAssist(assist_killer_id,
                 tank->getOwner(), 
                 assist_damage_percentage, 
                 KILL_ASSIST_SCORE_VALUE, 
                 KILL_ASSIST_UPGRADE_VALUE, 
                 true);
}





//------------------------------------------------------------------------------
/**
 *  Deals damage for a hit, applies impact impulse to dynamic
 *  objects. Accounts for shot projectiles and hit objects(accuracy
 *  calculation).
 *
 *  \param hit_object The object to deal damage to.
 *
 *  \param hit_percentage 1.0 for a direct hit, lower values indicate
 *  a splash hit.
 *
 *  \param info The collision information for the primary, direct hit.
 */
void GameLogicServerBeaconstrike::onProjectileHit(Projectile * projectile,
                                                  RigidBody * hit_object,
                                                  float hit_percentage,
                                                  const physics::CollisionInfo & info)
{
    const SystemAddress & proj_owner = projectile->getOwner();
    
    if (hit_object && hit_object->getType() == "Beacon") // handle beacon damage, deletion
    {
        // neither increment shots nor hits -> beacon hit is neutral
        // w.r.t. accuracy
        
        Beacon * beacon = (Beacon*)hit_object;

        // always deal same amount of damage to beacons, independent from upgrades
        beacon->dealWeaponDamage((int)(s_params.get<unsigned>("dummy_weapon_system.beacon_damage")*hit_percentage),
                                 proj_owner);        
        
        if(beacon->getHitpoints() <= 0) killBeacon(beacon, proj_owner);

        
        if (hit_percentage == 1.0f)
        {
            sendWeaponHit(proj_owner, UNASSIGNED_SYSTEM_ADDRESS,
                          info, projectile->getSplashRadius() ? WHT_PROJECTILE_SPLASH_HIT_TANK :
                          WHT_PROJECTILE_HIT_TANK);
        }

        hit_object = NULL;
    }

    
    
    GameLogicServerCommon::onProjectileHit(projectile,
                                           hit_object,
                                           hit_percentage,
                                           info);




    // score & accuracy calculation
    PlayerScore * shooter_score = score_.getPlayerScore(proj_owner);
    // player score is null if firing tank left game while his
    // projectile was in progress
    if (shooter_score && hit_object && hit_object->getType() == "Tank")
    {
        Tank * tank = (Tank*)hit_object;
        
        shooter_score->shots_++;

        TEAM_ID hit_team_id     = score_.getTeamId(tank->getOwner());
        TEAM_ID shooter_team_id = score_.getTeamId(proj_owner);
            
        // only count direct hits, do not count hits on wrecks or
        // team members
        if (hit_percentage == 1.0f &&
            shooter_team_id != hit_team_id)
        {
            shooter_score->hits_++;
        }

        // sends score for acc, eff calc. to shooting player only
        sendScoreUpdate(proj_owner, proj_owner, CST_SINGLE);
    }    
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onMachinegunHit(TankMachineGun * gun,
                                                  RigidBody * hit_object)
{
    GameLogicServerCommon::onMachinegunHit(gun, hit_object);

    if (hit_object->getType() == "Beacon") // handle beacon damage, deletion
    {        
        Beacon * beacon = (Beacon*)hit_object;
        if (!beacon->isScheduledForDeletion())
        {
            beacon->dealWeaponDamage((int)gun->getDamage(), gun->getTank()->getOwner());
            if(beacon->getHitpoints() <= 0) killBeacon(beacon, gun->getTank()->getOwner());
        }
    } 
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onTimeLimitExpired()
{
    sendGameWon(TEAM_ID_DEFENDER, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
    gameWon();
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onPlayerTeamChanged(Player * player)
{
    // Start countdown if first player is assigned to any team.
    if (task_update_time_limit_ == INVALID_TASK_HANDLE &&
        score_.getTeam(player->getId()))
    {
        s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerBeaconstrike::fillBeaconQueue),
                            1.0f,
                            "GameLogicServerBeaconstrike::fillBeaconQueue",
                            &fp_group_);

        startTimeLimit(s_params.get<float>("server.settings.time_limit"));
    }
}


//------------------------------------------------------------------------------
GameLogicServer * GameLogicServerBeaconstrike::create()
{
    return new GameLogicServerBeaconstrike();
}



//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onBeaconAdded(Beacon * beacon)
{
    assert (beacon->getTeamId() <= NUM_TEAMS_BS || beacon->getTeamId() == INVALID_TEAM_ID);

    beacon->addObserver(ObserverCallbackFun2(this, &GameLogicServerBeaconstrike::onBeaconScheduledForDeletion),
                        GOE_SCHEDULED_FOR_DELETION, &fp_group_);
    beacon->addObserver(ObserverCallbackFun2(this, &GameLogicServerBeaconstrike::onBeaconDeployedChanged),
                        BE_DEPLOYED_CHANGED, &fp_group_);
    beacon->addObserver(ObserverCallbackFun2(this, &GameLogicServerBeaconstrike::onBeaconInRadiusChanged),
                        BE_INSIDE_RADIUS_CHANGED, &fp_group_);

    beacon_boundary_->addBeacon(beacon);
}



//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onBeaconScheduledForDeletion(Observable * observable, unsigned event)
{
    Beacon * beacon = dynamic_cast<Beacon*>(observable);
    assert(beacon);

    // assert(!beacon->isCarried()); This is not a valid assertion
    // because this can happen at reset time
    beacon_boundary_->deleteBeacon(beacon);
    
    updateBoundary(true);
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::onBeaconDeployedChanged(Observable* observable, unsigned event)
{
    updateBoundary(true);
}


//------------------------------------------------------------------------------
/**
 *  If a beacon newly comes into the radius, try to activate it by
 *  hovering.
 */
void GameLogicServerBeaconstrike::onBeaconInRadiusChanged(Observable* observable, unsigned event)
{
    Beacon * beacon = (Beacon*)observable;
    assert(event == BE_INSIDE_RADIUS_CHANGED);

    if (beacon->isInsideRadius() &&
        beacon->getState() == BS_UNDEPLOYED)
    {
        beacon->rise();
    }
}


//------------------------------------------------------------------------------
/**
 *  
 */
void GameLogicServerBeaconstrike::handleLogic(float dt)
{
    if (state_ != TGS_IN_PROGRESS) return;

    // remove health for undeployed beacons
    unsigned i=0;
    while(i < beacon_boundary_->getBeacons().size())
    {
        Beacon * cur_beacon = beacon_boundary_->getBeacons()[i];

        cur_beacon->handleHealth(dt);
        
        if (cur_beacon->getHitpoints() == 0)
        {
            killBeacon(cur_beacon, UNASSIGNED_SYSTEM_ADDRESS);
        } else ++i;
    }

    updateBoundary(false);

    
    // Test whether tank collides with own base
    sensor_space_->collide();


    // reload ammo code for tank inside beacon boundary
    // Traverse all controllables and reload if in active area
    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        Tank * tank = dynamic_cast<Tank*>(it->getControllable());
        if (!tank) continue;

        // Player with tank must have team
        assert(score_.getTeamId(it->getId()) != INVALID_TEAM_ID);        
        
        if (beacon_boundary_->isInsideArea(tank->getTarget()->getGeom(TANK_VOLUME_NAME))
            [score_.getTeamId(it->getId())])
        {
            // only reload 1st weapon right now
            tank->getWeaponSystems()[0]->setAmmo(tank->getWeaponSystems()[0]->getAmmo() + 5);

        }


        if (tank->getGlobalLinearVel().length() < s_params.get<float>("server.logic.tank_heal_velocity_threshold"))
        {
            tank->startHealing();
        } else
        {
            tank->stopHealing();
        }
    }


    
    // check if beacon spawn pos is free and spawn beacon if queued
    // beacons available. At the same time count free bases.
    unsigned num_free_bases = 0;
    for (unsigned b=0; b<spawn_stage_.size(); ++b)
    {
        if (spawn_stage_[b]->getBeacon()) ++num_free_bases;
        else if ( spawn_stage_[b]->getBeaconSpawnPos()               &&
                 !spawn_stage_[b]->getBeaconSpawnPos()->isOccupied() &&
                 team_[TEAM_ID_ATTACKER].getBeaconFromQueue())
        {
            Beacon * beacon = (Beacon*)createRigidBody(team_[TEAM_ID_ATTACKER].getBeaconName(),
                                                       "Beacon");
                
            beacon->setTeamId(TEAM_ID_ATTACKER);
            beacon->setTransform(spawn_stage_[b]->getBeaconSpawnPos()->getTransform());

            puppet_master_->addGameObject(beacon);
        }
    }    

    // See if base was conquered...
    for (unsigned c=0; c<spawn_stage_.size(); ++c)
    {
        Beacon * def_beacon = spawn_stage_[c]->getBeacon();
        if (!def_beacon) continue;
        
        if (beacon_boundary_->isInsideArea(def_beacon->getBodyGeom())[TEAM_ID_ATTACKER])
        {
            spawn_stage_[c]->setBeacon(NULL);
            def_beacon->scheduleForDeletion();

            Matrix new_pos = def_beacon->getTransform();
            new_pos.getTranslation().y_ += BEACON_CONQUERED_HEIGHT_OFFSET;
            
            Beacon * new_beacon = (Beacon*)createRigidBody(team_[TEAM_ID_ATTACKER].getBeaconName(),
                                                           "Beacon");
            new_beacon->setTeamId(TEAM_ID_ATTACKER);
            new_beacon->setTransform(new_pos);
            new_beacon->setFixed();
            puppet_master_->addGameObject(new_beacon);

            updateBoundary(true);

            // Don't send "base conquered" message for last stage, as
            // "game won" will be sent.
            if (num_free_bases > 1)
            {
                sendStageConquered(c, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
                score_.setTimeLeft(score_.getTimeLeft() + s_params.get<float>("server.logic_beaconstrike.time_extension"));
                sendScoreUpdate(UNASSIGNED_SYSTEM_ADDRESS, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
            } else
            {
                gameWon();
                sendGameWon(TEAM_ID_ATTACKER, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
            }
        }
    }
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::fillBeaconQueue(float dt)
{
    team_[TEAM_ID_ATTACKER].fillBeaconQueue(
        dt*s_params.get<float>("server.logic_beaconstrike.beacon_queue_increment_per_second"));    
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::killBeacon(Beacon * beacon,
                                             const SystemAddress & killer)
{
    beacon->scheduleForDeletion();

    PlayerScore * killer_score = score_.getPlayerScore(killer);

    // killing player doesn't neccessarily exist on server
    // anymore...
    if (killer_score)
    {
        if (killer_score->getTeam()->getId() == beacon->getTeamId())
        {
            // Killed your own beacon            
        } else
        {
            killer_score->upgrade_points_ += BEACON_DESTROY_VALUE;
        }        
        sendKill(killer, UNASSIGNED_SYSTEM_ADDRESS, PKT_UNKNOWN1, false);
        sendScoreUpdate(killer, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
    }


    // Kill damage assist
    float assist_damage_percentage;
    SystemAddress assist_killer_id = beacon->getTopAssistant(killer, assist_damage_percentage);
    handleAssist(assist_killer_id, UNASSIGNED_SYSTEM_ADDRESS, assist_damage_percentage,
                 0, BEACON_DESTROY_VALUE, true);

    sendBeaconDestroyed(beacon->getId());
}

//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::pickupBeacon(Tank * tank)
{
    assert(!tank->getCarriedObject());
    
    s_log << Log::debug('l')
          << "GameLogicServerBeaconstrike::pickupBeacon\n";
    
    // Find canditate beacons to pick up
    Beacon * nearest_beacon = NULL;
    float nearest_dist = std::numeric_limits<float>::max();    

    TEAM_ID team_id = score_.getTeamId(tank->getOwner());

    // Select nearest beacon to pick up
    for (BeaconBoundary::BeaconContainer::iterator it = beacon_boundary_->getBeacons().begin();
         it != beacon_boundary_->getBeacons().end();
         ++it)
    {
        // Skip beacons of other team
        if ((*it)->getTeamId() != team_id &&
            (*it)->getTeamId() != INVALID_TEAM_ID) continue;
        
        // Skip fixed beacons
        if ((*it)->getState() == BS_FIXED) continue;
        
        // skip beacons already held by another tank
        if((*it)->getState() == BS_CARRIED) continue;
                   
        // skip beacons not in pickup radius
        float distance = ((*it)->getPosition() - tank->getPosition()).length();
        if(distance > s_params.get<float>("server.logic_beaconstrike.beacon_pickup_radius")) continue;
        
        if(distance < nearest_dist)
        {
            nearest_dist   = distance;
            nearest_beacon = *it;
        }
    }


    // Bail if no beacon in range
    if (!nearest_beacon) return;

    // Bail if something blocks LOS
    if (!tank->pickupObject(nearest_beacon)) return;
    
    nearest_beacon->setCarried(true);

    // don't touch gravity on client, so do it here instead of in setCarried()
    nearest_beacon->getTarget()->enableGravity(false);
    
    // Inform clients of pickup
    sendPickupBeacon(tank->getId(), nearest_beacon->getId(),
                     UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
}

//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::dropBeacon(Tank * tank)
{
    s_log << Log::debug('l')
          << "GameLogicServerBeaconstrike::dropBeacon\n";

    assert(tank->getCarriedObject());
    
    Beacon * beacon = (Beacon*)tank->getCarriedObject();
    
    tank->dropObject();
    beacon->setCarried(false);

    beacon->getTarget()->enableGravity(true);

    updateBoundary(false);

    sendDropBeacon(tank->getId());
    
    if (beacon->isInsideRadius()) beacon->rise();
}

//------------------------------------------------------------------------------
/**
 *  Boundary is updated regularly to visualize radius state of carried
 *  beacons. Don't broadcast beacon connections in this case.
 */
void GameLogicServerBeaconstrike::updateBoundary(bool broadcast_connections)
{
    beacon_boundary_->update();

    if (broadcast_connections)
    {
        sendBeaconConnections(UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
    }
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::sendPickupBeacon(uint16_t tank_id, uint16_t beacon_id,
                                                const SystemAddress & target_id,
                                                COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    args.Write(tank_id);
    args.Write(beacon_id);

    network::CustomServerCmd pickup_cmd(CSCTBS_PICKUP_BEACON, args);
    puppet_master_->sendNetworkCommand(pickup_cmd, target_id, type);
}


//------------------------------------------------------------------------------
/**
 *  Pickup beacon only goes to ready players, drop beacon as well.
 */
void GameLogicServerBeaconstrike::sendDropBeacon(uint16_t tank_id)
{
    RakNet::BitStream args;
    args.Write(tank_id);
    
    network::CustomServerCmd drop_cmd(CSCTBS_DROP_BEACON, args);
    puppet_master_->sendNetworkCommand(drop_cmd, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
}


//------------------------------------------------------------------------------.
void GameLogicServerBeaconstrike::sendBeaconDestroyed(uint16_t beacon_id)
{
    RakNet::BitStream args;
    args.Write(beacon_id);
    
    network::CustomServerCmd destroyed_cmd(CSCTBS_BEACON_DESTROYED, args);
    puppet_master_->sendNetworkCommand(destroyed_cmd, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
}


//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::sendBeaconConnections(const SystemAddress & target_id,
                                                        COMMAND_SEND_TYPE type)
{
    std::vector<std::pair<uint16_t, uint16_t> > connected_beacons =
        beacon_boundary_->getConnectedBeacons();

    RakNet::BitStream args;
    args.Write((uint16_t)connected_beacons.size());

    for (unsigned c=0; c<connected_beacons.size(); ++c)
    {        
        args.Write(connected_beacons[c].first);
        args.Write(connected_beacons[c].second);
    }

    network::CustomServerCmd connection_cmd(CSCTBS_BEACON_CONNECTIONS, args);
    puppet_master_->sendNetworkCommand(connection_cmd, target_id, type);
}



//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::sendStageConquered(uint8_t stage,
                                                  const SystemAddress & target_id,
                                                  COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    args.Write((uint8_t)stage);

    network::CustomServerCmd upgrade_cmd(CSCTBS_BASE_CONQUERED, args);
    puppet_master_->sendNetworkCommand(upgrade_cmd, target_id, type);
}



//------------------------------------------------------------------------------
void GameLogicServerBeaconstrike::sendGameWon(TEAM_ID winner_team,
                                              const SystemAddress & target_id,
                                              COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    args.Write(winner_team);
    
    network::CustomServerCmd game_won_cmd(CSCTBS_GAME_WON, args);
    puppet_master_->sendNetworkCommand(game_won_cmd, target_id, type);

    assert(score_.getTeam(winner_team));
    s_log << score_.getTeam(winner_team)->getName()
          << " team has won the match.\n";    
}


