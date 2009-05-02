
#include "SpawnStageClient.h"


#include "TeamBs.h"

//------------------------------------------------------------------------------
SpawnStageClient::SpawnStageClient() :
    owner_team_(TEAM_ID_ATTACKER),
    camera_(true)
{
}


//------------------------------------------------------------------------------
void SpawnStageClient::setOwnerTeam(TEAM_ID id)
{
    owner_team_ = id;
}


//------------------------------------------------------------------------------
TEAM_ID SpawnStageClient::getOwnerTeam() const
{
    return owner_team_;
}



//------------------------------------------------------------------------------
void SpawnStageClient::setCameraPosition(const Matrix & pos) 
{
    camera_ = pos;
}


//------------------------------------------------------------------------------
const Matrix & SpawnStageClient::getCameraPosition() const
{
    return camera_;
}
