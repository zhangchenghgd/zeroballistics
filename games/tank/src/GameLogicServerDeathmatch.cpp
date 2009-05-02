
#include "GameLogicServerDeathmatch.h"

#include <limits>

#include "AutoRegister.h"
#include "Tank.h"
#include "SpawnPos.h"
#include "NetworkCommandServer.h"
#include "Projectile.h"
#include "Tank.h"

#undef min
#undef max

#ifdef ZERO_BALLISTICS_MODE
REGISTER_CLASS(GameLogicServer, GameLogicServerDeathmatch);
#endif

//------------------------------------------------------------------------------
GameLogicServerDeathmatch::GameLogicServerDeathmatch() :
    winner_(UNASSIGNED_SYSTEM_ADDRESS)
{
}

//------------------------------------------------------------------------------
void GameLogicServerDeathmatch::init(PuppetMasterServer * master)
{
    GameLogicServerCommon::init(master);
    
    team_.setId(0);
    team_.setConfigName("team_deathmatch");
    score_.addTeam(&team_);
}



//------------------------------------------------------------------------------
void GameLogicServerDeathmatch::addPlayer(const Player * player, bool newly_connected)
{
    GameLogicServerCommon::addPlayer(player, newly_connected);

    if (state_ == TGS_BEFORE_MATCH_START) onMatchStart();
    
    if (newly_connected)
    {
        if (state_ == TGS_WON)
        {
            sendGameWon(winner_,
                        player->getId(),
                        CST_SINGLE);
        }

        puppet_master_->requestPlayerReady(player->getId());
    }
}


//------------------------------------------------------------------------------
void GameLogicServerDeathmatch::loadLevel(const std::string & name)
{
    winner_ = UNASSIGNED_SYSTEM_ADDRESS;
    
    GameLogicServerCommon::loadLevel(name);    
}


//------------------------------------------------------------------------------
bool GameLogicServerDeathmatch::onLevelObjectLoaded(RigidBody * obj,
                                                    const LocalParameters & params)
{
    return GameLogicServerCommon::onLevelObjectLoaded(obj, params);
}


//------------------------------------------------------------------------------
Matrix GameLogicServerDeathmatch::getRespawnPos(const SystemAddress & pid,
                                                unsigned stage)
{
    if (s_params.get<bool>("server.logic_deathmatch.allow_spawn_stage_selection"))
    {
        assert(!spawn_pos_.empty());
        assert(stage < spawn_pos_.size());
    
        stage = std::min(spawn_pos_.size()-1, stage);
    
        return spawn_pos_[stage]->getTransform();
    } else
    {
        return getRandomSpawnPos(spawn_pos_);
    }
}


//------------------------------------------------------------------------------
Tank * GameLogicServerDeathmatch::createNewPlayerTank(const SystemAddress & pid)
{
    // XXXX make this go into parameterfile, use skins....
    RigidBody * rb;
    if (rand() <(RAND_MAX>>1)) rb = createRigidBody("hornet",    "Tank");
    else                       rb = createRigidBody("brummbaer", "Tank");
    return dynamic_cast<Tank*>(rb);    
}



//------------------------------------------------------------------------------
void GameLogicServerDeathmatch::onTankKilled(Tank * tank, const SystemAddress & killer_id)
{
    assert(killer_id != tank->getOwner()); // suicide is always "killed by UNASSIGNED_SYSTEM_ADDRESS"

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
            // Ordinary kill - add to score
            killer_score->score_          += KILL_SCORE_VALUE;
            killer_score->kills_          += KILL_VALUE;
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
                 false);
}


//------------------------------------------------------------------------------
void GameLogicServerDeathmatch::onProjectileHit(Projectile * projectile,
                                                RigidBody * hit_object,
                                                float hit_percentage,
                                                const physics::CollisionInfo & info,
                                                bool create_feedback)
{
    GameLogicServerCommon::onProjectileHit(projectile, hit_object, hit_percentage, info, create_feedback);

    
    // score & accuracy calculation
    // Need different calculation here
    // because all players are in the same team...
    PlayerScore * shooter_score = score_.getPlayerScore(projectile->getOwner());

    // player score is null if firing tank left game while his
    // projectile was in progress
    if (shooter_score && hit_object && hit_object->getType() == "Tank")
    {
        if (hit_percentage == 1.0f)
        {
            if (hit_object->getOwner() != UNASSIGNED_SYSTEM_ADDRESS)
            {
                shooter_score->shots_++;
                shooter_score->hits_++;
            } else
            {
                // tank wrecks are neutral w.r.t. accuracy
            }
        } else
        {
            // only count direct hits for accuracy
            shooter_score->shots_++;
        }

        // sends score for acc, eff calc. to shooting player only
        sendScoreUpdate(projectile->getOwner(), projectile->getOwner(), CST_SINGLE);
    }    
}


//------------------------------------------------------------------------------
void GameLogicServerDeathmatch::onTimeLimitExpired()
{
    PlayerScore * best_score = NULL;
    bool draw = false;
    // find player with most kills
    for (PuppetMasterServer::PlayerContainer::iterator it = puppet_master_->getPlayers().begin();
         it != puppet_master_->getPlayers().end();
         ++it)
    {
        PlayerScore * score = score_.getPlayerScore(it->getId());

        if (!best_score)
        {
            best_score = score;
        } else
        {
            if (score->score_ >  best_score->score_)
            {
                draw = false;
                best_score = score;
            } else if (score->score_ == best_score->score_)
            {
                draw = true;
            }
        }        
    }

    // we might be without players...
    if (best_score)
    {
        winner_ = draw ? UNASSIGNED_SYSTEM_ADDRESS : best_score->getPlayer()->getId();
        sendGameWon(winner_,
                    UNASSIGNED_SYSTEM_ADDRESS,
                    CST_BROADCAST_ALL);
    }

    gameWon();
}




//------------------------------------------------------------------------------
GameLogicServer * GameLogicServerDeathmatch::create()
{
    return new GameLogicServerDeathmatch();
}

//------------------------------------------------------------------------------
void GameLogicServerDeathmatch::sendGameWon(const SystemAddress & player_id,
                                            const SystemAddress & target_id,
                                            COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    args.Write(player_id);
    
    network::CustomServerCmd game_won_cmd(CSCTDM_GAME_WON, args);
    puppet_master_->sendNetworkCommand(game_won_cmd, target_id, type);

    Player * player = puppet_master_->getPlayer(player_id);
    if (player)
    {
        s_log << player->getName()
              << " has won the match.\n";
    } else
    {
        s_log << "Round draw.\n";
    }
}


//------------------------------------------------------------------------------
/**
 *  Ignore all team messages in deathmatch mode.
 */
void GameLogicServerDeathmatch::onTeamMessage(RakNet::BitStream & args)
{
}
