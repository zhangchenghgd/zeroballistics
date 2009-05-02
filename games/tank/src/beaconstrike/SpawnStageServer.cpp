
#include "SpawnStageServer.h"

#include <limits>

#include "physics/OdeCollisionSpace.h"

#include "Log.h"
#include "Beacon.h"
#include "ParameterManager.h"
#include "../SpawnPos.h"



#undef min
#undef max


//------------------------------------------------------------------------------
SpawnStageServer::SpawnStageServer() :
    defender_beacon_(NULL),
    beacon_spawn_pos_(NULL)
{
}

//------------------------------------------------------------------------------
SpawnStageServer::~SpawnStageServer()
{
}

//------------------------------------------------------------------------------
TEAM_ID SpawnStageServer::getOwnerTeam() const
{
    if (defender_beacon_) return TEAM_ID_DEFENDER;
    else return TEAM_ID_ATTACKER;
}

//------------------------------------------------------------------------------
void SpawnStageServer::setDefenderBeacon(Beacon * b)
{
    defender_beacon_ = b;
}


//------------------------------------------------------------------------------
Beacon * SpawnStageServer::getDefenderBeacon()
{
    return defender_beacon_;
}



//------------------------------------------------------------------------------
void SpawnStageServer::addPlayerSpawnPos(SpawnPos * pos, TEAM_ID team)
{    
    player_spawn_pos_[team].push_back(pos);
}

//------------------------------------------------------------------------------
void SpawnStageServer::setBeaconSpawnPos(std::auto_ptr<SpawnPos> pos)
{
    beacon_spawn_pos_ = pos;
}

//------------------------------------------------------------------------------
const SpawnPos * SpawnStageServer::getBeaconSpawnPos() const
{
    return beacon_spawn_pos_.get();
}



//------------------------------------------------------------------------------
const std::vector<SpawnPos*> & SpawnStageServer::getSpawnPositions(TEAM_ID team)
{
    assert(team < NUM_TEAMS_BS);
    return player_spawn_pos_[team];
}



