
#include "GameLogicServerTeamDeathmatch.h"

#include <limits>

#include "AutoRegister.h"
#include "Tank.h"
#include "SpawnPos.h"
#include "NetworkCommandServer.h"
#include "Projectile.h"
#include "Tank.h"
#include "AIPlayerTeamDeathmatch.h"

#undef min
#undef max

REGISTER_CLASS(GameLogicServer, GameLogicServerTeamDeathmatch);


//------------------------------------------------------------------------------
GameLogicServerTeamDeathmatch::GameLogicServerTeamDeathmatch() :
    winner_team_(INVALID_TEAM_ID)
{
}

//------------------------------------------------------------------------------
void GameLogicServerTeamDeathmatch::init(PuppetMasterServer * master)
{
    GameLogicServerCommon::init(master);

    for (unsigned t=0; t<NUM_TEAMS_TDM; ++t)
    {
        team_[t].setId(t);
        team_[t].setConfigName(TEAM_CONFIG_TDM[t]);
        score_.addTeam(&team_[t]);
    }
}



//------------------------------------------------------------------------------
void GameLogicServerTeamDeathmatch::addPlayer(const Player * player, bool newly_connected)
{
    GameLogicServerCommon::addPlayer(player, newly_connected);

    if (state_ == TGS_BEFORE_MATCH_START) onMatchStart();
    
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
}


//------------------------------------------------------------------------------
void GameLogicServerTeamDeathmatch::loadLevel(const std::string & name)
{
    winner_team_ = INVALID_TEAM_ID;
    GameLogicServerCommon::loadLevel(name);    
}


//------------------------------------------------------------------------------
bool GameLogicServerTeamDeathmatch::onLevelObjectLoaded(RigidBody * obj,
                                                    const LocalParameters & params)
{
    return GameLogicServerCommon::onLevelObjectLoaded(obj, params);
}


//------------------------------------------------------------------------------
Matrix GameLogicServerTeamDeathmatch::getRespawnPos(const SystemAddress & pid,
                                                unsigned stage)
{
    return getRandomSpawnPos(spawn_pos_);
}


//------------------------------------------------------------------------------
Tank * GameLogicServerTeamDeathmatch::createNewPlayerTank(const SystemAddress & pid)
{
    TEAM_ID id = score_.getTeamId(pid);
    assert(id != INVALID_TEAM_ID);
    
    RigidBody * rb = createRigidBody(s_params.get<std::string>(team_[id].getConfigName() + ".tank_name"), "Tank");
    return dynamic_cast<Tank*>(rb);
}



//------------------------------------------------------------------------------
void GameLogicServerTeamDeathmatch::onTankKilled(Tank * tank, const SystemAddress & killer_id)
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

        // punish team for suicide
        score_.getTeamScore(killed_score->getTeam()->getId())->score_ -= KILL_VALUE;
        
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
                // punish team for team kill
                score_.getTeamScore(killer_score->getTeam()->getId())->score_ -= KILL_VALUE;
            } else
            {            
                // Ordinary kill - add to score
                killer_score->score_          += KILL_SCORE_VALUE;
                killer_score->kills_          += KILL_VALUE;
                killer_score->upgrade_points_ += KILL_UPGRADE_VALUE;

                // add kill to team score
                score_.getTeamScore(killer_score->getTeam()->getId())->score_ += KILL_VALUE;
                
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
void GameLogicServerTeamDeathmatch::onProjectileHit(Projectile * projectile,
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
void GameLogicServerTeamDeathmatch::onTimeLimitExpired()
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

    sendGameWon(winner_team,
                UNASSIGNED_SYSTEM_ADDRESS,
                CST_BROADCAST_ALL);
    gameWon();
}




//------------------------------------------------------------------------------
GameLogicServer * GameLogicServerTeamDeathmatch::create()
{
    return new GameLogicServerTeamDeathmatch();
}

//------------------------------------------------------------------------------
/**
 *  creates the logic specific bot. return null if there is no implementation
 *  for this logic
 */
AIPlayer * GameLogicServerTeamDeathmatch::createAIPlayer() const
{
    return new AIPlayerTeamDeathmatch(puppet_master_);
} 

//------------------------------------------------------------------------------
void GameLogicServerTeamDeathmatch::sendGameWon(TEAM_ID winner_team,
                                                const SystemAddress & target_id,
                                                COMMAND_SEND_TYPE type)
{
    RakNet::BitStream args;
    args.Write(winner_team);
    
    network::CustomServerCmd game_won_cmd(CSCTTDM_GAME_WON, args);
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


