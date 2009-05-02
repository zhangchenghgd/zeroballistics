

#include "GameLogicClientCommon.h"

#include <raknet/RakNetTypes.h>

#include <osg/Depth>

#include "SceneManager.h"


#include "physics/OdeSimulator.h"
#include "physics/OdeRigidBody.h"

#include "ProjectileVisual.h"
#include "Projectile.h"
#include "PlayerInput.h"
#include "Controllable.h"
#include "Tank.h"
#include "NetworkCommand.h"
#include "GameState.h"
#include "GameHudTankZb.h"
#include "Profiler.h"
#include "SoundManager.h"
#include "SoundSource.h"

#include "GUIScore.h"
#include "GUIHelp.h"
#include "GUIUpgradeSystem.h"
#include "GUITankEquipment.h"
#include "GUITeamSelect.h"
#include "GUIMatchSummary.h"

#include "NetworkCommandClient.h"
#include "GameHud.h"
#include "EffectManager.h"
#include "ParameterManager.h"
#include "TankVisual.h"
#include "WeaponSystem.h"
#include "ReaderWriterBbm.h"
#include "InputHandler.h"

#include "TerrainDataClient.h"
#include "LevelData.h"
#include "TerrainVisual.h"
#include "OsgNodeWrapper.h"

#include "Paths.h"

#include "HudMessageColors.h"


#include "RankingStatisticsSoccer.h" // XXXXX


// Don't allow respawn input for this amount of time to prevent
// accidential input.
const float RESPAWN_BLOCK_TIME = 1.0f;

const float MINIMAP_REVEAL_TIME = 1.0f;

//------------------------------------------------------------------------------
GameLogicClientCommon::GameLogicClientCommon() :
    spectator_camera_(this),
    input_mode_(IM_CONTROL_CAMERA),
    respawn_request_sent_(false),
    task_respawn_counter_(INVALID_TASK_HANDLE),
    task_change_to_spawn_selection_(INVALID_TASK_HANDLE),
    respawn_time_(0),
    selected_spawn_stage_(0),
    show_respawn_text_(true),
    respawn_input_blocked_(false),
    game_won_(false)
{
    s_log << Log::debug('i')
          << "GameLogicClientCommon constructor \n";

    
    s_scheduler.addFrameTask(PeriodicTaskCallback(&spectator_camera_, &SpectatorCamera::update),
                        "SpectatorCamera::update",
                        &fp_group_);

    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicClientCommon::updateTimeLimit),
                        0.01f,
                        "GameLogicClientCommon::updateTimeLimit",
                        &fp_group_);

    /// XXX fast fix to avoid client message flooding
    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicClientCommon::decAnnoyingClientRequests),
                        DEC_ANNOYING_CLIENT_REQUEST_PERIOD,
                        "GameLogicClientCommon::decAnnoyingClientRequests",
                        &fp_group_);


 //// XXXX not used in FMS
    s_input_handler.registerInputCallback(
        "Upgrade Weapons", input_handler::SingleInputHandler(this, &GameLogicClientCommon::upgradeWeapon),
        &fp_group_);
    s_input_handler.registerInputCallback(
        "Upgrade Armor", input_handler::SingleInputHandler(this, &GameLogicClientCommon::upgradeArmor),
        &fp_group_);
    s_input_handler.registerInputCallback(
        "Upgrade Speed", input_handler::SingleInputHandler(this, &GameLogicClientCommon::upgradeSpeed),
        &fp_group_);
    s_input_handler.registerInputCallback(
        "Team Message 1", input_handler::SingleInputHandler(this, &GameLogicClientCommon::sendTeamMessage1),
        &fp_group_);
    s_input_handler.registerInputCallback(
        "Team Message 2", input_handler::SingleInputHandler(this, &GameLogicClientCommon::sendTeamMessage2),
        &fp_group_);

#ifdef ENABLE_DEV_FEATURES    
    s_input_handler.registerInputCallback(
        "Toggle camera control mode",
        input_handler::SingleInputHandler(this, &GameLogicClientCommon::toggleInputMode),
        &fp_group_);
#endif

    
    if (s_params.get<unsigned>("client.graphics.shader_quality") > 0)
    {
        terrain_visual_ = new terrain::TerrainVisual();
    }
}


//------------------------------------------------------------------------------
GameLogicClientCommon::~GameLogicClientCommon()
{
    s_log << Log::debug('d')
          << "GameLogicClientCommon destructor \n";

    if(sky_visual_.get())
    {
        sky_visual_->removeFromScene();
        assert(sky_visual_->referenceCount() == 1);
        sky_visual_ = NULL;
    }
    
    if (terrain_visual_.get())
    {
        terrain_visual_->removeFromScene();
        assert(terrain_visual_->referenceCount() == 1);
        terrain_visual_ = NULL;
    }
}


//------------------------------------------------------------------------------
/**
 *  Def facto constructor (because of factory)
 */
void GameLogicClientCommon::init(PuppetMasterClient * master)
{
    GameLogicClient::init(master);

    WeaponSystem::setGameLogicClient(this);    

    createGuiScreens();

    physics::OdeSimulator * sim = puppet_master_->getGameState()->getSimulator();

    sim->enableCategoryCollisions(CCC_PROXY,        CCCC_WHEEL_RAY,  false);

    sim->enableCategoryCollisions(CCC_CONTROLLED,   CCCC_PROJECTILE, false);
    sim->enableCategoryCollisions(CCC_CONTROLLED,   CCCC_WHEEL_RAY,  false);
    
    sim->enableCategoryCollisions(CCCC_PROJECTILE,  CCCC_PROJECTILE, false);
    sim->enableCategoryCollisions(CCCC_PROJECTILE,  CCCC_WHEEL_RAY,  false);

    sim->enableCategoryCollisions(CCCC_HEIGHTFIELD, CCCC_WHEEL_RAY,  false);

    sim->enableCategoryCollisions(CCCC_WHEEL_RAY,   CCCC_WHEEL_RAY,  false);
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::addPlayer(const Player * player)
{
    s_log << Log::debug('l')
          << "GameLogicClientCommon::addPlayer("
          << player->getId()
          << ")\n";
    
    score_.addPlayer(player);
    gui_score_->update(score_);

    if (player == puppet_master_->getLocalPlayer())
    {
        spectator_camera_.trackPlayer(player->getId());
        s_console.addVariable("ping", const_cast<float*>(&player->getNetworkDelay()), &fp_group_);

        // send first equipment selection to server
        if (gui_tank_equipment_.get())
        {
            gui_tank_equipment_->sendEquipmentSelection();
        }
    }
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::removePlayer(const SystemAddress & pid)
{
    score_.removePlayer(pid);
    gui_score_->update(score_);

    if (spectator_camera_.getTargetPlayer() == pid)
    {
        spectator_camera_.changeTrackedPlayer(true);
    }
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::onPlayerNameChanged(Player * player)
{

    // if players name changed is local player also update 
    // params according
    if(puppet_master_->getLocalPlayer() == player)
    {
        s_params.set<std::string>("client.app.player_name", player->getName());    
    }

    updatePlayerLabel(player);
    gui_score_->update(score_);
}




//------------------------------------------------------------------------------
bool GameLogicClientCommon::handleInput(PlayerInput & input)
{
    waypoint_manager_->handleInput(input);

    if (input_mode_ == IM_CONTROL_CAMERA)
    {
     //   s_log << "cam ppos:" << spectator_camera_.getTransform().getTranslation() << "\n";
        spectator_camera_.handleInput(input);
        return true;
    } else if (input_mode_ == IM_SELECT_SPAWN_STAGE && !respawn_input_blocked_)
    {
        // Make sure we currently are without controllable
        assert(!puppet_master_->getLocalPlayer()->getControllable());

        if (input.fire1_)
        {
            sendRespawnRequest(selected_spawn_stage_);
            // immediately update msg
            handleRespawnCounter(0.0f);
        } else if (!respawn_request_sent_)
        {            
            int d = 0;
            if      (input.left_ ) d = 1;
            else if (input.right_) d = -1;

            if (d)
            {
                spectator_camera_.setTransform(selectValidSpawnStage(d));
                spectator_camera_.setMode(CM_FREE);
            }
        }
    }
/*
    if(puppet_master_->getLocalPlayer()->getControllable())
    {
            Tank * tank = dynamic_cast<Tank*>(puppet_master_->getLocalPlayer()->getControllable());
            float yaw,pitch;
            tank->getProxyTurretPos(yaw,pitch);
        s_log << "player jaw: " << yaw << "\n";
    }*/

    return false;
}





//------------------------------------------------------------------------------
/**
 *  
 */
Matrix GameLogicClientCommon::getCameraPosition()
{
    return spectator_camera_.getTransform();
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::onControllableChanged(Player * player, Controllable * prev_controllable)
{
    if (prev_controllable)
    {
        // This was used for collision sounds
        prev_controllable->getTarget()->clearCollisionCallback("s_body");
    }

    Tank * tank = dynamic_cast<Tank*>(player->getControllable());
    

    if (player == puppet_master_->getLocalPlayer() && tank)
    {
        // Set collision callback for client-side-predicted target
        // object, as this works much more reliable (collision sounds)
        tank->getTarget()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicClientCommon::tankCollisionCallback),
            "s_body");
        
        onLocalControllableSet();
    }
    
    if (tank)
    {
        executeEquipmentChange(tank);
        executeAllUpgrades(tank);

        if (tank->getOwner() == spectator_camera_.getTargetPlayer())
        {
            spectator_camera_.trackPlayer(spectator_camera_.getTargetPlayer());
        }

        // Re-enable 1st person view after tracking death
        // camera. Unfortunately, this will also enable 1st-person view
        // after manually switching to 3rd person tracking mode.
        if (player->getId() == spectator_camera_.getTargetPlayer() &&
            spectator_camera_.getMode() == CM_TRACKING_3RD_CONSTANT_DIR)
        {
            spectator_camera_.setMode(CM_TRACKING_1ST);
        }

    }

    updatePlayerLabel(player);

    repopulateMinimap();            
}


//------------------------------------------------------------------------------
/**
 *  See whether the object tracked by spectator camera is replaced,
 *  track server side part if so.
 */
void GameLogicClientCommon::onObjectReplaced(RigidBody * obj,
                                             const std::vector<RigidBody*> & parts)
{
    if (spectator_camera_.getTargetBody() != obj) return;

    for (unsigned t=0; t<parts.size(); ++t)
    {
        if (!parts[t]->getTarget()->isClientSideOnly())
        {
            spectator_camera_.trackBody(parts[t]);
            return;
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  This is called after object precaching but before actually
 *  creating the objects.
 */
void GameLogicClientCommon::loadLevel(const std::string & lvl_name)
{
    // early map check
    if(!existsFile((LEVEL_PATH + lvl_name + "/objects.xml").c_str()) &&
       !existsFile((LEVEL_PATH + lvl_name + "/terrain.hm").c_str()) &&
       !existsFile((LEVEL_PATH + lvl_name + "/detail.png").c_str()))
    {
        Exception e;
        e << "The map \""
          << lvl_name
          << "\" is not installed.";
        throw e;
    }

    respawn_input_blocked_ = false;
    
    s_log << Log::debug('i')
          << "GameLogicClientCommon::loadLevel("
          << lvl_name
          << ")\n";

    lvl_name_ = lvl_name;

    fp_group_remove_revealed_.deregisterAllOfType(TaskFp());

    createHud();
    
    bbm::LevelData lvl_data;
    lvl_data.load(lvl_name);

    
    // -------------------- Height Data stuff --------------------
    std::auto_ptr<terrain::TerrainDataClient> td(new terrain::TerrainDataClient);
    td->load(lvl_name);

    if (terrain_visual_.get())
    {    
        terrain_visual_->setData(td.get());        
        terrain_visual_->setTextures(lvl_data.getDetailTexInfo(),
                                     lvl_data.getName());
    }
                                 
    hud_->getMinimap()->setupBackground(LEVEL_PATH + lvl_name + "/minimap.dds",
                                        td->getHorzScale() * td->getResX());    
    RigidBodyVisual::setTerrainData(td.get());
    Tank           ::setTerrainData(td.get());
    // pass ownership to gamestate
    puppet_master_->getGameState()->setTerrainData(std::auto_ptr<terrain::TerrainData>(td.release()), CCCC_HEIGHTFIELD);


    // -------------------- Sky Dome --------------------
    try
    {
        sky_visual_ = ReaderWriterBbm::loadModel(lvl_data.getParams().get<std::string>("skydome.model"));
        sky_visual_->addToScene();

        // Disable z-buffer writing, z-test
        osg::MatrixTransform * osg_node = sky_visual_->getOsgNode();
        osg::StateSet * state_set = osg_node->getOrCreateStateSet();
        state_set->setAttribute(new osg::Depth(osg::Depth::ALWAYS, 0,1,false));
                
        state_set->setRenderBinDetails(BN_SKY, "RenderBin");

        // XXXXX shader to keep sky centered around viewer; hack for
        // no GLSL-support
        if (s_params.get<unsigned>("client.graphics.shader_quality") != 0)
        {
            osg::Program * program = s_scene_manager.getCachedProgram("sky");
            state_set->setAttribute(program, osg::StateAttribute::OVERRIDE);
        }

        osg_node->setCullingActive(false);
        for (unsigned i=0; i<osg_node->getNumChildren(); ++i)
        {
            osg_node->getChild(i)->setCullingActive(false);
        }
    } catch (Exception & e)
    {
        s_log << Log::error
              << "Failed to load sky dome: "
              << e;
    }
    

    // -------------------- Level Data stuff --------------------
    s_scene_manager.setAmbient(lvl_data.getParams().get<float>("light.ambient"));
    Vector fog_color = lvl_data.getParams().get<Vector>("fog.color");
    s_scene_manager.setFogProperties(Color((unsigned)fog_color.x_,
                                           (unsigned)fog_color.y_,
                                           (unsigned)fog_color.z_),
                                     lvl_data.getParams().get<float>("fog.offset"),
                                     lvl_data.getParams().get<float>("fog.density") * 0.001f);

    
    // -------------------- Objects --------------------
    for (std::vector<bbm::ObjectInfo>::const_iterator cur_object_desc = lvl_data.getObjectInfo().begin();
         cur_object_desc != lvl_data.getObjectInfo().end();
         ++cur_object_desc)
    {
        std::string type = "RigidBody";
        try
        {
            type = cur_object_desc->params_.get<std::string>("type.type");
        } catch (ParamNotFoundException & e) {}


        if (type == "AmbientSound")
        {            
            SoundSource * effect = new SoundSource("ambient/" + cur_object_desc->name_ + ".wav", SSCF_LOOP);
            effect->setPosition(cur_object_desc->transform_.getTranslation());
            
            effect->setReferenceDistance(5.0);
            effect->setRolloffFactor(10.5);

            s_scene_manager.getRootNode()->addChild(effect);
            effect->play();
            continue;
        }

        onLevelObjectLoaded(type, *cur_object_desc);
    }


    waypoint_manager_.reset(new WaypointManagerClient(puppet_master_));

}


//------------------------------------------------------------------------------
const std::string & GameLogicClientCommon::getLevelName() const
{
    return lvl_name_;
}




//------------------------------------------------------------------------------
void GameLogicClientCommon::executeCustomCommand(unsigned type, RakNet::BitStream & args)
{
    switch (type)
    {
    case CSCT_WEAPON_HIT:
        weaponHit(args);
        break;
    case CSCT_SCORE_UPDATE:
        updateScore(args);
        break;
    case CSCT_PLAYERS_LATENCY_UPDATE:
        updatePlayersLatency(args);
        break;
    case CSCT_TEAM_ASSIGNMENT:
        teamAssignment(args);
        break;
    case CSCT_KILL:
        kill(args);
        break;
    case CSCT_EXECUTE_UPGRADE:
        setUpgradeLevel(args);
        break;
    case CSCT_EQUIPMENT_UPDATE:
        setEquipment(args);
        break;
    case CSCT_TEAM_MESSAGE:
        teamMessage(args);
        break;
    case CSCT_STATUS_MESSAGE:
        statusMessage(args);
        break;
    case CSCT_MATCH_STATISTICS:
        matchStatistics(args);
        break;
    default:
        s_log << Log::warning
              << "Unknown game logic command type: "
              << type
              << "\n";
    }
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::onGameObjectAdded(GameObject * object)
{
    assert(!lvl_name_.empty());
    if (lvl_name_.empty()) return;
    
    RigidBody * rigid_body = dynamic_cast<RigidBody*>(object);
    assert(rigid_body);

    if (rigid_body->isStatic())
    {
        rigid_body->setCollisionCategory(CCC_STATIC, false);
    } else
    {
        rigid_body->setCollisionCategory(CCC_PROXY, true);
    }
    

    if (dynamic_cast<Projectile*>(rigid_body))
    {
        rigid_body->setCollisionCategory(CCCC_PROJECTILE, true);
        
        // XXXX this should be moved into TankCannon, needs access to puppetmasterclient first!!
        rigid_body->getProxy()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicClientCommon::projectileCollisionCallback));
            
    } else if (rigid_body->getType() == "Tank")
    {
        Tank * tank = (Tank*)rigid_body;
        tank->setWheelCategory(CCCC_WHEEL_RAY);

        rigid_body->getProxy()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicClientCommon::tankCollisionCallback),
            "s_body");
        
    } else
    {
        if (rigid_body->getProxy())
        {
            rigid_body->getProxy()->setCollisionCallback(
                physics::CollisionCallback(this, &GameLogicClientCommon::bodyCollisionCallback));
        } else if (!rigid_body->isStatic())
        {
            // client-side only obejcts have no proxy...
            rigid_body->getTarget()->setCollisionCallback(
                physics::CollisionCallback(this, &GameLogicClientCommon::bodyCollisionCallback));
        }
    }

    handleMinimapIcon(rigid_body);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::createGuiScreens()
{
    gui_score_.             reset(new GUIScore(puppet_master_));
    gui_help_.              reset(new GUIHelp());
    gui_upgrade_system_.    reset(new GUIUpgradeSystem(puppet_master_));
    gui_tank_equipment_.    reset(new GUITankEquipment(puppet_master_));
    gui_teamselect_.        reset(new GUITeamSelect(puppet_master_));
    gui_match_summary_.     reset(new GUIMatchSummary(puppet_master_));
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::createHud()
{
    hud_.reset(new GameHudTankZb(puppet_master_, "hud_tank.xml"));
    hud_->enable(false);    
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::onLevelObjectLoaded(const std::string & type, const bbm::ObjectInfo & info)
{
    if (type == "HelperObject")
    {
        if (info.params_.get<std::string>("logic.role") == "start_camera")
        {
            // stage index is 1--based!
            spectator_camera_.setTransform(info.transform_);
        } else if (info.params_.get<std::string>("logic.role") == "light_direction")
        {
            s_scene_manager.setLightDir(-info.transform_.getZ());
        }
    }
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::setInputMode(INPUT_MODE mode, bool console_msg)
{    
    if (mode == IM_CONTROL_CAMERA)
    {
        if (input_mode_ == mode) return;
        
        input_mode_ = mode;

        // Don't stay in 1st person mode if switching to camera control...
        if (spectator_camera_.getMode() == CM_TRACKING_1ST)
        {
            spectator_camera_.setMode(CM_FREE);
        }

        if (console_msg) puppet_master_->getHud()->addMessage("Controlling camera.");
    } else if (mode == IM_CONTROL_TANK)
    {
        if (input_mode_ == mode) return;        
        input_mode_ = mode;
        spectator_camera_.trackPlayer(puppet_master_->getLocalPlayer()->getId());

        if (console_msg) puppet_master_->getHud()->addMessage("Controlling tank.");
    } else
    {
        // Show currently selected spawn stage
        input_mode_ = IM_SELECT_SPAWN_STAGE;

        // make sure to select a stage that belongs to us
        selectValidSpawnStage(0);

        handleRespawnCounter(0.0f);
    }

    spectator_camera_.clearMotion();    
}


//------------------------------------------------------------------------------
/**
 *  Temporarily reveals an icon on the minimap. Forces addition of
 *  object to minimap, schedules removal.
 */
void GameLogicClientCommon::revealOnMinimap(RigidBody * object)
{
    if (hud_->getMinimap()->hasIcon(object)) return;
    
    hud_->getMinimap()->removeIcon(object);

    handleMinimapIcon(object, true);

    s_scheduler.addEvent(SingleEventCallback(this, &GameLogicClientCommon::removeRevealed),
                         MINIMAP_REVEAL_TIME,
                         object,
                         "GameLogicClientCommon::removeRevealed",
                         &fp_group_remove_revealed_);
}

//------------------------------------------------------------------------------
/**
 *  First removes all icons from the minimap, then traverses all
 *  gameobjects and executes their handleMinimap coe.
 */
void GameLogicClientCommon::repopulateMinimap()
{
    if(!hud_.get()) return;

    hud_->getMinimap()->clear();
    
    fp_group_remove_revealed_.deregisterAllOfType(TaskFp());

    for (GameState::GameObjectContainer::iterator it = puppet_master_->getGameState()->getGameObjects().begin();
         it != puppet_master_->getGameState()->getGameObjects().end();
         ++it)
    {
        assert(dynamic_cast<RigidBody*>(it->second));
        handleMinimapIcon((RigidBody*)it->second);
    }

    onRepopulateMinimap();
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::removeRevealed(void * obj)
{
    hud_->getMinimap()->removeIcon((RigidBody*)obj);    
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::toggleInputMode()
{
    if (input_mode_ == IM_SELECT_SPAWN_STAGE) return;
    setInputMode(input_mode_ == IM_CONTROL_TANK ? IM_CONTROL_CAMERA : IM_CONTROL_TANK, true);
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::weaponHit(RakNet::BitStream & args)
{
    SystemAddress player_hit,shooter;
    Vector pos;
    Vector n;
    uint8_t weapon_hit_type;
    uint8_t object_hit_type;

    args.Read(shooter);
    args.Read(player_hit);
    args.Read(pos);
    args.Read(n);
    args.Read(weapon_hit_type);
    args.Read(object_hit_type);

    if(object_hit_type == OHT_TANK)
    {
        activateHitMarker(shooter, player_hit, pos, (WEAPON_HIT_TYPE)weapon_hit_type);
        activateHitFeedback(shooter, player_hit, (WEAPON_HIT_TYPE)weapon_hit_type);
    }

    std::string sound;
    std::string effect_group;
    std::vector<std::string> mg_tank_hit_sounds;


    // handle types

    /// ------------------------------ PROJECTILE -------------------------
    if(weapon_hit_type == WHT_PROJECTILE)
    {
        if(object_hit_type == OHT_TANK)
        {
            sound   = s_params.get<std::string>("sfx.projectile_hit_tank");
            effect_group = "projectile_hit_tank";
        }
        if(object_hit_type == OHT_OTHER)
        {
            sound   = s_params.get<std::string>("sfx.projectile_hit_other");
            effect_group = "projectile_hit_other";
        }
        if(object_hit_type == OHT_WATER)
        {
            sound   = s_params.get<std::string>("sfx.projectile_hit_other");
            effect_group = "projectile_hit_water";
        }
    }

    /// ------------------------------ PROJECTILE SPLASH -------------------------
    if(weapon_hit_type == WHT_PROJECTILE_SPLASH)
    {
        if(object_hit_type == OHT_TANK)
        {
            sound   = s_params.get<std::string>("sfx.projectile_hit_tank");
            effect_group = "projectile_splash_hit_tank";
        }
        if(object_hit_type == OHT_OTHER)
        {
            sound   = s_params.get<std::string>("sfx.projectile_hit_other");
            effect_group = "projectile_splash_hit_other";
        }
        if(object_hit_type == OHT_WATER)
        {
            sound   = s_params.get<std::string>("sfx.projectile_hit_other");
            effect_group = "projectile_hit_water";
        }
    }

    /// ------------------------------ PROJECTILE SPLASH INDIRECT -------------------------
    if(weapon_hit_type == WHT_PROJECTILE_SPLASH_INDIRECT)
    {
        if(object_hit_type == OHT_TANK)
        {
            sound   = s_params.get<std::string>("sfx.projectile_indirect_hit_tank");
        }
    }

    /// ------------------------------ MINE -------------------------
    if(weapon_hit_type == WHT_MINE)
    {
        if(object_hit_type == OHT_TANK)
        {
            sound   = s_params.get<std::string>("sfx.mine_explosion");
            effect_group = "mine_hit_tank";
        }
        if(object_hit_type == OHT_OTHER)
        {
            sound   = s_params.get<std::string>("sfx.mine_explosion");
            effect_group = "mine_hit_other";
        }
    }

    /// ------------------------------ MACHINE GUN -------------------------
    if(weapon_hit_type == WHT_MACHINE_GUN)
    {
        if(object_hit_type == OHT_TANK)
        {
            mg_tank_hit_sounds = s_params.get<std::vector<std::string> >("sfx.mg_hit_tank");
            sound = mg_tank_hit_sounds[rand() % mg_tank_hit_sounds.size()];
            effect_group = "machine_gun_hit_tank";
        }
        if(object_hit_type == OHT_OTHER)
        {
            sound   = s_params.get<std::string>("sfx.mg_hit_other");
            effect_group = "machine_gun_hit_other";
        }
        if(object_hit_type == OHT_WATER)
        {
            sound   = s_params.get<std::string>("sfx.mg_hit_water");
            effect_group = "machine_gun_hit_water";
        }
    }

    /// ------------------------------ FLAMETHROWER -------------------------
    if(weapon_hit_type == WHT_FLAME_THROWER)
    {
        if(object_hit_type == OHT_TANK)
        {
            sound   = s_params.get<std::string>("sfx.flamethrower_hit");
            effect_group = "flamethrower_hit";
        }
        if(object_hit_type == OHT_OTHER)
        {
            sound   = s_params.get<std::string>("sfx.flamethrower_hit");
            effect_group = "flamethrower_hit";
        }
        if(object_hit_type == OHT_WATER)
        {
            sound   = s_params.get<std::string>("sfx.water_boil");
            effect_group = "projectile_hit_water";
        }
    }

    /// ------------------------------ LASER, Tractor Beam -----------
    if(weapon_hit_type == WHT_LASER) 
    {
        if(object_hit_type == OHT_TANK)
        {
            sound   = s_params.get<std::string>("sfx.flamethrower_hit");
            effect_group = "laser_hit";
        }
        if(object_hit_type == OHT_OTHER)
        {
            sound   = s_params.get<std::string>("sfx.flamethrower_hit");
            effect_group = "laser_hit";
        }
        if(object_hit_type == OHT_WATER)
        {
            sound   = s_params.get<std::string>("sfx.water_boil");
            effect_group = "projectile_hit_water";
        }
    }

    if (weapon_hit_type == WHT_TRACTOR_BEAM)
    {
        if(object_hit_type == OHT_TANK)
        {
            sound   = s_params.get<std::string>("sfx.flamethrower_hit");
            effect_group = "tractor_hit";
        }
        if(object_hit_type == OHT_OTHER)
        {
            sound   = s_params.get<std::string>("sfx.flamethrower_hit");
            effect_group = "tractor_hit";
        }
        if(object_hit_type == OHT_WATER)
        {
            sound   = s_params.get<std::string>("sfx.water_boil");
            effect_group = "projectile_hit_water";
        }
    }

    /// actually play selected effects
    if(!sound.empty())
    {
        s_soundmanager.playSimpleEffect(sound, pos);
    }

    if (!effect_group.empty())
    {
        s_effect_manager.createEffect(effect_group, pos, n, false, s_scene_manager.getRootNode());
    }

}


//------------------------------------------------------------------------------
/**
 *  Handles custom command "CSCT_SCORE_UPDATE".
 */
void GameLogicClientCommon::updateScore(RakNet::BitStream & args)
{
    SystemAddress id;

    args.Read(id);

    if (id != UNASSIGNED_SYSTEM_ADDRESS)
    {
        PlayerScore * score = score_.getPlayerScore(id);

        if (!score)
        {
            s_log << Log::warning
                  << "Received score for nonexisting player "
                  << id << "\n";
            return;
        }

        uint16_t cur_upgrade_points = score->upgrade_points_;
        // Play a sound effect if an upgrade becomes possible in any category.
        bool upgrade_possible[NUM_UPGRADE_CATEGORIES];
        bool any_upgrade_possible = false;
        bool has_any_upgrade = false;
        for (unsigned c=0; c<NUM_UPGRADE_CATEGORIES; ++c)
        {
            upgrade_possible[c] = score->isUpgradePossible((UPGRADE_CATEGORY)c);
            any_upgrade_possible |= upgrade_possible[c];
            has_any_upgrade |= score->active_upgrades_[c] != 0;
        }

        args.Read(score->score_);
        args.Read(score->kills_);
        args.Read(score->deaths_);
        args.Read(score->goals_);
        args.Read(score->assists_);
        args.Read(score->shots_);
        args.Read(score->hits_);
        args.Read(score->upgrade_points_);

        if (puppet_master_->getLocalPlayer()->getId() == id)
        {
            for (unsigned c=0; c<NUM_UPGRADE_CATEGORIES; ++c)
            {
                // check if upgrade category is upgradable
                if(score->isUpgradePossible((UPGRADE_CATEGORY)c) &&
                    !upgrade_possible[c])
                {
                    SoundSource * snd =
                        s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.upgrade_available"),
                                                        s_soundmanager.getListenerInfo().position_);
                    snd->setRolloffFactor(0.0f);


                    // If this is the first upgrade available at all,
                    // display a hint in the message box.
                    if (!any_upgrade_possible && !has_any_upgrade)
                    {
                        try
                        {
                            std::string
                                u  = s_input_handler.getKeyForFunction("Show Upgrade Screen"),
                                u1 = s_input_handler.getKeyForFunction("Upgrade Weapons"),
                                u2 = s_input_handler.getKeyForFunction("Upgrade Armor"),
                                u3 = s_input_handler.getKeyForFunction("Upgrade Speed");
                            puppet_master_->getHud()->addMessage("Upgrade available!", MC_UPGRADE_AVAILABLE);
                            puppet_master_->getHud()->addMessage("Press '" + u +
                                                                 "' to display the upgrade screen,", MC_UPGRADE_AVAILABLE);
                            puppet_master_->getHud()->addMessage("or press '" +
                                                                 u1 + "', '" +
                                                                 u2 + "' or '" +
                                                                 u3 +
                                                                 "' to upgrade directly.", MC_UPGRADE_AVAILABLE);
                        } catch (input_handler::KeyNotRegisteredException & e)
                        {
                        }
                    }
                    
                    break;
                }
            }

            if (score->upgrade_points_ > cur_upgrade_points)
            {
                puppet_master_->getHud()->appendMessage(" (+" +
                                                        toString(score->upgrade_points_-cur_upgrade_points) +
                                                        " UP)");
            }
        }
    }

    // get team score
    for(unsigned c=0; c < score_.getNumTeams(); c++)
    {
        args.Read(score_.getTeamScore((TEAM_ID)c)->score_);
    }
    

    // read time left
    float time;
    args.Read(time);
    score_.setTimeLeft(time);

    gui_score_->update(score_);
    if (gui_upgrade_system_.get())
    {
        gui_upgrade_system_->update(score_);
    }
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::updatePlayersLatency(RakNet::BitStream & args)
{
    SystemAddress pid;
    unsigned num_players;
    float net_latency;

    args.Read(num_players);

    for(unsigned p=0; p < num_players; p++)
    {
        args.Read(pid);
        args.Read(net_latency);

        Player * player = puppet_master_->getLocalOrRemotePlayer(pid);

        if(player)
        {
            player->setNetworkDelay(net_latency);
        }
    }

    /// XXX probably not necessary, but convenient
    gui_score_->update(score_);

}

//------------------------------------------------------------------------------
void GameLogicClientCommon::teamAssignment(RakNet::BitStream & args)
{
    SystemAddress pid;
    TEAM_ID team_id;

    args.Read(pid);
    args.Read(team_id);

    if (!score_.getPlayerScore(pid))
    {
        s_log << Log::warning
              << "Ignoring teamAssignment for unknown player "
              << pid
              << "\n";
    }
    
    score_.assignToTeam(pid, team_id);

    gui_score_->update(score_);

    const TeamScore * team_score = score_.getTeamScore(team_id);

    Player * player = NULL;
    if (pid == puppet_master_->getLocalPlayer()->getId())
    {
        // Update hud if local player
        player = puppet_master_->getLocalPlayer();
    } else
    {
        player = puppet_master_->getRemotePlayer(pid);
        
        // Notification if other player changes team
        if (!team_score &&
            spectator_camera_.getTargetPlayer() == player->getId())
        {
            spectator_camera_.changeTrackedPlayer(true);
        }
    }

    assert(player);
    onTeamAssignmentChanged(player);    
    updatePlayerLabel(player);

    repopulateMinimap();
}



//------------------------------------------------------------------------------
/**
 *  Outputs a message for the kill.
 */
void GameLogicClientCommon::kill(RakNet::BitStream & args)
{
    SystemAddress killer, killed;
    bool assist;
    PLAYER_KILL_TYPE kill_type;
    args.Read(killer);
    args.Read(killed);
    args.Read(kill_type);
    args.Read(assist);

    onKill(killer, killed, kill_type, assist);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::setUpgradeLevel(RakNet::BitStream & args)
{
    
    SystemAddress player_id;
    args.Read(player_id);

    
    PlayerScore * score = score_.getPlayerScore(player_id);

    if (!score)
    {
        s_log << Log::warning
              << "Received score for nonexisting player "
              << player_id << "\n";
        return;
    } else
    {
        s_log << Log::debug('l')
              << "upgradeRequestAccepted received for "
              << player_id
              << "\n";
    }

    uint8_t category;
    uint8_t level;

    args.Read(score->upgrade_points_);
    args.Read(category);
    args.Read(level);


    // upgrade the players' tank provided in args
    Controllable * controllable = NULL;
    Player * player = NULL;

    player = puppet_master_->getLocalOrRemotePlayer(player_id);
    
    if(player)
    {
        controllable = player->getControllable();
    }

    assert(score->active_upgrades_[category] != level);

    // loop over levels and upgrade new levels
    for(uint8_t new_level = score->active_upgrades_[category]+1; new_level <= level; new_level++)
    {
        executeUpgradeStep(controllable, category, new_level);
    }

    score->active_upgrades_[category] = level;
    
    // upgrade gui, for feedback
    if (gui_upgrade_system_.get())
    {
        gui_upgrade_system_->update(score_);
    }
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::setEquipment(RakNet::BitStream & args)
{
    SystemAddress player_id;
    bool execute;
    args.Read(player_id);
    args.Read(execute);
    
    PlayerScore * score = score_.getPlayerScore(player_id);

    if (!score)
    {
        s_log << Log::warning
              << "Received equipment for nonexisting player "
              << player_id << "\n";
        return;
    } else
    {
        s_log << Log::debug('l')
              << "setEquipment received for "
              << player_id
              << "\n";
    }

    for(unsigned s=0; s < NUM_EQUIPMENT_SLOTS; s++)
    {
        args.Read(score->active_equipment_[s]);
    }

    if (execute)
    {
        executeEquipmentChange(score->getPlayer()->getControllable());
    }
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::teamMessage(RakNet::BitStream & args)
{

    SystemAddress player_id;
    unsigned message;

    args.Read(player_id);
    args.Read(message);  

    Player * player = puppet_master_->getLocalOrRemotePlayer(player_id);

    if(!player)
    {
        s_log << Log::warning << "Received Team Message from unknown player, id: "
              << player_id << "\n";
        return;        
    }

    if (!player->getControllable())
    {
        s_log << Log::warning << "Received Team Message from player "
              << player->getName()
              << " without controllable.\n";
        return;
    }

    switch(message)
    {
    case CTM_NEED_ASSISTANCE:
        puppet_master_->getHud()->addChatLine(player->getName() + " needs assistance!",Color(1.0,1.0,0.0,1.0));
        s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.need_assistance"),
                                        s_soundmanager.getListenerInfo().position_, 0.0f);

        hud_->getMinimap()->enableFlash(player->getControllable(), 3.0f);
        break;
    case CTM_ACKNOWLEDGED:
        puppet_master_->getHud()->addChatLine("Acknowledged by " + player->getName(),Color(1.0,1.0,0.0,1.0));
        s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.acknowledged"),
                                        s_soundmanager.getListenerInfo().position_, 0.0f);
        break;
    default:
        s_log << Log::warning << "Received unknown Team Message Command\n";
        return;
    }

}

//------------------------------------------------------------------------------
void GameLogicClientCommon::statusMessage(RakNet::BitStream & args)
{
    std::string message;
    Color color;

    network::readFromBitstream(args, message);
    args.Read(color.r_);
    args.Read(color.g_);
    args.Read(color.b_);
    color.a_ = 1.0f;

    puppet_master_->getHud()->addMessage(message, color);
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::matchStatistics(RakNet::BitStream & args)
{
    onMatchStatsReceived(args);
}



//------------------------------------------------------------------------------
/**
 *  Currently has only the job of playing collision sounds.
 */
bool GameLogicClientCommon::tankCollisionCallback(const physics::CollisionInfo & info)
{
    const float COLLISION_SOUND_THRESHOLD = 1.0f; // PPPP
    const float HIT_FEEDBACK_THRESHOLD    = 3.0f; // PPPP
    
    if (info.type_ != physics::CT_START) return true;
    
    Tank * tank = (Tank*)info.this_geom_->getUserData();

    // Collisions of local tank with static geometry are handled by
    // the target collision callback, so bail in this case if we are
    // proxy.
    if (tank->getProxy() == info.this_geom_->getBody()                &&
        tank->getOwner() == puppet_master_->getLocalPlayer()->getId() &&
        info.other_geom_->isStatic())
    {
        return true;
    }
    
    if (info.other_geom_->getName() == "void")
    {
        return false;
    } else 
    {
        // Ignore projectiles and mines for collision sound
        if (info.other_geom_->getName() != "s_projectile" &&
            info.other_geom_->getName() != "s_mine")
        {
            // check normal velocity, if above threshold, play sound
            Vector v_rel = tank->getGlobalLinearVel();
            if (!info.other_geom_->isStatic()) v_rel -= info.other_geom_->getBody()->getGlobalLinearVel();
            float dot = -vecDot(&v_rel, &info.n_);

            if (dot > COLLISION_SOUND_THRESHOLD)
            {
                const float COLLISION_SOUND_GAIN_FACTOR = 0.17f;

                RigidBody  * other_body = (RigidBody*) info.other_geom_->getUserData();
                
                std::string coll_sound;
                if(other_body && other_body->getType() == "Tank")
                {
                    // Play collision sounds only once
                    if (tank < other_body) return true;

                    coll_sound = s_params.get<std::string>("sfx.collision_tank");
                }
                else
                {
                    coll_sound = s_params.get<std::string>("sfx.collision_non_tank");
                }

                SoundSource * snd_collide = s_soundmanager.playSimpleEffect(coll_sound, info.pos_);
                snd_collide->setGain(dot * COLLISION_SOUND_GAIN_FACTOR);

            }

            // XXX this should match the parameter in tanks.xml
            if (dot > HIT_FEEDBACK_THRESHOLD &&
                puppet_master_->getLocalPlayer()->getControllable() == tank)
            {
                hud_->activateHitMaker(info.pos_);
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------
/**
 *  Make projectile invisible on first contact, to avoid ugly fly through effects
 */
bool GameLogicClientCommon::projectileCollisionCallback(const physics::CollisionInfo & info)
{
    if (info.type_ != physics::CT_START) return false;
    
    Projectile * projectile = (Projectile*)info.this_geom_->getUserData();
    GameObjectVisual * visual = (GameObjectVisual*)projectile->getUserData();
    assert(dynamic_cast<ProjectileVisual*>(visual));
    
    if (!info.other_geom_->isStatic())
    {
        RigidBody  * other_body = (RigidBody*) info.other_geom_->getUserData();

        // ignore if projectile hits own tank
        if (other_body &&
            projectile->getOwner() == other_body->getOwner() &&
            other_body->getType() == "Tank") return false;


        // ignore if soccer cannon hits tank
        if (other_body &&
            other_body->getType() == "Tank" &&
            projectile->getSection() == "soccer_cannon") return false;
        
        if (visual->isVisible())
        {
            projectile->applyImpactImpulse(other_body,
                                           1.0f,
                                           info.pos_,
                                           info.n_);
        }
    }

    visual->setVisible(false);
    
    return false;
}


//------------------------------------------------------------------------------
bool GameLogicClientCommon::bodyCollisionCallback(const physics::CollisionInfo & info)
{
    const float COLLISION_SOUND_THRESHOLD   = 1.0f; // PPPP
    const float COLLISION_SOUND_GAIN_FACTOR = 0.17f;
    
    if (info.type_ != physics::CT_START) return true;

    if (info.other_geom_->getName() == "void" ||
        info.this_geom_ ->getName() == "void") return false;
    
    RigidBody * body1 = (RigidBody*)info.this_geom_ ->getUserData();
    RigidBody * body2 = (RigidBody*)info.other_geom_->getUserData();
    if (!body1) return true;

    if (body1 < body2) return true;
    
    // check normal velocity, if above threshold, play sound
    Vector v_rel = body1->getGlobalLinearVel();
    if (body2) v_rel -= body2->getGlobalLinearVel();
    float dot = -vecDot(&v_rel, &info.n_);

    if (dot < COLLISION_SOUND_THRESHOLD) return true;
    
    EffectManager::EffectPair pair =
        s_effect_manager.createEffect("collision",
                                      info.pos_,
                                      info.n_,
                                      false,
                                      s_scene_manager.getRootNode());
    for (unsigned i=0; i<pair.second->getNumEffects(); ++i)
    {
        pair.second->getEffect(i)->setGain(dot * COLLISION_SOUND_GAIN_FACTOR);
    }

    return true;
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::upgradeWeapon()
{
    sendUpgradeRequest(UC_WEAPON);
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::upgradeArmor()
{
    sendUpgradeRequest(UC_ARMOR);
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::upgradeSpeed()
{
    sendUpgradeRequest(UC_SPEED);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::sendTeamMessage1()
{
    sendTeamMessage(CTM_NEED_ASSISTANCE);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::sendTeamMessage2()
{
    sendTeamMessage(CTM_ACKNOWLEDGED);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::sendTeamMessage(const CUSTOM_TEAM_MESSAGE & ctm)
{
    // check if player is allowed to send message
    if(!puppet_master_->getLocalPlayer()->allowAnnoyingClientRequest())
    {
        puppet_master_->getHud()->addChatLine("Blocked due to flooding.",Color(1.0,0.0,0.0,1.0));        
        return;
    }

    // if player has no controllable bail out
    if (!puppet_master_->getLocalPlayer()->getControllable()) return;

    RakNet::BitStream rak_args;
    rak_args.Write(puppet_master_->getLocalPlayer()->getId());
    rak_args.Write((unsigned)ctm);

    network::CustomClientCmd request(CCCT_TEAM_MESSAGE, rak_args);
    request.send(puppet_master_->getInterface());  

    puppet_master_->getLocalPlayer()->incAnnoyingClientRequest();
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::executeAllUpgrades(Controllable * controllable)
{ 
    if(!controllable) return;

    PlayerScore * score = score_.getPlayerScore(controllable->getOwner());

    if (!score)
    {
        s_log << Log::error
              << "No PlayerScore in GameLogicClientCommon::executeUpgrade("
              << *controllable
              << ")\n";
        return;
    }

    for(uint8_t cat = 0; cat < NUM_UPGRADE_CATEGORIES; cat++)
    {
        for(uint8_t lvl = 1; lvl <= score->active_upgrades_[cat]; lvl++)
        {
            executeUpgradeStep(controllable, cat, lvl);
        }
    }

}

//------------------------------------------------------------------------------
/**
 *  Load appropriate parameter super sections into tank
 *
 *  On client-side also upgrade the controllable inside the replay simulator
 */
void GameLogicClientCommon::executeUpgradeStep(Controllable * controllable,
                                               uint8_t category,
                                               uint8_t level)
{     
    assert(level > 0);

    Tank * tank = dynamic_cast<Tank*>(controllable);
    if(!tank) return;

    std::string file = CONFIG_PATH + "upgrades.xml";
    std::string upgrade_name      = UPGRADES[category] + toString(level  );
    std::string prev_upgrade_name = UPGRADES[category] + toString(level-1);
    
    tank->loadParameters(file, upgrade_name);

    // update the tank inside the replay simulator for local player
    LocalPlayer * local_player = puppet_master_->getLocalPlayer();
    if(tank->getOwner() == local_player->getId())
    {
        Tank * replay_tank = (Tank*)local_player->getReplaySimulatorControllable();
        replay_tank->loadParameters(file, upgrade_name);  
    }
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::executeEquipmentChange(Controllable * controllable)
{ 
    Tank * tank = dynamic_cast<Tank*>(controllable);
    
    if(!tank) return;

    std::string file = CONFIG_PATH + "equipment.xml";

    PlayerScore * score = score_.getPlayerScore(tank->getOwner());

    for(unsigned slot = 0; slot < NUM_EQUIPMENT_SLOTS; slot++)
    {   
        std::string super_section = EQUIPMENT_SLOTS[slot] + toString(score->active_equipment_[slot]);   
        tank->loadParameters(file, super_section);    

        // update the tank inside the replay simulator for local player
        LocalPlayer * local_player = puppet_master_->getLocalPlayer();
        if(tank->getOwner() == local_player->getId())
        {
            Tank * replay_tank = (Tank*)local_player->getReplaySimulatorControllable();
            replay_tank->loadParameters(file, super_section);  

            // also set crosshair depending on weapon equipment
            if(slot == 0) hud_->setCrosshair("crosshair_" + super_section);
        }

        TankVisual * tank_visual = (TankVisual*)(tank->getUserData());
        assert(tank_visual);

        EnableGroupVisitor v1(super_section, true);
        tank_visual->getWrapperNode()->getOsgNode()->accept(v1);
    }
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::updateTimeLimit(float dt)
{
    float time_left = score_.getTimeLeft();
    time_left = std::max(0.0f, time_left - dt);
    score_.setTimeLeft(time_left);
}

//------------------------------------------------------------------------------
/**
 *  XXX fast fix to avoid client message flooding
 */
void GameLogicClientCommon::decAnnoyingClientRequests(float dt)
{
    puppet_master_->getLocalPlayer()->decAnnoyingClientRequest();   
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::updatePlayerLabel(Player * player)
{
    if (!player->getControllable()) return;
    assert(dynamic_cast<ControllableVisual*>((GameObjectVisual*)player->getControllable()->getUserData()));
    
    ControllableVisual * controllable_visual =
        (ControllableVisual*)player->getControllable()->getUserData();

    
    controllable_visual->setLabelText(player->getName());
    
    Team * team = score_.getTeam(player->getId());
    if (team)
    {
        controllable_visual->setLabelColor(team->getColor());            
    }
}


//------------------------------------------------------------------------------
Score & GameLogicClientCommon::getScore()
{
    return score_;
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::sendUpgradeRequest(UPGRADE_CATEGORY category)
{
    // to avoid flooding with upgrade requests, only send the command 
    // if the player has enough upgrade points available
    SystemAddress pid = puppet_master_->getLocalPlayer()->getId();
    PlayerScore * player_score = score_.getPlayerScore(pid);

    assert(player_score);    

    // if player has enough points, send the upgrade request to server
    if(player_score->isUpgradePossible(category))
    {
        RakNet::BitStream rak_args;
        rak_args.Write(pid);
        rak_args.Write((uint8_t)category);

        network::CustomClientCmd request(CCCT_REQUEST_UPGRADE, rak_args);
        request.send(puppet_master_->getInterface());
    }
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::sendEquipmentChangeRequest(const std::vector<uint8_t> & equipment)
{
    // to avoid flooding with upgrade requests, only send the command 
    // if the player has enough upgrade points available
    SystemAddress pid = puppet_master_->getLocalPlayer()->getId();
    PlayerScore * player_score = score_.getPlayerScore(pid);

    assert(player_score);    
    assert(equipment.size() == NUM_EQUIPMENT_SLOTS);

    // send the equipment change request to server
    RakNet::BitStream rak_args;
    rak_args.Write(pid);

    for(unsigned c=0; c < equipment.size(); c++)
    {
        rak_args.Write(equipment[c]);
    }

    network::CustomClientCmd request(CCCT_REQUEST_EQUIPMENT_CHANGE, rak_args);
    request.send(puppet_master_->getInterface());
    
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::sendTeamChangeRequest(TEAM_ID team)
{
    if (team >= score_.getNumTeams() && team != INVALID_TEAM_ID) return;

    RakNet::BitStream rak_args;
    rak_args.Write(puppet_master_->getLocalPlayer()->getId());
    rak_args.Write(team);

    network::CustomClientCmd request(CCCT_REQUEST_TEAM_CHANGE, rak_args);
    request.send(puppet_master_->getInterface());

    // Team change cancels respawn request so spawn selection is
    // re-enabled
    respawn_request_sent_ = false;
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::sendRespawnRequest(unsigned base)
{
    if (respawn_input_blocked_) return;
    
    RakNet::BitStream args;

    args.Write(puppet_master_->getLocalPlayer()->getId());
    args.Write((uint8_t)base);
    
    network::CustomClientCmd request(CCCT_REQUEST_RESPAWN, args);
    request.send(puppet_master_->getInterface());

    respawn_request_sent_ = true;
}




//------------------------------------------------------------------------------
const SystemAddress & GameLogicClientCommon::getTrackedPlayer() const
{
    return spectator_camera_.getTargetPlayer();
}



//------------------------------------------------------------------------------
/**
 *  Don't draw tank's turret if in local view. In the future, disable
 *  own shadow, own smoke etc, adapt engine sound to play at local pos
 *  etc...
 */
void GameLogicClientCommon::enableLocalView(bool e)
{
    if (!spectator_camera_.getTargetBody()) return;
    TankVisual * tv = dynamic_cast<TankVisual*>((GameObjectVisual*)spectator_camera_.getTargetBody()->getUserData());
    if (!tv) return;

    if(hud_.get()) hud_->enable(e);
    tv->enableInternalView(e);    
}




//------------------------------------------------------------------------------
void GameLogicClientCommon::onLocalControllableSet()
{
    show_respawn_text_ = false;
    
    setInputMode(IM_CONTROL_TANK);
    spectator_camera_.setMode(CM_TRACKING_1ST);
    spectator_camera_.trackPlayer(puppet_master_->getLocalPlayer()->getId());

    if (!game_won_)
    {
        // remove respawn message
        puppet_master_->getHud()->setStatusLine("");
    }

    Controllable * controllable = puppet_master_->getLocalPlayer()->getControllable();
    if (controllable) hud_->setAttitudeTexture(controllable->getName());
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::onKill(const SystemAddress & killer,
                                   const SystemAddress & killed,
                                   PLAYER_KILL_TYPE kill_type,
                                   bool assist)
{
    assert(killed != UNASSIGNED_SYSTEM_ADDRESS);
    if (killed == UNASSIGNED_SYSTEM_ADDRESS) return;
    
    if (killer == killed)
    {
        // suicide is always signaled by killer "UNASSIGNED_SYSTEM_ADDRESS"
        s_log << Log::warning
              << "killer == killed in GameLogicClientCommon::onKill: "
              << killer
              << ", assist: "
              << assist
              << "\n";
        
        return;
    }
    

    // check kill type
    std::string kill_type_msg;
    switch(kill_type)
    {
    case PKT_WEAPON1:
        kill_type_msg = " (SC) ";
        break;
    case PKT_WEAPON2:
        kill_type_msg = " (AP) ";
        break;
    case PKT_WEAPON3:
        kill_type_msg = " (HI) ";
        break;
    case PKT_WEAPON4:
        kill_type_msg = " (SOC) ";
        break;
    case PKT_MINE:
        kill_type_msg = " (Mine) ";
        break;
    case PKT_MISSILE:
        kill_type_msg = " (Missile) ";
        break;
    case PKT_MACHINE_GUN:
        kill_type_msg = " (MG) ";
        break;
    case PKT_FLAME_THROWER:
        kill_type_msg = " (Flame) ";
        break;
    case PKT_LASER:
        kill_type_msg = " (Laser) ";
        break;
    case PKT_COLL_DAMAGE:
        kill_type_msg = " (Collision) ";
        break;
    case PKT_WRECK_EXPLOSION:
        kill_type_msg = " (Explosion) ";
        break;
    default:
        kill_type_msg = " ";
        break;
    }



    std::string hud_msg;
    Player * player=NULL,*player2=NULL;

    Color color = Color(1.0f, 1.0f, 1.0f);


    if (killer == puppet_master_->getLocalPlayer()->getId())
    {
        // We killed somebody
        player = puppet_master_->getRemotePlayer(killed);
        if (player) 
        {
            if(assist)
            {
                hud_msg = "Kill assist for " + player->getName();
                color = MC_KILL_DAMAGE_ASSIST;
            } else
            {
                hud_msg = puppet_master_->getLocalPlayer()->getName() + kill_type_msg + player->getName();

                bool teamkill = score_.getNumTeams() > 1 &&
                    (score_.getTeam(puppet_master_->getLocalPlayer()->getId()) ==
                     score_.getTeam(player->getId()));
                
                if (teamkill)
                {
                    color = MC_OWN_TEAMKILL;
                    hud_msg += "\nYou just killed your own teammate!";
                } else color = MC_KILL;
            }
        } else s_log << Log::warning << "Killed unknown player " << killed << ".\n";
        
    } else if (killed == puppet_master_->getLocalPlayer()->getId())
    {            
        // We were killed
        if (killer == UNASSIGNED_SYSTEM_ADDRESS)
        {
            // ... by self
            hud_msg = "You commited suicide" + kill_type_msg;
            color = MC_SUICIDE;
        } else
        {
            // .. by other player
            player = puppet_master_->getRemotePlayer(killer);
            if (player)
            {
                hud_msg = player->getName() + kill_type_msg + puppet_master_->getLocalPlayer()->getName();
                color = MC_KILLED;
            }
            else s_log << Log::warning << "Killed by unknown player " << killer << ".\n";
        }

        onLocalPlayerKilled(player);
    } else
    {
        // Somebody killed smbdy else
        player2 = puppet_master_->getRemotePlayer(killed);
        if (player2)
        {
            if (killer == UNASSIGNED_SYSTEM_ADDRESS)
            {
                // It was a suicide
                hud_msg = player2->getName() + " committed suicide" + kill_type_msg;
                color = MC_OTHER_SUICIDE;
            } else
            {
                // It was murder
                player = puppet_master_->getRemotePlayer(killer);
                if (player)
                {
                    hud_msg = player->getName() + kill_type_msg + player2->getName();
                        
//                     Team * killer_team = score_.getTeam(player->getId());
//                     assert(killer_team);
//                     color = killer_team->getColor();
                        
                } else
                {
                    s_log << Log::warning
                          << player2->getName()
                          << " has been killed by unknown player "
                          << killer
                          << ".\n";
                }
            }
        } else
        {
            s_log << Log::warning << "Killed player " << killed << " is unknown\n";
        }
    }
    

    if (!hud_msg.empty()) puppet_master_->getHud()->addMessage(hud_msg, color);    
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::onLocalPlayerKilled(Player * killer)
{
    // free camera is automatically used if there is no
    // controllable.
    setupDeathCam(killer ? killer->getControllable() : NULL,
                  puppet_master_->getLocalPlayer()->getControllable());

    startRespawnCounter();

    setInputMode(IM_SELECT_SPAWN_STAGE);
    task_change_to_spawn_selection_ = s_scheduler.addEvent(
        SingleEventCallback(this, &GameLogicClientCommon::changeToSpawnSelectionCam),
        s_params.get<float>("server.logic.spawn_select_delay"),
        NULL,
        "GameLogicClientBeaconstrike::changeToSpawnSelectionCam",
        &fp_group_);

    blockRespawnInput();
}



//------------------------------------------------------------------------------
/**
 *  Add compass directions.
 */
void GameLogicClientCommon::onRepopulateMinimap()
{
    const float MINIMAP_COMPASS_ICON_OFFSET = 100000.0f;

    const terrain::TerrainData * td = puppet_master_->getGameState()->getTerrainData();
    float terrain_size_x =  td->getHorzScale()*td->getResX();
    float terrain_size_z =  td->getHorzScale()*td->getResZ();

    Vector center(terrain_size_x*0.5f, 0.0f, terrain_size_z*0.5f);
    
    Matrix t(true);
    
    t.getTranslation() = center + Vector(0.0f, 0.0f, -MINIMAP_COMPASS_ICON_OFFSET);
    hud_->getMinimap()->addIcon(t, "north.dds");
    t.getTranslation() = center + Vector(0.0f, 0.0f, MINIMAP_COMPASS_ICON_OFFSET);
    hud_->getMinimap()->addIcon(t, "south.dds");
    t.getTranslation() = center + Vector(MINIMAP_COMPASS_ICON_OFFSET, 0.0f, 0.0f);
    hud_->getMinimap()->addIcon(t, "east.dds");
    t.getTranslation() = center + Vector(-MINIMAP_COMPASS_ICON_OFFSET, 0.0f, 0.0f);
    hud_->getMinimap()->addIcon(t, "west.dds");
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::onMatchStatsReceived(RakNet::BitStream & args)
{
}



//------------------------------------------------------------------------------
/**
 *  Activates the hit marker if somebody fired on us, showing the
 *  proper direction.
 *
 *  \param shooter The guy who fired on us, if any
 *  \param player_hit The player which was hit. Do nothing if any
 *  other than the local player was hit.
 *
 *  \param pos The hit spot in world coordinates.
 *
 *  \param type If we drive into a mine, we don't want to show the
 *  direction to the guy who placed it...
 */
void GameLogicClientCommon::activateHitMarker(const SystemAddress & shooter,
                                                 const SystemAddress & player_hit,
                                                 Vector pos,
                                                 WEAPON_HIT_TYPE type)
{
    // Bail if we are not the one hit or our tank is gone for some
    // reason
    if(player_hit != puppet_master_->getLocalPlayer()->getId()) return;

    // Now determine whether to display the direction to the hitting
    // player or the direction to the hit position (in case of splash
    // damage dealt to oneself or if the shooting player doesn't exist
    // anymore)
    RemotePlayer * shooting_player = puppet_master_->getRemotePlayer(shooter);
    if (shooting_player && shooting_player->getControllable() &&
        type != WHT_MINE)
    {
        pos = shooting_player->getControllable()->getPosition();
    }

    hud_->activateHitMaker(pos);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::activateHitFeedback( const SystemAddress & shooter,
                                                 const SystemAddress & player_hit,
                                                 WEAPON_HIT_TYPE type)
{
    // Bail if we are not the one shooting or our tank is gone for some
    // reason
    if(shooter != puppet_master_->getLocalPlayer()->getId()) return;
    if(shooter == player_hit || player_hit == puppet_master_->getLocalPlayer()->getId()) return;

    // Avoid hit feedback if player hit is already dead, or for some reason gone
    RemotePlayer * remote_player = puppet_master_->getRemotePlayer(player_hit);
    if(!remote_player) return;
    if(!remote_player->getControllable()) return;
    

    switch(type)
    {
    case WHT_PROJECTILE:
    case WHT_PROJECTILE_SPLASH:
    case WHT_PROJECTILE_SPLASH_INDIRECT:
        hud_->activateHitFeedback();
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::setupDeathCam(Controllable * killer,
                                             Controllable * killed)
{
    if (spectator_camera_.getMode() != CM_TRACKING_1ST) return;
    
    if (!killed)
    {
        s_log << Log::warning
              << "We have been killed but have no controllable assigned.\n";
        return;
    }
    
    
    const Vector OFFSET = Vector(0,1,3);
    ControllableVisual * visual = (ControllableVisual*)killed->getUserData();
    Matrix new_pos = visual->getTrackingPos(Vector(0,0,0));
    if (!killer)
    {
        // Suicide
        new_pos.getTranslation() += new_pos.transformVector(OFFSET);        
        new_pos.loadOrientation(killed->getPosition() - new_pos.getTranslation(), Vector(0,1,0));
    } else
    {
        // We have been killed, move camera so it new_poss killer
        Vector dir = killer->getPosition() - (killed->getPosition() + Vector(0,OFFSET.y_,0));

        new_pos.loadOrientation(dir, Vector(0,1,0));
        new_pos.getTranslation() += OFFSET.z_*new_pos.getZ();
    }
    
    spectator_camera_.trackPlayer(killed->getOwner());
    spectator_camera_.setMode(CM_TRACKING_3RD_CONSTANT_DIR);
    spectator_camera_.setTransform(new_pos);
}




//------------------------------------------------------------------------------
void GameLogicClientCommon::changeToSpawnSelectionCam(void*)
{
    task_change_to_spawn_selection_ = INVALID_TASK_HANDLE;
    
    if (respawn_request_sent_) return;
    
    setInputMode(IM_SELECT_SPAWN_STAGE);    
    spectator_camera_.setMode(CM_FREE);
    spectator_camera_.setTransform(selectValidSpawnStage(0));
}


//------------------------------------------------------------------------------
void GameLogicClientCommon::startRespawnCounter()
{
    respawn_time_ = (unsigned)s_params.get<float>("server.logic.spawn_delay")+1;
    respawn_request_sent_ = false;
    task_respawn_counter_ = s_scheduler.addTask(
        PeriodicTaskCallback(this, &GameLogicClientCommon::handleRespawnCounter),
                             1.0f,
                             "GameLogicClientCommon::handleRespawnCounter",
                             &fp_group_);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::handleRespawnCounter(float dt)
{
    respawn_time_ -= (unsigned)dt;

    std::string msg;

    if (show_respawn_text_)
    {
        if ((int)respawn_time_ <= 0)
        {
            s_scheduler.removeTask(task_respawn_counter_, &fp_group_);
            task_respawn_counter_ = INVALID_TASK_HANDLE;
        
            if (respawn_request_sent_)
            {
                msg = "Respawning...";
            } else
            {
                msg = "Press fire to respawn";
            }
        } else
        {
            if (respawn_request_sent_)
            {
                msg = "Respawning in ";
            } else
            {
                msg = "Press fire to respawn in ";
            }
            msg += toString(respawn_time_);
            msg += " second";
            if (respawn_time_!=1) msg += "s";
        }

        if (!respawn_request_sent_)
        {
            try
            {
                msg += (" or '" +
                        s_input_handler.getKeyForFunction("Show Tank Equipment Screen") +
                        "' to show the equipment menu.");
            } catch (input_handler::KeyNotRegisteredException & e)
            {
            }
        } else
        {
            if (!msg.empty()) msg += ". ";
        }


        if (input_mode_ == IM_SELECT_SPAWN_STAGE && !respawn_request_sent_)
        {
            msg += ("Use '" +
                    s_input_handler.getKeyForFunction("Turn left") +
                    "' and '" +
                    s_input_handler.getKeyForFunction("Turn right") +
                    "' to select spawning site.");
        }
    }
    
    puppet_master_->getHud()->setStatusLine(msg);
}



//------------------------------------------------------------------------------
void GameLogicClientCommon::blockRespawnInput()
{
    respawn_input_blocked_ = true;
    s_scheduler.addEvent(
        SingleEventCallback(this, &GameLogicClientCommon::unblockRespawnInput),
        RESPAWN_BLOCK_TIME,
        NULL,
        "GameLogicClientCommon::unblockRespawnRequests",
        &fp_group_);
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::unblockRespawnInput(void *)
{
    respawn_input_blocked_ = false;
}

//------------------------------------------------------------------------------
void GameLogicClientCommon::gameWon()
{
    game_won_ = true;
    
    s_scheduler.removeTask(task_respawn_counter_, &fp_group_);
    task_respawn_counter_ = INVALID_TASK_HANDLE;
    s_scheduler.removeTask(task_change_to_spawn_selection_, &fp_group_);
    task_change_to_spawn_selection_ = INVALID_TASK_HANDLE;

    respawn_input_blocked_ = true;
}
