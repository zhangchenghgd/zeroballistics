
#include "GameLogicClientSoccer.h"

#include "AutoRegister.h"
#include "LevelData.h"
#include "GameHudTankSoccer.h"
#include "GameHud.h"
#include "GUIScoreSoccer.h"
#include "GUIMatchSummarySoccer.h"
#include "GUITeamSelect.h"
#include "GUITankEquipment.h"
#include "GUIHelp.h"
#include "GUIUpgradeSystem.h"
#include "InputHandler.h"
#include "ControllableVisual.h"
#include "TankMineVisual.h"
#include "EffectManager.h"

#include "Tank.h"
#include "WeaponSystem.h"

#include "RankingStatisticsSoccer.h"

#include "GameLogicServerSoccer.h"
#include "SoccerBall.h"

REGISTER_CLASS(GameLogicClient, GameLogicClientSoccer);




//------------------------------------------------------------------------------
GameLogicClientSoccer::GameLogicClientSoccer()
{
}

//------------------------------------------------------------------------------
void GameLogicClientSoccer::init(PuppetMasterClient * master)
{
    GameLogicClientCommon::init(master);

    
    std::vector<Team*> team_for_sel;
    for (unsigned t=0; t<NUM_TEAMS_SOCCER; ++t)
    {
        team_[t].setId(t);
        team_[t].setConfigName(TEAM_CONFIG_SOCCER[t]);
        score_.addTeam(&team_[t]);

        team_for_sel.push_back(&team_[t]);
    }

    gui_teamselect_->setTeams(team_for_sel);
    gui_score_     ->setTeams(team_for_sel);
}


//------------------------------------------------------------------------------
bool GameLogicClientSoccer::handleInput(PlayerInput & input)
{
    if (score_.getTeam(puppet_master_->getLocalPlayer()->getId()) != NULL &&
        !puppet_master_->getLocalPlayer()->getControllable() &&
        input.fire1_ &&
        !respawn_input_blocked_)
    {
        sendRespawnRequest(0);
        // immediately update msg
        handleRespawnCounter(0.0f);        
    }

    return GameLogicClientCommon::handleInput(input);
}



//------------------------------------------------------------------------------
void GameLogicClientSoccer::onRequestReady()
{
    gui_teamselect_->show(true);
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::executeCustomCommand(unsigned type, RakNet::BitStream & args)
{
    switch (type)
    {
    case CSCTS_GAME_WON:
        gameWon(args);
        break;
    case CSCTS_GOAL:
        onGoalScored(args);
        break;
    default:
        GameLogicClientCommon::executeCustomCommand(type, args);
    }    
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::onGameObjectAdded(GameObject * object)
{
    GameLogicClientCommon::onGameObjectAdded(object);

    RigidBody * rigid_body = dynamic_cast<RigidBody*>(object);
    assert(rigid_body);

    if (rigid_body->getType() == "Tank")
    {
        rigid_body->getTarget()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicClientSoccer::onTankClawCollision),
            "claw");        
        rigid_body->getProxy()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicClientSoccer::onTankClawCollision),
            "claw");        
    } else if (rigid_body->getType() == "SoccerBall")
    {
        SoccerBall * soccer_ball = (SoccerBall*)rigid_body;
        soccer_ball->setGameState(puppet_master_->getGameState());

        soccer_ball->getProxy()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicClientSoccer::onSoccerballCollision),
            "ball");        
        
    }
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::createGuiScreens()
{
    gui_help_.          reset(new GUIHelp());
    gui_score_.         reset(new GUIScoreSoccer(puppet_master_));
    gui_teamselect_.    reset(new GUITeamSelect(puppet_master_));
    gui_match_summary_. reset(new GUIMatchSummarySoccer(puppet_master_));
}

//------------------------------------------------------------------------------
void GameLogicClientSoccer::createHud()
{
    hud_.reset(new GameHudTankSoccer(puppet_master_, "hud_tank_soccer.xml"));
    hud_->enable(false);    
}


//------------------------------------------------------------------------------
/**
 *  Remember all spawning postions so they can be cycled in dev mode.
 */
void GameLogicClientSoccer::onLevelObjectLoaded(const std::string & type,
                                                    const bbm::ObjectInfo & info)
{
    GameLogicClientCommon::onLevelObjectLoaded(type, info);
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::onTeamAssignmentChanged(Player * player)
{
    if (player == puppet_master_->getLocalPlayer())
    {
        TEAM_ID team_id = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());

        // also update mine warning billboards
        TankMineVisual::setLocalPlayerTeam(team_id);
        
        if (team_id != INVALID_TEAM_ID)
        {
        } else
        {
            setInputMode(IM_CONTROL_CAMERA);
        }
    }



    Team * team = score_.getTeam(player->getId());

    if (player == puppet_master_->getLocalPlayer())
    {
        if (team == NULL)
        {
            puppet_master_->getHud()->addMessage("You have been assigned to no team.");

            // Hide respawn counter
            show_respawn_text_ = false;
            handleRespawnCounter(0.0f);
        } else
        {
            show_respawn_text_ = true;
            puppet_master_->getHud()->addMessage(std::string("You have been assigned to ") +
                                                 team->getName() +
                                                 ".",
                                                 team->getColor());

            sendRespawnRequest(0);
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
void GameLogicClientSoccer::onLocalControllableSet()
{
    GameLogicClientCommon::onLocalControllableSet();
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::onLocalPlayerKilled(Player * killer)
{
    // free camera is automatically used if there is no
    // controllable.
    setupDeathCam(killer ? killer->getControllable() : NULL,
                  puppet_master_->getLocalPlayer()->getControllable());

    startRespawnCounter();
    sendRespawnRequest(0);
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::handleMinimapIcon(RigidBody * object, bool force_reveal)
{
    hud_->getMinimap()->removeIcon(object);

    if(object->getName() == RB_SOCCERBALL)
    {
        hud_->getMinimap()->addIcon(object, RB_SOCCERBALL + ".dds");
    }

    Team * own_team = score_.getTeam(puppet_master_->getLocalPlayer()->getId());
    if (!own_team) return;
    
    if (object->getType() == "Tank"//  &&
//         (score_.getTeamId(object->getOwner()) == own_team->getId() || force_reveal)
        )
    {
        hud_->getMinimap()->addIcon(object, "tank_" + object->getName() + ".dds");
    }
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::onRepopulateMinimap()
{
}

//------------------------------------------------------------------------------
void GameLogicClientSoccer::onMatchStatsReceived(RakNet::BitStream & args)
{
    network::ranking::StatisticsSoccer stats;
    stats.readFromBitstream(args);


    // read the mapping from ranking id to system address
    std::map<uint32_t, SystemAddress> id_map;
    uint32_t num_players;
    args.Read(num_players);
    for (unsigned i=0; i<num_players; ++i)
    {
        uint32_t ranking_id;
        SystemAddress address;
        args.Read(ranking_id);
        args.Read(address);
        id_map[ranking_id] = address;
    }
    
    GUIMatchSummarySoccer * gmss = dynamic_cast<GUIMatchSummarySoccer*>(gui_match_summary_.get());
    
    std::ostringstream ss_poss0, ss_distr0, ss_poss1, ss_distr1;
    std::string match_text_left, match_text_right;
    match_text_left = "Goals (Blue/Red):\n\nBall possession:\n\nMatch distribution:";
    match_text_right = toString(stats.goals_blue_) + ":" + toString(stats.goals_red_) + "\n\n";

    // write correct ball possession
    ss_poss0 << std::fixed << std::setprecision(2) << (stats.ball_possession_ * 100.0f);
    ss_poss1 << std::fixed << std::setprecision(2) << (1.0f - stats.ball_possession_) * 100.0f;
    match_text_right += ss_poss0.str() + " % : " + ss_poss1.str() + " %\n\n";

    // write correct game distribution
    ss_distr0 << std::fixed << std::setprecision(2) << (stats.game_distribution_) * 100.0f;
    ss_distr1 << std::fixed << std::setprecision(2) << (1.0f - stats.game_distribution_) * 100.0f;
    match_text_right += ss_distr0.str() + " % : " + ss_distr1.str() + " %\n";


    gmss->setMatchSummaryText(match_text_left, match_text_right);

    for (std::map<uint32_t, network::ranking::ScoreStats>::const_iterator it = stats.score_stats_.begin();
         it != stats.score_stats_.end();
         ++it)
    {
        Player * player = puppet_master_->getLocalOrRemotePlayer(id_map[it->first]);
        if (!player) continue;
        
        bool selected = (player->getId() == puppet_master_->getLocalPlayer()->getId());

        std::ostringstream ss_skill, ss_delta;
        ss_skill << std::fixed << std::setprecision(2) << it->second.new_score_;
        ss_delta << std::fixed << std::showpos << std::setprecision(2) << it->second.delta_;
        gmss->addSkillElement(player->getName(),ss_skill.str(), ss_delta.str() ,selected);
        
    }

    gmss->show(1.0f);
}


//------------------------------------------------------------------------------
GameLogicClient * GameLogicClientSoccer::create()
{
    return new GameLogicClientSoccer();
}


//------------------------------------------------------------------------------
physics::CONTACT_GENERATION GameLogicClientSoccer::onTankClawCollision(const physics::CollisionInfo & info)
{
    // Claws collide with ball only.
    if (info.other_geom_->getName() != "ball") return physics::CG_GENERATE_NONE;
    
    Tank * tank = (Tank*)info.this_geom_->getUserData();
    assert(tank);

    
    // Only push the ball towards the center, never outwards!
    // For this, go to tank local coordinates.
    Vector pos = tank->getTransform().transformPointInv(info.pos_);
    Vector n   = tank->getTransform().transformVectorInv(info.n_);

    if (abs(n.x_) > abs(n.y_) &&
        abs(n.x_) > abs(n.z_) &&
        sign(pos.x_) != sign(n.x_)) return physics::CG_GENERATE_NONE;        
  
//    return physics::CG_GENERATE_BOTH;
    return physics::CG_GENERATE_SECOND;
}

//------------------------------------------------------------------------------
/**
 *  Don't let anything be affected by ball on client side for now...
 *  The main reason behind this is that the tank shouldn't be lifted
 *  by the ball proxy under any circumstances (when pushing the ball).
 */
physics::CONTACT_GENERATION GameLogicClientSoccer::onSoccerballCollision(const physics::CollisionInfo & info)
{
//    return physics::CG_GENERATE_BOTH;
    return physics::CG_GENERATE_FIRST;
}


//------------------------------------------------------------------------------
void GameLogicClientSoccer::gameWon (RakNet::BitStream & args)
{
    GameLogicClientCommon::gameWon();

    TEAM_ID own_team = score_.getTeamId(puppet_master_->getLocalPlayer()->getId());
    TEAM_ID winner_team;
    args.Read(winner_team);

    std::string game_info_label;

    if (winner_team == INVALID_TEAM_ID)
    {
        game_info_label = "Round draw.";
    } else if (own_team == INVALID_TEAM_ID)
    {
        game_info_label = " " + team_[winner_team].getName() + " wins.";
    }  else if (winner_team == score_.getTeamId(puppet_master_->getLocalPlayer()->getId()))
    {
        game_info_label = "Congratulations! You are the winner!";
    } else
    {
        game_info_label = "You have lost the match.";
    }

    s_log << Log::debug('l') << game_info_label;

    puppet_master_->getHud()->setStatusLine(game_info_label);

//    gui_score_->show(1.0f);

//     SoundSource * snd = NULL;
//     if(team_id == TEAM_ID_ATTACKER)
//     {
//         snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.attacker_wins"),
//                                               s_soundmanager.getListenerInfo().position_);
//     } else
//     {
//         snd = s_soundmanager.playSimpleEffect(s_params.get<std::string>("sfx.defender_wins"),
//                                               s_soundmanager.getListenerInfo().position_);
//     }
//     snd->setRolloffFactor(0.0f);


    spectator_camera_.setMode(CM_TRACKING_3RD_CONSTANT_DIR);

//     // winning camera
//     if (winning_player && winning_player->getControllable())
//     {
//         const Vector OFFSET = Vector(0,1,3);
//         ControllableVisual * visual = (ControllableVisual*)winning_player->getControllable()->getUserData();
//         Matrix new_pos = visual->getTrackingPos(Vector(0,0,0));

//         new_pos.getTranslation() += new_pos.transformVector(OFFSET);
//         new_pos.loadOrientation(winning_player->getControllable()->getPosition() - new_pos.getTranslation(), Vector(0,1,0));
    
//         spectator_camera_.trackPlayer(winning_player->getId());
//         spectator_camera_.setMode(CM_TRACKING_3RD_CONSTANT_DIR);
//         spectator_camera_.setTransform(new_pos);
//     }    
}

//------------------------------------------------------------------------------
void GameLogicClientSoccer::onGoalScored(RakNet::BitStream & args)
{

    int score_team_a, score_team_b;
    unsigned type_of_goal;
    TEAM_ID goal_scoring_team;
    Vector goal_pos;
    SystemAddress goalgetter;
    SystemAddress assist;

    args.Read(goal_scoring_team);
    args.Read(goalgetter);

    args.Read(assist);
    args.Read(type_of_goal);

    args.ReadVector(goal_pos.x_,
                    goal_pos.y_,
                    goal_pos.z_);
    
    args.Read(score_team_a);
    args.Read(score_team_b);
    
    std::string game_info_label = score_.getTeam(goal_scoring_team)->getName() + " scored! ";

    Player * p_goalgetter = puppet_master_->getLocalOrRemotePlayer(goalgetter);
    Player * p_assist     = puppet_master_->getLocalOrRemotePlayer(assist);

    if(type_of_goal == GTS_NORMAL_GOAL)
    {
        if(p_goalgetter)
        {        
            game_info_label += "Goal: " + p_goalgetter->getName() + " ";
        }
       
        if(p_assist)
        {        
            game_info_label += "Assist: " + p_assist->getName() + " ";
        }
    }
    else if(type_of_goal == GTS_OWN_GOAL)
    {
        if(p_goalgetter)
        {        
            game_info_label += "Own Goal: " + p_goalgetter->getName() + " ";
        }
    }
    else if(type_of_goal == GTS_DEFLECTED_GOAL)
    {
        if(p_assist) ///< due to deflection nominate assistant as goal getter
        {        
            game_info_label += "Goal: " + p_assist->getName() + " ";
        }
    }

    puppet_master_->getHud()->addMessage(game_info_label, Color(1.0f,1.0f,0.0f));

    // add goal score particle effect
    s_effect_manager.createEffect("soccer_goal", goal_pos, Vector(0,1,0), false, s_scene_manager.getRootNode());


    // There currently is no mechanism to set overheating over the
    // network. Cooldown all weapons here.
    Tank * own_tank = dynamic_cast<Tank*>(puppet_master_->getLocalPlayer()->getControllable());
    if (own_tank)
    {
        for (unsigned w=0; w<NUM_WEAPON_SLOTS; ++w)
        {
            WeaponSystem * ws = own_tank->getWeaponSystems()[w];
            if (ws) ws->setCooldownStatus(0.0f);
        }        
    }
}
