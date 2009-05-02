
#include "GameLogicClientBeaconstrike.h"


#include "physics/OdeSimulator.h"


#include "AutoRegister.h"
#include "Beacon.h"
#include "BeaconVisual.h"
#include "BeaconBoundaryClient.h"
#include "SoundManager.h"
#include "SoundSource.h"
#include "GameHud.h"
#include "../Tank.h"
#include "../GameHudTank.h"
#include "../GUITeamSelect.h"
#include "../GUIScore.h"
#include "../TankMineVisual.h"
#include "LevelData.h"
#include "TerrainDataClient.h"
#include "TerrainVisual.h"
#include "Paths.h"
#include "ReaderWriterBbm.h"
#include "OsgNodeWrapper.h"
#include "NetworkCommandClient.h"
#include "InputHandler.h"
#include "SpawnStageClient.h"

#include "GameLogicServerBeaconstrike.h" // for network msg types

#include "../HudMessageColors.h"

#ifdef ENABLE_DEV_FEATURES
REGISTER_CLASS(GameLogicClient, GameLogicClientBeaconstrike);
#endif


//------------------------------------------------------------------------------
GameLogicClientBeaconstrike::GameLogicClientBeaconstrike()
{
    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicClientBeaconstrike::updateBoundary),
                        1.0f / s_params.get<float>("client.logic.update_boundary_fps"),
                        "GameLogicClientBeaconstrike::updateBoundary",
                        &fp_group_);
}



//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::init(PuppetMasterClient * master)
{
    GameLogicClientCommon::init(master);
    
    team_.resize(NUM_TEAMS_BS);
    std::vector<Team*> team_for_sel;
    for (unsigned t=0; t<NUM_TEAMS_BS; ++t)
    {
        team_[t].setId(t);
        team_[t].setConfigName(TEAM_CONFIG_BS[t]);
        score_.addTeam(&team_[t]);

        team_for_sel.push_back(&team_[t]);
    }

    // XXXX somewhat hacky, addTeam function in GUITeamSelect would be better...
    gui_teamselect_.reset(new GUITeamSelect(master, team_for_sel));
    gui_score_->setTeams(team_for_sel);
}


//------------------------------------------------------------------------------
/**
 *  \return Whether the input was processed. Used to determine whether
 *  to pass the input to the controllable.
 */
bool GameLogicClientBeaconstrike::handleInput(PlayerInput & input)
{
    if (GameLogicClientCommon::handleInput(input)) return true;
    
    if (input_mode_ == IM_CONTROL_TANK)
    {
        Tank * tank = dynamic_cast<Tank*>(puppet_master_->getLocalPlayer()->getControllable());
        if(tank)
        {
            // pickup / drop beacon
            if (input.action1_)
            {    
                if (tank->getCarriedObject() && !tank->isBeaconActionExecuted())
                {
                    // check if tank is to fast for dropping beacon, brake it
                    Vector velocity = tank->getTarget()->getLocalLinearVel();
                    if(velocity.z_ < 0)
                    {
                        input.up_   = false;
                        input.down_ = true;
                    } else
                    {
                        input.up_   = true;
                        input.down_ = false;                    
                    }
                }
            } else
            {
                tank->setBeaconActionExecuted(false);
            }
        } else if (score_.getPlayerScore(puppet_master_->getLocalPlayer()->getId())->getTeam() &&
                   input.fire1_)
        {
            sendRespawnRequest(selected_spawn_stage_);
        }
    }

    return false;
}

//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::loadLevel(const std::string & lvl_name)
{
    GameLogicClientCommon::loadLevel(lvl_name);

    for (unsigned t=0; t<NUM_TEAMS_BS; ++t)
    {
        team_[t].createBoundary(puppet_master_->getGameState()->getSimulator()->getStaticSpace());
    }
    
    setInputMode(IM_CONTROL_CAMERA);    
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onRequestReady()
{
    gui_teamselect_->show(true);
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::executeCustomCommand(unsigned type, RakNet::BitStream & args)
{
    switch (type)
    {
    case CSCTBS_PICKUP_BEACON:
        pickupBeacon(args);
        break;
    case CSCTBS_DROP_BEACON:
        dropBeacon(args);
        break;
    case CSCTBS_BEACON_DESTROYED:
        beaconExplosion(args);
        break;
    case CSCTBS_BEACON_CONNECTIONS:
        beaconConnections(args);
        break;
    case CSCTBS_BASE_CONQUERED:
        stageConquered(args);
        break;
    case CSCTBS_GAME_WON:
        gameWon(args);
        break;
    default:
        GameLogicClientCommon::executeCustomCommand(type, args);
    }    
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onGameObjectAdded(GameObject * object)
{
    GameLogicClientCommon::onGameObjectAdded(object);

    
    if (object->getType() == "Beacon")
    {
        Beacon * beacon = (Beacon*)object;
        TEAM_ID team = beacon->getTeamId();

        if (team != INVALID_TEAM_ID)
        {
            team_[team].getBoundary()->addBeacon(beacon);
        } else
        {
            s_log << Log::warning
                  << "Beacon "
                  << *object
                  << " has no team\n";
        }

        beacon->addObserver(ObserverCallbackFun2(this, &GameLogicClientBeaconstrike::onBeaconDeployedChanged),
                            BE_DEPLOYED_CHANGED, &fp_group_);

        beacon->addObserver(ObserverCallbackFun2(this, &GameLogicClientBeaconstrike::onBeaconScheduledForDeletion),
                            GOE_SCHEDULED_FOR_DELETION, &fp_group_);


        beacon->addObserver(ObserverCallbackFun2(this, &GameLogicClientBeaconstrike::onBeaconHealthChanged),
                            BE_HEALTHY, &fp_group_);
        beacon->addObserver(ObserverCallbackFun2(this, &GameLogicClientBeaconstrike::onBeaconHealthChanged),
                            BE_UNDER_ATTACK, &fp_group_);
        beacon->addObserver(ObserverCallbackFun2(this, &GameLogicClientBeaconstrike::onBeaconHealthChanged),
                            BE_HEALTH_CRITICAL, &fp_group_);

    } 
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onLevelObjectLoaded(const std::string & type,
                                                      const bbm::ObjectInfo & info)
{
    GameLogicClientCommon::onLevelObjectLoaded(type, info);
    
    if (type == "HelperObject")
    {
        if (info.params_.get<std::string>("logic.role") == "spawn_camera")
        {
            // stage index is 1--based!
            unsigned stage_no = info.params_.get<unsigned>("logic.stage");
            assert(stage_no > 0);
            stage_.resize(std::max(stage_.size(), stage_no));
            stage_[stage_no-1].setCameraPosition(info.transform_);
        }
    } else if (type == "Beacon")
    {
        // look for defender beacon, set stage owner accordingly.
        if (info.params_.get<std::string>("logic.role") == "fixed_beacon" &&
            info.params_.get<std::string>("logic.team") == "defender")
        {
            unsigned stage_no = info.params_.get<unsigned>("logic.stage");
            assert(stage_no > 0);
            stage_.resize(std::max(stage_.size(), stage_no));

            stage_[stage_no-1].setOwnerTeam(TEAM_ID_DEFENDER);
        }
    }
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onTeamAssignmentChanged(Player * player)
{
    if (player == puppet_master_->getLocalPlayer())
    {
        s_scheduler.removeTask(task_change_to_spawn_selection_, &fp_group_);
        task_change_to_spawn_selection_ = INVALID_TASK_HANDLE;
    
        TEAM_ID team_id = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());

        // also update mine warning billboards
        TankMineVisual::setLocalPlayerTeam(team_id);
        
        if (team_id != INVALID_TEAM_ID)
        {
            changeToSpawnSelectionCam(NULL);

            // Don't spend time updating the outline geometry for enemy boundary
            team_[team_id]  .getBoundary()->setUpdateOutline(true);    
            team_[1-team_id].getBoundary()->setUpdateOutline(false);

            show_respawn_text_ = true;
        } else
        {
            setInputMode(IM_CONTROL_CAMERA);
        
            show_respawn_text_ = false;
        }

        handleRespawnCounter(0.0f);        
    }



    Team * team = score_.getTeam(player->getId());

    if (player == puppet_master_->getLocalPlayer())
    {
        if (team == NULL)
        {
            puppet_master_->getHud()->addMessage("You have been assigned to no team.");
        } else
        {
            puppet_master_->getHud()->addMessage(std::string("You have been assigned to ") +
                                                 team->getName() +
                                                 ".",
                                                 team->getColor());
        }
    } else if (puppet_master_->getLocalPlayer()->isLevelDataSet())
    {
        // Notification if other player changes team
        if (team)
        {
            puppet_master_->getHud()->addMessage(player->getName()   +
                                                 " has been assigned to " +
                                                 team->getName() +
                                                 ".",
                                                 team->getColor());
        } else
        {
            puppet_master_->getHud()->addMessage(player->getName() + " has been assigned to no team.");
        }
    }    
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onLocalControllableSet()
{
    GameLogicClientCommon::onLocalControllableSet();
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onKill(const SystemAddress & killer,
                                         const SystemAddress & killed,
                                         PLAYER_KILL_TYPE kill_type,
                                         bool assist)
{
    std::string hud_msg;
    Color color(1.0f,1.0f,1.0f);
    Player * player=NULL;
    
    if (kill_type == PKT_UNKNOWN1)
    {
        // A beacon was destroyed.
        if (killer == puppet_master_->getLocalPlayer()->getId())
        {
            if (score_.getTeamId(killer) == TEAM_ID_ATTACKER)
            {
                hud_msg = "You just destroyed your own beacon!";
                color = MC_OWN_BEACON_DESTROYED;
            } else
            {            
                if (assist)
                {
                    hud_msg = "You helped destroy a beacon!";
                    color = MC_BEACON_ASSIST;
                } else
                {
                    hud_msg = "You destroyed a beacon!";
                    color = MC_BEACON_DESTROYED;
                }
            }
        } else
        {
            player = puppet_master_->getRemotePlayer(killer);
            if (player)
            {
                hud_msg = player->getName() + " has destroyed a beacon!";

                TEAM_ID local_team_id = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());
                if (local_team_id == TEAM_ID_ATTACKER)
                {
                    color = MC_BEACON_DESTROYED_OTHER_ATTACKER;
                } else if (local_team_id == TEAM_ID_DEFENDER)
                {
                    color = MC_BEACON_DESTROYED_OTHER_DEFENDER;
                }
            }
        }


        puppet_master_->getHud()->addMessage(hud_msg);
        
    } else GameLogicClientCommon::onKill(killer, killed, kill_type, assist);
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::handleMinimapIcon(RigidBody * body, bool force_reveal)
{
    hud_->getMinimap()->removeIcon(body);

    Team * own_team = score_.getTeam(puppet_master_->getLocalPlayer()->getId());
    if (!own_team) return;
    
    Beacon * b;
    if ((b = dynamic_cast<Beacon*>(body)))
    {
        if (b->getTeamId() == own_team->getId() || b->getState() == BS_FIXED || force_reveal)
        {
            Team * beacon_team = score_.getTeamScore(b->getTeamId())->getTeam();
            if (b->isDeployed())
            {
                hud_->getMinimap()->addIcon(body, 
                    s_params.get<std::string>(beacon_team->getConfigName() + ".minimap_icon_beacon"));
            } else
            {
                hud_->getMinimap()->addIcon(body, 
                    s_params.get<std::string>(beacon_team->getConfigName() + ".minimap_icon_beacon_inactive"));
            }


            if (b->getHealthState() != BHS_HEALTHY)
            {
                hud_->getMinimap()->enableFlash(b, true);
            }
            
            return;
        } 
    }

    if (dynamic_cast<Tank*>(body))
    {
        if (score_.getTeamId(body->getOwner()) == own_team->getId() || force_reveal)
        {
            hud_->getMinimap()->addIcon(body, "tank_" + body->getName() + ".dds");
            return;
        }
    }
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::repopulateMinimap()
{
    GameLogicClientCommon::repopulateMinimap();

    TEAM_ID local_player_team_id = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());
    
    if(local_player_team_id != INVALID_TEAM_ID)
    {
        hud_->getMinimap()->addNode(team_[local_player_team_id].getBoundary()->getOutlineNode());
    }    
}


//------------------------------------------------------------------------------
Matrix GameLogicClientBeaconstrike::selectValidSpawnStage(int delta)
{   
    TEAM_ID team = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());
    assert(team != INVALID_TEAM_ID);

    assert(!stage_.empty());
    if(selected_spawn_stage_ >= stage_.size()) selected_spawn_stage_ = 0;

    if (delta == 0)
    {
        if (stage_[selected_spawn_stage_].getOwnerTeam() == team)
        {
            return stage_[selected_spawn_stage_].getCameraPosition();
        } else delta = 1;
    }

    unsigned start_stage = selected_spawn_stage_;
    
    do
    {
        // avoid % issues with neg. numbers...
        selected_spawn_stage_ = (selected_spawn_stage_+stage_.size()+delta) % stage_.size();


        // avoid endless loop...
        if (selected_spawn_stage_ == start_stage)
        {
            if (stage_[selected_spawn_stage_].getOwnerTeam() == team)
            {
                return stage_[selected_spawn_stage_].getCameraPosition();
            }
            
            assert(false && !"No valid spawn stage found");
            return Matrix(true);
        }
    } while(stage_[selected_spawn_stage_].getOwnerTeam() != team);

    return stage_[selected_spawn_stage_].getCameraPosition();
}




//------------------------------------------------------------------------------
GameLogicClient * GameLogicClientBeaconstrike::create()
{
    return new GameLogicClientBeaconstrike();
}




//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::pickupBeacon(RakNet::BitStream & args)
{
    uint16_t tank_id, beacon_id;

    args.Read(tank_id);
    args.Read(beacon_id);

    Tank * tank = dynamic_cast<Tank*>(puppet_master_->getGameState()->getGameObject(tank_id));
    Beacon * beacon = dynamic_cast<Beacon*>(puppet_master_->getGameState()->getGameObject(beacon_id));

    if (!tank || !beacon)
    {
        s_log << Log::warning
              << "Received invalid tank/beacon combination in GameLogicClientBeaconstrike::pickupBeacon\n";
        return;
    }

    tank->pickupObject(beacon);
    beacon->setCarried(true);
    tank->setBeaconActionExecuted(true); //pickup successful
    
    // play SFX on beacon pickup
    std::string sound = s_params.get<std::string>("sfx.beacon_pickup");
    s_soundmanager.playSimpleEffect(sound, beacon->getPosition());
   

    s_log << Log::debug('l')
          << "Pickup beacon command received for "
          << *tank
          << " and "
          << *beacon
          << "\n";
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::dropBeacon(RakNet::BitStream & args)
{
    uint16_t tank_id;

    args.Read(tank_id);


    Tank * tank = dynamic_cast<Tank*>(puppet_master_->getGameState()->getGameObject(tank_id));
    Beacon * beacon = (Beacon*)tank->getCarriedObject();

    if (!tank)
    {
        s_log << Log::warning
              << "Received invalid tank in GameLogicClientBeaconstrike::dropBeacon\n";
        return;
    }

    if (!beacon)
    {
        s_log << Log::warning
              << "Tank has no beacon to drop in GameLogicClientBeaconstrike::dropBeacon\n";
        return;
    }

    
    tank->dropObject();
    beacon->setCarried(false);
    tank->setBeaconActionExecuted(true); //drop successful
    
    // play SFX on beacon drop
    std::string sound = s_params.get<std::string>("sfx.beacon_drop");
    s_soundmanager.playSimpleEffect(sound, beacon->getPosition());
    

    s_log << Log::debug('l')
          << "Drop beacon command received for "
          << *tank
          << "\n";
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::beaconExplosion(RakNet::BitStream & args)
{
    uint16_t beacon_id;
    args.Read(beacon_id);

    Beacon * beacon = dynamic_cast<Beacon*>(puppet_master_->getGameState()->getGameObject(beacon_id));
    if (!beacon)
    {
        s_log << Log::warning
              << "Ignoring beaconExplosion for invalid beacon "
              << beacon_id
              << "\n";
        return;
    }

    BeaconVisual * beacon_visual = (BeaconVisual*)beacon->getUserData();
    beacon_visual->destroy();


    if (beacon->getTeamId() == score_.getTeamId(puppet_master_->getLocalPlayer()->getId()))
    {
        puppet_master_->getHud()->addMessage("Beacon lost.", MC_BEACON_LOST);
    }    
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::beaconConnections(RakNet::BitStream & args)
{
    for (unsigned i=0; i<NUM_TEAMS_BS; ++i)
    {
        if (!team_[i].getBoundary())
        {
            s_log << Log::warning
                  << "Received beacon connection while boundary is still NULL\n";
            return;
        }
    }
    
    std::vector<std::pair<Beacon*, Beacon*> > connections[NUM_TEAMS_BS];

    uint16_t num_connections;
    args.Read(num_connections);


    for (unsigned c=0; c<num_connections; ++c)
    {
        uint16_t id1, id2;
        args.Read(id1);
        args.Read(id2);

        Beacon * beacon1 = dynamic_cast<Beacon*>(puppet_master_->getGameState()->getGameObject(id1));
        Beacon * beacon2 = dynamic_cast<Beacon*>(puppet_master_->getGameState()->getGameObject(id2));

        if (!beacon1 || !beacon2)
        {
            s_log << Log::warning
                  << "Received invalid beacon in GameLogicClientBeaconstrike::beaconConnections\n";
            continue;
        }

        if ((beacon1->getTeamId() == INVALID_TEAM_ID && beacon2->getTeamId() == INVALID_TEAM_ID) ||
            beacon1->getTeamId() != beacon2->getTeamId() &&
            beacon1->getTeamId() != INVALID_TEAM_ID    &&
            beacon2->getTeamId() != INVALID_TEAM_ID)
        {
            s_log << Log::warning
                  << "Received invalid beacon combination in GameLogicClientBeaconstrike::beaconConnections: "
                  << *beacon1
                  << ", "
                  << *beacon2
                  << "\n";
            continue;            
        }

        TEAM_ID team = beacon1->getTeamId() == INVALID_TEAM_ID ? beacon2->getTeamId() : beacon1->getTeamId();
        
        connections[team].push_back(std::make_pair(beacon1, beacon2));
    }
    
    for (unsigned i=0;i<NUM_TEAMS_BS; ++i)
    {
        team_[i].getBoundary()->updateBeaconConnections(connections[i]);
    }
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::stageConquered(RakNet::BitStream & args)
{
    uint8_t stage;

    args.Read(stage);    

    Color color(1.0f,1.0f,1.0f);
    
    TEAM_ID local_team_id = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());
    if (local_team_id == TEAM_ID_ATTACKER)
    {
        color = MC_BASE_CONQUERED_ATTACKER;
    } else if (local_team_id == TEAM_ID_DEFENDER)
    {
        color = MC_BASE_CONQUERED_DEFENDER;
    }
    puppet_master_->getHud()->addMessage("A base has been conquered.", color);
        
    const SystemAddress & local_player = puppet_master_->getLocalPlayer()->getId();

    Team * team = score_.getTeam(local_player);

    SoundSource * snd = NULL;
    if(team == NULL ||
       team->getId() == TEAM_ID_ATTACKER )
    {
        snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.attacker_conquers_base"),
                                              s_soundmanager.getListenerInfo().position_);
    } else
    {
        snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.defender_loses_base"),
                                              s_soundmanager.getListenerInfo().position_);
    }
    snd->setRolloffFactor(0.0f);

    if (stage >= stage_.size())
    {
        s_log << Log::error
              << "stage "
              << (unsigned)stage
              << " was conquered but doesn't exist on client.";
    } else
    {
        stage_[stage].setOwnerTeam(TEAM_ID_ATTACKER);
    }
}




//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::gameWon(RakNet::BitStream & args)
{
    GameLogicClientCommon::gameWon();
    
    TEAM_ID team_id = INVALID_TEAM_ID;

    args.Read(team_id);

    Team * winner_team = score_.getTeam(team_id);
    if (!winner_team)
    {
        s_log << Log::warning
              << "Ignoring gameWon with invalid team\n";
        return;
    }
    
    std::string game_info_label;
    game_info_label += winner_team->getName() + " has won the match!\n";

    s_log << Log::debug('l') << game_info_label;

    puppet_master_->getHud()->setStatusLine(game_info_label);

    gui_score_->show(1.0f);

    SoundSource * snd = NULL;
    if(team_id == TEAM_ID_ATTACKER)
    {
        snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.attacker_wins"),
                                              s_soundmanager.getListenerInfo().position_);
    } else
    {
        snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.defender_wins"),
                                              s_soundmanager.getListenerInfo().position_);
    }
    snd->setRolloffFactor(0.0f);
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onBeaconDeployedChanged(Observable* observable, unsigned event)
{
    assert(event == BE_DEPLOYED_CHANGED);
    Beacon * beacon = (Beacon*)observable;
    assert(beacon->getTeamId() != INVALID_TEAM_ID);
    
    // If beacon was in minimap previously, re-add it with new active
    // state
    if (hud_->getMinimap()->removeIcon(beacon))
    {
        handleMinimapIcon(beacon);
    }
}

//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onBeaconScheduledForDeletion(Observable* observable, unsigned event)
{
    Beacon * beacon = (Beacon*)observable;

    TEAM_ID team = beacon->getTeamId();
    
    if (team != INVALID_TEAM_ID)
    {
        team_[beacon->getTeamId()].getBoundary()->deleteBeacon(beacon);
    }
}


//------------------------------------------------------------------------------
void GameLogicClientBeaconstrike::onBeaconHealthChanged(Observable* observable, unsigned event)
{
    Beacon * beacon = (Beacon*)observable;
    
    if (beacon->getTeamId() == score_.getTeamId(puppet_master_->getLocalPlayer()->getId()))
    {
        switch (event)
        {
        case BE_HEALTHY:
            puppet_master_->getHud()->addMessage("One of our beacons has regenerated.", MC_BEACON_REGENERATED);
            hud_->getMinimap()->enableFlash(beacon, false);
            break;
            
        case BE_UNDER_ATTACK:
            puppet_master_->getHud()->addMessage("One of our beacons is losing health.", MC_BEACON_LOSING_HEALTH);
            hud_->getMinimap()->enableFlash(beacon, true);
            break;
            
        case BE_HEALTH_CRITICAL:
            puppet_master_->getHud()->addMessage("One of our beacons is close to destruction.", MC_BEACON_CLOSE_DESTRUCTION);
            break;
        default:
            assert(false);
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  Boundary gets updated when a beacon changes its active status, but
 *  only at a max FPS.
 */
void GameLogicClientBeaconstrike::updateBoundary(float dt)
{
    for (unsigned t=0; t<NUM_TEAMS_BS; ++t)
    {
        if (team_[t].getBoundary()) team_[t].getBoundary()->update();
    }
}

