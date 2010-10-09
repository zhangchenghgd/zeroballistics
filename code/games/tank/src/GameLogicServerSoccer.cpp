
#include "GameLogicServerSoccer.h"

#include <limits>

#include "AutoRegister.h"
#include "Tank.h"
#include "SpawnPos.h"
#include "NetworkCommandServer.h"
#include "Projectile.h"
#include "Tank.h"
#include "TankEquipment.h"
#include "InstantHitWeapon.h"
#include "RankingMatchEvents.h"
#include "Ranking.h"
#include "SoccerBall.h"
#include "AIPlayerSoccer.h"

#undef min
#undef max

REGISTER_CLASS(GameLogicServer, GameLogicServerSoccer);

const int GOAL_SCORE     =  2;
const int ASSIST_SCORE   =  1;
const int OWN_GOAL_SCORE = -2;





const std::string GOAL_NAME_RED  = "g_red_s";
const std::string GOAL_NAME_BLUE = "g_blue_s";

const std::string FIELD_HALF_NAME_RED  = "half_red_s";
const std::string FIELD_HALF_NAME_BLUE = "half_blue_s";


const float SEND_ABSOLUTE_DELAY = 0.1f;


//------------------------------------------------------------------------------
GameLogicServerSoccer::GameLogicServerSoccer() :
    winner_team_(INVALID_TEAM_ID),
    spawn_pos_ball_(true),
    soccer_ball_(NULL),
    task_spawn_soccer_ball_(INVALID_TASK_HANDLE),
    task_clear_assistant_(INVALID_TASK_HANDLE),
    task_send_absolute_(INVALID_TASK_HANDLE)
{
    last_ball_contact_[0] = UNASSIGNED_SYSTEM_ADDRESS;
    last_ball_contact_[1] = UNASSIGNED_SYSTEM_ADDRESS;

	goal_pos_[0] = Vector(0.0f,0.0f,0.0f);
	goal_pos_[1] = Vector(0.0f,0.0f,0.0f);
}

//------------------------------------------------------------------------------
void GameLogicServerSoccer::init(PuppetMasterServer * master)
{
    GameLogicServerCommon::init(master);

    for (unsigned t=0; t<NUM_TEAMS_SOCCER; ++t)
    {
        team_[t].setId(t);
        team_[t].setConfigName(TEAM_CONFIG_SOCCER[t]);
        score_.addTeam(&team_[t]);
    }
}



//------------------------------------------------------------------------------
void GameLogicServerSoccer::addPlayer(const Player * player, bool newly_connected)
{
    GameLogicServerCommon::addPlayer(player, newly_connected);
    
    if (newly_connected)
    {
        if (state_ == TGS_WON)
        {
            sendGameWon(winner_team_,
                        player->getId(),
                        CST_SINGLE);
        }

        puppet_master_->requestPlayerReady(player->getId());
    }


    // initialize player weapons to soccer game
    PlayerScore * score = score_.getPlayerScore(player->getId());
    assert(score);

    score->active_equipment_[0] = PW_SOCCER;
    score->active_equipment_[1] = SW_SOCCER;
    score->active_equipment_[2] = FS_RAM;
    score->active_equipment_[3] = SS_BOOST;

    sendEquipment(player->getId(), UNASSIGNED_SYSTEM_ADDRESS, true, CST_BROADCAST_ALL);
}


//------------------------------------------------------------------------------
void GameLogicServerSoccer::loadLevel(const std::string & name)
{
    winner_team_ = INVALID_TEAM_ID;
    GameLogicServerCommon::loadLevel(name);

    last_ball_contact_[0] = UNASSIGNED_SYSTEM_ADDRESS;
    last_ball_contact_[1] = UNASSIGNED_SYSTEM_ADDRESS;

    spawnSoccerball(NULL);

    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerSoccer::handleBall),
                        1.0f / s_params.get<float>("physics.fps"),
                        "GameLogicServerSoccer::handleBall",
                        &fp_group_);
    
    s_scheduler.addTask(PeriodicTaskCallback(this, &GameLogicServerSoccer::healTanks),
                        s_params.get<float>("soccer.logic.regeneration_dt"),
                        "GameLogicServerSoccer::healTanks",
                        &fp_group_);    
}

//------------------------------------------------------------------------------
void GameLogicServerSoccer::onGameObjectAdded(GameObject * object)
{
    GameLogicServerCommon::onGameObjectAdded(object);

    RigidBody * rigid_body = dynamic_cast<RigidBody*>(object);
    assert(rigid_body);

    if(rigid_body->getType() == "SoccerBall")
    {
        soccer_ball_ = dynamic_cast<SoccerBall*>(rigid_body);

        soccer_ball_->addObserver(ObserverCallbackFun0(this, &GameLogicServerSoccer::onSoccerBallDestroyed),
                                  GOE_SCHEDULED_FOR_DELETION,
                                  &fp_group_);
        
        soccer_ball_->getTarget()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicServerSoccer::onSoccerballSensorCollision),
            "ball_sensor");

        soccer_ball_->getTarget()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicServerSoccer::onSoccerballCollision),
            "ball");

        soccer_ball_->setGameState(puppet_master_->getGameState());
        
    } else if (rigid_body->getType() == "Tank")
    {
        rigid_body->getTarget()->setCollisionCallback(
            physics::CollisionCallback(this, &GameLogicServerSoccer::onTankClawCollision),
            "claw");        
    }

    // find goal positions, quite annoying iterating all RBs -> goals are inside stadium
    for (std::vector<physics::OdeGeom*>::iterator cur_geom = rigid_body->getTarget()->getGeoms().begin();
         cur_geom != rigid_body->getTarget()->getGeoms().end();
         ++cur_geom)
    {
        if ((*cur_geom)->getName() == GOAL_NAME_RED)
        {
            goal_pos_[TEAM_ID_RED] = (*cur_geom)->getTransform().getTranslation();
        } else if ((*cur_geom)->getName() == GOAL_NAME_BLUE)
        {
            goal_pos_[TEAM_ID_BLUE] = (*cur_geom)->getTransform().getTranslation();
        }
    }

}


//------------------------------------------------------------------------------
void GameLogicServerSoccer::createMatchEventlog()
{
    match_events_.reset(new network::ranking::MatchEventsSoccer());
}


//------------------------------------------------------------------------------
bool GameLogicServerSoccer::onLevelObjectLoaded(RigidBody * obj,
                                                const LocalParameters & params)
{
    if(obj->getName() == "spawn_pos_ball")
    {
        spawn_pos_ball_ = obj->getTransform();
        obj->scheduleForDeletion();
        return true;
    } else return GameLogicServerCommon::onLevelObjectLoaded(obj, params);
}


//------------------------------------------------------------------------------
Matrix GameLogicServerSoccer::getRespawnPos(const SystemAddress & pid,
                                            unsigned stage)
{
    Team * team = score_.getTeam(pid);
    if (!team)
    {
        Exception e;
        e << pid
          << " is assigned to no team in GameLogicServerSoccer::getRespawnPos().\n";
        throw e;
    }
        
    std::vector<SpawnPos*> cur_team_spawn_positions_;

    for(unsigned i=0; i < spawn_pos_.size(); i++)
    {
        if(team->getConfigName() == spawn_pos_[i]->getTeamName())
        {
            cur_team_spawn_positions_.push_back(spawn_pos_[i]);
        }
    }

    return getRandomSpawnPos(cur_team_spawn_positions_);
}


//------------------------------------------------------------------------------
Tank * GameLogicServerSoccer::createNewPlayerTank(const SystemAddress & pid)
{
    TEAM_ID id = score_.getTeamId(pid);
    assert(id != INVALID_TEAM_ID);
    
    RigidBody * rb = createRigidBody(s_params.get<std::string>(team_[id].getConfigName() + ".tank_name"), "Tank");
    return dynamic_cast<Tank*>(rb);
}



//------------------------------------------------------------------------------
void GameLogicServerSoccer::onTankKilled(Tank * tank, const SystemAddress & killer_id)
{
    ServerPlayer * killer = puppet_master_->getPlayer(killer_id);
    ServerPlayer * killed = puppet_master_->getPlayer(tank->getOwner());

    getMatchEvents()->logKill(killer ? killer->getRankingId() : network::ranking::INVALID_USER_ID,
                        killed ? killed->getRankingId() : network::ranking::INVALID_USER_ID);

    // Immediately schedule the player for respawn.
    respawning_player_[tank->getOwner()] = 0;
}


//------------------------------------------------------------------------------
void GameLogicServerSoccer::onProjectileHit(Projectile * projectile,
                                            RigidBody * hit_object,
                                            float hit_percentage,
                                            const physics::CollisionInfo & info,
                                            bool create_feedback)
{
    GameLogicServerCommon::onProjectileHit(projectile, hit_object, hit_percentage, info, create_feedback);

    
    // score & accuracy calculation
    PlayerScore * shooter_score = score_.getPlayerScore(projectile->getOwner());

    // player score is null if firing tank left game while his
    // projectile was in progress
    if (shooter_score && hit_object && hit_object->getType() == "Tank")
    {        
        Tank * tank = (Tank*)hit_object;
        
        shooter_score->shots_++;

        TEAM_ID hit_team_id     = score_.getTeamId(tank->getOwner());
        TEAM_ID shooter_team_id = score_.getTeamId(projectile->getOwner());
        
        if (hit_percentage == 1.0f)
        {
            if (hit_object->getOwner() != UNASSIGNED_SYSTEM_ADDRESS)
            {
                shooter_score->shots_++;
                // do not count hits team members
                if (shooter_team_id != hit_team_id)
                {
                    shooter_score->hits_++;
                }
            } else
            {
                // tank wrecks are neutral w.r.t. accuracy
            }


            // slow down hit tank
            tank->setGlobalLinearVel(tank->getGlobalLinearVel() *
                                     s_params.get<float>(projectile->getSection() + ".slowdown_factor"));
            
        } else
        {
            // only count direct hits for accuracy
            shooter_score->shots_++;
        }

        // sends score for acc, eff calc. to shooting player only
        sendScoreUpdate(projectile->getOwner(), projectile->getOwner(), CST_SINGLE);
    } else if (hit_object && hit_object->getType() == "SoccerBall")
    {
        pushBallContact(projectile->getOwner());
    }
}

//------------------------------------------------------------------------------
void GameLogicServerSoccer::onInstantWeaponHit(InstantHitWeapon * gun,
                                               RigidBody * hit_object)
{
    
    GameLogicServerCommon::onInstantWeaponHit(gun, hit_object);

    if (hit_object->getType() == "SoccerBall")
    {
        // slow down ball and accelerate it towards tank
        Vector dir = gun->getTank()->getPosition() - hit_object->getPosition();
        dir.normalize();
        
        float f1 = s_params.get<float>("soccer.logic.tractor_slowdown_factor");
        float f2 = s_params.get<float>("soccer.logic.tractor_acc_factor");
        
        hit_object->setGlobalAngularVel(hit_object->getGlobalAngularVel() * f1);
        hit_object->setGlobalLinearVel (hit_object->getGlobalLinearVel()  * f1 + dir * f2);

        hit_object->setSleeping(false);

        pushBallContact(gun->getTank()->getOwner());
    }   
}

//------------------------------------------------------------------------------
void GameLogicServerSoccer::onTimeLimitExpired()
{    
    TEAM_ID winner_team = INVALID_TEAM_ID;
    unsigned winner_score = 0;

    for(unsigned c=0; c < score_.getNumTeams(); c++)
    {
        /// handle draw
        if(score_.getTeamScore(c)->score_ == (int)winner_score)
        {
            winner_team = INVALID_TEAM_ID;
        }

        // new winner
        if(score_.getTeamScore(c)->score_ > (int)winner_score)
        {
            winner_team = (TEAM_ID)c;
            winner_score = score_.getTeamScore(c)->score_;
        }
    }

    getMatchEvents()->setFinalGoals(score_.getTeamScore(team_[TEAM_ID_BLUE].getId())->score_,
                              score_.getTeamScore(team_[TEAM_ID_RED ].getId())->score_);
    

    getMatchEvents()->setBallPossessionPercentage(intervalsToPercentage(team_[TEAM_ID_BLUE].getBallPossession(),
                                                                  team_[TEAM_ID_RED]. getBallPossession()));
    getMatchEvents()->setGameDistribution(intervalsToPercentage(team_[TEAM_ID_RED]. getBallInHalf(),
                                                          team_[TEAM_ID_BLUE].getBallInHalf()));
        
    
    sendGameWon(winner_team,
                UNASSIGNED_SYSTEM_ADDRESS,
                CST_BROADCAST_ALL);

    s_scheduler.removeTask(task_clear_assistant_, &fp_group_);
    task_clear_assistant_ = INVALID_TASK_HANDLE;
    
    gameWon();
}


//------------------------------------------------------------------------------
void GameLogicServerSoccer::onPlayerTeamChanged(ServerPlayer * player)
{
    GameLogicServerCommon::onPlayerTeamChanged(player);

    // start game if each team has one player at least...
    if (state_ == TGS_BEFORE_MATCH_START &&
        score_.getTeamSize(TEAM_ID_BLUE) > 0 &&
        score_.getTeamSize(TEAM_ID_RED)  > 0)
    {
        onMatchStart();
    }

    if (state_ == TGS_BEFORE_MATCH_START)
    {
        sendStatusMessage("Waiting for other players to join the match...",
                          Color(1.0f,1.0f,1.0f),
                          player->getId(), 
                          CST_SINGLE);
    }
}


//------------------------------------------------------------------------------
GameLogicServer * GameLogicServerSoccer::create()
{
    return new GameLogicServerSoccer();
}

//------------------------------------------------------------------------------
/**
 *  creates the logic specific bot. return null if there is no implementation
 *  for this logic
 */
AIPlayer * GameLogicServerSoccer::createAIPlayer() const
{
    return new AIPlayerSoccer(puppet_master_);
} 

//------------------------------------------------------------------------------
void GameLogicServerSoccer::doHandleLogic(float dt)
{
    GameLogicServerCommon::doHandleLogic(dt);

    if (state_ != TGS_IN_PROGRESS) return;
    
    TeamSoccer * owner_team = (TeamSoccer*)score_.getTeam(last_ball_contact_[0]);
    if (owner_team)
    {
        owner_team->incBallPossession();
    }    
}

//------------------------------------------------------------------------------
/**
 *  resets scores, respawns all players and the ball used for
 *  auto-restart after all teams have players assigned to them
 **/
void GameLogicServerSoccer::onMatchStart()
{
    // remove the ball from play
    if(soccer_ball_)
    {
        // new ball gets auto respawned
        soccer_ball_->scheduleForDeletion();
    } else
    {
        // if the respawn task is already in progress, only reschedule
        // it
        assert(task_spawn_soccer_ball_);
        s_scheduler.reschedule(task_spawn_soccer_ball_,
                               s_params.get<float>("soccer.logic.goal_restart_delay"));
    }
    
    // reset all scores
    // this might become neccessary when a match in progress goes back
    // to TGS_BEFORE_MATCH_START e.g. after a player leaves
    score_.resetPlayerScores();
    score_.getTeamScore(0)->score_ = 0;
    score_.getTeamScore(1)->score_ = 0;

    team_[TEAM_ID_BLUE].resetBallStats();
    team_[TEAM_ID_RED] .resetBallStats();
    

    // reset ball contacts
    last_ball_contact_[0] = UNASSIGNED_SYSTEM_ADDRESS;
    last_ball_contact_[1] = UNASSIGNED_SYSTEM_ADDRESS;

    GameLogicServerCommon::onMatchStart();

}


//------------------------------------------------------------------------------
void GameLogicServerSoccer::healTanks(float dt)
{
    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        Tank * tank = dynamic_cast<Tank*>(it->getControllable());
        if (!tank) continue;

        tank->setHitpoints(tank->getHitpoints()+1);
    }
}


//------------------------------------------------------------------------------
void GameLogicServerSoccer::sendGameWon(TEAM_ID winner_team,
                                                const SystemAddress & target_id,
                                                COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    args.Write(winner_team);
    
    network::CustomServerCmd game_won_cmd(CSCTS_GAME_WON, args);
    puppet_master_->sendNetworkCommand(game_won_cmd, target_id, type);

    if (winner_team != INVALID_TEAM_ID)
    {
        s_log << team_[winner_team].getName()
              << " has won the match.\n";
    } else
    {
        s_log << "Round draw.\n";
    }
}

//------------------------------------------------------------------------------
void GameLogicServerSoccer::sendGoalCmd(TEAM_ID goal_scoring_team,
                                        const SystemAddress & goalgetter,
                                        const SystemAddress & assist,
                                        unsigned type_of_goal,
                                        const Vector & pos)
{
    RakNet::BitStream args;
    args.Write(goal_scoring_team);
    args.Write(goalgetter);

    args.Write(assist);
    args.Write(type_of_goal);
    
    args.WriteVector(pos.x_, pos.y_, pos.z_);
    
    args.Write(score_.getTeamScore(0)->score_);
    args.Write(score_.getTeamScore(1)->score_);

    network::CustomServerCmd goal_cmd(CSCTS_GOAL, args);
    puppet_master_->sendNetworkCommand(goal_cmd);
}


//------------------------------------------------------------------------------
bool GameLogicServerSoccer::onSoccerballSensorCollision(const physics::CollisionInfo & info)
{
    if (state_ != TGS_IN_PROGRESS) return false;
    
    
    if (info.other_geom_->getName() == FIELD_HALF_NAME_RED)
    {
        team_[TEAM_ID_RED].incBallInHalf();
    }

    if (info.other_geom_->getName() == FIELD_HALF_NAME_BLUE)
    {
        team_[TEAM_ID_BLUE].incBallInHalf();
    }
    
    if (info.type_ != physics::CT_START)
    {
        return false;
    }

    if(info.other_geom_->getName() == GOAL_NAME_RED)
    {
        handleGoal(TEAM_ID_BLUE);
    } else if (info.other_geom_->getName() == GOAL_NAME_BLUE)
    {
        handleGoal(TEAM_ID_RED);
    }

    return false;
}


//------------------------------------------------------------------------------
physics::CONTACT_GENERATION GameLogicServerSoccer::onSoccerballCollision(const physics::CollisionInfo & info)
{
    Controllable * controllable = dynamic_cast<Controllable*>((RigidBody*)info.other_geom_->getUserData());

    if (controllable)
    {
        if (info.type_ == physics::CT_START ||
            info.type_ == physics::CT_IN_PROGRESS)
        {
            if (task_send_absolute_ == INVALID_TASK_HANDLE)
            {
                task_send_absolute_ =
                    s_scheduler.addEvent(SingleEventCallback(this, &GameLogicServerSoccer::sendBallAbsolute),
                                         SEND_ABSOLUTE_DELAY,
                                         NULL,
                                         "GameLogicServerSoccer::sendBallAbsolute",
                                         &fp_group_);

                soccer_ball_->sendPosRelativeTo(controllable->getId());
            } else
            {
                s_scheduler.reschedule(task_send_absolute_,
                                       SEND_ABSOLUTE_DELAY);
            }
        } 
    }

    // generate both contacts or ugly effects e.g. when pushing ball
    // against wall...
    return physics::CG_GENERATE_BOTH;
}


//------------------------------------------------------------------------------
physics::CONTACT_GENERATION GameLogicServerSoccer::onTankClawCollision(const physics::CollisionInfo & info)
{
    // Claws collide with ball only.
    if (info.other_geom_->getName() != "ball") return physics::CG_GENERATE_NONE;
    
    Tank * tank = (Tank*)info.this_geom_->getUserData();
    assert(tank);
    pushBallContact(tank->getOwner());


    // Only push the ball towards the center, never outwards!
    // For this, go to tank local coordinates.
    Vector pos = tank->getTransform().transformPointInv(info.pos_);
    Vector n   = tank->getTransform().transformVectorInv(info.n_);

    if (abs(n.x_) > abs(n.y_) &&
        abs(n.x_) > abs(n.z_) &&
        sign(pos.x_) != sign(n.x_)) return physics::CG_GENERATE_NONE;        
    
    return physics::CG_GENERATE_BOTH;
    return physics::CG_GENERATE_SECOND;
}


//------------------------------------------------------------------------------
/***
 *
 **/
void GameLogicServerSoccer::handleGoal(TEAM_ID goal_scoring_team)
{
    assert(state_ == TGS_IN_PROGRESS);

    // require both teams to have players in order to count goal.
    bool count_goal = (score_.getTeamSize(TEAM_ID_BLUE) > 0 &&
                       score_.getTeamSize(TEAM_ID_RED)  > 0);
    

    if (count_goal)
    {
        ++score_.getTeamScore(goal_scoring_team)->score_;
    }
    
    unsigned type_of_goal = GTS_NORMAL_GOAL;

    const ServerPlayer * goal_getter    = puppet_master_->getPlayer(last_ball_contact_[0]);
    const ServerPlayer * goal_assistant = puppet_master_->getPlayer(last_ball_contact_[1]);

    PlayerScore * score_getter    = score_.getPlayerScore(last_ball_contact_[0]);
    PlayerScore * score_assistant = score_.getPlayerScore(last_ball_contact_[1]);

    if (goal_getter && count_goal)
    {
        if (score_.getTeamId(goal_getter->getId()) == goal_scoring_team)
        {
            // It was a proper goal, player gets score
            score_getter->score_ += GOAL_SCORE;
            score_getter->goals_++;
            getMatchEvents()->logGoal(goal_getter->getRankingId());

            if (score_assistant &&
                (score_getter->getTeam() ==
                 score_assistant->getTeam()))
            {
                // assistant was of same team, credit him
                score_assistant->score_ += ASSIST_SCORE;
                score_assistant->assists_++;
                getMatchEvents()->logGoalAssist(goal_assistant->getRankingId());
            } else
            {
                // assistant was of other team, ignore him
                goal_assistant        = NULL;
                score_assistant       = NULL;
                last_ball_contact_[1] = UNASSIGNED_SYSTEM_ADDRESS;
            }
        } else
        {
            // it was an own goal. If assistant is of other team,
            // score as proper goal. If not, punish for own goal.
            if (!score_assistant ||
                (score_getter->getTeam() ==
                 score_assistant->getTeam()))
            {
                // own goal, punish
                score_getter->score_ += OWN_GOAL_SCORE;
                type_of_goal = GTS_OWN_GOAL;
                getMatchEvents()->logOwnGoal(goal_getter->getRankingId());
            } else if (score_assistant)
            {
                // assume ball was only deflected by goal getter,
                // true shot was assistant
                score_assistant->score_ += GOAL_SCORE;
                score_assistant->goals_++;
                type_of_goal = GTS_DEFLECTED_GOAL;
                getMatchEvents()->logGoal(goal_assistant->getRankingId());
            }
        }

        sendScoreUpdate(goal_getter->getId(),
                        UNASSIGNED_SYSTEM_ADDRESS,
                        CST_BROADCAST_ALL);
        if (goal_assistant)
        {
            sendScoreUpdate(goal_assistant->getId(),
                            UNASSIGNED_SYSTEM_ADDRESS,
                            CST_BROADCAST_ALL);
        }
    }

    sendGoalCmd(goal_scoring_team,
                last_ball_contact_[0],
                last_ball_contact_[1],
                type_of_goal,
                soccer_ball_->getPosition());

    soccer_ball_->scheduleForDeletion();


    last_ball_contact_[0] = UNASSIGNED_SYSTEM_ADDRESS;
    last_ball_contact_[1] = UNASSIGNED_SYSTEM_ADDRESS;


    // cool down weapons, reload health for all players
    for (PuppetMasterServer::PlayerContainer::iterator p = puppet_master_->getPlayers().begin();
         p != puppet_master_->getPlayers().end();
         ++p)
    {
        Tank * tank = dynamic_cast<Tank*>(p->getControllable());
        if (!tank) continue;

        tank->setHitpoints(tank->getMaxHitpoints());
        for (unsigned w=0; w<NUM_WEAPON_SLOTS; ++w)
        {
            WeaponSystem * ws = tank->getWeaponSystems()[w];
            if (ws) ws->setCooldownStatus(0.0f);
        }
    }

    sendScoreUpdate(UNASSIGNED_SYSTEM_ADDRESS, UNASSIGNED_SYSTEM_ADDRESS, CST_BROADCAST_ALL);
}

//------------------------------------------------------------------------------
/**
 *  After ball doesn't touch tank anymore, send absolute position
 *  again.
 */
void GameLogicServerSoccer::sendBallAbsolute(void*)
{
    task_send_absolute_ = INVALID_TASK_HANDLE;
    if (!soccer_ball_) return;
    soccer_ball_->sendPosRelativeTo(INVALID_GAMEOBJECT_ID);
}


//------------------------------------------------------------------------------
void GameLogicServerSoccer::spawnSoccerball(void* )
{
    soccer_ball_ = (SoccerBall*)createRigidBody(RB_SOCCERBALL, "SoccerBall");

    soccer_ball_->setTransform(spawn_pos_ball_);
    soccer_ball_->setSleeping(false);

    puppet_master_->addGameObject(soccer_ball_);

    task_spawn_soccer_ball_ = INVALID_TASK_HANDLE;
}



//------------------------------------------------------------------------------
/**
 *  Called if the ball is destroyed for any reason (goal, void etc)
 */
void GameLogicServerSoccer::onSoccerBallDestroyed()
{
    assert(task_spawn_soccer_ball_ == INVALID_TASK_HANDLE);
    
    soccer_ball_ = NULL;


    task_spawn_soccer_ball_ = s_scheduler.addEvent(SingleEventCallback(this, &GameLogicServerSoccer::spawnSoccerball),
                                                   s_params.get<float>("soccer.logic.goal_restart_delay"),
                                                   NULL,
                                                   "GameLogicServerSoccer::spawnSoccerball()",
                                                   &fp_group_);    
}


//------------------------------------------------------------------------------
/**
 *  Slow down ball to make it easier to control. Update relative body
 *  posiiton if appropriate.
 */
void GameLogicServerSoccer::handleBall(float dt)
{
    if (!soccer_ball_) return;

    soccer_ball_->getTarget()->addLocalTorque(-s_params.get<float>("soccer.logic.ball_dampening_factor") *
                                              soccer_ball_->getLocalAngularVel());


    RigidBody * rel_body = (RigidBody*)puppet_master_->getGameState()->getGameObject(soccer_ball_->getRelObjectId());
    if (rel_body) soccer_ball_->setRelPos(soccer_ball_->getPosition() - rel_body->getPosition());
}


//------------------------------------------------------------------------------
/**
 *  Goal assistance is only scored if goal is achieved within
 *  clear_assist_delay seconds after last contact of assistant.
 *
 *  If opponent scores an own goal within clear_no_own_goal_delay
 *  seconds of us touching the ball, it scoreas as a goal, not as an
 *  own goal.
 */
void GameLogicServerSoccer::pushBallContact(const SystemAddress & address)
{
    if (last_ball_contact_[0] == address) return;

    last_ball_contact_[1] = last_ball_contact_[0];
    last_ball_contact_[0] = address;

    float clear_delay;
    if (score_.getTeam(last_ball_contact_[0]) ==
        score_.getTeam(last_ball_contact_[1]))
    {
        clear_delay = s_params.get<float>("soccer.logic.clear_assist_delay");
    } else
    {
        clear_delay = s_params.get<float>("soccer.logic.clear_no_own_goal_delay");
    }

    
    if (task_clear_assistant_ == INVALID_TASK_HANDLE)
    {
        task_clear_assistant_ =
            s_scheduler.addEvent(SingleEventCallback(this, &GameLogicServerSoccer::clearAssistant),
                                 clear_delay,
                                 NULL,
                                 "GameLogicServerSoccer::clearAssistant",
                                 &fp_group_);
    } else
    {
        s_scheduler.reschedule(task_clear_assistant_, clear_delay);
    }
}


//------------------------------------------------------------------------------
network::ranking::MatchEventsSoccer * GameLogicServerSoccer::getMatchEvents()
{
    assert(dynamic_cast<network::ranking::MatchEventsSoccer*>(match_events_.get()));
    return (network::ranking::MatchEventsSoccer*)match_events_.get();
}

//------------------------------------------------------------------------------
void GameLogicServerSoccer::clearAssistant(void*)
{
    task_clear_assistant_ = INVALID_TASK_HANDLE;
    last_ball_contact_[1] = UNASSIGNED_SYSTEM_ADDRESS;
}


//------------------------------------------------------------------------------
float GameLogicServerSoccer::intervalsToPercentage(unsigned i0, unsigned i1) const
{
    if (!i0 && !i1) return 0.5f;
    else return (float)i0 / (float)(i0+i1);
}



