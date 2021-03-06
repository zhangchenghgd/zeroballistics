

#include "GameLogicServerCommon.h"

#include <limits>


#include <raknet/RakNetTypes.h>


#include "physics/OdeSimulator.h"
#include "physics/OdeRigidBody.h"
#include "physics/OdeCollisionSpace.h"

#include "GameState.h"
#include "PlayerInput.h"
#include "NetworkCommandServer.h"
#include "Projectile.h"
#include "TankMine.h"
#include "InstantHitWeapon.h"
#include "Tank.h"
#include "WeaponSystem.h"
#include "ParameterManager.h"
#include "LevelData.h"
#include "Profiler.h"
#include "TerrainData.h"
#include "SpawnPos.h"

#include "Scheduler.h"
#include "AIPlayer.h"
#include "Paths.h"

#include "ObjectParts.h"

#include "RankingMatchEvents.h"

#include "WaypointManagerServer.h" ///< XXX move this to puppetmasterserver too

#ifndef DEDICATED_SERVER
#include "TerrainVisual.h"
#include "TerrainDataClient.h"
#include "RigidBodyVisual.h"
#endif




#undef max
#undef min


const float MIDAIR_SPAWN_HEIGHT = 1.0;

const std::string MSG_GAME_COMMENCING =      "Game commencing...";


const std::string ACTIVATE_APPENDIX_COLLISION  = "cd";
const std::string ACTIVATE_APPENDIX_WEAPON_HIT = "wd";

//------------------------------------------------------------------------------
GameLogicServerCommon::GameLogicServerCommon() :
    sensor_space_(new physics::OdeCollisionSpace("sensor", true)),
    state_(TGS_WON),
    task_update_time_limit_(INVALID_TASK_HANDLE)
{    
#ifndef DEDICATED_SERVER
    terrain_visual_ = new terrain::TerrainVisual();
#endif

    s_console.addFunction("addBot",
                          ConsoleFun(this, &GameLogicServerCommon::addBot), &fp_group_);
    s_console.addFunction("removeBot",
                          ConsoleFun(this, &GameLogicServerCommon::removeBot), &fp_group_);

#ifdef ENABLE_DEV_FEATURES    
    s_console.addFunction("dumpServerSimulator",
                          ConsoleFun(this, &GameLogicServerCommon::dumpSimulatorContents), &fp_group_);
    s_console.addFunction("dumpServerCollisionSpaces",
                          ConsoleFun(this, &GameLogicServerCommon::dumpSpaceContents), &fp_group_);
    s_console.addFunction("addPoints",
                          ConsoleFun(this, &GameLogicServerCommon::addPoints), &fp_group_);
    s_console.addFunction("timeLimit",
                          ConsoleFun(this, &GameLogicServerCommon::setTimeLimit), &fp_group_);
    s_console.addFunction("startMatch",
                          ConsoleFun(this, &GameLogicServerCommon::startMatch), &fp_group_);

    s_console.addVariable("weapon_system_debug",
                          &WeaponSystem::weapon_system_debug_,
                          &fp_group_);
    
#endif
}


//------------------------------------------------------------------------------
GameLogicServerCommon::~GameLogicServerCommon()
{
    s_log << Log::debug('d')
          << "GameLogicServerCommon destructor\n";
    
    WeaponSystem::setGameLogicServer(NULL);
    Projectile  ::setGameLogicServer(NULL);
    TankMine    ::setGameLogicServer(NULL);

    for (unsigned pos=0; pos<spawn_pos_.size(); ++pos)
    {
        delete spawn_pos_[pos];
    }
}


//------------------------------------------------------------------------------
/**
 *  Instead of constructor because we are created by factory.
 */
void GameLogicServerCommon::init(PuppetMasterServer * master)
{
    GameLogicServer::init(master);

    WeaponSystem::setGameLogicServer(this);
    Projectile  ::setGameLogicServer(this);
    TankMine    ::setGameLogicServer(this);


    
    physics::OdeSimulator * sim = puppet_master_->getGameState()->getSimulator();

    sim->enableCategoryCollisions(CCCS_STATIC,      CCCS_STATIC,      false);
    sim->enableCategoryCollisions(CCCS_STATIC,      CCCS_HEIGHTFIELD, false);

    sim->enableCategoryCollisions(CCCS_DYNAMIC,     CCCS_WHEEL_RAY,   false);
     
    sim->enableCategoryCollisions(CCCS_PROJECTILE,  CCCS_PROJECTILE,  false);
    sim->enableCategoryCollisions(CCCS_PROJECTILE,  CCCS_WHEEL_RAY,   false);
 
    sim->enableCategoryCollisions(CCCS_HEIGHTFIELD, CCCS_HEIGHTFIELD, false);
    sim->enableCategoryCollisions(CCCS_HEIGHTFIELD, CCCS_WHEEL_RAY,   false);
 
    sim->enableCategoryCollisions(CCCS_WHEEL_RAY,   CCCS_WHEEL_RAY,   false);
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::handleInput(ServerPlayer * player, const PlayerInput & input)
{
    Tank * tank = dynamic_cast<Tank*>(player->getControllable());
    assert(tank != NULL);


    // Jump button
    const float JUMP_TORQUE = 2.0f;
    const float JUMP_FORCE  = 50.0f;
    const float JUMP_SPEED_THRESHOLD = 0.1;
    if (input.action3_)
    {
        if (tank->getGlobalAngularVel().length() < JUMP_SPEED_THRESHOLD &&
            tank->getGlobalLinearVel ().length() < JUMP_SPEED_THRESHOLD)
        {
            // Always try to get tank upright
            if (tank->getTransform().getX().y_ > 0.0f)
            {
                tank->getTarget()->addLocalTorque(Vector(0,0,-JUMP_TORQUE));
            } else
            {
                tank->getTarget()->addLocalTorque(Vector(0,0,JUMP_TORQUE));
            }

            if (tank->getTransform().getY().y_ > 0.0f)
            {
                tank->getTarget()->addLocalForce (Vector(0,JUMP_FORCE,0));
            } else
            {
                tank->getTarget()->addGlobalForce(Vector(0,JUMP_FORCE,0));
            }
        }
    }
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::addPlayer(const Player * player, bool newly_connected)
{
    score_.addPlayer(player);

    if (newly_connected)
    {
        // transmit the current scores and team assignments to a newly
        // connected player
        PuppetMasterServer::PlayerContainer::const_iterator it;
        PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers();
    
        for(it = pc.begin(); it != pc.end(); it++)
        {
            if (it->getId() != player->getId())
            {
                sendTeamAssignment(it->getId(), player->getId(), CST_SINGLE);

                // transmit upgrade status of other players
                for(uint8_t cat = 0; cat < NUM_UPGRADE_CATEGORIES; cat++)
                {
                    sendExecuteUpgrade(it->getId(), (UPGRADE_CATEGORY)cat, player->getId(), CST_SINGLE);
                }
            }

            sendScoreUpdate(it->getId(), player->getId(), CST_SINGLE);

            // Send equipment of others to player
            sendEquipment(it->getId(), player->getId(), true, CST_SINGLE);
        }
    }
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::removePlayer(const SystemAddress & pid)
{
    assert(puppet_master_->getPlayer(pid));
    
    Tank * tank = dynamic_cast<Tank*>(puppet_master_->getPlayer(pid)->getControllable());
    
    if (tank)
    {
        tank->scheduleForDeletion();
    }
    
    score_.removePlayer(pid);
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::loadLevel(const std::string & name)
{
    // early map check
    if(!existsFile((LEVEL_PATH + name + "/objects.xml").c_str()) &&
       !existsFile((LEVEL_PATH + name + "/terrain.hm").c_str()))
    {
        Exception e("Cannot find map ");
        e << name;

        throw e;
    }

    /// XXX fast fix to avoid client message flooding
    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerCommon::decAnnoyingClientRequests),
                        DEC_ANNOYING_CLIENT_REQUEST_PERIOD,
                        "GameLogicServerCommon::decAnnoyingClientRequests",
                        &fp_group_);

    // update players latency constantly
    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerCommon::sendPlayersLatency),
                        1.0 / SEND_PLAYERS_LATENCY_FPS,
                        "GameLogicServerCommon::sendPlayersLatency",
                        &fp_group_);

    sendScoreUpdate(UNASSIGNED_SYSTEM_ADDRESS, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
    
    std::auto_ptr<terrain::TerrainData> td;
#ifdef DEDICATED_SERVER
    td.reset(new terrain::TerrainData);
#else
    td.reset(create_visuals_ ? new terrain::TerrainDataClient : new terrain::TerrainData);
#endif        
    td->load(name);


    bbm::LevelData lvl_data;
    lvl_data.load(name);

#ifndef DEDICATED_SERVER
    if (create_visuals_)
    {
        terrain_visual_->setData((terrain::TerrainDataClient*)td.get());
        terrain_visual_->setTextures(lvl_data.getDetailTexInfo(),
                                     lvl_data.getName());
        RigidBodyVisual::setTerrainData((terrain::TerrainDataClient*)td.get());
    }
#endif
    Tank::setTerrainData(td.get());

    // pass ownership to gamestate
    puppet_master_->getGameState()->setTerrainData(td, CCCS_HEIGHTFIELD);


        
    for (std::vector<bbm::ObjectInfo>::const_iterator cur_object_desc = lvl_data.getObjectInfo().begin();
         cur_object_desc != lvl_data.getObjectInfo().end();
         ++cur_object_desc)
    {        
        // Try to get object type from params, if it is not
        // set, assume default "RigidBody"
        std::string type = "RigidBody";
        try
        {
            type = cur_object_desc->params_.get<std::string>("type.type");
        } catch (ParamNotFoundException & e) {}

        if (type == "AmbientSound") continue;

        // Ignore helper objects, such as spawn camera, light dir etc.
        if (type == "HelperObject") continue;


        std::auto_ptr<RigidBody> body;
        std::vector<std::string> part_names = getObjectPartNames(cur_object_desc->name_);
        if (part_names.empty())
        {
            s_log << Log::error
                  << "Cannot load "
                  << cur_object_desc->name_
                  << "\n";
            continue;
        }
        for (std::vector<std::string>::iterator it = part_names.begin();
             it != part_names.end();
             ++it)
        {
            body.reset(createRigidBody(*it, type));
            body->setTransform(cur_object_desc->transform_);            

            if (onLevelObjectLoaded(body.get(), cur_object_desc->params_)) continue;


            // for debugging, let controllables be placeable by level file
            Tank * t = dynamic_cast<Tank*>(body.get());
            if (t)
            {
                t->setLocation(CL_SERVER_SIDE);
                PlayerInput in;
                in.fire1_ = IKS_DOWN;
                in.fire2_ = IKS_DOWN;
                // t->setPlayerInput(in);
            }
        

            // Search static bodies for interesting stuff
            if (body->isStatic())
            {
                if (body->getName() == VOID_GEOM_NAME)
                {
                    for (unsigned g=0; g<body->getTarget()->getGeoms().size(); ++g)
                    {
                        physics::OdeGeom * cur_geom = body->getTarget()->getGeoms()[g];                        
                        cur_geom->setCollisionCallback(
                            physics::CollisionCallback(this, &GameLogicServerCommon::voidCollisionCallback));
                    }
                }
            } 

            puppet_master_->addGameObject(body.release(), true);
        }
    } // for object infos


    // Recreate player tanks
    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        puppet_master_->requestPlayerReady(it->getId());
    }



    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerCommon::handleLogic),
                        1.0f / s_params.get<float>("server.logic.logic_fps"),
                        "GameLogicServerCommon::handleLogic",
                        &fp_group_);    

    
    state_ = TGS_BEFORE_MATCH_START;

    createMatchEventlog();
    match_events_->setMapName(name);

    /// XXXX move this to PuppetMasterServer
    s_waypoint_manager_server.loadWaypoints(name);
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::onGameObjectAdded(GameObject * object)
{
    RigidBody * rigid_body = dynamic_cast<RigidBody*>(object);
    if(!rigid_body) return;

    if (!rigid_body->isStatic())
    {
        rigid_body->setCollisionCategory(CCCS_DYNAMIC, false);
    } else
    {
        rigid_body->setCollisionCategory(CCCS_STATIC, false);
    }


    // Per default, put sensors into sensor_space_.
    for (std::vector<physics::OdeGeom*>::iterator cur_geom = rigid_body->getTarget()->getGeoms().begin();
         cur_geom != rigid_body->getTarget()->getGeoms().end();
         ++cur_geom)
    {
        if ((*cur_geom)->isSensor())
        {
            (*cur_geom)->setSpace(sensor_space_.get());
        }
    }

    if (object->getType() == "Tank")
    {
        Tank * tank = (Tank*)rigid_body;
        tank->setWheelCategory(CCCS_WHEEL_RAY);        

        rigid_body->getTarget()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicServerCommon::tankBodyCollisionCallback),
            TANK_BODY_NAME);
    }
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::executeCustomCommand(unsigned type,
                                                    RakNet::BitStream & args)
{
    switch (type)
    {
    case CCCT_REQUEST_TEAM_CHANGE:
        onTeamChangeRequested(args);
        break;
    case CCCT_REQUEST_UPGRADE:
        onUpgradeRequested(args);
        break;
    case CCCT_REQUEST_RESPAWN:
        onRespawnRequested(args);
        break;
    case CCCT_TEAM_MESSAGE:
        onTeamMessage(args);
        break;
    case CCCT_REQUEST_EQUIPMENT_CHANGE:
        onEquipmentChangeRequested(args);
        break;
    default:
        s_log << Log::warning
              << "Unknown game logic command type: "
              << type
              << "\n";
    }
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::createMatchEventlog()
{
    match_events_.reset(new network::ranking::MatchEvents());
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::onMatchStatsReceived(const network::ranking::Statistics & stats)
{
    sendMatchStats(stats);
}


//------------------------------------------------------------------------------
bool GameLogicServerCommon::onLevelObjectLoaded(RigidBody * obj, const LocalParameters & params)
{
    return extractSpawnPos(obj, params) != NULL;
}


//------------------------------------------------------------------------------
/**
 *  Deals damage for a hit, applies impact impulse to dynamic
 *  objects. Accounts for shot projectiles and hit objects(accuracy
 *  calculation).
 *
 *  Shots are incremented on per-type basis because there are objects
 *  which are neutral w.r.t. accuracy (such as beacons).
 *
 *  \param hit_object The object to deal damage to.
 *
 *  \param hit_percentage 1.0 for a direct hit, lower values indicate
 *  a splash hit.
 *
 *  \param info The collision information for the primary, direct hit.
 *
 *  \param create_feedback False after the water plane has been hit,
 *  to avoid explosion below water.
 */
void GameLogicServerCommon::onProjectileHit(Projectile * projectile,
                                            RigidBody * hit_object,
                                            float hit_percentage,
                                            const physics::CollisionInfo & info,
                                            bool create_feedback)
{
    const SystemAddress & proj_owner = projectile->getOwner();
    PlayerScore * player_score = score_.getPlayerScore(proj_owner);

    if (hit_object && hit_object->getType() == "Tank")
    {
        // Soccer cannon does not do splash damage on tank...
        if (projectile->getSection() == "soccer_cannon" && hit_percentage != 1.0f)
        {
            return;
        }
        
        Tank * tank = (Tank*)hit_object;

        // if tank is hit by projectile -> resolve damage, score, death ....
        //
        // we can hit ourselves with splash damage, damage to self is
        // always afflicted by UNASSIGNED_SYSTEM_ADDRESS
        SystemAddress damage_dealer = proj_owner == tank->getOwner() ? UNASSIGNED_SYSTEM_ADDRESS : proj_owner;


        // Contact point doesn't give useable results for impact
        // location when tank is hit directly -> use projectile
        // velocity to estimate where the hit occured.
        Vector projectile_pos;
        if (hit_percentage == 1.0f)
        {
            projectile_pos = tank->getPosition() - projectile->getGlobalLinearVel();
        } else
        {
            projectile_pos = info.pos_;
        }

        tank->dealWeaponDamage((int)(projectile->getDamage()*hit_percentage),
                               damage_dealer,
                               HitpointTracker::calcHitLocation(tank->getTransform(), projectile_pos));

        if (tank->getHitpoints() == 0)
        {
            PLAYER_KILL_TYPE kill_type = PKT_UNKNOWN1;
            if(projectile->getSection() == "splash_cannon")         kill_type = PKT_WEAPON1;
            if(projectile->getSection() == "armor_piercing_cannon") kill_type = PKT_WEAPON2;
            if(projectile->getSection() == "heavy_impact_cannon")   kill_type = PKT_WEAPON3;
            if(projectile->getSection() == "soccer_cannon")         kill_type = PKT_WEAPON4;
            if(projectile->getSection() == "missile")               kill_type = PKT_MISSILE;

            kill(kill_type, tank, proj_owner);
        }

        WEAPON_HIT_TYPE type;        
        if (hit_percentage == 1.0f)
        {
            if(projectile->getSection() == "splash_cannon") type = WHT_PROJECTILE_SPLASH;            
            else type = WHT_PROJECTILE;
        } else type = WHT_PROJECTILE_SPLASH_INDIRECT;


        sendWeaponHit(proj_owner, tank->getOwner(), info, type, OHT_TANK);
        
    } else if (hit_object && hit_object->getType() == "Water") // handle water hit different for particle effect
    {
        // only handle direct hits, otherwise splash damage triggers this too
        if(hit_percentage == 1.0f)
        {
            // just create splash feedback on client, no other effect
            // "real" hit is still to be handled, but without feedback
            // on client side (except when a tank is hit)
            sendWeaponHit(proj_owner, UNASSIGNED_SYSTEM_ADDRESS,
                          info, WHT_PROJECTILE, OHT_WATER);        
        }
    } else // hit something entirely else...
    {
        // only handle direct hits, otherwise splash damage triggers this too
        if(hit_percentage == 1.0f)
        {
            // increment shots if nothing meaningful is hit and send score
            // for accuracy calculation.
            if (player_score)
            {
                player_score->shots_++;
            }
            if (proj_owner != UNASSIGNED_SYSTEM_ADDRESS) sendScoreUpdate(proj_owner, proj_owner, CST_SINGLE);

            if (create_feedback)
            {
                sendWeaponHit(proj_owner, UNASSIGNED_SYSTEM_ADDRESS,
                              info, (projectile->getSection() == "splash_cannon" ? WHT_PROJECTILE_SPLASH :
                              WHT_PROJECTILE), OHT_OTHER);
            }
        }

        if (hit_object) hit_object->dealActivation((unsigned)projectile->getDamage(), ACTIVATE_APPENDIX_WEAPON_HIT);
    }

    // Reduce soccer cannon impact on tank
    float impact_factor = 1.0f;
    if (hit_object &&
        hit_object->getType()    == "Tank" &&
        projectile->getSection() == "soccer_cannon")
    {
        impact_factor = 0.1f;
    }
    
    projectile->applyImpactImpulse(hit_object,
                                   hit_percentage,
                                   info.pos_,
                                   info.n_,
                                   impact_factor);
}



//------------------------------------------------------------------------------
void GameLogicServerCommon::onInstantWeaponHit(InstantHitWeapon * gun,
                                               RigidBody * hit_object)
{
    if (hit_object->getType() == "Tank")
    {
        // tank is hit by projectile -> resolve damage, score, death ....
        Tank * tank = (Tank*)hit_object;
        Tank * other_tank = gun->getTank();
        assert(other_tank);
        
        tank->dealWeaponDamage((int)gun->getDamage(), gun->getTank()->getOwner(),
                               HitpointTracker::calcHitLocation(tank->getTransform(), other_tank->getPosition()));
    
        if (tank->getHitpoints() == 0)
        {
            PLAYER_KILL_TYPE pkt = PKT_UNKNOWN1;
            if(gun->getSection() == "mg")
            {
                pkt = PKT_MACHINE_GUN;
            } else if(gun->getSection() == "flamethrower")
            {
                pkt = PKT_FLAME_THROWER;
            } else if(gun->getSection() == "laser")
            {
                pkt = PKT_LASER;
            }

            kill(pkt ,tank, gun->getTank()->getOwner());
        }
    } else if (hit_object->getType() == "TankMine")
    {
        ((TankMine*)hit_object)->explode(UNASSIGNED_SYSTEM_ADDRESS, WHT_MINE, OHT_OTHER);
    }    
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::onPlayerTeamChanged(ServerPlayer * player)
{
    Team * team = score_.getTeam(player->getId());
    match_events_->logTeamChange(player->getRankingId(), team ? team->getId() : INVALID_TEAM_ID);
}


//------------------------------------------------------------------------------
/**
 *  Utility function to create a rigid body with the current
 *  parameters (simulator, create_visuals_).
 */
RigidBody * GameLogicServerCommon::createRigidBody(const std::string & desc_name,
                                                   const std::string & type)
{
    return RigidBody::create(desc_name,
                             type,
                             puppet_master_->getGameState()->getSimulator(),
                             create_visuals_);
}


//------------------------------------------------------------------------------
const Score & GameLogicServerCommon::getScore() const
{
    return score_;
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::recreateAIPlayers()
{
    // recreate AI players for this type of game logic
    // if not supported, remove them
    unsigned num_ai_players = 0;
    std::vector<std::string> args;
    PuppetMasterServer::PlayerContainer::iterator it;
    PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers(); 

    for(it = pc.begin(); it != pc.end(); it++)
    {
        if((*it).getAIPlayer())
        {
           num_ai_players++;
        }
    }
    
    for(unsigned i=0; i < num_ai_players; i++)
    {
        s_log << removeBot(args) << "\n";
    }   

    for(unsigned i=0; i < num_ai_players; i++)
    {
        s_log << addBot(args) << "\n";
    }
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::handleLogic(float dt)
{
    doHandleLogic(dt);
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::doHandleLogic(float dt)
{
    sensor_space_->collide();

    PuppetMasterServer::PlayerContainer::iterator it;
    PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers();
	unsigned num_ai_players = 0;

    // iterate through players and handle AIs
    for(it = pc.begin(); it != pc.end(); it++)
    {              
        if((*it).getAIPlayer()) 
        {
			num_ai_players++;
            (*it).getAIPlayer()->handleLogic(dt);
        }
    }

	

	// handle add/remove of AI players according to bot_limit set
	std::vector<std::string> args;
	unsigned bot_limit = s_params.get<unsigned>("server.settings.bot_limit");
	
	if(bot_limit == num_ai_players) return; // bail if equal

	if(bot_limit > num_ai_players)
	{
		addBot(args);
		(*s_params.getPointer<unsigned>("server.settings.bot_limit"))--;
	}

	if(bot_limit < num_ai_players)
	{
		removeBot(args);
		(*s_params.getPointer<unsigned>("server.settings.bot_limit"))++;
	}

}


//------------------------------------------------------------------------------
/**
 *  Do anything neccessary when match truly starts (start timelimit,
 *  reset players to their starting positions, clear score etc.
 */
void GameLogicServerCommon::onMatchStart()
{
    startTimeLimit(s_params.get<float>("server.settings.time_limit"));


    // reset all players
    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        Tank * tank = dynamic_cast<Tank*>(it->getControllable());
        if (!tank) continue;

        tank->scheduleForDeletion();
        puppet_master_->setControllable(it->getId(), NULL);

        respawn(it->getId(), 0);
    }

    sendStatusMessage(MSG_GAME_COMMENCING,
                      Color(1.0f,1.0f,1.0f),
                      UNASSIGNED_SYSTEM_ADDRESS, 
                      CST_BROADCAST_READY);
    
    match_events_->logMatchStart();
    state_ = TGS_IN_PROGRESS;
}








//------------------------------------------------------------------------------
bool GameLogicServerCommon::voidCollisionCallback(const physics::CollisionInfo & info)
{
    // Just do collision if void plane is not a sensor
    bool ret = !info.this_geom_->isSensor();
    
    if (info.type_ != physics::CT_START) return ret;

    RigidBody * other = (RigidBody*)info.other_geom_->getUserData();

    if (other->isScheduledForDeletion()) return ret;

    s_log << Log::debug('l')
          << *other
          << " collided with void\n";
    
    if (other->getType() == "Tank")
    {
        kill(PKT_SUICIDE, (Controllable*)other);
    } else if(!other->isStatic()) // delete all dynamic objects colliding with void
    {
        s_log << Log::debug('l')
              << *other
              << " scheduled for deletion because of void collision.\n";
        other->scheduleForDeletion();
    }

    return ret;
}



//------------------------------------------------------------------------------
bool GameLogicServerCommon::tankBodyCollisionCallback(const physics::CollisionInfo & info)
{
    if (info.type_ == physics::CT_STOP) return false;
    if (info.other_geom_->getCategory() == CCCS_PROJECTILE) return false;


    Tank * tank = (Tank*)info.this_geom_->getUserData();
    RigidBody * other_body = (RigidBody*)info.other_geom_->getUserData();

    // we don't want collision damage from mines....
    if (other_body && other_body->getType() == "TankMine") return false;
    
    Tank * other_tank = NULL;
    if (other_body && other_body->getType() == "Tank")
    {
        other_tank = (Tank*)other_body;

        // Tank <-> Tank collision: we want to handle this only once,
        // so we are able to correctly attribute 2 kills if both tanks
        // get destroyed in the collision. Handling this separately
        // will result in one kill and one suicide.
        if (tank < other_tank) return true;
    }    
    
    Vector v_rel = tank->getGlobalLinearVel();
    if (!info.other_geom_->isStatic()) v_rel -= info.other_geom_->getBody()->getGlobalLinearVel();
    float collision_velocity = abs(vecDot(&v_rel, &info.n_));
    
    if (equalsZero(collision_velocity)) return false;
    
    v_rel.normalize();

    bool ram_upgrade_active = isRamUpgradeActive(tank, other_body,  v_rel);

    if (other_tank)
    {
        bool other_ram_upgrade_active = isRamUpgradeActive(other_tank, tank,  -v_rel);
        // Called only once per collision, so must act symmetrically
        dealTankTankCollisionDamage(tank, other_tank, collision_velocity,
                                    ram_upgrade_active, other_ram_upgrade_active);
        dealTankTankCollisionDamage(other_tank, tank, collision_velocity,
                                    other_ram_upgrade_active, ram_upgrade_active);

        // kill removes the tank's owner, so remember this here...
        SystemAddress owner       = tank      ->getOwner();
        SystemAddress other_owner = other_tank->getOwner();
        
        if (tank->getHitpoints() <= 0)
        {
            kill(PKT_COLL_DAMAGE, tank, other_owner);
        }
        if (other_tank->getHitpoints() <= 0)
        {
            kill(PKT_COLL_DAMAGE, other_tank, owner);
        }
        
    } else if (!ram_upgrade_active)
    {
        SystemAddress dealer_id = other_body ? other_body->getOwner() : UNASSIGNED_SYSTEM_ADDRESS;   
        tank->dealCollisionDamage(collision_velocity,
                                  tank->getMaxSpeed(),
                                  dealer_id);

        if (other_body)
        {
            // XXXXXX hack: collision damage calculation from tank is used!!
            other_body->dealActivation( (unsigned)tank->calcCollisionDamage(2.0f*collision_velocity, tank->getMaxSpeed()),
                                        ACTIVATE_APPENDIX_COLLISION);
        }
        
        if (tank->getHitpoints() <= 0)
        {
            kill(PKT_COLL_DAMAGE, tank, dealer_id);
        }
    }

    return true;
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::startTimeLimit(float limit)
{
    score_.setTimeLeft(limit);
    
    if (task_update_time_limit_ != INVALID_TASK_HANDLE)
    {
        s_log << Log::warning
              << "GameLogicServerCommon::startTimeLimit: time already was running\n";
        s_scheduler.removeTask(task_update_time_limit_, &fp_group_);
    }
    
    task_update_time_limit_ =
        s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerCommon::updateTimeLimit),
                            0.1f,
                            "GameLogicServerCommon::updateTimeLimit",
                            &fp_group_);

    // Broadcast starting time (currently in score)
    sendScoreUpdate(UNASSIGNED_SYSTEM_ADDRESS, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::updateTimeLimit(float dt)
{
    float time_left = score_.getTimeLeft();

    if (time_left <= 0.0f)
    {
        s_scheduler.removeTask(task_update_time_limit_, &fp_group_);
        task_update_time_limit_ = INVALID_TASK_HANDLE;

        if (state_ == TGS_IN_PROGRESS) onTimeLimitExpired();
    }
    
    time_left -= dt;
    score_.setTimeLeft(time_left);
}



//------------------------------------------------------------------------------
/**
 *  XXX fast fix to avoid client message flooding
 */
void GameLogicServerCommon::decAnnoyingClientRequests(float dt)
{
    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        it->decAnnoyingClientRequest();
    }
}

//------------------------------------------------------------------------------
/**
 *  Constantly transmit players latency to all clients
 */
void GameLogicServerCommon::sendPlayersLatency(float dt)
{
    RakNet::BitStream args;

    unsigned num_players = puppet_master_->getPlayers().size();
    args.Write(num_players);
    

    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        args.Write(it->getId());
        args.Write(it->getNetworkDelay());

    }

    network::CustomServerCmd latency_cmd(CSCT_PLAYERS_LATENCY_UPDATE, args);
    puppet_master_->sendNetworkCommand(latency_cmd, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);   

}

//------------------------------------------------------------------------------
/**
 *  Sets the controllable for the killed player to NULL and schedules
 *  the respawn event. Creates a beacon for the opposing team.
 *
 *  \param kill_type How the player has been killed, weapon, mine, coll damage...
 *  \param controllable The controllable of the player to kill.
 *  \param Who did it? If this UNASSIGNED_SYSTEM_ADDRESS, a suicide is
 *  assumed.
 */
void GameLogicServerCommon::kill(PLAYER_KILL_TYPE kill_type,
                                 Controllable * controllable, 
                                 SystemAddress killer_id)
{
    assert(controllable);

    SystemAddress killed_id = controllable->getOwner();

    // If tank is ownerless, bail out
    if (killed_id == UNASSIGNED_SYSTEM_ADDRESS) return;

    s_log << Log::debug('l')
          << killer_id
          << " killed "
          << killed_id
          << "\n";


    Tank * tank = dynamic_cast<Tank*>(controllable);
    assert(tank);
    assert(!tank->isScheduledForDeletion());


    // suicide is always signaled by killer
    // "UNASSIGNED_SYSTEM_ADDRESS" to avoid two separate categories in
    // kill assist tracker.
    if (killer_id == killed_id) killer_id = UNASSIGNED_SYSTEM_ADDRESS;

    tank->setHitpoints(0); // triggers smoke
    tank->setInvincible(false); // stop flashing
    tank->setHasRamUpgrade(false);

    sendKill(killer_id, killed_id, kill_type, false);

    s_log << Log::debug('l')
          << "Scheduling tank respawn for "
          << killed_id
          << "\n";

    assert(respawning_player_.find(killed_id) == respawning_player_.end());
    respawning_player_[killed_id] = -1;
    s_scheduler.addEvent(SingleEventCallback(this, &GameLogicServerCommon::allowRespawn),
                         s_params.get<float>("server.logic.spawn_delay"),
                         new SystemAddress(killed_id),
                         "GameLogicServerCommon::allowRespawn",
                         &fp_group_);


    onTankKilled(tank, killer_id);
    
    puppet_master_->setControllable(killed_id, NULL);
    

    std::string app = "d";
    puppet_master_->onRigidBodyActivated(tank, &app, RBE_ACTIVATED);
}




//------------------------------------------------------------------------------
void GameLogicServerCommon::executeUpgrade(Controllable * controllable)
{ 
    if(!controllable) return;

    PlayerScore * score = score_.getPlayerScore(controllable->getOwner());

    for(uint8_t cat = 0; cat < NUM_UPGRADE_CATEGORIES; cat++)
    {
        for(uint8_t lvl = 1; lvl <= score->active_upgrades_[cat]; lvl++)
        {
            executeUpgrade(controllable, (UPGRADE_CATEGORY)cat, lvl);
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Load appropriate parameter super sections into tank
 */
void GameLogicServerCommon::executeUpgrade(Controllable * controllable, UPGRADE_CATEGORY category, uint8_t level)
{     
    Tank * tank = dynamic_cast<Tank*>(controllable);
    
    if(!tank) return;

    std::string file = CONFIG_PATH + "upgrades.xml";

    tank->loadParameters(file, UPGRADES[category] + toString(level));        
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::executeEquipmentChange(Controllable * controllable)
{ 
    Tank * tank = dynamic_cast<Tank*>(controllable);
    
    if(!tank) return;

    std::string file = CONFIG_PATH + "equipment.xml";

    PlayerScore * score = score_.getPlayerScore(tank->getOwner());

    for(unsigned slot = 0; slot < NUM_EQUIPMENT_SLOTS; slot++)
    {   
        std::string super_section = EQUIPMENT_SLOTS[slot] + toString(score->active_equipment_[slot]);   
        tank->loadParameters(file, super_section);    
    }
}

//------------------------------------------------------------------------------
Matrix GameLogicServerCommon::getRandomSpawnPos(const std::vector<SpawnPos*> & possible_spawn_positions) const
{
    if (possible_spawn_positions.empty())
    {
        s_log << Log::warning
              << "No valid spawning pos in GameLogicServerCommon::getRandomSpawnPos. Respawning in midair.\n";
        
        Matrix ret(true);
        ret.getTranslation().y_ = 20.0;
        return ret;
    }

    
    std::vector<SpawnPos*> free_pos;

    // pick possible starting positions
    for (unsigned c=0; c<possible_spawn_positions.size(); ++c)
    {
        if(!possible_spawn_positions[c]->isOccupied())
        {
            free_pos.push_back(possible_spawn_positions[c]);
        }
    }

    Matrix spawn_pos;
    if(free_pos.empty())
    {
#ifdef ENABLE_DEV_FEATURES
        s_log << "Found no free starting position.\n";
#endif
        // No free spawn point, pick any one
        spawn_pos = possible_spawn_positions[rand() % possible_spawn_positions.size()]->getTransform();
        spawn_pos.getTranslation().y_ += MIDAIR_SPAWN_HEIGHT;
    } else
    {
        spawn_pos = free_pos[rand() % free_pos.size()]->getTransform();
    }
    
    return spawn_pos;    
}


//------------------------------------------------------------------------------
/**
 *  Removes the given player from respawning_player_. Immediately
 *  respawns the player if he already requested a respawn.
 *
 *  \param player_id A pointer to a new'ed SystemAddress. Will be deleted
 *  in this function.
 */
void GameLogicServerCommon::allowRespawn(void * player_id)
{
    SystemAddress pid = *((SystemAddress*)player_id);
    delete (SystemAddress*)player_id;

    // Remove player from "respawning" list. It can be missing if
    // respawn was called directly.
    std::map<SystemAddress, int>::iterator it = respawning_player_.find(pid);
    if (it != respawning_player_.end()) 
    {
        if (it->second != -1 && score_.getTeam(pid)) respawn(pid, it->second);
        respawning_player_.erase(it);
    }
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::respawn(const SystemAddress & pid, unsigned stage)
{
    if (state_ == TGS_WON) return;
    
    // Test whether player is still connected
    if (!puppet_master_->existsPlayer(pid)) return;

    ServerPlayer * player = puppet_master_->getPlayer(pid);
    assert(player);

    if (player->getControllable() != NULL)
    {
        s_log << Log::warning
              << "Ignoring respawn because controllable is already set for player "
              << pid
              << "\n";
        return;
    }

    Matrix spawn_pos;
    try
    {
        spawn_pos = getRespawnPos(pid, stage);
    } catch (Exception & e)
    {
        s_log << Log::warning
              << "Aborting spawn: "
              << e
              << "\n";
        return;
    }

    s_log << Log::debug('l')
          << "Respawning "
          << pid
          << " at stage "
          << (unsigned)stage
          << "\n";

    Tank * new_tank = createNewPlayerTank(pid);

    if (!new_tank)
    {
        s_log << Log::error
              << "Could not get new player tank. Aborting spawn.\n";
        return;
    }
    
    new_tank->setTransform(spawn_pos);   
    new_tank->setLocation(CL_SERVER_SIDE);
    
    puppet_master_->addGameObject(new_tank, true);
    puppet_master_->setControllable(pid, new_tank);

    executeEquipmentChange(new_tank);
    executeUpgrade(new_tank);

    // This sets the spawning pos to occupied, avoids tanks spawning
    // at same time to spawn at same positon (e.g. at level start)
    sensor_space_->collide();

    // schedule the end of invincibility.
    s_scheduler.addEvent(SingleEventCallback(new_tank, &Tank::setInvincible),
                         s_params.get<float>("server.logic.invincibility_duration"),
                         (void*)false,
                         "Tank::setInvincible(false)",
                         &new_tank->getFpGroup());
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::gameWon()
{
    assert(state_ != TGS_BEFORE_MATCH_START);
    
    if (state_ == TGS_WON) return;

    fp_group_.deregisterAllOfType(TaskFp());
    task_update_time_limit_ = INVALID_TASK_HANDLE;
    
    respawning_player_.clear();
    
    state_ = TGS_WON;

    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        SystemAddress pid = it->getId();

        puppet_master_->setControllable(pid, NULL);
    }

    match_events_->logMatchEnd();    
    puppet_master_->onGameFinished();
}




//------------------------------------------------------------------------------
void GameLogicServerCommon::handleAssist(const SystemAddress & assistant,
                                         const SystemAddress & killed,
                                         float percentage,
                                         unsigned score_points,
                                         unsigned upgrade_points,
                                         bool respect_teams)
{
    if (percentage < s_params.get<float>("server.logic.damage_percentage_assist_threshold")) return;
        
    PlayerScore * assist_killer_score = score_.getPlayerScore(assistant);
    PlayerScore * killed_score        = score_.getPlayerScore(killed);
    // assistant killing player doesn't neccessarily exist on server anymore...
    if (!assist_killer_score) return;

    // Team damage done - hmm do nothing
    if (respect_teams &&
        killed_score &&
        assist_killer_score->getTeam() == killed_score->getTeam()) return;

    // Ordinary kill assist - add to score
    assist_killer_score->score_          += score_points;
    assist_killer_score->upgrade_points_ += upgrade_points;

    // Send kill damage assist message
    sendKill(assistant, killed, PKT_UNKNOWN1, true); // kill type has no meaning on assist
    sendScoreUpdate(assistant, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
}




//------------------------------------------------------------------------------
void GameLogicServerCommon::sendWeaponHit(const SystemAddress & shooter,
                                          const SystemAddress & player_hit,
                                          const physics::CollisionInfo & info, 
                                          WEAPON_HIT_TYPE type,
                                          OBJECT_HIT_TYPE object_hit)
{
    RakNet::BitStream args;
    args.Write(shooter);
    args.Write(player_hit);
    args.Write(info.pos_);
    args.Write(info.n_);
    args.Write((uint8_t)type);
    args.Write((uint8_t)object_hit);
    
    network::CustomServerCmd hit_cmd(CSCT_WEAPON_HIT, args);
    puppet_master_->sendNetworkCommand(hit_cmd, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
}


//------------------------------------------------------------------------------
/**
 *  Sends the scores of a specific player or teamscores only to a
 *  given target.
 *
 *  \param player_id The player the score of which is going to be
 *  transferred. Team Score gets transmitted in any case.
 *
 *  \param target_id The player to send the score to.
 *
 *  \param type Which players to send the packet to.
 */
void GameLogicServerCommon::sendScoreUpdate(const SystemAddress & player_id,
                                            const SystemAddress & target_id,
                                            COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    args.Write(player_id);

    if (player_id != UNASSIGNED_SYSTEM_ADDRESS &&
        puppet_master_->getPlayer(player_id)) // Bail if player doesn't exist anymore
                                              // (e.g. quit game while projectile was in transit...)

    {
        assert(score_.getPlayerScore(player_id));
        
        PlayerScore * score = score_.getPlayerScore(player_id);

        args.Write(score->score_);
        args.Write(score->kills_);
        args.Write(score->deaths_);
        args.Write(score->goals_);
        args.Write(score->assists_);
        args.Write(score->shots_);
        args.Write(score->hits_);
        args.Write(score->upgrade_points_);        
    }

    // transfer team score
    for(unsigned c=0; c < score_.getNumTeams(); c++)
    {
        args.Write(score_.getTeamScore((TEAM_ID)c)->score_);
    }    
    
    // remaining time
    args.Write(score_.getTimeLeft());
    
    network::CustomServerCmd score_cmd(CSCT_SCORE_UPDATE, args);
    puppet_master_->sendNetworkCommand(score_cmd, target_id, type);
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::sendTeamAssignment(const SystemAddress & player_id,
                                               const SystemAddress & target_id,
                                               COMMAND_SEND_TYPE type)
{
    TEAM_ID team_id = score_.getTeamId(player_id);
    
    RakNet::BitStream args;
    args.Write(player_id);
    args.Write(team_id);
    
    network::CustomServerCmd team_cmd(CSCT_TEAM_ASSIGNMENT, args);
    puppet_master_->sendNetworkCommand(team_cmd, target_id, type);
}





//------------------------------------------------------------------------------
void GameLogicServerCommon::sendKill(const SystemAddress & killer_id, 
                                     const SystemAddress & killed_id, 
                                     PLAYER_KILL_TYPE kill_type,
                                     bool assist)
{
    RakNet::BitStream args;
    args.Write(killer_id);
    args.Write(killed_id);
    args.Write(kill_type);
    args.Write(assist);

    network::CustomServerCmd kill_cmd(CSCT_KILL, args);

    if(assist)
    {
        puppet_master_->sendNetworkCommand(kill_cmd, killer_id, CST_SINGLE);
    }else
    {
        puppet_master_->sendNetworkCommand(kill_cmd, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
    }    
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::sendExecuteUpgrade(const SystemAddress & upgrading_player_id,
                                                  UPGRADE_CATEGORY category, 
                                                  const SystemAddress & target_id,
                                                  COMMAND_SEND_TYPE type)
{
    PlayerScore * player_score = score_.getPlayerScore(upgrading_player_id);
    assert(player_score);

    uint8_t level = player_score->active_upgrades_[category];

    if(level == 0) return; // do not send "no upgrade" cmd

    RakNet::BitStream args;
    args.Write(upgrading_player_id);
    args.Write(player_score->upgrade_points_);
    args.Write((uint8_t)category);
    args.Write(level);

    network::CustomServerCmd upgrade_cmd(CSCT_EXECUTE_UPGRADE, args);
    puppet_master_->sendNetworkCommand(upgrade_cmd, target_id, type);
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::sendEquipment (const SystemAddress & player_id,
                                           const SystemAddress & target_id,
                                           bool execute,
                                           COMMAND_SEND_TYPE type)
{
    assert(player_id != UNASSIGNED_SYSTEM_ADDRESS);
    assert(puppet_master_->getPlayer(player_id));
    assert(score_.getPlayerScore(player_id));
    
    RakNet::BitStream args;
    args.Write(player_id);
    args.Write(execute);
    
    PlayerScore * score = score_.getPlayerScore(player_id);
    for(unsigned s=0; s < NUM_EQUIPMENT_SLOTS; s++)
    {
        args.Write(score->active_equipment_[s]);
    }
    
    network::CustomServerCmd score_cmd(CSCT_EQUIPMENT_UPDATE, args);
    puppet_master_->sendNetworkCommand(score_cmd, target_id, type);    
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::sendStatusMessage(const std::string & message,
                                              const Color & color,
                                              const SystemAddress & target_id,
                                              COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    network::writeToBitstream(args, message);

    args.Write(color.r_);
    args.Write(color.g_);
    args.Write(color.b_);

    network::CustomServerCmd msg_cmd(CSCT_STATUS_MESSAGE, args);
    puppet_master_->sendNetworkCommand(msg_cmd, target_id, type); 
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::sendMatchStats(const network::ranking::Statistics & stats)
{    
    RakNet::BitStream args;
    stats.writeToBitstream(args);

    // transmit the mapping ranking id -> player system address so
    // client can use the data...
    args.Write((uint32_t)puppet_master_->getPlayers().size());
    for (PuppetMasterServer::PlayerContainer::const_iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        args.Write((uint32_t)it->getRankingId());
        args.Write(it->getId());
    }
    

    network::CustomServerCmd cmd(CSCT_MATCH_STATISTICS, args);
    puppet_master_->sendNetworkCommand(cmd, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY); 
}


//------------------------------------------------------------------------------
void GameLogicServerCommon::onTeamChangeRequested(RakNet::BitStream & args)
{
    SystemAddress pid;
    TEAM_ID team_id;

    args.Read(pid);
    args.Read(team_id);

    ServerPlayer * player = puppet_master_->getPlayer(pid);

    if (player == NULL)
    {
        s_log << Log::warning
              << "requestTeamChange received for nonexisting player "
              << pid
              << "\n";
        return;
    }
    assert(score_.getPlayerScore(pid));

    if (team_id >= score_.getNumTeams() && team_id != INVALID_TEAM_ID)
    {
        s_log << Log::warning
              << "requestTeamChange received for nonexisting team "
              << team_id
              << "\n";
        return;
    }

    // cancel any spawn request, regardless of whether a different
    // team was actually chosen.
    if (respawning_player_.find(pid) != respawning_player_.end()) respawning_player_[pid] = -1;
    
    if (score_.getTeamId(pid) == team_id)
    {
        s_log << Log::debug('l')
              << "Ignoring change to same team for "
              << pid
              << "\n";
        return;
    }
    
    s_log << Log::debug('l')
          << "requestTeamChange received from "
          << pid
          << " for "
          << (unsigned)team_id
          << "\n";


    // No matter what the new team is, kill the player which schedules
    // a respawn. This has to happen before the team assignment to get
    // the score right.
    if (player->getControllable())
    {
        kill(PKT_SUICIDE, player->getControllable());
    }

    
    score_.assignToTeam(pid, team_id);
    sendTeamAssignment(pid, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);    

    onPlayerTeamChanged(player);
}



//------------------------------------------------------------------------------
void GameLogicServerCommon::onUpgradeRequested(RakNet::BitStream & args)
{
    SystemAddress pid;
    uint8_t c;

    args.Read(pid);
    args.Read(c);

    if(c > NUM_UPGRADE_CATEGORIES) return;
    UPGRADE_CATEGORY category = (UPGRADE_CATEGORY)c;

    PlayerScore * player_score = score_.getPlayerScore(pid);

    if (player_score == NULL)
    {
        s_log << Log::warning
              << "onUpgradeRequested received for nonexisting player: "
              << pid
              << "\n";
        return;
    }

    
    if(player_score->isUpgradePossible(category))
    {
        s_log << "Player "
              << player_score->getPlayer()->getName()
              << " requested "
              << UPGRADES[category]
              << " upgrade and now is at level "
              << player_score->active_upgrades_[category]+1
              << "\n";
        
        player_score->upgrade_points_ -= player_score->getNeededUpgradePoints(category);
        player_score->active_upgrades_[category]++;

        // send to client
        sendExecuteUpgrade(pid, category, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_READY);
        sendScoreUpdate(pid, pid, CST_SINGLE);

        // do the specific upgrade
        executeUpgrade(puppet_master_->getPlayer(pid)->getControllable(),
                       category,
                       player_score->active_upgrades_[category]);

        s_log << Log::debug('l')
              << "Request granted.\n";
    }
}


//------------------------------------------------------------------------------
/**
 *  If player is still blocked for spawning (contained in
 *  respawning_player_), set the request flag, else directly spawn the
 *  player.
 */
void GameLogicServerCommon::onRespawnRequested(RakNet::BitStream & args)
{
    SystemAddress pid;
    uint8_t base_no;

    args.Read(pid);
    args.Read(base_no);

    s_log << Log::debug('l')
          << "respawn requested for stage "
          << (unsigned)base_no
          << "\n";

    if (state_ == TGS_WON) return;
    
    // player must belong to a team, or respawn request is ignored
    if (!score_.getTeam(pid))
    {
        s_log << Log::warning
              << "Player "
              << pid
              << " has no team but requested respawn. Ignoring.\n";
        return;
    }

    // Bail if player already has a controllable.
    if (puppet_master_->getPlayer(pid)->getControllable()) return;
    
    
    // See whether player is allowed to spawn. If not, remember the
    // selected base, else spawn him directly.
    std::map<SystemAddress, int>::iterator it = respawning_player_.find(pid);
    if (it == respawning_player_.end())
    {
        respawn(pid, base_no);
    } else
    {
        it->second = base_no;
    }
}

//------------------------------------------------------------------------------
void GameLogicServerCommon::onEquipmentChangeRequested(RakNet::BitStream & args)
{
    SystemAddress pid;

    args.Read(pid);
    PlayerScore * player_score = score_.getPlayerScore(pid);

    if (player_score == NULL)
    {
        s_log << Log::warning
              << "onEquipmentChange received for nonexisting player: "
              << pid
              << "\n";
        return;
    }


    for(unsigned c=0; c < NUM_EQUIPMENT_SLOTS; c++)
    {
        args.Read(player_score->active_equipment_[c]);
    }

    sendEquipment(pid, UNASSIGNED_SYSTEM_ADDRESS, false, CST_BROADCAST_ALL);
}



//------------------------------------------------------------------------------
void GameLogicServerCommon::onTeamMessage(RakNet::BitStream & args)
{
    SystemAddress pid;
    unsigned message;

    args.Read(pid);
    args.Read(message);

    // test if player is allowed to send this message
    ServerPlayer * player = puppet_master_->getPlayer(pid);
    if(!player->allowAnnoyingClientRequest()) return;


    PlayerScore * calling_player = score_.getPlayerScore(pid);

    if (calling_player == NULL)
    {
        s_log << Log::warning
              << "onTeamMessage received for nonexisting player: "
              << pid
              << "\n";
        return;
    }
    
    std::vector<PlayerScore> players = score_.getPlayers();

    // iterate over all players from same team
    RakNet::BitStream fwd_args;
    fwd_args.Write(pid);
    fwd_args.Write(message);
    network::CustomServerCmd team_cmd(CSCT_TEAM_MESSAGE, fwd_args);
    for(unsigned c=0; c < players.size(); c++)
    {
        if(players[c].getTeam() == calling_player->getTeam())
        {
            puppet_master_->sendNetworkCommand(team_cmd, players[c].getPlayer()->getId(), CST_SINGLE);
        }
    }

    player->incAnnoyingClientRequest();
}


//------------------------------------------------------------------------------
std::string GameLogicServerCommon::dumpSimulatorContents(const std::vector<std::string> & args)
{
    s_log << "Dumping server simulator:\n";
    puppet_master_->getGameState()->getSimulator()->dumpContents();
    
    return "";    
}

//------------------------------------------------------------------------------
std::string GameLogicServerCommon::dumpSpaceContents(const std::vector<std::string> & args)
{
    s_log << "Dumping server collision spaces:\n";
    puppet_master_->getGameState()->getSimulator()->getStaticSpace()->dumpContents();
    puppet_master_->getGameState()->getSimulator()->getActorSpace() ->dumpContents();
    sensor_space_->dumpContents();
    return "";
}


//------------------------------------------------------------------------------
/**
 *  Add upgrade points for testing reasons.
 */
std::string GameLogicServerCommon::addPoints(const std::vector<std::string> & args)
{
    for (PuppetMasterServer::PlayerContainer::const_iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        PlayerScore * score = score_.getPlayerScore(it->getId());
        score->upgrade_points_ += 200;
        sendScoreUpdate(score->getPlayer()->getId(), UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
    }

    return "Points boosted.";
}


//------------------------------------------------------------------------------
std::string GameLogicServerCommon::setTimeLimit(const std::vector<std::string> & args)
{
    if (task_update_time_limit_ == INVALID_TASK_HANDLE) return "time limit not yet started.\n";
    if (args.size() != 1) return "need time in seconds.";

    unsigned new_limit = fromString<unsigned>(args[0]);
    
    s_scheduler.reschedule(task_update_time_limit_, new_limit);
    score_.setTimeLeft(new_limit);

    sendScoreUpdate(UNASSIGNED_SYSTEM_ADDRESS, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);

    return "";
}


//------------------------------------------------------------------------------
std::string GameLogicServerCommon::startMatch(const std::vector<std::string> & args)
{
    onMatchStart();
    return "Match started.";
}

//------------------------------------------------------------------------------
std::string GameLogicServerCommon::addBot(const std::vector<std::string> & args)
{
    // creates logic specific bot type.
    AIPlayer * new_bot = createAIPlayer();    

    if(!new_bot) return "This game type does not support bots.";

    // check how many AI players there are, to increase sv port number to
	// make the system address unique
    unsigned num_ai_players = 0;
	unsigned num_total_players = 0;
    PuppetMasterServer::PlayerContainer::iterator it;
    PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers();

    for(it = pc.begin(); it != pc.end(); it++)
    {
		num_total_players++;
        if((*it).getAIPlayer()) num_ai_players++;
    }

    if (num_total_players >= s_params.get<unsigned>("server.settings.max_connections"))
    {
		return "Server is full.\n";	
	}

    // add AI player to puppet_master, team assignment,...
    new_bot->addPlayer(num_ai_players);

	(*s_params.getPointer<unsigned>("server.settings.bot_limit"))++;
    return "Bot added.";
}

//------------------------------------------------------------------------------
std::string GameLogicServerCommon::removeBot(const std::vector<std::string> & args)
{
    PuppetMasterServer::PlayerContainer::iterator it;
    PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers();
	SystemAddress to_be_removed = UNASSIGNED_SYSTEM_ADDRESS;
	unsigned highest_port = 0;

    // remove AI player with highest port num
    for(it = pc.begin(); it != pc.end(); it++)
    {
        if((*it).getAIPlayer()) 
        {
            if((*it).getId().port > highest_port)
			{
				highest_port = (*it).getId().port;
				to_be_removed = (*it).getId();
			}            
        }
    }

	if(to_be_removed == UNASSIGNED_SYSTEM_ADDRESS)
	{
		return "No bots found for removal";
	}
	else
	{
		puppet_master_->removePlayer(to_be_removed);
		(*s_params.getPointer<unsigned>("server.settings.bot_limit"))--;
		return "Bot removed";
	}

}

//------------------------------------------------------------------------------
void GameLogicServerCommon::dealTankTankCollisionDamage(Tank * dealer,
                                                        Tank * victim,
                                                        float collision_speed,
                                                        bool dealer_ram_active,
                                                        bool victim_ram_active)
{
    if (victim_ram_active && !dealer_ram_active) return;
    
    victim->dealCollisionDamage(collision_speed,
                                victim->getMaxSpeed(),
                                dealer->getOwner(),
                                dealer_ram_active ? s_params.get<float>("server.logic.ram_damage_factor") : 1.0f);
}


//------------------------------------------------------------------------------
bool GameLogicServerCommon::isRamUpgradeActive(const Tank * t1,
                                               const RigidBody * other,
                                               const Vector & v_rel) const
{
    if (!t1->hasRamUpgrade()) return false;

    // t1 must be going forward
    Vector tank_dir = -t1->getTransform().getZ();
    if (vecDot(&v_rel, &tank_dir) < s_params.get<float>("server.logic.cos_ram_damage_angle_threshold"))
    {
        return false;
    }

    if (!other || other->isStatic()) return true;
    
    Vector pd = other->getPosition() - t1->getPosition();
    pd.normalize();
        
    if (vecDot(&pd, &tank_dir) < s_params.get<float>("server.logic.cos_ram_damage_angle_threshold"))
    {
        return false;
    }

    return true;    
}



//------------------------------------------------------------------------------
/**
 *  Checks whether the given object just loaded for a level is a
 *  spawning position. If so, add it to spawn_pos_ and return a
 *  pointer to it, else return NULL.
 */
SpawnPos * GameLogicServerCommon::extractSpawnPos(RigidBody * obj,
                                                  const LocalParameters & params)
{
    std::string role;
    std::string team;

    try
    {
        role = params.get<std::string>("logic.role");
    } catch (ParamNotFoundException & e)
    {
        return NULL;
    }
 
    try
    {
        team = params.get<std::string>("logic.team");
    } catch (ParamNotFoundException & e)
    {
        team = "unkown_team_name";
    }

    if (role == "start_pos")
    {
        physics::OdeGeom * spawn_geom = obj->getTarget()->detachGeom(0);
        if (!spawn_geom)
        {
            s_log << Log::error
                  << *obj
                  << " has no geom.\n";
        } else
        {            
            spawn_geom->setSpace(sensor_space_.get());
            spawn_pos_.push_back(new SpawnPos(spawn_geom, puppet_master_->getGameState()->getTerrainData(), team));
        }
        
        return spawn_pos_.back();
    } else return NULL;
}


