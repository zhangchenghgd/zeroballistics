

#ifndef TANK_SPAWN_STAGE_CLIENT_INCLUDED
#define TANK_SPAWN_STAGE_CLIENT_INCLUDED

#include "Matrix.h"
#include "../Team.h"

//------------------------------------------------------------------------------
class SpawnStageClient
{
 public:
    SpawnStageClient();

    void setOwnerTeam(TEAM_ID id);
    TEAM_ID getOwnerTeam() const;

    void setCameraPosition(const Matrix & pos);
    const Matrix & getCameraPosition() const;
 protected:
    TEAM_ID owner_team_; ///< Current owner of this spawn stage. 
    
    Matrix camera_; ///< The camera position for spawn stage selection
};


#endif
