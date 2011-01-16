
#include "AIPlayerTeamDeathmatch.h"

#include <raknet/RakPeerInterface.h>

#include "ParameterManager.h"
#include "PuppetMasterServer.h"
#include "GameLogicServer.h"
#include "WeaponSystem.h"
#include "Tank.h"

#include "GameLogicServerDeathmatch.h"

#include "Profiler.h"

const float STUCK_THRESHOLD1 = 3.0f;
const float STUCK_THRESHOLD2 = 7.0f;
const float STUCK_THRESHOLD_SUICIDE = 15.0f;

const unsigned HEAL_HP_THRESHOLD = 20;

const float FIRE_ANGLE_THRESHOLD = 0.001f;

const float DELTA_YAW_FACTOR = 14.0f;
const float DELTA_PITCH_FACTOR = 10.0f;
const float DELTA_YAW_FACTOR_THRESHOLD = 0.030f;

//------------------------------------------------------------------------------
AIPlayerTeamDeathmatch::AIPlayerTeamDeathmatch(PuppetMasterServer * puppet_master) :
    AIPlayerDeathmatch(puppet_master)
{
}

//------------------------------------------------------------------------------
AIPlayerTeamDeathmatch::~AIPlayerTeamDeathmatch()
{
}

//------------------------------------------------------------------------------
void AIPlayerTeamDeathmatch::assignToTeam() const
{
/*
    TEAM_ID new_team = 0;
    if ((pid_.port % 2) == 0) new_team = 1;
    */


    GameLogicServerCommon * glsc = dynamic_cast<GameLogicServerCommon*>(puppet_master_->getGameLogic());
    TEAM_ID new_team = glsc->getScore().getSmallestTeam();
    RakNet::BitStream rak_args;
    rak_args.Write(pid_);
    rak_args.Write(new_team);
    glsc->executeCustomCommand(CCCT_REQUEST_TEAM_CHANGE, rak_args);
}

//------------------------------------------------------------------------------
/***
  *   Remember nearest enemy, do this to avoid iterating over players 
  *   all the time.
  **/
void AIPlayerTeamDeathmatch::getNearestEnemy(Tank * tank)
{
    // iterate over other tanks
    PuppetMasterServer::PlayerContainer::const_iterator it;
    PuppetMasterServer::PlayerContainer & pc = puppet_master_->getPlayers();

    for(it = pc.begin(); it != pc.end(); it++)
    {
        /// if other player has a controllable
        if(it->getControllable() && 
           (it->getControllable() != sv_player_->getControllable()))
        {

            // check if on same team
            GameLogicServerCommon * glsc = dynamic_cast<GameLogicServerCommon*>(puppet_master_->getGameLogic());
            TEAM_ID my_team = glsc->getScore().getTeamId(pid_);
            TEAM_ID enemy_team = glsc->getScore().getTeamId(it->getId());

            // only attack if team is different
            if(my_team == INVALID_TEAM_ID || enemy_team == INVALID_TEAM_ID) continue;
            if(my_team == enemy_team) continue;

            // if inside fire radius
            Vector dist = it->getControllable()->getPosition() - tank->getPosition();
            if(dist.lengthSqr() < (*attack_range_sqr_))
            {
                enemy_ = it->getControllable();

                enemy_->addObserver(ObserverCallbackFun0(this, &AIPlayerTeamDeathmatch::onEnemyDestroyed),
                                  GOE_SCHEDULED_FOR_DELETION,
                                  &fp_group_);
                return;
            }
        }
    }

    /// no enemy found nearby
    enemy_ = NULL;

}

